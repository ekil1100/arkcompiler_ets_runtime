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

#include <cstddef>
#include "ecmascript/builtins/builtins.h"
#include "ecmascript/builtins/builtins_function.h"
#include "ecmascript/builtins/builtins_object.h"
#include "ecmascript/compiler/aot_file/an_file_data_manager.h"
#include "ecmascript/compiler/aot_file/aot_file_manager.h"
#include "ecmascript/compiler/circuit_builder_helper.h"
#include "ecmascript/deoptimizer/deoptimizer.h"
#include "ecmascript/ecma_global_storage.h"
#include "ecmascript/ecma_vm.h"
#include "ecmascript/global_env.h"
#include "ecmascript/js_api/js_api_tree_map.h"
#include "ecmascript/js_api/js_api_tree_set.h"
#include "ecmascript/js_api/js_api_vector.h"
#include "ecmascript/js_array.h"
#include "ecmascript/js_bigint.h"
#include "ecmascript/js_date_time_format.h"
#include "ecmascript/js_generator_object.h"
#include "ecmascript/js_map.h"
#include "ecmascript/js_map_iterator.h"
#include "ecmascript/js_primitive_ref.h"
#include "ecmascript/js_regexp.h"
#include "ecmascript/js_runtime_options.h"
#include "ecmascript/js_set.h"
#include "ecmascript/js_set_iterator.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/js_weak_container.h"
#include "ecmascript/linked_hash_table.h"
#include "ecmascript/mem/mem_map_allocator.h"
#include "ecmascript/module/js_module_manager.h"
#include "ecmascript/module/js_module_source_text.h"
#include "ecmascript/napi/include/jsnapi.h"
#include "ecmascript/napi/include/jsnapi_internals.h"
#include "ecmascript/napi/jsnapi_helper.h"
#include "ecmascript/object_factory.h"
#include "ecmascript/pgo_profiler/pgo_profiler.h"
#include "ecmascript/pgo_profiler/pgo_profiler_decoder.h"
#include "ecmascript/pgo_profiler/pgo_profiler_encoder.h"
#include "ecmascript/pgo_profiler/pgo_profiler_manager.h"
#include "ecmascript/tagged_array.h"
#include "ecmascript/tests/test_helper.h"
#include "ecmascript/tagged_tree.h"
#include "ecmascript/weak_vector.h"
#include "gtest/gtest.h"

using namespace panda;
using namespace panda::ecmascript;
using namespace panda::ecmascript::kungfu;

namespace panda::test {
using BuiltinsFunction = ecmascript::builtins::BuiltinsFunction;
using PGOProfilerManager = panda::ecmascript::pgo::PGOProfilerManager;
using FunctionForRef = Local<JSValueRef> (*)(JsiRuntimeCallInfo *);
class JSNApiTests : public testing::Test {
public:
    static void SetUpTestCase()
    {
        GTEST_LOG_(INFO) << "SetUpTestCase";
    }

    static void TearDownTestCase()
    {
        GTEST_LOG_(INFO) << "TearDownCase";
    }

    void SetUp() override
    {
        RuntimeOption option;
        option.SetLogLevel(RuntimeOption::LOG_LEVEL::ERROR);
        vm_ = JSNApi::CreateJSVM(option);
        ASSERT_TRUE(vm_ != nullptr) << "Cannot create Runtime";
        thread_ = vm_->GetJSThread();
        vm_->SetEnableForceGC(true);
    }

    void TearDown() override
    {
        vm_->SetEnableForceGC(false);
        JSNApi::DestroyJSVM(vm_);
    }

    template <typename T> void TestNumberRef(T val, TaggedType expected)
    {
        LocalScope scope(vm_);
        Local<NumberRef> obj = NumberRef::New(vm_, val);
        ASSERT_TRUE(obj->IsNumber());
        JSTaggedType res = JSNApiHelper::ToJSTaggedValue(*obj).GetRawData();
        ASSERT_EQ(res, expected);
        if constexpr (std::is_floating_point_v<T>) {
            if (std::isnan(val)) {
                ASSERT_TRUE(std::isnan(obj->Value()));
            } else {
                ASSERT_EQ(obj->Value(), val);
            }
        } else if constexpr (sizeof(T) >= sizeof(int32_t)) {
            ASSERT_EQ(obj->IntegerValue(vm_), val);
        } else if constexpr (std::is_signed_v<T>) {
            ASSERT_EQ(obj->Int32Value(vm_), val);
        } else {
            ASSERT_EQ(obj->Uint32Value(vm_), val);
        }
    }

