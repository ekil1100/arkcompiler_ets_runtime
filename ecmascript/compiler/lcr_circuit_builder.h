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

#ifndef ECMASCRIPT_COMPILER_LCR_CIRCUIT_BUILDER_H
#define ECMASCRIPT_COMPILER_LCR_CIRCUIT_BUILDER_H

#include "ecmascript/compiler/circuit_builder.h"
#include "ecmascript/compiler/circuit_builder_helper.h"
#include "ecmascript/compiler/share_gate_meta_data.h"
#include "ecmascript/mem/region.h"
#include "ecmascript/method.h"

namespace panda::ecmascript::kungfu {

GateRef CircuitBuilder::Int8Equal(GateRef x, GateRef y)
{
    return Equal(x, y);
}

GateRef CircuitBuilder::Int32NotEqual(GateRef x, GateRef y)
{
    return NotEqual(x, y);
}

GateRef CircuitBuilder::Int64NotEqual(GateRef x, GateRef y)
{
    return NotEqual(x, y);
}

GateRef CircuitBuilder::Int64Equal(GateRef x, GateRef y)
{
    return Equal(x, y);
}

GateRef CircuitBuilder::Int32Equal(GateRef x, GateRef y)
{
    return Equal(x, y);
}

GateRef CircuitBuilder::IntPtrGreaterThan(GateRef x, GateRef y)
{
    return env_->Is32Bit() ? Int32GreaterThan(x, y) : Int64GreaterThan(x, y);
}

GateRef CircuitBuilder::IntPtrAnd(GateRef x, GateRef y)
{
    return env_->Is32Bit() ? Int32And(x, y) : Int64And(x, y);
}

GateRef CircuitBuilder::IntPtrNot(GateRef x)
{
    return env_->Is32Bit() ? Int32Not(x) : Int64Not(x);
}

GateRef CircuitBuilder::IntPtrEqual(GateRef x, GateRef y)
{
    return env_->Is32Bit() ? Int32Equal(x, y) : Int64Equal(x, y);
}

GateRef CircuitBuilder::IntPtrLSR(GateRef x, GateRef y)
{
    auto ptrSize = env_->Is32Bit() ? MachineType::I32 : MachineType::I64;
    return BinaryArithmetic(circuit_->Lsr(), ptrSize, x, y);
}

GateRef CircuitBuilder::IntPtrLSL(GateRef x, GateRef y)
{
    auto ptrSize = env_->Is32Bit() ? MachineType::I32 : MachineType::I64;
    return BinaryArithmetic(circuit_->Lsl(), ptrSize, x, y);
}

GateRef CircuitBuilder::DoubleTrunc(GateRef gate, GateRef value, const char* comment)
{
    if (GetCompilationConfig()->IsAArch64()) {
        return DoubleTrunc(value, comment);
    }

    GateRef glue = acc_.GetGlueFromArgList();
    return CallNGCRuntime(glue, RTSTUB_ID(FloatTrunc), Gate::InvalidGateRef, {value}, gate, comment);
}

GateRef CircuitBuilder::Int16ToBigEndianInt16(GateRef x)
{
    GateRef int16toint32 = ZExtInt16ToInt32(x);
    GateRef high8bits = Int32LSL(Int32And(int16toint32, Int32(0x00FF)), Int32(8));
    GateRef low8bits = Int32LSR(Int32And(int16toint32, Int32(0xFF00)), Int32(8));
    return TruncInt32ToInt16(Int32Add(high8bits, low8bits));
}

GateRef CircuitBuilder::Int32ToBigEndianInt32(GateRef x)
{
    GateRef first8bits = Int32LSL(Int32And(x, Int32(0x000000FF)), Int32(24));
    GateRef second8bits = Int32LSL(Int32And(x, Int32(0x0000FF00)), Int32(8));
    GateRef third8bits = Int32LSR(Int32And(x, Int32(0x00FF0000)), Int32(8));
    GateRef fourth8bits = Int32LSR(Int32And(x, Int32(0xFF000000)), Int32(24));
    GateRef firstHalf = Int32Add(first8bits, second8bits);
    GateRef secondHalf = Int32Add(third8bits, fourth8bits);
    return Int32Add(firstHalf, secondHalf);
}

GateRef CircuitBuilder::Int64ToBigEndianInt64(GateRef x)
{
    GateRef first8bits = Int64LSL(Int64And(x, Int64(0x00000000000000FF)), Int64(56));
    GateRef second8bits = Int64LSL(Int64And(x, Int64(0x000000000000FF00)), Int64(40));
    // 0-16bits
    GateRef first16bits = Int64Add(first8bits, second8bits);
    GateRef third8bits = Int64LSL(Int64And(x, Int64(0x0000000000FF0000)), Int64(24));
    GateRef fourth8bits = Int64LSL(Int64And(x, Int64(0x00000000FF000000)), Int64(8));
    // 16-32bits
    GateRef second16bits = Int64Add(third8bits, fourth8bits);
    // 0-32bits
    GateRef firstHalf = Int64Add(first16bits, second16bits);
    GateRef fifth8bits = Int64LSR(Int64And(x, Int64(0x000000FF00000000)), Int64(8));
    GateRef sixth8bits = Int64LSR(Int64And(x, Int64(0x0000FF0000000000)), Int64(24));
    //32-48bits
    GateRef third16bits = Int64Add(fifth8bits, sixth8bits);
    GateRef seventh8bits = Int64LSR(Int64And(x, Int64(0x00FF000000000000)), Int64(40));
    GateRef eighth8bits = Int64LSR(Int64And(x, Int64(0xFF00000000000000)), Int64(56));
    //48-64bits
    GateRef fourth16bits = Int64Add(seventh8bits, eighth8bits);
    //32-64bits
    GateRef secondHalf = Int64Add(third16bits, fourth16bits);
    //0-64bits
    return Int64Add(firstHalf, secondHalf);
}

GateRef CircuitBuilder::IntPtrOr(GateRef x, GateRef y)
{
    auto ptrsize = env_->Is32Bit() ? MachineType::I32 : MachineType::I64;
    return BinaryArithmetic(circuit_->Or(), ptrsize, x, y);
}

GateRef CircuitBuilder::IntPtrDiv(GateRef x, GateRef y)
{
    return env_->Is32Bit() ? Int32Div(x, y) : Int64Div(x, y);
}

GateRef CircuitBuilder::GetInt64OfTInt(GateRef x)
{
    GateRef tagged = ChangeTaggedPointerToInt64(x);
    return Int64And(tagged, Int64(~JSTaggedValue::TAG_MARK));
}

GateRef CircuitBuilder::GetInt32OfTInt(GateRef x)
{
    GateRef tagged = ChangeTaggedPointerToInt64(x);
    return TruncInt64ToInt32(tagged);
}

GateRef CircuitBuilder::TaggedCastToIntPtr(GateRef x)
{
    return env_->Is32Bit() ? GetInt32OfTInt(x) : GetInt64OfTInt(x);
}

GateRef CircuitBuilder::GetDoubleOfTInt(GateRef x)
{
    return ChangeInt32ToFloat64(GetInt32OfTInt(x));
}

GateRef CircuitBuilder::GetDoubleOfTDouble(GateRef x)
{
    GateRef tagged = ChangeTaggedPointerToInt64(x);
    GateRef val = Int64Sub(tagged, Int64(JSTaggedValue::DOUBLE_ENCODE_OFFSET));
    return CastInt64ToFloat64(val);
}

GateRef CircuitBuilder::GetBooleanOfTBoolean(GateRef x)
{
    GateRef tagged = ChangeTaggedPointerToInt64(x);
    return TruncInt64ToInt1(tagged);
}

GateRef CircuitBuilder::GetDoubleOfTNumber(GateRef x)
{
    Label subentry(env_);
    SubCfgEntry(&subentry);
    Label isInt(env_);
    Label isDouble(env_);
    Label exit(env_);
    DEFVALUE(result, env_, VariableType::FLOAT64(), Double(0));
    BRANCH_CIR2(TaggedIsInt(x), &isInt, &isDouble);
    Bind(&isInt);
    {
        result = ChangeInt32ToFloat64(GetInt32OfTInt(x));
        Jump(&exit);
    }
    Bind(&isDouble);
    {
        result = GetDoubleOfTDouble(x);
        Jump(&exit);
    }
    Bind(&exit);
    GateRef ret = *result;
    SubCfgExit();
    return ret;
}

GateRef CircuitBuilder::DoubleToInt(GateRef x, Label *exit)
{
    Label overflow(env_);

    GateRef xInt = ChangeFloat64ToInt32(x);
    DEFVALUE(result, env_, VariableType::INT32(), xInt);

    GateRef xInt64 = CastDoubleToInt64(x);
    // exp = (u64 & DOUBLE_EXPONENT_MASK) >> DOUBLE_SIGNIFICAND_SIZE - DOUBLE_EXPONENT_BIAS
    GateRef exp = Int64And(xInt64, Int64(base::DOUBLE_EXPONENT_MASK));
    exp = TruncInt64ToInt32(Int64LSR(exp, Int64(base::DOUBLE_SIGNIFICAND_SIZE)));
    exp = Int32Sub(exp, Int32(base::DOUBLE_EXPONENT_BIAS));
    GateRef bits = Int32(base::INT32_BITS - 1);
    // exp < 32 - 1
    BRANCH_CIR2(Int32LessThan(exp, bits), exit, &overflow);

    Bind(&overflow);
    {
        result = CallNGCRuntime(acc_.GetGlueFromArgList(), RTSTUB_ID(DoubleToInt),
                                Circuit::NullGate(), { x, IntPtr(base::INT32_BITS) }, Circuit::NullGate());
        Jump(exit);
    }
    Bind(exit);
    auto ret = *result;
    return ret;
}

GateRef CircuitBuilder::DoubleToInt(GateRef glue, GateRef x, size_t typeBits)
{
    Label entry(env_);
    env_->SubCfgEntry(&entry);
    Label exit(env_);
    Label overflow(env_);

    GateRef xInt = ChangeFloat64ToInt32(x);
    DEFVALUE(result, env_, VariableType::INT32(), xInt);

    if (env_->IsAmd64()) {
        // 0x80000000: amd64 overflow return value
        BRANCH_CIR2(Int32Equal(xInt, Int32(0x80000000)), &overflow, &exit);
    } else {
        GateRef xInt64 = CastDoubleToInt64(x);
        // exp = (u64 & DOUBLE_EXPONENT_MASK) >> DOUBLE_SIGNIFICAND_SIZE - DOUBLE_EXPONENT_BIAS
        GateRef exp = Int64And(xInt64, Int64(base::DOUBLE_EXPONENT_MASK));
        exp = TruncInt64ToInt32(Int64LSR(exp, Int64(base::DOUBLE_SIGNIFICAND_SIZE)));
        exp = Int32Sub(exp, Int32(base::DOUBLE_EXPONENT_BIAS));
        GateRef bits = Int32(typeBits - 1);
        // exp < 32 - 1
        BRANCH_CIR2(Int32LessThan(exp, bits), &exit, &overflow);
    }
    Bind(&overflow);
    {
        result = CallNGCRuntime(glue, RTSTUB_ID(DoubleToInt), Circuit::NullGate(), { x, IntPtr(typeBits) },
                                Circuit::NullGate());
        Jump(&exit);
    }
    Bind(&exit);
    auto ret = *result;
    env_->SubCfgExit();
    return ret;
}

GateRef CircuitBuilder::DoubleCheckINFInRangeInt32(GateRef x)
{
    Label entry(env_);
    env_->SubCfgEntry(&entry);
    Label exit(env_);
    Label isInfinity(env_);
    Label positiveInf(env_);
    Label negativeInf(env_);

    DEFVALUE(result, env_, VariableType::INT32(), DoubleInRangeInt32(x));
    GateRef Max = Double(INT32_MAX);
    GateRef Min = Double(INT32_MIN);
    GateRef pInfinity = Double(base::POSITIVE_INFINITY);
    Branch(DoubleIsINF(x), &isInfinity, &exit);
    Bind(&isInfinity);
    {
        Branch(DoubleEqual(x, pInfinity), &positiveInf, &negativeInf);
        Bind(&positiveInf);
        {
            result = ChangeFloat64ToInt32(Max);
            Jump(&exit);
        }
        Bind(&negativeInf);
        {
            result = ChangeFloat64ToInt32(Min);
            Jump(&exit);
        }
    }
    Bind(&exit);
    auto ret = *result;
    env_->SubCfgExit();
    return ret;
}

GateRef CircuitBuilder::DoubleInRangeInt32(GateRef x)
{
    Label entry(env_);
    env_->SubCfgEntry(&entry);
    Label exit(env_);
    Label overflow(env_);
    Label checkUnderflow(env_);
    Label underflow(env_);

    DEFVALUE(result, env_, VariableType::INT32(), ChangeFloat64ToInt32(x));
    GateRef Max = Double(INT32_MAX);
    GateRef Min = Double(INT32_MIN);
    Branch(DoubleGreaterThan(x, Max), &overflow, &checkUnderflow);
    Bind(&overflow);
    {
        result = ChangeFloat64ToInt32(Max);
        Jump(&exit);
    }
    Bind(&checkUnderflow);
    {
        Branch(DoubleLessThan(x, Min), &underflow, &exit);
        Bind(&underflow);
        {
            result = ChangeFloat64ToInt32(Min);
            Jump(&exit);
        }
    }
    Bind(&exit);
    auto ret = *result;
    env_->SubCfgExit();
    return ret;
}

GateRef CircuitBuilder::Int32ToTaggedInt(GateRef x)
{
    GateRef val = SExtInt32ToInt64(x);
    return Int64Or(val, Int64(JSTaggedValue::TAG_INT));
}

GateRef CircuitBuilder::Int32ToTaggedPtr(GateRef x)
{
    GateRef val = SExtInt32ToInt64(x);
    return Int64ToTaggedPtr(Int64Or(val, Int64(JSTaggedValue::TAG_INT)));
}

GateRef CircuitBuilder::Int64ToTaggedPtr(GateRef x)
{
    return GetCircuit()->NewGate(circuit_->Int64ToTagged(),
        MachineType::I64, { x }, GateType::TaggedValue());
}

GateRef CircuitBuilder::ToTaggedInt(GateRef x)
{
    return Int64Or(x, Int64(JSTaggedValue::TAG_INT));
}

GateRef CircuitBuilder::ToTaggedIntPtr(GateRef x)
{
    return Int64ToTaggedPtr(Int64Or(x, Int64(JSTaggedValue::TAG_INT)));
}

GateRef CircuitBuilder::DoubleToTaggedDoublePtr(GateRef x)
{
    GateRef val = CastDoubleToInt64(x);
    return Int64ToTaggedPtr(Int64Add(val, Int64(JSTaggedValue::DOUBLE_ENCODE_OFFSET)));
}

GateRef CircuitBuilder::DoubleIsImpureNaN(GateRef x)
{
    GateRef impureNaN = Int64(JSTaggedValue::TAG_INT - JSTaggedValue::DOUBLE_ENCODE_OFFSET);
    GateRef val = CastDoubleToInt64(x);
    return Int64UnsignedGreaterThanOrEqual(val, impureNaN);
}

GateRef CircuitBuilder::BooleanToTaggedBooleanPtr(GateRef x)
{
    auto val = ZExtInt1ToInt64(x);
    return Int64ToTaggedPtr(Int64Or(val, Int64(JSTaggedValue::TAG_BOOLEAN_MASK)));
}

GateRef CircuitBuilder::BooleanToInt32(GateRef x)
{
    return ZExtInt1ToInt32(x);
}

GateRef CircuitBuilder::BooleanToFloat64(GateRef x)
{
    return ChangeInt32ToFloat64(ZExtInt1ToInt32(x));
}

GateRef CircuitBuilder::Float32ToTaggedDoublePtr(GateRef x)
{
    GateRef val = ExtFloat32ToDouble(x);
    return DoubleToTaggedDoublePtr(val);
}

GateRef CircuitBuilder::TaggedDoublePtrToFloat32(GateRef x)
{
    GateRef val = GetDoubleOfTDouble(x);
    return TruncDoubleToFloat32(val);
}

GateRef CircuitBuilder::TaggedIntPtrToFloat32(GateRef x)
{
    GateRef val = GetInt32OfTInt(x);
    return ChangeInt32ToFloat32(val);
}

GateRef CircuitBuilder::DoubleToTaggedDouble(GateRef x)
{
    GateRef val = CastDoubleToInt64(x);
    return Int64Add(val, Int64(JSTaggedValue::DOUBLE_ENCODE_OFFSET));
}

GateRef CircuitBuilder::DoubleIsNAN(GateRef x)
{
    GateRef diff = DoubleEqual(x, x);
    return Equal(SExtInt1ToInt32(diff), Int32(0));
}

GateRef CircuitBuilder::DoubleToTagged(GateRef x)
{
    GateRef val = CastDoubleToInt64(x);
    acc_.SetGateType(val, GateType::TaggedValue());
    return Int64Add(val, Int64(JSTaggedValue::DOUBLE_ENCODE_OFFSET));
}

template<OpCode Op, MachineType Type>
GateRef CircuitBuilder::BinaryOp(GateRef x, GateRef y)
{
    if (Op == OpCode::ADD) {
        return BinaryArithmetic(circuit_->Add(), Type, x, y);
    } else if (Op == OpCode::SUB) {
        return BinaryArithmetic(circuit_->Sub(), Type, x, y);
    } else if (Op == OpCode::MUL) {
        return BinaryArithmetic(circuit_->Mul(), Type, x, y);
    }
    UNREACHABLE();
    return Circuit::NullGate();
}

template<OpCode Op, MachineType Type>
GateRef CircuitBuilder::BinaryOpWithOverflow(GateRef x, GateRef y)
{
    if (Op == OpCode::ADD) {
        return BinaryArithmetic(circuit_->AddWithOverflow(), Type, x, y);
    } else if (Op == OpCode::SUB) {
        return BinaryArithmetic(circuit_->SubWithOverflow(), Type, x, y);
    } else if (Op == OpCode::MUL) {
        return BinaryArithmetic(circuit_->MulWithOverflow(), Type, x, y);
    }
    UNREACHABLE();
    return Circuit::NullGate();
}

GateRef CircuitBuilder::Equal(GateRef x, GateRef y, const char* comment)
{
    auto xType = acc_.GetMachineType(x);
    switch (xType) {
        case ARCH:
        case FLEX:
        case I1:
        case I8:
        case I16:
        case I32:
        case I64:
            return BinaryCmp(circuit_->Icmp(static_cast<uint64_t>(ICmpCondition::EQ)), x, y, comment);
        case F32:
        case F64:
            return BinaryCmp(circuit_->Fcmp(static_cast<uint64_t>(FCmpCondition::OEQ)), x, y, comment);
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
}

GateRef CircuitBuilder::NotEqual(GateRef x, GateRef y, const char* comment)
{
    auto xType = acc_.GetMachineType(x);
    switch (xType) {
        case ARCH:
        case FLEX:
        case I1:
        case I8:
        case I16:
        case I32:
        case I64:
            return BinaryCmp(circuit_->Icmp(static_cast<uint64_t>(ICmpCondition::NE)), x, y, comment);
        case F32:
        case F64:
            return BinaryCmp(circuit_->Fcmp(static_cast<uint64_t>(FCmpCondition::ONE)), x, y, comment);
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
}

}

#endif  // ECMASCRIPT_COMPILER_LCR_CIRCUIT_BUILDER_H
