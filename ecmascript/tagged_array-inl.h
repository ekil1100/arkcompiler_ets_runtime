/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_TAGGED_ARRAY_INL_H
#define ECMASCRIPT_TAGGED_ARRAY_INL_H

#include "ecmascript/tagged_array.h"
#include "ecmascript/mem/barriers-inl.h"

namespace panda::ecmascript {
template<typename T>
inline void TaggedArray::Set(const JSThread *thread, uint32_t idx, const JSHandle<T> &value)
{
    ASSERT(idx < GetLength());
    size_t offset = JSTaggedValue::TaggedTypeSize() * idx;

    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if (value.GetTaggedValue().IsHeapObject()) {
        Barriers::SetObject<true>(thread, GetData(), offset, value.GetTaggedValue().GetRawData());
    } else {  // NOLINTNEXTLINE(readability-misleading-indentation)
        Barriers::SetPrimitive<JSTaggedType>(GetData(), offset, value.GetTaggedValue().GetRawData());
    }
}

inline JSTaggedValue TaggedArray::Get(uint32_t idx) const
{
    ASSERT(idx < GetLength());
    // Note: Here we can't statically decide the element type is a primitive or heap object, especially for
    //       dynamically-typed languages like JavaScript. So we simply skip the read-barrier.
    size_t offset = JSTaggedValue::TaggedTypeSize() * idx;
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    return JSTaggedValue(Barriers::GetValue<JSTaggedType>(GetData(), offset));
}

}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_TAGGED_ARRAY_INL_H
