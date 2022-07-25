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

#ifndef ECMASCRIPT_COMPILER_STUB_INL_H
#define ECMASCRIPT_COMPILER_STUB_INL_H

#include "ecmascript/compiler/bc_call_signature.h"
#include "ecmascript/compiler/stub_builder.h"
#include "ecmascript/accessor_data.h"
#include "ecmascript/base/number_helper.h"
#include "ecmascript/global_dictionary.h"
#include "ecmascript/global_env_constants.h"
#include "ecmascript/ic/ic_handler.h"
#include "ecmascript/ic/proto_change_details.h"
#include "ecmascript/js_array.h"
#include "ecmascript/js_function.h"
#include "ecmascript/js_generator_object.h"
#include "ecmascript/js_object.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/layout_info.h"
#include "ecmascript/message_string.h"
#include "ecmascript/mem/slots.h"
#include "ecmascript/mem/visitor.h"

namespace panda::ecmascript::kungfu {
using JSFunction = panda::ecmascript::JSFunction;
using PropertyBox = panda::ecmascript::PropertyBox;

inline GateRef StubBuilder::Int8(int8_t value)
{
    return env_->GetBuilder()->Int8(value);
}

inline GateRef StubBuilder::Int16(int16_t value)
{
    return env_->GetBuilder()->Int16(value);
}

inline GateRef StubBuilder::Int32(int32_t value)
{
    return env_->GetBuilder()->Int32(value);
};

inline GateRef StubBuilder::Int64(int64_t value)
{
    return env_->GetBuilder()->Int64(value);
}

inline GateRef StubBuilder::IntPtr(int64_t value)
{
    return env_->Is32Bit() ? Int32(value) : Int64(value);
};

inline GateRef StubBuilder::IntPtrSize()
{
    return env_->Is32Bit() ? Int32(sizeof(uint32_t)) : Int64(sizeof(uint64_t));
}

inline GateRef StubBuilder::True()
{
    return TruncInt32ToInt1(Int32(1));
}

inline GateRef StubBuilder::False()
{
    return TruncInt32ToInt1(Int32(0));
}

inline GateRef StubBuilder::Boolean(bool value)
{
    return env_->GetBuilder()->Boolean(value);
}

inline GateRef StubBuilder::Double(double value)
{
    return env_->GetBuilder()->Double(value);
}

inline GateRef StubBuilder::Undefined(VariableType type)
{
    return env_->GetBuilder()->UndefineConstant(type.GetGateType());
}

inline GateRef StubBuilder::Hole(VariableType type)
{
    return env_->GetBuilder()->HoleConstant(type.GetGateType());
}

inline GateRef StubBuilder::Null(VariableType type)
{
    return env_->GetBuilder()->NullConstant(type.GetGateType());
}

inline GateRef StubBuilder::Exception(VariableType type)
{
    return env_->GetBuilder()->ExceptionConstant(type.GetGateType());
}

inline GateRef StubBuilder::PtrMul(GateRef x, GateRef y)
{
    if (env_->Is32Bit()) {
        return Int32Mul(x, y);
    } else {
        return Int64Mul(x, y);
    }
}

inline GateRef StubBuilder::RelocatableData(uint64_t value)
{
    return env_->GetBuilder()->RelocatableData(value);
}

// parameter
inline GateRef StubBuilder::Argument(size_t index)
{
    return env_->GetArgument(index);
}

inline GateRef StubBuilder::Int1Argument(size_t index)
{
    return Argument(index);
}

inline GateRef StubBuilder::Int32Argument(size_t index)
{
    return Argument(index);
}

inline GateRef StubBuilder::Int64Argument(size_t index)
{
    return Argument(index);
}

inline GateRef StubBuilder::TaggedArgument(size_t index)
{
    return Argument(index);
}

inline GateRef StubBuilder::TaggedPointerArgument(size_t index)
{
    return Argument(index);
}

inline GateRef StubBuilder::PtrArgument(size_t index)
{
    return Argument(index);
}

inline GateRef StubBuilder::Float32Argument(size_t index)
{
    return Argument(index);
}

inline GateRef StubBuilder::Float64Argument(size_t index)
{
    return Argument(index);
}

inline GateRef StubBuilder::Alloca(int size)
{
    return env_->GetBuilder()->Alloca(size);
}

inline GateRef StubBuilder::Return(GateRef value)
{
    auto control = env_->GetCurrentLabel()->GetControl();
    auto depend = env_->GetCurrentLabel()->GetDepend();
    return env_->GetBuilder()->Return(control, depend, value);
}

inline GateRef StubBuilder::Return()
{
    auto control = env_->GetCurrentLabel()->GetControl();
    auto depend = env_->GetCurrentLabel()->GetDepend();
    return env_->GetBuilder()->ReturnVoid(control, depend);
}

inline void StubBuilder::Bind(Label *label)
{
    label->Bind();
    env_->SetCurrentLabel(label);
}

inline GateRef StubBuilder::CallRuntime(GateRef glue, int index, const std::initializer_list<GateRef>& args)
{
    SavePcIfNeeded(glue);
    GateRef result = env_->GetBuilder()->CallRuntime(glue, index, Gate::InvalidGateRef, args);
    return result;
}

inline GateRef StubBuilder::CallRuntime(GateRef glue, int index, GateRef argc, GateRef argv)
{
    SavePcIfNeeded(glue);
    GateRef result = env_->GetBuilder()->CallRuntimeVarargs(glue, index, argc, argv);
    return result;
}

inline GateRef StubBuilder::CallNGCRuntime(GateRef glue, int index, const std::initializer_list<GateRef>& args)
{
    GateRef result = env_->GetBuilder()->CallNGCRuntime(glue, index, Gate::InvalidGateRef, args);
    return result;
}

inline GateRef StubBuilder::UpdateLeaveFrameAndCallNGCRuntime(GateRef glue, int index,
    const std::initializer_list<GateRef>& args)
{
    if (env_->IsAsmInterp()) {
        // CpuProfiler will get the latest leaveFrame_ in thread to up frames.
        // So it's necessary to update leaveFrame_ if the program enters the c++ environment.
        // We use the latest asm interpreter frame to update it when CallNGCRuntime.
        GateRef sp = Argument(static_cast<size_t>(InterpreterHandlerInputs::SP));
        GateRef spOffset = IntPtr(JSThread::GlueData::GetLeaveFrameOffset(env_->Is32Bit()));
        Store(VariableType::NATIVE_POINTER(), glue, glue, spOffset, sp);
    }
    GateRef result = CallNGCRuntime(glue, index, args);
    return result;
}

inline GateRef StubBuilder::CallStub(GateRef glue, int index, const std::initializer_list<GateRef>& args)
{
    SavePcIfNeeded(glue);
    GateRef result = env_->GetBuilder()->CallStub(glue, index, args);
    return result;
}

inline void StubBuilder::DebugPrint(GateRef glue, std::initializer_list<GateRef> args)
{
    UpdateLeaveFrameAndCallNGCRuntime(glue, RTSTUB_ID(DebugPrint), args);
}

inline void StubBuilder::FatalPrint(GateRef glue, std::initializer_list<GateRef> args)
{
    UpdateLeaveFrameAndCallNGCRuntime(glue, RTSTUB_ID(FatalPrint), args);
}

void StubBuilder::SavePcIfNeeded(GateRef glue)
{
    if (env_->IsAsmInterp()) {
        GateRef sp = Argument(static_cast<size_t>(InterpreterHandlerInputs::SP));
        GateRef pc = Argument(static_cast<size_t>(InterpreterHandlerInputs::PC));
        GateRef frame = PtrSub(sp,
            IntPtr(AsmInterpretedFrame::GetSize(GetEnvironment()->IsArch32Bit())));
        Store(VariableType::INT64(), glue, frame,
            IntPtr(AsmInterpretedFrame::GetPcOffset(GetEnvironment()->IsArch32Bit())), pc);
    }
}

// memory
inline GateRef StubBuilder::Load(VariableType type, GateRef base, GateRef offset)
{
    if (type == VariableType::NATIVE_POINTER()) {
        type = env_->IsArch64Bit() ? VariableType::INT64() : VariableType::INT32();
    }
    return env_->GetBuilder()->Load(type, base, offset);
}

inline GateRef StubBuilder::Load(VariableType type, GateRef base)
{
    return Load(type, base, IntPtr(0));
}

// arithmetic
inline GateRef StubBuilder::Int16Add(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::ADD), MachineType::I16, x, y);
}

inline GateRef StubBuilder::Int32Add(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::ADD), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int64Add(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::ADD), MachineType::I64, x, y);
}

inline GateRef StubBuilder::DoubleAdd(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::ADD), MachineType::F64, x, y);
}

inline GateRef StubBuilder::PtrAdd(GateRef x, GateRef y)
{
    if (env_->Is32Bit()) {
        return Int32Add(x, y);
    }
    return Int64Add(x, y);
}

inline GateRef StubBuilder::IntPtrAnd(GateRef x, GateRef y)
{
    return env_->Is32Bit() ? Int32And(x, y) : Int64And(x, y);
}

