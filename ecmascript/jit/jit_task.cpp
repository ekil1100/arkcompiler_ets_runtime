/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "ecmascript/jit/jit_task.h"
#include "ecmascript/jspandafile/program_object.h"
#include "ecmascript/ohos/jit_tools.h"
#ifdef JIT_ENABLE_CODE_SIGN
#include "jit_buffer_integrity.h"
#endif

namespace panda::ecmascript {

JitTaskpool *JitTaskpool::GetCurrentTaskpool()
{
    static JitTaskpool *taskpool = new JitTaskpool();
    return taskpool;
}

uint32_t JitTaskpool::TheMostSuitableThreadNum([[maybe_unused]]uint32_t threadNum) const
{
    return 1;
}

JitTask::JitTask(JSThread *hostThread, JSThread *compilerThread, Jit *jit, JSHandle<JSFunction> &jsFunction,
    CompilerTier tier, CString &methodName, int32_t offset, uint32_t taskThreadId,
    JitCompileMode mode)
    : hostThread_(hostThread),
    compilerThread_(compilerThread),
    jit_(jit),
    jsFunction_(jsFunction),
    compilerTask_(nullptr),
    state_(CompileState::SUCCESS),
    compilerTier_(tier),
    methodName_(methodName),
    offset_(offset),
    taskThreadId_(taskThreadId),
    ecmaContext_(nullptr),
    jitCompileMode_(mode),
    runState_(RunState::INIT)
{
    jit->IncJitTaskCnt(hostThread);
    ecmaContext_ = hostThread->GetCurrentEcmaContext();
    sustainingJSHandle_ = std::make_unique<SustainingJSHandle>(hostThread->GetEcmaVM());
}

void JitTask::PrepareCompile()
{
    CloneProfileTypeInfo();
    SustainingJSHandles();
    compilerTask_ = jit_->CreateJitCompilerTask(this);

    Method *method = Method::Cast(jsFunction_->GetMethod().GetTaggedObject());
    JSTaggedValue constpool = method->GetConstantPool();
    if (!ConstantPool::CheckUnsharedConstpool(constpool)) {
        hostThread_->GetCurrentEcmaContext()->FindOrCreateUnsharedConstpool(constpool);
    }

    SetRunState(RunState::INIT);
}

void JitTask::Optimize()
{
    ECMA_BYTRACE_NAME(HITRACE_TAG_ARK, "JIT::Compiler frontend");
    bool res = jit_->JitCompile(compilerTask_, this);
    if (!res) {
        SetCompileFailed();
    }
}

void JitTask::Finalize()
{
    if (!IsCompileSuccess()) {
        return;
    }

    ECMA_BYTRACE_NAME(HITRACE_TAG_ARK, "JIT::Compiler backend");
    bool res = jit_->JitFinalize(compilerTask_, this);
    if (!res) {
        SetCompileFailed();
    }
}

void JitTask::InstallOsrCode(JSHandle<MachineCode> &codeObj)
{
    auto profile = jsFunction_->GetProfileTypeInfo();
    if (profile.IsUndefined()) {
        LOG_JIT(DEBUG) << "[OSR] Empty profile for installing code:" << GetMethodName();
        return;
    }
    FuncEntryDes *funcEntryDes = reinterpret_cast<FuncEntryDes*>(codeObj->GetFuncEntryDes());
    jsFunction_->SetIsCompiledFastCall(funcEntryDes->isFastCall_);
    JSHandle<ProfileTypeInfo> profileInfoHandle =
        JSHandle<ProfileTypeInfo>::Cast(JSHandle<JSTaggedValue>(hostThread_, profile));
    uint32_t slotId = profileInfoHandle->GetIcSlotToOsrLength() - 1; // 1 : get last slot
    auto profileData = profileInfoHandle->Get(slotId);
    auto factory = hostThread_->GetEcmaVM()->GetFactory();
    if (!profileData.IsTaggedArray()) {
        const uint32_t initLen = 1;
        JSHandle<TaggedArray> newArr = factory->NewTaggedArray(initLen);
        newArr->Set(hostThread_, 0, codeObj.GetTaggedValue());
        profileInfoHandle->Set(hostThread_, slotId, newArr.GetTaggedValue());
        LOG_JIT(DEBUG) << "[OSR] Install machine code:" << GetMethodName()
                       << ", code address: " << reinterpret_cast<void*>(codeObj->GetFuncAddr())
                       << ", index: " << newArr->GetLength() - 1;
        return;
    }
    JSHandle<TaggedArray> arr(hostThread_, profileData);
    JSHandle<TaggedArray> newArr = factory->NewTaggedArray(arr->GetLength() + 1);  // 1 : added for current codeObj
    uint32_t i = 0;
    for (; i < arr->GetLength(); i++) {
        newArr->Set(hostThread_, i, arr->Get(i));
    }
    newArr->Set(hostThread_, i, codeObj.GetTaggedValue());
    profileInfoHandle->Set(hostThread_, slotId, newArr.GetTaggedValue());
    LOG_JIT(DEBUG) << "[OSR] Install machine code:" << GetMethodName()
                   << ", code address: " << reinterpret_cast<void*>(codeObj->GetFuncAddr())
                   << ", index: " << newArr->GetLength() - 1;
    return;
}

static void ComputeAlignedSizes(MachineCodeDesc &desc)
{
    desc.funcEntryDesSizeAlign = AlignUp(desc.funcEntryDesSize, MachineCode::TEXT_ALIGN);
    desc.stackMapSizeAlign = AlignUp(desc.stackMapOrOffsetTableSize, MachineCode::DATA_ALIGN);
    desc.rodataSizeBeforeTextAlign = AlignUp(desc.rodataSizeBeforeText, MachineCode::TEXT_ALIGN);

    if (desc.codeType == MachineCodeType::BASELINE_CODE) {
        desc.codeSizeAlign = Jit::GetInstance()->IsEnableJitFort() ?
            AlignUp(desc.codeSize, MachineCode::TEXT_ALIGN) :
            AlignUp(desc.codeSize, MachineCode::DATA_ALIGN);
        return;
    }

    // FastJit
    if (Jit::GetInstance()->IsEnableJitFort()) {
        // align for multiple instruction blocks installed in JitFort
        if (desc.rodataSizeAfterText) {
            desc.codeSizeAlign = AlignUp(desc.codeSize, MachineCode::DATA_ALIGN);
            desc.rodataSizeAfterTextAlign = AlignUp(desc.rodataSizeAfterText, MachineCode::TEXT_ALIGN);
        } else {
            desc.codeSizeAlign = AlignUp(desc.codeSize, MachineCode::TEXT_ALIGN);
        }
    } else {
        desc.codeSizeAlign = AlignUp(desc.codeSize, MachineCode::DATA_ALIGN);
        desc.rodataSizeAfterTextAlign = AlignUp(desc.rodataSizeAfterText, MachineCode::DATA_ALIGN);
    }
}

static size_t ComputePayLoadSize(MachineCodeDesc &codeDesc)
{
    ComputeAlignedSizes(codeDesc);
    if (codeDesc.codeType == MachineCodeType::BASELINE_CODE) {
        // only code section in BaselineCode
        if (Jit::GetInstance()->IsEnableJitFort()) {
            size_t payLoadSize = codeDesc.stackMapSizeAlign + codeDesc.codeSizeAlign;
            size_t allocSize = AlignUp(payLoadSize + MachineCode::SIZE,
                static_cast<size_t>(MemAlignment::MEM_ALIGN_OBJECT));
            codeDesc.instructionsSize = codeDesc.codeSizeAlign;
            LOG_JIT(DEBUG) << "InstallCode:: MachineCode Object size to allocate: "
                << allocSize << " (instruction size): " << codeDesc.codeSizeAlign;
            if (allocSize > MAX_REGULAR_HEAP_OBJECT_SIZE) {
                return payLoadSize;
            } else {
                // regular sized machine code object instructions are installed in separate jit fort space
                return payLoadSize - codeDesc.codeSizeAlign;
            }
        } else {
            return codeDesc.stackMapSizeAlign + codeDesc.codeSizeAlign;
        }
    }

    ASSERT(codeDesc.codeType == MachineCodeType::FAST_JIT_CODE);
    if (Jit::GetInstance()->IsEnableJitFort()) {
        // instructionsSize: size of JIT generated native instructions
        // payLoadSize: size of JIT generated output including native code
        size_t instructionsSize =
            codeDesc.rodataSizeBeforeTextAlign + codeDesc.codeSizeAlign + codeDesc.rodataSizeAfterTextAlign;
        size_t payLoadSize = codeDesc.funcEntryDesSizeAlign + instructionsSize + codeDesc.stackMapSizeAlign;
        size_t allocSize = AlignUp(payLoadSize + MachineCode::SIZE,
            static_cast<size_t>(MemAlignment::MEM_ALIGN_OBJECT));
        LOG_JIT(DEBUG) << "InstallCode:: MachineCode Object size to allocate: "
            << allocSize << " (instruction size): " << instructionsSize;

        codeDesc.instructionsSize = instructionsSize;
        if (allocSize > MAX_REGULAR_HEAP_OBJECT_SIZE) {
            //
            // A Huge machine code object is consisted of contiguous 256Kb aligned blocks.
            // With JitFort, a huge machine code object starts with a page aligned mutable area
            // (that holds Region and MachineCode object header, FuncEntryDesc and StackMap), followed
            // by a page aligned nonmutable (JitFort space) area of JIT generated native instructions.
            // i.e.
            // mutableSize = align up to PageSize
            //     (sizeof(Region) + HUGE_OBJECT_BITSET_SIZE +MachineCode::SIZE + payLoadSize - instructionsSize)
            // immutableSize = instructionsSize (native page boundary aligned)
            // See comments at HugeMachineCodeSpace::Allocate()
            //
            codeDesc.isHugeObj = true;
            return payLoadSize;
        } else {
            // regular sized machine code object instructions are installed in separate jit fort space
            return payLoadSize - instructionsSize;
        }
    } else {
        return codeDesc.funcEntryDesSizeAlign + codeDesc.rodataSizeBeforeTextAlign + codeDesc.codeSizeAlign +
               codeDesc.rodataSizeAfterTextAlign + codeDesc.stackMapSizeAlign;
    }
}

void DumpJitCode(JSHandle<MachineCode> &machineCode, JSHandle<Method> &method)
{
    if (!ohos::JitTools::GetJitDumpObjEanble()) {
        return;
    }
    JsJitDumpElf jitDumpElf;
    jitDumpElf.Init();
    char *funcAddr = reinterpret_cast<char *>(machineCode->GetFuncAddr());
    size_t len = machineCode->GetTextSize();
    std::vector<uint8> vec(len);
    if (memmove_s(vec.data(), len, funcAddr, len) != EOK) {
        LOG_JIT(DEBUG) << "Fail to get machineCode on function addr: " << funcAddr;
    }
    jitDumpElf.AppendData(vec);
    const char *filename =  method->GetMethodName();
    std::string fileName = std::string(filename);
    uintptr_t addr = machineCode->GetFuncAddr();
    fileName = fileName + "_" + std::to_string(addr) + "+" + std::to_string(len);
    jitDumpElf.AppendSymbolToSymTab(0, 0, len, std::string(filename));
    std::string realOutPath;
    std::string sanboxPath = panda::os::file::File::GetExtendedFilePath(AotCrashInfo::GetSandBoxPath());
    if (!ecmascript::RealPath(sanboxPath, realOutPath, false)) {
        return;
    }
    std::string outFile = realOutPath + "/" + std::string(fileName);
    if (!ecmascript::FileExist(outFile.c_str())) {
        return;
    }
    int fd = open(outFile.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    jitDumpElf.WriteJitElfFile(fd);
    close(fd);
}

void JitTask::InstallCode()
{
    if (!IsCompileSuccess()) {
        return;
    }
    [[maybe_unused]] EcmaHandleScope handleScope(hostThread_);

    JSHandle<Method> methodHandle(hostThread_, Method::Cast(jsFunction_->GetMethod().GetTaggedObject()));

    size_t size = ComputePayLoadSize(codeDesc_);

    codeDesc_.isAsyncCompileMode = IsAsyncTask();
    JSHandle<MachineCode> machineCodeObj;
    if (Jit::GetInstance()->IsEnableJitFort()) {
        // skip install if JitFort out of memory
        TaggedObject *machineCode = hostThread_->GetEcmaVM()->GetFactory()->NewMachineCodeObject(size, codeDesc_);
        if (machineCode == nullptr) {
            LOG_JIT(DEBUG) << "InstallCode skipped. NewMachineCode NULL for size " << size;
            if (hostThread_->HasPendingException()) {
                hostThread_->SetMachineCodeLowMemory(true);
                hostThread_->ClearException();
            }
            return;
        }
        machineCodeObj = hostThread_->GetEcmaVM()->GetFactory()->SetMachineCodeObjectData(
            machineCode, size, codeDesc_, methodHandle);
    } else {
        machineCodeObj = hostThread_->GetEcmaVM()->GetFactory()->NewMachineCodeObject(
            size, codeDesc_, methodHandle);
    }
    if (machineCodeObj.GetAddress() == ToUintPtr(nullptr)) {
        // skip install
        return;
    }
    machineCodeObj->SetOSROffset(offset_);

    if (hostThread_->HasPendingException()) {
        // check is oom exception
        hostThread_->SetMachineCodeLowMemory(true);
        hostThread_->ClearException();
    }

    if (IsOsrTask()) {
        InstallOsrCode(machineCodeObj);
    } else {
        InstallCodeByCompilerTier(machineCodeObj, methodHandle);
    }

    // sometimes get ILL_ILLOPC error if i-cache  not flushed for Jit code
    uintptr_t codeAddr = machineCodeObj->GetFuncAddr();
    uintptr_t codeAddrEnd = codeAddr + machineCodeObj->GetInstructionsSize();
    __builtin___clear_cache(reinterpret_cast<char *>(codeAddr), reinterpret_cast<char*>(codeAddrEnd));

    if (Jit::GetInstance()->IsEnableJitFort()) {
        if (codeDesc_.memDesc) {
            ASSERT(codeDesc_.isHugeObj == false);
            codeDesc_.memDesc->SetInstalled(true);
        }
    }
}

void JitTask::InstallCodeByCompilerTier(JSHandle<MachineCode> &machineCodeObj,
    JSHandle<Method> &methodHandle)
{
    uintptr_t codeAddr = machineCodeObj->GetFuncAddr();
    DumpJitCode(machineCodeObj, methodHandle);
    if (compilerTier_ == CompilerTier::FAST) {
        FuncEntryDes *funcEntryDes = reinterpret_cast<FuncEntryDes*>(machineCodeObj->GetFuncEntryDes());
        jsFunction_->SetCompiledFuncEntry(codeAddr, funcEntryDes->isFastCall_);
        methodHandle->SetDeoptThreshold(hostThread_->GetEcmaVM()->GetJSOptions().GetDeoptThreshold());
        jsFunction_->SetMachineCode(hostThread_, machineCodeObj);
        jsFunction_->SetJitMachineCodeCache(hostThread_, machineCodeObj);
        uintptr_t codeAddrEnd = codeAddr + machineCodeObj->GetInstructionsSize();
        LOG_JIT(DEBUG) <<"Install fast jit machine code:" << GetMethodName() << ", code range:" <<
            reinterpret_cast<void*>(codeAddr) <<"--" << reinterpret_cast<void*>(codeAddrEnd);
#if ECMASCRIPT_ENABLE_JIT_WARMUP_PROFILER
        auto &profMap = JitWarmupProfiler::GetInstance()->profMap_;
        if (profMap.find(GetMethodName()) != profMap.end()) {
            profMap[GetMethodName()] = true;
        }
#endif
    } else {
        ASSERT(compilerTier_ == CompilerTier::BASELINE);
        methodHandle->SetDeoptThreshold(hostThread_->GetEcmaVM()->GetJSOptions().GetDeoptThreshold());
        jsFunction_->SetBaselineCode(hostThread_, machineCodeObj);
        LOG_BASELINEJIT(DEBUG) <<"Install baseline jit machine code:" << GetMethodName();
    }
}

void JitTask::SustainingJSHandles()
{
    // transfer to sustaining handle
    JSHandle<JSFunction> sustainingJsFunctionHandle = sustainingJSHandle_->NewHandle(jsFunction_);
    SetJsFunction(sustainingJsFunctionHandle);

    JSHandle<ProfileTypeInfo> profileTypeInfo = sustainingJSHandle_->NewHandle(profileTypeInfo_);
    SetProfileTypeInfo(profileTypeInfo);
}

void JitTask::ReleaseSustainingJSHandle()
{
    // in abort case, vm exit before task finish, release by explict
    sustainingJSHandle_ = nullptr;
}

void JitTask::CloneProfileTypeInfo()
{
    [[maybe_unused]] EcmaHandleScope handleScope(hostThread_);

    Method *method = Method::Cast(jsFunction_->GetMethod().GetTaggedObject());
    uint32_t slotSize = method->GetSlotSize();
    JSTaggedValue profileTypeInfoVal = jsFunction_->GetProfileTypeInfo();
    JSHandle<ProfileTypeInfo> newProfileTypeInfo;
    ObjectFactory *factory = hostThread_->GetEcmaVM()->GetFactory();
    if (profileTypeInfoVal.IsUndefined() || slotSize == 0) {
        slotSize = slotSize == 0 ? 1 : slotSize; // there's no profiletypeinfo, just generate a temp profiletypeinfo
        newProfileTypeInfo = factory->NewProfileTypeInfo(slotSize);
    } else {
        JSHandle<ProfileTypeInfo> profileTypeInfo(hostThread_,
            ProfileTypeInfo::Cast(profileTypeInfoVal.GetTaggedObject()));
        newProfileTypeInfo = factory->NewProfileTypeInfo(slotSize);
        for (uint32_t i = 0; i < slotSize; i++) {
            JSTaggedValue value = profileTypeInfo->Get(i);
            newProfileTypeInfo->Set(hostThread_, i, value);
        }
    }
    SetProfileTypeInfo(newProfileTypeInfo);
}

JitTask::~JitTask()
{
    ReleaseSustainingJSHandle();
    jit_->DeleteJitCompile(compilerTask_);
    jit_->DecJitTaskCnt(hostThread_);
}

void JitTask::WaitFinish()
{
    LockHolder lock(runStateMutex_);
    if (!IsFinish()) {
        runStateCondition_.Wait(&runStateMutex_);
    }
}

bool JitTask::AsyncTask::CopyCodeToFort()
{
    MachineCodeDesc &desc = jitTask_->GetMachineCodeDesc();
    uint8_t *pText = reinterpret_cast<uint8_t*>(desc.instructionsAddr);
    if (desc.rodataSizeBeforeTextAlign != 0) {
        pText += desc.rodataSizeBeforeTextAlign;
    }
#ifdef JIT_ENABLE_CODE_SIGN
    if ((uintptr_t)desc.codeSigner == 0) {
        if (memcpy_s(pText, desc.codeSizeAlign, reinterpret_cast<uint8_t*>(desc.codeAddr), desc.codeSize) != EOK) {
            LOG_JIT(ERROR) << "memcpy failed in CopyToCache";
            return false;
        }
    } else {
        LOG_JIT(DEBUG) << "Copy: "
                       << std::hex << (uintptr_t)pText << " <- "
                       << std::hex << (uintptr_t)desc.codeAddr << " size: " << desc.codeSize;
        LOG_JIT(DEBUG) << "     codeSigner = " << std::hex << (uintptr_t)desc.codeSigner;
        OHOS::Security::CodeSign::JitCodeSignerBase *signer =
            reinterpret_cast<OHOS::Security::CodeSign::JitCodeSignerBase*>(desc.codeSigner);
        int err = OHOS::Security::CodeSign::CopyToJitCode(
            signer, pText, reinterpret_cast<void *>(desc.codeAddr), desc.codeSize);
        if (err != EOK) {
            LOG_JIT(ERROR) << "     CopyToJitCode failed, err: " << err;
            return false;
        } else {
            LOG_JIT(DEBUG) << "     CopyToJitCode success!!";
        }
        delete reinterpret_cast<OHOS::Security::CodeSign::JitCodeSignerBase*>(desc.codeSigner);
    }
#else
    if (memcpy_s(pText, desc.codeSizeAlign, reinterpret_cast<uint8_t*>(desc.codeAddr), desc.codeSize) != EOK) {
        LOG_JIT(ERROR) << "memcpy failed in CopyToCache";
        return false;
    }
#endif
    return true;
}

bool JitTask::AsyncTask::AllocFromFortAndCopy()
{
    Jit::JitGCLockHolder lock(jitTask_->GetHostThread());

    MachineCodeDesc &desc = jitTask_->GetMachineCodeDesc();
    size_t size = ComputePayLoadSize(desc);
    const Heap *heap = jitTask_->GetHostThread()->GetEcmaVM()->GetHeap();

    if (desc.isHugeObj) {
        Region *region = heap->GetHugeMachineCodeSpace()->AllocateFort(
            size + MachineCode::SIZE, heap->GetJSThread(), &desc);
        if (!region || !desc.instructionsAddr) {
            return false;
        }
        desc.hugeObjRegion = ToUintPtr(region);
    } else {
        uintptr_t mem = heap->GetMachineCodeSpace()->JitFortAllocate(&desc);
        if (mem == ToUintPtr(nullptr)) {
            LOG_JIT(DEBUG) << "JitTask: AsyncTask JitFort allocate returned null";
            return false;
        }
        desc.instructionsAddr = mem;
    }

    if (!CopyCodeToFort()) {
        return false;
    }
    return true;
}

bool JitTask::AsyncTask::Run([[maybe_unused]] uint32_t threadIndex)
{
    if (IsTerminate() || !jitTask_->GetHostThread()->GetEcmaVM()->IsInitialized()) {
        return false;
    }
    DISALLOW_HEAP_ACCESS;

    CString info = "compile method:" + jitTask_->GetMethodName();
    ECMA_BYTRACE_NAME(HITRACE_TAG_ARK, ConvertToStdString("JIT::Compile:" + info));

    AsyncTaskRunScope asyncTaskRunScope(jitTask_.get());

    if (jitTask_->GetJsFunction().GetAddress() == 0) {
        // for unit test
    } else {
        info = info + ", in jit thread";
        Jit::TimeScope scope(jitTask_->GetHostThread()->GetEcmaVM(), info, jitTask_->GetCompilerTier());

        jitTask_->Optimize();
        jitTask_->Finalize();

        // info main thread compile complete
        if (!jitTask_->IsCompileSuccess()) {
            return false;
        }

        if (jitTask_->IsAsyncTask()) {
            if (Jit::GetInstance()->IsEnableJitFort() &&
                Jit::GetInstance()->IsEnableAsyncCopyToFort() &&
                AllocFromFortAndCopy() == false) {
                return false;
            }
            jitTask_->jit_->RequestInstallCode(jitTask_);
        }
        int compilerTime = scope.TotalSpentTimeInMicroseconds();
        JitDfx::GetInstance()->RecordSpentTimeAndPrintStatsLogInJitThread(compilerTime, jitTask_->methodName_,
            jitTask_->compilerTier_ == CompilerTier::BASELINE, jitTask_->mainThreadCompileTime_);
    }
    return true;
}

JitTask::AsyncTask::AsyncTaskRunScope::AsyncTaskRunScope(JitTask *jitTask)
{
    jitTask_ = jitTask;
    jitTask_->SetRunState(RunState::RUNNING);
    JSThread *compilerThread = jitTask_->GetCompilerThread();
    ASSERT(compilerThread->IsJitThread());
    JitThread *jitThread = static_cast<JitThread*>(compilerThread);
    jitvm_ = jitThread->GetJitVM();
    jitvm_->SetHostVM(jitTask_->GetHostThread());
}

JitTask::AsyncTask::AsyncTaskRunScope::~AsyncTaskRunScope()
{
    jitvm_->ReSetHostVM();
    jitTask_->SetRunStateFinish();
}
}  // namespace panda::ecmascript
