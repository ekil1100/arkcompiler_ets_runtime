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

#include "ecmascript/compiler/type_lowering.h"

namespace panda::ecmascript::kungfu {
void TypeLowering::RunTypeLowering()
{
    std::vector<GateRef> gateList;
    circuit_->GetAllGates(gateList);
    for (const auto &gate : gateList) {
        auto op = acc_.GetOpCode(gate);
        if (op == OpCode::JS_BYTECODE) {
            Lower(gate);
        }
    }

    if (IsLogEnabled()) {
        LOG_COMPILER(INFO) << "================== type lowering print all gates Start==================";
        circuit_->PrintAllGates(*bcBuilder_);
    }
}

void TypeLowering::Lower(GateRef gate)
{
    ArgumentAccessor argAcc(circuit_);
    auto glue = argAcc.GetCommonArgGate(CommonArgIdx::GLUE);

    auto pc = bcBuilder_->GetJSBytecode(gate);
    EcmaOpcode op = static_cast<EcmaOpcode>(*pc);
    // initialize label manager
    Environment env(gate, circuit_, &builder_);
    switch (op) {
        case NEWOBJDYNRANGE_PREF_IMM16_V8:
            LowerTypeNewObjDynRange(gate, glue);
            break;
        case ADD2DYN_PREF_V8:
            LowerTypeAdd2Dyn(gate, glue);
            break;
        case SUB2DYN_PREF_V8:
            LowerTypeSub2Dyn(gate);
            break;
        case MUL2DYN_PREF_V8:
            LowerTypeMul2Dyn(gate);
            break;
        case MOD2DYN_PREF_V8:
            LowerTypeMod2Dyn(gate, glue);
            break;
        case LESSDYN_PREF_V8:
            LowerTypeLessDyn(gate);
            break;
        case LESSEQDYN_PREF_V8:
            LowerTypeLessEqDyn(gate);
            break;
        case GREATERDYN_PREF_V8:
            LowerTypeGreaterDyn(gate);
            break;
        case GREATEREQDYN_PREF_V8:
            LowerTypeGreaterEqDyn(gate);
            break;
        default:break;
    }
}

void TypeLowering::RebuildSlowpathCfg(GateRef hir, std::map<GateRef, size_t> &stateGateMap)
{
    acc_.ReplaceStateIn(hir, builder_.GetState());
    acc_.ReplaceDependIn(hir, builder_.GetDepend());
    auto uses = acc_.Uses(hir);
    GateRef stateGate = Circuit::NullGate();
    for (auto useIt = uses.begin(); useIt != uses.end(); ++useIt) {
        const OpCode op = acc_.GetOpCode(*useIt);
        if (op == OpCode::IF_SUCCESS) {
            stateGate = *useIt;
            builder_.SetState(*useIt);
            break;
        }
    }
    auto nextUses = acc_.Uses(stateGate);
    for (auto it = nextUses.begin(); it != nextUses.end(); ++it) {
        if (it.GetOpCode().IsState()) {
            stateGateMap[*it] = it.GetIndex();
        }
    }
    builder_.SetDepend(hir);
}

void TypeLowering::GenerateSuccessMerge(std::vector<GateRef> &successControl)
{
    GateRef stateMerge = builder_.GetState();
    GateRef dependSelect = builder_.GetDepend();
    successControl.emplace_back(stateMerge);
    successControl.emplace_back(dependSelect);
}

void TypeLowering::ReplaceHirToFastPathCfg(GateRef hir, GateRef outir, const std::vector<GateRef> &successControl)
{
    auto uses = acc_.Uses(hir);
    for (auto useIt = uses.begin(); useIt != uses.end();) {
        const OpCode op = acc_.GetOpCode(*useIt);
        if (op == OpCode::JS_BYTECODE && useIt.GetIndex() == 1) {
            acc_.ReplaceStateIn(*useIt, successControl[0]);
            useIt = acc_.ReplaceIn(useIt, successControl[1]);
        } else if (op == OpCode::RETURN) {
            if (acc_.IsValueIn(useIt)) {
                useIt = acc_.ReplaceIn(useIt, outir);
                continue;
            }
            if (acc_.GetOpCode(acc_.GetIn(*useIt, 0)) != OpCode::IF_EXCEPTION) {
                acc_.ReplaceStateIn(*useIt, successControl[0]);
                acc_.ReplaceDependIn(*useIt, successControl[1]);
                acc_.ReplaceValueIn(*useIt, outir);
            }
            ++useIt;
        } else if (op == OpCode::IF_SUCCESS || op == OpCode::IF_EXCEPTION) {
            ++useIt;
        } else if (op == OpCode::VALUE_SELECTOR) {
            if (*useIt != outir) {
                useIt = acc_.ReplaceIn(useIt, outir);
            } else {
                ++useIt;
            }
        } else if (op == OpCode::DEPEND_SELECTOR) {
            if (*useIt != successControl[1]) {
                useIt = acc_.ReplaceIn(useIt, successControl[1]);
            } else {
                ++useIt;
            }
        } else {
            useIt = acc_.ReplaceIn(useIt, outir);
        }
    }
}

void TypeLowering::ReplaceHirToCall(GateRef hirGate, GateRef callGate, bool noThrow)
{
    GateRef stateInGate = acc_.GetState(hirGate);
    // copy depend-wire of hirGate to callGate
    GateRef dependInGate = acc_.GetDep(hirGate);
    acc_.SetDep(callGate, dependInGate);

    GateRef ifBranch;
    if (!noThrow) {
        // exception value
        GateRef exceptionVal = builder_.ExceptionConstant(GateType::TaggedNPointer());
        // compare with trampolines result
        GateRef equal = builder_.BinaryLogic(OpCode(OpCode::EQ), callGate, exceptionVal);
        ifBranch = builder_.Branch(stateInGate, equal);
    } else {
        ifBranch = builder_.Branch(stateInGate, builder_.Boolean(false));
    }

    auto uses = acc_.Uses(hirGate);
    for (auto it = uses.begin(); it != uses.end();) {
        if (acc_.GetOpCode(*it) == OpCode::IF_SUCCESS) {
            acc_.SetOpCode(*it, OpCode::IF_FALSE);
            it = acc_.ReplaceIn(it, ifBranch);
        } else {
            if (acc_.GetOpCode(*it) == OpCode::IF_EXCEPTION) {
                acc_.SetOpCode(*it, OpCode::IF_TRUE);
                it = acc_.ReplaceIn(it, ifBranch);
            } else {
                it = acc_.ReplaceIn(it, callGate);
            }
        }
    }

    // delete old gate
    acc_.DeleteGate(hirGate);
}

GateRef TypeLowering::LowerCallRuntime(GateRef glue, int index, const std::vector<GateRef> &args, bool useLabel)
{
    if (useLabel) {
        GateRef result = builder_.CallRuntime(glue, index, Gate::InvalidGateRef, args);
        return result;
    } else {
        const CallSignature *cs = RuntimeStubCSigns::Get(RTSTUB_ID(CallRuntime));
        GateRef target = builder_.IntPtr(index);
        GateRef result = builder_.Call(cs, glue, target, dependEntry_, args);
        return result;
    }
}

template<OpCode::Op Op>
GateRef TypeLowering::FastAddOrSubOrMul(GateRef left, GateRef right)
{
    auto env = builder_.GetCurrentEnvironment();
    Label entry(&builder_);
    env->SubCfgEntry(&entry);
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    DEFVAlUE(doubleLeft, (&builder_), VariableType::FLOAT64(), builder_.Double(0));
    DEFVAlUE(doubleRight, (&builder_), VariableType::FLOAT64(), builder_.Double(0));

    Label exit(&builder_);
    Label doFloatOp(&builder_);
    Label doIntOp(&builder_);
    Label leftIsNumber(&builder_);
    Label rightIsNumber(&builder_);
    Label leftIsIntRightIsDouble(&builder_);
    Label rightIsInt(&builder_);
    Label rightIsDouble(&builder_);
    builder_.Branch(builder_.TaggedIsNumber(left), &leftIsNumber, &exit);
    builder_.Bind(&leftIsNumber);
    {
        builder_.Branch(builder_.TaggedIsNumber(right), &rightIsNumber, &exit);
        builder_.Bind(&rightIsNumber);
        {
            Label leftIsInt(&builder_);
            Label leftIsDouble(&builder_);
            builder_.Branch(builder_.TaggedIsInt(left), &leftIsInt, &leftIsDouble);
            builder_.Bind(&leftIsInt);
            {
                builder_.Branch(builder_.TaggedIsInt(right), &doIntOp, &leftIsIntRightIsDouble);
                builder_.Bind(&leftIsIntRightIsDouble);
                {
                    doubleLeft = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(left));
                    doubleRight = builder_.TaggedCastToDouble(right);
                    builder_.Jump(&doFloatOp);
                }
            }
            builder_.Bind(&leftIsDouble);
            {
                builder_.Branch(builder_.TaggedIsInt(right), &rightIsInt, &rightIsDouble);
                builder_.Bind(&rightIsInt);
                {
                    doubleLeft = builder_.TaggedCastToDouble(left);
                    doubleRight = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(right));
                    builder_.Jump(&doFloatOp);
                }
                builder_.Bind(&rightIsDouble);
                {
                    doubleLeft = builder_.TaggedCastToDouble(left);
                    doubleRight = builder_.TaggedCastToDouble(right);
                    builder_.Jump(&doFloatOp);
                }
            }
        }
    }
    builder_.Bind(&doIntOp);
    {
        Label overflow(&builder_);
        Label notOverflow(&builder_);
        // handle left is int and right is int
        GateRef res = BinaryOp<Op, MachineType::I64>(builder_.TaggedCastToInt64(left),
                                                     builder_.TaggedCastToInt64(right));
        GateRef max = builder_.Int64(INT32_MAX);
        GateRef min = builder_.Int64(INT32_MIN);
        Label greaterZero(&builder_);
        Label notGreaterZero(&builder_);
        builder_.Branch(builder_.Int32GreaterThan(builder_.TaggedCastToInt32(left), builder_.Int32(0)),
                        &greaterZero, &notGreaterZero);
        builder_.Bind(&greaterZero);
        {
            builder_.Branch(builder_.Int64GreaterThan(res, max), &overflow, &notOverflow);
        }
        builder_.Bind(&notGreaterZero);
        {
            Label lessZero(&builder_);
            builder_.Branch(builder_.Int32LessThan(builder_.TaggedCastToInt32(left), builder_.Int32(0)),
                            &lessZero, &notOverflow);
            builder_.Bind(&lessZero);
            builder_.Branch(builder_.Int64LessThan(res, min), &overflow, &notOverflow);
        }
        builder_.Bind(&overflow);
        {
            GateRef newDoubleLeft = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(left));
            GateRef newDoubleRight = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(right));
            GateRef middleRet = BinaryOp<Op, MachineType::F64>(newDoubleLeft, newDoubleRight);
            result = DoubleToTaggedDoublePtr(middleRet);
            builder_.Jump(&exit);
        }
        builder_.Bind(&notOverflow);
        {
            result = builder_.ToTaggedIntPtr(res);
            builder_.Jump(&exit);
        }
    }
    builder_.Bind(&doFloatOp);
    {
        // Other situations
        auto res = BinaryOp<Op, MachineType::F64>(*doubleLeft, *doubleRight);
        result = DoubleToTaggedDoublePtr(res);
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    auto ret = *result;
    env->SubCfgExit();
    return ret;
}

