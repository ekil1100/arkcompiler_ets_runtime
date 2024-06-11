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

#include "ecmascript/builtins/builtins_promise_job.h"

#include "ecmascript/ecma_macros.h"
#include "ecmascript/global_env.h"
#include "ecmascript/interpreter/interpreter.h"
#include "ecmascript/js_function.h"
#include "ecmascript/js_handle.h"
#include "ecmascript/jspandafile/js_pandafile_executor.h"
#include "ecmascript/jspandafile/js_pandafile_manager.h"
#include "ecmascript/js_promise.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/module/js_dynamic_import.h"
#include "ecmascript/module/js_module_deregister.h"
#include "ecmascript/module/js_module_manager.h"
#include "ecmascript/module/module_path_helper.h"
#include "ecmascript/platform/file.h"
#include "ecmascript/require/js_cjs_module.h"
#include "libpandabase/macros.h"

namespace panda::ecmascript::builtins {
using JSRecordInfo = ecmascript::JSPandaFile::JSRecordInfo;
using ModulePathHelper = ecmascript::ModulePathHelper;
using PathHelper = ecmascript::base::PathHelper;

JSTaggedValue BuiltinsPromiseJob::PromiseReactionJob(EcmaRuntimeCallInfo *argv)
{
    ASSERT(argv);
    BUILTINS_API_TRACE(argv->GetThread(), PromiseJob, Reaction);
    JSThread *thread = argv->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    // 1. Assert: reaction is a PromiseReaction Record.
    JSHandle<JSTaggedValue> value = GetCallArg(argv, 0);
    ASSERT(value->IsPromiseReaction());
    JSHandle<PromiseReaction> reaction = JSHandle<PromiseReaction>::Cast(value);
    JSHandle<JSTaggedValue> argument = GetCallArg(argv, 1);

    const GlobalEnvConstants *globalConst = thread->GlobalConstants();
    // 2. Let promiseCapability be reaction.[[Capabilities]].
    JSHandle<PromiseCapability> capability(thread, reaction->GetPromiseCapability());
    // 3. Let handler be reaction.[[Handler]].
    JSHandle<JSTaggedValue> handler(thread, reaction->GetHandler());
    JSHandle<JSTaggedValue> call(thread, capability->GetResolve());
    const uint32_t argsLength = 1;
    JSHandle<JSTaggedValue> undefined = globalConst->GetHandledUndefined();
    EcmaRuntimeCallInfo *runtimeInfo =
        EcmaInterpreter::NewRuntimeCallInfo(thread, call, undefined, undefined, argsLength);
    RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
    if (handler->IsString()) {
        // 4. If handler is "Identity", let handlerResult be NormalCompletion(argument).
        // 5. Else if handler is "Thrower", let handlerResult be Completion{[[type]]: throw, [[value]]: argument,
        // [[target]]: empty}.
        runtimeInfo->SetCallArg(argument.GetTaggedValue());
        if (EcmaStringAccessor::StringsAreEqual(thread->GetEcmaVM(),
            JSHandle<EcmaString>(handler), JSHandle<EcmaString>(globalConst->GetHandledThrowerString()))) {
            runtimeInfo->SetFunction(capability->GetReject());
        }
    } else {
        // 6. Else, let handlerResult be Call(handler, undefined, «argument»).
        EcmaRuntimeCallInfo *info =
            EcmaInterpreter::NewRuntimeCallInfo(thread, handler, undefined, undefined, argsLength);
        RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
        info->SetCallArg(argument.GetTaggedValue());
        JSTaggedValue taggedValue = JSFunction::Call(info);
        // 7. If handlerResult is an abrupt completion, then
        // a. Let status be Call(promiseCapability.[[Reject]], undefined, «handlerResult.[[value]]»).
        // b. NextJob Completion(status).
        if (thread->HasPendingException()) {
            JSHandle<JSTaggedValue> throwValue = JSPromise::IfThrowGetThrowValue(thread);
            runtimeInfo->SetCallArg(throwValue.GetTaggedValue());
            thread->ClearException();
            runtimeInfo->SetFunction(capability->GetReject());
        } else {
            runtimeInfo->SetCallArg(taggedValue);
        }
    }
    // 8. Let status be Call(promiseCapability.[[Resolve]], undefined, «handlerResult.[[value]]»).
    return JSFunction::Call(runtimeInfo);
}

JSTaggedValue BuiltinsPromiseJob::PromiseResolveThenableJob(EcmaRuntimeCallInfo *argv)
{
    ASSERT(argv);
    BUILTINS_API_TRACE(argv->GetThread(), PromiseJob, ResolveThenableJob);
    JSThread *thread = argv->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> promise = GetCallArg(argv, 0);
    ASSERT(promise->IsJSPromise());
    // 1. Let resolvingFunctions be CreateResolvingFunctions(promiseToResolve).
    JSHandle<ResolvingFunctionsRecord> resolvingFunctions =
        JSPromise::CreateResolvingFunctions(thread, JSHandle<JSPromise>::Cast(promise));
    JSHandle<JSTaggedValue> thenable = GetCallArg(argv, 1);
    JSHandle<JSTaggedValue> then = GetCallArg(argv, BuiltinsBase::ArgsPosition::THIRD);

    // 2. Let thenCallResult be Call(then, thenable, «resolvingFunctions.[[Resolve]], resolvingFunctions.[[Reject]]»).
    const uint32_t argsLength = 2; // 2: «resolvingFunctions.[[Resolve]], resolvingFunctions.[[Reject]]»
    JSHandle<JSTaggedValue> undefined = thread->GlobalConstants()->GetHandledUndefined();
    EcmaRuntimeCallInfo *info = EcmaInterpreter::NewRuntimeCallInfo(thread, then, thenable, undefined, argsLength);
    RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
    info->SetCallArg(resolvingFunctions->GetResolveFunction(), resolvingFunctions->GetRejectFunction());
    JSTaggedValue result = JSFunction::Call(info);
    JSHandle<JSTaggedValue> thenResult(thread, result);
    // 3. If thenCallResult is an abrupt completion,
    // a. Let status be Call(resolvingFunctions.[[Reject]], undefined, «thenCallResult.[[value]]»).
    // b. NextJob Completion(status).
    if (thread->HasPendingException()) {
        thenResult = JSPromise::IfThrowGetThrowValue(thread);
        thread->ClearException();
        JSHandle<JSTaggedValue> reject(thread, resolvingFunctions->GetRejectFunction());
        EcmaRuntimeCallInfo *runtimeInfo =
            EcmaInterpreter::NewRuntimeCallInfo(thread, reject, undefined, undefined, 1);
        RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
        runtimeInfo->SetCallArg(thenResult.GetTaggedValue());
        return JSFunction::Call(runtimeInfo);
    }
    // 4. NextJob Completion(thenCallResult).
    return result;
}

JSTaggedValue BuiltinsPromiseJob::DynamicImportJob(EcmaRuntimeCallInfo *argv)
{
    ASSERT(argv);
    BUILTINS_API_TRACE(argv->GetThread(), PromiseJob, DynamicImportJob);
    JSThread *thread = argv->GetThread();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    EcmaVM *vm = thread->GetEcmaVM();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSPromiseReactionsFunction> resolve(GetCallArg(argv, 0));
    JSHandle<JSPromiseReactionsFunction> reject(GetCallArg(argv, 1));   // 1 : reject method
    JSHandle<EcmaString> dirPath(GetCallArg(argv, 2));                  // 2 : current file path(containing file name)
    JSHandle<JSTaggedValue> specifier(GetCallArg(argv, 3));             // 3 : request module's path
    JSHandle<JSTaggedValue> recordName(GetCallArg(argv, 4));            // 4 : js recordName or undefined

    // Let specifierString be Completion(ToString(specifier))
    JSHandle<EcmaString> specifierString = JSTaggedValue::ToString(thread, specifier);
    RETURN_VALUE_IF_ABRUPT_COMPLETION(thread, CatchException(thread, reject));

    // Resolve request module's ohmurl
    JSMutableHandle<JSTaggedValue> moduleName(thread, thread->GlobalConstants()->GetUndefined());
    CString entryPoint = JSPandaFile::ENTRY_MAIN_FUNCTION;
    CString fileNameStr = ModulePathHelper::Utf8ConvertToString(dirPath.GetTaggedValue());
    CString requestPath = ModulePathHelper::Utf8ConvertToString(specifierString.GetTaggedValue());
    LOG_ECMA(DEBUG) << "Start importing dynamic module : " << requestPath;
    bool enableESMTrace = thread->GetEcmaVM()->GetJSOptions().EnableESMTrace();
    if (enableESMTrace) {
        CString traceInfo = "DynamicImportJob: " + requestPath;
        ECMA_BYTRACE_START_TRACE(HITRACE_TAG_ARK, traceInfo.c_str());
    }
    std::shared_ptr<JSPandaFile> curJsPandaFile;
    CString recordNameStr;
    if (!recordName->IsUndefined()) {
        recordNameStr = ModulePathHelper::Utf8ConvertToString(recordName.GetTaggedValue());
        curJsPandaFile = JSPandaFileManager::GetInstance()->LoadJSPandaFile(thread, fileNameStr, recordNameStr.c_str());
        if (curJsPandaFile == nullptr) {
            LOG_FULL(FATAL) << "Load current file's panda file failed. Current file is " << recordNameStr;
        }
        // translate requestPath to OhmUrl
        if (vm->IsNormalizedOhmUrlPack()) {
            ModulePathHelper::TranslateExpressionToNormalized(thread, curJsPandaFile.get(), fileNameStr, recordNameStr,
                requestPath);
            LOG_ECMA(DEBUG) << "Exit Translate Normalized OhmUrl for DynamicImport, resultPath: " << requestPath;
            RETURN_VALUE_IF_ABRUPT_COMPLETION(thread, CatchException(thread, reject));
        } else if (ModulePathHelper::NeedTranstale(requestPath)) {
            ModulePathHelper::TranstaleExpressionInput(curJsPandaFile.get(), requestPath);
            LOG_ECMA(DEBUG) << "Exit Translate OhmUrl for DynamicImport, resultPath: " << requestPath;
        }
        specifierString = thread->GetEcmaVM()->GetFactory()->NewFromUtf8(requestPath);
    }
    // resolve native module
    auto [isNative, moduleType] = SourceTextModule::CheckNativeModule(requestPath);
    ModuleManager *moduleManager = thread->GetCurrentEcmaContext()->GetModuleManager();
    if (isNative) {
        return DynamicImport::ExecuteNativeOrJsonModule(thread, specifierString, moduleType, resolve, reject);
    }

    if (recordName->IsUndefined()) {
        moduleName.Update(ResolveFilenameFromNative(thread, dirPath.GetTaggedValue(),
            specifierString.GetTaggedValue()).GetTaggedValue());
        RETURN_VALUE_IF_ABRUPT_COMPLETION(thread, CatchException(thread, reject));
        fileNameStr =
            ModulePathHelper::Utf8ConvertToString(moduleName.GetTaggedValue());
    } else {
        entryPoint =
            ModulePathHelper::ConcatFileNameWithMerge(thread, curJsPandaFile.get(),
                fileNameStr, recordNameStr, requestPath);
        RETURN_VALUE_IF_ABRUPT_COMPLETION(thread, CatchException(thread, reject));
        moduleName.Update(factory->NewFromUtf8(entryPoint).GetTaggedValue());
    }
    std::shared_ptr<JSPandaFile> jsPandaFile =
        JSPandaFileManager::GetInstance()->LoadJSPandaFile(thread, fileNameStr, entryPoint);
    if (jsPandaFile == nullptr) {
        LOG_FULL(FATAL) << "Load current file's panda file failed. Current file is " << fileNameStr;
    }

    JSRecordInfo recordInfo;
    bool hasRecord = jsPandaFile->CheckAndGetRecordInfo(entryPoint, recordInfo);
    if (!hasRecord) {
        CString normalizeStr = ModulePathHelper::ReformatPath(entryPoint);
        CString msg =  "Cannot find dynamic-import module '" + normalizeStr;
        JSTaggedValue error = factory->GetJSError(ErrorType::REFERENCE_ERROR,
            msg.c_str(), StackCheck::NO).GetTaggedValue();
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, CatchException(thread, reject));
    }
    if (jsPandaFile->IsJson(recordInfo)) {
        JSHandle<EcmaString> jsonRecordName = factory->NewFromUtf8(entryPoint);
        return DynamicImport::ExecuteNativeOrJsonModule(
            thread, jsonRecordName, ModuleTypes::JSON_MODULE, resolve, reject, jsPandaFile.get());
    }
    // Loading request module.
    thread->GetEcmaVM()->PushToDeregisterModuleList(entryPoint);
    if (!moduleManager->IsModuleLoaded(moduleName.GetTaggedValue())) {
        if (!JSPandaFileExecutor::ExecuteFromAbcFile(thread, fileNameStr.c_str(), entryPoint.c_str(), false, true)) {
            CString msg = "Cannot execute request dynamic-imported module : " + entryPoint;
            JSTaggedValue error = factory->GetJSError(ErrorType::REFERENCE_ERROR, msg.c_str(),
                                                      StackCheck::NO).GetTaggedValue();
            THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, CatchException(thread, reject));
        }
    } else {
        ModuleDeregister::ReviseLoadedModuleCount(thread, moduleName.GetTaggedValue());
    }

    RETURN_VALUE_IF_ABRUPT_COMPLETION(thread, CatchException(thread, reject));
    JSMutableHandle<JSTaggedValue> moduleNamespace(thread, JSTaggedValue::Undefined());
    // only support importing es module, or return a default object.
    if (!jsPandaFile->IsModule(recordInfo)) {
        moduleNamespace.Update(vm->GetGlobalEnv()->GetExportOfScript());
    } else {
        // b. Let moduleRecord be ! HostResolveImportedModule(referencingScriptOrModule, specifier).
        JSHandle<SourceTextModule> moduleRecord =
            moduleManager->GetImportedModule(moduleName.GetTaggedValue());
        JSHandle<JSTaggedValue> nameSp = SourceTextModule::GetModuleNamespace(thread, moduleRecord);
        RETURN_VALUE_IF_ABRUPT_COMPLETION(thread, CatchException(thread, reject));
        // d. Let namespace be ? GetModuleNamespace(moduleRecord).
        moduleNamespace.Update(nameSp);
    }
    JSHandle<JSTaggedValue> undefined = thread->GlobalConstants()->GetHandledUndefined();
    EcmaRuntimeCallInfo *info =
        EcmaInterpreter::NewRuntimeCallInfo(thread,
                                            JSHandle<JSTaggedValue>(resolve),
                                            undefined, undefined, 1);
    RETURN_VALUE_IF_ABRUPT_COMPLETION(thread, CatchException(thread, reject));
    info->SetCallArg(moduleNamespace.GetTaggedValue());
    RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
    if (enableESMTrace) {
        ECMA_BYTRACE_FINISH_TRACE(HITRACE_TAG_ARK);
    }
    return JSFunction::Call(info);
}

JSTaggedValue BuiltinsPromiseJob::CatchException(JSThread *thread, JSHandle<JSPromiseReactionsFunction> reject)
{
    BUILTINS_API_TRACE(thread, PromiseJob, CatchException);
    JSHandle<JSTaggedValue> undefined = thread->GlobalConstants()->GetHandledUndefined();
    ASSERT(thread->HasPendingException());
    JSHandle<JSTaggedValue> thenResult = JSPromise::IfThrowGetThrowValue(thread);
    thread->ClearException();
    JSHandle<JSTaggedValue> rejectfun(reject);
    EcmaRuntimeCallInfo *runtimeInfo =
        EcmaInterpreter::NewRuntimeCallInfo(thread, rejectfun, undefined, undefined, 1);
    RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
    runtimeInfo->SetCallArg(thenResult.GetTaggedValue());
    return JSFunction::Call(runtimeInfo);
}
}  // namespace panda::ecmascript::builtins
