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
#include "ecmascript/compiler/mcr_lowering.h"
#include "ecmascript/compiler/bytecodes.h"
#include "ecmascript/compiler/share_gate_meta_data.h"
#include "ecmascript/global_env.h"
#include "ecmascript/jspandafile/program_object.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/js_function.h"
#include "ecmascript/message_string.h"

namespace panda::ecmascript::kungfu {

GateRef MCRLowering::VisitGate(GateRef gate)
{
    auto op = acc_.GetOpCode(gate);
    switch (op) {
        case OpCode::GET_CONSTPOOL:
            LowerGetConstPool(gate);
            break;
        case OpCode::STATE_SPLIT:
            DeleteStateSplit(gate);
            break;
        case OpCode::ARRAY_GUARDIAN_CHECK:
            LowerArrayGuardianCheck(gate);
            break;
        case OpCode::HCLASS_STABLE_ARRAY_CHECK:
            LowerHClassStableArrayCheck(gate);
            break;
        case OpCode::HEAP_OBJECT_CHECK:
            LowerHeapObjectCheck(gate);
            break;
        case OpCode::LOAD_CONST_OFFSET:
            LowerLoadConstOffset(gate);
            break;
        case OpCode::LOAD_HCLASS_FROM_CONSTPOOL:
            LowerLoadHClassFromConstpool(gate);
            break;
        case OpCode::STORE_CONST_OFFSET:
            LowerStoreConstOffset(gate);
            break;
        case OpCode::CONVERT_HOLE_AS_UNDEFINED:
            LowerConvertHoleAsUndefined(gate);
            break;
        case OpCode::GET_GLOBAL_ENV:
            LowerGetGlobalEnv(gate);
            break;
        case OpCode::GET_GLOBAL_ENV_OBJ:
            LowerGetGlobalEnvObj(gate);
            break;
        case OpCode::GET_GLOBAL_ENV_OBJ_HCLASS:
            LowerGetGlobalEnvObjHClass(gate);
            break;
        case OpCode::GET_GLOBAL_CONSTANT_VALUE:
            LowerGetGlobalConstantValue(gate);
            break;
        case OpCode::HEAP_ALLOC:
            LowerHeapAllocate(gate);
            break;
        case OpCode::INT32_CHECK_RIGHT_IS_ZERO:
            LowerInt32CheckRightIsZero(gate);
            break;
        case OpCode::REMAINDER_IS_NEGATIVE_ZERO:
            LowerRemainderIsNegativeZero(gate);
            break;
        case OpCode::FLOAT64_CHECK_RIGHT_IS_ZERO:
            LowerFloat64CheckRightIsZero(gate);
            break;
        case OpCode::VALUE_CHECK_NEG_OVERFLOW:
            LowerValueCheckNegOverflow(gate);
            break;
        case OpCode::OVERFLOW_CHECK:
            LowerOverflowCheck(gate);
            break;
        case OpCode::INT32_UNSIGNED_UPPER_BOUND_CHECK:
            LowerInt32UnsignedUpperBoundCheck(gate);
            break;
        case OpCode::INT32_DIV_WITH_CHECK:
            LowerInt32DivWithCheck(gate);
            break;
        case OpCode::LEX_VAR_IS_HOLE_CHECK:
            LowerLexVarIsHoleCheck(gate);
            break;
        case OpCode::STORE_MEMORY:
            LowerStoreMemory(gate);
            break;
        case OpCode::CHECK_AND_CONVERT:
            LowerCheckAndConvert(gate);
            break;
        case OpCode::TAGGED_IS_HEAP_OBJECT:
            LowerTaggedIsHeapObject(gate);
            break;
        case OpCode::IS_MARKER_CELL_VALID:
            LowerIsMarkerCellValid(gate);
            break;
        case OpCode::IS_SPECIFIC_OBJECT_TYPE:
            LowerIsSpecificObjectType(gate);
            break;
        default:
            break;
    }
    return Circuit::NullGate();
}

void MCRLowering::LowerConvertHoleAsUndefined(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);

    Label returnUndefined(&builder_);
    Label exit(&builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    DEFVALUE(result, (&builder_), VariableType::JS_ANY(), receiver);

    builder_.Branch(builder_.TaggedIsHole(*result), &returnUndefined, &exit, 1, BranchWeight::DEOPT_WEIGHT);
    builder_.Bind(&returnUndefined);
    {
        result = builder_.UndefineConstant();
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), *result);
}

