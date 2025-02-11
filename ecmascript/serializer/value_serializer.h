/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_SERIALIZER_VALUE_SERIALIZER_H
#define ECMASCRIPT_SERIALIZER_VALUE_SERIALIZER_H

#include "ecmascript/serializer/base_serializer-inl.h"

namespace panda::ecmascript {

class ValueSerializer : public BaseSerializer {
public:
    explicit ValueSerializer(JSThread *thread) : BaseSerializer(thread) {}
    ~ValueSerializer() override = default;
    NO_COPY_SEMANTIC(ValueSerializer);
    NO_MOVE_SEMANTIC(ValueSerializer);

    bool WriteValue(JSThread *thread, const JSHandle<JSTaggedValue> &value, const JSHandle<JSTaggedValue> &transfer);

private:
    void SerializeObjectImpl(TaggedObject *object, bool isWeak = false) override;
    void SerializeJSError(TaggedObject *object);
    void SerializeNativeBindingObject(TaggedObject *object);
    bool SerializeJSArrayBufferPrologue(TaggedObject *object);
    void SerializeJSSharedArrayBufferPrologue(TaggedObject *object);
    void SerializeMethodPrologue(Method *method);
    void SerializeJSRegExpPrologue(JSRegExp *jsRegExp);
    void InitTransferSet(CUnorderedSet<uintptr_t> transferDataSet);
    void ClearTransferSet();
    bool PrepareTransfer(JSThread *thread, const JSHandle<JSTaggedValue> &transfer);
    bool CheckObjectCanSerialize(TaggedObject *object);

    bool IsInternalJSType(JSType type)
    {
        if (type >= JSType::JS_RECORD_FIRST && type <= JSType::JS_RECORD_LAST) {
            return false;
        }
        return type >= JSType::HCLASS && type <= JSType::TYPE_LAST && type != JSType::SYMBOL;
    }

private:
    bool defaultTransfer_ {false};
    bool notSupport_ {false};
    CUnorderedSet<uintptr_t> transferDataSet_;
};
}

#endif  // ECMASCRIPT_SERIALIZER_BASE_SERIALIZER_H