template<OpCode::Op Op, MachineType Type>
GateRef TypeLowering::BinaryOp(GateRef x, GateRef y)
{
    return builder_.BinaryArithmetic(OpCode(Op), Type, x, y);
}

GateRef TypeLowering::DoubleToTaggedDoublePtr(GateRef gate)
{
    return builder_.DoubleToTaggedDoublePtr(gate);
}

GateRef TypeLowering::ChangeInt32ToFloat64(GateRef gate)
{
    return builder_.ChangeInt32ToFloat64(gate);
}

GateRef TypeLowering::Int32Mod(GateRef left, GateRef right)
{
    return BinaryOp<OpCode::SMOD, MachineType::I32>(left, right);
}

GateRef TypeLowering::DoubleMod(GateRef left, GateRef right)
{
    return BinaryOp<OpCode::FMOD, MachineType::F64>(left, right);
}

GateRef TypeLowering::IntToTaggedNGc(GateRef x)
{
    GateRef val = builder_.SExtInt32ToInt64(x);
    return builder_.ToTaggedIntPtr(val);
}

GateRef TypeLowering::DoubleIsNAN(GateRef x)
{
    GateRef isEqual = builder_.Equal(x, x);
    return builder_.Equal(builder_.SExtInt1ToInt32(isEqual), builder_.Int32(0));
}

