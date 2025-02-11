/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_NAPI_INCLUDE_JSNAPI_EXPO_H
#define ECMASCRIPT_NAPI_INCLUDE_JSNAPI_EXPO_H

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <map>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecmascript/base/aligned_struct.h"
#include "ecmascript/base/config.h"
#include "ecmascript/mem/mem_common.h"

#ifndef NDEBUG
#include "libpandabase/utils/debug.h"
#endif

#ifdef ERROR
#undef ERROR
#endif

namespace panda {
class JSNApiHelper;
class EscapeLocalScope;
class PromiseRejectInfo;
template<typename T>
class CopyableGlobal;
template<typename T>
class Global;
class JSNApi;
class SymbolRef;
template<typename T>
class Local;
class JSValueRef;
class PrimitiveRef;
class ArrayRef;
class BigIntRef;
class StringRef;
class ObjectRef;
class FunctionRef;
class NumberRef;
class MapIteratorRef;
class BooleanRef;
class NativePointerRef;
class JsiRuntimeCallInfo;
class RuntimeOption;
namespace test {
class JSNApiTests;
}  // namespace test
class BufferRef;
namespace ecmascript {
class EcmaVM;
class JSTaggedValue;
class EcmaContext;
class JSRuntimeOptions;
class JSThread;
struct EcmaRuntimeCallInfo;
namespace base {
template<size_t ElementAlign, typename... Ts>
struct AlignedStruct;
struct AlignedPointer;
}
}  // namespace ecmascript

using Deleter = void (*)(void *nativePointer, void *data);
using WeakRefClearCallBack = void (*)(void *);
using EcmaVM = ecmascript::EcmaVM;
using EcmaContext = ecmascript::EcmaContext;
using JSThread = ecmascript::JSThread;
using JSTaggedType = uint64_t;
using ConcurrentCallback = void (*)(Local<JSValueRef> result, bool success, void *taskInfo, void *data);
using SourceMapTranslateCallback = std::function<bool(std::string& url, int& line, int& column)>;
using DeviceDisconnectCallback = std::function<bool()>;

#define ECMA_DISALLOW_COPY(className)      \
    className(const className &) = delete; \
    className &operator=(const className &) = delete

#define ECMA_DISALLOW_MOVE(className) \
    className(className &&) = delete; \
    className &operator=(className &&) = delete

#ifdef PANDA_TARGET_WINDOWS
#define ECMA_PUBLIC_API __declspec(dllexport)
#else
#define ECMA_PUBLIC_API __attribute__((visibility ("default")))
#endif

#ifndef NDEBUG
#define ECMA_ASSERT(cond) \
    if (!(cond)) { \
        panda::debug::AssertionFail(#cond, __FILE__, __LINE__, __FUNCTION__); \
    }
#else
#define ECMA_ASSERT(cond) static_cast<void>(0)
#endif

template<typename T>
class ECMA_PUBLIC_API Local {  // NOLINT(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
public:
    inline Local() = default;

    template<typename S>
    inline Local(const Local<S> &current) : address_(reinterpret_cast<uintptr_t>(*current))
    {
        // Check
    }

    Local(const EcmaVM *vm, const Global<T> &current);

    Local(const EcmaVM *vm, const CopyableGlobal<T> &current);

    ~Local() = default;

    inline T *operator*() const
    {
        return GetAddress();
    }

    inline T *operator->() const
    {
        return GetAddress();
    }

    inline bool IsEmpty() const
    {
        return GetAddress() == nullptr;
    }

    inline void Empty()
    {
        address_ = 0;
    }

    inline bool IsNull() const
    {
        return IsEmpty() || GetAddress()->IsHole();
    }

    explicit inline Local(uintptr_t addr) : address_(addr) {}

private:
    inline T *GetAddress() const
    {
        return reinterpret_cast<T *>(address_);
    };
    uintptr_t address_ = 0U;
    friend JSNApiHelper;
    friend EscapeLocalScope;
    friend JsiRuntimeCallInfo;
};

/**
 * A Copyable global handle, keeps a separate global handle for each CopyableGlobal.
 *
 * Support Copy Constructor and Assign, Move Constructor And Assign.
 *
 * If destructed, the global handle held will be automatically released.
 *
 * Usage: It Can be used as heap object assign to another variable, a value passing parameter, or
 *        a value passing return value and so on.
 */
template<typename T>
class ECMA_PUBLIC_API CopyableGlobal {
public:
    inline CopyableGlobal() = default;
    ~CopyableGlobal()
    {
        Free();
    }

    inline CopyableGlobal(const CopyableGlobal &that)
    {
        Copy(that);
    }

    inline CopyableGlobal &operator=(const CopyableGlobal &that)
    {
        Copy(that);
        return *this;
    }

    inline CopyableGlobal(CopyableGlobal &&that)
    {
        Move(that);
    }

    inline CopyableGlobal &operator=(CopyableGlobal &&that)
    {
        Move(that);
        return *this;
    }

    template<typename S>
    CopyableGlobal(const EcmaVM *vm, const Local<S> &current);

    CopyableGlobal(const EcmaVM *vm, const Local<T> &current);

    template<typename S>
    CopyableGlobal(const CopyableGlobal<S> &that)
    {
        Copy(that);
    }

    void Reset()
    {
        Free();
    }

    Local<T> ToLocal() const
    {
        if (IsEmpty()) {
            return Local<T>();
        }
        return Local<T>(vm_, *this);
    }

    void Empty()
    {
        address_ = 0;
    }

    inline T *operator*() const
    {
        return GetAddress();
    }

    inline T *operator->() const
    {
        return GetAddress();
    }

    inline bool IsEmpty() const
    {
        return GetAddress() == nullptr;
    }

    void SetWeakCallback(void *ref, WeakRefClearCallBack freeGlobalCallBack,
                         WeakRefClearCallBack nativeFinalizeCallback);
    void SetWeak();

    void ClearWeak();

    bool IsWeak() const;

    const EcmaVM *GetEcmaVM() const
    {
        return vm_;
    }

private:
    inline T *GetAddress() const
    {
        return reinterpret_cast<T *>(address_);
    };
    inline void Copy(const CopyableGlobal &that);
    template<typename S>
    inline void Copy(const CopyableGlobal<S> &that);
    inline void Move(CopyableGlobal &that);
    inline void Free();
    uintptr_t address_ = 0U;
    const EcmaVM *vm_ {nullptr};
};

template<typename T>
class ECMA_PUBLIC_API Global {  // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions
public:
    inline Global() = default;

    inline Global(const Global &that)
    {
        Update(that);
    }

    inline Global &operator=(const Global &that)
    {
        Update(that);
        return *this;
    }

    inline Global(Global &&that)
    {
        Update(that);
    }

    inline Global &operator=(Global &&that)
    {
        Update(that);
        return *this;
    }

    template<typename S>
    Global(const EcmaVM *vm, const Local<S> &current);
    template<typename S>
    Global(const EcmaVM *vm, const Global<S> &current);

    ~Global() = default;

    Local<T> ToLocal() const
    {
        if (IsEmpty()) {
            return Local<T>();
        }
        return Local<T>(vm_, *this);
    }

    Local<T> ToLocal(const EcmaVM *vm) const
    {
        return Local<T>(vm, *this);
    }

    void Empty()
    {
        address_ = 0;
    }

    // This method must be called before Global is released.
    void FreeGlobalHandleAddr();

    inline T *operator*() const
    {
        return GetAddress();
    }

    inline T *operator->() const
    {
        return GetAddress();
    }

    inline bool IsEmpty() const
    {
        return GetAddress() == nullptr;
    }

    void SetWeak();

    void SetWeakCallback(void *ref, WeakRefClearCallBack freeGlobalCallBack,
                         WeakRefClearCallBack nativeFinalizeCallback);

    void ClearWeak();

    bool IsWeak() const;

private:
    inline T *GetAddress() const
    {
        return reinterpret_cast<T *>(address_);
    };
    inline void Update(const Global &that);
    uintptr_t address_ = 0U;
    const EcmaVM *vm_ {nullptr};
};

class ECMA_PUBLIC_API JSValueRef {
public:
    static Local<PrimitiveRef> Undefined(const EcmaVM *vm);
    static Local<PrimitiveRef> Null(const EcmaVM *vm);
    static Local<PrimitiveRef> True(const EcmaVM *vm);
    static Local<PrimitiveRef> False(const EcmaVM *vm);

    bool BooleaValue();
    int64_t IntegerValue(const EcmaVM *vm);
    uint32_t Uint32Value(const EcmaVM *vm);
    int32_t Int32Value(const EcmaVM *vm);

    Local<NumberRef> ToNumber(const EcmaVM *vm);
    Local<BooleanRef> ToBoolean(const EcmaVM *vm);
    Local<BigIntRef> ToBigInt(const EcmaVM *vm);
    Local<StringRef> ToString(const EcmaVM *vm);
    Local<ObjectRef> ToObject(const EcmaVM *vm);
    Local<NativePointerRef> ToNativePointer(const EcmaVM *vm);

    bool IsUndefined();
    bool IsNull();
    bool IsHole();
    bool IsTrue();
    bool IsFalse();
    bool IsNumber();
    bool IsBigInt();
    bool IsInt();
    bool WithinInt32();
    bool IsBoolean();
    bool IsString();
    bool IsSymbol();
    bool IsObject();
    bool IsArray(const EcmaVM *vm);
    bool IsJSArray(const EcmaVM *vm);
    bool IsConstructor();
    bool IsFunction();
    bool IsJSFunction();
    bool IsProxy();
    bool IsPromise();
    bool IsDataView();
    bool IsTypedArray();
    bool IsNativePointer();
    bool IsDate();
    bool IsError();
    bool IsMap();
    bool IsSet();
    bool IsWeakRef();
    bool IsWeakMap();
    bool IsWeakSet();
    bool IsRegExp();
    bool IsArrayIterator();
    bool IsStringIterator();
    bool IsSetIterator();
    bool IsMapIterator();
    bool IsArrayBuffer();
    bool IsBuffer();
    bool IsUint8Array();
    bool IsInt8Array();
    bool IsUint8ClampedArray();
    bool IsInt16Array();
    bool IsUint16Array();
    bool IsInt32Array();
    bool IsUint32Array();
    bool IsFloat32Array();
    bool IsFloat64Array();
    bool IsBigInt64Array();
    bool IsBigUint64Array();
    bool IsJSPrimitiveRef();
    bool IsJSPrimitiveNumber();
    bool IsJSPrimitiveInt();
    bool IsJSPrimitiveBoolean();
    bool IsJSPrimitiveString();

    bool IsGeneratorObject();
    bool IsJSPrimitiveSymbol();

    bool IsArgumentsObject();
    bool IsGeneratorFunction();
    bool IsAsyncFunction();
    bool IsJSLocale();
    bool IsJSDateTimeFormat();
    bool IsJSRelativeTimeFormat();
    bool IsJSIntl();
    bool IsJSNumberFormat();
    bool IsJSCollator();
    bool IsJSPluralRules();
    bool IsJSListFormat();
    bool IsAsyncGeneratorFunction();
    bool IsAsyncGeneratorObject();

    bool IsModuleNamespaceObject();
    bool IsSharedArrayBuffer();

    bool IsStrictEquals(const EcmaVM *vm, Local<JSValueRef> value);
    Local<StringRef> Typeof(const EcmaVM *vm);
    bool InstanceOf(const EcmaVM *vm, Local<JSValueRef> value);

    bool IsArrayList();
    bool IsDeque();
    bool IsHashMap();
    bool IsHashSet();
    bool IsLightWeightMap();
    bool IsLightWeightSet();
    bool IsLinkedList();
    bool IsLinkedListIterator();
    bool IsList();
    bool IsPlainArray();
    bool IsQueue();
    bool IsStack();
    bool IsTreeMap();
    bool IsTreeSet();
    bool IsVector();

private:
    JSTaggedType value_;
    friend JSNApi;
    template<typename T>
    friend class Global;
    template<typename T>
    friend class Local;
};
  
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class ECMA_PUBLIC_API PropertyAttribute {
public:
    static PropertyAttribute Default()
    {
        return PropertyAttribute();
    }
    PropertyAttribute() = default;
    PropertyAttribute(Local<JSValueRef> value, bool w, bool e, bool c)
        : value_(value),
          writable_(w),
          enumerable_(e),
          configurable_(c),
          hasWritable_(true),
          hasEnumerable_(true),
          hasConfigurable_(true)
    {}
    ~PropertyAttribute() = default;

    bool IsWritable() const
    {
        return writable_;
    }
    void SetWritable(bool flag)
    {
        writable_ = flag;
        hasWritable_ = true;
    }
    bool IsEnumerable() const
    {
        return enumerable_;
    }
    void SetEnumerable(bool flag)
    {
        enumerable_ = flag;
        hasEnumerable_ = true;
    }
    bool IsConfigurable() const
    {
        return configurable_;
    }
    void SetConfigurable(bool flag)
    {
        configurable_ = flag;
        hasConfigurable_ = true;
    }
    bool HasWritable() const
    {
        return hasWritable_;
    }
    bool HasConfigurable() const
    {
        return hasConfigurable_;
    }
    bool HasEnumerable() const
    {
        return hasEnumerable_;
    }
    Local<JSValueRef> GetValue(const EcmaVM *vm) const
    {
        if (value_.IsEmpty()) {
            return JSValueRef::Undefined(vm);
        }
        return value_;
    }
    void SetValue(Local<JSValueRef> value)
    {
        value_ = value;
    }
    inline bool HasValue() const
    {
        return !value_.IsEmpty();
    }
    Local<JSValueRef> GetGetter(const EcmaVM *vm) const
    {
        if (getter_.IsEmpty()) {
            return JSValueRef::Undefined(vm);
        }
        return getter_;
    }
    void SetGetter(Local<JSValueRef> value)
    {
        getter_ = value;
    }
    bool HasGetter() const
    {
        return !getter_.IsEmpty();
    }
    Local<JSValueRef> GetSetter(const EcmaVM *vm) const
    {
        if (setter_.IsEmpty()) {
            return JSValueRef::Undefined(vm);
        }
        return setter_;
    }
    void SetSetter(Local<JSValueRef> value)
    {
        setter_ = value;
    }
    bool HasSetter() const
    {
        return !setter_.IsEmpty();
    }

private:
    Local<JSValueRef> value_;
    Local<JSValueRef> getter_;
    Local<JSValueRef> setter_;
    bool writable_ = false;
    bool enumerable_ = false;
    bool configurable_ = false;
    bool hasWritable_ = false;
    bool hasEnumerable_ = false;
    bool hasConfigurable_ = false;
};

using NativePointerCallback = void (*)(void* value, void* hint);
class ECMA_PUBLIC_API NativePointerRef : public JSValueRef {
public:
    static Local<NativePointerRef> New(const EcmaVM *vm, void *nativePointer, size_t nativeBindingsize = 0);
    static Local<NativePointerRef> New(const EcmaVM *vm, void *nativePointer, NativePointerCallback callBack,
                                       void *data, size_t nativeBindingsize = 0);
    void *Value();
};

class ECMA_PUBLIC_API ObjectRef : public JSValueRef {
public:
    static constexpr int MAX_PROPERTIES_ON_STACK = 32;
    static inline ObjectRef *Cast(JSValueRef *value)
    {
        return static_cast<ObjectRef *>(value);
    }
    static Local<ObjectRef> New(const EcmaVM *vm);
    static Local<ObjectRef> NewWithProperties(const EcmaVM *vm, size_t propertyCount, const Local<JSValueRef> *keys,
                                              const PropertyAttribute *attributes);
    static Local<ObjectRef> NewWithNamedProperties(const EcmaVM *vm, size_t propertyCount, const char **keys,
                                                   const Local<JSValueRef> *values);
    static Local<ObjectRef> CreateAccessorData(const EcmaVM *vm, Local<FunctionRef> getter, Local<FunctionRef> setter);
    bool ConvertToNativeBindingObject(const EcmaVM *vm, Local<NativePointerRef> value);
    bool Set(const EcmaVM *vm, Local<JSValueRef> key, Local<JSValueRef> value);
    bool Set(const EcmaVM *vm, uint32_t key, Local<JSValueRef> value);
    bool SetAccessorProperty(const EcmaVM *vm, Local<JSValueRef> key, Local<FunctionRef> getter,
                             Local<FunctionRef> setter, PropertyAttribute attribute = PropertyAttribute::Default());
    Local<JSValueRef> Get(const EcmaVM *vm, Local<JSValueRef> key);
    Local<JSValueRef> Get(const EcmaVM *vm, int32_t key);

    bool GetOwnProperty(const EcmaVM *vm, Local<JSValueRef> key, PropertyAttribute &property);
    Local<ArrayRef> GetOwnPropertyNames(const EcmaVM *vm);
    Local<ArrayRef> GetAllPropertyNames(const EcmaVM *vm, uint32_t filter);
    Local<ArrayRef> GetOwnEnumerablePropertyNames(const EcmaVM *vm);
    Local<JSValueRef> GetPrototype(const EcmaVM *vm);
    bool SetPrototype(const EcmaVM *vm, Local<ObjectRef> prototype);

    bool DefineProperty(const EcmaVM *vm, Local<JSValueRef> key, PropertyAttribute attribute);

    bool Has(const EcmaVM *vm, Local<JSValueRef> key);
    bool Has(const EcmaVM *vm, uint32_t key);

    bool Delete(const EcmaVM *vm, Local<JSValueRef> key);
    bool Delete(const EcmaVM *vm, uint32_t key);

    Local<JSValueRef> Freeze(const EcmaVM *vm);
    Local<JSValueRef> Seal(const EcmaVM *vm);

    void SetNativePointerFieldCount(int32_t count);
    int32_t GetNativePointerFieldCount();
    void *GetNativePointerField(int32_t index);
    void SetNativePointerField(int32_t index,
                               void *nativePointer = nullptr,
                               NativePointerCallback callBack = nullptr,
                               void *data = nullptr, size_t nativeBindingsize = 0);
};

using FunctionCallback = Local<JSValueRef>(*)(JsiRuntimeCallInfo*);
using InternalFunctionCallback = JSValueRef(*)(JsiRuntimeCallInfo*);
class ECMA_PUBLIC_API FunctionRef : public ObjectRef {
public:
    static Local<FunctionRef> New(EcmaVM *vm, FunctionCallback nativeFunc, Deleter deleter = nullptr,
        void *data = nullptr, bool callNapi = false, size_t nativeBindingsize = 0);
    static Local<FunctionRef> New(EcmaVM *vm, InternalFunctionCallback nativeFunc, Deleter deleter,
        void *data = nullptr, bool callNapi = false, size_t nativeBindingsize = 0);
    static Local<FunctionRef> NewClassFunction(EcmaVM *vm, FunctionCallback nativeFunc, Deleter deleter,
        void *data, bool callNapi = false, size_t nativeBindingsize = 0);
    static Local<FunctionRef> NewClassFunction(EcmaVM *vm, InternalFunctionCallback nativeFunc, Deleter deleter,
        void *data, bool callNapi = false, size_t nativeBindingsize = 0);
    JSValueRef* CallForNapi(const EcmaVM *vm, JSValueRef *thisObj, JSValueRef *const argv[],
        int32_t length);
    Local<JSValueRef> Call(const EcmaVM *vm, Local<JSValueRef> thisObj, const Local<JSValueRef> argv[],
        int32_t length);
    Local<JSValueRef> Constructor(const EcmaVM *vm, const Local<JSValueRef> argv[], int32_t length);
    Local<JSValueRef> ConstructorOptimize(const EcmaVM *vm, JSValueRef* argv[], int32_t length);

    Local<JSValueRef> GetFunctionPrototype(const EcmaVM *vm);
    bool Inherit(const EcmaVM *vm, Local<FunctionRef> parent);
    void SetName(const EcmaVM *vm, Local<StringRef> name);
    Local<StringRef> GetName(const EcmaVM *vm);
    Local<StringRef> GetSourceCode(const EcmaVM *vm, int lineNumber);
    bool IsNative(const EcmaVM *vm);
    void SetData(const EcmaVM *vm, void *data, Deleter deleter = nullptr, bool callNapi = false);
    void* GetData(const EcmaVM *vm);
};

class ECMA_PUBLIC_API PrimitiveRef : public JSValueRef {
public:
    Local<JSValueRef> GetValue(const EcmaVM *vm);
};

class ECMA_PUBLIC_API SymbolRef : public PrimitiveRef {
public:
    static Local<SymbolRef> New(const EcmaVM *vm, Local<StringRef> description = Local<StringRef>());
    Local<StringRef> GetDescription(const EcmaVM *vm);
};

class ECMA_PUBLIC_API BooleanRef : public PrimitiveRef {
public:
    static Local<BooleanRef> New(const EcmaVM *vm, bool input);
    bool Value();
};

class ECMA_PUBLIC_API StringRef : public PrimitiveRef {
public:
    static inline StringRef *Cast(JSValueRef *value)
    {
        // check
        return static_cast<StringRef *>(value);
    }
    static Local<StringRef> NewFromUtf8(const EcmaVM *vm, const char *utf8, int length = -1);
    static Local<StringRef> NewFromUtf16(const EcmaVM *vm, const char16_t *utf16, int length = -1);
    std::string ToString();
    uint32_t Length();
    int32_t Utf8Length(const EcmaVM *vm);
    int WriteUtf8(char *buffer, int length, bool isWriteBuffer = false);
    int WriteUtf16(char16_t *buffer, int length);
    int WriteLatin1(char *buffer, int length);
    static Local<StringRef> GetNapiWrapperString(const EcmaVM *vm);
};

class ECMA_PUBLIC_API PromiseRejectInfo {
public:
    enum class ECMA_PUBLIC_API PROMISE_REJECTION_EVENT : uint32_t { REJECT = 0, HANDLE };
    PromiseRejectInfo(Local<JSValueRef> promise, Local<JSValueRef> reason,
                      PromiseRejectInfo::PROMISE_REJECTION_EVENT operation, void* data);
    ~PromiseRejectInfo() {}
    Local<JSValueRef> GetPromise() const;
    Local<JSValueRef> GetReason() const;
    PromiseRejectInfo::PROMISE_REJECTION_EVENT GetOperation() const;
    void* GetData() const;

private:
    Local<JSValueRef> promise_ {};
    Local<JSValueRef> reason_ {};
    PROMISE_REJECTION_EVENT operation_ = PROMISE_REJECTION_EVENT::REJECT;
    void* data_ {nullptr};
};

/**
 * An external exception handler.
 */
class ECMA_PUBLIC_API TryCatch {
public:
    explicit TryCatch(const EcmaVM *ecmaVm) : ecmaVm_(ecmaVm) {};

    /**
     * Consumes the exception by default if not rethrow explicitly.
     */
    ~TryCatch();

    bool HasCaught() const;
    void Rethrow();
    Local<ObjectRef> GetAndClearException();
    Local<ObjectRef> GetException();
    void ClearException();

    ECMA_DISALLOW_COPY(TryCatch);
    ECMA_DISALLOW_MOVE(TryCatch);

    bool getrethrow_()
    {
        return rethrow_;
    }

private:
    // Disable dynamic allocation
    void* operator new(size_t size) = delete;
    void operator delete(void*, size_t) = delete;
    void* operator new[](size_t size) = delete;
    void operator delete[](void*, size_t) = delete;

    const EcmaVM *ecmaVm_ {nullptr};
    bool rethrow_ {false};
};

class ECMA_PUBLIC_API BigIntRef : public PrimitiveRef {
public:
    static Local<BigIntRef> New(const EcmaVM *vm, uint64_t input);
    static Local<BigIntRef> New(const EcmaVM *vm, int64_t input);
    static Local<JSValueRef> CreateBigWords(const EcmaVM *vm, bool sign, uint32_t size, const uint64_t* words);
    void BigIntToInt64(const EcmaVM *vm, int64_t *value, bool *lossless);
    void BigIntToUint64(const EcmaVM *vm, uint64_t *value, bool *lossless);
    void GetWordsArray(bool* signBit, size_t wordCount, uint64_t* words);
    uint32_t GetWordsArraySize();
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class ECMA_PUBLIC_API LocalScope {
public:
    explicit LocalScope(const EcmaVM *vm);
    virtual ~LocalScope();

protected:
    inline LocalScope(const EcmaVM *vm, JSTaggedType value);

private:
    void *prevNext_ = nullptr;
    void *prevEnd_ = nullptr;
    int prevHandleStorageIndex_ {-1};
    void *thread_ = nullptr;
};

class ECMA_PUBLIC_API EscapeLocalScope final : public LocalScope {
public:
    explicit EscapeLocalScope(const EcmaVM *vm);
    ~EscapeLocalScope() override = default;

    ECMA_DISALLOW_COPY(EscapeLocalScope);
    ECMA_DISALLOW_MOVE(EscapeLocalScope);

    template<typename T>
    inline Local<T> Escape(Local<T> current)
    {
        ECMA_ASSERT(!alreadyEscape_);
        alreadyEscape_ = true;
        *(reinterpret_cast<T *>(escapeHandle_)) = **current;
        return Local<T>(escapeHandle_);
    }

private:
    bool alreadyEscape_ = false;
    uintptr_t escapeHandle_ = 0U;
};

class ECMA_PUBLIC_API IntegerRef : public PrimitiveRef {
public:
    static Local<IntegerRef> New(const EcmaVM *vm, int input);
    static Local<IntegerRef> NewFromUnsigned(const EcmaVM *vm, unsigned int input);
    int Value();
};

class ECMA_PUBLIC_API ArrayBufferRef : public ObjectRef {
public:
    static Local<ArrayBufferRef> New(const EcmaVM *vm, int32_t length);
    static Local<ArrayBufferRef> New(const EcmaVM *vm, void *buffer, int32_t length, const Deleter &deleter,
                                     void *data);

    int32_t ByteLength(const EcmaVM *vm);
    void *GetBuffer();

    void Detach(const EcmaVM *vm);
    bool IsDetach();
};

class ECMA_PUBLIC_API DateRef : public ObjectRef {
public:
    static Local<DateRef> New(const EcmaVM *vm, double time);
    Local<StringRef> ToString(const EcmaVM *vm);
    double GetTime();
};

class ECMA_PUBLIC_API TypedArrayRef : public ObjectRef {
public:
    uint32_t ByteLength(const EcmaVM *vm);
    uint32_t ByteOffset(const EcmaVM *vm);
    uint32_t ArrayLength(const EcmaVM *vm);
    Local<ArrayBufferRef> GetArrayBuffer(const EcmaVM *vm);
};

class ECMA_PUBLIC_API ArrayRef : public ObjectRef {
public:
    static Local<ArrayRef> New(const EcmaVM *vm, uint32_t length = 0);
    uint32_t Length(const EcmaVM *vm);
    static bool SetValueAt(const EcmaVM *vm, Local<JSValueRef> obj, uint32_t index, Local<JSValueRef> value);
    static Local<JSValueRef> GetValueAt(const EcmaVM *vm, Local<JSValueRef> obj, uint32_t index);
};

class ECMA_PUBLIC_API Int8ArrayRef : public TypedArrayRef {
public:
    static Local<Int8ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset, int32_t length);
};

class ECMA_PUBLIC_API Uint8ArrayRef : public TypedArrayRef {
public:
    static Local<Uint8ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset, int32_t length);
};

class ECMA_PUBLIC_API Uint8ClampedArrayRef : public TypedArrayRef {
public:
    static Local<Uint8ClampedArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset,
                                           int32_t length);
};

