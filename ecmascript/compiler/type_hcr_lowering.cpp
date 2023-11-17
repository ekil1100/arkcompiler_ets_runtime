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

#include "ecmascript/compiler/type_hcr_lowering.h"
#include "ecmascript/compiler/builtins_lowering.h"
#include "ecmascript/compiler/new_object_stub_builder.h"
#include "ecmascript/compiler/builtins/builtins_string_stub_builder.h"
#include "ecmascript/compiler/rt_call_signature.h"
#include "ecmascript/compiler/share_gate_meta_data.h"
#include "ecmascript/compiler/variable_type.h"
#include "ecmascript/deoptimizer/deoptimizer.h"
#include "ecmascript/js_arraybuffer.h"
#include "ecmascript/js_native_pointer.h"
#include "ecmascript/js_object.h"
#include "ecmascript/js_primitive_ref.h"
#include "ecmascript/message_string.h"
#include "ecmascript/subtyping_operator.h"
#include "ecmascript/vtable.h"
namespace panda::ecmascript::kungfu {
GateRef TypeHCRLowering::VisitGate(GateRef gate)
{
    GateRef glue = acc_.GetGlueFromArgList();
    auto op = acc_.GetOpCode(gate);
    switch (op) {
        case OpCode::PRIMITIVE_TYPE_CHECK:
            LowerPrimitiveTypeCheck(gate);
            break;
        case OpCode::BUILTIN_PROTOTYPE_HCLASS_CHECK:
            LowerBuiltinPrototypeHClassCheck(gate);
            break;
        case OpCode::STABLE_ARRAY_CHECK:
            LowerStableArrayCheck(gate);
            break;
        case OpCode::TYPED_ARRAY_CHECK:
            LowerTypedArrayCheck(gate);
            break;
        case OpCode::ECMA_STRING_CHECK:
            LowerEcmaStringCheck(gate);
            break;
        case OpCode::FLATTEN_TREE_STRING_CHECK:
            LowerFlattenTreeStringCheck(gate, glue);
            break;
        case OpCode::LOAD_STRING_LENGTH:
            LowerStringLength(gate);
            break;
        case OpCode::LOAD_TYPED_ARRAY_LENGTH:
            LowerLoadTypedArrayLength(gate);
            break;
        case OpCode::OBJECT_TYPE_CHECK:
            LowerObjectTypeCheck(gate);
            break;
        case OpCode::OBJECT_TYPE_COMPARE:
            LowerObjectTypeCompare(gate);
            break;
        case OpCode::RANGE_CHECK_PREDICATE:
            LowerRangeCheckPredicate(gate);
            break;
        case OpCode::INDEX_CHECK:
            LowerIndexCheck(gate);
            break;
        case OpCode::TYPED_CALLTARGETCHECK_OP:
            LowerJSCallTargetCheck(gate);
            break;
        case OpCode::TYPED_CALL_CHECK:
            LowerCallTargetCheck(gate);
            break;
        case OpCode::JSINLINETARGET_TYPE_CHECK:
            LowerJSInlineTargetTypeCheck(gate);
            break;
        case OpCode::TYPE_CONVERT:
            LowerTypeConvert(gate);
            break;
        case OpCode::LOAD_PROPERTY:
            LowerLoadProperty(gate);
            break;
        case OpCode::CALL_GETTER:
            LowerCallGetter(gate, glue);
            break;
        case OpCode::STORE_PROPERTY:
        case OpCode::STORE_PROPERTY_NO_BARRIER:
            LowerStoreProperty(gate);
            break;
        case OpCode::CALL_SETTER:
            LowerCallSetter(gate, glue);
            break;
        case OpCode::LOAD_ARRAY_LENGTH:
            LowerLoadArrayLength(gate);
            break;
        case OpCode::LOAD_ELEMENT:
            LowerLoadElement(gate);
            break;
        case OpCode::STORE_ELEMENT:
            LowerStoreElement(gate, glue);
            break;
        case OpCode::TYPED_CALL_BUILTIN:
            LowerTypedCallBuitin(gate);
            break;
        case OpCode::TYPED_NEW_ALLOCATE_THIS:
            LowerTypedNewAllocateThis(gate, glue);
            break;
        case OpCode::TYPED_SUPER_ALLOCATE_THIS:
            LowerTypedSuperAllocateThis(gate, glue);
            break;
        case OpCode::GET_SUPER_CONSTRUCTOR:
            LowerGetSuperConstructor(gate);
            break;
        case OpCode::COW_ARRAY_CHECK:
            LowerCowArrayCheck(gate, glue);
            break;
        case OpCode::LOAD_GETTER:
            LowerLoadGetter(gate);
            break;
        case OpCode::LOAD_SETTER:
            LowerLoadSetter(gate);
            break;
        case OpCode::INLINE_ACCESSOR_CHECK:
            LowerInlineAccessorCheck(gate);
            break;
        case OpCode::STRING_EQUAL:
            LowerStringEqual(gate, glue);
            break;
        case OpCode::STRING_ADD:
            LowerStringAdd(gate, glue);
            break;
        case OpCode::TYPE_OF_CHECK:
            LowerTypeOfCheck(gate);
            break;
        case OpCode::TYPE_OF:
            LowerTypeOf(gate, glue);
            break;
        case OpCode::ARRAY_CONSTRUCTOR_CHECK:
            LowerArrayConstructorCheck(gate, glue);
            break;
        case OpCode::ARRAY_CONSTRUCTOR:
            LowerArrayConstructor(gate, glue);
            break;
        case OpCode::LOAD_BUILTIN_OBJECT:
            if (enableLoweringBuiltin_) {
                LowerLoadBuiltinObject(gate);
            }
            break;
        case OpCode::OBJECT_CONSTRUCTOR_CHECK:
            LowerObjectConstructorCheck(gate, glue);
            break;
        case OpCode::OBJECT_CONSTRUCTOR:
            LowerObjectConstructor(gate, glue);
            break;
        default:
            break;
    }
    return Circuit::NullGate();
}

void TypeHCRLowering::LowerJSCallTargetCheck(GateRef gate)
{
    TypedCallTargetCheckOp Op = acc_.GetTypedCallTargetCheckOp(gate);
    switch (Op) {
        case TypedCallTargetCheckOp::JSCALL_IMMEDIATE_AFTER_FUNC_DEF: {
            LowerJSCallTargetFromDefineFuncCheck(gate);
            break;
        }
        case TypedCallTargetCheckOp::JSCALL: {
            LowerJSCallTargetTypeCheck(gate);
            break;
        }
        case TypedCallTargetCheckOp::JSCALL_FAST: {
            LowerJSFastCallTargetTypeCheck(gate);
            break;
        }
        case TypedCallTargetCheckOp::JSCALLTHIS: {
            LowerJSCallThisTargetTypeCheck(gate);
            break;
        }
        case TypedCallTargetCheckOp::JSCALLTHIS_FAST: {
            LowerJSFastCallThisTargetTypeCheck(gate);
            break;
        }
        case TypedCallTargetCheckOp::JSCALLTHIS_NOGC: {
            LowerJSNoGCCallThisTargetTypeCheck(gate);
            break;
        }
        case TypedCallTargetCheckOp::JSCALLTHIS_FAST_NOGC: {
            LowerJSNoGCFastCallThisTargetTypeCheck(gate);
            break;
        }
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
}

void TypeHCRLowering::LowerPrimitiveTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    if (type.IsIntType()) {
        LowerIntCheck(gate);
    } else if (type.IsDoubleType()) {
        LowerDoubleCheck(gate);
    } else if (type.IsNumberType()) {
        LowerNumberCheck(gate);
    } else if (type.IsBooleanType()) {
        LowerBooleanCheck(gate);
    } else {
        LOG_ECMA(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerIntCheck(GateRef gate)
{
    GateRef frameState = GetFrameState(gate);

    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsInt(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTINT);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerDoubleCheck(GateRef gate)
{
    GateRef frameState = GetFrameState(gate);

    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsDouble(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTDOUBLE);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerNumberCheck(GateRef gate)
{
    GateRef frameState = GetFrameState(gate);

    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsNumber(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTNUMBER);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerBooleanCheck(GateRef gate)
{
    GateRef frameState = GetFrameState(gate);

    GateRef value = acc_.GetValueIn(gate, 0);
    GateRef typeCheck = builder_.TaggedIsBoolean(value);
    builder_.DeoptCheck(typeCheck, frameState, DeoptType::NOTBOOL);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerStableArrayCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = GetFrameState(gate);

    GateRef receiver = acc_.GetValueIn(gate, 0);
    builder_.HeapObjectCheck(receiver, frameState);

    GateRef receiverHClass = builder_.LoadConstOffset(
        VariableType::JS_POINTER(), receiver, TaggedObject::HCLASS_OFFSET);
    ArrayMetaDataAccessor accessor = acc_.GetArrayMetaDataAccessor(gate);
    builder_.HClassStableArrayCheck(receiverHClass, frameState, accessor);
    builder_.ArrayGuardianCheck(frameState);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::SetDeoptTypeInfo(BuiltinTypeId id, DeoptType &type, size_t &funcIndex)
{
    type = DeoptType::NOTARRAY;
    switch (id) {
        case BuiltinTypeId::INT8_ARRAY:
            funcIndex = GlobalEnv::INT8_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::UINT8_ARRAY:
            funcIndex = GlobalEnv::UINT8_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::UINT8_CLAMPED_ARRAY:
            funcIndex = GlobalEnv::UINT8_CLAMPED_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::INT16_ARRAY:
            funcIndex = GlobalEnv::INT16_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::UINT16_ARRAY:
            funcIndex = GlobalEnv::UINT16_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::INT32_ARRAY:
            funcIndex = GlobalEnv::INT32_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::UINT32_ARRAY:
            funcIndex = GlobalEnv::UINT32_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::FLOAT32_ARRAY:
            funcIndex = GlobalEnv::FLOAT32_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::FLOAT64_ARRAY:
            funcIndex = GlobalEnv::FLOAT64_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::BIGINT64_ARRAY:
            funcIndex = GlobalEnv::BIGINT64_ARRAY_FUNCTION_INDEX;
            break;
        case BuiltinTypeId::BIGUINT64_ARRAY:
            funcIndex = GlobalEnv::BIGUINT64_ARRAY_FUNCTION_INDEX;
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
}

void TypeHCRLowering::LowerTypedArrayCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    size_t typedArrayFuncIndex = GlobalEnv::TYPED_ARRAY_FUNCTION_INDEX;
    auto deoptType = DeoptType::NOTCHECK;

    auto builtinTypeId = tsManager_->GetTypedArrayBuiltinId(type);
    SetDeoptTypeInfo(builtinTypeId, deoptType, typedArrayFuncIndex);

    GateRef frameState = GetFrameState(gate);
    GateRef glueGlobalEnv = builder_.GetGlobalEnv();
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef receiverHClass = builder_.LoadHClass(receiver);
    GateRef protoOrHclass = builder_.GetGlobalEnvObjHClass(glueGlobalEnv, typedArrayFuncIndex);
    GateRef check = builder_.Equal(receiverHClass, protoOrHclass);
    builder_.DeoptCheck(check, frameState, deoptType);

    if (IsOnHeap()) {
        GateRef isOnHeap = builder_.LoadConstOffset(VariableType::BOOL(), receiver, JSTypedArray::ON_HEAP_OFFSET);
        builder_.DeoptCheck(isOnHeap, frameState, DeoptType::NOTONHEAP);
    }

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerEcmaStringCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = GetFrameState(gate);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    builder_.HeapObjectCheck(receiver, frameState);
    GateRef isString = builder_.TaggedObjectIsString(receiver);
    builder_.DeoptCheck(isString, frameState, DeoptType::NOTSTRING);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerFlattenTreeStringCheck(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef str = acc_.GetValueIn(gate, 0);
    DEFVALUE(result, (&builder_), VariableType::JS_POINTER(), str);
    Label isTreeString(&builder_);
    Label exit(&builder_);

    builder_.Branch(builder_.IsTreeString(str), &isTreeString, &exit);
    builder_.Bind(&isTreeString);
    {
        Label isFlat(&builder_);
        Label needFlat(&builder_);
        builder_.Branch(builder_.TreeStringIsFlat(str), &isFlat, &needFlat);
        builder_.Bind(&isFlat);
        {
            result = builder_.GetFirstFromTreeString(str);
            builder_.Jump(&exit);
        }
        builder_.Bind(&needFlat);
        {
            result = LowerCallRuntime(glue, gate, RTSTUB_ID(SlowFlattenString), { str }, true);
            builder_.Jump(&exit);
        }
    }

    builder_.Bind(&exit);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), *result);
}

GateRef TypeHCRLowering::GetLengthFromString(GateRef gate)
{
    GateRef shiftCount = builder_.Int32(EcmaString::STRING_LENGTH_SHIFT_COUNT);
    return builder_.Int32LSR(
        builder_.LoadConstOffset(VariableType::INT32(), gate, EcmaString::MIX_LENGTH_OFFSET), shiftCount);
}

void TypeHCRLowering::LowerStringLength(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef length = GetLengthFromString(receiver);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), length);
}

void TypeHCRLowering::LowerLoadTypedArrayLength(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef length = builder_.LoadConstOffset(VariableType::INT32(), receiver, JSTypedArray::ARRAY_LENGTH_OFFSET);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), length);
}

void TypeHCRLowering::LowerObjectTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateType type = acc_.GetObjectTypeAccessor(gate).GetType();
    if (tsManager_->IsClassInstanceTypeKind(type)) {
        LowerTSSubtypingCheck(gate);
    } else if (tsManager_->IsClassTypeKind(type) ||
               tsManager_->IsObjectTypeKind(type)) {
        LowerSimpleHClassCheck(gate);
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerTSSubtypingCheck(GateRef gate)
{
    GateRef frameState = GetFrameState(gate);
    Label levelValid(&builder_);
    Label exit(&builder_);
    GateRef compare = BuildCompareSubTyping(gate, frameState, &levelValid, &exit);
    builder_.DeoptCheck(compare, frameState, DeoptType::INCONSISTENTHCLASS);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerSimpleHClassCheck(GateRef gate)
{
    GateRef frameState = GetFrameState(gate);
    GateRef compare = BuildCompareHClass(gate, frameState);
    builder_.DeoptCheck(compare, frameState, DeoptType::INCONSISTENTHCLASS);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerObjectTypeCompare(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetObjectTypeAccessor(gate).GetType();
    if (tsManager_->IsClassInstanceTypeKind(type)) {
        LowerTSSubtypingCompare(gate);
    } else if (tsManager_->IsClassTypeKind(type) ||
               tsManager_->IsObjectTypeKind(type)) {
        LowerSimpleHClassCompare(gate);
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerSimpleHClassCompare(GateRef gate)
{
    GateRef frameState = GetFrameState(gate);
    GateRef compare = BuildCompareHClass(gate, frameState) ;
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), compare);
}

void TypeHCRLowering::LowerTSSubtypingCompare(GateRef gate)
{
    GateRef frameState = GetFrameState(gate);
    Label levelValid(&builder_);
    Label exit(&builder_);
    GateRef compare = BuildCompareSubTyping(gate, frameState, &levelValid, &exit);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), compare);
}

GateRef TypeHCRLowering::BuildCompareSubTyping(GateRef gate, GateRef frameState, Label *levelValid, Label *exit)
{
    GateRef receiver = acc_.GetValueIn(gate, 0);
    bool isHeapObject = acc_.GetObjectTypeAccessor(gate).IsHeapObject();
    if (!isHeapObject) {
        builder_.HeapObjectCheck(receiver, frameState);
    }

    GateRef aotHCIndex = acc_.GetValueIn(gate, 1);
    ArgumentAccessor argAcc(circuit_);
    GateRef jsFunc = argAcc.GetFrameArgsIn(frameState, FrameArgIdx::FUNC);
    JSTaggedValue aotHC = tsManager_->GetAOTHClassInfoByIndex(acc_.TryGetValue(aotHCIndex));
    ASSERT(aotHC.IsJSHClass());

    int32_t level = JSHClass::Cast(aotHC.GetTaggedObject())->GetLevel();
    ASSERT(level >= 0);

    GateRef receiverHClass = builder_.LoadConstOffset(
        VariableType::JS_POINTER(), receiver, TaggedObject::HCLASS_OFFSET);
    GateRef supers = LoadSupers(receiverHClass);

    auto hclassIndex = acc_.GetConstantValue(aotHCIndex);
    GateRef aotHCGate = LoadFromConstPool(jsFunc, hclassIndex, ConstantPool::AOT_HCLASS_INFO_INDEX);

    if (LIKELY(static_cast<uint32_t>(level) < SubtypingOperator::DEFAULT_SUPERS_CAPACITY)) {
        return builder_.Equal(aotHCGate, GetValueFromSupers(supers, level), "checkHClass");
    }

    DEFVALUE(check, (&builder_), VariableType::BOOL(), builder_.False());
    GateRef levelGate = builder_.Int32(level);
    GateRef length = GetLengthFromSupers(supers);

    GateRef cmp = builder_.Int32LessThan(levelGate, length, "checkSubtyping");
    builder_.Branch(cmp, levelValid, exit, BranchWeight::DEOPT_WEIGHT, BranchWeight::ONE_WEIGHT);
    builder_.Bind(levelValid);
    {
        check = builder_.Equal(aotHCGate, GetValueFromSupers(supers, level), "checkSubtyping");
        builder_.Jump(exit);
    }
    builder_.Bind(exit);
    return *check;
}

GateRef TypeHCRLowering::BuildCompareHClass(GateRef gate, GateRef frameState)
{
    GateRef receiver = acc_.GetValueIn(gate, 0);
    bool isHeapObject = acc_.GetObjectTypeAccessor(gate).IsHeapObject();
    if (!isHeapObject) {
        builder_.HeapObjectCheck(receiver, frameState);
    }

    GateRef aotHCIndex = acc_.GetValueIn(gate, 1);
    auto hclassIndex = acc_.GetConstantValue(aotHCIndex);
    ArgumentAccessor argAcc(circuit_);
    GateRef jsFunc = argAcc.GetFrameArgsIn(frameState, FrameArgIdx::FUNC);
    GateRef aotHCGate = LoadFromConstPool(jsFunc, hclassIndex, ConstantPool::AOT_HCLASS_INFO_INDEX);
    GateRef receiverHClass = builder_.LoadConstOffset(
        VariableType::JS_POINTER(), receiver, TaggedObject::HCLASS_OFFSET);
    return builder_.Equal(aotHCGate, receiverHClass, "checkHClass");
}

void TypeHCRLowering::LowerRangeCheckPredicate(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto deoptType = DeoptType::NOTARRAY;
    GateRef frameState = GetFrameState(gate);
    GateRef x = acc_.GetValueIn(gate, 0);
    GateRef y = acc_.GetValueIn(gate, 1);
    TypedBinaryAccessor accessor = acc_.GetTypedBinaryAccessor(gate);
    TypedBinOp cond = accessor.GetTypedBinOp();
    GateRef check = Circuit::NullGate();
    // check the condition
    switch (cond) {
        case TypedBinOp::TYPED_GREATER:
            check = builder_.Int32GreaterThan(x, y);
            break;
        case TypedBinOp::TYPED_GREATEREQ:
            check = builder_.Int32GreaterThanOrEqual(x, y);
            break;
        case TypedBinOp::TYPED_LESS:
            check = builder_.Int32LessThan(x, y);
            break;
        case TypedBinOp::TYPED_LESSEQ:
            check = builder_.Int32LessThanOrEqual(x, y);
            break;
        default:
            UNREACHABLE();
            break;
    }
    builder_.DeoptCheck(check, frameState, deoptType);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerBuiltinPrototypeHClassCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    BuiltinPrototypeHClassAccessor accessor = acc_.GetBuiltinHClassAccessor(gate);
    BuiltinTypeId type = accessor.GetBuiltinTypeId();
    GateRef frameState = GetFrameState(gate);
    GateRef glue = acc_.GetGlueFromArgList();

    GateRef receiver = acc_.GetValueIn(gate, 0);
    builder_.HeapObjectCheck(receiver, frameState);

    JSThread *thread = tsManager_->GetThread();
    // Only HClasses recorded in the JSThread during builtin initialization are available
    [[maybe_unused]] JSHClass *initialPrototypeHClass = thread->GetBuiltinPrototypeHClass(type);
    ASSERT(initialPrototypeHClass != nullptr);

    // Phc = PrototypeHClass
    size_t phcOffset = JSThread::GlueData::GetBuiltinPrototypeHClassOffset(type, env.IsArch32Bit());
    GateRef receiverPhcAddress = builder_.LoadPrototypeHClass(receiver);
    GateRef initialPhcAddress = builder_.LoadConstOffset(VariableType::JS_POINTER(), glue, phcOffset);
    GateRef phcMatches = builder_.Equal(receiverPhcAddress, initialPhcAddress);
    // De-opt if HClass of X.prototype changed where X is the current builtin object.
    builder_.DeoptCheck(phcMatches, frameState, DeoptType::BUILTINPROTOHCLASSMISMATCH);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerIndexCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto deoptType = DeoptType::NOTLEGALIDX;

    GateRef frameState = GetFrameState(gate);
    GateRef length = acc_.GetValueIn(gate, 0);
    GateRef index = acc_.GetValueIn(gate, 1);
    ASSERT(acc_.GetGateType(length).IsNJSValueType());
    // UnsignedLessThan can check both lower and upper bounds
    GateRef lengthCheck = builder_.Int32UnsignedLessThan(index, length);
    builder_.DeoptCheck(lengthCheck, frameState, deoptType);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), index);
}

GateRef TypeHCRLowering::LowerCallRuntime(GateRef glue, GateRef hirGate, int index, const std::vector<GateRef> &args,
                                          bool useLabel)
{
    if (useLabel) {
        GateRef result = builder_.CallRuntime(glue, index, Gate::InvalidGateRef, args, hirGate);
        return result;
    } else {
        const CallSignature *cs = RuntimeStubCSigns::Get(RTSTUB_ID(CallRuntime));
        GateRef target = builder_.IntPtr(index);
        GateRef result = builder_.Call(cs, glue, target, dependEntry_, args, hirGate);
        return result;
    }
}

void TypeHCRLowering::LowerTypeConvert(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto leftType = acc_.GetLeftType(gate);
    auto rightType = acc_.GetRightType(gate);
    if (rightType.IsNumberType()) {
        GateRef value = acc_.GetValueIn(gate, 0);
        if (leftType.IsDigitablePrimitiveType()) {
            LowerPrimitiveToNumber(gate, value, leftType);
        }
        return;
    }
}

void TypeHCRLowering::LowerPrimitiveToNumber(GateRef dst, GateRef src, GateType srcType)
{
    DEFVALUE(result, (&builder_), VariableType::JS_ANY(), builder_.HoleConstant());
    if (srcType.IsBooleanType()) {
        Label exit(&builder_);
        Label isTrue(&builder_);
        Label isFalse(&builder_);
        builder_.Branch(builder_.TaggedIsTrue(src), &isTrue, &isFalse);
        builder_.Bind(&isTrue);
        {
            result = IntToTaggedIntPtr(builder_.Int32(1));
            builder_.Jump(&exit);
        }
        builder_.Bind(&isFalse);
        {
            result = IntToTaggedIntPtr(builder_.Int32(0));
            builder_.Jump(&exit);
        }
        builder_.Bind(&exit);
    } else if (srcType.IsUndefinedType()) {
        result = DoubleToTaggedDoublePtr(builder_.Double(base::NAN_VALUE));
    } else if (srcType.IsBigIntType() || srcType.IsNumberType()) {
        result = src;
    } else if (srcType.IsNullType()) {
        result = IntToTaggedIntPtr(builder_.Int32(0));
    } else {
        LOG_ECMA(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
    acc_.ReplaceGate(dst, builder_.GetState(), builder_.GetDepend(), *result);
}

GateRef TypeHCRLowering::LoadFromConstPool(GateRef jsFunc, size_t index, size_t valVecType)
{
    GateRef constPool = builder_.GetConstPool(jsFunc);
    GateRef constPoolSize = builder_.GetLengthOfTaggedArray(constPool);
    GateRef valVecIndex = builder_.Int32Sub(constPoolSize, builder_.Int32(valVecType));
    GateRef valVec = builder_.GetValueFromTaggedArray(constPool, valVecIndex);
    return builder_.LoadFromTaggedArray(valVec, index);
}

GateRef TypeHCRLowering::GetObjectFromConstPool(GateRef jsFunc, GateRef index)
{
    GateRef constPool = builder_.GetConstPool(jsFunc);
    return builder_.GetValueFromTaggedArray(constPool, index);
}

void TypeHCRLowering::LowerLoadProperty(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    ASSERT(acc_.GetNumValueIn(gate) == 2);  // 2: receiver, plr
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef propertyLookupResult = acc_.GetValueIn(gate, 1);
    PropertyLookupResult plr(acc_.TryGetValue(propertyLookupResult));
    ASSERT(plr.IsLocal() || plr.IsFunction());

    GateRef result = Circuit::NullGate();
    if (plr.IsNotHole()) {
        ASSERT(plr.IsLocal());
        if (plr.IsInlinedProps()) {
            result = builder_.LoadConstOffset(VariableType::JS_ANY(), receiver, plr.GetOffset());
        } else {
            auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), receiver, JSObject::PROPERTIES_OFFSET);
            result = builder_.GetValueFromTaggedArray(properties, builder_.Int32(plr.GetOffset()));
        }
    } else if (plr.IsLocal()) {
        if (plr.IsInlinedProps()) {
            result = builder_.LoadConstOffset(VariableType::JS_ANY(), receiver, plr.GetOffset());
        } else {
            auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), receiver, JSObject::PROPERTIES_OFFSET);
            result = builder_.GetValueFromTaggedArray(properties, builder_.Int32(plr.GetOffset()));
        }
        result = builder_.ConvertHoleAsUndefined(result);
    } else {
        UNREACHABLE();
    }

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void TypeHCRLowering::LowerCallGetter(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    ASSERT(acc_.GetNumValueIn(gate) == 3);  // 3: receiver, holder, plr
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef propertyLookupResult = acc_.GetValueIn(gate, 1);
    GateRef holder = acc_.GetValueIn(gate, 2);
    PropertyLookupResult plr(acc_.TryGetValue(propertyLookupResult));

    GateRef accessor = Circuit::NullGate();
    if (plr.IsNotHole()) {
        ASSERT(plr.IsLocal());
        if (plr.IsInlinedProps()) {
            accessor = builder_.LoadConstOffset(VariableType::JS_ANY(), holder, plr.GetOffset());
        } else {
            auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), holder, JSObject::PROPERTIES_OFFSET);
            accessor = builder_.GetValueFromTaggedArray(properties, builder_.Int32(plr.GetOffset()));
        }
    } else if (plr.IsLocal()) {
        if (plr.IsInlinedProps()) {
            accessor = builder_.LoadConstOffset(VariableType::JS_ANY(), holder, plr.GetOffset());
        } else {
            auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), holder, JSObject::PROPERTIES_OFFSET);
            accessor = builder_.GetValueFromTaggedArray(properties, builder_.Int32(plr.GetOffset()));
        }
        accessor = builder_.ConvertHoleAsUndefined(accessor);
    } else {
        UNREACHABLE();
    }

    DEFVALUE(result, (&builder_), VariableType::JS_ANY(), builder_.UndefineConstant());
    Label isInternalAccessor(&builder_);
    Label notInternalAccessor(&builder_);
    Label callGetter(&builder_);
    Label exit(&builder_);
    builder_.Branch(builder_.IsAccessorInternal(accessor), &isInternalAccessor, &notInternalAccessor);
    {
        builder_.Bind(&isInternalAccessor);
        {
            result = builder_.CallRuntime(glue, RTSTUB_ID(CallInternalGetter),
                Gate::InvalidGateRef, { accessor, holder }, gate);
            builder_.Jump(&exit);
        }
        builder_.Bind(&notInternalAccessor);
        {
            GateRef getter = builder_.LoadConstOffset(VariableType::JS_ANY(), accessor, AccessorData::GETTER_OFFSET);
            builder_.Branch(builder_.IsSpecial(getter, JSTaggedValue::VALUE_UNDEFINED), &exit, &callGetter);
            builder_.Bind(&callGetter);
            {
                result = CallAccessor(glue, gate, getter, receiver, AccessorMode::GETTER);
                builder_.Jump(&exit);
            }
        }
    }
    builder_.Bind(&exit);
    ReplaceHirWithPendingException(gate, glue, builder_.GetState(), builder_.GetDepend(), *result);
}