GateRef TypeLowering::DoubleIsINF(GateRef x)
{
    GateRef infinity = builder_.Double(base::POSITIVE_INFINITY);
    GateRef negativeInfinity = builder_.Double(-base::POSITIVE_INFINITY);
    GateRef isInfinity = builder_.Equal(x, infinity);
    GateRef isNegativeInfinity = builder_.Equal(x, negativeInfinity);
    return builder_.BoolOr(builder_.Equal(builder_.SExtInt1ToInt32(isInfinity), builder_.Int32(1)),
                           builder_.Equal(builder_.SExtInt1ToInt32(isNegativeInfinity), builder_.Int32(1)));
}

GateRef TypeLowering::GeneralMod(GateRef left, GateRef right, GateRef glue)
{
    auto env = builder_.GetCurrentEnvironment();
    Label entry(&builder_);
    env->SubCfgEntry(&entry);
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    DEFVAlUE(intLeft, (&builder_), VariableType::INT32(), builder_.Int32(0));
    DEFVAlUE(intRight, (&builder_), VariableType::INT32(), builder_.Int32(0));
    DEFVAlUE(doubleLeft, (&builder_), VariableType::FLOAT64(), builder_.Double(0));
    DEFVAlUE(doubleRight, (&builder_), VariableType::FLOAT64(), builder_.Double(0));

    Label leftIsInt(&builder_);
    Label leftNotIntOrRightNotInt(&builder_);
    Label exit(&builder_);
    builder_.Branch(builder_.TaggedIsInt(left), &leftIsInt, &leftNotIntOrRightNotInt);
    builder_.Bind(&leftIsInt);
    {
        Label rightIsInt(&builder_);
        builder_.Branch(builder_.TaggedIsInt(right), &rightIsInt, &leftNotIntOrRightNotInt);
        builder_.Bind(&rightIsInt);
        {
            intLeft = builder_.TaggedCastToInt32(left);
            intRight = builder_.TaggedCastToInt32(right);
            Label leftGreaterZero(&builder_);
            builder_.Branch(builder_.Int32GreaterThan(*intLeft, builder_.Int32(0)),
                            &leftGreaterZero, &leftNotIntOrRightNotInt);
            builder_.Bind(&leftGreaterZero);
            {
                Label rightGreaterZero(&builder_);
                builder_.Branch(builder_.Int32GreaterThan(*intRight, builder_.Int32(0)),
                                &rightGreaterZero, &leftNotIntOrRightNotInt);
                builder_.Bind(&rightGreaterZero);
                {
                    result = IntToTaggedNGc(Int32Mod(*intLeft, *intRight));
                    builder_.Jump(&exit);
                }
            }
        }
    }
    builder_.Bind(&leftNotIntOrRightNotInt);
    {
        Label leftIsNumber(&builder_);
        Label leftNotNumberOrRightNotNumber(&builder_);
        Label leftIsNumberAndRightIsNumber(&builder_);
        Label leftIsDoubleAndRightIsDouble(&builder_);
        builder_.Branch(builder_.TaggedIsNumber(left), &leftIsNumber, &leftNotNumberOrRightNotNumber);
        builder_.Bind(&leftIsNumber);
        {
            Label rightIsNumber(&builder_);
            builder_.Branch(builder_.TaggedIsNumber(right), &rightIsNumber, &leftNotNumberOrRightNotNumber);
            builder_.Bind(&rightIsNumber);
            {
                Label leftIsInt1(&builder_);
                Label leftNotInt1(&builder_);
                builder_.Branch(builder_.TaggedIsInt(left), &leftIsInt1, &leftNotInt1);
                builder_.Bind(&leftIsInt1);
                {
                    doubleLeft = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(left));
                    builder_.Jump(&leftIsNumberAndRightIsNumber);
                }
                builder_.Bind(&leftNotInt1);
                {
                    doubleLeft = builder_.TaggedCastToDouble(left);
                    builder_.Jump(&leftIsNumberAndRightIsNumber);
                }
            }
        }
        builder_.Bind(&leftNotNumberOrRightNotNumber);
        {
            builder_.Jump(&exit);
        }
        builder_.Bind(&leftIsNumberAndRightIsNumber);
        {
            Label rightIsInt1(&builder_);
            Label rightNotInt1(&builder_);
            builder_.Branch(builder_.TaggedIsInt(right), &rightIsInt1, &rightNotInt1);
            builder_.Bind(&rightIsInt1);
            {
                doubleRight = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(right));
                builder_.Jump(&leftIsDoubleAndRightIsDouble);
            }
            builder_.Bind(&rightNotInt1);
            {
                doubleRight = builder_.TaggedCastToDouble(right);
                builder_.Jump(&leftIsDoubleAndRightIsDouble);
            }
        }
        builder_.Bind(&leftIsDoubleAndRightIsDouble);
        {
            Label rightNotZero(&builder_);
            Label rightIsZeroOrNanOrLeftIsNanOrInf(&builder_);
            Label rightNotZeroAndNanAndLeftNotNanAndInf(&builder_);
            builder_.Branch(builder_.Equal(*doubleRight, builder_.Double(0.0)), &rightIsZeroOrNanOrLeftIsNanOrInf,
                            &rightNotZero);
            builder_.Bind(&rightNotZero);
            {
                Label rightNotNan(&builder_);
                builder_.Branch(DoubleIsNAN(*doubleRight), &rightIsZeroOrNanOrLeftIsNanOrInf, &rightNotNan);
                builder_.Bind(&rightNotNan);
                {
                    Label leftNotNan(&builder_);
                    builder_.Branch(DoubleIsNAN(*doubleLeft), &rightIsZeroOrNanOrLeftIsNanOrInf, &leftNotNan);
                    builder_.Bind(&leftNotNan);
                    builder_.Branch(DoubleIsINF(*doubleLeft), &rightIsZeroOrNanOrLeftIsNanOrInf,
                                    &rightNotZeroAndNanAndLeftNotNanAndInf);
                }
            }
            builder_.Bind(&rightIsZeroOrNanOrLeftIsNanOrInf);
            {
                result = DoubleToTaggedDoublePtr(builder_.Double(base::NAN_VALUE));
                builder_.Jump(&exit);
            }
            builder_.Bind(&rightNotZeroAndNanAndLeftNotNanAndInf);
            {
                Label leftNotZero(&builder_);
                Label leftIsZeroOrRightIsInf(&builder_);
                builder_.Branch(builder_.Equal(*doubleLeft, builder_.Double(0.0)), &leftIsZeroOrRightIsInf,
                                &leftNotZero);
                builder_.Bind(&leftNotZero);
                {
                    Label rightNotInf(&builder_);
                    builder_.Branch(DoubleIsINF(*doubleRight), &leftIsZeroOrRightIsInf, &rightNotInf);
                    builder_.Bind(&rightNotInf);
                    {
                        result = LowerCallRuntime(glue, RTSTUB_ID(FloatMod), {*doubleLeft, *doubleRight});
                        builder_.Jump(&exit);
                    }
                }
                builder_.Bind(&leftIsZeroOrRightIsInf);
                {
                    result = DoubleToTaggedDoublePtr(*doubleLeft);
                    builder_.Jump(&exit);
                }
            }
        }
    }
    builder_.Bind(&exit);
    auto ret = *result;
    env->SubCfgExit();
    return ret;
}