class ECMA_PUBLIC_API Int16ArrayRef : public TypedArrayRef {
public:
    static Local<Int16ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset, int32_t length);
};

class ECMA_PUBLIC_API Uint16ArrayRef : public TypedArrayRef {
public:
    static Local<Uint16ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset,
                                     int32_t length);
};

class ECMA_PUBLIC_API Int32ArrayRef : public TypedArrayRef {
public:
    static Local<Int32ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset, int32_t length);
};

class ECMA_PUBLIC_API Uint32ArrayRef : public TypedArrayRef {
public:
    static Local<Uint32ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset,
                                     int32_t length);
};

class ECMA_PUBLIC_API Float32ArrayRef : public TypedArrayRef {
public:
    static Local<Float32ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset,
                                      int32_t length);
};

class ECMA_PUBLIC_API Float64ArrayRef : public TypedArrayRef {
public:
    static Local<Float64ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset,
                                      int32_t length);
};

class ECMA_PUBLIC_API BigInt64ArrayRef : public TypedArrayRef {
public:
    static Local<BigInt64ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset,
                                      int32_t length);
};

class ECMA_PUBLIC_API BigUint64ArrayRef : public TypedArrayRef {
public:
    static Local<BigUint64ArrayRef> New(const EcmaVM *vm, Local<ArrayBufferRef> buffer, int32_t byteOffset,
                                      int32_t length);
};