void TypeHCRLowering::LowerStoreProperty(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    ASSERT(acc_.GetNumValueIn(gate) == 3);  // 3: receiver, plr, value
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef propertyLookupResult = acc_.GetValueIn(gate, 1);
    GateRef value = acc_.GetValueIn(gate, 2); // 2: value
    PropertyLookupResult plr(acc_.TryGetValue(propertyLookupResult));
    ASSERT(plr.IsLocal());
    auto op = OpCode(acc_.GetOpCode(gate));
    if (op == OpCode::STORE_PROPERTY) {
        if (plr.IsInlinedProps()) {
            builder_.StoreConstOffset(VariableType::JS_ANY(), receiver, plr.GetOffset(), value);
        } else {
            auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), receiver, JSObject::PROPERTIES_OFFSET);
            builder_.SetValueToTaggedArray(
                VariableType::JS_ANY(), acc_.GetGlueFromArgList(), properties, builder_.Int32(plr.GetOffset()), value);
        }
    } else if (op == OpCode::STORE_PROPERTY_NO_BARRIER) {
        if (plr.IsInlinedProps()) {
            builder_.StoreConstOffset(GetVarType(plr), receiver, plr.GetOffset(), value);
        } else {
            auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), receiver, JSObject::PROPERTIES_OFFSET);
            builder_.SetValueToTaggedArray(
                GetVarType(plr), acc_.GetGlueFromArgList(), properties, builder_.Int32(plr.GetOffset()), value);
        }
    } else {
        UNREACHABLE();
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerCallSetter(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    ASSERT(acc_.GetNumValueIn(gate) == 4);  // 4: receiver, holder, plr, value
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef propertyLookupResult = acc_.GetValueIn(gate, 1);
    GateRef holder = acc_.GetValueIn(gate, 2);
    GateRef value = acc_.GetValueIn(gate, 3);

    PropertyLookupResult plr(acc_.TryGetValue(propertyLookupResult));
    ASSERT(plr.IsAccessor());
    GateRef accessor = Circuit::NullGate();
    if (plr.IsNotHole()) {
        ASSERT(plr.IsLocal());
        if (plr.IsInlinedProps()) {
            accessor = builder_.LoadConstOffset(VariableType::JS_ANY(), holder, plr.GetOffset());
        } else {
            auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), holder, JSObject::PROPERTIES_OFFSET);
            accessor = builder_.GetValueFromTaggedArray(properties, builder_.Int32(plr.GetOffset()));
        }
    } else if (plr.IsLocal()) {
        if (plr.IsInlinedProps()) {
            accessor = builder_.LoadConstOffset(VariableType::JS_ANY(), holder, plr.GetOffset());
        } else {
            auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), holder, JSObject::PROPERTIES_OFFSET);
            accessor = builder_.GetValueFromTaggedArray(properties, builder_.Int32(plr.GetOffset()));
        }
        accessor = builder_.ConvertHoleAsUndefined(accessor);
    } else {
        UNREACHABLE();
    }
    Label isInternalAccessor(&builder_);
    Label notInternalAccessor(&builder_);
    Label callSetter(&builder_);
    Label exit(&builder_);
    builder_.Branch(builder_.IsAccessorInternal(accessor), &isInternalAccessor, &notInternalAccessor);
    {
        builder_.Bind(&isInternalAccessor);
        {
            builder_.CallRuntime(glue, RTSTUB_ID(CallInternalSetter),
                Gate::InvalidGateRef, { receiver, accessor, value }, gate);
            builder_.Jump(&exit);
        }
        builder_.Bind(&notInternalAccessor);
        {
            GateRef setter = builder_.LoadConstOffset(VariableType::JS_ANY(), accessor, AccessorData::SETTER_OFFSET);
            builder_.Branch(builder_.IsSpecial(setter, JSTaggedValue::VALUE_UNDEFINED), &exit, &callSetter);
            builder_.Bind(&callSetter);
            {
                CallAccessor(glue, gate, setter, receiver, AccessorMode::SETTER, value);
                builder_.Jump(&exit);
            }
        }
    }
    builder_.Bind(&exit);
    ReplaceHirWithPendingException(gate, glue, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerLoadArrayLength(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef array = acc_.GetValueIn(gate, 0);
    GateRef result = builder_.LoadConstOffset(VariableType::INT32(), array, JSArray::LENGTH_OFFSET);
    acc_.SetGateType(gate, GateType::NJSValue());
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

GateRef TypeHCRLowering::GetElementSize(BuiltinTypeId id)
{
    GateRef elementSize = Circuit::NullGate();
    switch (id) {
        case BuiltinTypeId::INT8_ARRAY:
        case BuiltinTypeId::UINT8_ARRAY:
        case BuiltinTypeId::UINT8_CLAMPED_ARRAY:
            elementSize = builder_.Int32(sizeof(uint8_t));
            break;
        case BuiltinTypeId::INT16_ARRAY:
        case BuiltinTypeId::UINT16_ARRAY:
            elementSize = builder_.Int32(sizeof(uint16_t));
            break;
        case BuiltinTypeId::INT32_ARRAY:
        case BuiltinTypeId::UINT32_ARRAY:
        case BuiltinTypeId::FLOAT32_ARRAY:
            elementSize = builder_.Int32(sizeof(uint32_t));
            break;
        case BuiltinTypeId::FLOAT64_ARRAY:
            elementSize = builder_.Int32(sizeof(double));
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
    return elementSize;
}

VariableType TypeHCRLowering::GetVariableType(BuiltinTypeId id)
{
    VariableType type = VariableType::JS_ANY();
    switch (id) {
        case BuiltinTypeId::INT8_ARRAY:
        case BuiltinTypeId::UINT8_ARRAY:
        case BuiltinTypeId::UINT8_CLAMPED_ARRAY:
            type = VariableType::INT8();
            break;
        case BuiltinTypeId::INT16_ARRAY:
        case BuiltinTypeId::UINT16_ARRAY:
            type = VariableType::INT16();
            break;
        case BuiltinTypeId::INT32_ARRAY:
        case BuiltinTypeId::UINT32_ARRAY:
            type = VariableType::INT32();
            break;
        case BuiltinTypeId::FLOAT32_ARRAY:
            type = VariableType::FLOAT32();
            break;
        case BuiltinTypeId::FLOAT64_ARRAY:
            type = VariableType::FLOAT64();
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
    return type;
}

void TypeHCRLowering::LowerLoadElement(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto op = acc_.GetTypedLoadOp(gate);
    switch (op) {
        case TypedLoadOp::ARRAY_LOAD_INT_ELEMENT:
        case TypedLoadOp::ARRAY_LOAD_DOUBLE_ELEMENT:
        case TypedLoadOp::ARRAY_LOAD_OBJECT_ELEMENT:
        case TypedLoadOp::ARRAY_LOAD_TAGGED_ELEMENT:
            LowerArrayLoadElement(gate, ArrayState::PACKED);
            break;
        case TypedLoadOp::ARRAY_LOAD_HOLE_TAGGED_ELEMENT:
            LowerArrayLoadElement(gate, ArrayState::HOLEY);
            break;
        case TypedLoadOp::INT8ARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::INT8_ARRAY);
            break;
        case TypedLoadOp::UINT8ARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::UINT8_ARRAY);
            break;
        case TypedLoadOp::UINT8CLAMPEDARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::UINT8_CLAMPED_ARRAY);
            break;
        case TypedLoadOp::INT16ARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::INT16_ARRAY);
            break;
        case TypedLoadOp::UINT16ARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::UINT16_ARRAY);
            break;
        case TypedLoadOp::INT32ARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::INT32_ARRAY);
            break;
        case TypedLoadOp::UINT32ARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::UINT32_ARRAY);
            break;
        case TypedLoadOp::FLOAT32ARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::FLOAT32_ARRAY);
            break;
        case TypedLoadOp::FLOAT64ARRAY_LOAD_ELEMENT:
            LowerTypedArrayLoadElement(gate, BuiltinTypeId::FLOAT64_ARRAY);
            break;
        case TypedLoadOp::STRING_LOAD_ELEMENT:
            LowerStringLoadElement(gate);
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
}

