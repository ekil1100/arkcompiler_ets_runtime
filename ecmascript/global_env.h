/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_GLOBAL_ENV_H
#define ECMASCRIPT_GLOBAL_ENV_H

#include "ecmascript/js_global_object.h"
#include "ecmascript/js_function.h"
#include "ecmascript/lexical_env.h"
#include "ecmascript/js_handle.h"
#include "ecmascript/global_env_constants-inl.h"
#include "ecmascript/global_env_fields.h"
#include "ecmascript/snapshot/mem/snapshot_env.h"

namespace panda::ecmascript {
class JSThread;
class GlobalEnv : public TaggedObject {
public:
    using Field = GlobalEnvField;

#define GLOBAL_ENV_SLOT(type, name, index) \
    static constexpr uint16_t index = static_cast<uint16_t>(GlobalEnvField::index);

    GLOBAL_ENV_FIELDS(GLOBAL_ENV_SLOT)
    static constexpr uint16_t FIRST_DETECTOR_SYMBOL_INDEX = static_cast<uint16_t>(Field::REPLACE_SYMBOL_INDEX);
    static constexpr uint16_t LAST_DETECTOR_SYMBOL_INDEX = static_cast<uint16_t>(Field::SPECIES_SYMBOL_INDEX);
    static constexpr uint16_t FINAL_INDEX = static_cast<uint16_t>(GlobalEnvField::FINAL_INDEX);
    static constexpr uint8_t RESERVED_LENGTH = 1; // divide the gc area
    static constexpr uint16_t JSTHREAD_INDEX = FINAL_INDEX; // not need gc
#undef GLOBAL_ENV_SLOT

    JSTaggedValue GetGlobalObject() const
    {
        return GetJSGlobalObject().GetTaggedValue();
    }

    uintptr_t ComputeObjectAddress(size_t index) const
    {
        return reinterpret_cast<uintptr_t>(this) + HEADER_SIZE + index * JSTaggedValue::TaggedTypeSize();
    }

    JSHandle<JSTaggedValue> GetGlobalEnvObjectByIndex(size_t index) const
    {
        ASSERT(index < FINAL_INDEX);
        uintptr_t address = ComputeObjectAddress(index);
        JSHandle<JSTaggedValue> result(address);
        return result;
    }

    JSHandle<JSTaggedValue> GetNoLazyEnvObjectByIndex(size_t index) const
    {
        JSHandle<JSTaggedValue> result = GetGlobalEnvObjectByIndex(index);
        if (result->IsInternalAccessor()) {
            JSThread *thread = GetJSThread();
            AccessorData *accessor = AccessorData::Cast(result->GetTaggedObject());
            accessor->CallInternalGet(thread, JSHandle<JSObject>::Cast(GetJSGlobalObject()));
        }
        return result;
    }

    size_t GetGlobalEnvFieldSize() const
    {
        return FINAL_INDEX;
    }

    void Init(JSThread *thread);

    static GlobalEnv *Cast(TaggedObject *object)
    {
        ASSERT(JSTaggedValue(object).IsJSGlobalEnv());
        return reinterpret_cast<GlobalEnv *>(object);
    }

    JSThread* GetJSThread() const
    {
        uintptr_t address = ComputeObjectAddress(JSTHREAD_INDEX);
        return *reinterpret_cast<JSThread**>(address);
    }

    void SetJSThread(JSThread *thread)
    {
        uintptr_t address = ComputeObjectAddress(JSTHREAD_INDEX);
        *reinterpret_cast<JSThread**>(address) = thread;
    }

    // For work serialize, add initialized global env object to snapshot env map
    void AddValueToSnapshotEnv(const JSThread *thread, JSTaggedValue value, uint16_t index, uint32_t offset)
    {
        if (!value.IsInternalAccessor()) {
            SnapshotEnv *snapshotEnv = thread->GetEcmaVM()->GetSnapshotEnv();
            if (!RemoveValueFromSnapshotEnv(snapshotEnv, value, offset)) {
                return;
            }
            size_t globalConstCount = thread->GlobalConstants()->GetConstantCount();
            snapshotEnv->Push(value.GetRawData(), index + globalConstCount);
        }
    }

    // For work serialize, remove old value from snapshot env map
    bool RemoveValueFromSnapshotEnv(SnapshotEnv *snapshotEnv, JSTaggedValue value, uint32_t offset)
    {
        JSTaggedValue oldValue(Barriers::GetValue<JSTaggedType>(this, offset));
        if (oldValue == value) {
            return false;
        }
        if (oldValue.IsHeapObject() && !oldValue.IsInternalAccessor()) {
            // Remove old value
            snapshotEnv->Remove(oldValue.GetRawData());
        }
        return true;
    }

    JSHandle<JSTaggedValue> GetSymbol(JSThread *thread, const JSHandle<JSTaggedValue> &string);
    JSHandle<JSTaggedValue> GetStringFunctionByName(JSThread *thread, const char *name);
    JSHandle<JSTaggedValue> GetStringPrototypeFunctionByName(JSThread *thread, const char *name);