inline GateRef StubBuilder::IntPtrEqual(GateRef x, GateRef y)
{
    if (env_->Is32Bit()) {
        return Int32Equal(x, y);
    }
    return Int64Equal(x, y);
}

inline GateRef StubBuilder::PtrSub(GateRef x, GateRef y)
{
    if (env_->Is32Bit()) {
        return Int32Sub(x, y);
    }
    return Int64Sub(x, y);
}

inline GateRef StubBuilder::PointerSub(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SUB), MachineType::ARCH, x, y);
}

inline GateRef StubBuilder::Int16Sub(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SUB), MachineType::I16, x, y);
}

inline GateRef StubBuilder::Int32Sub(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SUB), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int64Sub(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SUB), MachineType::I64, x, y);
}

inline GateRef StubBuilder::DoubleSub(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SUB), MachineType::F64, x, y);
}

inline GateRef StubBuilder::Int32Mul(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::MUL), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int64Mul(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::MUL), MachineType::I64, x, y);
}

inline GateRef StubBuilder::DoubleMul(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::MUL), MachineType::F64, x, y);
}

inline GateRef StubBuilder::DoubleDiv(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::FDIV), MachineType::F64, x, y);
}

inline GateRef StubBuilder::Int32Div(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SDIV), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int64Div(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SDIV), MachineType::I64, x, y);
}

inline GateRef StubBuilder::IntPtrDiv(GateRef x, GateRef y)
{
    return env_->Is32Bit() ? Int32Div(x, y) : Int64Div(x, y);
}

inline GateRef StubBuilder::Int32Mod(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SMOD), MachineType::I32, x, y);
}

inline GateRef StubBuilder::DoubleMod(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::SMOD), MachineType::F64, x, y);
}

// bit operation
inline GateRef StubBuilder::Int32Or(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::OR), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int8And(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::AND), MachineType::I8, x, y);
}

inline GateRef StubBuilder::Int32And(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::AND), MachineType::I32, x, y);
}

inline GateRef StubBuilder::BoolAnd(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::AND), MachineType::I1, x, y);
}

inline GateRef StubBuilder::BoolOr(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::OR), MachineType::I1, x, y);
}

inline GateRef StubBuilder::Int32Not(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::REV), MachineType::I32, x);
}

inline GateRef StubBuilder::BoolNot(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::REV), MachineType::I1, x);
}

inline GateRef StubBuilder::Int64Or(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::OR), MachineType::I64, x, y);
}

inline GateRef StubBuilder::IntPtrOr(GateRef x, GateRef y)
{
    auto ptrsize = env_->Is32Bit() ? MachineType::I32 : MachineType::I64;
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::OR), ptrsize, x, y);
}

inline GateRef StubBuilder::Int64And(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::AND), MachineType::I64, x, y);
}

inline GateRef StubBuilder::Int16LSL(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::LSL), MachineType::I16, x, y);
}

inline GateRef StubBuilder::Int64Xor(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::XOR), MachineType::I64, x, y);
}

inline GateRef StubBuilder::Int32Xor(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::XOR), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int8LSR(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::LSR), MachineType::I8, x, y);
}

inline GateRef StubBuilder::Int64Not(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::REV), MachineType::I64, x);
}

inline GateRef StubBuilder::Int32LSL(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::LSL), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int64LSL(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::LSL), MachineType::I64, x, y);
}

inline GateRef StubBuilder::IntPtrLSL(GateRef x, GateRef y)
{
    auto ptrSize = env_->Is32Bit() ? MachineType::I32 : MachineType::I64;
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::LSL), ptrSize, x, y);
}

inline GateRef StubBuilder::Int32ASR(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::ASR), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int32LSR(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::LSR), MachineType::I32, x, y);
}

inline GateRef StubBuilder::Int64LSR(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::LSR), MachineType::I64, x, y);
}

inline GateRef StubBuilder::IntPtrLSR(GateRef x, GateRef y)
{
    auto ptrSize = env_->Is32Bit() ? MachineType::I32 : MachineType::I64;
    return env_->GetBuilder()->BinaryArithmetic(OpCode(OpCode::LSR), ptrSize, x, y);
}

template<OpCode::Op Op, MachineType Type>
inline GateRef StubBuilder::BinaryOp(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryArithmetic(OpCode(Op), Type, x, y);
}

inline GateRef StubBuilder::TaggedIsInt(GateRef x)
{
    return Int64Equal(Int64And(x, Int64(JSTaggedValue::TAG_MARK)),
                      Int64(JSTaggedValue::TAG_INT));
}

inline GateRef StubBuilder::TaggedIsDouble(GateRef x)
{
    return BoolAnd(TaggedIsNumber(x), BoolNot(TaggedIsInt(x)));
}

inline GateRef StubBuilder::TaggedIsObject(GateRef x)
{
    return Int64Equal(Int64And(x, Int64(JSTaggedValue::TAG_MARK)),
                      Int64(JSTaggedValue::TAG_OBJECT));
}

inline GateRef StubBuilder::TaggedIsNumber(GateRef x)
{
    return BoolNot(TaggedIsObject(x));
}

inline GateRef StubBuilder::TaggedIsHole(GateRef x)
{
    return Int64Equal(x, Int64(JSTaggedValue::VALUE_HOLE));
}

inline GateRef StubBuilder::TaggedIsNotHole(GateRef x)
{
    return Int64NotEqual(x, Int64(JSTaggedValue::VALUE_HOLE));
}

inline GateRef StubBuilder::TaggedIsUndefined(GateRef x)
{
    return Int64Equal(x, Int64(JSTaggedValue::VALUE_UNDEFINED));
}

inline GateRef StubBuilder::TaggedIsException(GateRef x)
{
    return Int64Equal(x, Int64(JSTaggedValue::VALUE_EXCEPTION));
}

inline GateRef StubBuilder::TaggedIsSpecial(GateRef x)
{
    return BoolOr(Int64Equal(Int64And(x, Int64(JSTaggedValue::TAG_SPECIAL_MARK)),
        Int64(JSTaggedValue::TAG_SPECIAL)), TaggedIsHole(x));
}

inline GateRef StubBuilder::TaggedIsHeapObject(GateRef x)
{
    return Int64Equal(Int64And(x, Int64(JSTaggedValue::TAG_HEAPOBJECT_MARK)), Int64(0));
}

inline GateRef StubBuilder::TaggedIsGeneratorObject(GateRef x)
{
    GateRef isHeapObj = SExtInt1ToInt32(TaggedIsHeapObject(x));
    GateRef objType = GetObjectType(LoadHClass(x));
    GateRef isGeneratorObj = Int32Or(
        SExtInt1ToInt32(Int32Equal(objType, Int32(static_cast<int32_t>(JSType::JS_GENERATOR_OBJECT)))),
        SExtInt1ToInt32(Int32Equal(objType, Int32(static_cast<int32_t>(JSType::JS_ASYNC_FUNC_OBJECT)))));
    return TruncInt32ToInt1(Int32And(isHeapObj, isGeneratorObj));
}

inline GateRef StubBuilder::TaggedIsPropertyBox(GateRef x)
{
    return TruncInt32ToInt1(
        Int32And(SExtInt1ToInt32(TaggedIsHeapObject(x)),
                 SExtInt1ToInt32(HclassIsPropertyBox(LoadHClass(x)))));
}

inline GateRef StubBuilder::TaggedIsWeak(GateRef x)
{
    return Int64Equal(Int64And(x, Int64(JSTaggedValue::TAG_WEAK_MARK)), Int64(JSTaggedValue::TAG_WEAK));
}

inline GateRef StubBuilder::TaggedIsPrototypeHandler(GateRef x)
{
    return HclassIsPrototypeHandler(LoadHClass(x));
}

inline GateRef StubBuilder::TaggedIsTransitionHandler(GateRef x)
{
    return TruncInt32ToInt1(
        Int32And(SExtInt1ToInt32(TaggedIsHeapObject(x)),
                 SExtInt1ToInt32(HclassIsTransitionHandler(LoadHClass(x)))));
}

inline GateRef StubBuilder::GetNextPositionForHash(GateRef last, GateRef count, GateRef size)
{
    auto nextOffset = Int32LSR(Int32Mul(count, Int32Add(count, Int32(1))),
                               Int32(1));
    return Int32And(Int32Add(last, nextOffset), Int32Sub(size, Int32(1)));
}

inline GateRef StubBuilder::DoubleIsNAN(GateRef x)
{
    GateRef diff = DoubleEqual(x, x);
    return Int32Equal(SExtInt1ToInt32(diff), Int32(0));
}

inline GateRef StubBuilder::DoubleIsINF(GateRef x)
{
    GateRef infinity = Double(base::POSITIVE_INFINITY);
    GateRef negativeInfinity = Double(-base::POSITIVE_INFINITY);
    GateRef diff1 = DoubleEqual(x, infinity);
    GateRef diff2 = DoubleEqual(x, negativeInfinity);
    return TruncInt32ToInt1(Int32Or(Int32Equal(SExtInt1ToInt32(diff1), Int32(1)),
        Int32Equal(SExtInt1ToInt32(diff2), Int32(1))));
}