class ECMA_PUBLIC_API Exception {
public:
    static Local<JSValueRef> Error(const EcmaVM *vm, Local<StringRef> message);
    static Local<JSValueRef> RangeError(const EcmaVM *vm, Local<StringRef> message);
    static Local<JSValueRef> ReferenceError(const EcmaVM *vm, Local<StringRef> message);
    static Local<JSValueRef> SyntaxError(const EcmaVM *vm, Local<StringRef> message);
    static Local<JSValueRef> TypeError(const EcmaVM *vm, Local<StringRef> message);
    static Local<JSValueRef> AggregateError(const EcmaVM *vm, Local<StringRef> message);
    static Local<JSValueRef> EvalError(const EcmaVM *vm, Local<StringRef> message);
    static Local<JSValueRef> OOMError(const EcmaVM *vm, Local<StringRef> message);
    static Local<JSValueRef> TerminationError(const EcmaVM *vm, Local<StringRef> message);
};

class ECMA_PUBLIC_API FunctionCallScope {
public:
    FunctionCallScope(EcmaVM *vm);
    ~FunctionCallScope();

private:
    EcmaVM *vm_;
};

class ECMA_PUBLIC_API JSExecutionScope {
public:
    explicit JSExecutionScope(const EcmaVM *vm);
    ~JSExecutionScope();
    ECMA_DISALLOW_COPY(JSExecutionScope);
    ECMA_DISALLOW_MOVE(JSExecutionScope);

private:
    void *lastCurrentThread_ = nullptr;
    bool isRevert_ = false;
};

