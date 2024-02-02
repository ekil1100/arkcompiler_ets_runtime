/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "ecmascript/mem/work_manager.h"

#include "ecmascript/js_hclass-inl.h"
#include "ecmascript/mem/area.h"
#include "ecmascript/mem/full_gc.h"
#include "ecmascript/mem/heap.h"
#include "ecmascript/mem/heap_region_allocator.h"
#include "ecmascript/mem/incremental_marker.h"
#include "ecmascript/mem/mark_stack.h"
#include "ecmascript/mem/parallel_marker-inl.h"
#include "ecmascript/mem/partial_gc.h"
#include "ecmascript/mem/region.h"
#include "ecmascript/mem/tlab_allocator-inl.h"

namespace panda::ecmascript {
WorkManagerBase::WorkManagerBase(NativeAreaAllocator *allocator)
    : spaceChunk_(allocator), workSpace_(0), spaceStart_(0), spaceEnd_(0)
{
    workSpace_ = ToUintPtr(GetSpaceChunk()->Allocate(WORKNODE_SPACE_SIZE));
}

WorkNode *WorkManagerBase::AllocateWorkNode()
{
    size_t totalSize = sizeof(WorkNode) + sizeof(Stack) + STACK_AREA_SIZE;
    ASSERT(totalSize < WORKNODE_SPACE_SIZE);

    // CAS
    volatile auto atomicField = reinterpret_cast<volatile std::atomic<uintptr_t> *>(&spaceStart_);
    bool result = false;
    uintptr_t begin = 0;
    do {
        begin = atomicField->load(std::memory_order_acquire);
        if (begin + totalSize >= spaceEnd_) {
            LockHolder lock(mtx_);
            begin = atomicField->load(std::memory_order_acquire);
            if (begin + totalSize >= spaceEnd_) {
                agedSpaces_.emplace_back(workSpace_);
                workSpace_ = ToUintPtr(GetSpaceChunk()->Allocate(WORKNODE_SPACE_SIZE));
                spaceStart_ = workSpace_;
                spaceEnd_ = workSpace_ + WORKNODE_SPACE_SIZE;
                begin = spaceStart_;
            }
        }
        result = std::atomic_compare_exchange_strong_explicit(atomicField, &begin, begin + totalSize,
                                                              std::memory_order_release, std::memory_order_relaxed);
    } while (!result);
    Stack *stack = reinterpret_cast<Stack *>(begin + sizeof(WorkNode));
    stack->ResetBegin(begin + sizeof(WorkNode) + sizeof(Stack), begin + totalSize);
    WorkNode *work = reinterpret_cast<WorkNode *>(begin);
    return new (work) WorkNode(stack);
}

WorkManagerBase::~WorkManagerBase()
{
    GetSpaceChunk()->Free(reinterpret_cast<void *>(workSpace_));
}

WorkManager::WorkManager(Heap *heap, uint32_t threadNum)
    : WorkManagerBase(heap->GetNativeAreaAllocator()), heap_(heap), threadNum_(threadNum),
      continuousQueue_ { nullptr }, parallelGCTaskPhase_(UNDEFINED_TASK)
{
    for (uint32_t i = 0; i < threadNum_; i++) {
        continuousQueue_.at(i) = new ProcessQueue();
    }
}

WorkManager::~WorkManager()
{
    Finish();
    for (uint32_t i = 0; i < threadNum_; i++) {
        continuousQueue_.at(i)->Destroy();
        delete continuousQueue_.at(i);
        continuousQueue_.at(i) = nullptr;
    }
}

bool WorkManager::Push(uint32_t threadId, TaggedObject *object)
{
    WorkNode *&inNode = works_.at(threadId).inNode_;
    if (!inNode->PushObject(ToUintPtr(object))) {
        PushWorkNodeToGlobal(threadId);
        return inNode->PushObject(ToUintPtr(object));
    }
    return true;
}

bool WorkManager::Push(uint32_t threadId, TaggedObject *object, Region *region)
{
    if (Push(threadId, object)) {
        auto klass = object->GetClass();
        auto size = klass->SizeFromJSHClass(object);
        region->IncreaseAliveObjectSafe(size);
        return true;
    }
    return false;
}

void WorkManager::PushWorkNodeToGlobal(uint32_t threadId, bool postTask)
{
    WorkNode *&inNode = works_.at(threadId).inNode_;
    if (!inNode->IsEmpty()) {
        workStack_.Push(inNode);
        inNode = AllocateWorkNode();
        if (postTask && heap_->IsParallelGCEnabled() && heap_->CheckCanDistributeTask() &&
            !(heap_->IsMarking() && heap_->GetIncrementalMarker()->IsTriggeredIncrementalMark())) {
            heap_->PostParallelGCTask(parallelGCTaskPhase_);
        }
    }
}

bool WorkManager::Pop(uint32_t threadId, TaggedObject **object)
{
    WorkNode *&outNode = works_.at(threadId).outNode_;
    WorkNode *&inNode = works_.at(threadId).inNode_;
    if (!outNode->PopObject(reinterpret_cast<uintptr_t *>(object))) {
        if (!inNode->IsEmpty()) {
            WorkNode *tmp = outNode;
            outNode = inNode;
            inNode = tmp;
        } else if (!PopWorkNodeFromGlobal(threadId)) {
            return false;
        }
        return outNode->PopObject(reinterpret_cast<uintptr_t *>(object));
    }
    return true;
}

bool WorkManager::PopWorkNodeFromGlobal(uint32_t threadId)
{
    return workStack_.Pop(&works_.at(threadId).outNode_);
}

size_t WorkManager::Finish()
{
    size_t aliveSize = 0;
    for (uint32_t i = 0; i < threadNum_; i++) {
        WorkNodeHolder &holder = works_.at(i);
        if (holder.weakQueue_ != nullptr) {
            holder.weakQueue_->FinishMarking(continuousQueue_.at(i));
            delete holder.weakQueue_;
            holder.weakQueue_ = nullptr;
        }
        if (holder.allocator_ != nullptr) {
            holder.allocator_->Finalize();
            delete holder.allocator_;
            holder.allocator_ = nullptr;
        }
        holder.pendingUpdateSlots_.clear();
        aliveSize += holder.aliveSize_;
    }

    FinishBase();
    initialized_.store(false, std::memory_order_release);
    return aliveSize;
}

void WorkManager::Finish(size_t &aliveSize, size_t &promotedSize)
{
    aliveSize = Finish();
    for (uint32_t i = 0; i < threadNum_; i++) {
        WorkNodeHolder &holder = works_.at(i);
        promotedSize += holder.promotedSize_;
    }
    initialized_.store(false, std::memory_order_release);
}

void WorkManager::Initialize(TriggerGCType gcType, ParallelGCTaskPhase taskPhase)
{
    parallelGCTaskPhase_ = taskPhase;
    InitializeBase();
    for (uint32_t i = 0; i < threadNum_; i++) {
        WorkNodeHolder &holder = works_.at(i);
        holder.inNode_ = AllocateWorkNode();
        holder.outNode_ = AllocateWorkNode();
        holder.weakQueue_ = new ProcessQueue();
        holder.weakQueue_->BeginMarking(continuousQueue_.at(i));
        holder.aliveSize_ = 0;
        holder.promotedSize_ = 0;
        if (gcType != TriggerGCType::OLD_GC) {
            holder.allocator_ = new TlabAllocator(heap_);
        }
    }
    if (initialized_.load(std::memory_order_acquire)) {
        LOG_ECMA(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
    initialized_.store(true, std::memory_order_release);
}

ShareGCWorkManager::ShareGCWorkManager(SharedHeap *heap, uint32_t threadNum)
    : WorkManagerBase(heap->GetNativeAreaAllocator()), sHeap_(heap), threadNum_(threadNum),
      continuousQueue_ { nullptr }
{
    for (uint32_t i = 0; i < threadNum_; i++) {
        continuousQueue_.at(i) = new ProcessQueue();
    }
}

ShareGCWorkManager::~ShareGCWorkManager()
{
    Finish();
    for (uint32_t i = 0; i < threadNum_; i++) {
        continuousQueue_.at(i)->Destroy();
        delete continuousQueue_.at(i);
        continuousQueue_.at(i) = nullptr;
    }
}

void ShareGCWorkManager::Initialize()
{
    InitializeBase();
    for (uint32_t i = 0; i < threadNum_; i++) {
        ShareGCWorkNodeHolder &holder = works_.at(i);
        holder.inNode_ = AllocateWorkNode();
        holder.outNode_ = AllocateWorkNode();
        holder.weakQueue_ = new ProcessQueue();
        holder.weakQueue_->BeginMarking(continuousQueue_.at(i));
    }
    if (initialized_.load(std::memory_order_relaxed)) {
        LOG_ECMA(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
    initialized_.store(true, std::memory_order_release);
}

void ShareGCWorkManager::Finish()
{
    for (uint32_t i = 0; i < threadNum_; i++) {
        ShareGCWorkNodeHolder &holder = works_.at(i);
        if (holder.weakQueue_ != nullptr) {
            holder.weakQueue_->FinishMarking(continuousQueue_.at(i));
            delete holder.weakQueue_;
            holder.weakQueue_ = nullptr;
        }
    }
    FinishBase();
    initialized_.store(false, std::memory_order_release);
}

bool ShareGCWorkManager::Push(uint32_t threadId, TaggedObject *object)
{
    WorkNode *&inNode = works_.at(threadId).inNode_;
    if (!inNode->PushObject(ToUintPtr(object))) {
        PushWorkNodeToGlobal(threadId);
        return inNode->PushObject(ToUintPtr(object));
    }
    return true;
}

void ShareGCWorkManager::PushWorkNodeToGlobal(uint32_t threadId, bool postTask)
{
    WorkNode *&inNode = works_.at(threadId).inNode_;
    if (!inNode->IsEmpty()) {
        workStack_.Push(inNode);
        inNode = AllocateWorkNode();
        if (postTask && sHeap_->IsParallelGCEnabled() && sHeap_->CheckCanDistributeTask()) {
            sHeap_->PostGCMarkingTask();
        }
    }
}

bool ShareGCWorkManager::Pop(uint32_t threadId, TaggedObject **object)
{
    WorkNode *&outNode = works_.at(threadId).outNode_;
    WorkNode *&inNode = works_.at(threadId).inNode_;
    if (!outNode->PopObject(reinterpret_cast<uintptr_t *>(object))) {
        if (!inNode->IsEmpty()) {
            WorkNode *tmp = outNode;
            outNode = inNode;
            inNode = tmp;
        } else if (!PopWorkNodeFromGlobal(threadId)) {
            return false;
        }
        return outNode->PopObject(reinterpret_cast<uintptr_t *>(object));
    }
    return true;
}

bool ShareGCWorkManager::PopWorkNodeFromGlobal(uint32_t threadId)
{
    return workStack_.Pop(&works_.at(threadId).outNode_);
}
}  // namespace panda::ecmascript
