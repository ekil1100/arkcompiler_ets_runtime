/* * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_ECMA_CONTEXT_H
#define ECMASCRIPT_ECMA_CONTEXT_H

#include <optional>

#include "ecmascript/base/config.h"
#include "ecmascript/common.h"
#include "ecmascript/frames.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/js_handle.h"
#include "ecmascript/dfx/vmstat/opt_code_profiler.h"
#include "ecmascript/mem/c_containers.h"
#include "ecmascript/mem/visitor.h"
#include "ecmascript/regexp/regexp_parser_cache.h"
#include "libpandafile/file.h"

namespace panda {
class JSNApi;
namespace panda_file {
class File;
}  // namespace panda_file

namespace ecmascript {
class GlobalEnv;
class ObjectFactory;
class RegExpParserCache;
class JSPandaFileManager;
class JSPandaFile;
class EcmaStringTable;
class ConstantPool;
class JSPromise;
class RegExpExecResultCache;
enum class PromiseRejectionEvent : uint8_t;

template<typename T>
class JSHandle;
class JSThread;
class JSFunction;
class EcmaVM;
class ModuleManager;
class TSManager;
class AOTFileManager;
class QuickFixManager;
class OptCodeProfiler;

namespace job {
class MicroJobQueue;
}  // namespace job
namespace tooling {
class JsDebuggerManager;
}  // namespace tooling

using HostPromiseRejectionTracker = void (*)(const EcmaVM* vm,
                                             const JSHandle<JSPromise> promise,
                                             const JSHandle<JSTaggedValue> reason,
                                             PromiseRejectionEvent operation,
                                             void* data);
using PromiseRejectCallback = void (*)(void* info);
class EcmaContext {
public:
    static EcmaContext *Create(JSThread *thread);

    static bool Destroy(EcmaContext *context);

    EcmaContext(JSThread *thread);
    ~EcmaContext();

    EcmaVM *GetEcmaVM() const
    {
        return vm_;
    }

    bool Initialize();

    bool ExecutePromisePendingJob();

    static EcmaContext *ConstCast(const EcmaContext *context)
    {
        return const_cast<EcmaContext *>(context);
    }

    EcmaStringTable *GetEcmaStringTable() const
    {
        ASSERT(stringTable_ != nullptr);
        return stringTable_;
    }

    ModuleManager *GetModuleManager() const
    {
        return moduleManager_;
    }

    TSManager *GetTSManager() const
    {
        return tsManager_;
    }

    ARK_INLINE JSThread *GetJSThread() const
    {
        return thread_;
    }
    PromiseRejectCallback GetPromiseRejectCallback() const
    {
        return promiseRejectCallback_;
    }

    void SetPromiseRejectCallback(PromiseRejectCallback cb)
    {
        promiseRejectCallback_ = cb;
    }

    void SetData(void* data)
    {
        data_ = data;
    }

    void PromiseRejectionTracker(const JSHandle<JSPromise> &promise,
                                 const JSHandle<JSTaggedValue> &reason, PromiseRejectionEvent operation)
    {
        if (hostPromiseRejectionTracker_ != nullptr) {
            hostPromiseRejectionTracker_(vm_, promise, reason, operation, data_);
        }
    }

    void SetHostPromiseRejectionTracker(HostPromiseRejectionTracker cb)
    {
        hostPromiseRejectionTracker_ = cb;
    }
    void SetupRegExpResultCache();
    JSHandle<JSTaggedValue> GetRegExpCache() const
    {
        return JSHandle<JSTaggedValue>(reinterpret_cast<uintptr_t>(&regexpCache_));
    }

    RegExpParserCache *GetRegExpParserCache() const
    {
        ASSERT(regExpParserCache_ != nullptr);
        return regExpParserCache_;
    }

    void SetRegExpCache(JSTaggedValue newCache)
    {
        regexpCache_ = newCache;
    }
    JSHandle<ecmascript::JSTaggedValue> GetAndClearEcmaUncaughtException() const;
    JSHandle<ecmascript::JSTaggedValue> GetEcmaUncaughtException() const;
    void EnableUserUncaughtErrorHandler();

    void AddConstpool(const JSPandaFile *jsPandaFile, JSTaggedValue constpool, int32_t index = 0);

    bool HasCachedConstpool(const JSPandaFile *jsPandaFile) const;

    JSTaggedValue FindConstpool(const JSPandaFile *jsPandaFile, int32_t index);
    // For new version instruction.
    JSTaggedValue FindConstpool(const JSPandaFile *jsPandaFile, panda_file::File::EntityId id);
    std::optional<std::reference_wrapper<CMap<int32_t, JSTaggedValue>>> FindConstpools(
        const JSPandaFile *jsPandaFile);

    JSHandle<ConstantPool> PUBLIC_API FindOrCreateConstPool(const JSPandaFile *jsPandaFile,
                                                            panda_file::File::EntityId id);
    void CreateAllConstpool(const JSPandaFile *jsPandaFile);

    void HandleUncaughtException(JSTaggedValue exception);

    JSHandle<GlobalEnv> GetGlobalEnv() const;
    JSHandle<job::MicroJobQueue> GetMicroJobQueue() const;

    void PrintJSErrorInfo(const JSHandle<JSTaggedValue> &exceptionInfo);
    void Iterate(const RootVisitor &v, const RootRangeVisitor &rv);
    static void MountContext(JSThread *thread);
    static void UnmountContext(JSThread *thread);
    void SetMicroJobQueue(job::MicroJobQueue *queue);
    void SetGlobalEnv(GlobalEnv *global);

    OptCodeProfiler *GetOptCodeProfiler() const
    {
        return optCodeProfiler_;
    }    
private:
    void ClearBufferData();
    Expected<JSTaggedValue, bool> InvokeEcmaEntrypoint(const JSPandaFile *jsPandaFile, std::string_view entryPoint,
                                                       bool excuteFromJob = false);

    NO_MOVE_SEMANTIC(EcmaContext);
    NO_COPY_SEMANTIC(EcmaContext);

    JSThread *thread_ {nullptr};
    EcmaVM *vm_ {nullptr};
    ObjectFactory *factory_ {nullptr};
    ModuleManager *moduleManager_ {nullptr};

    bool isUncaughtExceptionRegistered_ {false};
    bool isProcessingPendingJob_ {false};

    EcmaStringTable *stringTable_ {nullptr};
    JSTaggedValue globalEnv_ {JSTaggedValue::Hole()};

    JSTaggedValue microJobQueue_ {JSTaggedValue::Hole()};

    // VM execution states.
    RegExpParserCache *regExpParserCache_ {nullptr};
    JSTaggedValue regexpCache_ {JSTaggedValue::Hole()};
    CMap<const JSPandaFile *, CMap<int32_t, JSTaggedValue>> cachedConstpools_ {};
    CString assetPath_;

    // VM resources.
    TSManager *tsManager_ {nullptr};

    // Registered Callbacks
    PromiseRejectCallback promiseRejectCallback_ {nullptr};
    HostPromiseRejectionTracker hostPromiseRejectionTracker_ {nullptr};
    void* data_ {nullptr};

    // opt code Profiler
    OptCodeProfiler *optCodeProfiler_ {nullptr};

    friend class JSPandaFileExecutor;
    friend class ObjectFactory;
    friend class panda::JSNApi;
};
}  // namespace ecmascript
}  // namespace panda
#endif // ECMASCRIPT_ECMA_CONTEXT_H