/**
 * JsiRuntimeCallInfo is used for ace_engine and napi, is same to ark EcamRuntimeCallInfo except data.
 */
class ECMA_PUBLIC_API JsiRuntimeCallInfo
    : public ecmascript::base::AlignedStruct<ecmascript::base::AlignedPointer::Size(),
                                             ecmascript::base::AlignedPointer,
                                             ecmascript::base::AlignedPointer,
                                             ecmascript::base::AlignedPointer> {
    enum class Index : size_t {
        ThreadIndex = 0,
        NumArgsIndex,
        StackArgsIndex,
        NumOfMembers
    };
public:
    JsiRuntimeCallInfo() = default;
    ~JsiRuntimeCallInfo() = default;

    inline JSThread *GetThread() const
    {
        return thread_;
    }

    EcmaVM *GetVM() const;

    inline uint32_t GetArgsNumber() const
    {
        return numArgs_ - FIRST_ARGS_INDEX;
    }

    void* GetData();

    inline Local<JSValueRef> GetFunctionRef() const
    {
        return GetArgRef(FUNC_INDEX);
    }

    inline Local<JSValueRef> GetNewTargetRef() const
    {
        return GetArgRef(NEW_TARGET_INDEX);
    }

    inline Local<JSValueRef> GetThisRef() const
    {
        return GetArgRef(THIS_INDEX);
    }

    inline Local<JSValueRef> GetCallArgRef(uint32_t idx) const
    {
        return GetArgRef(FIRST_ARGS_INDEX + idx);
    }

private:
    enum ArgsIndex : uint8_t { FUNC_INDEX = 0, NEW_TARGET_INDEX, THIS_INDEX, FIRST_ARGS_INDEX };

    Local<JSValueRef> GetArgRef(uint32_t idx) const
    {
        return Local<JSValueRef>(GetArgAddress(idx));
    }

    uintptr_t GetArgAddress(uint32_t idx) const
    {
        if (idx < GetArgsNumber() + FIRST_ARGS_INDEX) {
            return reinterpret_cast<uintptr_t>(&stackArgs_[idx]);
        }
        return 0U;
    }

private:
    alignas(sizeof(JSTaggedType)) JSThread *thread_ {nullptr};
    alignas(sizeof(JSTaggedType))  uint32_t numArgs_ = 0;
    __extension__ alignas(sizeof(JSTaggedType)) JSTaggedType stackArgs_[0];
    friend class FunctionRef;
};

