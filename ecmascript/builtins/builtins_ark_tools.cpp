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

#include "ecmascript/builtins/builtins_ark_tools.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ecmascript/element_accessor-inl.h"
#include "ecmascript/js_function.h"
#include "ecmascript/base/string_helper.h"
#include "ecmascript/js_tagged_value-inl.h"
#include "ecmascript/mem/tagged_object-inl.h"
#include "ecmascript/napi/include/dfx_jsnapi.h"
#include "ecmascript/mem/clock_scope.h"
#include "ecmascript/property_detector-inl.h"
#include "ecmascript/js_arraybuffer.h"
#include "ecmascript/interpreter/fast_runtime_stub-inl.h"
#include "builtins_typedarray.h"
#include "ecmascript/jit/jit.h"

namespace panda::ecmascript::builtins {
using StringHelper = base::StringHelper;

#if defined(ECMASCRIPT_SUPPORT_CPUPROFILER)
constexpr char FILEDIR[] = "/data/storage/el2/base/files/";
#endif
JSTaggedValue BuiltinsArkTools::ObjectDump(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<EcmaString> str = JSTaggedValue::ToString(thread, GetCallArg(info, 0));
    RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
    // The default log level of ace_engine and js_runtime is error
    LOG_ECMA(ERROR) << ": " << EcmaStringAccessor(str).ToStdString();

    uint32_t numArgs = info->GetArgsNumber();
    for (uint32_t i = 1; i < numArgs; i++) {
        JSHandle<JSTaggedValue> obj = GetCallArg(info, i);
        std::ostringstream oss;
        obj->Dump(oss);

        // The default log level of ace_engine and js_runtime is error
        LOG_ECMA(ERROR) << ": " << oss.str();
    }

    return JSTaggedValue::Undefined();
}

JSTaggedValue BuiltinsArkTools::CompareHClass(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSTaggedValue> obj1 = GetCallArg(info, 0);
    JSHandle<JSTaggedValue> obj2 = GetCallArg(info, 1);
    JSHClass *obj1Hclass = obj1->GetTaggedObject()->GetClass();
    JSHClass *obj2Hclass = obj2->GetTaggedObject()->GetClass();
    std::ostringstream oss;
    obj1Hclass->Dump(oss);
    obj2Hclass->Dump(oss);
    bool res = (obj1Hclass == obj2Hclass);
    if (!res) {
        LOG_ECMA(ERROR) << "These two object don't share the same hclass:" << oss.str();
    }
    return JSTaggedValue(res);
}

JSTaggedValue BuiltinsArkTools::DumpHClass(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSTaggedValue> obj = GetCallArg(info, 0);
    JSHClass *objHclass = obj->GetTaggedObject()->GetClass();
    std::ostringstream oss;
    objHclass->Dump(oss);

    LOG_ECMA(ERROR) << "hclass:" << oss.str();
    return JSTaggedValue::Undefined();
}

JSTaggedValue BuiltinsArkTools::IsTSHClass(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    ASSERT(info->GetArgsNumber() == 1);
    JSHandle<JSTaggedValue> object = GetCallArg(info, 0);
    JSHClass *hclass = object->GetTaggedObject()->GetClass();
    bool isTSHClass = hclass->IsTS();
    return GetTaggedBoolean(isTSHClass);
}

JSTaggedValue BuiltinsArkTools::GetHClass(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    ASSERT(info->GetArgsNumber() == 1);
    JSHandle<JSTaggedValue> object = GetCallArg(info, 0);
    JSHClass *hclass = object->GetTaggedObject()->GetClass();
    return JSTaggedValue(hclass);
}

JSTaggedValue BuiltinsArkTools::HasTSSubtyping(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    ASSERT(info->GetArgsNumber() == 1);
    JSHandle<JSTaggedValue> object = GetCallArg(info, 0);
    JSHClass *hclass = object->GetTaggedObject()->GetClass();
    return GetTaggedBoolean(hclass->HasTSSubtyping());
}

JSTaggedValue BuiltinsArkTools::IsNotHoleProperty(EcmaRuntimeCallInfo *info)
{
    [[maybe_unused]] DisallowGarbageCollection noGc;
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    ASSERT(info->GetArgsNumber() == 2);  // 2 : object and key
    JSHandle<JSTaggedValue> object = GetCallArg(info, 0);
    JSTaggedValue key = GetCallArg(info, 1).GetTaggedValue();
    JSHClass *hclass = object->GetTaggedObject()->GetClass();
    int entry = JSHClass::FindPropertyEntry(thread, hclass, key);
    if (entry == -1) {
        return GetTaggedBoolean(false);
    }
    PropertyAttributes attr = LayoutInfo::Cast(hclass->GetLayout().GetTaggedObject())->GetAttr(entry);
    return GetTaggedBoolean(attr.IsNotHole());
}

JSTaggedValue BuiltinsArkTools::HiddenStackSourceFile(EcmaRuntimeCallInfo *info)
{
    [[maybe_unused]] DisallowGarbageCollection noGc;
    ASSERT(info);
    JSThread *thread = info->GetThread();
    thread->SetEnableStackSourceFile(false);
    return JSTaggedValue::True();
}

JSTaggedValue BuiltinsArkTools::ExcutePendingJob(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    thread->GetCurrentEcmaContext()->ExecutePromisePendingJob();
    return JSTaggedValue::True();
}

JSTaggedValue BuiltinsArkTools::GetLexicalEnv(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    ASSERT(info->GetArgsNumber() == 1);
    JSHandle<JSTaggedValue> object = GetCallArg(info, 0);
    if (object->IsHeapObject() && object->IsJSFunction()) {
        JSHandle<JSFunction> function = JSHandle<JSFunction>::Cast(object);
        return function->GetLexicalEnv();
    }
    return JSTaggedValue::Null();
}

JSTaggedValue BuiltinsArkTools::ForceFullGC(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    const_cast<Heap *>(info->GetThread()->GetEcmaVM()->GetHeap())->CollectGarbage(
        TriggerGCType::FULL_GC, GCReason::EXTERNAL_TRIGGER);
    return JSTaggedValue::True();
}

JSTaggedValue BuiltinsArkTools::HintGC(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    return JSTaggedValue(const_cast<Heap *>(info->GetThread()->GetEcmaVM()->GetHeap())->
        CheckAndTriggerHintGC());
}

JSTaggedValue BuiltinsArkTools::RemoveAOTFlag(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    ASSERT(info->GetArgsNumber() == 1);
    JSHandle<JSTaggedValue> object = GetCallArg(info, 0);
    if (object->IsHeapObject() && object->IsJSFunction()) {
        JSHandle<JSFunction> func = JSHandle<JSFunction>::Cast(object);
        JSHandle<Method> method = JSHandle<Method>(thread, func->GetMethod());
        method->SetAotCodeBit(false);
    }

    return JSTaggedValue::Undefined();
}

#if defined(ECMASCRIPT_SUPPORT_CPUPROFILER)
JSTaggedValue BuiltinsArkTools::StartCpuProfiler(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    auto vm = thread->GetEcmaVM();

    // get file name
    JSHandle<JSTaggedValue> fileNameValue = GetCallArg(info, 0);
    std::string fileName = "";
    if (fileNameValue->IsString()) {
        JSHandle<EcmaString> str = JSTaggedValue::ToString(thread, fileNameValue);
        RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
        fileName = EcmaStringAccessor(str).ToStdString() + ".cpuprofile";
    } else {
        fileName = GetProfileName();
    }

    if (!CreateFile(fileName)) {
        LOG_ECMA(ERROR) << "CreateFile failed " << fileName;
    }

    // get sampling interval
    JSHandle<JSTaggedValue> samplingIntervalValue = GetCallArg(info, 1);
    uint32_t interval = 500; // 500:Default Sampling interval 500 microseconds
    if (samplingIntervalValue->IsNumber()) {
        interval = JSTaggedValue::ToUint32(thread, samplingIntervalValue);
    }

    DFXJSNApi::StartCpuProfilerForFile(vm, fileName, interval);
    return JSTaggedValue::Undefined();
}

JSTaggedValue BuiltinsArkTools::StopCpuProfiler(EcmaRuntimeCallInfo *info)
{
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    auto vm = thread->GetEcmaVM();
    DFXJSNApi::StopCpuProfilerForFile(vm);

    return JSTaggedValue::Undefined();
}

std::string BuiltinsArkTools::GetProfileName()
{
    char time1[16] = {0}; // 16:Time format length
    char time2[16] = {0}; // 16:Time format length
    time_t timep = std::time(nullptr);
    struct tm nowTime1;
    localtime_r(&timep, &nowTime1);
    size_t result = 0;
    result = strftime(time1, sizeof(time1), "%Y%m%d", &nowTime1);
    if (result == 0) {
        LOG_ECMA(ERROR) << "get time failed";
        return "";
    }
    result = strftime(time2, sizeof(time2), "%H%M%S", &nowTime1);
    if (result == 0) {
        LOG_ECMA(ERROR) << "get time failed";
        return "";
    }
    std::string profileName = "cpuprofile-";
    profileName += time1;
    profileName += "TO";
    profileName += time2;
    profileName += ".cpuprofile";
    return profileName;
}

bool BuiltinsArkTools::CreateFile(std::string &fileName)
{
    std::string path = FILEDIR + fileName;
    if (access(path.c_str(), F_OK) == 0) {
        if (access(path.c_str(), W_OK) == 0) {
            fileName = path;
            return true;
        }
        LOG_ECMA(ERROR) << "file create failed, W_OK false";
        return false;
    }
    const mode_t defaultMode = S_IRUSR | S_IWUSR | S_IRGRP; // -rw-r--
    int fd = creat(path.c_str(), defaultMode);
    if (fd == -1) {
        fd = creat(fileName.c_str(), defaultMode);
        if (fd == -1) {
            LOG_ECMA(ERROR) << "file create failed, errno = "<< errno;
            return false;
        }
        close(fd);
        return true;
    } else {
        fileName = path;
        close(fd);
        return true;
    }
}
#endif

// It is used to check whether an object is a proto, and this function can be
// used to check whether the state machine of IC is faulty.
JSTaggedValue BuiltinsArkTools::IsPrototype(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSTaggedValue> obj = GetCallArg(info, 0);
    JSHClass *objHclass = obj->GetTaggedObject()->GetClass();
    return JSTaggedValue(objHclass->IsPrototype());
}

// It is used to check whether a function is aot compiled.
JSTaggedValue BuiltinsArkTools::IsAOTCompiled(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSTaggedValue> obj = GetCallArg(info, 0);
    JSHandle<JSFunction> func(thread, obj.GetTaggedValue());
    Method *method = func->GetCallTarget();
    return JSTaggedValue(method->IsAotWithCallField());
}

JSTaggedValue BuiltinsArkTools::GetElementsKind(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSTaggedValue> obj = GetCallArg(info, 0);
    JSHandle<JSObject> receiver(thread, obj.GetTaggedValue());
    ElementsKind kind = receiver->GetClass()->GetElementsKind();
    return JSTaggedValue(static_cast<uint32_t>(kind));
}

JSTaggedValue BuiltinsArkTools::IsRegExpReplaceDetectorValid(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    return JSTaggedValue(PropertyDetector::IsRegExpReplaceDetectorValid(env));
}

JSTaggedValue BuiltinsArkTools::IsSymbolIteratorDetectorValid(EcmaRuntimeCallInfo *info)
{
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSTaggedValue> kind = GetCallArg(info, 0);
    if (!kind->IsString()) {
        return JSTaggedValue::Undefined();
    }
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<EcmaString> mapString = factory->NewFromUtf8("Map");
    if (JSTaggedValue::Equal(thread, kind, JSHandle<JSTaggedValue>(mapString))) {
        return JSTaggedValue(PropertyDetector::IsMapIteratorDetectorValid(env));
    }
    JSHandle<EcmaString> setString = factory->NewFromUtf8("Set");
    if (JSTaggedValue::Equal(thread, kind, JSHandle<JSTaggedValue>(setString))) {
        return JSTaggedValue(PropertyDetector::IsSetIteratorDetectorValid(env));
    }
    JSHandle<EcmaString> stringString = factory->NewFromUtf8("String");
    if (JSTaggedValue::Equal(thread, kind, JSHandle<JSTaggedValue>(stringString))) {
        return JSTaggedValue(PropertyDetector::IsStringIteratorDetectorValid(env));
    }
    JSHandle<EcmaString> arrayString = factory->NewFromUtf8("Array");
    if (JSTaggedValue::Equal(thread, kind, JSHandle<JSTaggedValue>(arrayString))) {
        return JSTaggedValue(PropertyDetector::IsArrayIteratorDetectorValid(env));
    }
    JSHandle<EcmaString> typedarrayString = factory->NewFromUtf8("TypedArray");
    if (JSTaggedValue::Equal(thread, kind, JSHandle<JSTaggedValue>(typedarrayString))) {
        return JSTaggedValue(PropertyDetector::IsTypedArrayIteratorDetectorValid(env));
    }
    return JSTaggedValue::Undefined();
}

JSTaggedValue BuiltinsArkTools::TimeInUs([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    ClockScope scope;
    return JSTaggedValue(scope.GetCurTime());
}
// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::PrepareFunctionForOptimization([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter PrepareFunctionForOptimization()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::OptimizeFunctionOnNextCall([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSTaggedValue> thisValue = GetCallArg(info, 0);
    if (!thisValue->IsJSFunction()) {
        return JSTaggedValue::Undefined();
    }
    JSHandle<JSFunction> jsFunction(thisValue);
    Jit::Compile(thread->GetEcmaVM(), jsFunction);

    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::OptimizeMaglevOnNextCall([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter OptimizeMaglevOnNextCall()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::DeoptimizeFunction([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter DeoptimizeFunction()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::OptimizeOsr([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter OptimizeOsr()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::NeverOptimizeFunction([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter NeverOptimizeFunction()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::HeapObjectVerify([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter HeapObjectVerify()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::DisableOptimizationFinalization([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter DisableOptimizationFinalization()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::DeoptimizeNow([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter DeoptimizeNow()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::WaitForBackgroundOptimization([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter WaitForBackgroundOptimization()";
    return JSTaggedValue::Undefined();
}

// empty function for regress-xxx test cases
JSTaggedValue BuiltinsArkTools::Gc([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter Gc()";
    return JSTaggedValue::Undefined();
}

// empty function for pgoAssertType
JSTaggedValue BuiltinsArkTools::PGOAssertType([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter PGOAssertType";
    return JSTaggedValue::Undefined();
}

JSTaggedValue BuiltinsArkTools::ToLength([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter ToLength()";
    ASSERT(info);
    JSThread *thread = info->GetThread();
    JSHandle<JSTaggedValue> key = GetCallArg(info, 0);
    return JSTaggedValue::ToLength(thread, key);
}

JSTaggedValue BuiltinsArkTools::HasHoleyElements([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter HasHoleyElements()";
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> array = GetCallArg(info, 0);
    if (!array->IsJSArray()) {
        return JSTaggedValue::False();
    }
    JSHandle<JSObject> obj(array);
    uint32_t len = JSHandle<JSArray>::Cast(array)->GetArrayLength();
    for (uint32_t i = 0; i < len; i++) {
        if (ElementAccessor::Get(obj, i).IsHole()) {
            return JSTaggedValue::True();
        }
    }
    return JSTaggedValue::False();
}

JSTaggedValue BuiltinsArkTools::HasDictionaryElements([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter HasDictionaryElements()";
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> objValue = GetCallArg(info, 0);
    JSHandle<JSObject> obj = JSHandle<JSObject>::Cast(objValue);
    return JSTaggedValue(obj->GetJSHClass()->IsDictionaryMode());
}

JSTaggedValue BuiltinsArkTools::HasSmiElements([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter HasSmiElements()";
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> array = GetCallArg(info, 0);
    if (!array->IsJSArray()) {
        return JSTaggedValue::False();
    }
    JSHandle<JSObject> obj(array);
    uint32_t len = JSHandle<JSArray>::Cast(array)->GetArrayLength();
    for (uint32_t i = 0; i < len; i++) {
        if (ElementAccessor::Get(obj, i).IsInt()) {
            return JSTaggedValue::True();
        }
    }
    return JSTaggedValue::False();
}

JSTaggedValue BuiltinsArkTools::HasDoubleElements([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter HasDoubleElements()";
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> array = GetCallArg(info, 0);
    if (!array->IsJSArray()) {
        return JSTaggedValue::False();
    }
    JSHandle<JSObject> obj(array);
    uint32_t len = JSHandle<JSArray>::Cast(array)->GetArrayLength();
    for (uint32_t i = 0; i < len; i++) {
        if (ElementAccessor::Get(obj, i).IsDouble() && !ElementAccessor::Get(obj, i).IsZero()) {
            return JSTaggedValue::True();
        }
    }
    return JSTaggedValue::False();
}

JSTaggedValue BuiltinsArkTools::HasObjectElements([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter HasObjectElements()";
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> array = GetCallArg(info, 0);
    if (!array->IsJSArray()) {
        return JSTaggedValue::False();
    }
    JSHandle<JSObject> obj(array);
    uint32_t len = JSHandle<JSArray>::Cast(array)->GetArrayLength();
    for (uint32_t i = 0; i < len; i++) {
        if (ElementAccessor::Get(obj, i).IsObject()) {
            return JSTaggedValue::True();
        }
    }
    return JSTaggedValue::False();
}

JSTaggedValue BuiltinsArkTools::ArrayBufferDetach([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter ArrayBufferDetach()";
    ASSERT(info);
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> obj1 = GetCallArg(info, 0);
    JSHandle<JSArrayBuffer> arrBuf = JSHandle<JSArrayBuffer>::Cast(obj1);
    arrBuf->Detach(thread);
    return JSTaggedValue::Undefined();
}

JSTaggedValue BuiltinsArkTools::HaveSameMap([[maybe_unused]] EcmaRuntimeCallInfo *info)
{
    LOG_ECMA(INFO) << "Enter HaveSameMap()";
    JSThread *thread = info->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> obj1 = GetCallArg(info, 0);
    JSHandle<JSTaggedValue> obj2 = GetCallArg(info, 1);
    JSHClass *obj1Hclass = obj1->GetTaggedObject()->GetClass();
    JSHClass *obj2Hclass = obj2->GetTaggedObject()->GetClass();
    bool res = (obj1Hclass == obj2Hclass);
    if (!res) {
        return JSTaggedValue::False();
    }
    JSHandle<JSObject> jsobj1(obj1);
    JSHandle<JSObject> jsobj2(obj2);
    JSHandle<TaggedArray> nameList1 = JSObject::EnumerableOwnNames(thread, jsobj1);
    JSHandle<TaggedArray> nameList2 = JSObject::EnumerableOwnNames(thread, jsobj2);
    RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
    uint32_t len1 = nameList1->GetLength();
    uint32_t len2 = nameList2->GetLength();
    if (len1 != len2) {
        return JSTaggedValue::False();
    }
    for (uint32_t i = 0; i < len1; i++) {
        if (obj1->IsJSArray()) {
            JSTaggedValue objTagged1 = JSObject::GetProperty(thread, obj1, i).GetValue().GetTaggedValue();
            JSTaggedValue objTagged2 = JSObject::GetProperty(thread, obj2, i).GetValue().GetTaggedValue();
            if (FastRuntimeStub::FastTypeOf(thread, objTagged1) !=
                FastRuntimeStub::FastTypeOf(thread, objTagged2)) {
                return JSTaggedValue::False();
            }
        } else if (JSObject::GetProperty(thread, obj1, i).GetValue() !=
            JSObject::GetProperty(thread, obj2, i).GetValue()) {
            return JSTaggedValue::False();
        }
    }
    return JSTaggedValue::True();
}

}  // namespace panda::ecmascript::builtins