GateRef TypeLowering::Less(GateRef left, GateRef right)
{
    auto env = builder_.GetCurrentEnvironment();
    Label entry(&builder_);
    env->SubCfgEntry(&entry);
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    Label leftIsInt(&builder_);
    Label leftOrRightNotInt(&builder_);
    Label leftLessRight(&builder_);
    Label leftGreaterEqRight(&builder_);
    Label exit(&builder_);
    builder_.Branch(builder_.TaggedIsInt(left), &leftIsInt, &leftOrRightNotInt);
    builder_.Bind(&leftIsInt);
    {
        Label rightIsInt(&builder_);
        builder_.Branch(builder_.TaggedIsInt(right), &rightIsInt, &leftOrRightNotInt);
        builder_.Bind(&rightIsInt);
        {
            GateRef intLeft = builder_.TaggedCastToInt32(left);
            GateRef intRight = builder_.TaggedCastToInt32(right);
            builder_.Branch(builder_.Int32LessThan(intLeft, intRight), &leftLessRight, &leftGreaterEqRight);
        }
    }
    builder_.Bind(&leftOrRightNotInt);
    {
        Label leftIsNumber(&builder_);
        builder_.Branch(builder_.TaggedIsNumber(left), &leftIsNumber, &exit);
        builder_.Bind(&leftIsNumber);
        {
            Label rightIsNumber(&builder_);
            builder_.Branch(builder_.TaggedIsNumber(right), &rightIsNumber, &exit);
            builder_.Bind(&rightIsNumber);
            {
                // fast path
                DEFVAlUE(doubleLeft, (&builder_), VariableType::FLOAT64(), builder_.Double(0.0));
                DEFVAlUE(doubleRight, (&builder_), VariableType::FLOAT64(), builder_.Double(0.0));
                Label leftIsInt1(&builder_);
                Label leftNotInt1(&builder_);
                Label exit1(&builder_);
                Label exit2(&builder_);
                Label rightIsInt1(&builder_);
                Label rightNotInt1(&builder_);
                builder_.Branch(builder_.TaggedIsInt(left), &leftIsInt1, &leftNotInt1);
                builder_.Bind(&leftIsInt1);
                {
                    doubleLeft = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(left));
                    builder_.Jump(&exit1);
                }
                builder_.Bind(&leftNotInt1);
                {
                    doubleLeft = builder_.TaggedCastToDouble(left);
                    builder_.Jump(&exit1);
                }
                builder_.Bind(&exit1);
                builder_.Branch(builder_.TaggedIsInt(right), &rightIsInt1, &rightNotInt1);
                builder_.Bind(&rightIsInt1);
                {
                    doubleRight = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(right));
                    builder_.Jump(&exit2);
                }
                builder_.Bind(&rightNotInt1);
                {
                    doubleRight = builder_.TaggedCastToDouble(right);
                    builder_.Jump(&exit2);
                }
                builder_.Bind(&exit2);
                builder_.Branch(builder_.DoubleLessThan(*doubleLeft, *doubleRight), &leftLessRight,
                                &leftGreaterEqRight);
            }
        }
    }
    builder_.Bind(&leftLessRight);
    {
        result = builder_.Int64ToTaggedPtr(builder_.TaggedTrue());
        builder_.Jump(&exit);
    }
    builder_.Bind(&leftGreaterEqRight);
    {
        result = builder_.Int64ToTaggedPtr(builder_.TaggedFalse());
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    auto ret = *result;
    env->SubCfgExit();
    return ret;
}