void TypeHCRLowering::LowerCowArrayCheck(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    Label notCOWArray(&builder_);
    Label isCOWArray(&builder_);
    builder_.Branch(builder_.IsJsCOWArray(receiver), &isCOWArray, &notCOWArray);
    builder_.Bind(&isCOWArray);
    {
        LowerCallRuntime(glue, gate, RTSTUB_ID(CheckAndCopyArray), {receiver}, true);
        builder_.Jump(&notCOWArray);
    }
    builder_.Bind(&notCOWArray);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

// for JSArray
void TypeHCRLowering::LowerArrayLoadElement(GateRef gate, ArrayState arrayState)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef index = acc_.GetValueIn(gate, 1);
    GateRef element = builder_.LoadConstOffset(VariableType::JS_POINTER(), receiver, JSObject::ELEMENTS_OFFSET);
    GateRef result = builder_.GetValueFromTaggedArray(element, index);
    if (arrayState == ArrayState::HOLEY) {
        result = builder_.ConvertHoleAsUndefined(result);
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void TypeHCRLowering::LowerTypedArrayLoadElement(GateRef gate, BuiltinTypeId id)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef index = acc_.GetValueIn(gate, 1);
    GateRef elementSize = GetElementSize(id);
    GateRef offset = builder_.PtrMul(index, elementSize);
    VariableType type = GetVariableType(id);

    GateRef result = Circuit::NullGate();
    if (IsOnHeap()) {
        result = BuildOnHeapTypedArrayLoadElement(receiver, offset, type);
    } else {
        Label isByteArray(&builder_);
        Label isArrayBuffer(&builder_);
        Label exit(&builder_);
        result = BuildTypedArrayLoadElement(receiver, offset, type, &isByteArray, &isArrayBuffer, &exit);
    }

    switch (id) {
        case BuiltinTypeId::INT8_ARRAY:
            result = builder_.SExtInt8ToInt32(result);
            break;
        case BuiltinTypeId::UINT8_ARRAY:
        case BuiltinTypeId::UINT8_CLAMPED_ARRAY:
            result = builder_.ZExtInt8ToInt32(result);
            break;
        case BuiltinTypeId::INT16_ARRAY:
            result = builder_.SExtInt16ToInt32(result);
            break;
        case BuiltinTypeId::UINT16_ARRAY:
            result = builder_.ZExtInt16ToInt32(result);
            break;
        case BuiltinTypeId::FLOAT32_ARRAY:
            result = builder_.ExtFloat32ToDouble(result);
            break;
        default:
            break;
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

GateRef TypeHCRLowering::BuildOnHeapTypedArrayLoadElement(GateRef receiver, GateRef offset, VariableType type)
{
    GateRef arrbuffer =
        builder_.LoadConstOffset(VariableType::JS_POINTER(), receiver, JSTypedArray::VIEWED_ARRAY_BUFFER_OFFSET);
    GateRef data = builder_.PtrAdd(arrbuffer, builder_.IntPtr(ByteArray::DATA_OFFSET));
    GateRef result = builder_.Load(type, data, offset);
    return result;
}

GateRef TypeHCRLowering::BuildTypedArrayLoadElement(GateRef receiver, GateRef offset, VariableType type,
                                                    Label *isByteArray, Label *isArrayBuffer, Label *exit)
{
    GateRef byteArrayOrArraybuffer =
        builder_.LoadConstOffset(VariableType::JS_POINTER(), receiver, JSTypedArray::VIEWED_ARRAY_BUFFER_OFFSET);
    DEFVALUE(data, (&builder_), VariableType::JS_ANY(), builder_.Undefined());
    DEFVALUE(result, (&builder_), type, builder_.Double(0));

    GateRef isOnHeap = builder_.Load(VariableType::BOOL(), receiver, builder_.IntPtr(JSTypedArray::ON_HEAP_OFFSET));
    builder_.Branch(isOnHeap, isByteArray, isArrayBuffer);
    builder_.Bind(isByteArray);
    {
        data = builder_.PtrAdd(byteArrayOrArraybuffer, builder_.IntPtr(ByteArray::DATA_OFFSET));
        result = builder_.Load(type, *data, offset);
        builder_.Jump(exit);
    }
    builder_.Bind(isArrayBuffer);
    {
        data = builder_.Load(VariableType::JS_POINTER(), byteArrayOrArraybuffer,
                             builder_.IntPtr(JSArrayBuffer::DATA_OFFSET));
        GateRef block = builder_.Load(VariableType::JS_ANY(), *data, builder_.IntPtr(JSNativePointer::POINTER_OFFSET));
        GateRef byteOffset =
            builder_.Load(VariableType::INT32(), receiver, builder_.IntPtr(JSTypedArray::BYTE_OFFSET_OFFSET));
        result = builder_.Load(type, block, builder_.PtrAdd(offset, byteOffset));
        builder_.Jump(exit);
    }
    builder_.Bind(exit);

    return *result;
}

void TypeHCRLowering::LowerStringLoadElement(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef glue = acc_.GetGlueFromArgList();
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef index = acc_.GetValueIn(gate, 1);

    GateRef result = builder_.CallStub(glue, gate, CommonStubCSigns::GetSingleCharCodeByIndex,
                                       { glue, receiver, index });
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void TypeHCRLowering::LowerStoreElement(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    auto op = acc_.GetTypedStoreOp(gate);
    switch (op) {
        case TypedStoreOp::ARRAY_STORE_ELEMENT:
            LowerArrayStoreElement(gate, glue);
            break;
        case TypedStoreOp::INT8ARRAY_STORE_ELEMENT:
            LowerTypedArrayStoreElement(gate, BuiltinTypeId::INT8_ARRAY);
            break;
        case TypedStoreOp::UINT8ARRAY_STORE_ELEMENT:
            LowerTypedArrayStoreElement(gate, BuiltinTypeId::UINT8_ARRAY);
            break;
        case TypedStoreOp::UINT8CLAMPEDARRAY_STORE_ELEMENT:
            LowerUInt8ClampedArrayStoreElement(gate);
            break;
        case TypedStoreOp::INT16ARRAY_STORE_ELEMENT:
            LowerTypedArrayStoreElement(gate, BuiltinTypeId::INT16_ARRAY);
            break;
        case TypedStoreOp::UINT16ARRAY_STORE_ELEMENT:
            LowerTypedArrayStoreElement(gate, BuiltinTypeId::UINT16_ARRAY);
            break;
        case TypedStoreOp::INT32ARRAY_STORE_ELEMENT:
            LowerTypedArrayStoreElement(gate, BuiltinTypeId::INT32_ARRAY);
            break;
        case TypedStoreOp::UINT32ARRAY_STORE_ELEMENT:
            LowerTypedArrayStoreElement(gate, BuiltinTypeId::UINT32_ARRAY);
            break;
        case TypedStoreOp::FLOAT32ARRAY_STORE_ELEMENT:
            LowerTypedArrayStoreElement(gate, BuiltinTypeId::FLOAT32_ARRAY);
            break;
        case TypedStoreOp::FLOAT64ARRAY_STORE_ELEMENT:
            LowerTypedArrayStoreElement(gate, BuiltinTypeId::FLOAT64_ARRAY);
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
}

// for JSArray
void TypeHCRLowering::LowerArrayStoreElement(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);  // 0: receiver
    GateRef index = acc_.GetValueIn(gate, 1);     // 1: index
    GateRef value = acc_.GetValueIn(gate, 2);     // 2: value

    GateRef element = builder_.LoadConstOffset(VariableType::JS_POINTER(), receiver, JSObject::ELEMENTS_OFFSET);
    builder_.SetValueToTaggedArray(VariableType::JS_ANY(), glue, element, index, value);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

// for JSTypedArray
void TypeHCRLowering::LowerTypedArrayStoreElement(GateRef gate, BuiltinTypeId id)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef index = acc_.GetValueIn(gate, 1);
    GateRef value = acc_.GetValueIn(gate, 2);

    GateRef elementSize = GetElementSize(id);
    GateRef offset = builder_.PtrMul(index, elementSize);
    switch (id) {
        case BuiltinTypeId::INT8_ARRAY:
        case BuiltinTypeId::UINT8_ARRAY:
            value = builder_.TruncInt32ToInt8(value);
            break;
        case BuiltinTypeId::INT16_ARRAY:
        case BuiltinTypeId::UINT16_ARRAY:
            value = builder_.TruncInt32ToInt16(value);
            break;
        case BuiltinTypeId::FLOAT32_ARRAY:
            value = builder_.TruncDoubleToFloat32(value);
            break;
        default:
            break;
    }

    if (IsOnHeap()) {
        BuildOnHeapTypedArrayStoreElement(receiver, offset, value);
    } else {
        Label isByteArray(&builder_);
        Label isArrayBuffer(&builder_);
        Label exit(&builder_);
        BuildTypedArrayStoreElement(receiver, offset, value, &isByteArray, &isArrayBuffer, &exit);
    }

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::BuildOnHeapTypedArrayStoreElement(GateRef receiver, GateRef offset, GateRef value)
{
    GateRef arrbuffer = builder_.LoadConstOffset(VariableType::JS_POINTER(), receiver,
                                                 JSTypedArray::VIEWED_ARRAY_BUFFER_OFFSET);
    GateRef data = builder_.PtrAdd(arrbuffer, builder_.IntPtr(ByteArray::DATA_OFFSET));

    builder_.StoreMemory(MemoryType::ELEMENT_TYPE, VariableType::VOID(), data, offset, value);
}

void TypeHCRLowering::BuildTypedArrayStoreElement(GateRef receiver, GateRef offset, GateRef value,
                                                  Label *isByteArray, Label *isArrayBuffer, Label *exit)
{
    GateRef byteArrayOrArraybuffer = builder_.LoadConstOffset(VariableType::JS_POINTER(), receiver,
                                                              JSTypedArray::VIEWED_ARRAY_BUFFER_OFFSET);
    GateRef isOnHeap = builder_.Load(VariableType::BOOL(), receiver, builder_.IntPtr(JSTypedArray::ON_HEAP_OFFSET));
    DEFVALUE(data, (&builder_), VariableType::JS_ANY(), builder_.Undefined());
    builder_.Branch(isOnHeap, isByteArray, isArrayBuffer);
    builder_.Bind(isByteArray);
    {
        data = builder_.PtrAdd(byteArrayOrArraybuffer, builder_.IntPtr(ByteArray::DATA_OFFSET));
        builder_.StoreMemory(MemoryType::ELEMENT_TYPE, VariableType::VOID(), *data, offset, value);
        builder_.Jump(exit);
    }
    builder_.Bind(isArrayBuffer);
    {
        data = builder_.Load(VariableType::JS_POINTER(), byteArrayOrArraybuffer,
                             builder_.IntPtr(JSArrayBuffer::DATA_OFFSET));
        GateRef block = builder_.Load(VariableType::JS_ANY(), *data, builder_.IntPtr(JSNativePointer::POINTER_OFFSET));
        GateRef byteOffset =
            builder_.Load(VariableType::INT32(), receiver, builder_.IntPtr(JSTypedArray::BYTE_OFFSET_OFFSET));
        builder_.StoreMemory(MemoryType::ELEMENT_TYPE, VariableType::VOID(), block,
                             builder_.PtrAdd(offset, byteOffset), value);
        builder_.Jump(exit);
    }
    builder_.Bind(exit);
}

// for UInt8ClampedArray
void TypeHCRLowering::LowerUInt8ClampedArrayStoreElement(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);

    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef index = acc_.GetValueIn(gate, 1);
    GateRef elementSize = builder_.Int32(sizeof(uint8_t));
    GateRef offset = builder_.PtrMul(index, elementSize);
    GateRef value = acc_.GetValueIn(gate, 2);

    DEFVALUE(result, (&builder_), VariableType::INT32(), value);
    GateRef topValue = builder_.Int32(static_cast<uint32_t>(UINT8_MAX));
    GateRef bottomValue = builder_.Int32(static_cast<uint32_t>(0));
    Label isOverFlow(&builder_);
    Label notOverFlow(&builder_);
    Label exit(&builder_);
    builder_.Branch(builder_.Int32GreaterThan(value, topValue), &isOverFlow, &notOverFlow);
    builder_.Bind(&isOverFlow);
    {
        result = topValue;
        builder_.Jump(&exit);
    }
    builder_.Bind(&notOverFlow);
    {
        Label isUnderSpill(&builder_);
        builder_.Branch(builder_.Int32LessThan(value, bottomValue), &isUnderSpill, &exit);
        builder_.Bind(&isUnderSpill);
        {
            result = bottomValue;
            builder_.Jump(&exit);
        }
    }
    builder_.Bind(&exit);
    value = builder_.TruncInt32ToInt8(*result);

    GateRef arrbuffer = builder_.LoadConstOffset(VariableType::JS_POINTER(), receiver,
        JSTypedArray::VIEWED_ARRAY_BUFFER_OFFSET);

    GateRef data = builder_.PtrAdd(arrbuffer, builder_.IntPtr(ByteArray::DATA_OFFSET));

    builder_.StoreMemory(MemoryType::ELEMENT_TYPE, VariableType::VOID(), data, offset, value);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

GateRef TypeHCRLowering::DoubleToTaggedDoublePtr(GateRef gate)
{
    return builder_.DoubleToTaggedDoublePtr(gate);
}

GateRef TypeHCRLowering::ChangeInt32ToFloat64(GateRef gate)
{
    return builder_.ChangeInt32ToFloat64(gate);
}

GateRef TypeHCRLowering::TruncDoubleToInt(GateRef gate)
{
    return builder_.TruncInt64ToInt32(builder_.TruncFloatToInt64(gate));
}

GateRef TypeHCRLowering::IntToTaggedIntPtr(GateRef x)
{
    GateRef val = builder_.SExtInt32ToInt64(x);
    return builder_.ToTaggedIntPtr(val);
}

void TypeHCRLowering::LowerTypedCallBuitin(GateRef gate)
{
    BuiltinLowering lowering(circuit_);
    lowering.LowerTypedCallBuitin(gate);
}

void TypeHCRLowering::LowerJSCallTargetFromDefineFuncCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    if (tsManager_->IsFunctionTypeKind(type)) {
        GateRef frameState = GetFrameState(gate);
        auto func = acc_.GetValueIn(gate, 0);
        GateRef check = builder_.IsOptimized(func);
        builder_.DeoptCheck(check, frameState, DeoptType::NOTJSCALLTGT);
        acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerJSCallTargetTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    if (tsManager_->IsFunctionTypeKind(type)) {
        ArgumentAccessor argAcc(circuit_);
        GateRef frameState = GetFrameState(gate);
        GateRef jsFunc = argAcc.GetFrameArgsIn(frameState, FrameArgIdx::FUNC);
        auto func = acc_.GetValueIn(gate, 0);
        auto methodIndex = acc_.GetValueIn(gate, 1);
        GateRef isObj = builder_.TaggedIsHeapObject(func);
        GateRef isOptimized = builder_.IsOptimized(func);
        GateRef funcMethodTarget = builder_.GetMethodFromFunction(func);
        GateRef checkFunc = builder_.BoolAnd(isObj, isOptimized);
        GateRef methodTarget = GetObjectFromConstPool(jsFunc, methodIndex);
        GateRef check = builder_.BoolAnd(checkFunc, builder_.Equal(funcMethodTarget, methodTarget));
        builder_.DeoptCheck(check, frameState, DeoptType::NOTJSCALLTGT);
        acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerJSFastCallTargetTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    if (tsManager_->IsFunctionTypeKind(type)) {
        ArgumentAccessor argAcc(circuit_);
        GateRef frameState = GetFrameState(gate);
        GateRef jsFunc = argAcc.GetFrameArgsIn(frameState, FrameArgIdx::FUNC);
        auto func = acc_.GetValueIn(gate, 0);
        auto methodIndex = acc_.GetValueIn(gate, 1);
        GateRef isObj = builder_.TaggedIsHeapObject(func);
        GateRef canFastCall = builder_.CanFastCall(func);
        GateRef funcMethodTarget = builder_.GetMethodFromFunction(func);
        GateRef checkFunc = builder_.BoolAnd(isObj, canFastCall);
        GateRef methodTarget = GetObjectFromConstPool(jsFunc, methodIndex);
        GateRef check = builder_.BoolAnd(checkFunc, builder_.Equal(funcMethodTarget, methodTarget));
        builder_.DeoptCheck(check, frameState, DeoptType::NOTJSFASTCALLTGT);
        acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerJSCallThisTargetTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    if (tsManager_->IsFunctionTypeKind(type)) {
        GateRef frameState = GetFrameState(gate);
        auto func = acc_.GetValueIn(gate, 0);
        GateRef isObj = builder_.TaggedIsHeapObject(func);
        GateRef isOptimized = builder_.IsOptimizedAndNotFastCall(func);
        GateRef check = builder_.BoolAnd(isObj, isOptimized);
        builder_.DeoptCheck(check, frameState, DeoptType::NOTJSCALLTGT);
        acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerJSNoGCCallThisTargetTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    if (tsManager_->IsFunctionTypeKind(type)) {
        GateRef frameState = GetFrameState(gate);
        auto func = acc_.GetValueIn(gate, 0);
        GateRef isObj = builder_.TaggedIsHeapObject(func);
        GateRef isOptimized = builder_.IsOptimizedAndNotFastCall(func);
        GateRef methodId = builder_.GetMethodId(func);
        GateRef checkOptimized = builder_.BoolAnd(isObj, isOptimized);
        GateRef check = builder_.BoolAnd(checkOptimized, builder_.Equal(methodId, acc_.GetValueIn(gate, 1)));
        builder_.DeoptCheck(check, frameState, DeoptType::NOTJSCALLTGT);
        acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerJSFastCallThisTargetTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    if (tsManager_->IsFunctionTypeKind(type)) {
        GateRef frameState = GetFrameState(gate);
        auto func = acc_.GetValueIn(gate, 0);
        GateRef isObj = builder_.TaggedIsHeapObject(func);
        GateRef canFastCall = builder_.CanFastCall(func);
        GateRef check = builder_.BoolAnd(isObj, canFastCall);
        builder_.DeoptCheck(check, frameState, DeoptType::NOTJSFASTCALLTGT);
        acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerJSNoGCFastCallThisTargetTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    auto type = acc_.GetParamGateType(gate);
    if (tsManager_->IsFunctionTypeKind(type)) {
        GateRef frameState = GetFrameState(gate);
        auto func = acc_.GetValueIn(gate, 0);
        GateRef isObj = builder_.TaggedIsHeapObject(func);
        GateRef canFastCall = builder_.CanFastCall(func);
        GateRef methodId = builder_.GetMethodId(func);
        GateRef checkOptimized = builder_.BoolAnd(isObj, canFastCall);
        GateRef check = builder_.BoolAnd(checkOptimized, builder_.Equal(methodId, acc_.GetValueIn(gate, 1)));
        builder_.DeoptCheck(check, frameState, DeoptType::NOTJSFASTCALLTGT);
        acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
    } else {
        LOG_COMPILER(FATAL) << "this branch is unreachable";
        UNREACHABLE();
    }
}

void TypeHCRLowering::LowerCallTargetCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = GetFrameState(gate);

    BuiltinLowering lowering(circuit_);
    GateRef funcheck = lowering.LowerCallTargetCheck(&env, gate);
    GateRef check = lowering.CheckPara(gate, funcheck);
    builder_.DeoptCheck(check, frameState, DeoptType::NOTCALLTGT);

    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerJSInlineTargetTypeCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = GetFrameState(gate);
    auto func = acc_.GetValueIn(gate, 0);
    GateRef isObj = builder_.TaggedIsHeapObject(func);
    GateRef isJsFunc = builder_.IsJSFunction(func);
    GateRef checkFunc = builder_.BoolAnd(isObj, isJsFunc);
    GateRef GetMethodId = builder_.GetMethodId(func);
    GateRef check = builder_.BoolAnd(checkFunc, builder_.Equal(GetMethodId, acc_.GetValueIn(gate, 1)));
    builder_.DeoptCheck(check, frameState, DeoptType::INLINEFAIL);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerTypedNewAllocateThis(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    ArgumentAccessor argAcc(circuit_);
    GateRef frameState = GetFrameState(gate);

    GateRef ctor = acc_.GetValueIn(gate, 0);

    GateRef isObj = builder_.TaggedIsHeapObject(ctor);
    GateRef isJsFunc = builder_.IsJSFunction(ctor);
    GateRef checkFunc = builder_.BoolAnd(isObj, isJsFunc);
    GateRef check = builder_.BoolAnd(checkFunc, builder_.IsConstructor(ctor));
    builder_.DeoptCheck(check, frameState, DeoptType::NOTNEWOBJ);

    DEFVALUE(thisObj, (&builder_), VariableType::JS_ANY(), builder_.Undefined());
    Label allocate(&builder_);
    Label exit(&builder_);

    GateRef isBase = builder_.IsBase(ctor);
    builder_.Branch(isBase, &allocate, &exit);
    builder_.Bind(&allocate);
    {
        // add typecheck to detect protoOrHclass is equal with ihclass,
        // if pass typecheck: 1.no need to check whether hclass is valid 2.no need to check return result
        GateRef protoOrHclass = builder_.LoadConstOffset(VariableType::JS_ANY(), ctor,
            JSFunction::PROTO_OR_DYNCLASS_OFFSET);
        GateRef ihclassIndex = acc_.GetValueIn(gate, 1);
        auto hclassIndex = acc_.GetConstantValue(ihclassIndex);
        GateRef ihclass =  builder_.GetHClassGateFromIndex(frameState, hclassIndex);
        GateRef checkProto = builder_.Equal(protoOrHclass, ihclass);
        builder_.DeoptCheck(checkProto, frameState, DeoptType::NOTNEWOBJ);

        thisObj = builder_.CallStub(glue, gate, CommonStubCSigns::NewJSObject, { glue, protoOrHclass });
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), *thisObj);
}

void TypeHCRLowering::LowerTypedSuperAllocateThis(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef superCtor = acc_.GetValueIn(gate, 0);
    GateRef newTarget = acc_.GetValueIn(gate, 1);

    DEFVALUE(thisObj, (&builder_), VariableType::JS_ANY(), builder_.Undefined());
    Label allocate(&builder_);
    Label exit(&builder_);

    GateRef isBase = builder_.IsBase(superCtor);
    builder_.Branch(isBase, &allocate, &exit);
    builder_.Bind(&allocate);
    {
        GateRef protoOrHclass = builder_.LoadConstOffset(VariableType::JS_ANY(), newTarget,
            JSFunction::PROTO_OR_DYNCLASS_OFFSET);
        GateRef check = builder_.IsJSHClass(protoOrHclass);
        GateRef frameState = GetFrameState(gate);
        builder_.DeoptCheck(check, frameState, DeoptType::NOTNEWOBJ);

        thisObj = builder_.CallStub(glue, gate, CommonStubCSigns::NewJSObject, { glue, protoOrHclass });
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), *thisObj);
}

void TypeHCRLowering::LowerGetSuperConstructor(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef ctor = acc_.GetValueIn(gate, 0);
    GateRef hclass = builder_.LoadHClass(ctor);
    GateRef superCtor = builder_.LoadConstOffset(VariableType::JS_ANY(), hclass, JSHClass::PROTOTYPE_OFFSET);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), superCtor);
}

GateRef TypeHCRLowering::LoadFromVTable(GateRef receiver, size_t index)
{
    GateRef hclass = builder_.LoadConstOffset(
        VariableType::JS_POINTER(), receiver, TaggedObject::HCLASS_OFFSET);
    GateRef vtable = builder_.LoadConstOffset(VariableType::JS_ANY(),
        hclass, JSHClass::VTABLE_OFFSET);

    GateRef itemOwner = builder_.LoadFromTaggedArray(vtable, VTable::TupleItem::OWNER + index);
    GateRef itemOffset = builder_.LoadFromTaggedArray(vtable, VTable::TupleItem::OFFSET + index);
    return builder_.Load(VariableType::JS_ANY(), itemOwner, builder_.TaggedGetInt(itemOffset));
}

VariableType TypeHCRLowering::GetVarType(PropertyLookupResult plr)
{
    if (plr.GetRepresentation() == Representation::DOUBLE) {
        return kungfu::VariableType::FLOAT64();
    } else if (plr.GetRepresentation() == Representation::INT) {
        return kungfu::VariableType::INT32();
    } else {
        return kungfu::VariableType::INT64();
    }
}

GateRef TypeHCRLowering::LoadSupers(GateRef hclass)
{
    return builder_.LoadConstOffset(VariableType::JS_ANY(), hclass, JSHClass::SUPERS_OFFSET);
}

GateRef TypeHCRLowering::GetLengthFromSupers(GateRef supers)
{
    return builder_.LoadConstOffset(VariableType::INT32(), supers, TaggedArray::EXTRACT_LENGTH_OFFSET);
}

GateRef TypeHCRLowering::GetValueFromSupers(GateRef supers, size_t index)
{
    GateRef val = builder_.LoadFromTaggedArray(supers, index);
    return builder_.LoadObjectFromWeakRef(val);
}

GateRef TypeHCRLowering::CallAccessor(GateRef glue, GateRef gate, GateRef function, GateRef receiver,
    AccessorMode mode, GateRef value)
{
    const CallSignature *cs = RuntimeStubCSigns::Get(RTSTUB_ID(JSCall));
    GateRef target = builder_.IntPtr(RTSTUB_ID(JSCall));
    GateRef newTarget = builder_.Undefined();
    GateRef argc = builder_.Int64(NUM_MANDATORY_JSFUNC_ARGS + (mode == AccessorMode::SETTER ? 1 : 0));  // 1: value
    std::vector<GateRef> args { glue, argc, function, newTarget, receiver };
    if (mode == AccessorMode::SETTER) {
        args.emplace_back(value);
    }

    return builder_.Call(cs, glue, target, builder_.GetDepend(), args, gate);
}

void TypeHCRLowering::ReplaceHirWithPendingException(GateRef hirGate, GateRef glue, GateRef state, GateRef depend,
                                                     GateRef value)
{
    auto condition = builder_.HasPendingException(glue);
    GateRef ifBranch = builder_.Branch(state, condition);
    GateRef ifTrue = builder_.IfTrue(ifBranch);
    GateRef ifFalse = builder_.IfFalse(ifBranch);
    GateRef eDepend = builder_.DependRelay(ifTrue, depend);
    GateRef sDepend = builder_.DependRelay(ifFalse, depend);

    StateDepend success(ifFalse, sDepend);
    StateDepend exception(ifTrue, eDepend);
    acc_.ReplaceHirWithIfBranch(hirGate, success, exception, value);
}

void TypeHCRLowering::LowerLoadGetter(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    ASSERT(acc_.GetNumValueIn(gate) == 3);  // 3: receiver, holderHC, plr
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef holderHC = acc_.GetValueIn(gate, 1);
    GateRef propertyLookupResult = acc_.GetValueIn(gate, 2);

    PropertyLookupResult plr(acc_.TryGetValue(propertyLookupResult));
    ASSERT(plr.IsAccessor());
    DEFVALUE(holder, (&builder_), VariableType::JS_ANY(), receiver);
    // lookup from receiver for holder
    Label loopHead(&builder_);
    Label loadHolder(&builder_);
    Label lookUpProto(&builder_);
    builder_.Jump(&loopHead);

    builder_.LoopBegin(&loopHead);
    auto curHC = builder_.LoadHClass(*holder);
    builder_.Branch(builder_.Equal(curHC, holderHC), &loadHolder, &lookUpProto);

    builder_.Bind(&lookUpProto);
    holder = builder_.LoadConstOffset(VariableType::JS_ANY(), curHC, JSHClass::PROTOTYPE_OFFSET);
    builder_.LoopEnd(&loopHead);

    builder_.Bind(&loadHolder);

    GateRef getter;
    if (plr.IsInlinedProps()) {
        auto acceessorData = builder_.LoadConstOffset(VariableType::JS_ANY(), *holder, plr.GetOffset());
        getter = builder_.LoadConstOffset(VariableType::JS_ANY(), acceessorData, AccessorData::GETTER_OFFSET);
    } else {
        auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), *holder, JSObject::PROPERTIES_OFFSET);
        auto acceessorData = builder_.GetValueFromTaggedArray(properties, builder_.Int32(plr.GetOffset()));
        getter = builder_.LoadConstOffset(VariableType::JS_ANY(), acceessorData, AccessorData::GETTER_OFFSET);
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), getter);
}

void TypeHCRLowering::LowerLoadSetter(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    ASSERT(acc_.GetNumValueIn(gate) == 3);  // 3: receiver, holderHC, plr
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef holderHC = acc_.GetValueIn(gate, 1);
    GateRef propertyLookupResult = acc_.GetValueIn(gate, 2);

    PropertyLookupResult plr(acc_.TryGetValue(propertyLookupResult));
    ASSERT(plr.IsAccessor());
    DEFVALUE(holder, (&builder_), VariableType::JS_ANY(), receiver);
    // lookup from receiver for holder
    Label loopHead(&builder_);
    Label loadHolder(&builder_);
    Label lookUpProto(&builder_);
    builder_.Jump(&loopHead);

    builder_.LoopBegin(&loopHead);
    auto curHC = builder_.LoadHClass(*holder);
    builder_.Branch(builder_.Equal(curHC, holderHC), &loadHolder, &lookUpProto);

    builder_.Bind(&lookUpProto);
    holder = builder_.LoadConstOffset(VariableType::JS_ANY(), curHC, JSHClass::PROTOTYPE_OFFSET);
    builder_.LoopEnd(&loopHead);

    builder_.Bind(&loadHolder);

    GateRef setter;
    if (plr.IsInlinedProps()) {
        auto acceessorData = builder_.LoadConstOffset(VariableType::JS_ANY(), *holder, plr.GetOffset());
        setter = builder_.LoadConstOffset(VariableType::JS_ANY(), acceessorData, AccessorData::SETTER_OFFSET);
    } else {
        auto properties = builder_.LoadConstOffset(VariableType::JS_ANY(), *holder, JSObject::PROPERTIES_OFFSET);
        auto acceessorData = builder_.GetValueFromTaggedArray(properties, builder_.Int32(plr.GetOffset()));
        setter = builder_.LoadConstOffset(VariableType::JS_ANY(), acceessorData, AccessorData::SETTER_OFFSET);
    }
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), setter);
}

// subtyping check and hclss check
void TypeHCRLowering::LowerInlineAccessorCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef receiver = acc_.GetValueIn(gate, 0);
    GateRef frameState = acc_.GetFrameState(gate);
    builder_.HeapObjectCheck(receiver, frameState);

    auto receiverHC = builder_.LoadHClass(receiver);
    auto expectedReceiverHC = acc_.GetValueIn(gate, 1);
    GateRef receiverHcCheck = builder_.Equal(receiverHC, expectedReceiverHC);
    auto prototype = builder_.LoadConstOffset(VariableType::JS_ANY(), receiverHC, JSHClass::PROTOTYPE_OFFSET);
    auto protoHClass = builder_.LoadHClass(prototype);
    auto marker = builder_.LoadConstOffset(VariableType::JS_ANY(), protoHClass, JSHClass::PROTO_CHANGE_MARKER_OFFSET);
    auto prototypeHasChanged = builder_.GetHasChanged(marker);
    auto accessorHasChanged = builder_.GetAccessorHasChanged(marker);
    auto markerCheck = builder_.BoolAnd(builder_.BoolNot(prototypeHasChanged), builder_.BoolNot(accessorHasChanged));
    auto check = builder_.BoolAnd(markerCheck, receiverHcCheck);

    builder_.DeoptCheck(check, frameState, DeoptType::INLINEFAIL);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
    return;
}