class ECMA_PUBLIC_API MapRef : public ObjectRef {
public:
    int32_t GetSize();
    int32_t GetTotalElements();
    Local<JSValueRef> Get(const EcmaVM *vm, Local<JSValueRef> key);
    Local<JSValueRef> GetKey(const EcmaVM *vm, int entry);
    Local<JSValueRef> GetValue(const EcmaVM *vm, int entry);
    static Local<MapRef> New(const EcmaVM *vm);
    void Set(const EcmaVM *vm, Local<JSValueRef> key, Local<JSValueRef> value);
};

class ECMA_PUBLIC_API BufferRef : public ObjectRef {
public:
    static Local<BufferRef> New(const EcmaVM *vm, int32_t length);
    static Local<BufferRef> New(const EcmaVM *vm, void *buffer, int32_t length, const Deleter &deleter,
                                void *data);

    int32_t ByteLength(const EcmaVM *vm);
    void *GetBuffer();
    static ecmascript::JSTaggedValue BufferToStringCallback(ecmascript::EcmaRuntimeCallInfo *ecmaRuntimeCallInfo);
};

class ECMA_PUBLIC_API PromiseRef : public ObjectRef {
public:
    Local<PromiseRef> Catch(const EcmaVM *vm, Local<FunctionRef> handler);
    Local<PromiseRef> Then(const EcmaVM *vm, Local<FunctionRef> handler);
    Local<PromiseRef> Finally(const EcmaVM *vm, Local<FunctionRef> handler);
    Local<PromiseRef> Then(const EcmaVM *vm, Local<FunctionRef> onFulfilled, Local<FunctionRef> onRejected);
};