    static inline uintptr_t GetFirstDetectorSymbolAddr(const GlobalEnv *env)
    {
        constexpr size_t offset = HEADER_SIZE + FIRST_DETECTOR_SYMBOL_INDEX * JSTaggedValue::TaggedTypeSize();
        uintptr_t addr = reinterpret_cast<uintptr_t>(env) + offset;
        return *reinterpret_cast<uintptr_t *>(addr);
    }

    static uintptr_t GetLastDetectorSymbolAddr(const GlobalEnv *env)
    {
        constexpr size_t offset = HEADER_SIZE + LAST_DETECTOR_SYMBOL_INDEX * JSTaggedValue::TaggedTypeSize();
        uintptr_t addr = reinterpret_cast<uintptr_t>(env) + offset;
        return *reinterpret_cast<uintptr_t *>(addr);
    }

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GLOBAL_ENV_FIELD_ACCESSORS(type, name, index)                                                   \
    inline JSHandle<type> Get##name() const                                                             \
    {                                                                                                   \
        const uintptr_t address =                                                                       \
            reinterpret_cast<uintptr_t>(this) + HEADER_SIZE + index * JSTaggedValue::TaggedTypeSize();  \
        JSHandle<type> result(address);                                                                 \
        if (result.GetTaggedValue().IsInternalAccessor()) {                                             \
            JSThread *thread = GetJSThread();                                                           \
            AccessorData *accessor = AccessorData::Cast(result.GetTaggedValue().GetTaggedObject());     \
            accessor->CallInternalGet(thread, JSHandle<JSObject>::Cast(GetJSGlobalObject()));           \
        }                                                                                               \
        return result;                                                                                  \
    }                                                                                                   \
    inline JSTaggedValue GetTagged##name() const                                                        \
    {                                                                                                   \
        uint32_t offset = HEADER_SIZE + index * JSTaggedValue::TaggedTypeSize();                        \
        JSTaggedValue result(Barriers::GetValue<JSTaggedType>(this, offset));                           \
        if (result.IsInternalAccessor()) {                                                              \
            JSThread *thread = GetJSThread();                                                           \
            AccessorData *accessor = AccessorData::Cast(result.GetTaggedObject());                      \
            accessor->CallInternalGet(thread, JSHandle<JSObject>::Cast(GetJSGlobalObject()));           \
        }                                                                                               \
        return result;                                                                                  \
    }                                                                                                   \
    template<typename T>                                                                                \
    inline void Set##name(const JSThread *thread, JSHandle<T> value, BarrierMode mode = WRITE_BARRIER)  \
    {                                                                                                   \
        uint32_t offset = HEADER_SIZE + index * JSTaggedValue::TaggedTypeSize();                        \
        if (mode == WRITE_BARRIER && value.GetTaggedValue().IsHeapObject()) {                           \
            AddValueToSnapshotEnv(thread, value.GetTaggedValue(), index, offset);                       \
            Barriers::SetObject<true>(thread, this, offset, value.GetTaggedValue().GetRawData());       \
        } else {                                                                                        \
            SnapshotEnv *snapshotEnv = thread->GetEcmaVM()->GetSnapshotEnv();                           \
            RemoveValueFromSnapshotEnv(snapshotEnv, value.GetTaggedValue(), offset);                    \
            Barriers::SetPrimitive<JSTaggedType>(this, offset, value.GetTaggedValue().GetRawData());    \
        }                                                                                               \
    }                                                                                                   \
    inline void Set##name(const JSThread *thread, type value, BarrierMode mode = WRITE_BARRIER)         \
    {                                                                                                   \
        uint32_t offset = HEADER_SIZE + index * JSTaggedValue::TaggedTypeSize();                        \
        if (mode == WRITE_BARRIER && value.IsHeapObject()) {                                            \
            AddValueToSnapshotEnv(thread, value, index, offset);                                        \
            Barriers::SetObject<true>(thread, this, offset, value.GetRawData());                        \
        } else {                                                                                        \
            SnapshotEnv *snapshotEnv = thread->GetEcmaVM()->GetSnapshotEnv();                           \
            RemoveValueFromSnapshotEnv(snapshotEnv, value, offset);                                     \
            Barriers::SetPrimitive<JSTaggedType>(this, offset, value.GetRawData());                     \
        }                                                                                               \
    }
    GLOBAL_ENV_FIELDS(GLOBAL_ENV_FIELD_ACCESSORS)
#undef GLOBAL_ENV_FIELD_ACCESSORS

    static constexpr size_t HEADER_SIZE = TaggedObjectSize();
    static constexpr size_t DATA_SIZE = HEADER_SIZE + FINAL_INDEX * JSTaggedValue::TaggedTypeSize();
    static constexpr size_t SIZE = DATA_SIZE + RESERVED_LENGTH * JSTaggedValue::TaggedTypeSize();

    DECL_VISIT_OBJECT(HEADER_SIZE, DATA_SIZE);

    DECL_DUMP()
};
}  // namespace panda::ecmascript

#endif  // ECMASCRIPT_GLOBAL_ENV_H