void TypeHCRLowering::LowerStringEqual(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef left = acc_.GetValueIn(gate, 0);
    GateRef right = acc_.GetValueIn(gate, 1);
    GateRef leftLength = GetLengthFromString(left);
    GateRef rightLength = GetLengthFromString(right);

    DEFVALUE(result, (&builder_), VariableType::BOOL(), builder_.False());
    Label lenEqual(&builder_);
    Label exit(&builder_);
    builder_.Branch(builder_.Equal(leftLength, rightLength), &lenEqual, &exit);
    builder_.Bind(&lenEqual);
    {
        result = builder_.CallStub(glue, gate, CommonStubCSigns::FastStringEqual, { glue, left, right });
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), *result);
}

void TypeHCRLowering::LowerStringAdd(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef left = acc_.GetValueIn(gate, 0);
    GateRef right = acc_.GetValueIn(gate, 1);
    GateRef result = builder_.CallStub(glue, gate, CommonStubCSigns::FastStringAdd, { glue, left, right });;
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void TypeHCRLowering::LowerTypeOfCheck(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = GetFrameState(gate);
    GateRef value = acc_.GetValueIn(gate, 0);
    GateType type = acc_.GetParamGateType(gate);
    GateRef check = Circuit::NullGate();
    if (type.IsNumberType()) {
        check = builder_.TaggedIsNumber(value);
    } else if (type.IsBooleanType()) {
        check = builder_.TaggedIsBoolean(value);
    } else if (type.IsNullType()) {
        check = builder_.TaggedIsNull(value);
    } else if (type.IsUndefinedType()) {
        check = builder_.TaggedIsUndefined(value);
    } else if (type.IsStringType()) {
        check = builder_.BoolAnd(builder_.TaggedIsHeapObject(value), builder_.TaggedIsString(value));
    } else if (type.IsBigIntType()) {
        check = builder_.BoolAnd(builder_.TaggedIsHeapObject(value), builder_.IsJsType(value, JSType::BIGINT));
    } else if (type.IsSymbolType()) {
        check = builder_.BoolAnd(builder_.TaggedIsHeapObject(value), builder_.IsJsType(value, JSType::SYMBOL));
    } else if (tsManager_->IsFunctionTypeKind(type) || tsManager_->IsClassTypeKind(type)) {
        check = builder_.BoolAnd(builder_.TaggedIsHeapObject(value), builder_.IsCallable(value));
    } else if (tsManager_->IsObjectTypeKind(type) || tsManager_->IsClassInstanceTypeKind(type)) {
        check = builder_.BoolAnd(builder_.TaggedIsHeapObject(value), builder_.IsJsType(value, JSType::JS_OBJECT));
    } else if (tsManager_->IsArrayTypeKind(type)) {
        check = builder_.BoolAnd(builder_.TaggedIsHeapObject(value), builder_.IsJsType(value, JSType::JS_ARRAY));
    } else {
        UNREACHABLE();
    }

    builder_.DeoptCheck(check, frameState, DeoptType::INCONSISTENTTYPE);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerTypeOf(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateType type = acc_.GetParamGateType(gate);
    GateRef gConstAddr = builder_.Load(VariableType::JS_POINTER(), glue,
        builder_.IntPtr(JSThread::GlueData::GetGlobalConstOffset(builder_.GetCompilationConfig()->Is32Bit())));
    ConstantIndex index;
    if (type.IsNumberType()) {
        index = ConstantIndex::NUMBER_STRING_INDEX;
    } else if (type.IsBooleanType()) {
        index = ConstantIndex::BOOLEAN_STRING_INDEX;
    } else if (type.IsNullType()) {
        index = ConstantIndex::OBJECT_STRING_INDEX;
    } else if (type.IsUndefinedType()) {
        index = ConstantIndex::UNDEFINED_STRING_INDEX;
    } else if (type.IsStringType()) {
        index = ConstantIndex::STRING_STRING_INDEX;
    } else if (type.IsBigIntType()) {
        index = ConstantIndex::BIGINT_STRING_INDEX;
    } else if (type.IsSymbolType()) {
        index = ConstantIndex::SYMBOL_STRING_INDEX;
    } else if (tsManager_->IsFunctionTypeKind(type) || tsManager_->IsClassTypeKind(type)) {
        index = ConstantIndex::FUNCTION_STRING_INDEX;
    } else if (tsManager_->IsObjectTypeKind(type) || tsManager_->IsClassInstanceTypeKind(type)) {
        index = ConstantIndex::OBJECT_STRING_INDEX;
    } else if (tsManager_->IsArrayTypeKind(type)) {
        index = ConstantIndex::OBJECT_STRING_INDEX;
    } else {
        UNREACHABLE();
    }

    GateRef result = builder_.Load(VariableType::JS_POINTER(), gConstAddr, builder_.GetGlobalConstantOffset(index));
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), result);
}