GateRef TypeLowering::LessEq(GateRef left, GateRef right)
{
    auto env = builder_.GetCurrentEnvironment();
    Label entry(&builder_);
    env->SubCfgEntry(&entry);
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.Undefined());
    Label leftIsInt(&builder_);
    Label leftOrRightNotInt(&builder_);
    Label leftLessEqRight(&builder_);
    Label leftGreaterRight(&builder_);
    Label exit(&builder_);
    builder_.Branch(builder_.TaggedIsInt(left), &leftIsInt, &leftOrRightNotInt);
    builder_.Bind(&leftIsInt);
    {
        Label rightIsInt(&builder_);
        builder_.Branch(builder_.TaggedIsInt(right), &rightIsInt, &leftOrRightNotInt);
        builder_.Bind(&rightIsInt);
        {
            GateRef intLeft = builder_.TaggedCastToInt32(left);
            GateRef intRight = builder_.TaggedCastToInt32(right);
            builder_.Branch(builder_.Int32LessThanOrEqual(intLeft, intRight), &leftLessEqRight, &leftGreaterRight);
        }
    }
    builder_.Bind(&leftOrRightNotInt);
    {
        Label leftIsNumber(&builder_);
        builder_.Branch(builder_.TaggedIsNumber(left), &leftIsNumber, &exit);
        builder_.Bind(&leftIsNumber);
        {
            Label rightIsNumber(&builder_);
            builder_.Branch(builder_.TaggedIsNumber(right), &rightIsNumber, &exit);
            builder_.Bind(&rightIsNumber);
            {
                // fast path
                DEFVAlUE(doubleLeft, (&builder_), VariableType::FLOAT64(), builder_.Double(0.0));
                DEFVAlUE(doubleRight, (&builder_), VariableType::FLOAT64(), builder_.Double(0.0));
                Label leftIsInt1(&builder_);
                Label leftNotInt1(&builder_);
                Label exit1(&builder_);
                Label exit2(&builder_);
                Label rightIsInt1(&builder_);
                Label rightNotInt1(&builder_);
                builder_.Branch(builder_.TaggedIsInt(left), &leftIsInt1, &leftNotInt1);
                builder_.Bind(&leftIsInt1);
                {
                    doubleLeft = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(left));
                    builder_.Jump(&exit1);
                }
                builder_.Bind(&leftNotInt1);
                {
                    doubleLeft = builder_.TaggedCastToDouble(left);
                    builder_.Jump(&exit1);
                }
                builder_.Bind(&exit1);
                builder_.Branch(builder_.TaggedIsInt(right), &rightIsInt1, &rightNotInt1);
                builder_.Bind(&rightIsInt1);
                {
                    doubleRight = ChangeInt32ToFloat64(builder_.TaggedCastToInt32(right));
                    builder_.Jump(&exit2);
                }
                builder_.Bind(&rightNotInt1);
                {
                    doubleRight = builder_.TaggedCastToDouble(right);
                    builder_.Jump(&exit2);
                }
                builder_.Bind(&exit2);
                builder_.Branch(builder_.DoubleLessThanOrEqual(*doubleLeft, *doubleRight), &leftLessEqRight,
                                &leftGreaterRight);
            }
        }
    }
    builder_.Bind(&leftLessEqRight);
    {
        result = builder_.Int64ToTaggedPtr(builder_.TaggedTrue());
        builder_.Jump(&exit);
    }
    builder_.Bind(&leftGreaterRight);
    {
        result = builder_.Int64ToTaggedPtr(builder_.TaggedFalse());
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    auto ret = *result;
    env->SubCfgExit();
    return ret;
}

