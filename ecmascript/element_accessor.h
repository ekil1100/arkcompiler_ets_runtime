/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_ELEMENT_ACCESSOR_H
#define ECMASCRIPT_ELEMENT_ACCESSOR_H

#include "ecmascript/js_hclass.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/tagged_array.h"

namespace panda {
namespace ecmascript {
// ElementAccessor intends to replace all .GetElements and share the common following methods.
class ElementAccessor {
public:
    static JSTaggedValue Get(JSHandle<JSObject> receiver, uint32_t idx);
    static JSTaggedValue Get(JSObject *receiver, uint32_t idx);

    template<typename T>
    static void Set(const JSThread *thread, JSHandle<JSObject> receiver, uint32_t idx, const JSHandle<T> &value,
                    bool needTransition, ElementsKind extraKind = ElementsKind::NONE);

    template <bool needBarrier = true>
    static void Set(const JSThread *thread, JSHandle<JSObject> receiver, uint32_t idx, const JSTaggedValue &value,
                    bool needTransition, ElementsKind extraKind = ElementsKind::NONE);

    static bool IsDictionaryMode(JSHandle<JSObject> receiver);
    static bool IsDictionaryMode(JSObject *receiver);

    static uint32_t GetElementsLength(JSHandle<JSObject> receiver);
    static uint32_t GetElementsLength(JSObject *receiver);

    static JSTaggedValue GetTaggedValueWithElementsKind(JSTaggedType rawValue, ElementsKind kind);
    static JSTaggedType ConvertTaggedValueWithElementsKind(JSTaggedValue rawValue, ElementsKind kind);
private:
};
}  // namespace ecmascript
}  // namespace panda
#endif  // ECMASCRIPT_ELEMENT_ACCESSOR_H