class ECMA_PUBLIC_API PromiseCapabilityRef : public ObjectRef {
public:
    static Local<PromiseCapabilityRef> New(const EcmaVM *vm);
    bool Resolve(const EcmaVM *vm, Local<JSValueRef> value);
    bool Reject(const EcmaVM *vm, Local<JSValueRef> reason);
    Local<PromiseRef> GetPromise(const EcmaVM *vm);
};

class ECMA_PUBLIC_API NumberRef : public PrimitiveRef {
public:
    static Local<NumberRef> New(const EcmaVM *vm, double input);
    static Local<NumberRef> New(const EcmaVM *vm, int32_t input);
    static Local<NumberRef> New(const EcmaVM *vm, uint32_t input);
    static Local<NumberRef> New(const EcmaVM *vm, int64_t input);

    double Value();
};

class ECMA_PUBLIC_API DataViewRef : public ObjectRef {
public:
    static Local<DataViewRef> New(const EcmaVM *vm, Local<ArrayBufferRef> arrayBuffer, uint32_t byteOffset,
                                  uint32_t byteLength);
    uint32_t ByteLength();
    uint32_t ByteOffset();
    Local<ArrayBufferRef> GetArrayBuffer(const EcmaVM *vm);
};

class ECMA_PUBLIC_API MapIteratorRef : public ObjectRef {
public:
    int32_t GetIndex();
    Local<JSValueRef> GetKind(const EcmaVM *vm);
};

class ECMA_PUBLIC_API JSNApi {
public:
    struct DebugOption {
        const char *libraryPath;
        bool isDebugMode = false;
        int port = -1;
    };
    using DebuggerPostTask = std::function<void(std::function<void()>&&)>;

    using UncatchableErrorHandler = std::function<void(panda::TryCatch&)>;

    struct NativeBindingInfo {
        static NativeBindingInfo* CreateNewInstance() { return new NativeBindingInfo(); }
        void *env = nullptr;
        void *nativeValue = nullptr;
        void *attachFunc = nullptr;
        void *attachData = nullptr;
        void *detachFunc = nullptr;
        void *detachData = nullptr;
        void *hint = nullptr;
    };

