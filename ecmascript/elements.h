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

#ifndef ECMASCRIPT_ELEMENTS_H
#define ECMASCRIPT_ELEMENTS_H

#include "ecmascript/global_env_constants.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/mem/c_containers.h"

namespace panda::ecmascript {

#define ELEMENTS_KIND_INIT_HCLASS_LIST(V)           \
    V(NONE)                                         \
    V(HOLE)                                         \
    V(INT)                                          \
    V(NUMBER)                                       \
    V(STRING)                                       \
    V(OBJECT)                                       \
    V(TAGGED)                                       \
    V(HOLE_INT)                                     \
    V(HOLE_NUMBER)                                  \
    V(HOLE_STRING)                                  \
    V(HOLE_OBJECT)                                  \
    V(HOLE_TAGGED)

enum class ElementsKind : uint8_t {
    NONE = 0x00UL,
    HOLE = 0x01UL,
    INT = 0x1UL << 1,      // 2
    NUMBER = (0x1UL << 2) | INT, // 6
    STRING = 0x1UL << 3,   // 8
    OBJECT = 0x1UL << 4,   // 16
    TAGGED = 0x1EUL,       // 30
    HOLE_INT = HOLE | INT,
    HOLE_NUMBER = HOLE | NUMBER,
    HOLE_STRING = HOLE | STRING,
    HOLE_OBJECT = HOLE | OBJECT,
    HOLE_TAGGED = HOLE | TAGGED,
    GENERIC = HOLE_TAGGED,
    DICTIONARY = HOLE_TAGGED,
};

class PUBLIC_API Elements {
public:
    static CMap<ElementsKind, std::pair<ConstantIndex, ConstantIndex>> InitializeHClassMap();

    static std::string GetString(ElementsKind kind);
    static bool IsInt(ElementsKind kind);
    static bool IsNumber(ElementsKind kind);
    static bool IsTagged(ElementsKind kind);
    static bool IsObject(ElementsKind kind);
    static bool IsHole(ElementsKind kind);
    static bool IsGeneric(ElementsKind kind)
    {
        return kind == ElementsKind::GENERIC;
    }

    static bool IsNone(ElementsKind kind)
    {
        return kind == ElementsKind::NONE;
    }

    static bool IsComplex(ElementsKind kind)
    {
        return IsNumber(kind) || IsTagged(kind);
    }

    static bool IsInNumbers(ElementsKind kind)
    {
        return (static_cast<uint32_t>(kind) > static_cast<uint32_t>(ElementsKind::HOLE) &&
                static_cast<uint32_t>(kind) < static_cast<uint32_t>(ElementsKind::STRING));
    }

    static bool IsHoleInt(ElementsKind kind)
    {
        return kind == ElementsKind::HOLE_INT;
    }

    static bool IsHoleNumber(ElementsKind kind)
    {
        return kind == ElementsKind::HOLE_NUMBER;
    }

    static ConstantIndex GetGlobalContantIndexByKind(ElementsKind kind);
    static ElementsKind MergeElementsKind(ElementsKind curKind, ElementsKind newKind);
    static ElementsKind FixElementsKind(ElementsKind oldKind);
    static ElementsKind ToElementsKind(JSTaggedValue value, ElementsKind kind);
    static void MigrateArrayWithKind(const JSThread *thread, const JSHandle<JSObject> &object,
                                     const ElementsKind oldKind, const ElementsKind newKind);
private:
    static JSTaggedValue MigrateFromRawValueToHeapValue(const JSThread *thread, const JSHandle<JSObject> object,
                                                         bool needCOW, bool isIntKind);
    static void HandleIntKindMigration(const JSThread *thread, const JSHandle<JSObject> &object,
                                       const ElementsKind newKind, bool needCOW);
    static bool IsNumberKind(const ElementsKind kind);
    static bool IsStringOrNoneOrHole(const ElementsKind kind);
    static void HandleNumberKindMigration(const JSThread *thread,
                                          const JSHandle<JSObject> &object,
                                          const ElementsKind newKind, bool needCOW);
    static void HandleOtherKindMigration(const JSThread *thread, const JSHandle<JSObject> &object,
                                         const ElementsKind newKind, bool needCOW);
    static JSTaggedValue MigrateFromHeapValueToRawValue(const JSThread *thread, const JSHandle<JSObject> object,
                                                        bool needCOW, bool isIntKind);
    static void MigrateFromHoleIntToHoleNumber(const JSThread *thread, const JSHandle<JSObject> object);
    static void MigrateFromHoleNumberToHoleInt(const JSThread *thread, const JSHandle<JSObject> object);

};
}  // namespace panda::ecmascript
#endif // ECMASCRIPT_ELEMENTS_H