void TypeLowering::LowerTypeNewObjDynRange(GateRef gate, GateRef glue)
{
    GateRef ctor = acc_.GetValueIn(gate, 0);
    GateType ctorType = acc_.GetGateType(ctor);
    if (!ctorType.IsTSType()) {
        return;
    }

    if (!tsManager_->IsClassTypeKind(ctorType)) {
        return;
    }

    GlobalTSTypeRef gt = GlobalTSTypeRef(ctorType.GetType());
    std::map<GlobalTSTypeRef, uint32_t> gtHClassIndexMap = tsManager_->GetGtHClassIndexMap();
    int64_t index = gtHClassIndexMap[gt];
    GateRef ihcIndex = builder_.ToTaggedInt(builder_.Int64(index));

    size_t range = acc_.GetNumValueIn(gate);
    std::vector<GateRef> args(range + 1);

    for (size_t i = 0; i < range; ++i) {
        args[i] = acc_.GetValueIn(gate, i);
    }
    args[range] = ihcIndex;

    const int id = RTSTUB_ID(OptNewObjWithIHClass);
    GateRef newGate = LowerCallRuntime(glue, id, args);
    ReplaceHirToCall(gate, newGate);
}

void TypeLowering::LowerTypeAdd2Dyn(GateRef gate, [[maybe_unused]]GateRef glue)
{
    GateRef left = acc_.GetValueIn(gate, 0);
    GateType leftType = acc_.GetGateType(left);

    GateRef right = acc_.GetValueIn(gate, 1);
    GateType rightType = acc_.GetGateType(right);
    if (!leftType.IsTSType() || !rightType.IsTSType()) {
        return;
    }
    if (!leftType.IsNumberType() || !rightType.IsNumberType()) {
        return;
    }
    std::map<GateRef, size_t> stateGateMap;
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    result = FastAddOrSubOrMul<OpCode::ADD>(left, right);
    Label successExit(&builder_);
    Label slowPath(&builder_);
    builder_.Branch(builder_.IsSpecial(*result, JSTaggedValue::VALUE_HOLE),
                    &slowPath, &successExit);
    builder_.Bind(&slowPath);
    {
        // slow path
        result = gate;
        RebuildSlowpathCfg(gate, stateGateMap);
        builder_.Jump(&successExit);
    }
    builder_.Bind(&successExit);
    for (auto [state, index] : stateGateMap) {
        acc_.ReplaceIn(state, index, builder_.GetState());
    }
    std::vector<GateRef> successControl;
    GenerateSuccessMerge(successControl);
    ReplaceHirToFastPathCfg(gate, *result, successControl);
}