    // JSVM
    // fixme: Rename SEMI_GC to YOUNG_GC
    enum class ECMA_PUBLIC_API TRIGGER_GC_TYPE : uint8_t { SEMI_GC, OLD_GC, FULL_GC };

    enum class PatchErrorCode : uint8_t {
        SUCCESS = 0,
        PATCH_HAS_LOADED,
        PATCH_NOT_LOADED,
        FILE_NOT_EXECUTED,
        FILE_NOT_FOUND,
        PACKAGE_NOT_ESMODULE,
        MODIFY_IMPORT_EXPORT_NOT_SUPPORT,
        INTERNAL_ERROR
    };

    static EcmaVM *CreateJSVM(const RuntimeOption &option);
    static void DestroyJSVM(EcmaVM *ecmaVm);
    static void RegisterUncatchableErrorHandler(EcmaVM *ecmaVm, const UncatchableErrorHandler &handler);

    // aot load
    static void LoadAotFile(EcmaVM *vm, const std::string &moduleName);
    // context
    static EcmaContext *CreateJSContext(EcmaVM *vm);
    static void SwitchCurrentContext(EcmaVM *vm, EcmaContext *context);
    static void DestroyJSContext(EcmaVM *vm, EcmaContext *context);

    // context execute
    static bool ExecuteInContext(EcmaVM *vm, const std::string &fileName, const std::string &entry,
                                 bool needUpdate = false);
    // JS code
    static bool Execute(EcmaVM *vm, const std::string &fileName, const std::string &entry, bool needUpdate = false);
    static bool Execute(EcmaVM *vm, const uint8_t *data, int32_t size, const std::string &entry,
                        const std::string &filename = "", bool needUpdate = false);
    // merge abc, execute module buffer
    static bool ExecuteModuleBuffer(EcmaVM *vm, const uint8_t *data, int32_t size, const std::string &filename = "",
                                    bool needUpdate = false);
    static bool ExecuteModuleFromBuffer(EcmaVM *vm, const void *data, int32_t size, const std::string &file);
    static Local<ObjectRef> GetExportObject(EcmaVM *vm, const std::string &file, const std::string &key);
    static Local<ObjectRef> GetExportObjectFromBuffer(EcmaVM *vm, const std::string &file, const std::string &key);
    static Local<ObjectRef> ExecuteNativeModule(EcmaVM *vm, const std::string &key);
    // secure memory check
    static bool CheckSecureMem(uintptr_t mem);

    /*
     * Execute panda file from secure mem. secure memory lifecycle managed externally.
     * The data parameter needs to be created externally by an external caller and managed externally
     * by the external caller. The size parameter is the size of the data memory. The entry parameter
     * is the name of the entry function. The filename parameter is used to uniquely identify this
     * memory internally.
     */
    static bool ExecuteSecure(EcmaVM *vm, uint8_t *data, int32_t size, const std::string &entry,
                                const std::string &filename = "", bool needUpdate = false);
    /*
     * Execute panda file(merge abc) from secure mem. secure memory lifecycle managed externally.
     * The data parameter needs to be created externally by an external caller and managed externally
     * by the external caller. The size parameter is the size of the data memory. The filename parameter
     * is used to uniquely identify this memory internally.
     */
    static bool ExecuteModuleBufferSecure(EcmaVM *vm, uint8_t *data, int32_t size, const std::string &filename = "",
                                          bool needUpdate = false);

    // ObjectRef Operation
    static Local<ObjectRef> GetGlobalObject(const EcmaVM *vm);
    static void ExecutePendingJob(const EcmaVM *vm);

    // Memory
    // fixme: Rename SEMI_GC to YOUNG_GC
    static void TriggerGC(const EcmaVM *vm, TRIGGER_GC_TYPE gcType = TRIGGER_GC_TYPE::SEMI_GC);
    // Exception
    static void ThrowException(const EcmaVM *vm, Local<JSValueRef> error);
    static void PrintExceptionInfo(const EcmaVM *vm);
    static Local<ObjectRef> GetAndClearUncaughtException(const EcmaVM *vm);
    static Local<ObjectRef> GetUncaughtException(const EcmaVM *vm);
    static bool IsExecutingPendingJob(const EcmaVM *vm);
    static bool HasPendingException(const EcmaVM *vm);
    static bool HasPendingJob(const EcmaVM *vm);
    static void EnableUserUncaughtErrorHandler(EcmaVM *vm);
    // prevewer debugger.
    static bool StartDebugger(EcmaVM *vm, const DebugOption &option, int32_t instanceId = 0,
        const DebuggerPostTask &debuggerPostTask = {});
    // To be compatible with the old process.
    static bool StartDebuggerForOldProcess(EcmaVM *vm, const DebugOption &option, int32_t instanceId = 0,
        const DebuggerPostTask &debuggerPostTask = {});
    // socketpair process in ohos platform.
    static bool StartDebuggerForSocketPair(uint32_t tid, const DebugOption &option, int socketfd = -1,
        const DebuggerPostTask &debuggerPostTask = {});
    static bool StopDebugger(uint32_t tid);
    static bool NotifyDebugMode(uint32_t tid, EcmaVM *vm, const char *libraryPath, const DebugOption &option,
                                int32_t instanceId = 0, const DebuggerPostTask &debuggerPostTask = {},
                                bool debugApp = false, bool debugMode = false);
    static bool StopDebugger(EcmaVM *vm);
    static bool IsMixedDebugEnabled(const EcmaVM *vm);
    static void NotifyNativeCalling(const EcmaVM *vm, const void *nativeAddress);
    static void NotifyNativeReturnJS(const EcmaVM *vm);
    static void NotifyLoadModule(const EcmaVM *vm);
    static void SetDeviceDisconnectCallback(EcmaVM *vm, DeviceDisconnectCallback cb);
    // Serialize & Deserialize.
    static void* SerializeValue(const EcmaVM *vm, Local<JSValueRef> data, Local<JSValueRef> transfer);
    static Local<JSValueRef> DeserializeValue(const EcmaVM *vm, void *recoder, void *hint);
    static void DeleteSerializationData(void *data);
    static void SetHostPromiseRejectionTracker(EcmaVM *vm, void *cb, void* data);
    static void SetHostResolveBufferTracker(EcmaVM *vm,
        std::function<bool(std::string dirPath, uint8_t **buff, size_t *buffSize)> cb);
    static void SetUnloadNativeModuleCallback(EcmaVM *vm, const std::function<bool(const std::string &moduleKey)> &cb);
    static void SetNativePtrGetter(EcmaVM *vm, void* cb);
    static void SetSourceMapTranslateCallback(EcmaVM *vm, SourceMapTranslateCallback cb);
    static void SetHostEnqueueJob(const EcmaVM* vm, Local<JSValueRef> cb);
    static void InitializeIcuData(const ecmascript::JSRuntimeOptions &options);
    static void InitializeMemMapAllocator();
    static void InitializePGOProfiler(const ecmascript::JSRuntimeOptions &options);
    static void DestroyAnDataManager();
    static void DestroyMemMapAllocator();
    static void DestroyPGOProfiler();
    static EcmaVM* CreateEcmaVM(const ecmascript::JSRuntimeOptions &options);
    static void PreFork(EcmaVM *vm);
    static void PostFork(EcmaVM *vm, const RuntimeOption &option);
    static void AddWorker(EcmaVM *hostVm, EcmaVM *workerVm);
    static bool DeleteWorker(EcmaVM *hostVm, EcmaVM *workerVm);
    static void GetStackBeforeCallNapiSuccess(EcmaVM *vm, bool &getStackBeforeCallNapiSuccess);
    static void GetStackAfterCallNapi(EcmaVM *vm);