inline GateRef StubBuilder::TaggedIsNull(GateRef x)
{
    return Int64Equal(x, Int64(JSTaggedValue::VALUE_NULL));
}

inline GateRef StubBuilder::TaggedIsUndefinedOrNull(GateRef x)
{
    return Int64Equal(Int64And(x, Int64(JSTaggedValue::TAG_HEAPOBJECT_MARK)),
        Int64(JSTaggedValue::TAG_SPECIAL));
}

inline GateRef StubBuilder::TaggedIsTrue(GateRef x)
{
    return Int64Equal(x, Int64(JSTaggedValue::VALUE_TRUE));
}

inline GateRef StubBuilder::TaggedIsFalse(GateRef x)
{
    return Int64Equal(x, Int64(JSTaggedValue::VALUE_FALSE));
}

inline GateRef StubBuilder::TaggedIsBoolean(GateRef x)
{
    return Int64Equal(Int64And(x, Int64(JSTaggedValue::TAG_HEAPOBJECT_MARK)),
        Int64(JSTaggedValue::TAG_BOOLEAN_MARK));
}

inline GateRef StubBuilder::TaggedGetInt(GateRef x)
{
    return TruncInt64ToInt32(Int64And(x, Int64(~JSTaggedValue::TAG_MARK)));
}

inline GateRef StubBuilder::Int8ToTaggedTypeNGC(GateRef x)
{
    GateRef val = SExtInt8ToInt64(x);
    return Int64Or(val, Int64(JSTaggedValue::TAG_INT));
}

inline GateRef StubBuilder::Int16ToTaggedNGC(GateRef x)
{
    GateRef val = SExtInt16ToInt64(x);
    return ChangeInt64ToTagged(Int64Or(val, Int64(JSTaggedValue::TAG_INT)));
}

inline GateRef StubBuilder::Int16ToTaggedTypeNGC(GateRef x)
{
    GateRef val = SExtInt16ToInt64(x);
    return Int64Or(val, Int64(JSTaggedValue::TAG_INT));
}

inline GateRef StubBuilder::IntToTaggedNGC(GateRef x)
{
    GateRef val = SExtInt32ToInt64(x);
    return ChangeInt64ToTagged(Int64Or(val, Int64(JSTaggedValue::TAG_INT)));
}

inline GateRef StubBuilder::IntToTaggedTypeNGC(GateRef x)
{
    GateRef val = SExtInt32ToInt64(x);
    return Int64Or(val, Int64(JSTaggedValue::TAG_INT));
}

inline GateRef StubBuilder::DoubleBuildTaggedWithNoGC(GateRef x)
{
    GateRef val = CastDoubleToInt64(x);
    return ChangeInt64ToTagged(Int64Add(val, Int64(JSTaggedValue::DOUBLE_ENCODE_OFFSET)));
}

inline GateRef StubBuilder::DoubleBuildTaggedTypeWithNoGC(GateRef x)
{
    GateRef val = CastDoubleToInt64(x);
    return Int64Add(val, Int64(JSTaggedValue::DOUBLE_ENCODE_OFFSET));
}

inline GateRef StubBuilder::CastDoubleToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::BITCAST), MachineType::I64, x);
}

inline GateRef StubBuilder::TaggedTrue()
{
    return Int64(JSTaggedValue::VALUE_TRUE);
}

inline GateRef StubBuilder::TaggedFalse()
{
    return Int64(JSTaggedValue::VALUE_FALSE);
}

// compare operation
inline GateRef StubBuilder::Int8Equal(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::EQ), x, y);
}

inline GateRef StubBuilder::Int32Equal(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::EQ), x, y);
}

inline GateRef StubBuilder::Int32NotEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::NE), x, y);
}

inline GateRef StubBuilder::Int64Equal(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::EQ), x, y);
}

inline GateRef StubBuilder::DoubleEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::EQ), x, y);
}

inline GateRef StubBuilder::DoubleLessThan(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SLT), x, y);
}

inline GateRef StubBuilder::DoubleLessThanOrEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SLE), x, y);
}

inline GateRef StubBuilder::DoubleGreaterThan(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SGT), x, y);
}

inline GateRef StubBuilder::DoubleGreaterThanOrEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SGE), x, y);
}

inline GateRef StubBuilder::Int64NotEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::NE), x, y);
}

inline GateRef StubBuilder::Int32GreaterThan(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SGT), x, y);
}

inline GateRef StubBuilder::Int32LessThan(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SLT), x, y);
}

inline GateRef StubBuilder::Int32GreaterThanOrEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SGE), x, y);
}

inline GateRef StubBuilder::Int32LessThanOrEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SLE), x, y);
}

inline GateRef StubBuilder::Int32UnsignedGreaterThan(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::UGT), x, y);
}

inline GateRef StubBuilder::Int32UnsignedLessThan(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::ULT), x, y);
}

inline GateRef StubBuilder::Int32UnsignedGreaterThanOrEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::UGE), x, y);
}

inline GateRef StubBuilder::Int64GreaterThan(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SGT), x, y);
}

inline GateRef StubBuilder::Int64LessThan(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SLT), x, y);
}

inline GateRef StubBuilder::Int64LessThanOrEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SLE), x, y);
}

inline GateRef StubBuilder::Int64GreaterThanOrEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::SGE), x, y);
}

inline GateRef StubBuilder::Int64UnsignedLessThanOrEqual(GateRef x, GateRef y)
{
    return env_->GetBuilder()->BinaryLogic(OpCode(OpCode::ULE), x, y);
}

inline GateRef StubBuilder::IntPtrGreaterThan(GateRef x, GateRef y)
{
    return env_->Is32Bit() ? Int32GreaterThan(x, y) : Int64GreaterThan(x, y);
}

// cast operation
inline GateRef StubBuilder::ChangeInt64ToInt32(GateRef val)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::TRUNC_TO_INT32), val);
}

inline GateRef StubBuilder::ChangeInt64ToIntPtr(GateRef val)
{
    if (env_->IsArch32Bit()) {
        return ChangeInt64ToInt32(val);
    }
    return val;
}

inline GateRef StubBuilder::ChangeInt32ToIntPtr(GateRef val)
{
    if (env_->IsArch32Bit()) {
        return val;
    }
    return ZExtInt32ToInt64(val);
}

inline GateRef StubBuilder::ChangeIntPtrToInt32(GateRef val)
{
    if (env_->IsArch32Bit()) {
        return val;
    }
    return ChangeInt64ToInt32(val);
}

inline GateRef StubBuilder::GetSetterFromAccessor(GateRef accessor)
{
    GateRef setterOffset = IntPtr(AccessorData::SETTER_OFFSET);
    return Load(VariableType::JS_ANY(), accessor, setterOffset);
}

inline GateRef StubBuilder::GetElementsArray(GateRef object)
{
    GateRef elementsOffset = IntPtr(JSObject::ELEMENTS_OFFSET);
    return Load(VariableType::JS_POINTER(), object, elementsOffset);
}

inline void StubBuilder::SetElementsArray(VariableType type, GateRef glue, GateRef object, GateRef elementsArray)
{
    GateRef elementsOffset = IntPtr(JSObject::ELEMENTS_OFFSET);
    Store(type, glue, object, elementsOffset, elementsArray);
}

inline GateRef StubBuilder::GetPropertiesArray(GateRef object)
{
    GateRef propertiesOffset = IntPtr(JSObject::PROPERTIES_OFFSET);
    return Load(VariableType::JS_POINTER(), object, propertiesOffset);
}

// SetProperties in js_object.h
inline void StubBuilder::SetPropertiesArray(VariableType type, GateRef glue, GateRef object, GateRef propsArray)
{
    GateRef propertiesOffset = IntPtr(JSObject::PROPERTIES_OFFSET);
    Store(type, glue, object, propertiesOffset, propsArray);
}

inline void StubBuilder::SetHash(GateRef glue, GateRef object, GateRef hash)
{
    GateRef hashOffset = IntPtr(ECMAObject::HASH_OFFSET);
    Store(VariableType::INT64(), glue, object, hashOffset, hash);
}

inline GateRef StubBuilder::GetLengthOfTaggedArray(GateRef array)
{
    return Load(VariableType::INT32(), array, IntPtr(TaggedArray::LENGTH_OFFSET));
}

inline GateRef StubBuilder::IsJSHClass(GateRef obj)
{
    return Int32Equal(GetObjectType(LoadHClass(obj)),  Int32(static_cast<int32_t>(JSType::HCLASS)));
}
// object operation
inline GateRef StubBuilder::LoadHClass(GateRef object)
{
    return Load(VariableType::JS_POINTER(), object);
}

inline void StubBuilder::StoreHClass(GateRef glue, GateRef object, GateRef hclass)
{
    Store(VariableType::JS_POINTER(), glue, object, IntPtr(0), hclass);
}