void MCRLowering::LowerLoadConstOffset(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef offset = builder_.IntPtr(acc_.GetOffset(gate));
    VariableType type = VariableType(acc_.GetMachineType(gate), acc_.GetGateType(gate));
    GateRef result = builder_.Load(type, receiver, offset, acc_.GetMemoryOrder(gate));
    acc_.ReplaceGate(gate, Circuit::NullGate(), builder_.GetDepend(), result);
}

void MCRLowering::LowerLoadHClassFromConstpool(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef constpool = acc_.GetValueIn(gate, 0);
    uint32_t index = acc_.GetIndex(gate);
    GateRef constPoolSize = builder_.GetLengthOfTaggedArray(constpool);
    GateRef valVecIndex = builder_.Int32Sub(constPoolSize, builder_.Int32(ConstantPool::AOT_HCLASS_INFO_INDEX));
    GateRef valVec = builder_.GetValueFromTaggedArray(constpool, valVecIndex);
    GateRef hclass = builder_.GetValueFromTaggedArray(valVec, builder_.Int32(index));
    acc_.ReplaceGate(gate, Circuit::NullGate(), builder_.GetDepend(), hclass);
}

void MCRLowering::LowerStoreConstOffset(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);

    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef value = acc_.GetValueIn(gate, 1);
    GateRef offset = builder_.IntPtr(acc_.GetOffset(gate));
    VariableType type = VariableType(acc_.GetMachineType(gate), acc_.GetGateType(gate));
    builder_.Store(type, glue_, receiver, offset, value, acc_.GetMemoryOrder(gate));
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerHeapObjectCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef receiver = acc_.GetValueIn(gate, 0);

    GateRef heapObjectCheck = builder_.TaggedIsHeapObject(receiver);
    builder_.DeoptCheck(heapObjectCheck, frameState, DeoptType::NOTHEAPOBJECT1);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerTaggedIsHeapObject(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef result = builder_.TaggedIsHeapObject(receiver);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerIsMarkerCellValid(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef cell = acc_.GetValueIn(gate, 0);
    GateRef result = builder_.IsMarkerCellValid(cell);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerIsSpecificObjectType(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    JSType expectType = static_cast<JSType>(acc_.GetJSType(gate));
    GateRef obj = acc_.GetValueIn(gate, 0);
    GateRef result;
    switch (expectType) {
        case JSType::JS_MAP: {
            result = builder_.TaggedObjectIsJSMap(obj);
            break;
        }
        case JSType::JS_SET: {
            result = builder_.TaggedObjectIsJSSet(obj);
            break;
        }
        case JSType::JS_ARRAY: {
            result = builder_.TaggedObjectIsJSArray(obj);
            break;
        }
        case JSType::STRING_FIRST: {
            result = builder_.TaggedObjectIsString(obj);
            break;
        }
        case JSType::JS_TYPED_ARRAY_FIRST: {
            result = builder_.TaggedObjectIsTypedArray(obj);
            break;
        }
        default: {
            LOG_COMPILER(FATAL) << "this branch is unreachable";
            UNREACHABLE();
        }
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerGetConstPool(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef jsFunc = acc_.GetValueIn(gate, 0); // 0: this object
    GateRef newGate = builder_.GetConstPoolFromFunction(jsFunc);

    acc_.UpdateAllUses(gate, newGate);

    // delete old gate
    acc_.DeleteGate(gate);
}

void MCRLowering::DeleteStateSplit(GateRef gate)
{
    auto depend = acc_.GetDep(gate);
    auto frameState = acc_.GetFrameState(gate);
    acc_.DeleteGateIfNoUse(frameState);
    acc_.ReplaceGate(gate, Circuit::NullGate(), depend, Circuit::NullGate());
}

void MCRLowering::LowerArrayGuardianCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);

    GateRef frameState = acc_.GetFrameState(gate);
    GateRef guardiansOffset = builder_.IntPtr(JSThread::GlueData::GetStableArrayElementsGuardiansOffset(false));
    GateRef check = builder_.Load(VariableType::BOOL(), glue_, guardiansOffset);
    builder_.DeoptCheck(check, frameState, DeoptType::NOTSARRAY1);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerHClassStableArrayCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef hclass = acc_.GetValueIn(gate, 0);

    GateRef check = Circuit::NullGate();
    GateRef stableCheck = builder_.IsStableElements(hclass);
    ArrayMetaDataAccessor accessor = acc_.GetArrayMetaDataAccessor(gate);
    ElementsKind kind = accessor.GetElementsKind();
    if (accessor.IsLoadElement() && !Elements::IsHole(kind)) {
        if (Elements::IsComplex(kind)) {
            GateRef elementsKindCheck = builder_.Int32GreaterThanOrEqual(builder_.Int32(static_cast<int32_t>(kind)),
                                                                         builder_.GetElementsKindByHClass(hclass));
            check = builder_.BoolAnd(stableCheck, elementsKindCheck);
        } else {
            GateRef elementsKindCheck = builder_.Equal(builder_.Int32(static_cast<int32_t>(kind)),
                                                       builder_.GetElementsKindByHClass(hclass));
            check = builder_.BoolAnd(stableCheck, elementsKindCheck);
        }
    } else {
        check = stableCheck;
    }
    builder_.DeoptCheck(check, frameState, DeoptType::NOTSARRAY2);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

StateDepend MCRLowering::LowerConvert(StateDepend stateDepend, GateRef gate)
{
    Environment env(stateDepend.State(), stateDepend.Depend(), {}, circuit_, &builder_);
    GateRef value = acc_.GetValueIn(gate);
    ValueType dstType = acc_.GetDstType(gate);
    GateRef result = Circuit::NullGate();
    Label exit(&builder_);
    switch (acc_.GetSrcType(gate)) {
        case ValueType::BOOL:
            ASSERT(dstType == ValueType::TAGGED_BOOLEAN);
            result = ConvertBoolToTaggedBoolean(value);
            break;
        case ValueType::INT32:
            if (dstType == ValueType::TAGGED_INT) {
                result = ConvertInt32ToTaggedInt(value);
            } else if (dstType == ValueType::FLOAT64) {
                result = ConvertInt32ToFloat64(value);
            } else {
                ASSERT(dstType == ValueType::BOOL);
                result = builder_.NotEqual(value, builder_.Int32(0));
            }
            break;
        case ValueType::UINT32:
            if (dstType == ValueType::TAGGED_NUMBER) {
                result = ConvertUInt32ToTaggedNumber(value, &exit);
            } else if (dstType == ValueType::FLOAT64) {
                result = ConvertUInt32ToFloat64(value);
            } else {
                ASSERT(dstType == ValueType::BOOL);
                result = builder_.NotEqual(value, builder_.Int32(0));
            }
            break;
        case ValueType::FLOAT64:
            if (dstType == ValueType::TAGGED_DOUBLE) {
                result = ConvertFloat64ToTaggedDouble(value);
            } else if (dstType == ValueType::INT32) {
                result = ConvertFloat64ToInt32(value, &exit);
            } else {
                ASSERT(dstType == ValueType::BOOL);
                result = ConvertFloat64ToBool(value);
            }
            break;
        case ValueType::TAGGED_BOOLEAN:
            ASSERT((dstType == ValueType::BOOL));
            result = ConvertTaggedBooleanToBool(value);
            break;
        case ValueType::TAGGED_INT:
            ASSERT((dstType == ValueType::INT32));
            result = ConvertTaggedIntToInt32(value);
            break;
        case ValueType::TAGGED_DOUBLE:
            ASSERT((dstType == ValueType::FLOAT64));
            result = ConvertTaggedDoubleToFloat64(value);
            break;
        case ValueType::CHAR: {
            ASSERT((dstType == ValueType::ECMA_STRING));
            GateRef glue = acc_.GetGlueFromArgList();
            result = builder_.CallStub(glue, gate, CommonStubCSigns::CreateStringBySingleCharCode, { glue, value });
            break;
        }
        default:
            LOG_COMPILER(FATAL) << "this branch is unreachable";
            break;
    }
    acc_.ReplaceGate(gate, Circuit::NullGate(), Circuit::NullGate(), result);
    return builder_.GetStateDepend();
}

GateRef MCRLowering::ConvertTaggedNumberToBool(GateRef gate, Label *exit)
{
    DEFVALUE(result, (&builder_), VariableType::BOOL(), builder_.Boolean(false));
    Label isInt(&builder_);
    Label isDouble(&builder_);
    Label toInt32(&builder_);
    builder_.Branch(builder_.TaggedIsInt(gate), &isInt, &isDouble);
    builder_.Bind(&isInt);
    {
        GateRef intVal = builder_.GetInt64OfTInt(gate);
        result = builder_.NotEqual(intVal, builder_.Int64(0));
    }
    builder_.Jump(exit);
    builder_.Bind(&isDouble);
    {
        GateRef doubleVal = builder_.GetDoubleOfTDouble(gate);
        result = ConvertFloat64ToBool(doubleVal);
    }
    builder_.Jump(exit);
    builder_.Bind(exit);
    return *result;
}

GateRef MCRLowering::ConvertTaggedNumberToInt32(GateRef gate, Label *exit)
{
    DEFVALUE(result, (&builder_), VariableType::INT32(), builder_.Int32(0));
    Label isInt(&builder_);
    Label isDouble(&builder_);
    Label toInt32(&builder_);
    builder_.Branch(builder_.TaggedIsInt(gate), &isInt, &isDouble);
    builder_.Bind(&isInt);
    result = ConvertTaggedIntToInt32(gate);
    builder_.Jump(exit);
    builder_.Bind(&isDouble);
    result = ConvertFloat64ToInt32(ConvertTaggedDoubleToFloat64(gate), &toInt32);
    builder_.Jump(exit);
    builder_.Bind(exit);
    return *result;
}

GateRef MCRLowering::ConvertTaggedNumberToFloat64(GateRef gate, Label *exit)
{
    DEFVALUE(result, (&builder_), VariableType::FLOAT64(), builder_.Double(0));
    Label isInt(&builder_);
    Label isDouble(&builder_);
    builder_.Branch(builder_.TaggedIsInt(gate), &isInt, &isDouble);
    builder_.Bind(&isInt);
    result = ConvertInt32ToFloat64(ConvertTaggedIntToInt32(gate));
    builder_.Jump(exit);
    builder_.Bind(&isDouble);
    result = ConvertTaggedDoubleToFloat64(gate);
    builder_.Jump(exit);
    builder_.Bind(exit);
    return *result;
}

void MCRLowering::LowerCheckAndConvert(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    ValueType srcType = acc_.GetSrcType(gate);
    Label exit(&builder_);
    switch (srcType) {
        case ValueType::UINT32:
            LowerCheckUInt32AndConvert(gate, frameState);
            break;
        case ValueType::TAGGED_INT:
            LowerCheckTaggedIntAndConvert(gate, frameState);
            break;
        case ValueType::TAGGED_DOUBLE:
            LowerCheckTaggedDoubleAndConvert(gate, frameState, &exit);
            break;
        case ValueType::TAGGED_BOOLEAN:
            LowerCheckTaggedBoolAndConvert(gate, frameState);
            break;
        case ValueType::TAGGED_NUMBER:
            LowerCheckTaggedNumberAndConvert(gate, frameState, &exit);
            break;
        case ValueType::BOOL:
            LowerCheckSupportAndConvert(gate, frameState);
            break;
        case ValueType::TAGGED_NULL:
            LowerCheckNullAndConvert(gate, frameState);
            break;
        case ValueType::UNDEFINED:
            LowerUndefinedAndConvert(gate, frameState);
            break;
        default:
            UNREACHABLE();
    }
}

void MCRLowering::LowerCheckUInt32AndConvert(GateRef gate, GateRef frameState)
{
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef upperBound = builder_.Int32(INT32_MAX);
    GateRef check = builder_.Int32UnsignedLessThanOrEqual(value, upperBound);
    builder_.DeoptCheck(check, frameState, DeoptType::INT32OVERFLOW1);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), value);
}

void MCRLowering::LowerCheckTaggedIntAndConvert(GateRef gate, GateRef frameState)
{
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsInt(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTINT1);
    GateRef result = Circuit::NullGate();
    ValueType dst = acc_.GetDstType(gate);
    ASSERT(dst == ValueType::INT32 || dst == ValueType::FLOAT64);
    if (dst == ValueType::INT32) {
        result = ConvertTaggedIntToInt32(value);
    } else {
        result = ConvertTaggedIntToFloat64(value);
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerCheckTaggedDoubleAndConvert(GateRef gate, GateRef frameState, Label *exit)
{
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsDouble(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTDOUBLE1);
    GateRef result = Circuit::NullGate();
    ValueType dst = acc_.GetDstType(gate);
    ASSERT(dst == ValueType::INT32 || dst == ValueType::FLOAT64);
    if (dst == ValueType::INT32) {
        result = ConvertTaggedDoubleToInt32(value, exit);
    } else {
        builder_.CallRuntime(glue_, RTSTUB_ID(DebugAOTPrint), acc_.GetDep(gate), {value, builder_.Int64(0)}, gate);
        GateRef tagged = builder_.ChangeTaggedPointerToInt64(value);
        builder_.CallRuntime(glue_, RTSTUB_ID(DebugAOTPrint), acc_.GetDep(gate), {tagged, builder_.Int64(1)}, gate);
        GateRef val = builder_.Int64Sub(tagged, builder_.Int64(JSTaggedValue::DOUBLE_ENCODE_OFFSET));
        builder_.CallRuntime(glue_, RTSTUB_ID(DebugAOTPrint), acc_.GetDep(gate), {value, builder_.Int64(2)}, gate);
        result = builder_.CastInt64ToFloat64(val);
        builder_.CallRuntime(glue_, RTSTUB_ID(DebugAOTPrint), acc_.GetDep(gate), {result, builder_.Int64(3)}, gate);
        // result = ConvertTaggedDoubleToFloat64(value);
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerCheckTaggedNumberAndConvert(GateRef gate, GateRef frameState, Label *exit)
{
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsNumber(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTNUMBER1);
    GateRef result = Circuit::NullGate();
    ValueType dst = acc_.GetDstType(gate);
    if (dst == ValueType::INT32) {
        result = ConvertTaggedNumberToInt32(value, exit);
    } else if (dst == ValueType::FLOAT64) {
        result = ConvertTaggedNumberToFloat64(value, exit);
    } else {
        ASSERT(dst == ValueType::BOOL);
        result = ConvertTaggedNumberToBool(value, exit);
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerCheckSupportAndConvert(GateRef gate, GateRef frameState)
{
    ValueType dstType = acc_.GetDstType(gate);
    ASSERT(dstType == ValueType::INT32 || dstType == ValueType::FLOAT64);
    bool support = acc_.IsConvertSupport(gate);
    GateRef value = acc_.GetValueIn(gate, 0);

    GateRef result = Circuit::NullGate();
    if (dstType == ValueType::INT32) {
        builder_.DeoptCheck(builder_.Boolean(support), frameState, DeoptType::NOTINT2);
        result = builder_.BooleanToInt32(value);
    } else {
        builder_.DeoptCheck(builder_.Boolean(support), frameState, DeoptType::NOTDOUBLE2);
        result = builder_.BooleanToFloat64(value);
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerCheckTaggedBoolAndConvert(GateRef gate, GateRef frameState)
{
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsBoolean(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTBOOL1);
    GateRef result = Circuit::NullGate();
    GateRef boolValue = ConvertTaggedBooleanToBool(value);
    if (acc_.GetDstType(gate) == ValueType::BOOL) {
        result = boolValue;
    } else if (acc_.GetDstType(gate) == ValueType::INT32) {
        result = builder_.ZExtInt1ToInt32(boolValue);
    } else if (acc_.GetDstType(gate) == ValueType::FLOAT64) {
        result = builder_.BooleanToFloat64(boolValue);
    } else {
        UNREACHABLE();
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerCheckNullAndConvert(GateRef gate, GateRef frameState)
{
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsNull(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTNULL1);
    GateRef result = Circuit::NullGate();
    if (acc_.GetDstType(gate) == ValueType::INT32) {
        result = builder_.Int32(0);
    } else if (acc_.GetDstType(gate) == ValueType::FLOAT64) {
        result = builder_.Double(0);
    } else if (acc_.GetDstType(gate) == ValueType::BOOL) {
        result = builder_.False();
    } else {
        UNREACHABLE();
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerUndefinedAndConvert(GateRef gate, GateRef frameState)
{
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsUndefined(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTNULL2);
    GateRef result = Circuit::NullGate();
    if (acc_.GetDstType(gate) == ValueType::FLOAT64) {
        result = builder_.NanValue();
    } else if (acc_.GetDstType(gate) == ValueType::BOOL) {
        result = builder_.False();
    } else if (acc_.GetDstType(gate) == ValueType::INT32) {
        result = builder_.Int32(0);
    } else {
        UNREACHABLE();
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

GateRef MCRLowering::ConvertTaggedBooleanToBool(GateRef value)
{
    return builder_.TaggedIsTrue(value);
}

GateRef MCRLowering::ConvertBoolToTaggedBoolean(GateRef gate)
{
    return builder_.BooleanToTaggedBooleanPtr(gate);
}

GateRef MCRLowering::ConvertInt32ToFloat64(GateRef gate)
{
    return builder_.ChangeInt32ToFloat64(gate);
}

GateRef MCRLowering::ConvertUInt32ToFloat64(GateRef gate)
{
    return builder_.ChangeUInt32ToFloat64(gate);
}

GateRef MCRLowering::ConvertInt32ToTaggedInt(GateRef gate)
{
    return builder_.Int32ToTaggedPtr(gate);
}

GateRef MCRLowering::ConvertUInt32ToTaggedNumber(GateRef gate, Label *exit)
{
    Label isOverFlow(&builder_);
    Label notOverFlow(&builder_);
    GateRef upperBound = builder_.Int32(INT32_MAX);
    DEFVALUE(taggedVal, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    builder_.Branch(builder_.Int32UnsignedLessThanOrEqual(gate, upperBound), &notOverFlow, &isOverFlow);
    builder_.Bind(&notOverFlow);
    taggedVal = builder_.Int32ToTaggedPtr(gate);
    builder_.Jump(exit);
    builder_.Bind(&isOverFlow);
    taggedVal = builder_.DoubleToTaggedDoublePtr(builder_.ChangeUInt32ToFloat64(gate));
    builder_.Jump(exit);
    builder_.Bind(exit);
    return *taggedVal;
}

GateRef MCRLowering::ConvertFloat64ToInt32(GateRef gate, Label *exit)
{
    return builder_.DoubleToInt(gate, exit);
}

GateRef MCRLowering::ConvertFloat64ToBool(GateRef gate)
{
    GateRef doubleNotZero = builder_.DoubleNotEqual(gate, builder_.Double(0.0));
    GateRef doubleNotNAN = builder_.BoolNot(builder_.DoubleIsNAN(gate));
    return builder_.BoolAnd(doubleNotZero, doubleNotNAN);
}

GateRef MCRLowering::ConvertFloat64ToTaggedDouble(GateRef gate)
{
    return builder_.DoubleToTaggedDoublePtr(gate);
}

GateRef MCRLowering::ConvertTaggedIntToInt32(GateRef gate)
{
    return builder_.GetInt32OfTInt(gate);
}

GateRef MCRLowering::ConvertTaggedIntToFloat64(GateRef gate)
{
    return builder_.ChangeInt32ToFloat64(builder_.GetInt32OfTInt(gate));
}

GateRef MCRLowering::ConvertTaggedDoubleToInt32(GateRef gate, Label *exit)
{
    return builder_.DoubleToInt(builder_.GetDoubleOfTDouble(gate), exit);
}

GateRef MCRLowering::ConvertTaggedDoubleToFloat64(GateRef gate)
{
    return builder_.GetDoubleOfTDouble(gate);
}

void MCRLowering::LowerGetGlobalEnv(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef glueGlobalEnvOffset = builder_.IntPtr(JSThread::GlueData::GetGlueGlobalEnvOffset(false));
    GateRef glueGlobalEnv = builder_.Load(VariableType::JS_POINTER(), glue_, glueGlobalEnvOffset);
    acc_.ReplaceGate(gate, Circuit::NullGate(), builder_.GetDepend(), glueGlobalEnv);
}

void MCRLowering::LowerGetGlobalEnvObj(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef globalEnv = acc_.GetValueIn(gate, 0);
    size_t index = acc_.GetIndex(gate);
    GateRef offset = builder_.IntPtr(GlobalEnv::HEADER_SIZE + JSTaggedValue::TaggedTypeSize() * index);
    GateRef object = builder_.Load(VariableType::JS_ANY(), globalEnv, offset);
    acc_.ReplaceGate(gate, Circuit::NullGate(), builder_.GetDepend(), object);
}

void MCRLowering::LowerGetGlobalEnvObjHClass(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef globalEnv = acc_.GetValueIn(gate, 0);
    size_t index = acc_.GetIndex(gate);
    GateRef offset = builder_.IntPtr(GlobalEnv::HEADER_SIZE + JSTaggedValue::TaggedTypeSize() * index);
    GateRef object = builder_.Load(VariableType::JS_ANY(), globalEnv, offset);
    auto hclass = builder_.Load(VariableType::JS_POINTER(), object,
                                builder_.IntPtr(JSFunction::PROTO_OR_DYNCLASS_OFFSET));
    acc_.ReplaceGate(gate, Circuit::NullGate(), builder_.GetDepend(), hclass);
}

void MCRLowering::LowerGetGlobalConstantValue(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    size_t index = acc_.GetIndex(gate);
    GateRef gConstAddr = builder_.Load(VariableType::JS_POINTER(), glue_,
        builder_.IntPtr(JSThread::GlueData::GetGlobalConstOffset(false)));
    GateRef constantIndex = builder_.IntPtr(JSTaggedValue::TaggedTypeSize() * index);
    GateRef result = builder_.Load(VariableType::JS_POINTER(), gConstAddr, constantIndex);
    acc_.ReplaceGate(gate, Circuit::NullGate(), builder_.GetDepend(), result);
}

void MCRLowering::LowerHeapAllocate(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto flag = acc_.TryGetValue(gate);
    switch (flag) {
        case RegionSpaceFlag::IN_YOUNG_SPACE:
            HeapAllocateInYoung(gate);
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
}

void MCRLowering::HeapAllocateInYoung(GateRef gate)
{
    Label exit(&builder_);
    GateRef size = acc_.GetValueIn(gate, 0);
    DEFVALUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
#ifndef ARK_ASAN_ON
    Label success(&builder_);
    Label callRuntime(&builder_);
    size_t topOffset = JSThread::GlueData::GetNewSpaceAllocationTopAddressOffset(false);
    size_t endOffset = JSThread::GlueData::GetNewSpaceAllocationEndAddressOffset(false);
    GateRef topAddress = builder_.Load(VariableType::NATIVE_POINTER(), glue_, builder_.IntPtr(topOffset));
    GateRef endAddress = builder_.Load(VariableType::NATIVE_POINTER(), glue_, builder_.IntPtr(endOffset));
    GateRef top = builder_.Load(VariableType::JS_POINTER(), topAddress, builder_.IntPtr(0));
    GateRef end = builder_.Load(VariableType::JS_POINTER(), endAddress, builder_.IntPtr(0));

    GateRef newTop = builder_.PtrAdd(top, size);
    builder_.Branch(builder_.IntPtrGreaterThan(newTop, end), &callRuntime, &success);
    builder_.Bind(&success);
    {
        builder_.Store(VariableType::NATIVE_POINTER(), glue_, topAddress, builder_.IntPtr(0), newTop);
        result = top;
        builder_.Jump(&exit);
    }
    builder_.Bind(&callRuntime);
#endif
    {
        result = builder_.CallRuntime(glue_, RTSTUB_ID(AllocateInYoung), Gate::InvalidGateRef,
                                      {builder_.ToTaggedInt(size)}, gate);
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), *result);
}

void MCRLowering::LowerInt32CheckRightIsZero(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef right = acc_.GetValueIn(gate, 0);
    GateRef rightNotZero = builder_.Int32NotEqual(right, builder_.Int32(0));
    builder_.DeoptCheck(rightNotZero, frameState, DeoptType::MODZERO1);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerRemainderIsNegativeZero(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef left = acc_.GetValueIn(gate, 0);
    GateRef right = acc_.GetValueIn(gate, 1);
    GateRef leftIsNegative = builder_.Int32LessThan(left, builder_.Int32(0));
    GateRef remainder =
        builder_.BinaryArithmetic(circuit_->Smod(), MachineType::I32, left, right, GateType::NJSValue());
    GateRef remainderEqualZero = builder_.Int32Equal(remainder, builder_.Int32(0));
    GateRef remainderIsNotNegative = builder_.BoolNot(builder_.BoolAnd(leftIsNegative, remainderEqualZero));
    builder_.DeoptCheck(remainderIsNotNegative, frameState, DeoptType::REMAINDERISNEGATIVEZERO);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerFloat64CheckRightIsZero(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef right = acc_.GetValueIn(gate, 0);
    GateRef rightNotZero = builder_.DoubleNotEqual(right, builder_.Double(0.0));
    builder_.DeoptCheck(rightNotZero, frameState, DeoptType::DIVZERO1);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerLexVarIsHoleCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef valueIsNotHole = builder_.TaggedIsNotHole(value);
    builder_.DeoptCheck(valueIsNotHole, frameState, DeoptType::LEXVARISHOLE1);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerValueCheckNegOverflow(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef valueNotZero = builder_.NotEqual(value, builder_.Int32(0));
    builder_.DeoptCheck(valueNotZero, frameState, DeoptType::NOTNEGOV1);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerOverflowCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef result = acc_.GetValueIn(gate, 0);
    GateRef condition = builder_.BoolNot(builder_.ExtractValue(MachineType::I1, result, builder_.Int32(1)));
    builder_.DeoptCheck(condition, frameState, DeoptType::NOTINT3);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerInt32UnsignedUpperBoundCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef upperBound = acc_.GetValueIn(gate, 1);
    GateRef condition = builder_.Int32UnsignedLessThanOrEqual(value, upperBound);
    builder_.DeoptCheck(condition, frameState, DeoptType::NOTINT4);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::LowerInt32DivWithCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = acc_.GetFrameState(gate);
    GateRef left = acc_.GetValueIn(gate, 0);
    GateRef right = acc_.GetValueIn(gate, 1);
    GateRef result = Circuit::NullGate();
    GateRef rightGreaterZero = builder_.Int32GreaterThan(right, builder_.Int32(0));
    GateRef rightLessZero = builder_.Int32LessThan(right, builder_.Int32(0));
    GateRef leftNotZero = builder_.Int32NotEqual(left, builder_.Int32(0));
    GateRef condition = builder_.BoolOr(rightGreaterZero, builder_.BoolAnd(rightLessZero, leftNotZero));
    builder_.DeoptCheck(condition, frameState, DeoptType::DIVZERO2);
    result = builder_.BinaryArithmetic(circuit_->Sdiv(), MachineType::I32, left, right, GateType::NJSValue());
    GateRef truncated = builder_.BinaryArithmetic(circuit_->Mul(),
        MachineType::I32, result, right, GateType::NJSValue());
    GateRef overCheck = builder_.Int32Equal(truncated, left);
    builder_.DeoptCheck(overCheck, frameState, DeoptType::NOTINT5);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void MCRLowering::LowerStoreMemory(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef index = acc_.GetValueIn(gate, 1);
    GateRef value = acc_.GetValueIn(gate, 2);
    builder_.Store(VariableType::VOID(), glue_, receiver, index, value);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void MCRLowering::InitializeWithSpeicalValue(Label *exit, GateRef object, GateRef glue,
                                             GateRef value, GateRef start, GateRef end)
{
    Label begin(&builder_);
    Label storeValue(&builder_);
    Label endLoop(&builder_);

    DEFVALUE(startOffset, (&builder_), VariableType::INT32(), start);
    builder_.Jump(&begin);
    builder_.LoopBegin(&begin);
    {
        builder_.Branch(builder_.Int32UnsignedLessThan(*startOffset, end), &storeValue, exit);
        builder_.Bind(&storeValue);
        {
            builder_.Store(VariableType::INT64(), glue, object, builder_.ZExtInt32ToPtr(*startOffset), value);
            startOffset = builder_.Int32Add(*startOffset, builder_.Int32(JSTaggedValue::TaggedTypeSize()));
            builder_.Jump(&endLoop);
        }
        builder_.Bind(&endLoop);
        builder_.LoopEnd(&begin);
    }
}
}  // namespace panda::ecmascript
