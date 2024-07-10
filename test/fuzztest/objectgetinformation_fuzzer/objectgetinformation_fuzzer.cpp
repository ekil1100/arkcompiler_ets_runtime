/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "objectgetinformation_fuzzer.h"
#include "ecmascript/base/string_helper.h"
#include "ecmascript/napi/include/jsnapi.h"

using namespace panda;
using namespace panda::ecmascript;

namespace OHOS {
void ObjectGetAllPropertyNamesFuzzTest(const uint8_t *data, size_t size)
{
    RuntimeOption option;
    option.SetLogLevel(RuntimeOption::LOG_LEVEL::ERROR);
    EcmaVM *vm = JSNApi::CreateJSVM(option);
    if (data == nullptr || size <= 0) {
        LOG_ECMA(ERROR) << "illegal input!";
        return;
    }
    int32_t index = 0;
    size_t maxByteLen1 = 4;
    if (size > maxByteLen1) {
        size = maxByteLen1;
    }
    if (memcpy_s(&index, maxByteLen1, data, size) != EOK) {
        LOG_ECMA(ERROR) << "memcpy_s failed !";
        UNREACHABLE();
    }
    uint32_t filter = 0;
    size_t maxByteLen2 = 4;
    if (size > maxByteLen2) {
        size = maxByteLen2;
    }
    if (memcpy_s(&filter, maxByteLen2, data, size) != EOK) {
        LOG_ECMA(ERROR) << "memcpy_s failed !";
        UNREACHABLE();
    }
    Local<ObjectRef> object = ObjectRef::New(vm);
    NativePointerCallback callBack = nullptr;
    object->SetNativePointerField(vm, index, (void *)data, callBack, (void *)data);
    object->GetAllPropertyNames(vm, filter);
    JSNApi::DestroyJSVM(vm);
}

void ObjectGetNativePointerFieldCountFuzzTest(const uint8_t *data, size_t size)
{
    RuntimeOption option;
    option.SetLogLevel(RuntimeOption::LOG_LEVEL::ERROR);
    EcmaVM *vm = JSNApi::CreateJSVM(option);
    if (data == nullptr || size <= 0) {
        LOG_ECMA(ERROR) << "illegal input!";
        return;
    }
    int32_t key = 0;
    size_t maxByteLen = 4;
    if (size > maxByteLen) {
        size = maxByteLen;
    }
    if (memcpy_s(&key, maxByteLen, data, size) != EOK) {
        LOG_ECMA(ERROR) << "memcpy_s failed !";
        UNREACHABLE();
    }
    if (key <= 0 || key > 1024) { // 1024 : 1M in size
        key = 1024;               // 1024 : 1M in size
    }
    Local<ObjectRef> object = ObjectRef::New(vm);
    object->SetNativePointerFieldCount(vm, key);
    object->GetNativePointerFieldCount(vm);
    JSNApi::DestroyJSVM(vm);
}

void ObjectGetOwnEnumerablePropertyNamesFuzzTest(const uint8_t *data, size_t size)
{
    RuntimeOption option;
    option.SetLogLevel(RuntimeOption::LOG_LEVEL::ERROR);
    EcmaVM *vm = JSNApi::CreateJSVM(option);
    if (data == nullptr || size <= 0) {
        LOG_ECMA(ERROR) << "illegal input!";
        return;
    }
    int32_t index = 0;
    size_t maxByteLen = 4;
    if (size > maxByteLen) {
        size = maxByteLen;
    }
    if (memcpy_s(&index, maxByteLen, data, size) != EOK) {
        LOG_ECMA(ERROR) << "memcpy_s failed !";
        UNREACHABLE();
    }
    Local<ObjectRef> object = ObjectRef::New(vm);
    NativePointerCallback callBack = nullptr;
    object->SetNativePointerField(vm, index, (void *)data, callBack, (void *)data);
    object->GetOwnEnumerablePropertyNames(vm);
    JSNApi::DestroyJSVM(vm);
}

void ObjectGetOwnPropertyNamesFuzzTest(const uint8_t *data, size_t size)
{
    RuntimeOption option;
    option.SetLogLevel(RuntimeOption::LOG_LEVEL::ERROR);
    EcmaVM *vm = JSNApi::CreateJSVM(option);
    if (data == nullptr || size <= 0) {
        LOG_ECMA(ERROR) << "illegal input!";
        return;
    }
    int32_t index = 0;
    size_t maxByteLen = 4;
    if (size > maxByteLen) {
        size = maxByteLen;
    }
    if (memcpy_s(&index, maxByteLen, data, size) != EOK) {
        LOG_ECMA(ERROR) << "memcpy_s failed !";
        UNREACHABLE();
    }
    Local<ObjectRef> object = ObjectRef::New(vm);
    NativePointerCallback callBack = nullptr;
    object->SetNativePointerField(vm, index, (void *)data, callBack, (void *)data);
    object->GetOwnPropertyNames(vm);
    JSNApi::DestroyJSVM(vm);
}
}

// Fuzzer entry point.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Run your code on data.
    OHOS::ObjectGetAllPropertyNamesFuzzTest(data, size);
    OHOS::ObjectGetNativePointerFieldCountFuzzTest(data, size);
    OHOS::ObjectGetOwnEnumerablePropertyNamesFuzzTest(data, size);
    OHOS::ObjectGetOwnPropertyNamesFuzzTest(data, size);
    return 0;
}