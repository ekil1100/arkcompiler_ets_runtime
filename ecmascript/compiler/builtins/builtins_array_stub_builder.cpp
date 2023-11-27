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

#include "ecmascript/compiler/builtins/builtins_array_stub_builder.h"

#include "ecmascript/compiler/builtins/builtins_stubs.h"
#include "ecmascript/compiler/new_object_stub_builder.h"
#include "ecmascript/compiler/profiler_operation.h"
#include "ecmascript/compiler/rt_call_signature.h"
#include "ecmascript/runtime_call_id.h"
#include "ecmascript/js_iterator.h"
#include "ecmascript/compiler/access_object_stub_builder.h"

namespace panda::ecmascript::kungfu {
void BuiltinsArrayStubBuilder::Concat(GateRef glue, GateRef thisValue, GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label isHeapObject(env);
    Branch(TaggedIsHeapObject(thisValue), &isHeapObject, slowPath);
    Bind(&isHeapObject);
    {
        Label isExtensible(env);
        Branch(HasConstructor(thisValue), slowPath, &isExtensible);
        Bind(&isExtensible);
        {
            Label numArgsOne(env);
            Branch(Int64Equal(numArgs, IntPtr(1)), &numArgsOne, slowPath);
            Bind(&numArgsOne);
            {
                GateRef arg0 = GetCallArg0(numArgs);
                Label allEcmaObject(env);
                Label allStableJsArray(env);
                GateRef isThisEcmaObject = IsEcmaObject(thisValue);
                GateRef isArgEcmaObject = IsEcmaObject(arg0);
                Branch(BoolAnd(isThisEcmaObject, isArgEcmaObject), &allEcmaObject, slowPath);
                Bind(&allEcmaObject);
                {
                    GateRef isThisStableJSArray = IsStableJSArray(glue, thisValue);
                    GateRef isArgStableJSArray = IsStableJSArray(glue, arg0);
                    Branch(BoolAnd(isThisStableJSArray, isArgStableJSArray), &allStableJsArray, slowPath);
                    Bind(&allStableJsArray);
                    {
                        GateRef maxArrayIndex = Int64(TaggedArray::MAX_ARRAY_INDEX);
                        GateRef thisLen = ZExtInt32ToInt64(GetArrayLength(thisValue));
                        GateRef argLen = ZExtInt32ToInt64(GetArrayLength(arg0));
                        GateRef sumArrayLen = Int64Add(argLen, thisLen);
                        Label notOverFlow(env);
                        Branch(Int64GreaterThan(sumArrayLen, maxArrayIndex), slowPath, &notOverFlow);
                        Bind(&notOverFlow);
                        {
                            Label spreadable(env);
                            GateRef isSpreadable = IsConcatSpreadable(glue, thisValue);
                            GateRef argisSpreadable = IsConcatSpreadable(glue, arg0);
                            Branch(BoolAnd(isSpreadable, argisSpreadable), &spreadable, slowPath);
                            Bind(&spreadable);
                            {
                                Label setProperties(env);
                                GateRef glueGlobalEnvOffset =
                                    IntPtr(JSThread::GlueData::GetGlueGlobalEnvOffset(env->Is32Bit()));
                                GateRef glueGlobalEnv = Load(VariableType::NATIVE_POINTER(), glue, glueGlobalEnvOffset);
                                auto arrayFunc = GetGlobalEnvValue(VariableType::JS_ANY(), glueGlobalEnv,
                                    GlobalEnv::ARRAY_FUNCTION_INDEX);
                                GateRef intialHClass = Load(VariableType::JS_ANY(), arrayFunc,
                                    IntPtr(JSFunction::PROTO_OR_DYNCLASS_OFFSET));
                                NewObjectStubBuilder newBuilder(this);
                                newBuilder.SetParameters(glue, 0);
                                GateRef newArray = newBuilder.NewJSArrayWithSize(intialHClass, sumArrayLen);
                                Branch(TaggedIsException(newArray), exit, &setProperties);
                                Bind(&setProperties);
                                {
                                    GateRef lengthOffset = IntPtr(JSArray::LENGTH_OFFSET);
                                    Store(VariableType::INT32(), glue, newArray, lengthOffset,
                                        TruncInt64ToInt32(sumArrayLen));
                                    GateRef accessor = GetGlobalConstantValue(VariableType::JS_ANY(), glue,
                                        ConstantIndex::ARRAY_LENGTH_ACCESSOR);
                                    SetPropertyInlinedProps(glue, newArray, intialHClass, accessor,
                                        Int32(JSArray::LENGTH_INLINE_PROPERTY_INDEX));
                                    SetExtensibleToBitfield(glue, newArray, true);
                                    GateRef thisEles = GetElementsArray(thisValue);
                                    GateRef argEles = GetElementsArray(arg0);
                                    GateRef newArrayEles = GetElementsArray(newArray);
                                    DEFVARIABLE(i, VariableType::INT64(), Int64(0));
                                    DEFVARIABLE(j, VariableType::INT64(), Int64(0));
                                    DEFVARIABLE(k, VariableType::INT64(), Int64(0));
                                    Label loopHead(env);
                                    Label loopEnd(env);
                                    Label next(env);
                                    Label loopExit(env);
                                    Jump(&loopHead);
                                    LoopBegin(&loopHead);
                                    {
                                        Branch(Int64LessThan(*i, thisLen), &next, &loopExit);
                                        Bind(&next);
                                        GateRef ele = GetValueFromTaggedArray(thisEles, *i);
                                        SetValueToTaggedArray(VariableType::JS_ANY(), glue, newArrayEles, *j, ele);
                                        Jump(&loopEnd);
                                    }
                                    Bind(&loopEnd);
                                    i = Int64Add(*i, Int64(1));
                                    j = Int64Add(*j, Int64(1));
                                    LoopEnd(&loopHead);
                                    Bind(&loopExit);
                                    Label loopHead1(env);
                                    Label loopEnd1(env);
                                    Label next1(env);
                                    Label loopExit1(env);
                                    Jump(&loopHead1);
                                    LoopBegin(&loopHead1);
                                    {
                                        Branch(Int64LessThan(*k, argLen), &next1, &loopExit1);
                                        Bind(&next1);
                                        GateRef ele = GetValueFromTaggedArray(argEles, *k);
                                        SetValueToTaggedArray(VariableType::JS_ANY(), glue, newArrayEles, *j, ele);
                                        Jump(&loopEnd1);
                                    }
                                    Bind(&loopEnd1);
                                    k = Int64Add(*k, Int64(1));
                                    j = Int64Add(*j, Int64(1));
                                    LoopEnd(&loopHead1);
                                    Bind(&loopExit1);
                                    result->WriteVariable(newArray);
                                    Jump(exit);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void BuiltinsArrayStubBuilder::Filter(GateRef glue, GateRef thisValue, GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label thisIsEmpty(env);
    // Fast path if all the conditions below are satisfied:
    // (1) this is an empty array with constructor not reset (see ArraySpeciesCreate for details);
    // (2) callbackFn is callable (otherwise a TypeError shall be thrown in the slow path)
    JsArrayRequirements req;
    req.defaultConstructor = true;
    Branch(IsJsArrayWithLengthLimit(glue, thisValue, MAX_LENGTH_ZERO, req), &thisIsEmpty, slowPath);
    Bind(&thisIsEmpty);
    {
        Label isCallable(env);
        Label isHeapObject(env);
        Branch(TaggedIsHeapObject(GetCallArg0(numArgs)), &isHeapObject, slowPath);
        Bind(&isHeapObject);
        Branch(IsCallable(GetCallArg0(numArgs)), &isCallable, slowPath);
        // Creates an empty array on fast path
        Bind(&isCallable);
        NewObjectStubBuilder newBuilder(this);
        result->WriteVariable(newBuilder.CreateEmptyArray(glue));
        Jump(exit);
    }
}

// Note: unused arguments are reserved for further development
void BuiltinsArrayStubBuilder::ForEach([[maybe_unused]] GateRef glue, GateRef thisValue, GateRef numArgs,
    [[maybe_unused]] Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label thisIsEmpty(env);
    Label isHeapObject(env);
    // Fast path if all the conditions below are satisfied:
    // (1) this is an empty array with constructor not reset (see ArraySpeciesCreate for details);
    // (2) callbackFn is callable (otherwise a TypeError shall be thrown in the slow path)
    JsArrayRequirements req;
    req.defaultConstructor = true;
    Branch(IsJsArrayWithLengthLimit(glue, thisValue, MAX_LENGTH_ZERO, req), &thisIsEmpty, slowPath);
    Bind(&thisIsEmpty);
    // Do nothing on fast path
    Branch(TaggedIsHeapObject(GetCallArg0(numArgs)), &isHeapObject, slowPath);
    Bind(&isHeapObject);
    Branch(IsCallable(GetCallArg0(numArgs)), exit, slowPath);
}

// Note: unused arguments are reserved for further development
void BuiltinsArrayStubBuilder::IndexOf([[maybe_unused]] GateRef glue, GateRef thisValue, GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label thisIsEmpty(env);
    // Fast path if: (1) this is an empty array; (2) fromIndex is missing
    JsArrayRequirements req;
    Branch(IsJsArrayWithLengthLimit(glue, thisValue, MAX_LENGTH_ZERO, req), &thisIsEmpty, slowPath);
    Bind(&thisIsEmpty);
    {
        Label atMostOneArg(env);
        Branch(Int32LessThanOrEqual(TruncPtrToInt32(numArgs), Int32(1)), &atMostOneArg, slowPath);
        // Returns -1 on fast path
        Bind(&atMostOneArg);
        result->WriteVariable(IntToTaggedPtr(Int32(-1)));
        Jump(exit);
    }
}

// Note: unused arguments are reserved for further development
void BuiltinsArrayStubBuilder::LastIndexOf([[maybe_unused]] GateRef glue, GateRef thisValue, GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label thisIsEmpty(env);
    // Fast path if: (1) this is an empty array; (2) fromIndex is missing
    JsArrayRequirements req;
    Branch(IsJsArrayWithLengthLimit(glue, thisValue, MAX_LENGTH_ZERO, req), &thisIsEmpty, slowPath);
    Bind(&thisIsEmpty);
    {
        Label atMostOneArg(env);
        Branch(Int32LessThanOrEqual(TruncPtrToInt32(numArgs), Int32(1)), &atMostOneArg, slowPath);
        // Returns -1 on fast path
        Bind(&atMostOneArg);
        result->WriteVariable(IntToTaggedPtr(Int32(-1)));
        Jump(exit);
    }
}

void BuiltinsArrayStubBuilder::Slice(GateRef glue, GateRef thisValue, GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label isHeapObject(env);
    Branch(TaggedIsHeapObject(thisValue), &isHeapObject, slowPath);
    Bind(&isHeapObject);
    Label noConstructor(env);
    Branch(HasConstructor(thisValue), slowPath, &noConstructor);
    Bind(&noConstructor);
    Label thisIsEmpty(env);
    Label thisNotEmpty(env);
    // Fast path if:
    // (1) this is an empty array with constructor not reset (see ArraySpeciesCreate for details);
    // (2) no arguments exist
    JsArrayRequirements req;
    req.defaultConstructor = true;
    Branch(IsJsArrayWithLengthLimit(glue, thisValue, MAX_LENGTH_ZERO, req), &thisIsEmpty, &thisNotEmpty);
    Bind(&thisIsEmpty);
    {
        Label noArgs(env);
        GateRef numArgsAsInt32 = TruncPtrToInt32(numArgs);
        Branch(Int32Equal(numArgsAsInt32, Int32(0)), &noArgs, slowPath);
        // Creates a new empty array on fast path
        Bind(&noArgs);
        NewObjectStubBuilder newBuilder(this);
        result->WriteVariable(newBuilder.CreateEmptyArray(glue));
        Jump(exit);
    }
    Bind(&thisNotEmpty);
    {
        Label stableJSArray(env);
        Label arrayLenNotZero(env);

        GateRef isThisStableJSArray = IsStableJSArray(glue, thisValue);
        Branch(isThisStableJSArray, &stableJSArray, slowPath);
        Bind(&stableJSArray);

        GateRef msg0 = GetCallArg0(numArgs);
        GateRef msg1 = GetCallArg1(numArgs);

        GateRef thisArrLen = ZExtInt32ToInt64(GetArrayLength(thisValue));
        Label msg0Int(env);
        Branch(TaggedIsInt(msg0), &msg0Int, slowPath);
        Bind(&msg0Int);
        DEFVARIABLE(start, VariableType::INT64(), Int64(0));
        DEFVARIABLE(end, VariableType::INT64(), thisArrLen);

        GateRef argStart = SExtInt32ToInt64(TaggedGetInt(msg0));
        Label arg0LessZero(env);
        Label arg0NotLessZero(env);
        Label startDone(env);
        Branch(Int64LessThan(argStart, Int64(0)), &arg0LessZero, &arg0NotLessZero);
        Bind(&arg0LessZero);
        {
            Label tempGreaterZero(env);
            Label tempNotGreaterZero(env);
            GateRef tempStart = Int64Add(argStart, thisArrLen);
            Branch(Int64GreaterThan(tempStart, Int64(0)), &tempGreaterZero, &tempNotGreaterZero);
            Bind(&tempGreaterZero);
            {
                start = tempStart;
                Jump(&startDone);
            }
            Bind(&tempNotGreaterZero);
            {
                Jump(&startDone);
            }
        }
        Bind(&arg0NotLessZero);
        {
            Label argLessLen(env);
            Label argNotLessLen(env);
            Branch(Int64LessThan(argStart, thisArrLen), &argLessLen, &argNotLessLen);
            Bind(&argLessLen);
            {
                start = argStart;
                Jump(&startDone);
            }
            Bind(&argNotLessLen);
            {
                start = thisArrLen;
                Jump(&startDone);
            }
        }
        Bind(&startDone);
        Label endDone(env);
        Label msg1Def(env);
        Branch(TaggedIsUndefined(msg1), &endDone, &msg1Def);
        Bind(&msg1Def);
        {
            Label msg1Int(env);
            Branch(TaggedIsInt(msg1), &msg1Int, slowPath);
            Bind(&msg1Int);
            {
                GateRef argEnd = SExtInt32ToInt64(TaggedGetInt(msg1));
                Label arg1LessZero(env);
                Label arg1NotLessZero(env);
                Branch(Int64LessThan(argEnd, Int64(0)), &arg1LessZero, &arg1NotLessZero);
                Bind(&arg1LessZero);
                {
                    Label tempGreaterZero(env);
                    Label tempNotGreaterZero(env);
                    GateRef tempEnd = Int64Add(argEnd, thisArrLen);
                    Branch(Int64GreaterThan(tempEnd, Int64(0)), &tempGreaterZero, &tempNotGreaterZero);
                    Bind(&tempGreaterZero);
                    {
                        end = tempEnd;
                        Jump(&endDone);
                    }
                    Bind(&tempNotGreaterZero);
                    {
                        end = Int64(0);
                        Jump(&endDone);
                    }
                }
                Bind(&arg1NotLessZero);
                {
                    Label argLessLen(env);
                    Label argNotLessLen(env);
                    Branch(Int64LessThan(argEnd, thisArrLen), &argLessLen, &argNotLessLen);
                    Bind(&argLessLen);
                    {
                        end = argEnd;
                        Jump(&endDone);
                    }
                    Bind(&argNotLessLen);
                    {
                        end = thisArrLen;
                        Jump(&endDone);
                    }
                }
            }
        }

        Bind(&endDone);
        DEFVARIABLE(count, VariableType::INT64(), Int64(0));
        GateRef tempCnt = Int64Sub(*end, *start);
        Label tempCntGreaterOrEqualZero(env);
        Label tempCntDone(env);
        Branch(Int64LessThan(tempCnt, Int64(0)), &tempCntDone, &tempCntGreaterOrEqualZero);
        Bind(&tempCntGreaterOrEqualZero);
        {
            count = tempCnt;
            Jump(&tempCntDone);
        }
        Bind(&tempCntDone);
        GateRef newArray = NewArray(glue, *count);
        GateRef thisEles = GetElementsArray(thisValue);
        GateRef thisElesLen = ZExtInt32ToInt64(GetLengthOfTaggedArray(thisEles));
        GateRef newArrayEles = GetElementsArray(newArray);

        Label inThisEles(env);
        Label outThisEles(env);
        Branch(Int64GreaterThan(thisElesLen, Int64Add(*start, *count)), &inThisEles, &outThisEles);
        Bind(&inThisEles);
        {
            DEFVARIABLE(idx, VariableType::INT64(), Int64(0));
            Label loopHead(env);
            Label loopEnd(env);
            Label next(env);
            Label loopExit(env);
            Jump(&loopHead);
            LoopBegin(&loopHead);
            {
                Branch(Int64LessThan(*idx, *count), &next, &loopExit);
                Bind(&next);

                GateRef ele = GetValueFromTaggedArray(thisEles, Int64Add(*idx, *start));
                SetValueToTaggedArray(VariableType::JS_ANY(), glue, newArrayEles, *idx, ele);
                Jump(&loopEnd);
            }
            Bind(&loopEnd);
            idx = Int64Add(*idx, Int64(1));
            LoopEnd(&loopHead);
            Bind(&loopExit);
            result->WriteVariable(newArray);
            Jump(exit);
        }
        Bind(&outThisEles);
        {
            DEFVARIABLE(idx, VariableType::INT64(), Int64(0));
            Label loopHead(env);
            Label loopEnd(env);
            Label next(env);
            Label loopExit(env);
            Jump(&loopHead);
            LoopBegin(&loopHead);
            {
                Branch(Int64LessThan(*idx, *count), &next, &loopExit);
                Bind(&next);
                GateRef index = Int64Add(*idx, *start);
                DEFVARIABLE(ele, VariableType::JS_ANY(), Hole());

                Label indexOutRange(env);
                Label indexInRange(env);
                Label setEle(env);
                Branch(Int64GreaterThan(thisElesLen, index), &indexInRange, &indexOutRange);
                Bind(&indexInRange);
                {
                    ele = GetValueFromTaggedArray(thisEles, index);
                    Jump(&setEle);
                }
                Bind(&indexOutRange);
                {
                    ele = Hole();
                    Jump(&setEle);
                }
                Bind(&setEle);
                SetValueToTaggedArray(VariableType::JS_ANY(), glue, newArrayEles, *idx, *ele);
                Jump(&loopEnd);
            }

            Bind(&loopEnd);
            idx = Int64Add(*idx, Int64(1));
            LoopEnd(&loopHead);
            Bind(&loopExit);
            result->WriteVariable(newArray);
            Jump(exit);
        }
    }
}

// Note: unused arguments are reserved for further development
void BuiltinsArrayStubBuilder::Reverse(GateRef glue, GateRef thisValue, [[maybe_unused]] GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label isJSArray(env);
    GateRef jsCOWArray = IsJsCOWArray(thisValue);
    GateRef jsJSArray = IsJsArray(thisValue);
    Branch(BoolAnd(jsJSArray, BoolNot(jsCOWArray)), &isJSArray, slowPath);
    Bind(&isJSArray);
    Label stableJSArray(env);
    GateRef isThisStableJSArray = IsStableJSArray(glue, thisValue);
    Branch(isThisStableJSArray, &stableJSArray, slowPath);
    Bind(&stableJSArray);

    GateRef thisArrLen = ZExtInt32ToInt64(GetArrayLength(thisValue));
    GateRef elements = GetElementsArray(thisValue);
    DEFVARIABLE(i, VariableType::INT64(), Int64(0));
    DEFVARIABLE(j, VariableType::INT64(),  Int64Sub(thisArrLen, Int64(1)));

    Label loopHead(env);
    Label loopEnd(env);
    Label next(env);
    Label loopExit(env);
    Jump(&loopHead);
    LoopBegin(&loopHead);
    {
        Label arrayValue(env);
        Label valueEqual(env);
        Branch(Int64LessThan(*i, *j), &next, &loopExit);
        Bind(&next);
        {
            GateRef lower = GetValueFromTaggedArray(elements, *i);
            GateRef upper = GetValueFromTaggedArray(elements, *j);

            SetValueToTaggedArray(VariableType::JS_ANY(), glue, elements, *i, upper);
            SetValueToTaggedArray(VariableType::JS_ANY(), glue, elements, *j, lower);
            Jump(&loopEnd);
        }
    }
    Bind(&loopEnd);
    i = Int64Add(*i, Int64(1));
    j = Int64Sub(*j, Int64(1));
    LoopEnd(&loopHead);
    Bind(&loopExit);
    result->WriteVariable(thisValue);
    Jump(exit);
}

GateRef BuiltinsArrayStubBuilder::IsJsArrayWithLengthLimit(GateRef glue, GateRef object,
    uint32_t maxLength, JsArrayRequirements requirements)
{
    auto env = GetEnvironment();
    Label entry(env);
    env->SubCfgEntry(&entry);
    Label isHeapObject(env);
    Label isJsArray(env);
    Label stabilityCheckPassed(env);
    Label defaultConstructorCheckPassed(env);
    Label exit(env);
    DEFVARIABLE(result, VariableType::BOOL(), False());

    Branch(TaggedIsHeapObject(object), &isHeapObject, &exit);
    Bind(&isHeapObject);
    Branch(IsJsArray(object), &isJsArray, &exit);
    Bind(&isJsArray);
    if (requirements.stable) {
        Branch(IsStableJSArray(glue, object), &stabilityCheckPassed, &exit);
    } else {
        Jump(&stabilityCheckPassed);
    }
    Bind(&stabilityCheckPassed);
    if (requirements.defaultConstructor) {
        // If HasConstructor bit is set to 1, then the constructor has been modified.
        Branch(HasConstructor(object), &exit, &defaultConstructorCheckPassed);
    } else {
        Jump(&defaultConstructorCheckPassed);
    }
    Bind(&defaultConstructorCheckPassed);
    result.WriteVariable(Int32UnsignedLessThanOrEqual(GetArrayLength(object), Int32(maxLength)));
    Jump(&exit);
    Bind(&exit);
    GateRef ret = *result;
    env->SubCfgExit();
    return ret;
}

void BuiltinsArrayStubBuilder::Values(GateRef glue, GateRef thisValue,
    [[maybe_unused]] GateRef numArgs, Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label isHeapObject(env);
    Branch(TaggedIsHeapObject(thisValue), &isHeapObject, slowPath);
    Bind(&isHeapObject);
    ConstantIndex iterClassIdx = ConstantIndex::JS_ARRAY_ITERATOR_CLASS_INDEX;
    GateRef iteratorHClass = GetGlobalConstantValue(VariableType::JS_POINTER(), glue, iterClassIdx);
    NewObjectStubBuilder newBuilder(this);
    newBuilder.SetParameters(glue, 0);
    GateRef glueGlobalEnvOffset = IntPtr(JSThread::GlueData::GetGlueGlobalEnvOffset(env->Is32Bit()));
    GateRef glueGlobalEnv = Load(VariableType::NATIVE_POINTER(), glue, glueGlobalEnvOffset);
    GateRef prototype = GetGlobalEnvValue(VariableType::JS_POINTER(), glueGlobalEnv,
                                          GlobalEnv::ARRAY_ITERATOR_PROTOTYPE_INDEX);
    SetPrototypeToHClass(VariableType::JS_POINTER(), glue, iteratorHClass, prototype);
    GateRef iter = newBuilder.NewJSObject(glue, iteratorHClass);
    SetIteratedArrayOfArrayIterator(glue, iter, thisValue);
    SetNextIndexOfArrayIterator(glue, iter, Int32(0));
    GateRef kind = Int32(static_cast<int32_t>(IterationKind::VALUE));
    SetBitFieldOfArrayIterator(glue, iter, kind);
    result->WriteVariable(iter);
    Jump(exit);
}

void BuiltinsArrayStubBuilder::Find(GateRef glue, GateRef thisValue, GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label isHeapObject(env);
    Branch(TaggedIsHeapObject(thisValue), &isHeapObject, slowPath);
    Bind(&isHeapObject);
    Label stableJSArray(env);
    GateRef isThisStableJSArray = IsStableJSArray(glue, thisValue);
    Branch(isThisStableJSArray, &stableJSArray, slowPath);
    Bind(&stableJSArray);

    GateRef callbackFnHandle = GetCallArg0(numArgs);
    Label arg0HeapObject(env);
    Branch(TaggedIsHeapObject(callbackFnHandle), &arg0HeapObject, slowPath);
    Bind(&arg0HeapObject);
    Label callable(env);
    Branch(IsCallable(callbackFnHandle), &callable, slowPath);
    Bind(&callable);
    GateRef argHandle = GetCallArg1(numArgs);
    GateRef thisArrLen = ZExtInt32ToInt64(GetArrayLength(thisValue));

    DEFVARIABLE(i, VariableType::INT64(), Int64(0));
    Label loopHead(env);
    Label loopEnd(env);
    Label next(env);
    Label loopExit(env);
    Jump(&loopHead);
    LoopBegin(&loopHead);
    {
        Branch(Int64LessThan(*i, thisArrLen), &next, &loopExit);
        Bind(&next);
        GateRef kValue = FastGetPropertyByIndex(glue, thisValue, TruncInt64ToInt32(*i), ProfileOperation());
        GateRef key = Int64ToTaggedInt(*i);
        Label hasException(env);
        Label notHasException(env);
        GateRef retValue = JSCallDispatch(glue, callbackFnHandle, Int32(NUM_MANDATORY_JSFUNC_ARGS), 0,
            Circuit::NullGate(), JSCallMode::CALL_THIS_ARG3_WITH_RETURN, { argHandle, kValue, key, thisValue });
        Branch(HasPendingException(glue), &hasException, &notHasException);
        Bind(&hasException);
        {
            result->WriteVariable(retValue);
            Jump(exit);
        }
        Bind(&notHasException);
        {
            Label find(env);
            Branch(TaggedIsTrue(FastToBoolean(retValue)), &find, &loopEnd);
            Bind(&find);
            {
                result->WriteVariable(kValue);
                Jump(exit);
            }
        }
    }
    Bind(&loopEnd);
    i = Int64Add(*i, Int64(1));
    LoopEnd(&loopHead);
    Bind(&loopExit);
    Jump(exit);
}


void BuiltinsArrayStubBuilder::FindIndex(GateRef glue, GateRef thisValue, GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label stableJSArray(env);
    Label notStableJSArray(env);
    GateRef callbackFnHandle = GetCallArg0(numArgs);
    Label isHeapObject(env);
    Branch(TaggedIsHeapObject(callbackFnHandle), &isHeapObject, slowPath);
    Bind(&isHeapObject);
    Label callable(env);
    Branch(IsCallable(callbackFnHandle), &callable, slowPath);
    Bind(&callable);
    result->WriteVariable(IntToTaggedPtr(Int32(-1)));
    GateRef argHandle = GetCallArg1(numArgs);
    GateRef thisArrLen = ZExtInt32ToInt64(GetArrayLength(thisValue));
    GateRef isThisStableJSArray = IsStableJSArray(glue, thisValue);
    Branch(isThisStableJSArray, &stableJSArray, &notStableJSArray);
    Bind(&stableJSArray);
    {
        DEFVARIABLE(i, VariableType::INT64(), Int64(0));
        DEFVARIABLE(kValue, VariableType::JS_ANY(), Undefined());
        Label loopHead(env);
        Label loopEnd(env);
        Label next(env);
        Label loopExit(env);
        Jump(&loopHead);
        LoopBegin(&loopHead);
        {
        Branch(Int64LessThan(*i, thisArrLen), &next, &loopExit);
        Bind(&next);
        DebugPrint(glue, {thisArrLen});
        GateRef thisEles = GetElementsArray(thisValue);
        kValue = GetValueFromTaggedArray(thisEles, *i);
        Label isHole(env);
        Label notHole(env);
        Branch(TaggedIsHole(*kValue), &isHole, &notHole);
        Bind(&isHole);
        {
            GateRef res = FastGetPropertyByIndex(glue, thisValue, TruncInt64ToInt32(*i), ProfileOperation());
            Label resIsHole(env);
            Label resNotHole(env);
            Branch(TaggedIsHole(res), &resIsHole, &resNotHole);
            Bind(&resIsHole);
            kValue = Undefined();
            Jump(&notHole);
            Bind(&resNotHole);
            kValue = res;
            Jump(&notHole);
        }
        Bind(&notHole);
        GateRef key = IntToTaggedPtr(*i);
        Label hasException(env);
        Label notHasException(env);
        Label checkStable(env);
        GateRef retValue = JSCallDispatch(glue, callbackFnHandle, Int32(NUM_MANDATORY_JSFUNC_ARGS), 0,
            Circuit::NullGate(), JSCallMode::CALL_THIS_ARG3_WITH_RETURN, { argHandle, *kValue, key, thisValue });
        Branch(TaggedIsException(retValue), &hasException, &notHasException);
        Bind(&hasException);
        {
            result->WriteVariable(retValue);
            Jump(exit);
        }
        Bind(&notHasException);
        {
            Label find(env);
            Branch(TaggedIsTrue(FastToBoolean(retValue)), &find, &checkStable);
            Bind(&find);
            {
                result->WriteVariable(key);
                Jump(exit);
            }
        }

        Bind(&checkStable);
        i = Int64Add(*i, Int64(1));
        Branch(IsStableJSArray(glue, thisValue), &loopEnd, &notStableJSArray);
        }
        Bind(&loopEnd);
        LoopEnd(&loopHead);
        Bind(&loopExit);
        Jump(exit);
    }
    Bind(&notStableJSArray);
    {
        DebugPrint(glue, {Int64(999)});
        DEFVARIABLE(j, VariableType::INT64(), Int64(0));
        Label loopHead(env);
        Label loopEnd(env);
        Label next(env);
        Label loopExit(env);
        Jump(&loopHead);
        LoopBegin(&loopHead);
        {
            Branch(Int64LessThan(*j, thisArrLen), &next, &loopExit);
            Bind(&next);

            GateRef thisEles = GetElementsArray(thisValue);
            GateRef kValue = GetValueFromTaggedArray(thisEles, *j);
            GateRef key = IntToTaggedPtr(*j);
            Label hasException(env);
            Label notHasException(env);
            GateRef retValue = JSCallDispatch(glue, callbackFnHandle, Int32(NUM_MANDATORY_JSFUNC_ARGS), 0,
                Circuit::NullGate(), JSCallMode::CALL_THIS_ARG3_WITH_RETURN, { argHandle, kValue, key, thisValue });
            Branch(TaggedIsException(retValue), &hasException, &notHasException);
            Bind(&hasException);
            {
                result->WriteVariable(retValue);
                Jump(exit);
            }
            Bind(&notHasException);
            {
                Label find(env);
                Branch(TaggedIsTrue(FastToBoolean(retValue)), &find, &loopEnd);
                Bind(&find);
                {
                    result->WriteVariable(key);
                    Jump(exit);
                }
            }
        }
        Bind(&loopEnd);
        j = Int64Add(*j, Int64(1));
        LoopEnd(&loopHead);
        Bind(&loopExit);
        Jump(exit);
    }
}

void BuiltinsArrayStubBuilder::Push(GateRef glue, GateRef thisValue,
    GateRef numArgs, Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label isHeapObject(env);
    Label isJsArray(env);
    Label isStability(env);
    Label setLength(env);
    Label smallArgs(env);
    Label checkSmallArgs(env);

    Branch(TaggedIsHeapObject(thisValue), &isHeapObject, slowPath);
    Bind(&isHeapObject);
    Branch(IsJsArray(thisValue), &isJsArray, slowPath);
    Bind(&isJsArray);

    Branch(IsStableJSArray(glue, thisValue), &isStability, slowPath);
    Bind(&isStability);

    GateRef oldLength = GetArrayLength(thisValue);
    *result = IntToTaggedPtr(oldLength);

    Branch(Int32Equal(ChangeIntPtrToInt32(numArgs), Int32(0)), exit, &checkSmallArgs);
    Bind(&checkSmallArgs);
    // now unsupport more than 2 args
    Branch(Int32LessThanOrEqual(ChangeIntPtrToInt32(numArgs), Int32(2)), &smallArgs, slowPath);
    Bind(&smallArgs);
    GateRef newLength = Int32Add(oldLength, ChangeIntPtrToInt32(numArgs));

    DEFVARIABLE(elements, VariableType::JS_ANY(), GetElementsArray(thisValue));
    GateRef capacity = GetLengthOfTaggedArray(*elements);
    Label grow(env);
    Label setValue(env);
    Branch(Int32GreaterThan(newLength, capacity), &grow, &setValue);
    Bind(&grow);
    {
        elements =
            CallRuntime(glue, RTSTUB_ID(JSObjectGrowElementsCapacity), { thisValue, IntToTaggedInt(newLength) });
        Jump(&setValue);
    }
    Bind(&setValue);
    {
        Label oneArg(env);
        Label twoArg(env);
        DEFVARIABLE(index, VariableType::INT32(), Int32(0));
        DEFVARIABLE(value, VariableType::JS_ANY(), Undefined());
        Branch(Int64Equal(numArgs, IntPtr(1)), &oneArg, &twoArg);  // 1 one arg
        Bind(&oneArg);
        {
            value = GetCallArg0(numArgs);
            index = Int32Add(oldLength, Int32(0));  // 0 slot index
            SetValueToTaggedArray(VariableType::JS_ANY(), glue, *elements, *index, *value);
            Jump(&setLength);
        }
        Bind(&twoArg);
        {
            value = GetCallArg0(numArgs);
            index = Int32Add(oldLength, Int32(0));  // 0 slot index
            SetValueToTaggedArray(VariableType::JS_ANY(), glue, *elements, *index, *value);

            value = GetCallArg1(numArgs);
            index = Int32Add(oldLength, Int32(1));  // 1 slot index
            SetValueToTaggedArray(VariableType::JS_ANY(), glue, *elements, *index, *value);
            Jump(&setLength);
        }
    }
    Bind(&setLength);
    SetArrayLength(glue, thisValue, newLength);
    result->WriteVariable(IntToTaggedPtr(newLength));
    Jump(exit);
}

GateRef BuiltinsArrayStubBuilder::IsConcatSpreadable(GateRef glue, GateRef obj)
{
    auto env = GetEnvironment();
    Label entry(env);
    env->SubCfgEntry(&entry);
    DEFVARIABLE(result, VariableType::BOOL(), False());
    Label exit(env);
    Label isEcmaObj(env);
    Branch(IsEcmaObject(obj), &isEcmaObj, &exit);
    Bind(&isEcmaObj);
    {
        GateRef glueGlobalEnvOffset = IntPtr(JSThread::GlueData::GetGlueGlobalEnvOffset(env->Is32Bit()));
        GateRef glueGlobalEnv = Load(VariableType::NATIVE_POINTER(), glue, glueGlobalEnvOffset);
        GateRef isConcatsprKey =
            GetGlobalEnvValue(VariableType::JS_ANY(), glueGlobalEnv, GlobalEnv::ISCONCAT_SYMBOL_INDEX);
        AccessObjectStubBuilder builder(this);
        GateRef spreadable =
            builder.LoadObjByValue(glue, obj, isConcatsprKey, Undefined(), Int32(0), ProfileOperation());
        Label isDefined(env);
        Label isUnDefined(env);
        Branch(TaggedIsUndefined(spreadable), &isUnDefined, &isDefined);
        Bind(&isUnDefined);
        {
            Label IsArray(env);
            Branch(IsJsArray(obj), &IsArray, &exit);
            Bind(&IsArray);
            result = True();
            Jump(&exit);
        }
        Bind(&isDefined);
        {
            result = TaggedIsTrue(spreadable);
            Jump(&exit);
        }
    }
    Bind(&exit);
    auto res = *result;
    env->SubCfgExit();
    return res;
}

GateRef BuiltinsArrayStubBuilder::NewArray(GateRef glue, GateRef count)
{
    auto env = GetEnvironment();
    Label entry(env);
    env->SubCfgEntry(&entry);
    DEFVARIABLE(result, VariableType::JS_POINTER(), Undefined());
    Label exit(env);
    Label setProperties(env);
    GateRef glueGlobalEnvOffset = IntPtr(JSThread::GlueData::GetGlueGlobalEnvOffset(env->Is32Bit()));
    GateRef glueGlobalEnv = Load(VariableType::NATIVE_POINTER(), glue, glueGlobalEnvOffset);
    auto arrayFunc = GetGlobalEnvValue(VariableType::JS_ANY(), glueGlobalEnv, GlobalEnv::ARRAY_FUNCTION_INDEX);
    GateRef intialHClass = Load(VariableType::JS_ANY(), arrayFunc, IntPtr(JSFunction::PROTO_OR_DYNCLASS_OFFSET));
    NewObjectStubBuilder newBuilder(this);
    newBuilder.SetParameters(glue, 0);
    result = newBuilder.NewJSArrayWithSize(intialHClass, count);
    Branch(TaggedIsException(*result), &exit, &setProperties);
    Bind(&setProperties);
    {
        GateRef lengthOffset = IntPtr(JSArray::LENGTH_OFFSET);
        Store(VariableType::INT32(), glue, *result, lengthOffset, TruncInt64ToInt32(count));
        GateRef accessor = GetGlobalConstantValue(VariableType::JS_ANY(), glue, ConstantIndex::ARRAY_LENGTH_ACCESSOR);
        SetPropertyInlinedProps(glue, *result, intialHClass, accessor, Int32(JSArray::LENGTH_INLINE_PROPERTY_INDEX));
        SetExtensibleToBitfield(glue, *result, true);
        Jump(&exit);
    }
    Bind(&exit);
    auto res = *result;
    env->SubCfgExit();
    return res;
}

void BuiltinsArrayStubBuilder::Includes(GateRef glue, GateRef thisValue, GateRef numArgs,
    Variable *result, Label *exit, Label *slowPath)
{
    auto env = GetEnvironment();
    Label isHeapObject(env);
    Label isJsArray(env);
    Label notFound(env);
    Label thisLenNotZero(env);
    Branch(TaggedIsHeapObject(thisValue), &isHeapObject, slowPath);
    Bind(&isHeapObject);
    Branch(IsJsArray(thisValue), &isJsArray, slowPath);
    Bind(&isJsArray);
    GateRef thisLen = GetArrayLength(thisValue);
    Branch(Int32Equal(thisLen, Int32(0)), &notFound, &thisLenNotZero);
    Bind(&thisLenNotZero);
    {
        DEFVARIABLE(fromIndex, VariableType::INT32(), Int32(0));
        Label getArgTwo(env);
        Label nextProcess(env);
        Branch(Int64Equal(numArgs, IntPtr(2)), &getArgTwo, &nextProcess); // 2: 2 parameters
        Bind(&getArgTwo);
        {
            Label secondArgIsInt(env);
            GateRef fromIndexTemp = GetCallArg1(numArgs);
            Branch(TaggedIsInt(fromIndexTemp), &secondArgIsInt, slowPath);
            Bind(&secondArgIsInt);
            fromIndex = GetInt32OfTInt(fromIndexTemp);
            Jump(&nextProcess);
        }
        Bind(&nextProcess);
        {
            Label atLeastOneArg(env);
            Label setBackZero(env);
            Label calculateFrom(env);
            Label nextCheck(env);
            Branch(Int64GreaterThanOrEqual(numArgs, IntPtr(1)), &atLeastOneArg, slowPath);
            Bind(&atLeastOneArg);
            Branch(Int32GreaterThanOrEqual(*fromIndex, thisLen), &notFound, &nextCheck);
            Bind(&nextCheck);
            {
                GateRef negThisLen = Int32Sub(Int32(0), thisLen);
                Branch(Int32LessThan(*fromIndex, negThisLen), &setBackZero, &calculateFrom);
                Bind(&setBackZero);
                {
                    fromIndex = Int32(0);
                    Jump(&calculateFrom);
                }
                Bind(&calculateFrom);
                {
                    DEFVARIABLE(from, VariableType::INT32(), Int32(0));
                    Label fromIndexGreaterOrEqualZero(env);
                    Label fromIndexLessThanZero(env);
                    Label startLoop(env);
                    Branch(Int32GreaterThanOrEqual(*fromIndex, Int32(0)),
                        &fromIndexGreaterOrEqualZero, &fromIndexLessThanZero);
                    Bind(&fromIndexGreaterOrEqualZero);
                    {
                        from = *fromIndex;
                        Jump(&startLoop);
                    }
                    Bind(&fromIndexLessThanZero);
                    {
                        Label isLenFromIndex(env);
                        GateRef lenFromIndexSum = Int32Add(thisLen, *fromIndex);
                        Branch(Int32GreaterThanOrEqual(lenFromIndexSum, Int32(0)), &isLenFromIndex, &startLoop);
                        Bind(&isLenFromIndex);
                        {
                            from = lenFromIndexSum;
                            Jump(&startLoop);
                        }
                    }
                    Bind(&startLoop);
                    {
                        GateRef searchElement = GetCallArg0(numArgs);
                        GateRef elements = GetElementsArray(thisValue);
                        Label loopHead(env);
                        Label loopEnd(env);
                        Label next(env);
                        Label loopExit(env);
                        Jump(&loopHead);
                        LoopBegin(&loopHead);
                        {
                            Branch(Int32LessThan(*from, thisLen), &next, &loopExit);
                            Bind(&next);
                            {
                                Label notHoleOrUndefValue(env);
                                Label valueFound(env);
                                GateRef value = GetValueFromTaggedArray(elements, *from);
                                GateRef isHole = TaggedIsHole(value);
                                GateRef isUndef = TaggedIsUndefined(value);
                                Branch(BoolOr(isHole, isUndef), slowPath, &notHoleOrUndefValue);
                                Bind(&notHoleOrUndefValue);
                                GateRef valueEqual = StubBuilder::SameValueZero(glue, searchElement, value);
                                Branch(valueEqual, &valueFound, &loopEnd);
                                Bind(&valueFound);
                                {
                                    result->WriteVariable(TaggedTrue());
                                    Jump(exit);
                                }
                            }
                        }
                        Bind(&loopEnd);
                        from = Int32Add(*from, Int32(1));
                        LoopEnd(&loopHead);
                        Bind(&loopExit);
                        Jump(&notFound);
                    }
                }
            }
        }
    }
    Bind(&notFound);
    {
        result->WriteVariable(TaggedFalse());
        Jump(exit);
    }
}
}  // namespace panda::ecmascript::kungfu