inline GateRef StubBuilder::GetObjectType(GateRef hClass)
{
    GateRef bitfieldOffset = IntPtr(JSHClass::BIT_FIELD_OFFSET);
    GateRef bitfield = Load(VariableType::INT32(), hClass, bitfieldOffset);
    return Int32And(bitfield, Int32((1LU << JSHClass::ObjectTypeBits::SIZE) - 1));
}

inline GateRef StubBuilder::IsDictionaryMode(GateRef object)
{
    GateRef objectType = GetObjectType(LoadHClass(object));
    return Int32Equal(objectType,
        Int32(static_cast<int32_t>(JSType::TAGGED_DICTIONARY)));
}

inline GateRef StubBuilder::IsDictionaryModeByHClass(GateRef hClass)
{
    GateRef bitfieldOffset = IntPtr(JSHClass::BIT_FIELD_OFFSET);
    GateRef bitfield = Load(VariableType::INT32(), hClass, bitfieldOffset);
    return Int32NotEqual(
        Int32And(
            Int32LSR(bitfield, Int32(JSHClass::IsDictionaryBit::START_BIT)),
            Int32((1LU << JSHClass::IsDictionaryBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsDictionaryElement(GateRef hClass)
{
    GateRef bitfieldOffset = IntPtr(JSHClass::BIT_FIELD_OFFSET);
    GateRef bitfield = Load(VariableType::INT32(), hClass, bitfieldOffset);
    // decode
    return Int32NotEqual(
        Int32And(
            Int32LSR(bitfield, Int32(JSHClass::DictionaryElementBits::START_BIT)),
            Int32((1LU << JSHClass::DictionaryElementBits::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsClassConstructorFromBitField(GateRef bitfield)
{
    // decode
    return Int32NotEqual(
        Int32And(Int32LSR(bitfield, Int32(JSHClass::ClassConstructorBit::START_BIT)),
                 Int32((1LU << JSHClass::ClassConstructorBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsClassConstructor(GateRef object)
{
    GateRef hClass = LoadHClass(object);
    GateRef bitfieldOffset = IntPtr(JSHClass::BIT_FIELD_OFFSET);

    GateRef bitfield = Load(VariableType::INT32(), hClass, bitfieldOffset);
    return IsClassConstructorFromBitField(bitfield);
}

inline GateRef StubBuilder::IsClassPrototype(GateRef object)
{
    GateRef hClass = LoadHClass(object);
    GateRef bitfieldOffset = IntPtr(JSHClass::BIT_FIELD_OFFSET);

    GateRef bitfield = Load(VariableType::INT32(), hClass, bitfieldOffset);
    // decode
    return Int32NotEqual(
        Int32And(Int32LSR(bitfield, Int32(JSHClass::ClassPrototypeBit::START_BIT)),
            Int32((1LU << JSHClass::ClassPrototypeBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsExtensible(GateRef object)
{
    GateRef hClass = LoadHClass(object);
    GateRef bitfieldOffset = IntPtr(JSHClass::BIT_FIELD_OFFSET);

    GateRef bitfield = Load(VariableType::INT32(), hClass, bitfieldOffset);
    // decode
    return Int32NotEqual(
        Int32And(Int32LSR(bitfield, Int32(JSHClass::ExtensibleBit::START_BIT)),
                 Int32((1LU << JSHClass::ExtensibleBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::TaggedObjectIsEcmaObject(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    auto ret = Int32And(
        ZExtInt1ToInt32(
            Int32LessThanOrEqual(objectType, Int32(static_cast<int32_t>(JSType::ECMA_OBJECT_END)))),
        ZExtInt1ToInt32(
            Int32GreaterThanOrEqual(objectType,
                Int32(static_cast<int32_t>(JSType::ECMA_OBJECT_BEGIN)))));
    return TruncInt32ToInt1(ret);
}

inline GateRef StubBuilder::IsJSObject(GateRef obj)
{
    auto env = GetEnvironment();
    Label subentry(env);
    env->SubCfgEntry(&subentry);
    Label exit(env);
    Label isHeapObject(env);
    DEFVARIABLE(result, VariableType::BOOL(), False());
    Branch(TaggedIsHeapObject(obj), &isHeapObject, &exit);
    Bind(&isHeapObject);
    {
        GateRef objectType = GetObjectType(LoadHClass(obj));
        auto ret1 = Int32And(
            ZExtInt1ToInt32(
                Int32LessThanOrEqual(objectType, Int32(static_cast<int32_t>(JSType::JS_OBJECT_END)))),
            ZExtInt1ToInt32(
                Int32GreaterThanOrEqual(objectType,
                    Int32(static_cast<int32_t>(JSType::JS_OBJECT_BEGIN)))));
        result = TruncInt32ToInt1(ret1);
        Jump(&exit);
    }
    Bind(&exit);
    auto ret = *result;
    env->SubCfgExit();
    return ret;
}

inline GateRef StubBuilder::IsJSFunctionBase(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    GateRef greater = ZExtInt1ToInt32(Int32GreaterThanOrEqual(objectType,
        Int32(static_cast<int32_t>(JSType::JS_FUNCTION_BASE))));
    GateRef less = ZExtInt1ToInt32(Int32LessThanOrEqual(objectType,
        Int32(static_cast<int32_t>(JSType::JS_BOUND_FUNCTION))));
    return TruncInt32ToInt1(Int32And(greater, less));
}

inline GateRef StubBuilder::IsConstructor(GateRef object)
{
    GateRef hClass = LoadHClass(object);
    GateRef bitfieldOffset = IntPtr(JSHClass::BIT_FIELD_OFFSET);

    GateRef bitfield = Load(VariableType::INT32(), hClass, bitfieldOffset);
    // decode
    return Int32NotEqual(
        Int32And(Int32LSR(bitfield, Int32(JSHClass::ConstructorBit::START_BIT)),
                 Int32((1LU << JSHClass::ConstructorBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsBase(GateRef func)
{
    GateRef bitfieldOffset = IntPtr(JSFunction::BIT_FIELD_OFFSET);
    GateRef bitfield = Load(VariableType::INT32(), func, bitfieldOffset);
    // decode
    return Int32LessThanOrEqual(
        Int32And(Int32LSR(bitfield, Int32(JSFunction::FunctionKindBits::START_BIT)),
                 Int32((1LU << JSFunction::FunctionKindBits::SIZE) - 1)),
        Int32(static_cast<int32_t>(FunctionKind::CLASS_CONSTRUCTOR)));
}

inline GateRef StubBuilder::IsSymbol(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    return Int32Equal(objectType, Int32(static_cast<int32_t>(JSType::SYMBOL)));
}

inline GateRef StubBuilder::IsString(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    return Int32Equal(objectType, Int32(static_cast<int32_t>(JSType::STRING)));
}

inline GateRef StubBuilder::IsBigInt(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    return Int32Equal(objectType, Int32(static_cast<int32_t>(JSType::BIGINT)));
}

inline GateRef StubBuilder::IsJsProxy(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    return Int32Equal(objectType, Int32(static_cast<int32_t>(JSType::JS_PROXY)));
}

inline GateRef StubBuilder::IsJsArray(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    return Int32Equal(objectType, Int32(static_cast<int32_t>(JSType::JS_ARRAY)));
}

inline GateRef StubBuilder::IsWritable(GateRef attr)
{
    return Int32NotEqual(
        Int32And(
            Int32LSR(attr, Int32(PropertyAttributes::WritableField::START_BIT)),
            Int32((1LLU << PropertyAttributes::WritableField::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsAccessor(GateRef attr)
{
    return Int32NotEqual(
        Int32And(Int32LSR(attr,
            Int32(PropertyAttributes::IsAccessorField::START_BIT)),
            Int32((1LLU << PropertyAttributes::IsAccessorField::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsInlinedProperty(GateRef attr)
{
    return Int32NotEqual(
        Int32And(Int32LSR(attr,
            Int32(PropertyAttributes::IsInlinedPropsField::START_BIT)),
            Int32((1LLU << PropertyAttributes::IsInlinedPropsField::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::GetProtoCell(GateRef object)
{
    GateRef protoCellOffset = IntPtr(PrototypeHandler::PROTO_CELL_OFFSET);
    return Load(VariableType::INT64(), object, protoCellOffset);
}

inline GateRef StubBuilder::GetPrototypeHandlerHolder(GateRef object)
{
    GateRef holderOffset = IntPtr(PrototypeHandler::HOLDER_OFFSET);
    return Load(VariableType::JS_ANY(), object, holderOffset);
}

inline GateRef StubBuilder::GetPrototypeHandlerHandlerInfo(GateRef object)
{
    GateRef handlerInfoOffset = IntPtr(PrototypeHandler::HANDLER_INFO_OFFSET);
    return Load(VariableType::JS_ANY(), object, handlerInfoOffset);
}

inline GateRef StubBuilder::GetHasChanged(GateRef object)
{
    GateRef bitfieldOffset = IntPtr(ProtoChangeMarker::BIT_FIELD_OFFSET);
    GateRef bitfield = Load(VariableType::INT32(), object, bitfieldOffset);
    GateRef mask = Int32(1LLU << (ProtoChangeMarker::HAS_CHANGED_BITS - 1));
    return Int32NotEqual(Int32And(bitfield, mask), Int32(0));
}

inline GateRef StubBuilder::HclassIsPrototypeHandler(GateRef hclass)
{
    return Int32Equal(GetObjectType(hclass),
        Int32(static_cast<int32_t>(JSType::PROTOTYPE_HANDLER)));
}

inline GateRef StubBuilder::HclassIsTransitionHandler(GateRef hclass)
{
    return Int32Equal(GetObjectType(hclass),
        Int32(static_cast<int32_t>(JSType::TRANSITION_HANDLER)));
}

inline GateRef StubBuilder::HclassIsPropertyBox(GateRef hclass)
{
    return Int32Equal(GetObjectType(hclass),
        Int32(static_cast<int32_t>(JSType::PROPERTY_BOX)));
}

inline GateRef StubBuilder::IsField(GateRef attr)
{
    return Int32Equal(
        Int32And(
            Int32LSR(attr, Int32(HandlerBase::KindBit::START_BIT)),
            Int32((1LLU << HandlerBase::KindBit::SIZE) - 1)),
        Int32(HandlerBase::HandlerKind::FIELD));
}

inline GateRef StubBuilder::IsNonExist(GateRef attr)
{
    return Int32Equal(
        Int32And(
            Int32LSR(attr, Int32(HandlerBase::KindBit::START_BIT)),
            Int32((1LLU << HandlerBase::KindBit::SIZE) - 1)),
        Int32(HandlerBase::HandlerKind::NON_EXIST));
}

inline GateRef StubBuilder::HandlerBaseIsAccessor(GateRef attr)
{
    return Int32NotEqual(
        Int32And(Int32LSR(attr,
            Int32(HandlerBase::AccessorBit::START_BIT)),
            Int32((1LLU << HandlerBase::AccessorBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::HandlerBaseIsJSArray(GateRef attr)
{
    return Int32NotEqual(
        Int32And(Int32LSR(attr,
            Int32(HandlerBase::IsJSArrayBit::START_BIT)),
            Int32((1LLU << HandlerBase::IsJSArrayBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::HandlerBaseIsInlinedProperty(GateRef attr)
{
    return Int32NotEqual(
        Int32And(Int32LSR(attr,
            Int32(HandlerBase::InlinedPropsBit::START_BIT)),
            Int32((1LLU << HandlerBase::InlinedPropsBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::HandlerBaseGetOffset(GateRef attr)
{
    return Int32And(Int32LSR(attr,
        Int32(HandlerBase::OffsetBit::START_BIT)),
        Int32((1LLU << HandlerBase::OffsetBit::SIZE) - 1));
}

inline GateRef StubBuilder::IsInternalAccessor(GateRef attr)
{
    return Int32NotEqual(
        Int32And(Int32LSR(attr,
            Int32(HandlerBase::InternalAccessorBit::START_BIT)),
            Int32((1LLU << HandlerBase::InternalAccessorBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsInvalidPropertyBox(GateRef obj)
{
    GateRef valueOffset = IntPtr(PropertyBox::VALUE_OFFSET);
    GateRef value = Load(VariableType::INT64(), obj, valueOffset);
    return TaggedIsHole(value);
}

inline GateRef StubBuilder::GetValueFromPropertyBox(GateRef obj)
{
    GateRef valueOffset = IntPtr(PropertyBox::VALUE_OFFSET);
    return Load(VariableType::JS_ANY(), obj, valueOffset);
}

inline void StubBuilder::SetValueToPropertyBox(GateRef glue, GateRef obj, GateRef value)
{
    GateRef valueOffset = IntPtr(PropertyBox::VALUE_OFFSET);
    Store(VariableType::JS_ANY(), glue, obj, valueOffset, value);
}

inline GateRef StubBuilder::GetTransitionFromHClass(GateRef obj)
{
    GateRef transitionHClassOffset = IntPtr(TransitionHandler::TRANSITION_HCLASS_OFFSET);
    return Load(VariableType::JS_POINTER(), obj, transitionHClassOffset);
}

inline GateRef StubBuilder::GetTransitionHandlerInfo(GateRef obj)
{
    GateRef handlerInfoOffset = IntPtr(TransitionHandler::HANDLER_INFO_OFFSET);
    return Load(VariableType::JS_ANY(), obj, handlerInfoOffset);
}

inline GateRef StubBuilder::PropAttrGetOffset(GateRef attr)
{
    return Int32And(
        Int32LSR(attr, Int32(PropertyAttributes::OffsetField::START_BIT)),
        Int32((1LLU << PropertyAttributes::OffsetField::SIZE) - 1));
}

// SetDictionaryOrder func in property_attribute.h
inline GateRef StubBuilder::SetDictionaryOrderFieldInPropAttr(GateRef attr, GateRef value)
{
    GateRef mask = Int32LSL(
        Int32((1LLU << PropertyAttributes::DictionaryOrderField::SIZE) - 1),
        Int32(PropertyAttributes::DictionaryOrderField::START_BIT));
    GateRef newVal = Int32Or(Int32And(attr, Int32Not(mask)),
        Int32LSL(value, Int32(PropertyAttributes::DictionaryOrderField::START_BIT)));
    return newVal;
}

inline GateRef StubBuilder::GetPrototypeFromHClass(GateRef hClass)
{
    GateRef protoOffset = IntPtr(JSHClass::PROTOTYPE_OFFSET);
    return Load(VariableType::JS_ANY(), hClass, protoOffset);
}

inline GateRef StubBuilder::GetLayoutFromHClass(GateRef hClass)
{
    GateRef attrOffset = IntPtr(JSHClass::LAYOUT_OFFSET);
    return Load(VariableType::JS_POINTER(), hClass, attrOffset);
}

inline GateRef StubBuilder::GetBitFieldFromHClass(GateRef hClass)
{
    GateRef offset = IntPtr(JSHClass::BIT_FIELD_OFFSET);
    return Load(VariableType::INT32(), hClass, offset);
}

inline GateRef StubBuilder::GetLengthFromString(GateRef value)
{
    GateRef len = Load(VariableType::INT32(), value, IntPtr(EcmaString::MIX_LENGTH_OFFSET));
    return Int32LSR(len, Int32(2));  // 2 : 2 means len must be right shift 2 bits
}

inline void StubBuilder::SetBitFieldToHClass(GateRef glue, GateRef hClass, GateRef bitfield)
{
    GateRef offset = IntPtr(JSHClass::BIT_FIELD_OFFSET);
    Store(VariableType::INT32(), glue, hClass, offset, bitfield);
}

inline void StubBuilder::SetPrototypeToHClass(VariableType type, GateRef glue, GateRef hClass, GateRef proto)
{
    GateRef offset = IntPtr(JSHClass::PROTOTYPE_OFFSET);
    Store(type, glue, hClass, offset, proto);
}

inline void StubBuilder::SetProtoChangeDetailsToHClass(VariableType type, GateRef glue, GateRef hClass, GateRef protoChange)
{
    GateRef offset = IntPtr(JSHClass::PROTO_CHANGE_DETAILS_OFFSET);
    Store(type, glue, hClass, offset, protoChange);
}

inline void StubBuilder::SetLayoutToHClass(VariableType type, GateRef glue, GateRef hClass, GateRef attr)
{
    GateRef offset = IntPtr(JSHClass::LAYOUT_OFFSET);
    Store(type, glue, hClass, offset, attr);
}

inline void StubBuilder::SetEnumCacheToHClass(VariableType type, GateRef glue, GateRef hClass, GateRef key)
{
    GateRef offset = IntPtr(JSHClass::ENUM_CACHE_OFFSET);
    Store(type, glue, hClass, offset, key);
}

inline void StubBuilder::SetTransitionsToHClass(VariableType type, GateRef glue, GateRef hClass, GateRef transition)
{
    GateRef offset = IntPtr(JSHClass::TRANSTIONS_OFFSET);
    Store(type, glue, hClass, offset, transition);
}

inline void StubBuilder::SetIsProtoTypeToHClass(GateRef glue, GateRef hClass, GateRef value)
{
    GateRef oldValue = ZExtInt1ToInt32(value);
    GateRef bitfield = GetBitFieldFromHClass(hClass);
    GateRef mask = Int32LSL(
        Int32((1LU << JSHClass::IsPrototypeBit::SIZE) - 1),
        Int32(JSHClass::IsPrototypeBit::START_BIT));
    GateRef newVal = Int32Or(Int32And(bitfield, Int32Not(mask)),
        Int32LSL(oldValue, Int32(JSHClass::IsPrototypeBit::START_BIT)));
    SetBitFieldToHClass(glue, hClass, newVal);
}

inline GateRef StubBuilder::IsProtoTypeHClass(GateRef hClass)
{
    GateRef bitfield = GetBitFieldFromHClass(hClass);
    return TruncInt32ToInt1(Int32And(Int32LSR(bitfield,
        Int32(JSHClass::IsPrototypeBit::START_BIT)),
        Int32((1LU << JSHClass::IsPrototypeBit::SIZE) - 1)));
}

inline void StubBuilder::SetPropertyInlinedProps(GateRef glue, GateRef obj, GateRef hClass,
    GateRef value, GateRef attrOffset, VariableType type)
{
    GateRef bitfield = Load(VariableType::INT32(), hClass,
                            IntPtr(JSHClass::BIT_FIELD1_OFFSET));
    GateRef inlinedPropsStart = Int32And(Int32LSR(bitfield,
        Int32(JSHClass::InlinedPropsStartBits::START_BIT)),
        Int32((1LU << JSHClass::InlinedPropsStartBits::SIZE) - 1));
    GateRef propOffset = Int32Mul(
        Int32Add(inlinedPropsStart, attrOffset), Int32(JSTaggedValue::TaggedTypeSize()));

    // NOTE: need to translate MarkingBarrier
    Store(type, glue, obj, ChangeInt32ToIntPtr(propOffset), value);
}

inline void StubBuilder::IncNumberOfProps(GateRef glue, GateRef hClass)
{
    GateRef propNums = GetNumberOfPropsFromHClass(hClass);
    SetNumberOfPropsToHClass(glue, hClass, Int32Add(propNums, Int32(1)));
}

inline GateRef StubBuilder::GetNumberOfPropsFromHClass(GateRef hClass)
{
    GateRef bitfield = Load(VariableType::INT32(), hClass, IntPtr(JSHClass::BIT_FIELD1_OFFSET));
    return Int32And(Int32LSR(bitfield,
        Int32(JSHClass::NumberOfPropsBits::START_BIT)),
        Int32((1LLU << JSHClass::NumberOfPropsBits::SIZE) - 1));
}

inline void StubBuilder::SetNumberOfPropsToHClass(GateRef glue, GateRef hClass, GateRef value)
{
    GateRef bitfield1 = Load(VariableType::INT32(), hClass, IntPtr(JSHClass::BIT_FIELD1_OFFSET));
    GateRef oldWithMask = Int32And(bitfield1,
        Int32(~static_cast<uint32_t>(JSHClass::NumberOfPropsBits::Mask())));
    GateRef newValue = Int32LSR(value, Int32(JSHClass::NumberOfPropsBits::START_BIT));
    Store(VariableType::INT32(), glue, hClass, IntPtr(JSHClass::BIT_FIELD1_OFFSET),
        Int32Or(oldWithMask, newValue));
}

inline GateRef StubBuilder::GetInlinedPropertiesFromHClass(GateRef hClass)
{
    GateRef bitfield = Load(VariableType::INT32(), hClass, IntPtr(JSHClass::BIT_FIELD1_OFFSET));
    GateRef objectSizeInWords = Int32And(Int32LSR(bitfield,
        Int32(JSHClass::ObjectSizeInWordsBits::START_BIT)),
        Int32((1LU << JSHClass::ObjectSizeInWordsBits::SIZE) - 1));
    GateRef inlinedPropsStart = Int32And(Int32LSR(bitfield,
        Int32(JSHClass::InlinedPropsStartBits::START_BIT)),
        Int32((1LU << JSHClass::InlinedPropsStartBits::SIZE) - 1));
    return Int32Sub(objectSizeInWords, inlinedPropsStart);
}

inline GateRef StubBuilder::GetObjectSizeFromHClass(GateRef hClass) // NOTE: check for special case of string and TAGGED_ARRAY
{
    GateRef bitfield = Load(VariableType::INT32(), hClass, IntPtr(JSHClass::BIT_FIELD1_OFFSET));
    GateRef objectSizeInWords = Int32And(Int32LSR(bitfield,
        Int32(JSHClass::ObjectSizeInWordsBits::START_BIT)),
        Int32((1LU << JSHClass::ObjectSizeInWordsBits::SIZE) - 1));
    return PtrMul(ChangeInt32ToIntPtr(objectSizeInWords),
        IntPtr(JSTaggedValue::TaggedTypeSize()));
}

inline GateRef StubBuilder::GetInlinedPropsStartFromHClass(GateRef hClass)
{
    GateRef bitfield = Load(VariableType::INT32(), hClass, IntPtr(JSHClass::BIT_FIELD1_OFFSET));
    return Int32And(Int32LSR(bitfield,
        Int32(JSHClass::InlinedPropsStartBits::START_BIT)),
        Int32((1LU << JSHClass::InlinedPropsStartBits::SIZE) - 1));
}

inline void StubBuilder::SetValueToTaggedArray(VariableType valType, GateRef glue, GateRef array, GateRef index, GateRef val)
{
    // NOTE: need to translate MarkingBarrier
    GateRef offset =
        PtrMul(ChangeInt32ToIntPtr(index), IntPtr(JSTaggedValue::TaggedTypeSize()));
    GateRef dataOffset = PtrAdd(offset, IntPtr(TaggedArray::DATA_OFFSET));
    Store(valType, glue, array, dataOffset, val);
}

inline GateRef StubBuilder::GetValueFromTaggedArray(VariableType returnType, GateRef array, GateRef index)
{
    GateRef offset =
        PtrMul(ChangeInt32ToIntPtr(index), IntPtr(JSTaggedValue::TaggedTypeSize()));
    GateRef dataOffset = PtrAdd(offset, IntPtr(TaggedArray::DATA_OFFSET));
    return Load(returnType, array, dataOffset);
}

inline GateRef StubBuilder::IsSpecialIndexedObj(GateRef jsType)
{
    return Int32GreaterThan(jsType, Int32(static_cast<int32_t>(JSType::JS_ARRAY)));
}

inline GateRef StubBuilder::IsSpecialContainer(GateRef jsType)
{
    // arraylist and vector has fast pass now
    return TruncInt32ToInt1(Int32And(
        ZExtInt1ToInt32(
            Int32Equal(jsType, Int32(static_cast<int32_t>(JSType::JS_API_ARRAY_LIST)))),
        ZExtInt1ToInt32(Int32Equal(jsType, Int32(static_cast<int32_t>(JSType::JS_API_VECTOR))))));
}

inline GateRef StubBuilder::IsFastTypeArray(GateRef jsType)
{
    return TruncInt32ToInt1(Int32And(
        ZExtInt1ToInt32(
            Int32GreaterThanOrEqual(jsType, Int32(static_cast<int32_t>(JSType::JS_TYPED_ARRAY_BEGIN)))),
        ZExtInt1ToInt32(Int32LessThanOrEqual(jsType, Int32(static_cast<int32_t>(JSType::JS_FLOAT64_ARRAY))))));
}

inline GateRef StubBuilder::IsAccessorInternal(GateRef value)
{
    return Int32Equal(GetObjectType(LoadHClass(value)),
                      Int32(static_cast<int32_t>(JSType::INTERNAL_ACCESSOR)));
}

inline GateRef StubBuilder::GetPropAttrFromLayoutInfo(GateRef layout, GateRef entry)
{
    GateRef index = Int32Add(Int32Add(Int32(LayoutInfo::ELEMENTS_START_INDEX),
        Int32LSL(entry, Int32(1))), Int32(1));
    return GetValueFromTaggedArray(VariableType::INT64(), layout, index);
}

inline GateRef StubBuilder::GetPropertyMetaDataFromAttr(GateRef attr)
{
    return Int32And(Int32LSR(attr, Int32(PropertyAttributes::PropertyMetaDataField::START_BIT)),
        Int32((1LLU << PropertyAttributes::PropertyMetaDataField::SIZE) - 1));
}

inline GateRef StubBuilder::GetKeyFromLayoutInfo(GateRef layout, GateRef entry)
{
    GateRef index = Int32Add(
        Int32(LayoutInfo::ELEMENTS_START_INDEX),
        Int32LSL(entry, Int32(1)));
    return GetValueFromTaggedArray(VariableType::JS_ANY(), layout, index);
}

inline GateRef StubBuilder::GetPropertiesAddrFromLayoutInfo(GateRef layout)
{
    GateRef eleStartIdx = PtrMul(IntPtr(LayoutInfo::ELEMENTS_START_INDEX),
        IntPtr(JSTaggedValue::TaggedTypeSize()));
    return PtrAdd(layout, PtrAdd(IntPtr(TaggedArray::DATA_OFFSET), eleStartIdx));
}

inline GateRef StubBuilder::TaggedCastToInt64(GateRef x)
{
    GateRef tagged = ChangeTaggedPointerToInt64(x);
    return Int64And(tagged, Int64(~JSTaggedValue::TAG_MARK));
}

inline GateRef StubBuilder::TaggedCastToInt32(GateRef x)
{
    return ChangeInt64ToInt32(TaggedCastToInt64(x));
}

inline GateRef StubBuilder::TaggedCastToIntPtr(GateRef x)
{
    return env_->Is32Bit() ? ChangeInt64ToInt32(TaggedCastToInt64(x)) : TaggedCastToInt64(x);
}

inline GateRef StubBuilder::TaggedCastToDouble(GateRef x)
{
    GateRef tagged = ChangeTaggedPointerToInt64(x);
    GateRef val = Int64Sub(tagged, Int64(JSTaggedValue::DOUBLE_ENCODE_OFFSET));
    return CastInt64ToFloat64(val);
}

inline GateRef StubBuilder::TaggedCastToWeakReferentUnChecked(GateRef x)
{
    x = ChangeTaggedPointerToInt64(x);
    return Int64And(x, Int64(~JSTaggedValue::TAG_WEAK));
}

inline GateRef StubBuilder::ChangeInt32ToFloat64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::SIGNED_INT_TO_FLOAT), MachineType::F64, x);
}

inline GateRef StubBuilder::ChangeUInt32ToFloat64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::UNSIGNED_INT_TO_FLOAT), MachineType::F64, x);
}

inline GateRef StubBuilder::ChangeFloat64ToInt32(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::FLOAT_TO_SIGNED_INT), MachineType::I32, x);
}

inline GateRef StubBuilder::ChangeTaggedPointerToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::TAGGED_TO_INT64), x);
}

inline GateRef StubBuilder::ChangeInt64ToTagged(GateRef x)
{
    return env_->GetBuilder()->TaggedNumber(OpCode(OpCode::INT64_TO_TAGGED), x);
}

inline GateRef StubBuilder::CastInt64ToFloat64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::BITCAST), MachineType::F64, x);
}

inline GateRef StubBuilder::SExtInt32ToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::SEXT_TO_INT64), x);
}

inline GateRef StubBuilder::SExtInt16ToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::SEXT_TO_INT64), x);
}

inline GateRef StubBuilder::SExtInt8ToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::SEXT_TO_INT64), x);
}

inline GateRef StubBuilder::SExtInt1ToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::SEXT_TO_INT64), x);
}

inline GateRef StubBuilder::SExtInt1ToInt32(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::SEXT_TO_INT32), x);
}

inline GateRef StubBuilder::ZExtInt8ToInt16(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT16), x);
}

inline GateRef StubBuilder::ZExtInt32ToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT64), x);
}

inline GateRef StubBuilder::ZExtInt1ToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT64), x);
}

inline GateRef StubBuilder::ZExtInt1ToInt32(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT32), x);
}

inline GateRef StubBuilder::ZExtInt8ToInt32(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT32), x);
}

inline GateRef StubBuilder::ZExtInt8ToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT64), x);
}

inline GateRef StubBuilder::ZExtInt8ToPtr(GateRef x)
{
    if (env_->IsArch32Bit()) {
        return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT32), x);
    }
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT64), x);
}

inline GateRef StubBuilder::ZExtInt16ToPtr(GateRef x)
{
    if (env_->IsArch32Bit()) {
        return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT32), x);
    }
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT64), x);
}

inline GateRef StubBuilder::SExtInt32ToPtr(GateRef x)
{
    if (env_->IsArch32Bit()) {
        return x;
    }
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::SEXT_TO_INT64), x);
}

inline GateRef StubBuilder::ZExtInt16ToInt32(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT32), x);
}

inline GateRef StubBuilder::ZExtInt16ToInt64(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::ZEXT_TO_INT64), x);
}

inline GateRef StubBuilder::TruncInt64ToInt32(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::TRUNC_TO_INT32), x);
}

inline GateRef StubBuilder::TruncPtrToInt32(GateRef x)
{
    if (env_->Is32Bit()) {
        return x;
    }
    return TruncInt64ToInt32(x);
}

inline GateRef StubBuilder::TruncInt64ToInt1(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::TRUNC_TO_INT1), x);
}

inline GateRef StubBuilder::TruncInt32ToInt1(GateRef x)
{
    return env_->GetBuilder()->UnaryArithmetic(OpCode(OpCode::TRUNC_TO_INT1), x);
}

inline GateRef StubBuilder::GetGlobalConstantAddr(GateRef index)
{
    return Int64Mul(Int64(sizeof(JSTaggedValue)), index);
}

inline GateRef StubBuilder::GetGlobalConstantString(ConstantIndex index)
{
    if (env_->Is32Bit()) {
        return Int32Mul(Int32(sizeof(JSTaggedValue)), Int32(static_cast<int>(index)));
    } else {
        return Int64Mul(Int64(sizeof(JSTaggedValue)), Int64(static_cast<int>(index)));
    }
}

inline GateRef StubBuilder::IsCallableFromBitField(GateRef bitfield)
{
    return Int32NotEqual(
        Int32And(Int32LSR(bitfield, Int32(JSHClass::CallableBit::START_BIT)),
            Int32((1LU << JSHClass::CallableBit::SIZE) - 1)),
        Int32(0));
}

inline GateRef StubBuilder::IsCallable(GateRef obj)
{
    GateRef hclass = LoadHClass(obj);
    GateRef bitfieldOffset = IntPtr(JSHClass::BIT_FIELD_OFFSET);
    GateRef bitfield = Load(VariableType::INT32(), hclass, bitfieldOffset);
    return IsCallableFromBitField(bitfield);
}

// GetOffset func in property_attribute.h
inline GateRef StubBuilder::GetOffsetFieldInPropAttr(GateRef attr)
{
    return Int32And(
        Int32LSR(attr, Int32(PropertyAttributes::OffsetField::START_BIT)),
        Int32((1LLU << PropertyAttributes::OffsetField::SIZE) - 1));
}

// SetOffset func in property_attribute.h
inline GateRef StubBuilder::SetOffsetFieldInPropAttr(GateRef attr, GateRef value)
{
    GateRef mask = Int32LSL(
        Int32((1LLU << PropertyAttributes::OffsetField::SIZE) - 1),
        Int32(PropertyAttributes::OffsetField::START_BIT));
    GateRef newVal = Int32Or(Int32And(attr, Int32Not(mask)),
        Int32LSL(value, Int32(PropertyAttributes::OffsetField::START_BIT)));
    return newVal;
}

// SetIsInlinedProps func in property_attribute.h
inline GateRef StubBuilder::SetIsInlinePropsFieldInPropAttr(GateRef attr, GateRef value)
{
    GateRef mask = Int32LSL(
        Int32((1LU << PropertyAttributes::IsInlinedPropsField::SIZE) - 1),
        Int32(PropertyAttributes::IsInlinedPropsField::START_BIT));
    GateRef newVal = Int32Or(Int32And(attr, Int32Not(mask)),
        Int32LSL(value, Int32(PropertyAttributes::IsInlinedPropsField::START_BIT)));
    return newVal;
}

inline void StubBuilder::SetHasConstructorToHClass(GateRef glue, GateRef hClass, GateRef value)
{
    GateRef bitfield = Load(VariableType::INT32(), hClass, IntPtr(JSHClass::BIT_FIELD_OFFSET));
    GateRef mask = Int32LSL(
        Int32((1LU << JSHClass::HasConstructorBits::SIZE) - 1),
        Int32(JSHClass::HasConstructorBits::START_BIT));
    GateRef newVal = Int32Or(Int32And(bitfield, Int32Not(mask)),
        Int32LSL(value, Int32(JSHClass::HasConstructorBits::START_BIT)));
    Store(VariableType::INT32(), glue, hClass, IntPtr(JSHClass::BIT_FIELD_OFFSET), newVal);
}

inline GateRef StubBuilder::IntPtrEuqal(GateRef x, GateRef y)
{
    return env_->Is32Bit() ? Int32Equal(x, y) : Int64Equal(x, y);
}

inline GateRef StubBuilder::GetBitMask(GateRef bitoffset)
{
    // BIT_PER_WORD_MASK
    GateRef bitPerWordMask = Int32(GCBitset::BIT_PER_WORD_MASK);
    // IndexInWord(bitOffset) = bitOffset & BIT_PER_WORD_MASK
    GateRef indexInWord = Int32And(bitoffset, bitPerWordMask);
    // Mask(indeInWord) = 1 << index
    return Int32LSL(Int32(1), indexInWord);
}

inline GateRef StubBuilder::ObjectAddressToRange(GateRef x)
{
    return IntPtrAnd(TaggedCastToIntPtr(x), IntPtr(~panda::ecmascript::DEFAULT_REGION_MASK));
}

inline GateRef StubBuilder::InYoungGeneration(GateRef region)
{
    auto offset = Region::PackedData::GetFlagOffset(env_->Is32Bit());
    GateRef x = Load(VariableType::NATIVE_POINTER(), PtrAdd(IntPtr(offset), region),
        IntPtr(0));
    if (env_->Is32Bit()) {
        return Int32Equal(Int32And(x,
            Int32(RegionSpaceFlag::VALID_SPACE_MASK)), Int32(RegionSpaceFlag::IN_YOUNG_SPACE));
    } else {
        return Int64Equal(Int64And(x,
            Int64(RegionSpaceFlag::VALID_SPACE_MASK)), Int64(RegionSpaceFlag::IN_YOUNG_SPACE));
    }
}

inline GateRef StubBuilder::GetParentEnv(GateRef object)
{
    GateRef index = Int32(LexicalEnv::PARENT_ENV_INDEX);
    return GetValueFromTaggedArray(VariableType::JS_ANY(), object, index);
}

inline GateRef StubBuilder::GetPropertiesFromLexicalEnv(GateRef object, GateRef index)
{
    GateRef valueIndex = Int32Add(index, Int32(LexicalEnv::RESERVED_ENV_LENGTH));
    return GetValueFromTaggedArray(VariableType::JS_ANY(), object, valueIndex);
}

inline void StubBuilder::SetPropertiesToLexicalEnv(GateRef glue, GateRef object, GateRef index, GateRef value)
{
    GateRef valueIndex = Int32Add(index, Int32(LexicalEnv::RESERVED_ENV_LENGTH));
    SetValueToTaggedArray(VariableType::JS_ANY(), glue, object, valueIndex, value);
}

inline GateRef StubBuilder::GetFunctionBitFieldFromJSFunction(GateRef object)
{
    GateRef offset = IntPtr(JSFunction::BIT_FIELD_OFFSET);
    return Load(VariableType::INT32(), object, offset);
}

inline GateRef StubBuilder::GetHomeObjectFromJSFunction(GateRef object)
{
    GateRef offset = IntPtr(JSFunction::HOME_OBJECT_OFFSET);
    return Load(VariableType::JS_ANY(), object, offset);
}

inline GateRef StubBuilder::GetMethodFromJSFunction(GateRef object)
{
    auto env = GetEnvironment();
    Label subentry(env);
    env->SubCfgEntry(&subentry);

    DEFVARIABLE(methodOffset, VariableType::INT32(), Int32(0));
    Label funcIsJSFunctionBase(env);
    Label funcIsJSProxy(env);
    Label getMethod(env);
    Branch(IsJSFunctionBase(object), &funcIsJSFunctionBase, &funcIsJSProxy);
    Bind(&funcIsJSFunctionBase);
    {
        methodOffset = Int32(JSFunctionBase::METHOD_OFFSET);
        Jump(&getMethod);
    }
    Bind(&funcIsJSProxy);
    {
        methodOffset = Int32(JSProxy::METHOD_OFFSET);
        Jump(&getMethod);
    }
    Bind(&getMethod);
    GateRef method = Load(VariableType::NATIVE_POINTER(), object, ChangeInt32ToIntPtr(*methodOffset));
    env->SubCfgExit();
    return method;
}

inline GateRef StubBuilder::GetCallFieldFromMethod(GateRef method)
{
    GateRef callFieldOffset = IntPtr(JSMethod::GetCallFieldOffset(env_->IsArch32Bit()));
    return Load(VariableType::INT64(), method, callFieldOffset);
}

inline void StubBuilder::SetLexicalEnvToFunction(GateRef glue, GateRef object, GateRef lexicalEnv)
{
    GateRef offset = IntPtr(JSFunction::LEXICAL_ENV_OFFSET);
    Store(VariableType::JS_ANY(), glue, object, offset, lexicalEnv);
}

inline GateRef StubBuilder::GetGlobalObject(GateRef glue)
{
    GateRef offset = IntPtr(JSThread::GlueData::GetGlobalObjOffset(env_->Is32Bit()));
    return Load(VariableType::JS_ANY(), glue, offset);
}

inline GateRef StubBuilder::GetEntryIndexOfGlobalDictionary(GateRef entry)
{
    return Int32Add(Int32(OrderTaggedHashTable<GlobalDictionary>::TABLE_HEADER_SIZE),
        Int32Mul(entry, Int32(GlobalDictionary::ENTRY_SIZE)));
}

inline GateRef StubBuilder::GetBoxFromGlobalDictionary(GateRef object, GateRef entry)
{
    GateRef index = GetEntryIndexOfGlobalDictionary(entry);
    GateRef offset = PtrAdd(ChangeInt32ToIntPtr(index),
        IntPtr(GlobalDictionary::ENTRY_VALUE_INDEX));
    return Load(VariableType::JS_POINTER(), object, offset);
}

inline GateRef StubBuilder::GetValueFromGlobalDictionary(GateRef object, GateRef entry)
{
    GateRef box = GetBoxFromGlobalDictionary(object, entry);
    return Load(VariableType::JS_ANY(), box, IntPtr(PropertyBox::VALUE_OFFSET));
}

inline GateRef StubBuilder::GetPropertiesFromJSObject(GateRef object)
{
    GateRef offset = IntPtr(JSObject::PROPERTIES_OFFSET);
    return Load(VariableType::JS_ANY(), object, offset);
}

inline GateRef StubBuilder::IsJSFunction(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    GateRef greater = ZExtInt1ToInt32(Int32GreaterThanOrEqual(objectType,
        Int32(static_cast<int32_t>(JSType::JS_FUNCTION_BEGIN))));
    GateRef less = ZExtInt1ToInt32(Int32LessThanOrEqual(objectType,
        Int32(static_cast<int32_t>(JSType::JS_FUNCTION_END))));
    return TruncInt32ToInt1(Int32And(greater, less));
}

inline GateRef StubBuilder::IsBoundFunction(GateRef obj)
{
    GateRef objectType = GetObjectType(LoadHClass(obj));
    return Int32Equal(objectType, Int32(static_cast<int32_t>(JSType::JS_BOUND_FUNCTION)));
}

inline GateRef StubBuilder::IsNativeMethod(GateRef method)
{
    GateRef callFieldOffset = IntPtr(JSMethod::GetCallFieldOffset(env_->Is32Bit()));
    GateRef callfield = Load(VariableType::INT64(), method, callFieldOffset);
    return Int64NotEqual(
        Int64And(
            Int64LSR(callfield, Int32(JSMethod::IsNativeBit::START_BIT)),
            Int64((1LU << JSMethod::IsNativeBit::SIZE) - 1)),
        Int64(0));
}

inline GateRef StubBuilder::HasAotCode(GateRef method)
{
    GateRef callFieldOffset = IntPtr(JSMethod::GetCallFieldOffset(env_->Is32Bit()));
    GateRef callfield = Load(VariableType::INT64(), method, callFieldOffset);
    return Int64NotEqual(
        Int64And(
            Int64LSR(callfield, Int32(JSMethod::IsAotCodeBit::START_BIT)),
            Int64((1LU << JSMethod::IsAotCodeBit::SIZE) - 1)),
        Int64(0));
}

inline GateRef StubBuilder::GetExpectedNumOfArgs(GateRef method)
{
    GateRef callFieldOffset = IntPtr(JSMethod::GetCallFieldOffset(env_->Is32Bit()));
    GateRef callfield = Load(VariableType::INT64(), method, callFieldOffset);
    return TruncInt64ToInt32(Int64And(
        Int64LSR(callfield, Int32(JSMethod::NumArgsBits::START_BIT)),
        Int64((1LU << JSMethod::NumArgsBits::SIZE) - 1)));
}

inline GateRef StubBuilder::GetMethodFromJSProxy(GateRef proxy)
{
    GateRef offset = IntPtr(JSProxy::METHOD_OFFSET);
    return Load(VariableType::JS_ANY(), proxy, offset);
}

inline GateRef StubBuilder::GetHandlerFromJSProxy(GateRef proxy)
{
    GateRef offset = IntPtr(JSProxy::HANDLER_OFFSET);
    return Load(VariableType::JS_ANY(), proxy, offset);
}

inline GateRef StubBuilder::GetTargetFromJSProxy(GateRef proxy)
{
    GateRef offset = IntPtr(JSProxy::TARGET_OFFSET);
    return Load(VariableType::JS_ANY(), proxy, offset);
}

inline GateRef StubBuilder::ComputeTaggedArraySize(GateRef length)
{
    return PtrAdd(IntPtr(TaggedArray::DATA_OFFSET),
        PtrMul(IntPtr(JSTaggedValue::TaggedTypeSize()), length));
}
inline GateRef StubBuilder::GetGlobalConstantValue(VariableType type, GateRef glue, ConstantIndex index)
{
    GateRef gConstAddr = PtrAdd(glue,
        IntPtr(JSThread::GlueData::GetGlobalConstOffset(env_->Is32Bit())));
    auto constantIndex = IntPtr(JSTaggedValue::TaggedTypeSize() * static_cast<size_t>(index));
    return Load(type, gConstAddr, constantIndex);
}

inline GateRef StubBuilder::HasPendingException(GateRef glue)
{
    GateRef exceptionOffset = IntPtr(JSThread::GlueData::GetExceptionOffset(env_->IsArch32Bit()));
    GateRef exception = Load(VariableType::JS_ANY(), glue, exceptionOffset);
    return Int64NotEqual(exception, Int64(JSTaggedValue::VALUE_HOLE));
}
} //  namespace panda::ecmascript::kungfu
#endif // ECMASCRIPT_COMPILER_STUB_INL_H