void TypeHCRLowering::LowerArrayConstructorCheck(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = GetFrameState(gate);
    GateRef newTarget = acc_.GetValueIn(gate, 0);
    Label isHeapObject(&builder_);
    Label exit(&builder_);
    DEFVALUE(check, (&builder_), VariableType::BOOL(), builder_.True());
    check = builder_.TaggedIsHeapObject(newTarget);
    builder_.Branch(*check, &isHeapObject, &exit);
    builder_.Bind(&isHeapObject);
    {
        Label isJSFunction(&builder_);
        check = builder_.IsJSFunction(newTarget);
        builder_.Branch(*check, &isJSFunction, &exit);
        builder_.Bind(&isJSFunction);
        {
            Label getHclass(&builder_);
            GateRef glueGlobalEnvOffset = builder_.IntPtr(
                JSThread::GlueData::GetGlueGlobalEnvOffset(builder_.GetCurrentEnvironment()->Is32Bit()));
            GateRef glueGlobalEnv = builder_.Load(VariableType::NATIVE_POINTER(), glue, glueGlobalEnvOffset);
            GateRef arrayFunc =
                builder_.GetGlobalEnvValue(VariableType::JS_ANY(), glueGlobalEnv, GlobalEnv::ARRAY_FUNCTION_INDEX);
            check = builder_.Equal(arrayFunc, newTarget);
            builder_.Branch(*check, &getHclass, &exit);
            builder_.Bind(&getHclass);
            {
                GateRef intialHClass = builder_.Load(VariableType::JS_ANY(), newTarget,
                                                     builder_.IntPtr(JSFunction::PROTO_OR_DYNCLASS_OFFSET));
                check = builder_.IsJSHClass(intialHClass);
                builder_.Jump(&exit);
            }
        }
    }
    builder_.Bind(&exit);
    builder_.DeoptCheck(*check, frameState, DeoptType::NEWBUILTINCTORFAIL);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerArrayConstructor(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    if (acc_.GetNumValueIn(gate) == 1) {
        NewArrayConstructorWithNoArgs(gate, glue);
        return;
    }
    ASSERT(acc_.GetNumValueIn(gate) == 2); // 2: new target and arg0
    DEFVALUE(res, (&builder_), VariableType::JS_ANY(), builder_.Undefined());
    Label slowPath(&builder_);
    Label exit(&builder_);
    GateRef newTarget = acc_.GetValueIn(gate, 0);
    GateRef arg0 = acc_.GetValueIn(gate, 1);
    GateRef intialHClass =
        builder_.Load(VariableType::JS_ANY(), newTarget, builder_.IntPtr(JSFunction::PROTO_OR_DYNCLASS_OFFSET));
    DEFVALUE(arrayLength, (&builder_), VariableType::INT64(), builder_.Int64(0));
    Label argIsNumber(&builder_);
    Label arrayCreate(&builder_);
    builder_.Branch(builder_.TaggedIsNumber(arg0), &argIsNumber, &slowPath);
    builder_.Bind(&argIsNumber);
    {
        Label argIsInt(&builder_);
        Label argIsDouble(&builder_);
        builder_.Branch(builder_.TaggedIsInt(arg0), &argIsInt, &argIsDouble);
        builder_.Bind(&argIsInt);
        {
            Label validIntLength(&builder_);
            GateRef intLen = builder_.GetInt64OfTInt(arg0);
            GateRef isGEZero = builder_.Int64GreaterThanOrEqual(intLen, builder_.Int64(0));
            GateRef isLEMaxLen = builder_.Int64LessThanOrEqual(intLen, builder_.Int64(JSArray::MAX_ARRAY_INDEX));
            builder_.Branch(builder_.BoolAnd(isGEZero, isLEMaxLen), &validIntLength, &slowPath);
            builder_.Bind(&validIntLength);
            {
                arrayLength = intLen;
                builder_.Jump(&arrayCreate);
            }
        }
        builder_.Bind(&argIsDouble);
        {
            Label validDoubleLength(&builder_);
            Label GetDoubleToIntValue(&builder_);
            GateRef doubleLength = builder_.GetDoubleOfTDouble(arg0);
            GateRef doubleToInt = builder_.DoubleToInt(doubleLength, &GetDoubleToIntValue);
            GateRef intToDouble = builder_.CastInt64ToFloat64(builder_.SExtInt32ToInt64(doubleToInt));
            GateRef doubleEqual = builder_.DoubleEqual(doubleLength, intToDouble);
            GateRef doubleLEMaxLen =
                builder_.DoubleLessThanOrEqual(doubleLength, builder_.Double(JSArray::MAX_ARRAY_INDEX));
            builder_.Branch(builder_.BoolAnd(doubleEqual, doubleLEMaxLen), &validDoubleLength, &slowPath);
            builder_.Bind(&validDoubleLength);
            {
                arrayLength = builder_.SExtInt32ToInt64(doubleToInt);
                builder_.Jump(&arrayCreate);
            }
        }
    }
    builder_.Bind(&arrayCreate);
    {
        Label lengthValid(&builder_);
        builder_.Branch(
            builder_.Int64GreaterThan(*arrayLength, builder_.Int64(JSObject::MAX_GAP)), &slowPath, &lengthValid);
        builder_.Bind(&lengthValid);
        {
            NewObjectStubBuilder newBuilder(builder_.GetCurrentEnvironment());
            newBuilder.SetParameters(glue, 0);
            res = newBuilder.NewJSArrayWithSize(intialHClass, *arrayLength);
            GateRef lengthOffset = builder_.IntPtr(JSArray::LENGTH_OFFSET);
            builder_.Store(VariableType::INT32(), glue, *res, lengthOffset, builder_.TruncInt64ToInt32(*arrayLength));
            GateRef accessor = builder_.GetGlobalConstantValue(ConstantIndex::ARRAY_LENGTH_ACCESSOR);
            builder_.SetPropertyInlinedProps(glue, *res, intialHClass, accessor,
                builder_.Int32(JSArray::LENGTH_INLINE_PROPERTY_INDEX), VariableType::JS_ANY());
            builder_.SetExtensibleToBitfield(glue, *res, true);
            builder_.Jump(&exit);
        }
    }
    builder_.Bind(&slowPath);
    {
        size_t range = acc_.GetNumValueIn(gate);
        std::vector<GateRef> args(range);
        for (size_t i = 0; i < range; ++i) {
            args[i] = acc_.GetValueIn(gate, i);
        }
        res = LowerCallRuntime(glue, gate, RTSTUB_ID(OptNewObjRange), args, true);
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    ReplaceGateWithPendingException(glue, gate, builder_.GetState(), builder_.GetDepend(), *res);
}

void TypeHCRLowering::NewArrayConstructorWithNoArgs(GateRef gate, GateRef glue)
{
    GateRef newTarget = acc_.GetValueIn(gate, 0);
    GateRef intialHClass =
        builder_.Load(VariableType::JS_ANY(), newTarget, builder_.IntPtr(JSFunction::PROTO_OR_DYNCLASS_OFFSET));
    GateRef arrayLength = builder_.Int64(0);
    NewObjectStubBuilder newBuilder(builder_.GetCurrentEnvironment());
    newBuilder.SetParameters(glue, 0);
    GateRef res = newBuilder.NewJSArrayWithSize(intialHClass, arrayLength);
    GateRef lengthOffset = builder_.IntPtr(JSArray::LENGTH_OFFSET);
    builder_.Store(VariableType::INT32(), glue, res, lengthOffset, builder_.TruncInt64ToInt32(arrayLength));
    GateRef accessor = builder_.GetGlobalConstantValue(ConstantIndex::ARRAY_LENGTH_ACCESSOR);
    builder_.SetPropertyInlinedProps(glue, res, intialHClass, accessor,
                                     builder_.Int32(JSArray::LENGTH_INLINE_PROPERTY_INDEX), VariableType::JS_ANY());
    builder_.SetExtensibleToBitfield(glue, res, true);
    ReplaceGateWithPendingException(glue, gate, builder_.GetState(), builder_.GetDepend(), res);
}

void TypeHCRLowering::LowerObjectConstructorCheck(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef frameState = GetFrameState(gate);
    GateRef newTarget = acc_.GetValueIn(gate, 0);
    Label isHeapObject(&builder_);
    Label exit(&builder_);
    DEFVALUE(check, (&builder_), VariableType::BOOL(), builder_.True());
    check = builder_.TaggedIsHeapObject(newTarget);
    builder_.Branch(*check, &isHeapObject, &exit);
    builder_.Bind(&isHeapObject);
    {
        Label isJSFunction(&builder_);
        check = builder_.IsJSFunction(newTarget);
        builder_.Branch(*check, &isJSFunction, &exit);
        builder_.Bind(&isJSFunction);
        {
            Label getHclass(&builder_);
            GateRef glueGlobalEnvOffset = builder_.IntPtr(
                JSThread::GlueData::GetGlueGlobalEnvOffset(builder_.GetCurrentEnvironment()->Is32Bit()));
            GateRef glueGlobalEnv = builder_.Load(VariableType::NATIVE_POINTER(), glue, glueGlobalEnvOffset);
            GateRef targetFunc =
                builder_.GetGlobalEnvValue(VariableType::JS_ANY(), glueGlobalEnv, GlobalEnv::OBJECT_FUNCTION_INDEX);
            check = builder_.Equal(targetFunc, newTarget);
            builder_.Branch(*check, &getHclass, &exit);
            builder_.Bind(&getHclass);
            {
                GateRef intialHClass = builder_.Load(VariableType::JS_ANY(), newTarget,
                                                     builder_.IntPtr(JSFunction::PROTO_OR_DYNCLASS_OFFSET));
                check = builder_.IsJSHClass(intialHClass);
                builder_.Jump(&exit);
            }
        }
    }
    builder_.Bind(&exit);
    builder_.DeoptCheck(*check, frameState, DeoptType::NEWBUILTINCTORFAIL);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), Circuit::NullGate());
}

