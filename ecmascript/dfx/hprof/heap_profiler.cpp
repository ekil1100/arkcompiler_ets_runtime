/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include "ecmascript/dfx/hprof/heap_profiler.h"

#include "ecmascript/checkpoint/thread_state_transition.h"
#include "ecmascript/dfx/hprof/heap_snapshot.h"
#include "ecmascript/jspandafile/js_pandafile_manager.h"
#include "ecmascript/mem/heap-inl.h"
#include "ecmascript/mem/shared_heap/shared_concurrent_sweeper.h"
#include "ecmascript/base/block_hook_scope.h"

#if defined(ENABLE_DUMP_IN_FAULTLOG)
#include "faultloggerd_client.h"
#endif

namespace panda::ecmascript {
static pid_t ForkBySyscall(void)
{
#ifdef SYS_fork
    return syscall(SYS_fork);
#else
    return syscall(SYS_clone, SIGCHLD, 0);
#endif
}

std::pair<bool, NodeId> EntryIdMap::FindId(JSTaggedType addr)
{
    auto it = idMap_.find(addr);
    if (it == idMap_.end()) {
        return std::make_pair(false, GetNextId()); // return nextId if entry not exits
    } else {
        return std::make_pair(true, it->second);
    }
}

bool EntryIdMap::InsertId(JSTaggedType addr, NodeId id)
{
    auto it = idMap_.find(addr);
    if (it == idMap_.end()) {
        idMap_.emplace(addr, id);
        return true;
    }
    idMap_[addr] = id;
    return false;
}

bool EntryIdMap::EraseId(JSTaggedType addr)
{
    auto it = idMap_.find(addr);
    if (it == idMap_.end()) {
        return false;
    }
    idMap_.erase(it);
    return true;
}

bool EntryIdMap::Move(JSTaggedType oldAddr, JSTaggedType forwardAddr)
{
    if (oldAddr == forwardAddr) {
        return true;
    }
    auto it = idMap_.find(oldAddr);
    if (it != idMap_.end()) {
        NodeId id = it->second;
        idMap_.erase(it);
        idMap_[forwardAddr] = id;
        return true;
    }
    return false;
}

void EntryIdMap::UpdateEntryIdMap(HeapSnapshot *snapshot)
{
    if (snapshot == nullptr) {
        LOG_ECMA(FATAL) << "EntryIdMap::UpdateEntryIdMap:snapshot is nullptr";
    }
    auto nodes = snapshot->GetNodes();
    CUnorderedMap<JSTaggedType, NodeId> newIdMap;
    for (auto node : *nodes) {
        auto addr = node->GetAddress();
        auto it = idMap_.find(addr);
        if (it != idMap_.end()) {
            newIdMap.emplace(addr, it->second);
        }
    }
    idMap_.clear();
    idMap_ = newIdMap;
}

HeapProfiler::HeapProfiler(const EcmaVM *vm) : vm_(vm), stringTable_(vm), chunk_(vm->GetNativeAreaAllocator())
{
    isProfiling_ = false;
    entryIdMap_ = GetChunk()->New<EntryIdMap>();
}

HeapProfiler::~HeapProfiler()
{
    JSPandaFileManager::GetInstance()->ClearNameMap();
    ClearSnapshot();
    GetChunk()->Delete(entryIdMap_);
}

void HeapProfiler::AllocationEvent(TaggedObject *address, size_t size)
{
    DISALLOW_GARBAGE_COLLECTION;
    if (isProfiling_) {
        // Id will be allocated later while add new node
        if (heapTracker_ != nullptr) {
            heapTracker_->AllocationEvent(address, size);
        }
    }
}

void HeapProfiler::MoveEvent(uintptr_t address, TaggedObject *forwardAddress, size_t size)
{
    LockHolder lock(mutex_);
    if (isProfiling_) {
        entryIdMap_->Move(static_cast<JSTaggedType>(address), reinterpret_cast<JSTaggedType>(forwardAddress));
        if (heapTracker_ != nullptr) {
            heapTracker_->MoveEvent(address, forwardAddress, size);
        }
    }
}

void HeapProfiler::UpdateHeapObjects(HeapSnapshot *snapshot)
{
    SharedHeap::GetInstance()->GetSweeper()->WaitAllTaskFinished();
    snapshot->UpdateNodes();
}

void HeapProfiler::DumpHeapSnapshot([[maybe_unused]] const DumpSnapShotOption &dumpOption)
{
#if defined(ENABLE_DUMP_IN_FAULTLOG)
    // Write in faultlog for heap leak.
    int32_t fd = RequestFileDescriptor(static_cast<int32_t>(FaultLoggerType::JS_HEAP_SNAPSHOT));
    if (fd < 0) {
        LOG_ECMA(ERROR) << "OOM Dump Write FD failed, fd" << fd;
        return;
    }
    FileDescriptorStream stream(fd);
    DumpHeapSnapshot(&stream, dumpOption);
#endif
}

bool HeapProfiler::DoDump(Stream *stream, Progress *progress, const DumpSnapShotOption &dumpOption)
{
    int32_t heapCount = 0;
    HeapSnapshot *snapshot = nullptr;
    {
        if (dumpOption.isFullGC) {
            size_t heapSize = vm_->GetHeap()->GetLiveObjectSize();
            LOG_ECMA(INFO) << "HeapProfiler DumpSnapshot heap size " << heapSize;
            heapCount = static_cast<int32_t>(vm_->GetHeap()->GetHeapObjectCount());
            if (progress != nullptr) {
                progress->ReportProgress(0, heapCount);
            }
        }
        snapshot = MakeHeapSnapshot(SampleType::ONE_SHOT, dumpOption);
        ASSERT(snapshot != nullptr);
    }
    entryIdMap_->UpdateEntryIdMap(snapshot);
    isProfiling_ = true;
    if (progress != nullptr) {
        progress->ReportProgress(heapCount, heapCount);
    }
    if (!stream->Good()) {
        FileStream newStream(GenDumpFileName(dumpOption.dumpFormat));
        auto serializerResult = HeapSnapshotJSONSerializer::Serialize(snapshot, &newStream);
        GetChunk()->Delete(snapshot);
        return serializerResult;
    }
    auto serializerResult = HeapSnapshotJSONSerializer::Serialize(snapshot, stream);
    GetChunk()->Delete(snapshot);
    return serializerResult;
}

[[maybe_unused]]static void WaitProcess(pid_t pid)
{
    time_t startTime = time(nullptr);
    constexpr int DUMP_TIME_OUT = 300;
    constexpr int DEFAULT_SLEEP_TIME = 100000;
    while (true) {
        int status = 0;
        pid_t p = waitpid(pid, &status, WNOHANG);
        if (p < 0 || p == pid) {
            break;
        }
        if (time(nullptr) > startTime + DUMP_TIME_OUT) {
            LOG_GC(ERROR) << "DumpHeapSnapshot kill thread, wait " << DUMP_TIME_OUT << " s";
            kill(pid, SIGTERM);
            break;
        }
        usleep(DEFAULT_SLEEP_TIME);
    }
}

void HeapProfiler::FillIdMap()
{
    EntryIdMap* newEntryIdMap = GetChunk()->New<EntryIdMap>();
    // Iterate SharedHeap Object
    SharedHeap* sHeap = SharedHeap::GetInstance();
    if (sHeap != nullptr) {
        sHeap->IterateOverObjects([newEntryIdMap, this](TaggedObject *obj) {
            JSTaggedType addr = ((JSTaggedValue)obj).GetRawData();
            auto [idExist, sequenceId] = entryIdMap_->FindId(addr);
            newEntryIdMap->InsertId(addr, sequenceId);
        });
    }

    // Iterate LocalHeap Object
    auto heap = vm_->GetHeap();
    if (heap != nullptr) {
        heap->IterateOverObjects([newEntryIdMap, this](TaggedObject *obj) {
            JSTaggedType addr = ((JSTaggedValue)obj).GetRawData();
            auto [idExist, sequenceId] = entryIdMap_->FindId(addr);
            newEntryIdMap->InsertId(addr, sequenceId);
        });
    }

    // copy entryIdMap
    CUnorderedMap<JSTaggedType, NodeId>* idMap = entryIdMap_->GetIdMap();
    CUnorderedMap<JSTaggedType, NodeId>* newIdMap = newEntryIdMap->GetIdMap();
    *idMap = *newIdMap;

    GetChunk()->Delete(newEntryIdMap);
}

bool HeapProfiler::DumpHeapSnapshot(Stream *stream, const DumpSnapShotOption &dumpOption, Progress *progress)
{
    bool res = false;
    base::BlockHookScope blockScope;
    ThreadManagedScope managedScope(vm_->GetJSThread());

    pid_t pid = -1;
    {
        if (dumpOption.isFullGC) {
            [[maybe_unused]] bool heapClean = ForceFullGC(vm_);
            ASSERT(heapClean);
        }
        // suspend All.
        SuspendAllScope suspendScope(vm_->GetAssociatedJSThread());
        if (dumpOption.isFullGC) {
            DISALLOW_GARBAGE_COLLECTION;
            const_cast<Heap *>(vm_->GetHeap())->Prepare();
        }
        if (dumpOption.isBeforeFill) {
            Runtime::GetInstance()->GCIterateThreadList([&](JSThread *thread) {
                ASSERT(!thread->IsInRunningState());
                const_cast<Heap*>(thread->GetEcmaVM()->GetHeap())->FillBumpPointerForTlab();
            });
            FillIdMap();
        }
        // fork
        if ((pid = ForkBySyscall()) < 0) {
            LOG_ECMA(ERROR) << "DumpHeapSnapshot fork failed!";
            return false;
        }
        if (pid == 0) {
            vm_->GetAssociatedJSThread()->EnableCrossThreadExecution();
            prctl(PR_SET_NAME, reinterpret_cast<unsigned long>("dump_process"), 0, 0, 0);
            res = DoDump(stream, progress, dumpOption);
            _exit(0);
        }
    }

    if (pid != 0) {
        if (dumpOption.isSync) {
            WaitProcess(pid);
        } else {
            std::thread thread(&WaitProcess, pid);
            thread.detach();
        }
        stream->EndOfStream();
    }
    isProfiling_ = true;
    return res;
}

bool HeapProfiler::StartHeapTracking(double timeInterval, bool isVmMode, Stream *stream,
                                     bool traceAllocation, bool newThread)
{
    vm_->CollectGarbage(TriggerGCType::OLD_GC);
    ForceSharedGC();
    SuspendAllScope suspendScope(vm_->GetAssociatedJSThread());
    DumpSnapShotOption dumpOption;
    dumpOption.isVmMode = isVmMode;
    dumpOption.isPrivate = false;
    dumpOption.captureNumericValue = false;
    HeapSnapshot *snapshot = MakeHeapSnapshot(SampleType::REAL_TIME, dumpOption, traceAllocation);
    if (snapshot == nullptr) {
        return false;
    }
    isProfiling_ = true;
    UpdateHeapObjects(snapshot);
    heapTracker_ = std::make_unique<HeapTracker>(snapshot, timeInterval, stream);
    const_cast<EcmaVM *>(vm_)->StartHeapTracking();
    if (newThread) {
        heapTracker_->StartTracing();
    }

    return true;
}

bool HeapProfiler::UpdateHeapTracking(Stream *stream)
{
    if (heapTracker_ == nullptr) {
        return false;
    }
    HeapSnapshot *snapshot = heapTracker_->GetHeapSnapshot();
    if (snapshot == nullptr) {
        return false;
    }

    {
        vm_->CollectGarbage(TriggerGCType::OLD_GC);
        ForceSharedGC();
        SuspendAllScope suspendScope(vm_->GetAssociatedJSThread());
        snapshot->RecordSampleTime();
        UpdateHeapObjects(snapshot);
    }

    if (stream != nullptr) {
        snapshot->PushHeapStat(stream);
    }

    return true;
}

bool HeapProfiler::StopHeapTracking(Stream *stream, Progress *progress, bool newThread)
{
    if (heapTracker_ == nullptr) {
        return false;
    }
    int32_t heapCount = static_cast<int32_t>(vm_->GetHeap()->GetHeapObjectCount());

    const_cast<EcmaVM *>(vm_)->StopHeapTracking();
    if (newThread) {
        heapTracker_->StopTracing();
    }

    HeapSnapshot *snapshot = heapTracker_->GetHeapSnapshot();
    if (snapshot == nullptr) {
        return false;
    }

    if (progress != nullptr) {
        progress->ReportProgress(0, heapCount);
    }
    {
        ForceSharedGC();
        SuspendAllScope suspendScope(vm_->GetAssociatedJSThread());
        SharedHeap::GetInstance()->GetSweeper()->WaitAllTaskFinished();
        snapshot->FinishSnapshot();
    }

    isProfiling_ = false;
    if (progress != nullptr) {
        progress->ReportProgress(heapCount, heapCount);
    }
    return HeapSnapshotJSONSerializer::Serialize(snapshot, stream);
}

std::string HeapProfiler::GenDumpFileName(DumpFormat dumpFormat)
{
    CString filename("hprof_");
    switch (dumpFormat) {
        case DumpFormat::JSON:
            filename.append(GetTimeStamp());
            break;
        case DumpFormat::BINARY:
            filename.append("unimplemented");
            break;
        case DumpFormat::OTHER:
            filename.append("unimplemented");
            break;
        default:
            filename.append("unimplemented");
            break;
    }
    filename.append(".heapsnapshot");
    return ConvertToStdString(filename);
}

CString HeapProfiler::GetTimeStamp()
{
    std::time_t timeSource = std::time(nullptr);
    struct tm tm {
    };
    struct tm *timeData = localtime_r(&timeSource, &tm);
    if (timeData == nullptr) {
        LOG_FULL(FATAL) << "localtime_r failed";
        UNREACHABLE();
    }
    CString stamp;
    const int TIME_START = 1900;
    stamp.append(ToCString(timeData->tm_year + TIME_START))
        .append("-")
        .append(ToCString(timeData->tm_mon + 1))
        .append("-")
        .append(ToCString(timeData->tm_mday))
        .append("_")
        .append(ToCString(timeData->tm_hour))
        .append("-")
        .append(ToCString(timeData->tm_min))
        .append("-")
        .append(ToCString(timeData->tm_sec));
    return stamp;
}

bool HeapProfiler::ForceFullGC(const EcmaVM *vm)
{
    if (vm->IsInitialized()) {
        const_cast<Heap *>(vm->GetHeap())->CollectGarbage(TriggerGCType::FULL_GC);
        return true;
    }
    return false;
}

void HeapProfiler::ForceSharedGC()
{
    SharedHeap *sHeap = SharedHeap::GetInstance();
    sHeap->CollectGarbage<TriggerGCType::SHARED_GC, GCReason::OTHER>(vm_->GetAssociatedJSThread());
    sHeap->GetSweeper()->WaitAllTaskFinished();
}

HeapSnapshot *HeapProfiler::MakeHeapSnapshot(SampleType sampleType, const DumpSnapShotOption &dumpOption,
                                             bool traceAllocation)
{
    LOG_ECMA(INFO) << "HeapProfiler::MakeHeapSnapshot";
    if (dumpOption.isFullGC) {
        DISALLOW_GARBAGE_COLLECTION;
        const_cast<Heap *>(vm_->GetHeap())->Prepare();
    }
    switch (sampleType) {
        case SampleType::ONE_SHOT: {
            auto *snapshot = GetChunk()->New<HeapSnapshot>(vm_, GetEcmaStringTable(), dumpOption,
                                                           traceAllocation, entryIdMap_, GetChunk());
            if (snapshot == nullptr) {
                LOG_FULL(FATAL) << "alloc snapshot failed";
                UNREACHABLE();
            }
            snapshot->BuildUp(dumpOption.isSimplify);
            return snapshot;
        }
        case SampleType::REAL_TIME: {
            auto *snapshot = GetChunk()->New<HeapSnapshot>(vm_, GetEcmaStringTable(), dumpOption,
                                                           traceAllocation, entryIdMap_, GetChunk());
            if (snapshot == nullptr) {
                LOG_FULL(FATAL) << "alloc snapshot failed";
                UNREACHABLE();
            }
            AddSnapshot(snapshot);
            snapshot->PrepareSnapshot();
            return snapshot;
        }
        default:
            return nullptr;
    }
}

void HeapProfiler::AddSnapshot(HeapSnapshot *snapshot)
{
    if (hprofs_.size() >= MAX_NUM_HPROF) {
        ClearSnapshot();
    }
    ASSERT(snapshot != nullptr);
    hprofs_.emplace_back(snapshot);
}

void HeapProfiler::ClearSnapshot()
{
    for (auto *snapshot : hprofs_) {
        GetChunk()->Delete(snapshot);
    }
    hprofs_.clear();
}

bool HeapProfiler::StartHeapSampling(uint64_t samplingInterval, int stackDepth)
{
    if (heapSampling_.get()) {
        LOG_ECMA(ERROR) << "Do not start heap sampling twice in a row.";
        return false;
    }
    heapSampling_ = std::make_unique<HeapSampling>(vm_, const_cast<Heap *>(vm_->GetHeap()),
                                                   samplingInterval, stackDepth);
    return true;
}

void HeapProfiler::StopHeapSampling()
{
    heapSampling_.reset();
}

const struct SamplingInfo *HeapProfiler::GetAllocationProfile()
{
    if (!heapSampling_.get()) {
        LOG_ECMA(ERROR) << "Heap sampling was not started, please start firstly.";
        return nullptr;
    }
    return heapSampling_->GetAllocationProfile();
}
}  // namespace panda::ecmascript