    TaggedType ConvertDouble(double val)
    {
        return base::bit_cast<JSTaggedType>(val) + JSTaggedValue::DOUBLE_ENCODE_OFFSET;
    }

protected:
    JSThread *thread_ = nullptr;
    EcmaVM *vm_ = nullptr;
};

Local<JSValueRef> FunctionCallback(JsiRuntimeCallInfo *info)
{
    EscapeLocalScope scope(info->GetVM());
    return scope.Escape(ArrayRef::New(info->GetVM(), info->GetArgsNumber()));
}

void WeakRefCallback(EcmaVM *vm)
{
    LocalScope scope(vm);
    Local<ObjectRef> object = ObjectRef::New(vm);
    Global<ObjectRef> globalObject(vm, object);
    globalObject.SetWeak();
    Local<ObjectRef> object1 = ObjectRef::New(vm);
    Global<ObjectRef> globalObject1(vm, object1);
    globalObject1.SetWeak();
    vm->CollectGarbage(TriggerGCType::YOUNG_GC);
    vm->CollectGarbage(TriggerGCType::OLD_GC);
    globalObject.FreeGlobalHandleAddr();
}

void ThreadCheck(const EcmaVM *vm)
{
    EXPECT_TRUE(vm->GetJSThread()->GetThreadId() != JSThread::GetCurrentThreadId());
}

void CheckReject(JsiRuntimeCallInfo *info)
{
    ASSERT_EQ(info->GetArgsNumber(), 1U);
    Local<JSValueRef> reason = info->GetCallArgRef(0);
    ASSERT_TRUE(reason->IsString());
    ASSERT_EQ(Local<StringRef>(reason)->ToString(), "Reject");
}

Local<JSValueRef> RejectCallback(JsiRuntimeCallInfo *info)
{
    LocalScope scope(info->GetVM());
    CheckReject(info);
    return JSValueRef::Undefined(info->GetVM());
}

/**
 * @tc.number: ffi_interface_api_105
 * @tc.name: JSValueRef_IsGeneratorObject
 * @tc.desc: Determine if it is a Generator generator object
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, JSValueRef_IsGeneratorObject)
{
    LocalScope scope(vm_);
    ObjectFactory *factory = vm_->GetFactory();
    auto env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> genFunc = env->GetGeneratorFunctionFunction();
    JSHandle<JSGeneratorObject> genObjHandleVal = factory->NewJSGeneratorObject(genFunc);
    JSHandle<JSHClass> hclass = JSHandle<JSHClass>::Cast(env->GetGeneratorFunctionClass());
    JSHandle<JSFunction> generatorFunc = JSHandle<JSFunction>::Cast(factory->NewJSObject(hclass));
    JSFunction::InitializeJSFunction(thread_, generatorFunc, FunctionKind::GENERATOR_FUNCTION);
    JSHandle<GeneratorContext> generatorContext = factory->NewGeneratorContext();
    generatorContext->SetMethod(thread_, generatorFunc.GetTaggedValue());
    JSHandle<JSTaggedValue> generatorContextVal = JSHandle<JSTaggedValue>::Cast(generatorContext);
    genObjHandleVal->SetGeneratorContext(thread_, generatorContextVal.GetTaggedValue());
    JSHandle<JSTaggedValue> genObjTagHandleVal = JSHandle<JSTaggedValue>::Cast(genObjHandleVal);
    Local<JSValueRef> genObjectRef = JSNApiHelper::ToLocal<GeneratorObjectRef>(genObjTagHandleVal);
    ASSERT_TRUE(genObjectRef->IsGeneratorObject());
}

static JSFunction *JSObjectTestCreate(JSThread *thread)
{
    EcmaVM *ecmaVM = thread->GetEcmaVM();
    JSHandle<GlobalEnv> globalEnv = ecmaVM->GetGlobalEnv();
    return globalEnv->GetObjectFunction().GetObject<JSFunction>();
}

/**
 * @tc.number: ffi_interface_api_106
 * @tc.name: JSValueRef_IsProxy
 * @tc.desc: Determine if it is the type of proxy
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, JSValueRef_IsProxy)
{
    LocalScope scope(vm_);
    JSHandle<JSTaggedValue> hclass(thread_, JSObjectTestCreate(thread_));
    JSHandle<JSTaggedValue> targetHandle(
        thread_->GetEcmaVM()->GetFactory()->NewJSObjectByConstructor(JSHandle<JSFunction>::Cast(hclass), hclass));

    JSHandle<JSTaggedValue> key(thread_->GetEcmaVM()->GetFactory()->NewFromASCII("x"));
    JSHandle<JSTaggedValue> value(thread_, JSTaggedValue(1));
    JSObject::SetProperty(thread_, targetHandle, key, value);
    EXPECT_EQ(JSObject::GetProperty(thread_, targetHandle, key).GetValue()->GetInt(), 1);

    JSHandle<JSTaggedValue> handlerHandle(
        thread_->GetEcmaVM()->GetFactory()->NewJSObjectByConstructor(JSHandle<JSFunction>::Cast(hclass), hclass));
    EXPECT_TRUE(handlerHandle->IsECMAObject());

    JSHandle<JSProxy> proxyHandle = JSProxy::ProxyCreate(thread_, targetHandle, handlerHandle);
    EXPECT_TRUE(*proxyHandle != nullptr);

    EXPECT_EQ(JSProxy::GetProperty(thread_, proxyHandle, key).GetValue()->GetInt(), 1);
    PropertyDescriptor desc(thread_);
    JSProxy::GetOwnProperty(thread_, proxyHandle, key, desc);
    EXPECT_EQ(desc.GetValue()->GetInt(), 1);
    Local<JSValueRef> proxy = JSNApiHelper::ToLocal<JSProxy>(JSHandle<JSTaggedValue>(proxyHandle));
    ASSERT_TRUE(proxy->IsProxy());
}

/**
 * @tc.number: ffi_interface_api_107
 * @tc.name: BufferRef_New_ByteLength
 * @tc.desc:
 * New：Create a buffer and specify the length size
 * ByteLength：Returns the length of the buffer
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, BufferRef_New_ByteLength)
{
    LocalScope scope(vm_);
    int32_t length = 10;
    Local<BufferRef> buffer = BufferRef::New(vm_, length);
    ASSERT_TRUE(buffer->IsBuffer());
    EXPECT_EQ(buffer->ByteLength(vm_), length);
}

/**
 * @tc.number: ffi_interface_api_108
 * @tc.name: BufferRef_New_ByteLength_GetBuffer
 * @tc.desc:
 * New：Create a buffer and specify the length size
 * ByteLength：Returns the length of the buffer
 * GetBuffer：Return buffer pointer
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, BufferRef_New_ByteLength_GetBuffer)
{
    LocalScope scope(vm_);
    int32_t length = 10;
    Local<BufferRef> buffer = BufferRef::New(vm_, length);
    ASSERT_TRUE(buffer->IsBuffer());
    EXPECT_EQ(buffer->ByteLength(vm_), 10U);
    ASSERT_NE(buffer->GetBuffer(), nullptr);
}

/**
 * @tc.number: ffi_interface_api_109
 * @tc.name: BufferRef_New01_ByteLength_GetBuffer_BufferToStringCallback
 * @tc.desc:
 * BufferToStringCallback：Implement callback when calling ToString (vm) mode in buffer
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, BufferRef_New01_ByteLength_GetBuffer_BufferToStringCallback)
{
    LocalScope scope(vm_);
    static bool isFree = false;
    struct Data {
        int32_t length;
    };
    const int32_t length = 15;
    Deleter deleter = [](void *buffer, void *data) -> void {
        delete[] reinterpret_cast<uint8_t *>(buffer);
        Data *currentData = reinterpret_cast<Data *>(data);
        delete currentData;
        isFree = true;
    };
    isFree = false;
    uint8_t *buffer = new uint8_t[length]();
    Data *data = new Data();
    data->length = length;
    Local<BufferRef> bufferRef = BufferRef::New(vm_, buffer, length, deleter, data);
    ASSERT_TRUE(bufferRef->IsBuffer());
    ASSERT_TRUE(bufferRef->IsBuffer());
    EXPECT_EQ(bufferRef->ByteLength(vm_), 15U);
    Local<StringRef> bufferStr = bufferRef->ToString(vm_);
    ASSERT_TRUE(bufferStr->IsString());
}

/**
 * @tc.number: ffi_interface_api_110
 * @tc.name: LocalScope_LocalScope
 * @tc.desc: Build Escape LocalScope sub Vm scope
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, LocalScope_LocalScope)
{
    EscapeLocalScope scope(vm_);
    LocalScope *perScope = nullptr;
    perScope = &scope;
    ASSERT_TRUE(perScope != nullptr);
}

/**
 * @tc.number: ffi_interface_api_111
 * @tc.name: JSNApi_CreateJSContext_SwitchCurrentContext_DestroyJSContext
 * @tc.desc:
 * CreateJSContext：Create Context Object Pointer
 * SwitchCurrentContext：Record Update Context Object
 * DestroyJSContext：Delete Context Object
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, JSNApi_SwitchCurrentContext_DestroyJSContext)
{
    LocalScope scope(vm_);
    EXPECT_EQ(vm_->GetJSThread()->GetEcmaContexts().size(), 1);
    EcmaContext *context = JSNApi::CreateJSContext(vm_);
    GTEST_LOG_(WARNING) << "context test =" << context;
    EXPECT_EQ(vm_->GetJSThread()->GetEcmaContexts().size(), 2);
    EcmaContext *context1 = JSNApi::CreateJSContext(vm_);
    EXPECT_EQ(vm_->GetJSThread()->GetEcmaContexts().size(), 3);
    JSNApi::SwitchCurrentContext(vm_, context1);
    EXPECT_EQ(vm_->GetJSThread()->GetEcmaContexts().size(), 3);
    JSNApi::DestroyJSContext(vm_, context1);
    EXPECT_EQ(vm_->GetJSThread()->GetEcmaContexts().size(), 2);
}

/**
 * @tc.number: ffi_interface_api_112
 * @tc.name: JSNApi_CreateJSVM_DestroyJSVM
 * @tc.desc: Create/delete JSVM
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, JSNApi_CreateJSVM_DestroyJSVM)
{
    LocalScope scope(vm_);
    EcmaVM *vm1_ = nullptr;
    RuntimeOption option;
    option.SetLogLevel(RuntimeOption::LOG_LEVEL::ERROR);
    vm1_ = JSNApi::CreateJSVM(option);
    ASSERT_TRUE(vm1_ != nullptr) << "Cannot create Runtime";
    vm1_->SetEnableForceGC(true);
    JSNApi::DestroyJSVM(vm1_);
    vm1_->SetEnableForceGC(false);
}

/**
 * @tc.number: ffi_interface_api_114
 * @tc.name: JSNApi_GetUncaughtException
 * @tc.desc: Get uncaught exceptions
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, JSNApi_GetUncaughtException)
{
    LocalScope scope(vm_);
    Local<ObjectRef> excep = JSNApi::GetUncaughtException(vm_);
    ASSERT_TRUE(excep.IsNull()) << "CreateInstance occur Exception";
}

Local<JSValueRef> FuncRefNewCallbackForTest(JsiRuntimeCallInfo *info)
{
    GTEST_LOG_(INFO) << "runing FuncRefNewCallbackForTest";
    EscapeLocalScope scope(info->GetVM());
    return scope.Escape(ArrayRef::New(info->GetVM(), info->GetArgsNumber()));
}

/**
 * @tc.number: ffi_interface_api_115
 * @tc.name: ObjectRef_SetAccessorProperty
 * @tc.desc:
 * SetAccessorProperty：Set AccessorPro Properties
 * GetVM：Obtain environment information for runtime calls
 * LocalScope：Build Vm Scope
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, ObjectRef_SetAccessorProperty_JsiRuntimeCallInfo_GetVM)
{
    LocalScope scope(vm_);
    Local<JSValueRef> key = StringRef::NewFromUtf8(vm_, "TestKey");
    FunctionForRef nativeFunc = FuncRefNewCallbackForTest;
    Local<FunctionRef> getter = FunctionRef::New(vm_, nativeFunc);
    Local<FunctionRef> setter = FunctionRef::New(vm_, nativeFunc);
    Local<ObjectRef> object = ObjectRef::New(vm_);
    ASSERT_TRUE(object->SetAccessorProperty(vm_, key, getter, setter));
}

/*
 * @tc.number: ffi_interface_api_116
 * @tc.name: JSNApi_IsBoolean
 * @tc.desc: Judge  whether obj is a boolean type
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, IsBoolean)
{
    LocalScope scope(vm_);
    char16_t utf16[] = u"This is a char16 array";
    int size = sizeof(utf16);
    Local<StringRef> obj = StringRef::NewFromUtf16(vm_, utf16, size);
    ASSERT_FALSE(obj->IsBoolean());
}

/*
 * @tc.number: ffi_interface_api_117
 * @tc.name: JSNApi_IsNULL
 * @tc.desc: Judge  whether obj is a null type
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, IsNULL)
{
    LocalScope scope(vm_);
    char16_t utf16[] = u"This is a char16 array";
    int size = sizeof(utf16);
    Local<StringRef> obj = StringRef::NewFromUtf16(vm_, utf16, size);
    ASSERT_FALSE(obj->IsNull());
}

/*
 * @tc.number: ffi_interface_api_118
 * @tc.name: JSNApi_GetTime
 * @tc.desc: This function is to catch time
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, GetTime)
{
    LocalScope scope(vm_);
    const double time = 14.29;
    Local<DateRef> date = DateRef::New(vm_, time);
    ASSERT_EQ(date->GetTime(), time);
}

/*
 * @tc.number: ffi_interface_api_119
 * @tc.name: JSNApi_IsDetach
 * @tc.desc: Judge whether arraybuffer is detached
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, IsDetach)
{
    LocalScope scope(vm_);
    const int32_t length = 33;
    Local<ArrayBufferRef> arraybuffer = ArrayBufferRef::New(vm_, length);
    ASSERT_FALSE(arraybuffer->IsDetach());
}

/*
 * @tc.number: ffi_interface_api_120
 * @tc.name: JSNApi_Detach
 * @tc.desc: This function is to detach arraybuffer
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, Detach)
{
    LocalScope scope(vm_);
    const int32_t length = 33;
    Local<ArrayBufferRef> arraybuffer = ArrayBufferRef::New(vm_, length);
    arraybuffer->Detach(vm_);
}

/*
 * @tc.number: ffi_interface_api_121
 * @tc.name: JSNApi_New1
 * @tc.desc:  create a obj that is a arraybuffer type
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, New1)
{
    LocalScope scope(vm_);
    const int32_t length = 33;
    Local<ArrayBufferRef> arrayBuffer = ArrayBufferRef::New(vm_, length);
    ASSERT_TRUE(arrayBuffer->IsArrayBuffer());
    ASSERT_EQ(arrayBuffer->ByteLength(vm_), length);
    ASSERT_NE(arrayBuffer->GetBuffer(), nullptr);
}

/*
 * @tc.number: ffi_interface_api_122
 * @tc.name: JSNApi_New1
 * @tc.desc:  create a obj that is a arraybuffer type
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, New2)
{
    static bool isFree = false;
    struct Data {
        int32_t length;
    };
    const int32_t length = 15;
    Data *data = new Data();
    data->length = length;
    Deleter deleter = [](void *buffer, void *data) -> void {
        delete[] reinterpret_cast<uint8_t *>(buffer);
        Data *currentData = reinterpret_cast<Data *>(data);
        ASSERT_EQ(currentData->length, 15); // 5 : size of arguments
        delete currentData;
        isFree = true;
    };
    {
        LocalScope scope(vm_);
        uint8_t *buffer = new uint8_t[length]();
        Local<ArrayBufferRef> arrayBuffer = ArrayBufferRef::New(vm_, buffer, length, deleter, data);
        ASSERT_TRUE(arrayBuffer->IsArrayBuffer());
        ASSERT_EQ(arrayBuffer->ByteLength(vm_), length);
        ASSERT_EQ(arrayBuffer->GetBuffer(), buffer);
    }
}

/*
 * @tc.number: ffi_interface_api_123
 * @tc.name: JSNApi_Bytelength
 * @tc.desc:   capture bytelength of arraybuffer
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, Bytelength)
{
    LocalScope scope(vm_);
    const int32_t length = 33;
    Local<ArrayBufferRef> arrayBuffer = ArrayBufferRef::New(vm_, length);
    ASSERT_EQ(arrayBuffer->ByteLength(vm_), length);
}

/*
 * @tc.number: ffi_interface_api_124
 * @tc.name: JSNApi_GetBuffer
 * @tc.desc:  capture buffer of arraybuffer
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, GetBuffer)
{
    LocalScope scope(vm_);
    const int32_t length = 33;
    Local<ArrayBufferRef> arraybuffer = ArrayBufferRef::New(vm_, length);
    ASSERT_NE(arraybuffer->GetBuffer(), nullptr);
}

/*
 * @tc.number: ffi_interface_api_125
 * @tc.name: JSNApi_Is32Arraytest
 * @tc.desc:  judge  whether obj is a int32array type,
 * judge  whether obj is a uint32array type,
 * judge  whether obj is a float32array type,
 * judge  whether obj is a float64array type,
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, Is32Arraytest)
{
    LocalScope scope(vm_);
    char16_t utf16[] = u"This is a char16 array";
    int size = sizeof(utf16);
    Local<StringRef> obj = StringRef::NewFromUtf16(vm_, utf16, size);
    ASSERT_FALSE(obj->IsInt32Array());
    ASSERT_FALSE(obj->IsUint32Array());
    ASSERT_FALSE(obj->IsFloat32Array());
    ASSERT_FALSE(obj->IsFloat64Array());
}

/*
 * @tc.number: ffi_interface_api_126
 * @tc.name: JSNApi_SynchronizVMInfo
 * @tc.desc:  capture  synchronous info of vm
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, SynchronizVMInfo)
{
    LocalScope scope(vm_);
    JSRuntimeOptions option;
    EcmaVM *hostVM = JSNApi::CreateEcmaVM(option);
    LocalScope scope2(hostVM);
    JSNApi::SynchronizVMInfo(vm_, hostVM);
}

/*
 * @tc.number: ffi_interface_api_127
 * @tc.name: JSNApi_IsProfiling
 * @tc.desc:  judge whether vm is profiling
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, IsProfiling)
{
    LocalScope scope(vm_);
    ASSERT_FALSE(JSNApi::IsProfiling(vm_));
}

/*
 * @tc.number: ffi_interface_api_128
 * @tc.name: JSNApi_SetProfilerState
 * @tc.desc:  This function is to set state of profiler
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, SetProfilerState)
{
    bool value = true;
    bool value2 = false;
    LocalScope scope(vm_);
    JSNApi::SetProfilerState(vm_, value);
    JSNApi::SetProfilerState(vm_, value2);
}

/*
 * @tc.number: ffi_interface_api_129
 * @tc.name: JSNApi_SetLoop
 * @tc.desc:  This function is to set loop
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, SetLoop)
{
    LocalScope scope(vm_);
    void *data = reinterpret_cast<void *>(BuiltinsFunction::FunctionPrototypeInvokeSelf);
    JSNApi::SetLoop(vm_, data);
}

/*
 * @tc.number: ffi_interface_api_130
 * @tc.name: JSNApi_SetHostPromiseRejectionTracker
 * @tc.desc:  This function is to set host promise rejection about tracker
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, SetHostPromiseRejectionTracker)
{
    LocalScope scope(vm_);
    void *data = reinterpret_cast<void *>(BuiltinsFunction::FunctionPrototypeInvokeSelf);
    void *cb = reinterpret_cast<void *>(BuiltinsFunction::FunctionPrototypeInvokeSelf);
    JSNApi::SetHostPromiseRejectionTracker(vm_, cb, data);
}

/*
 * @tc.number: ffi_interface_api_131
 * @tc.name: JSNApi_SetHostResolveBufferTracker
 * @tc.desc:   This function is to set host resolve buffer about tracker
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, SetHostResolveBufferTracker)
{
    LocalScope scope(vm_);
    JSNApi::SetHostResolveBufferTracker(vm_, [&](std::string, uint8_t **, size_t *) -> bool { return true; });
}

/*
 * @tc.number: ffi_interface_api_132
 * @tc.name: JSNApi_SetUnloadNativeModuleCallback
 * @tc.desc:   This function is to set unload native module  about callback
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, SetUnloadNativeModuleCallback)
{
    LocalScope scope(vm_);
    JSNApi::SetUnloadNativeModuleCallback(vm_, [&](const std::string &) -> bool { return true; });
}

/*
 * @tc.number: ffi_interface_api_133
 * @tc.name: JSNApi_SetNativePtrGetter
 * @tc.desc:   This function is to set a native pointer about getter
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, SetNativePtrGetter)
{
    LocalScope scope(vm_);
    void *cb = reinterpret_cast<void *>(BuiltinsFunction::FunctionPrototypeInvokeSelf);
    JSNApi::SetNativePtrGetter(vm_, cb);
}

/*
 * @tc.number: ffi_interface_api_134
 * @tc.name: JSNApi_PreFork
 * @tc.desc:  This function is to set prefork
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, PreFork)
{
    LocalScope scope(vm_);
    JSNApi::PreFork(vm_);
}

/*
 * @tc.number: ffi_interface_api_135
 * @tc.name: JSNApi_PostFork
 * @tc.desc:  This function is to set postfork
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, PostFork)
{
    RuntimeOption option;
    LocalScope scope(vm_);
    JSNApi::PostFork(vm_, option);
}


/*
 * @tc.number: ffi_interface_api_136
 * @tc.name: JSNApi_NewFromUtf8
 * @tc.desc:  create a newfromutf8 object
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, NewFromUtf8)
{
    LocalScope scope(vm_);
    char utf8[] = "hello world!";
    int length = strlen(utf8);
    Local<StringRef> result = StringRef::NewFromUtf8(vm_, utf8, length);
    ASSERT_EQ(result->Utf8Length(vm_), length + 1);
}

/*
 * @tc.number: ffi_interface_api_137
 * @tc.name: JSNApi_NewFromUtf16
 * @tc.desc:  create a newfromutf16 object
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, NewFromUtf16)
{
    LocalScope scope(vm_);
    char16_t utf16[] = u"This is a char16 array";
    int length = sizeof(utf16);
    Local<StringRef> result = StringRef::NewFromUtf16(vm_, utf16, length);
    ASSERT_EQ(result->Length(), length);
}

/*
 * @tc.number: ffi_interface_api_138
 * @tc.name: JSNApi_GetNapiWrapperString
 * @tc.desc:  This function is to get a napiwrapper string
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, GetNapiWrapperString)
{
    LocalScope scope(vm_);
    Local<StringRef> result = StringRef::GetNapiWrapperString(vm_);
    ASSERT_TRUE(result->IsString());
}

/*
 * @tc.number: ffi_interface_api_139
 * @tc.name: JSNApi_JSExecutionScope
 * @tc.desc:  This function is to construct a object of jsexecutionscope
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, JSExecutionScope)
{
    LocalScope scope(vm_);
    JSExecutionScope jsexecutionScope(vm_);
}

/*
 * @tc.number: ffi_interface_api_140
 * @tc.name: WeakMapRef_GetSize_GetTotalElements_GetKey_GetValue
 * @tc.desc:  This function is to set a weakmap and capture it's size ,
 * key, value and total elements
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, WeakMapRef_GetSize_GetTotalElements_GetKey_GetValue)
{
    LocalScope scope(vm_);
    JSThread *thread = vm_->GetJSThread();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    JSHandle<JSTaggedValue> constructor = env->GetBuiltinsWeakMapFunction();
    JSHandle<JSWeakMap> weakMap =
        JSHandle<JSWeakMap>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(constructor), constructor));
    JSHandle<LinkedHashMap> hashMap = LinkedHashMap::Create(thread);
    weakMap->SetLinkedMap(thread, hashMap);
    JSHandle<JSTaggedValue> weakMapTag = JSHandle<JSTaggedValue>::Cast(weakMap);

    Local<WeakMapRef> map = JSNApiHelper::ToLocal<WeakMapRef>(weakMapTag);
    EXPECT_TRUE(map->IsWeakMap());
    JSHandle<JSTaggedValue> value(factory->NewFromASCII("value"));
    JSHandle<JSTaggedValue> key(factory->NewFromASCII("key"));
    JSWeakMap::Set(thread, weakMap, key, value);
    int32_t num = map->GetSize();
    int32_t num1 = map->GetTotalElements();
    ASSERT_EQ(num, 1);
    ASSERT_EQ(num1, 1);
    Local<JSValueRef> res1 = map->GetKey(vm_, 0);
    ASSERT_EQ(res1->ToString(vm_)->ToString(), "key");
    Local<JSValueRef> res2 = map->GetValue(vm_, 0);
    ASSERT_EQ(res2->ToString(vm_)->ToString(), "value");
}

/*
 * @tc.number: ffi_interface_api_141
 * @tc.name: JSNApi_ IsAGJA
 * @tc.desc:  This function is to judge whether object is a argumentsobject or
 * is a generatorfunction or is a asyncfunction
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, IsAGJA)
{
    LocalScope scope(vm_);
    char16_t utf16[] = u"This is a char16 array";
    int size = sizeof(utf16);
    Local<StringRef> obj = StringRef::NewFromUtf16(vm_, utf16, size);
    ASSERT_FALSE(obj->IsArgumentsObject());
    ASSERT_FALSE(obj->IsGeneratorFunction());
    ASSERT_FALSE(obj->IsAsyncFunction());
}

/**
 * @tc.number: ffi_interface_api_143
 * @tc.name: Int32Array
 * @tc.desc: Catch exceptions correctly
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, HasCaught)
{
    LocalScope scope(vm_);
    Local<StringRef> message = StringRef::NewFromUtf8(vm_, "ErrorTest");
    Local<JSValueRef> error = Exception::Error(vm_, message);
    ASSERT_TRUE(error->IsError());
    JSNApi::ThrowException(vm_, error);
    TryCatch tryCatch(vm_);
    ASSERT_TRUE(tryCatch.HasCaught());
    vm_->GetJSThread()->ClearException();
    ASSERT_FALSE(tryCatch.HasCaught());
}

/**
 * @tc.number: ffi_interface_api_144
 * @tc.name: Int32Array
 * @tc.desc: Rethrow the exception
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, Rethrow)
{
    LocalScope scope(vm_);
    TryCatch tryCatch(vm_);
    tryCatch.Rethrow();
    ASSERT_TRUE(tryCatch.getrethrow_());
}

/**
 * @tc.number: ffi_interface_api_145
 * @tc.name: Int32Array
 * @tc.desc: Clear caught exceptions
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, GetAndClearException)
{
    LocalScope scope(vm_);
    Local<StringRef> message = StringRef::NewFromUtf8(vm_, "ErrorTest");
    Local<JSValueRef> error = Exception::Error(vm_, message);
    ASSERT_TRUE(error->IsError());
    JSNApi::ThrowException(vm_, error);
    ASSERT_TRUE(vm_->GetJSThread()->HasPendingException());
    TryCatch tryCatch(vm_);
    tryCatch.GetAndClearException();
    EXPECT_FALSE(vm_->GetJSThread()->HasPendingException());
}
/**
 * @tc.number: ffi_interface_api_146
 * @tc.name: Int32Array
 * @tc.desc: trycatch class construction
 * @tc.type: FUNC
 * @tc.require:  parameter
 */
HWTEST_F_L0(JSNApiTests, TryCatch)
{
    LocalScope scope(vm_);
    TryCatch tryCatch(vm_);
    EXPECT_EQ(tryCatch.getrethrow_(), false);
    EXPECT_FALSE(tryCatch.HasCaught());
    tryCatch.Rethrow();
    EXPECT_EQ(tryCatch.getrethrow_(), true);
}

HWTEST_F_L0(JSNApiTests, NewObjectWithProperties)
{
    LocalScope scope(vm_);
    Local<JSValueRef> keys[1100];
    Local<JSValueRef> values[1100];
    PropertyAttribute attributes[1100];
    for (int i = 0; i < 1100; i += (i < 80 ? (i < 10 ? 1 : 80) : 1000)) {
        for (int j = 0; j <= i; ++j) {
            std::string strKey("TestKey" + std::to_string(i) + "_" + std::to_string(j));
            std::string strVal("TestValue" + std::to_string(i) + "_" + std::to_string(j));
            keys[j] = StringRef::NewFromUtf8(vm_, strKey.c_str());
            values[j] = StringRef::NewFromUtf8(vm_, strVal.c_str());
            attributes[j] = PropertyAttribute(values[j], true, true, true);
        }
        Local<ObjectRef> object = ObjectRef::NewWithProperties(vm_, i + 1, keys, attributes);
        for (int j = 0; j <= i; ++j) {
            Local<JSValueRef> value1 = object->Get(vm_, keys[j]);
            EXPECT_TRUE(values[j]->IsStrictEquals(vm_, value1));
        }
        JSHandle<JSObject> obj(JSNApiHelper::ToJSHandle(object));
        uint32_t propCount = obj->GetJSHClass()->NumberOfProps();
        if (i + 1 > PropertyAttributes::MAX_FAST_PROPS_CAPACITY) {
            EXPECT_TRUE(propCount == 0);
            EXPECT_TRUE(obj->GetJSHClass()->IsDictionaryMode());
            JSHandle<NameDictionary> dict(thread_, obj->GetProperties());
            EXPECT_TRUE(dict->EntriesCount() == i + 1);
        } else {
            EXPECT_TRUE(propCount == i + 1);
            int32_t in_idx = obj->GetJSHClass()->GetNextInlinedPropsIndex();
            int32_t nonin_idx = obj->GetJSHClass()->GetNextNonInlinedPropsIndex();
            if (i + 1 < JSHClass::DEFAULT_CAPACITY_OF_IN_OBJECTS) {
                EXPECT_TRUE(in_idx == i + 1);
                EXPECT_TRUE(nonin_idx == -1);
            } else {
                EXPECT_TRUE(in_idx == -1);
                EXPECT_TRUE(nonin_idx == 0);
            }
        }
    }
}

HWTEST_F_L0(JSNApiTests, NewObjectWithPropertieNonPureStringKey)
{
    LocalScope scope(vm_);
    Local<JSValueRef> keys[] = {
        StringRef::NewFromUtf8(vm_, "1"),
    };
    Local<JSValueRef> values[] = {
        StringRef::NewFromUtf8(vm_, "value1"),
    };
    PropertyAttribute attributes[] = {
        PropertyAttribute(values[0], true, true, true),
    };
    Local<ObjectRef> object = ObjectRef::NewWithProperties(vm_, 1, keys, attributes);
    JSHandle<JSTaggedValue> obj = JSNApiHelper::ToJSHandle(object);
    EXPECT_TRUE(obj.GetTaggedValue() == JSTaggedValue::Undefined());
    thread_->ClearException();
}

HWTEST_F_L0(JSNApiTests, NewObjectWithPropertiesDuplicate)
{
    LocalScope scope(vm_);
    Local<JSValueRef> keys[] = {
        StringRef::NewFromUtf8(vm_, "duplicateKey"),
        StringRef::NewFromUtf8(vm_, "simpleKey"),
        StringRef::NewFromUtf8(vm_, "duplicateKey"),
    };
    Local<JSValueRef> values[] = {
        StringRef::NewFromUtf8(vm_, "value1"),
        StringRef::NewFromUtf8(vm_, "value2"),
        StringRef::NewFromUtf8(vm_, "value3"),
    };
    PropertyAttribute attributes[] = {
        PropertyAttribute(values[0], true, true, true),
        PropertyAttribute(values[1], true, true, true),
        PropertyAttribute(values[2], true, true, true),
    };
    Local<ObjectRef> object = ObjectRef::NewWithProperties(vm_, 3, keys, attributes);
    JSHandle<JSTaggedValue> obj = JSNApiHelper::ToJSHandle(object);
    EXPECT_TRUE(obj.GetTaggedValue() == JSTaggedValue::Undefined());
    thread_->ClearException();
}

HWTEST_F_L0(JSNApiTests, NewObjectWithNamedProperties)
{
    LocalScope scope(vm_);
    const char *keys[1100];
    std::string strKeys[1100];
    Local<JSValueRef> values[1100];
    for (int i = 0; i < 1100; i += (i < 80 ? (i < 10 ? 1 : 80) : 1000)) {
        for (int j = 0; j <= i; ++j) {
            strKeys[j] = "TestKey" + std::to_string(i) + "_" + std::to_string(j);
            std::string strVal("TestValue" + std::to_string(i) + "_" + std::to_string(j));
            keys[j] = const_cast<char *>(strKeys[j].c_str());
            values[j] = StringRef::NewFromUtf8(vm_, strVal.c_str());
        }
        Local<ObjectRef> object = ObjectRef::NewWithNamedProperties(vm_, i + 1, keys, values);
        for (int j = 0; j <= i; ++j) {
            Local<JSValueRef> value1 = object->Get(vm_, StringRef::NewFromUtf8(vm_, keys[j]));
            EXPECT_TRUE(values[j]->IsStrictEquals(vm_, value1));
        }
        JSHandle<JSObject> obj(JSNApiHelper::ToJSHandle(object));
        uint32_t propCount = obj->GetJSHClass()->NumberOfProps();
        if (i + 1 > PropertyAttributes::MAX_FAST_PROPS_CAPACITY) {
            EXPECT_TRUE(propCount == 0);
            EXPECT_TRUE(obj->GetJSHClass()->IsDictionaryMode());
            JSHandle<NameDictionary> dict(thread_, obj->GetProperties());
            EXPECT_TRUE(dict->EntriesCount() == i + 1);
        } else {
            EXPECT_TRUE(propCount == i + 1);
            int32_t in_idx = obj->GetJSHClass()->GetNextInlinedPropsIndex();
            int32_t nonin_idx = obj->GetJSHClass()->GetNextNonInlinedPropsIndex();
            if (i + 1 < JSHClass::DEFAULT_CAPACITY_OF_IN_OBJECTS) {
                EXPECT_TRUE(in_idx == i + 1);
                EXPECT_TRUE(nonin_idx == -1);
            } else {
                EXPECT_TRUE(in_idx == -1);
                EXPECT_TRUE(nonin_idx == 0);
            }
        }
    }
}

HWTEST_F_L0(JSNApiTests, NewObjectWithNamedPropertieNonPureStringKey)
{
    LocalScope scope(vm_);
    const char *keys[] = {
        "1",
    };
    Local<JSValueRef> values[] = {
        StringRef::NewFromUtf8(vm_, "value1"),
    };
    Local<ObjectRef> object = ObjectRef::NewWithNamedProperties(vm_, 2, keys, values);
    JSHandle<JSTaggedValue> obj = JSNApiHelper::ToJSHandle(object);
    EXPECT_TRUE(obj.GetTaggedValue() == JSTaggedValue::Undefined());
    thread_->ClearException();
}

HWTEST_F_L0(JSNApiTests, NewObjectWithNamedPropertiesDuplicate)
{
    LocalScope scope(vm_);
    const char *keys[] = {
        "duplicateKey",
        "simpleKey",
        "duplicateKey",
    };
    Local<JSValueRef> values[] = {
        StringRef::NewFromUtf8(vm_, "value1"),
        StringRef::NewFromUtf8(vm_, "value2"),
        StringRef::NewFromUtf8(vm_, "value3"),
    };
    Local<ObjectRef> object = ObjectRef::NewWithNamedProperties(vm_, 3, keys, values);
    JSHandle<JSTaggedValue> obj = JSNApiHelper::ToJSHandle(object);
    EXPECT_TRUE(obj.GetTaggedValue() == JSTaggedValue::Undefined());
    thread_->ClearException();
}
} // namespace panda::test