/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_COMPILER_NEW_OBJECT_STUB_BUILDER_H
#define ECMASCRIPT_COMPILER_NEW_OBJECT_STUB_BUILDER_H

#include "ecmascript/compiler/builtins/builtins_string_stub_builder.h"
#include "ecmascript/compiler/profiler_operation.h"
#include "ecmascript/compiler/stub_builder.h"

namespace panda::ecmascript::kungfu {

struct TraceIdInfo {
    GateRef pc = 0;
    GateRef traceId = 0;
    bool isPc = true;
};

class NewObjectStubBuilder : public StubBuilder {
public:
    explicit NewObjectStubBuilder(StubBuilder *parent)
        : StubBuilder(parent) {}
    explicit NewObjectStubBuilder(Environment *env)
        : StubBuilder(env) {}
    ~NewObjectStubBuilder() override = default;
    NO_MOVE_SEMANTIC(NewObjectStubBuilder);
    NO_COPY_SEMANTIC(NewObjectStubBuilder);
    void GenerateCircuit() override {}

    void SetParameters(GateRef glue, GateRef size)
    {
        glue_ = glue;
        size_ = size;
    }

    void SetGlue(GateRef glue)
    {
        glue_ = glue;
    }

    void NewLexicalEnv(Variable *result, Label *exit, GateRef numSlots, GateRef parent);
    void NewJSObject(Variable *result, Label *exit, GateRef hclass, GateRef size);
    void NewJSObject(Variable *result, Label *exit, GateRef hclass);
    void NewSObject(Variable *result, Label *exit, GateRef hclass);
    GateRef NewJSObject(GateRef glue, GateRef hclass);
    GateRef NewSObject(GateRef glue, GateRef hclass);
    GateRef NewJSFunctionByHClass(GateRef glue, GateRef method, GateRef hclass,
                                  FunctionKind targetKind = FunctionKind::LAST_FUNCTION_KIND);
    GateRef NewSFunctionByHClass(GateRef glue, GateRef method, GateRef hclass,
                                 FunctionKind targetKind = FunctionKind::LAST_FUNCTION_KIND);
    GateRef CloneJSFunction(GateRef glue, GateRef value);
    GateRef CloneProperties(GateRef glue, GateRef currentEnv, GateRef elements, GateRef obj);
    GateRef NewAccessorData(GateRef glue);
    GateRef CloneObjectLiteral(GateRef glue, GateRef literal, GateRef currentEnv);
    GateRef CreateObjectHavingMethod(GateRef glue, GateRef literal, GateRef currentEnv);
    GateRef NewJSProxy(GateRef glue, GateRef target, GateRef handler);
    GateRef NewJSArray(GateRef glue, GateRef hclass);
    GateRef NewTaggedArray(GateRef glue, GateRef len);
    GateRef NewMutantTaggedArray(GateRef glue, GateRef len);
    GateRef CopyArray(GateRef glue, GateRef elements, GateRef oldLen, GateRef newLen);
    GateRef ExtendArray(GateRef glue, GateRef elements, GateRef newLen);
    GateRef NewJSArrayWithSize(GateRef hclass, GateRef size);
    GateRef NewJSForinIterator(GateRef glue, GateRef receiver, GateRef keys, GateRef cachedHclass);
    GateRef LoadHClassFromMethod(GateRef glue, GateRef method);
    GateRef LoadSHClassFromMethod(GateRef glue, GateRef method);
    GateRef NewJSFunction(GateRef glue, GateRef method,
                          FunctionKind targetKind = FunctionKind::LAST_FUNCTION_KIND);
    GateRef NewJSSendableFunction(GateRef glue, GateRef method,
                                  FunctionKind targetKind = FunctionKind::LAST_FUNCTION_KIND);
    void NewJSFunction(GateRef glue, GateRef jsFunc, GateRef index, GateRef length, GateRef lexEnv,
                       Variable *result, Label *success, Label *failed, GateRef slotId,
                       FunctionKind targetKind = FunctionKind::LAST_FUNCTION_KIND);
    void SetProfileTypeInfoCellToFunction(GateRef jsFunc, GateRef definedFunc, GateRef slotId);
    GateRef NewJSBoundFunction(GateRef glue, GateRef target, GateRef boundThis, GateRef args);
    GateRef EnumerateObjectProperties(GateRef glue, GateRef obj);
    void NewArgumentsList(Variable *result, Label *exit, GateRef sp, GateRef startIdx, GateRef numArgs);
    void NewArgumentsObj(Variable *result, Label *exit, GateRef argumentsList, GateRef numArgs);
    void AssignRestArg(Variable *result, Label *exit, GateRef sp, GateRef startIdx, GateRef numArgs,
                       GateRef intialHClass);
    void AllocLineStringObject(Variable *result, Label *exit, GateRef length, bool compressed);
    void AllocSlicedStringObject(Variable *result, Label *exit, GateRef from, GateRef length,
        FlatStringStubBuilder *flatString);
    void AllocTreeStringObject(Variable *result, Label *exit, GateRef first, GateRef second,
        GateRef length, bool compressed);
    void HeapAlloc(Variable *result, Label *exit, RegionSpaceFlag spaceType, GateRef hclass);
    void NewJSArrayLiteral(Variable *result, Label *exit, RegionSpaceFlag spaceType, GateRef obj, GateRef hclass,
                           GateRef trackInfo, bool isEmptyArray);
    GateRef NewTrackInfo(GateRef glue, GateRef cachedHClass, GateRef cachedFunc, RegionSpaceFlag spaceFlag,
                         GateRef arraySize);
    // Note: The size is the num of bytes, it is required to be divisible by 8.
    void InitializeWithSpeicalValue(Label *exit, GateRef object, GateRef value, GateRef start, GateRef end,
                                    MemoryAttribute mAttr = MemoryAttribute::Default());
    GateRef FastNewThisObject(GateRef glue, GateRef ctor);
    GateRef FastSuperAllocateThis(GateRef glue, GateRef superCtor, GateRef newTarget);
    GateRef NewThisObjectChecked(GateRef glue, GateRef ctor);
    GateRef CreateEmptyObject(GateRef glue);
    GateRef CreateEmptyArray(GateRef glue);
    GateRef CreateEmptyArray(GateRef glue, GateRef jsFunc, TraceIdInfo traceIdInfo,
        GateRef profileTypeInfo, GateRef slotId, ProfileOperation callback);
    GateRef CreateArrayWithBuffer(GateRef glue, GateRef index, GateRef jsFunc, TraceIdInfo traceIdInfo,
                                  GateRef profileTypeInfo, GateRef slotId, ProfileOperation callback);
    void NewTaggedArrayChecked(Variable *result, GateRef len, Label *exit);
    void NewMutantTaggedArrayChecked(Variable *result, GateRef len, Label *exit);
    template <typename IteratorType, typename CollectionType>
    void CreateJSCollectionIterator(Variable *result, Label *exit, GateRef set, GateRef kind);
    void CreateJSTypedArrayIterator(Variable *result, Label *exit, GateRef set, GateRef kind);
    GateRef NewTaggedSubArray(GateRef glue, GateRef srcTypedArray, GateRef elementSize, GateRef newLength,
        GateRef beginIndex, GateRef arrayCls, GateRef buffer);
    GateRef NewTypedArray(GateRef glue, GateRef srcTypedArray, GateRef srcType, GateRef length);
    GateRef NewFloat32ArrayObj(GateRef glue, GateRef glueGlobalEnv);
    GateRef NewFloat32ArrayWithSize(GateRef glue, GateRef size);
    GateRef NewTypedArrayFromCtor(GateRef glue, GateRef ctor, GateRef length, Label *slowPath);
    void NewByteArray(Variable *result, Label *exit, GateRef elementSize, GateRef length);
    GateRef NewProfileTypeInfoCell(GateRef glue, GateRef value);
    GateRef GetElementSizeFromType(GateRef glue, GateRef type);
    GateRef GetOnHeapHClassFromType(GateRef glue, GateRef type);
private:
    static constexpr int MAX_TAGGED_ARRAY_LENGTH = 50;
    static constexpr int LOOP_UNROLL_FACTOR = 2;
    GateRef LoadTrackInfo(GateRef glue, GateRef jsFunc, TraceIdInfo traceIdInfo,
        GateRef profileTypeInfo, GateRef slotId, GateRef slotValue, GateRef arrayLiteral, ProfileOperation callback);
    GateRef LoadArrayHClassSlowPath(
        GateRef glue, GateRef jsFunc, TraceIdInfo traceIdInfo, GateRef arrayLiteral, ProfileOperation callback);
    GateRef CreateEmptyArrayCommon(GateRef glue, GateRef hclass, GateRef trackInfo);
    void AllocateInYoungPrologue(Variable *result, Label *callRuntime, Label *exit);
    void AllocateInYoung(Variable *result, Label *exit, GateRef hclass);
    void AllocateInYoung(Variable *result, Label *error, Label *noError, GateRef hclass);
    void AllocateInSOldPrologue(Variable *result, Label *callRuntime, Label *exit);
    void AllocateInSOld(Variable *result, Label *exit, GateRef hclass);
    void InitializeTaggedArrayWithSpeicalValue(Label *exit,
        GateRef array, GateRef value, GateRef start, GateRef length);
    GateRef glue_ {Circuit::NullGate()};
    GateRef size_ {0};
};
}  // namespace panda::ecmascript::kungfu
#endif  // ECMASCRIPT_COMPILER_NEW_OBJECT_STUB_BUILDER_H