void TypeHCRLowering::LowerObjectConstructor(GateRef gate, GateRef glue)
{
    Environment env(gate, circuit_, &builder_);
    GateRef value = builder_.Undefined();
    ASSERT(acc_.GetNumValueIn(gate) <= 2); // 2: new target and arg0
    if (acc_.GetNumValueIn(gate) > 1) {
        value = acc_.GetValueIn(gate, 1);
    }
    DEFVALUE(res, (&builder_), VariableType::JS_ANY(), builder_.Undefined());
    Label slowPath(&builder_);
    Label exit(&builder_);

    Label isHeapObj(&builder_);
    Label notHeapObj(&builder_);
    builder_.Branch(builder_.TaggedIsHeapObject(value), &isHeapObj, &notHeapObj);
    builder_.Bind(&isHeapObj);
    {
        Label isEcmaObj(&builder_);
        Label notEcmaObj(&builder_);
        builder_.Branch(builder_.TaggedObjectIsEcmaObject(value), &isEcmaObj, &notEcmaObj);
        builder_.Bind(&isEcmaObj);
        {
            res = value;
            builder_.Jump(&exit);
        }
        builder_.Bind(&notEcmaObj);
        {
            Label isSymbol(&builder_);
            Label notSymbol(&builder_);
            builder_.Branch(builder_.TaggedIsSymbol(value), &isSymbol, &notSymbol);
            builder_.Bind(&isSymbol);
            {
                res = NewJSPrimitiveRef(PrimitiveType::PRIMITIVE_SYMBOL, glue, value);
                builder_.Jump(&exit);
            }
            builder_.Bind(&notSymbol);
            {
                Label isBigInt(&builder_);
                builder_.Branch(builder_.TaggedIsBigInt(value), &isBigInt, &slowPath);
                builder_.Bind(&isBigInt);
                {
                    res = NewJSPrimitiveRef(PrimitiveType::PRIMITIVE_BIGINT, glue, value);
                    builder_.Jump(&exit);
                }
            }
        }
    }
    builder_.Bind(&notHeapObj);
    {
        Label isNumber(&builder_);
        Label notNumber(&builder_);
        builder_.Branch(builder_.TaggedIsNumber(value), &isNumber, &notNumber);
        builder_.Bind(&isNumber);
        {
            res = NewJSPrimitiveRef(PrimitiveType::PRIMITIVE_NUMBER, glue, value);
            builder_.Jump(&exit);
        }
        builder_.Bind(&notNumber);
        {
            Label isBoolean(&builder_);
            builder_.Branch(builder_.TaggedIsBoolean(value), &isBoolean, &slowPath);
            builder_.Bind(&isBoolean);
            {
                res = NewJSPrimitiveRef(PrimitiveType::PRIMITIVE_BOOLEAN, glue, value);
                builder_.Jump(&exit);
            }
        }
    }
    builder_.Bind(&slowPath);
    {
        size_t range = acc_.GetNumValueIn(gate);
        std::vector<GateRef> args(range);
        for (size_t i = 0; i < range; ++i) {
            args[i] = acc_.GetValueIn(gate, i);
        }
        res = LowerCallRuntime(glue, gate, RTSTUB_ID(OptNewObjRange), args, true);
        builder_.Jump(&exit);
    }
    builder_.Bind(&exit);
    ReplaceGateWithPendingException(glue, gate, builder_.GetState(), builder_.GetDepend(), *res);
}