void TypeLowering::LowerTypeSub2Dyn([[maybe_unused]]GateRef gate)
{
    GateRef left = acc_.GetValueIn(gate, 0);
    GateType leftType = acc_.GetGateType(left);

    GateRef right = acc_.GetValueIn(gate, 1);
    GateType rightType = acc_.GetGateType(right);
    if (!leftType.IsNumberType() || !rightType.IsNumberType()) {
        return;
    }
    std::map<GateRef, size_t> stateGateMap;
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    result = FastAddOrSubOrMul<OpCode::SUB>(left, right);
    Label successExit(&builder_);
    Label slowPath(&builder_);
    builder_.Branch(builder_.IsSpecial(*result, JSTaggedValue::VALUE_HOLE),
                    &slowPath, &successExit);
    builder_.Bind(&slowPath);
    {
        // slow path
        result = gate;
        RebuildSlowpathCfg(gate, stateGateMap);
        builder_.Jump(&successExit);
    }
    builder_.Bind(&successExit);
    for (auto [state, index] : stateGateMap) {
        acc_.ReplaceIn(state, index, builder_.GetState());
    }
    std::vector<GateRef> successControl;
    GenerateSuccessMerge(successControl);
    ReplaceHirToFastPathCfg(gate, *result, successControl);
}

void TypeLowering::LowerTypeMul2Dyn(GateRef gate)
{
    GateRef left = acc_.GetValueIn(gate, 0);
    GateType leftType = acc_.GetGateType(left);

    GateRef right = acc_.GetValueIn(gate, 1);
    GateType rightType = acc_.GetGateType(right);
    if (!leftType.IsNumberType() || !rightType.IsNumberType()) {
        return;
    }
    std::map<GateRef, size_t> stateGateMap;
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    result = FastAddOrSubOrMul<OpCode::MUL>(left, right);
    Label successExit(&builder_);
    Label slowPath(&builder_);
    builder_.Branch(builder_.IsSpecial(*result, JSTaggedValue::VALUE_HOLE),
                    &slowPath, &successExit);
    builder_.Bind(&slowPath);
    {
        // slow path
        result = gate;
        RebuildSlowpathCfg(gate, stateGateMap);
        builder_.Jump(&successExit);
    }
    builder_.Bind(&successExit);
    for (auto [state, index] : stateGateMap) {
        acc_.ReplaceIn(state, index, builder_.GetState());
    }
    std::vector<GateRef> successControl;
    GenerateSuccessMerge(successControl);
    ReplaceHirToFastPathCfg(gate, *result, successControl);
}

void TypeLowering::LowerTypeMod2Dyn(GateRef gate, GateRef glue)
{
    GateRef left = acc_.GetValueIn(gate, 0);
    GateType leftType = acc_.GetGateType(left);

    GateRef right = acc_.GetValueIn(gate, 1);
    GateType rightType = acc_.GetGateType(right);
    if (!leftType.IsNumberType() || !rightType.IsNumberType()) {
        return;
    }
    std::map<GateRef, size_t> stateGateMap;
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    result = GeneralMod(left, right, glue);
    Label slowPath(&builder_);
    Label successExit(&builder_);
    builder_.Branch(builder_.IsSpecial(*result, JSTaggedValue::VALUE_HOLE),
                    &slowPath, &successExit);
    builder_.Bind(&slowPath);
    {
        // slow path
        result = gate;
        RebuildSlowpathCfg(gate, stateGateMap);
        builder_.Jump(&successExit);
    }
    builder_.Bind(&successExit);
    for (auto [state, index] : stateGateMap) {
        acc_.ReplaceIn(state, index, builder_.GetState());
    }
    std::vector<GateRef> successControl;
    GenerateSuccessMerge(successControl);
    ReplaceHirToFastPathCfg(gate, *result, successControl);
}

