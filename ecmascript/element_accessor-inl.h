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

#ifndef ECMASCRIPT_ELEMENT_ACCESSOR_INL_H
#define ECMASCRIPT_ELEMENT_ACCESSOR_INL_H

#include "ecmascript/element_accessor.h"

#include "ecmascript/mem/barriers-inl.h"
#include "ecmascript/js_tagged_value-inl.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/tagged_array.h"

namespace panda::ecmascript {

template<typename T>
inline void ElementAccessor::Set(const JSThread *thread, JSHandle<JSObject> receiver, uint32_t idx,
                                 const JSHandle<T> &value, bool needTransition, ElementsKind extraKind)
{
    // Change elementsKind
    ElementsKind oldKind = receiver->GetClass()->GetElementsKind();
    if (needTransition && JSHClass::TransitToElementsKind(thread, receiver, value, extraKind)) {
        ElementsKind newKind = receiver->GetClass()->GetElementsKind();
        Elements::MigrateArrayWithKind(thread, receiver, oldKind, newKind);
    }
    
    TaggedArray *elements = TaggedArray::Cast(receiver->GetElements());
    ASSERT(idx < elements->GetLength());
    size_t offset = JSTaggedValue::TaggedTypeSize() * idx;

    ElementsKind kind = receiver->GetClass()->GetElementsKind();
    if (!elements->GetClass()->IsMutantTaggedArray()) {
        kind = ElementsKind::GENERIC;
    }

    JSTaggedType convertedValue = ConvertTaggedValueWithElementsKind(value.GetTaggedValue(), kind);
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if (value.GetTaggedValue().IsHeapObject()) {
        Barriers::SetObject<true>(thread, elements->GetData(), offset, convertedValue);
    } else {  // NOLINTNEXTLINE(readability-misleading-indentation)
        Barriers::SetPrimitive<JSTaggedType>(elements->GetData(), offset, convertedValue);
    }
}

template <bool needBarrier>
inline void ElementAccessor::Set(const JSThread *thread, JSHandle<JSObject> receiver, uint32_t idx,
                                 const JSTaggedValue &value, bool needTransition, ElementsKind extraKind)
{
    // Change elementsKind
    ElementsKind oldKind = receiver->GetClass()->GetElementsKind();
    if (needTransition &&
        JSHClass::TransitToElementsKind(thread, receiver, JSHandle<JSTaggedValue>(thread, value), extraKind)) {
        ElementsKind newKind = receiver->GetClass()->GetElementsKind();
        Elements::MigrateArrayWithKind(thread, receiver, oldKind, newKind);
    }

    TaggedArray *elements = TaggedArray::Cast(receiver->GetElements());
    ASSERT(idx < elements->GetLength());
    size_t offset = JSTaggedValue::TaggedTypeSize() * idx;

    ElementsKind kind = receiver->GetClass()->GetElementsKind();
    if (!elements->GetClass()->IsMutantTaggedArray()) {
        kind = ElementsKind::GENERIC;
    }

    JSTaggedType convertedValue = ConvertTaggedValueWithElementsKind(value, kind);
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if (needBarrier && value.IsHeapObject()) {
        Barriers::SetObject<true>(thread, elements->GetData(), offset, convertedValue);
    } else {  // NOLINTNEXTLINE(readability-misleading-indentation)
        Barriers::SetPrimitive<JSTaggedType>(elements->GetData(), offset, convertedValue);
    }
}
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_ELEMENT_ACCESSOR_INL_H