GateRef TypeHCRLowering::NewJSPrimitiveRef(PrimitiveType type, GateRef glue, GateRef value)
{
    GateRef glueGlobalEnvOffset = builder_.IntPtr(
        JSThread::GlueData::GetGlueGlobalEnvOffset(builder_.GetCurrentEnvironment()->Is32Bit()));
    GateRef gloablEnv = builder_.Load(VariableType::NATIVE_POINTER(), glue, glueGlobalEnvOffset);
    GateRef ctor = Circuit::NullGate();
    switch (type) {
        case PrimitiveType::PRIMITIVE_NUMBER: {
            ctor = builder_.GetGlobalEnvValue(VariableType::JS_ANY(), gloablEnv, GlobalEnv::NUMBER_FUNCTION_INDEX);
            break;
        }
        case PrimitiveType::PRIMITIVE_SYMBOL: {
            ctor = builder_.GetGlobalEnvValue(VariableType::JS_ANY(), gloablEnv, GlobalEnv::SYMBOL_FUNCTION_INDEX);
            break;
        }
        case PrimitiveType::PRIMITIVE_BOOLEAN: {
            ctor = builder_.GetGlobalEnvValue(VariableType::JS_ANY(), gloablEnv, GlobalEnv::BOOLEAN_FUNCTION_INDEX);
            break;
        }
        case PrimitiveType::PRIMITIVE_BIGINT: {
            ctor = builder_.GetGlobalEnvValue(VariableType::JS_ANY(), gloablEnv, GlobalEnv::BIGINT_FUNCTION_INDEX);
            break;
        }
        default: {
            LOG_ECMA(FATAL) << "this branch is unreachable " << static_cast<int>(type);
            UNREACHABLE();
        }
    }
    GateRef hclass =
        builder_.Load(VariableType::JS_ANY(), ctor, builder_.IntPtr(JSFunction::PROTO_OR_DYNCLASS_OFFSET));
    NewObjectStubBuilder newBuilder(builder_.GetCurrentEnvironment());
    GateRef res = newBuilder.NewJSObject(glue, hclass);
    GateRef valueOffset = builder_.IntPtr(JSPrimitiveRef::VALUE_OFFSET);
    builder_.Store(VariableType::JS_ANY(), glue, res, valueOffset, value);
    return res;
}