void TypeLowering::LowerTypeLessDyn(GateRef gate)
{
    GateRef left = acc_.GetValueIn(gate, 0);
    GateType leftType = acc_.GetGateType(left);

    GateRef right = acc_.GetValueIn(gate, 1);
    GateType rightType = acc_.GetGateType(right);
    if (!leftType.IsNumberType() || !rightType.IsNumberType()) {
        return;
    }
    std::map<GateRef, size_t> stateGateMap;
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    result = Less(left, right);
    Label slowPath(&builder_);
    Label successExit(&builder_);
    builder_.Branch(builder_.IsSpecial(*result, JSTaggedValue::VALUE_HOLE),
                    &slowPath, &successExit);
    builder_.Bind(&slowPath);
    {
        // slow path
        result = gate;
        RebuildSlowpathCfg(gate, stateGateMap);
        builder_.Jump(&successExit);
    }
    builder_.Bind(&successExit);
    for (auto [state, index] : stateGateMap) {
        acc_.ReplaceIn(state, index, builder_.GetState());
    }
    std::vector<GateRef> successControl;
    GenerateSuccessMerge(successControl);
    ReplaceHirToFastPathCfg(gate, *result, successControl);
}

void TypeLowering::LowerTypeLessEqDyn(GateRef gate)
{
    GateRef left = acc_.GetValueIn(gate, 0);
    GateType leftType = acc_.GetGateType(left);

    GateRef right = acc_.GetValueIn(gate, 1);
    GateType rightType = acc_.GetGateType(right);
    if (!leftType.IsNumberType() || !rightType.IsNumberType()) {
        return;
    }
    std::map<GateRef, size_t> stateGateMap;
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    result = LessEq(left, right);
    Label slowPath(&builder_);
    Label successExit(&builder_);
    builder_.Branch(builder_.IsSpecial(*result, JSTaggedValue::VALUE_HOLE),
                    &slowPath, &successExit);
    builder_.Bind(&slowPath);
    {
        // slow path
        result = gate;
        RebuildSlowpathCfg(gate, stateGateMap);
        builder_.Jump(&successExit);
    }
    builder_.Bind(&successExit);
    for (auto [state, index] : stateGateMap) {
        acc_.ReplaceIn(state, index, builder_.GetState());
    }
    std::vector<GateRef> successControl;
    GenerateSuccessMerge(successControl);
    ReplaceHirToFastPathCfg(gate, *result, successControl);
}

void TypeLowering::LowerTypeGreaterDyn(GateRef gate)
{
    GateRef left = acc_.GetValueIn(gate, 0);
    GateType leftType = acc_.GetGateType(left);

    GateRef right = acc_.GetValueIn(gate, 1);
    GateType rightType = acc_.GetGateType(right);
    if (!leftType.IsNumberType() || !rightType.IsNumberType()) {
        return;
    }
    std::map<GateRef, size_t> stateGateMap;
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    result = Less(right, left);
    Label slowPath(&builder_);
    Label successExit(&builder_);
    builder_.Branch(builder_.IsSpecial(*result, JSTaggedValue::VALUE_HOLE),
                    &slowPath, &successExit);
    builder_.Bind(&slowPath);
    {
        // slow path
        result = gate;
        RebuildSlowpathCfg(gate, stateGateMap);
        builder_.Jump(&successExit);
    }
    builder_.Bind(&successExit);
    for (auto [state, index] : stateGateMap) {
        acc_.ReplaceIn(state, index, builder_.GetState());
    }
    std::vector<GateRef> successControl;
    GenerateSuccessMerge(successControl);
    ReplaceHirToFastPathCfg(gate, *result, successControl);
}

void TypeLowering::LowerTypeGreaterEqDyn(GateRef gate)
{
    GateRef left = acc_.GetValueIn(gate, 0);
    GateType leftType = acc_.GetGateType(left);

    GateRef right = acc_.GetValueIn(gate, 1);
    GateType rightType = acc_.GetGateType(right);
    if (!leftType.IsNumberType() || !rightType.IsNumberType()) {
        return;
    }
    std::map<GateRef, size_t> stateGateMap;
    DEFVAlUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    result = LessEq(right, left);
    Label slowPath(&builder_);
    Label successExit(&builder_);
    builder_.Branch(builder_.IsSpecial(*result, JSTaggedValue::VALUE_HOLE),
                    &slowPath, &successExit);
    builder_.Bind(&slowPath);
    {
        // slow path
        result = gate;
        RebuildSlowpathCfg(gate, stateGateMap);
        builder_.Jump(&successExit);
    }
    builder_.Bind(&successExit);
    for (auto [state, index] : stateGateMap) {
        acc_.ReplaceIn(state, index, builder_.GetState());
    }
    std::vector<GateRef> successControl;
    GenerateSuccessMerge(successControl);
    ReplaceHirToFastPathCfg(gate, *result, successControl);
}
}  // namespace panda::ecmascript