    static PatchErrorCode LoadPatch(EcmaVM *vm, const std::string &patchFileName, const std::string &baseFileName);
    static PatchErrorCode LoadPatch(EcmaVM *vm,
                                    const std::string &patchFileName, const void *patchBuffer, size_t patchSize,
                                    const std::string &baseFileName, const void *baseBuffer, size_t baseSize);
    static PatchErrorCode UnloadPatch(EcmaVM *vm, const std::string &patchFileName);
    // check whether the exception is caused by quickfix methods.
    static bool IsQuickFixCausedException(EcmaVM *vm, Local<ObjectRef> exception, const std::string &patchFileName);
    // register quickfix query function.
    static void RegisterQuickFixQueryFunc(EcmaVM *vm, std::function<bool(std::string baseFileName,
                        std::string &patchFileName,
                        void **patchBuffer,
                        size_t &patchSize)> callBack);
    static bool IsBundle(EcmaVM *vm);
    static void SetBundle(EcmaVM *vm, bool value);
    static void SetAssetPath(EcmaVM *vm, const std::string &assetPath);
    static void SetMockModuleList(EcmaVM *vm, const std::map<std::string, std::string> &list);

    static void SetLoop(EcmaVM *vm, void *loop);
    static std::string GetAssetPath(EcmaVM *vm);
    static bool InitForConcurrentThread(EcmaVM *vm, ConcurrentCallback cb, void *data);
    static bool InitForConcurrentFunction(EcmaVM *vm, Local<JSValueRef> func, void *taskInfo);
    static void* GetCurrentTaskInfo(const EcmaVM *vm);
    static void SetBundleName(EcmaVM *vm, const std::string &bundleName);
    static std::string GetBundleName(EcmaVM *vm);
    static void SetModuleName(EcmaVM *vm, const std::string &moduleName);
    static std::string GetModuleName(EcmaVM *vm);
    static std::string GetCurrentModuleName(EcmaVM *vm);
    static void AllowCrossThreadExecution(EcmaVM *vm);
    static void SynchronizVMInfo(EcmaVM *vm, const EcmaVM *hostVM);
    static bool IsProfiling(EcmaVM *vm);
    static void SetProfilerState(const EcmaVM *vm, bool value);
    static void SetRequestAotCallback(EcmaVM *vm, const std::function<int32_t(const std::string &bundleName,
                    const std::string &moduleName,
                    int32_t triggerMode)> &cb);

private:
    static int vmCount_;
    static bool initialize_;
    static bool CreateRuntime(const RuntimeOption &option);
    static bool DestroyRuntime();

    static uintptr_t GetHandleAddr(const EcmaVM *vm, uintptr_t localAddress);
    static uintptr_t GetGlobalHandleAddr(const EcmaVM *vm, uintptr_t localAddress);
    static uintptr_t SetWeak(const EcmaVM *vm, uintptr_t localAddress);
    static uintptr_t SetWeakCallback(const EcmaVM *vm, uintptr_t localAddress, void *ref,
                                     WeakRefClearCallBack freeGlobalCallBack,
                                     WeakRefClearCallBack nativeFinalizeCallback);
    static uintptr_t ClearWeak(const EcmaVM *vm, uintptr_t localAddress);
    static bool IsWeak(const EcmaVM *vm, uintptr_t localAddress);
    static void DisposeGlobalHandleAddr(const EcmaVM *vm, uintptr_t addr);
    template<typename T>
    friend class Global;
    template<typename T>
    friend class CopyableGlobal;
    template<typename T>
    friend class Local;
    friend class test::JSNApiTests;
};

class ECMA_PUBLIC_API ProxyRef : public ObjectRef {
public:
    Local<JSValueRef> GetHandler(const EcmaVM *vm);
    Local<JSValueRef> GetTarget(const EcmaVM *vm);
    bool IsRevoked();
};

class ECMA_PUBLIC_API WeakMapRef : public ObjectRef {
public:
    int32_t GetSize();
    int32_t GetTotalElements();
    Local<JSValueRef> GetKey(const EcmaVM *vm, int entry);
    Local<JSValueRef> GetValue(const EcmaVM *vm, int entry);
};

class ECMA_PUBLIC_API SetRef : public ObjectRef {
public:
    int32_t GetSize();
    int32_t GetTotalElements();
    Local<JSValueRef> GetValue(const EcmaVM *vm, int entry);
};

class ECMA_PUBLIC_API WeakSetRef : public ObjectRef {
public:
    int32_t GetSize();
    int32_t GetTotalElements();
    Local<JSValueRef> GetValue(const EcmaVM *vm, int entry);
};

class ECMA_PUBLIC_API SetIteratorRef : public ObjectRef {
public:
    int32_t GetIndex();
    Local<JSValueRef> GetKind(const EcmaVM *vm);
};
}
#endif