void TypeHCRLowering::ReplaceGateWithPendingException(GateRef glue, GateRef gate, GateRef state, GateRef depend,
                                                      GateRef value)
{
    auto condition = builder_.HasPendingException(glue);
    GateRef ifBranch = builder_.Branch(state, condition);
    GateRef ifTrue = builder_.IfTrue(ifBranch);
    GateRef ifFalse = builder_.IfFalse(ifBranch);
    GateRef eDepend = builder_.DependRelay(ifTrue, depend);
    GateRef sDepend = builder_.DependRelay(ifFalse, depend);

    StateDepend success(ifFalse, sDepend);
    StateDepend exception(ifTrue, eDepend);
    acc_.ReplaceHirWithIfBranch(gate, success, exception, value);
}

void TypeHCRLowering::LowerLoadBuiltinObject(GateRef gate)
{
    Environment env(gate, circuit_, &builder_);
    GateRef glue = acc_.GetGlueFromArgList();
    auto builtinEntriesOffset = JSThread::GlueData::GetBuiltinEntriesOffset(false);
    auto boxOffset = builtinEntriesOffset + acc_.GetIndex(gate);
    GateRef box = builder_.LoadConstOffset(VariableType::JS_POINTER(), glue, boxOffset);
    GateRef builtin = builder_.LoadConstOffset(VariableType::JS_POINTER(), box, PropertyBox::VALUE_OFFSET);
    builder_.CallRuntime(glue, RTSTUB_ID(AotDebug), acc_.GetDep(gate), { builtin }, gate);
    auto frameState = GetFrameState(gate);
    auto isHole = builder_.TaggedIsHole(builtin);
    GateRef traceGate = builder_.CallRuntime(glue, RTSTUB_ID(AotDebug), acc_.GetDep(gate),
                                                 { builder_.Int64(boxOffset), box, builtin }, gate);
    builder_.SetDepend(traceGate);
    // attributes on globalThis may change, it will cause renew a PropertyBox, the old box will be abandoned
    // so we need deopt
    builder_.DeoptCheck(isHole, frameState, DeoptType::LOADBUILTINOBJECTFAIL);
    acc_.ReplaceGate(gate, builder_.GetState(), builder_.GetDepend(), builtin);
}
}  // namespace panda::ecmascript::kungfu
