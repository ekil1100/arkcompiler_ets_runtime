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

#include "lower.h"
#include <string>
#include <cinttypes>
#include <vector>
#include "mir_symbol.h"
#include "mir_function.h"
#include "cg_option.h"
#include "switch_lowerer.h"
#include "try_catch.h"
#include "intrinsic_op.h"
#include "mir_builder.h"
#include "opcode_info.h"
#include "rt.h"
#include "securec.h"
#include "string_utils.h"
#include "cast_opt.h"
#include "simplify.h"
#include "me_safety_warning.h"

namespace maplebe {
namespace arrayNameForLower {
const std::set<std::string> kArrayKlassName {
#include "array_klass_name.def"
};

const std::set<std::string> kArrayBaseName {
#include "array_base_name.def"
};
}  // namespace arrayNameForLower

using namespace maple;

#define JAVALANG (mirModule.IsJavaModule())
#define TARGARM32 0

enum ExtFuncT : uint8 { kFmodDouble, kFmodFloat };

struct ExtFuncDescrT {
    ExtFuncT fid;
    const char *name;
    PrimType retType;
    PrimType argTypes[kMaxModFuncArgSize];
};

namespace {
std::pair<MIRIntrinsicID, const std::string> cgBuiltins[] = {
    {INTRN_JAVA_ARRAY_LENGTH, "MCC_DexArrayLength"},
    {INTRN_JAVA_ARRAY_FILL, "MCC_DexArrayFill"},
    {INTRN_JAVA_CHECK_CAST, "MCC_DexCheckCast"},
    {INTRN_JAVA_INSTANCE_OF, "MCC_DexInstanceOf"},
    {INTRN_JAVA_INTERFACE_CALL, "MCC_DexInterfaceCall"},
    {INTRN_JAVA_POLYMORPHIC_CALL, "MCC_DexPolymorphicCall"},
    {INTRN_MCC_DeferredFillNewArray, "MCC_DeferredFillNewArray"},
    {INTRN_MCC_DeferredInvoke, "MCC_DeferredInvoke"},
    {INTRN_JAVA_CONST_CLASS, "MCC_GetReferenceToClass"},
    {INTRN_JAVA_GET_CLASS, "MCC_GetClass"},
    {INTRN_MPL_SET_CLASS, "MCC_SetJavaClass"},
    {INTRN_MPL_MEMSET_LOCALVAR, "memset_s"},
};

ExtFuncDescrT extFnDescrs[] = {
    {kFmodDouble, "fmod", PTY_f64, {PTY_f64, PTY_f64, kPtyInvalid}},
    {kFmodFloat, "fmodf", PTY_f32, {PTY_f32, PTY_f32, kPtyInvalid}},
};

std::vector<std::pair<ExtFuncT, PUIdx>> extFuncs;
const std::string kOpAssertge = "OP_assertge";
const std::string kOpAssertlt = "OP_assertlt";
const std::string kOpCallAssertle = "OP_callassertle";
const std::string kOpReturnAssertle = "OP_returnassertle";
const std::string kOpAssignAssertle = "OP_assignassertle";
const std::string kFileSymbolNamePrefix = "symname";
}  // namespace

const std::string CGLowerer::kIntrnRetValPrefix = "__iret";
const std::string CGLowerer::kUserRetValPrefix = "__uret";

std::string CGLowerer::GetFileNameSymbolName(const std::string &fileName) const
{
    return kFileSymbolNamePrefix + std::regex_replace(fileName, std::regex("-"), "_");
}

MIRSymbol *CGLowerer::CreateNewRetVar(const MIRType &ty, const std::string &prefix)
{
    const uint32 bufSize = 257;
    char buf[bufSize] = {'\0'};
    MIRFunction *func = GetCurrentFunc();
    MIRSymbol *var = func->GetSymTab()->CreateSymbol(kScopeLocal);
    int eNum = sprintf_s(buf, bufSize - 1, "%s%" PRId64, prefix.c_str(), ++seed);
    if (eNum == -1) {
        FATAL(kLncFatal, "sprintf_s failed");
    }
    std::string strBuf(buf);
    var->SetNameStrIdx(mirModule.GetMIRBuilder()->GetOrCreateStringIndex(strBuf));
    var->SetTyIdx(ty.GetTypeIndex());
    var->SetStorageClass(kScAuto);
    var->SetSKind(kStVar);
    func->GetSymTab()->AddToStringSymbolMap(*var);
    return var;
}

void CGLowerer::RegisterExternalLibraryFunctions()
{
    for (uint32 i = 0; i < sizeof(extFnDescrs) / sizeof(extFnDescrs[0]); ++i) {
        ExtFuncT id = extFnDescrs[i].fid;
        CHECK_FATAL(id == i, "make sure id equal i");

        MIRFunction *func =
            mirModule.GetMIRBuilder()->GetOrCreateFunction(extFnDescrs[i].name, TyIdx(extFnDescrs[i].retType));
        beCommon.UpdateTypeTable(*func->GetMIRFuncType());
        func->AllocSymTab();
        MIRSymbol *funcSym = func->GetFuncSymbol();
        funcSym->SetStorageClass(kScExtern);
        funcSym->SetAppearsInCode(true);
        /* return type */
        MIRType *retTy = GlobalTables::GetTypeTable().GetPrimType(extFnDescrs[i].retType);

        /* use void* for PTY_dynany */
        if (retTy->GetPrimType() == PTY_dynany) {
            retTy = GlobalTables::GetTypeTable().GetPtr();
        }

        std::vector<MIRSymbol *> formals;
        for (uint32 j = 0; extFnDescrs[i].argTypes[j] != kPtyInvalid; ++j) {
            PrimType primTy = extFnDescrs[i].argTypes[j];
            MIRType *argTy = GlobalTables::GetTypeTable().GetPrimType(primTy);
            /* use void* for PTY_dynany */
            if (argTy->GetPrimType() == PTY_dynany) {
                argTy = GlobalTables::GetTypeTable().GetPtr();
            }
            MIRSymbol *argSt = func->GetSymTab()->CreateSymbol(kScopeLocal);
            const uint32 bufSize = 18;
            char buf[bufSize] = {'\0'};
            int eNum = sprintf_s(buf, bufSize - 1, "p%u", j);
            if (eNum == -1) {
                FATAL(kLncFatal, "sprintf_s failed");
            }
            std::string strBuf(buf);
            argSt->SetNameStrIdx(mirModule.GetMIRBuilder()->GetOrCreateStringIndex(strBuf));
            argSt->SetTyIdx(argTy->GetTypeIndex());
            argSt->SetStorageClass(kScFormal);
            argSt->SetSKind(kStVar);
            func->GetSymTab()->AddToStringSymbolMap(*argSt);
            formals.emplace_back(argSt);
        }
        func->UpdateFuncTypeAndFormalsAndReturnType(formals, retTy->GetTypeIndex(), false);
        auto *funcType = func->GetMIRFuncType();
        DEBUG_ASSERT(funcType != nullptr, "null ptr check");
        beCommon.AddTypeSizeAndAlign(funcType->GetTypeIndex(), GetPrimTypeSize(funcType->GetPrimType()));
        extFuncs.emplace_back(std::pair<ExtFuncT, PUIdx>(id, func->GetPuidx()));
    }
}

BaseNode *CGLowerer::NodeConvert(PrimType mType, BaseNode &expr)
{
    PrimType srcType = expr.GetPrimType();
    if (GetPrimTypeSize(mType) == GetPrimTypeSize(srcType)) {
        return &expr;
    }
    TypeCvtNode *cvtNode = mirModule.CurFuncCodeMemPool()->New<TypeCvtNode>(OP_cvt);
    cvtNode->SetFromType(srcType);
    cvtNode->SetPrimType(mType);
    cvtNode->SetOpnd(&expr, 0);
    return cvtNode;
}

BaseNode *CGLowerer::LowerIaddrof(const IreadNode &iaddrof)
{
    if (iaddrof.GetFieldID() == 0) {
        return iaddrof.Opnd(0);
    }
    MIRType *type = GlobalTables::GetTypeTable().GetTypeFromTyIdx(iaddrof.GetTyIdx());
    MIRPtrType *pointerTy = static_cast<MIRPtrType *>(type);
    CHECK_FATAL(pointerTy != nullptr, "LowerIaddrof: expect a pointer type at iaddrof node");
    MIRStructType *structTy =
        static_cast<MIRStructType *>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(pointerTy->GetPointedTyIdx()));
    CHECK_FATAL(structTy != nullptr, "LowerIaddrof: non-zero fieldID for non-structure");
    int32 offset = beCommon.GetFieldOffset(*structTy, iaddrof.GetFieldID()).first;
    if (offset == 0) {
        return iaddrof.Opnd(0);
    }
    uint32 loweredPtrType = static_cast<uint32>(GetLoweredPtrType());
    MIRIntConst *offsetConst = GlobalTables::GetIntConstTable().GetOrCreateIntConst(
        offset, *GlobalTables::GetTypeTable().GetTypeTable().at(loweredPtrType));
    BaseNode *offsetNode = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(offsetConst);
    offsetNode->SetPrimType(GetLoweredPtrType());

    BinaryNode *addNode = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_add);
    addNode->SetPrimType(GetLoweredPtrType());
    addNode->SetBOpnd(iaddrof.Opnd(0), 0);
    addNode->SetBOpnd(offsetNode, 1);
    return addNode;
}

BaseNode *CGLowerer::SplitBinaryNodeOpnd1(BinaryNode &bNode, BlockNode &blkNode)
{
    if (Globals::GetInstance()->GetOptimLevel() >= CGOptions::kLevel1) {
        return &bNode;
    }
    MIRBuilder *mirbuilder = mirModule.GetMIRBuilder();
    static uint32 val = 0;
    std::string name("bnaryTmp");
    name += std::to_string(val++);

    BaseNode *opnd1 = bNode.Opnd(1);
    MIRType *ty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(static_cast<TyIdx>(opnd1->GetPrimType()));
    MIRSymbol *dnodeSt = mirbuilder->GetOrCreateLocalDecl(const_cast<const std::string &>(name), *ty);
    DassignNode *dnode = mirbuilder->CreateStmtDassign(const_cast<MIRSymbol &>(*dnodeSt), 0, opnd1);
    blkNode.InsertAfter(blkNode.GetLast(), dnode);

    BaseNode *dreadNode = mirbuilder->CreateExprDread(*dnodeSt);
    bNode.SetOpnd(dreadNode, 1);

    return &bNode;
}

BaseNode *CGLowerer::SplitTernaryNodeResult(TernaryNode &tNode, BaseNode &parent, BlockNode &blkNode)
{
    if (Globals::GetInstance()->GetOptimLevel() >= CGOptions::kLevel1) {
        return &tNode;
    }
    MIRBuilder *mirbuilder = mirModule.GetMIRBuilder();
    static uint32 val = 0;
    std::string name("tnaryTmp");
    name += std::to_string(val++);

    MIRType *ty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(static_cast<TyIdx>(tNode.GetPrimType()));
    MIRSymbol *dassignNodeSym = mirbuilder->GetOrCreateLocalDecl(const_cast<const std::string &>(name), *ty);
    DassignNode *dassignNode = mirbuilder->CreateStmtDassign(const_cast<MIRSymbol &>(*dassignNodeSym), 0, &tNode);
    blkNode.InsertAfter(blkNode.GetLast(), dassignNode);

    BaseNode *dreadNode = mirbuilder->CreateExprDread(*dassignNodeSym);
    for (size_t i = 0; i < parent.NumOpnds(); i++) {
        if (parent.Opnd(i) == &tNode) {
            parent.SetOpnd(dreadNode, i);
            break;
        }
    }

    return dreadNode;
}

/* Check if the operand of the select node is complex enough for either
 * functionality or performance reason so we need to lower it to if-then-else.
 */
bool CGLowerer::IsComplexSelect(const TernaryNode &tNode) const
{
    if (tNode.GetPrimType() == PTY_agg) {
        return true;
    }
    /* Iread may have side effect which may cause correctness issue. */
    if (HasIreadExpr(tNode.Opnd(kFirstReg)) || HasIreadExpr(tNode.Opnd(kSecondReg))) {
        return true;
    }
    // it will be generated many insn for complex expr, leading to
    // worse performance than punishment of branch prediction error
    constexpr size_t maxDepth = 3;
    if (MaxDepth(tNode.Opnd(kFirstReg)) > maxDepth || MaxDepth(tNode.Opnd(kSecondReg)) > maxDepth) {
        return true;
    }
    return false;
}

int32 CGLowerer::FindTheCurrentStmtFreq(const StmtNode *stmt) const
{
    while (stmt != nullptr) {
        int32 freq = mirModule.CurFunction()->GetFreqFromLastStmt(stmt->GetStmtID());
        if (freq != -1) {
            return freq;
        }
        stmt = stmt->GetPrev();
    }
    return -1;
}

/* Lower agg select node back to if-then-else stmt. */
/*
  0(brfalse)
  |  \
  1   2
   \  |
    \ |
     3
*/
BaseNode *CGLowerer::LowerComplexSelect(const TernaryNode &tNode, BaseNode &parent, BlockNode &blkNode)
{
    MIRBuilder *mirbuilder = mirModule.GetMIRBuilder();

    MIRType *resultTy = 0;
    MIRFunction *func = mirModule.CurFunction();
    if (tNode.GetPrimType() == PTY_agg) {
        if (tNode.Opnd(1)->op == OP_dread) {
            DreadNode *trueNode = static_cast<DreadNode *>(tNode.Opnd(1));
            resultTy = mirModule.CurFunction()->GetLocalOrGlobalSymbol(trueNode->GetStIdx())->GetType();
        } else if (tNode.Opnd(1)->op == OP_iread) {
            IreadNode *trueNode = static_cast<IreadNode *>(tNode.Opnd(1));
            MIRPtrType *ptrty =
                static_cast<MIRPtrType *>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(trueNode->GetTyIdx()));
            resultTy =
                static_cast<MIRStructType *>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(ptrty->GetPointedTyIdx()));
            if (trueNode->GetFieldID() != 0) {
                MIRStructType *structty = static_cast<MIRStructType *>(resultTy);
                resultTy =
                    GlobalTables::GetTypeTable().GetTypeFromTyIdx(structty->GetFieldTyIdx(trueNode->GetFieldID()));
            }
        } else {
            CHECK_FATAL(false, "NYI: LowerComplexSelect");
        }
    } else {
        resultTy = GlobalTables::GetTypeTable().GetTypeFromTyIdx(static_cast<TyIdx>(tNode.GetPrimType()));
    }

    CondGotoNode *brTargetStmt = mirModule.CurFuncCodeMemPool()->New<CondGotoNode>(OP_brfalse);
    brTargetStmt->SetOpnd(tNode.Opnd(0), 0);
    LabelIdx targetIdx = mirModule.CurFunction()->GetLabelTab()->CreateLabel();
    mirModule.CurFunction()->GetLabelTab()->AddToStringLabelMap(targetIdx);
    brTargetStmt->SetOffset(targetIdx);
    // Update the current stmt frequence
    int32 currentStmtFreq = 0;
    if (kOpcodeInfo.IsStmt(parent.GetOpCode())) {
        currentStmtFreq = FindTheCurrentStmtFreq(static_cast<StmtNode *>(&parent));
    }
    currentStmtFreq = currentStmtFreq == -1 ? 0 : currentStmtFreq;
    func->SetLastFreqMap(brTargetStmt->GetStmtID(), static_cast<uint32>(currentStmtFreq));
    blkNode.InsertAfter(blkNode.GetLast(), brTargetStmt);
    union {
        MIRSymbol *resSym;
        PregIdx resPreg;
    } cplxSelRes;  // complex select result
    uint32 fallthruStmtFreq = static_cast<uint32>((currentStmtFreq + 1) / 2);
    if (tNode.GetPrimType() == PTY_agg) {
        static uint32 val = 0;
        std::string name("ComplexSelectTmp");
        name += std::to_string(val++);
        cplxSelRes.resSym = mirbuilder->GetOrCreateLocalDecl(const_cast<std::string &>(name), *resultTy);
        DassignNode *dassignTrue = mirbuilder->CreateStmtDassign(*cplxSelRes.resSym, 0, tNode.Opnd(1));
        // Fallthru: update the frequence 1
        func->SetFirstFreqMap(dassignTrue->GetStmtID(), fallthruStmtFreq);
        blkNode.InsertAfter(blkNode.GetLast(), dassignTrue);
    } else {
        cplxSelRes.resPreg = mirbuilder->GetCurrentFunction()->GetPregTab()->CreatePreg(tNode.GetPrimType());
        RegassignNode *regassignTrue =
            mirbuilder->CreateStmtRegassign(tNode.GetPrimType(), cplxSelRes.resPreg, tNode.Opnd(1));
        // Update the frequence first opnd
        func->SetFirstFreqMap(regassignTrue->GetStmtID(), fallthruStmtFreq);
        blkNode.InsertAfter(blkNode.GetLast(), regassignTrue);
    }

    GotoNode *gotoStmt = mirModule.CurFuncCodeMemPool()->New<GotoNode>(OP_goto);
    LabelIdx EndIdx = mirModule.CurFunction()->GetLabelTab()->CreateLabel();
    mirModule.CurFunction()->GetLabelTab()->AddToStringLabelMap(EndIdx);
    gotoStmt->SetOffset(EndIdx);
    // Update the frequence first opnd
    func->SetLastFreqMap(gotoStmt->GetStmtID(), fallthruStmtFreq);
    blkNode.InsertAfter(blkNode.GetLast(), gotoStmt);

    uint32 targetStmtFreq = static_cast<uint32>(currentStmtFreq / 2);
    LabelNode *lableStmt = mirModule.CurFuncCodeMemPool()->New<LabelNode>();
    lableStmt->SetLabelIdx(targetIdx);
    func->SetFirstFreqMap(lableStmt->GetStmtID(), targetStmtFreq);
    blkNode.InsertAfter(blkNode.GetLast(), lableStmt);

    if (tNode.GetPrimType() == PTY_agg) {
        DassignNode *dassignFalse = mirbuilder->CreateStmtDassign(*cplxSelRes.resSym, 0, tNode.Opnd(2));
        // Update the frequence second opnd
        func->SetLastFreqMap(dassignFalse->GetStmtID(), targetStmtFreq);
        blkNode.InsertAfter(blkNode.GetLast(), dassignFalse);
    } else {
        RegassignNode *regassignFalse =
            mirbuilder->CreateStmtRegassign(tNode.GetPrimType(), cplxSelRes.resPreg, tNode.Opnd(2));
        // Update the frequence 2
        func->SetLastFreqMap(regassignFalse->GetStmtID(), targetStmtFreq);
        blkNode.InsertAfter(blkNode.GetLast(), regassignFalse);
    }

    lableStmt = mirModule.CurFuncCodeMemPool()->New<LabelNode>();
    lableStmt->SetLabelIdx(EndIdx);
    // Update the frequence third opnd
    func->SetFirstFreqMap(lableStmt->GetStmtID(), static_cast<uint32>(currentStmtFreq));
    blkNode.InsertAfter(blkNode.GetLast(), lableStmt);

    BaseNode *exprNode =
        (tNode.GetPrimType() == PTY_agg)
            ? static_cast<BaseNode *>(mirbuilder->CreateExprDread(*cplxSelRes.resSym))
            : static_cast<BaseNode *>(mirbuilder->CreateExprRegread(tNode.GetPrimType(), cplxSelRes.resPreg));
    for (size_t i = 0; i < parent.NumOpnds(); i++) {
        if (parent.Opnd(i) == &tNode) {
            parent.SetOpnd(exprNode, i);
            break;
        }
    }

    return exprNode;
}

BaseNode *CGLowerer::LowerFarray(ArrayNode &array)
{
    auto *farrayType = static_cast<MIRFarrayType *>(array.GetArrayType(GlobalTables::GetTypeTable()));
    size_t eSize = GlobalTables::GetTypeTable().GetTypeFromTyIdx(farrayType->GetElemTyIdx())->GetSize();
    if (farrayType->GetKind() == kTypeJArray) {
        if (farrayType->GetElemType()->GetKind() != kTypeScalar) {
            /* not the last dimension of primitive array */
            eSize = RTSupport::GetRTSupportInstance().GetObjectAlignment();
        }
    }

    MIRType &arrayType = *GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(array.GetPrimType()));
    /* how about multi-dimension array? */
    if (array.GetIndex(0)->GetOpCode() == OP_constval) {
        const ConstvalNode *constvalNode = static_cast<const ConstvalNode *>(array.GetIndex(0));
        if (constvalNode->GetConstVal()->GetKind() == kConstInt) {
            const MIRIntConst *pIntConst = static_cast<const MIRIntConst *>(constvalNode->GetConstVal());
            CHECK_FATAL(JAVALANG || !pIntConst->IsNegative(), "Array index should >= 0.");
            uint64 eleOffset = pIntConst->GetExtValue() * eSize;

            if (farrayType->GetKind() == kTypeJArray) {
                eleOffset += RTSupport::GetRTSupportInstance().GetArrayContentOffset();
            }

            BaseNode *baseNode = NodeConvert(array.GetPrimType(), *array.GetBase());
            if (eleOffset == 0) {
                return baseNode;
            }

            MIRIntConst *eleConst = GlobalTables::GetIntConstTable().GetOrCreateIntConst(eleOffset, arrayType);
            BaseNode *offsetNode = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(eleConst);
            offsetNode->SetPrimType(array.GetPrimType());

            BaseNode *rAdd = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_add);
            rAdd->SetPrimType(array.GetPrimType());
            rAdd->SetOpnd(baseNode, 0);
            rAdd->SetOpnd(offsetNode, 1);
            return rAdd;
        }
    }

    BaseNode *resNode = NodeConvert(array.GetPrimType(), *array.GetIndex(0));
    BaseNode *rMul = nullptr;

    if ((farrayType->GetKind() == kTypeJArray) && (resNode->GetOpCode() == OP_constval)) {
        ConstvalNode *idxNode = static_cast<ConstvalNode *>(resNode);
        uint64 idx = safe_cast<MIRIntConst>(idxNode->GetConstVal())->GetExtValue();
        MIRIntConst *eConst = GlobalTables::GetIntConstTable().GetOrCreateIntConst(idx * eSize, arrayType);
        rMul = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(eConst);
        rMul->SetPrimType(array.GetPrimType());
    } else {
        MIRIntConst *eConst =
            GlobalTables::GetIntConstTable().GetOrCreateIntConst(static_cast<int64>(eSize), arrayType);
        BaseNode *eSizeNode = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(eConst);
        eSizeNode->SetPrimType(array.GetPrimType());
        rMul = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_mul);
        rMul->SetPrimType(array.GetPrimType());
        rMul->SetOpnd(resNode, 0);
        rMul->SetOpnd(eSizeNode, 1);
    }

    BaseNode *baseNode = NodeConvert(array.GetPrimType(), *array.GetBase());

    if (farrayType->GetKind() == kTypeJArray) {
        BaseNode *jarrayBaseNode = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_add);
        MIRIntConst *arrayHeaderNode = GlobalTables::GetIntConstTable().GetOrCreateIntConst(
            RTSupport::GetRTSupportInstance().GetArrayContentOffset(), arrayType);
        BaseNode *arrayHeaderCstNode = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(arrayHeaderNode);
        arrayHeaderCstNode->SetPrimType(array.GetPrimType());
        jarrayBaseNode->SetPrimType(array.GetPrimType());
        jarrayBaseNode->SetOpnd(baseNode, 0);
        jarrayBaseNode->SetOpnd(arrayHeaderCstNode, 1);
        baseNode = jarrayBaseNode;
    }

    BaseNode *rAdd = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_add);
    rAdd->SetPrimType(array.GetPrimType());
    rAdd->SetOpnd(baseNode, 0);
    rAdd->SetOpnd(rMul, 1);
    return rAdd;
}

BaseNode *CGLowerer::LowerArrayDim(ArrayNode &array, int32 dim)
{
    BaseNode *resNode = NodeConvert(array.GetPrimType(), *array.GetIndex(dim - 1));
    /* process left dimension index, resNode express the last dim, so dim need sub 2 */
    CHECK_FATAL(dim > (std::numeric_limits<int>::min)() + 1, "out of range");
    int leftDim = dim - 2;
    MIRType *aType = array.GetArrayType(GlobalTables::GetTypeTable());
    MIRArrayType *arrayType = static_cast<MIRArrayType *>(aType);
    for (int i = leftDim; i >= 0; --i) {
        BaseNode *mpyNode = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_mul);
        BaseNode *item =
            NodeConvert(array.GetPrimType(), *array.GetDim(mirModule, GlobalTables::GetTypeTable(), dim - 1));
        if (mirModule.IsCModule()) {
            item = NodeConvert(array.GetPrimType(), *array.GetIndex(static_cast<size_t>(static_cast<unsigned int>(i))));
            int64 offsetSize = 1;
            for (int32 j = i + 1; j < dim; ++j) {
                offsetSize *= arrayType->GetSizeArrayItem(static_cast<uint32>(j));
            }
            MIRIntConst *offsetCst = mirModule.CurFuncCodeMemPool()->New<MIRIntConst>(
                offsetSize, *GlobalTables::GetTypeTable().GetTypeFromTyIdx(array.GetPrimType()));
            BaseNode *eleOffset = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(offsetCst);
            eleOffset->SetPrimType(array.GetPrimType());
            mpyNode->SetPrimType(array.GetPrimType());
            mpyNode->SetOpnd(eleOffset, 0);
            mpyNode->SetOpnd(item, 1);
        } else {
            for (int j = leftDim; j > i; --j) {
                BaseNode *mpyNodes = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_mul);
                mpyNodes->SetPrimType(array.GetPrimType());
                mpyNodes->SetOpnd(item, 0);
                mpyNodes->SetOpnd(
                    NodeConvert(array.GetPrimType(), *array.GetDim(mirModule, GlobalTables::GetTypeTable(), j)), 1);
                item = mpyNodes;
            }
            mpyNode->SetPrimType(array.GetPrimType());
            mpyNode->SetOpnd(NodeConvert(array.GetPrimType(), *array.GetIndex(i)), 0);
            mpyNode->SetOpnd(item, 1);
        }

        BaseNode *newResNode = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_add);
        newResNode->SetPrimType(array.GetPrimType());
        newResNode->SetOpnd(resNode, 0);
        newResNode->SetOpnd(mpyNode, 1);
        resNode = newResNode;
    }
    return resNode;
}

BaseNode *CGLowerer::LowerArrayForLazyBiding(BaseNode &baseNode, BaseNode &offsetNode, const BaseNode &parent)
{
    if (parent.GetOpCode() == OP_iread && (baseNode.GetOpCode() == maple::OP_addrof)) {
        const MIRSymbol *st =
            mirModule.CurFunction()->GetLocalOrGlobalSymbol(static_cast<AddrofNode &>(baseNode).GetStIdx());
        if (StringUtils::StartsWith(st->GetName(), namemangler::kDecoupleStaticValueStr) ||
            ((StringUtils::StartsWith(st->GetName(), namemangler::kMuidFuncUndefTabPrefixStr) ||
              StringUtils::StartsWith(st->GetName(), namemangler::kMuidFuncDefTabPrefixStr) ||
              StringUtils::StartsWith(st->GetName(), namemangler::kMuidDataDefTabPrefixStr) ||
              StringUtils::StartsWith(st->GetName(), namemangler::kMuidDataUndefTabPrefixStr)) &&
              CGOptions::IsLazyBinding())) {
            /* for decouple static or lazybinding def/undef tables, replace it with intrinsic */
            MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
            args.emplace_back(&baseNode);
            args.emplace_back(&offsetNode);
            return mirBuilder->CreateExprIntrinsicop(INTRN_MPL_READ_STATIC_OFFSET_TAB, OP_intrinsicop,
                                                     *GlobalTables::GetTypeTable().GetPrimType(parent.GetPrimType()),
                                                     args);
        }
    }
    return nullptr;
}

BaseNode *CGLowerer::LowerArray(ArrayNode &array, const BaseNode &parent)
{
    MIRType *aType = array.GetArrayType(GlobalTables::GetTypeTable());
    if (aType->GetKind() == kTypeFArray || aType->GetKind() == kTypeJArray) {
        return LowerFarray(array);
    }
    MIRArrayType *arrayType = static_cast<MIRArrayType *>(aType);
    int32 dim = arrayType->GetDim();
    BaseNode *resNode = LowerArrayDim(array, dim);
    BaseNode *rMul = nullptr;
    size_t eSize = beCommon.GetTypeSize(arrayType->GetElemTyIdx().GetIdx());
    Opcode opAdd = OP_add;
    MIRType &arrayTypes = *GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(array.GetPrimType()));
    if (resNode->GetOpCode() == OP_constval) {
        /* index is a constant, we can calculate the offset now */
        ConstvalNode *idxNode = static_cast<ConstvalNode *>(resNode);
        uint64 idx = safe_cast<MIRIntConst>(idxNode->GetConstVal())->GetExtValue();
        MIRIntConst *eConst = GlobalTables::GetIntConstTable().GetOrCreateIntConst(idx * eSize, arrayTypes);
        rMul = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(eConst);
        rMul->SetPrimType(array.GetPrimType());
        if (dim == 1) {
            opAdd = OP_CG_array_elem_add;
        }
    } else {
        MIRIntConst *eConst =
            GlobalTables::GetIntConstTable().GetOrCreateIntConst(static_cast<int64>(eSize), arrayTypes);
        BaseNode *tmpNode = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(eConst);
        tmpNode->SetPrimType(array.GetPrimType());
        rMul = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_mul);
        rMul->SetPrimType(array.GetPrimType());
        rMul->SetOpnd(resNode, 0);
        rMul->SetOpnd(tmpNode, 1);
    }
    BaseNode *baseNode = NodeConvert(array.GetPrimType(), *array.GetBase());
    if (rMul->GetOpCode() == OP_constval) {
        BaseNode *intrnNode = LowerArrayForLazyBiding(*baseNode, *rMul, parent);
        if (intrnNode != nullptr) {
            return intrnNode;
        }
    }
    BaseNode *rAdd = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(opAdd);
    rAdd->SetPrimType(array.GetPrimType());
    rAdd->SetOpnd(baseNode, 0);
    rAdd->SetOpnd(rMul, 1);
    return rAdd;
}

BaseNode *CGLowerer::LowerCArray(ArrayNode &array)
{
    MIRType *aType = array.GetArrayType(GlobalTables::GetTypeTable());
    if (aType->GetKind() == kTypeFArray || aType->GetKind() == kTypeJArray) {
        return LowerFarray(array);
    }

    MIRArrayType *arrayType = static_cast<MIRArrayType *>(aType);
    /* There are two cases where dimension > 1.
     * 1) arrayType->dim > 1.  Process the current arrayType. (nestedArray = false)
     * 2) arrayType->dim == 1, but arraytype->eTyIdx is another array. (nestedArray = true)
     * Assume at this time 1) and 2) cannot mix.
     * Along with the array dimension, there is the array indexing.
     * It is allowed to index arrays less than the dimension.
     * This is dictated by the number of indexes.
     */
    bool nestedArray = false;
    int dim = arrayType->GetDim();
    MIRType *innerType = nullptr;
    MIRArrayType *innerArrayType = nullptr;
    uint64 elemSize = 0;
    if (dim == 1) {
        innerType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(arrayType->GetElemTyIdx());
        if (innerType->GetKind() == kTypeArray) {
            nestedArray = true;
            do {
                innerArrayType = static_cast<MIRArrayType *>(innerType);
                elemSize = RoundUp(beCommon.GetTypeSize(innerArrayType->GetElemTyIdx().GetIdx()),
                                   beCommon.GetTypeAlign(arrayType->GetElemTyIdx().GetIdx()));
                dim++;
                innerType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(innerArrayType->GetElemTyIdx());
            } while (innerType->GetKind() == kTypeArray);
        }
    }

    int32 numIndex = static_cast<int>(array.NumOpnds()) - 1;
    MIRArrayType *curArrayType = arrayType;
    BaseNode *resNode = NodeConvert(array.GetPrimType(), *array.GetIndex(0));
    if (dim > 1) {
        BaseNode *prevNode = nullptr;
        for (int i = 0; (i < dim) && (i < numIndex); i++) {
            uint32 mpyDim = 1;
            if (nestedArray) {
                CHECK_FATAL(arrayType->GetSizeArrayItem(0) > 0, "Zero size array dimension");
                innerType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(curArrayType->GetElemTyIdx());
                curArrayType = static_cast<MIRArrayType *>(innerType);
                while (innerType->GetKind() == kTypeArray) {
                    innerArrayType = static_cast<MIRArrayType *>(innerType);
                    mpyDim *= innerArrayType->GetSizeArrayItem(0);
                    innerType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(innerArrayType->GetElemTyIdx());
                }
            } else {
                CHECK_FATAL(arrayType->GetSizeArrayItem(static_cast<uint32>(i)) > 0, "Zero size array dimension");
                for (int j = i + 1; j < dim; j++) {
                    mpyDim *= arrayType->GetSizeArrayItem(static_cast<uint32>(j));
                }
            }

            BaseNode *index = static_cast<ConstvalNode *>(array.GetIndex(static_cast<size_t>(i)));
            bool isConst = false;
            uint64 indexVal = 0;
            if (index->op == OP_constval) {
                ConstvalNode *constNode = static_cast<ConstvalNode *>(index);
                indexVal = (static_cast<MIRIntConst *>(constNode->GetConstVal()))->GetExtValue();
                isConst = true;
                MIRIntConst *newConstNode = mirModule.GetMemPool()->New<MIRIntConst>(
                    indexVal * mpyDim, *GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(array.GetPrimType())));
                BaseNode *newValNode = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(newConstNode);
                newValNode->SetPrimType(array.GetPrimType());
                if (i == 0) {
                    prevNode = newValNode;
                    continue;
                } else {
                    resNode = newValNode;
                }
            }
            if (i > 0 && !isConst) {
                resNode = NodeConvert(array.GetPrimType(), *array.GetIndex(static_cast<size_t>(i)));
            }

            BaseNode *mpyNode;
            if (isConst) {
                MIRIntConst *mulConst = mirModule.GetMemPool()->New<MIRIntConst>(
                    mpyDim * indexVal, *GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(array.GetPrimType())));
                BaseNode *mulSize = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(mulConst);
                mulSize->SetPrimType(array.GetPrimType());
                mpyNode = mulSize;
            } else if (mpyDim == 1 && prevNode) {
                mpyNode = prevNode;
                prevNode = resNode;
            } else {
                mpyNode = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_mul);
                mpyNode->SetPrimType(array.GetPrimType());
                MIRIntConst *mulConst = mirModule.GetMemPool()->New<MIRIntConst>(
                    mpyDim, *GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(array.GetPrimType())));
                BaseNode *mulSize = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(mulConst);
                mulSize->SetPrimType(array.GetPrimType());
                mpyNode->SetOpnd(NodeConvert(array.GetPrimType(), *mulSize), 0);
                mpyNode->SetOpnd(resNode, 1);
            }
            if (i == 0) {
                prevNode = mpyNode;
                continue;
            }
            BaseNode *newResNode = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_add);
            newResNode->SetPrimType(array.GetPrimType());
            newResNode->SetOpnd(mpyNode, 0);
            newResNode->SetOpnd(prevNode, 1);
            prevNode = newResNode;
        }
        resNode = prevNode;
    }

    BaseNode *rMul = nullptr;
    // esize is the size of the array element (eg. int = 4 long = 8)
    uint64 esize;
    if (nestedArray) {
        esize = elemSize;
    } else {
        esize = beCommon.GetTypeSize(arrayType->GetElemTyIdx().GetIdx());
    }
    Opcode opadd = OP_add;
    if (resNode->op == OP_constval) {
        // index is a constant, we can calculate the offset now
        ConstvalNode *idxNode = static_cast<ConstvalNode *>(resNode);
        uint64 idx = static_cast<MIRIntConst *>(idxNode->GetConstVal())->GetExtValue();
        MIRIntConst *econst = mirModule.GetMemPool()->New<MIRIntConst>(
            idx * esize, *GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(array.GetPrimType())));
        rMul = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(econst);
        rMul->SetPrimType(array.GetPrimType());
        if (dim == 1 && array.GetBase()->op == OP_addrof &&
            static_cast<AddrofNode *>(array.GetBase())->GetFieldID() == 0) {
            opadd = OP_CG_array_elem_add;
        }
    } else {
        MIRIntConst *econst = mirModule.GetMemPool()->New<MIRIntConst>(
            esize, *GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(array.GetPrimType())));
        BaseNode *eSize = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(econst);
        eSize->SetPrimType(array.GetPrimType());
        rMul = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(OP_mul);
        rMul->SetPrimType(array.GetPrimType());
        rMul->SetOpnd(resNode, 0);
        rMul->SetOpnd(eSize, 1);
    }
    BaseNode *baseNode = NodeConvert(array.GetPrimType(), *array.GetBase());
    BaseNode *rAdd = mirModule.CurFuncCodeMemPool()->New<BinaryNode>(opadd);
    rAdd->SetPrimType(array.GetPrimType());
    rAdd->SetOpnd(baseNode, 0);
    rAdd->SetOpnd(rMul, 1);
    return rAdd;
}

StmtNode *CGLowerer::WriteBitField(const std::pair<int32, int32> &byteBitOffsets, const MIRBitFieldType *fieldType,
                                   BaseNode *baseAddr, BaseNode *rhs, BlockNode *block)
{
    auto bitSize = fieldType->GetFieldSize();
    auto primType = fieldType->GetPrimType();
    auto byteOffset = byteBitOffsets.first;
    auto bitOffset = byteBitOffsets.second;
    auto *builder = mirModule.GetMIRBuilder();
    auto *bitField = builder->CreateExprIreadoff(primType, byteOffset, baseAddr);
    auto primTypeBitSize = GetPrimTypeBitSize(primType);
    if ((static_cast<uint32>(bitOffset) + bitSize) <= primTypeBitSize) {
        if (CGOptions::IsBigEndian()) {
            bitOffset =
                (static_cast<int64>(beCommon.GetTypeSize(fieldType->GetTypeIndex()) * kBitsPerByte) - bitOffset) -
                bitSize;
        }
        auto depositBits = builder->CreateExprDepositbits(OP_depositbits, primType, static_cast<uint32>(bitOffset),
                                                          bitSize, bitField, rhs);
        return builder->CreateStmtIassignoff(primType, byteOffset, baseAddr, depositBits);
    }
    // if space not enough in the unit with size of primType, we would make an extra assignment from next bound
    auto bitsRemained = (bitOffset + bitSize) - primTypeBitSize;
    auto bitsExtracted = primTypeBitSize - bitOffset;
    if (CGOptions::IsBigEndian()) {
        bitOffset = 0;
    }
    auto *depositedLowerBits = builder->CreateExprDepositbits(OP_depositbits, primType, static_cast<uint32>(bitOffset),
                                                              bitsExtracted, bitField, rhs);
    auto *assignedLowerBits = builder->CreateStmtIassignoff(primType, byteOffset, baseAddr, depositedLowerBits);
    block->AddStatement(assignedLowerBits);
    auto *extractedHigherBits =
        builder->CreateExprExtractbits(OP_extractbits, primType, bitsExtracted, bitsRemained, rhs);
    auto *bitFieldRemained =
        builder->CreateExprIreadoff(primType, byteOffset + static_cast<int32>(GetPrimTypeSize(primType)), baseAddr);
    auto *depositedHigherBits = builder->CreateExprDepositbits(OP_depositbits, primType, 0, bitsRemained,
                                                               bitFieldRemained, extractedHigherBits);
    auto *assignedHigherBits = builder->CreateStmtIassignoff(
        primType, byteOffset + static_cast<int32>(GetPrimTypeSize(primType)), baseAddr, depositedHigherBits);
    return assignedHigherBits;
}

BaseNode *CGLowerer::ReadBitField(const std::pair<int32, int32> &byteBitOffsets, const MIRBitFieldType *fieldType,
                                  BaseNode *baseAddr)
{
    auto bitSize = fieldType->GetFieldSize();
    auto primType = fieldType->GetPrimType();
    auto byteOffset = byteBitOffsets.first;
    auto bitOffset = byteBitOffsets.second;
    auto *builder = mirModule.GetMIRBuilder();
    auto *bitField = builder->CreateExprIreadoff(primType, byteOffset, baseAddr);
    auto primTypeBitSize = GetPrimTypeBitSize(primType);
    if ((static_cast<uint32>(bitOffset) + bitSize) <= primTypeBitSize) {
        if (CGOptions::IsBigEndian()) {
            bitOffset =
                (static_cast<int64>(beCommon.GetTypeSize(fieldType->GetTypeIndex()) * kBitsPerByte) - bitOffset) -
                bitSize;
        }
        return builder->CreateExprExtractbits(OP_extractbits, primType, static_cast<uint32>(bitOffset), bitSize,
                                              bitField);
    }
    // if space not enough in the unit with size of primType, the result would be binding of two exprs of load
    auto bitsRemained = (bitOffset + bitSize) - primTypeBitSize;
    if (CGOptions::IsBigEndian()) {
        bitOffset = 0;
    }
    auto *extractedLowerBits = builder->CreateExprExtractbits(OP_extractbits, primType, static_cast<uint32>(bitOffset),
                                                              bitSize - bitsRemained, bitField);
    auto *bitFieldRemained =
        builder->CreateExprIreadoff(primType, byteOffset + static_cast<int32>(GetPrimTypeSize(primType)), baseAddr);
    auto *result = builder->CreateExprDepositbits(OP_depositbits, primType, bitSize - bitsRemained, bitsRemained,
                                                  extractedLowerBits, bitFieldRemained);
    return result;
}

BaseNode *CGLowerer::LowerDreadBitfield(DreadNode &dread)
{
    auto *symbol = mirModule.CurFunction()->GetLocalOrGlobalSymbol(dread.GetStIdx());
    auto *structTy = static_cast<MIRStructType *>(symbol->GetType());
    auto fTyIdx = structTy->GetFieldTyIdx(dread.GetFieldID());
    auto *fType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(fTyIdx));
    if (fType->GetKind() != kTypeBitField) {
        return &dread;
    }
    auto *builder = mirModule.GetMIRBuilder();
    auto *baseAddr = builder->CreateExprAddrof(0, dread.GetStIdx());
    auto byteBitOffsets = beCommon.GetFieldOffset(*structTy, dread.GetFieldID());
    return ReadBitField(byteBitOffsets, static_cast<MIRBitFieldType *>(fType), baseAddr);
}

BaseNode *CGLowerer::LowerIreadBitfield(IreadNode &iread)
{
    uint32 index = iread.GetTyIdx();
    MIRPtrType *pointerTy = static_cast<MIRPtrType *>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(index));
    MIRType *pointedTy = GlobalTables::GetTypeTable().GetTypeFromTyIdx(pointerTy->GetPointedTyIdx());
    /* Here pointed type can be Struct or JArray */
    MIRStructType *structTy = nullptr;
    if (pointedTy->GetKind() != kTypeJArray) {
        structTy = static_cast<MIRStructType *>(pointedTy);
    } else {
        /* it's a Jarray type. using it's parent's field info: java.lang.Object */
        structTy = static_cast<MIRJarrayType *>(pointedTy)->GetParentType();
    }
    TyIdx fTyIdx = structTy->GetFieldTyIdx(iread.GetFieldID());
    MIRType *fType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(fTyIdx));
    if (fType->GetKind() != kTypeBitField) {
        return &iread;
    }
    auto byteBitOffsets = beCommon.GetFieldOffset(*structTy, iread.GetFieldID());
    return ReadBitField(byteBitOffsets, static_cast<MIRBitFieldType *>(fType), iread.Opnd(0));
}

// input node must be cvt, retype, zext or sext
BaseNode *CGLowerer::LowerCastExpr(BaseNode &expr)
{
    if (CGOptions::GetInstance().GetOptimizeLevel() >= CGOptions::kLevel2) {
        BaseNode *simplified = MapleCastOpt::SimplifyCast(*mirBuilder, &expr);
        return simplified != nullptr ? simplified : &expr;
    }
    return &expr;
}

#if TARGARM32 || TARGAARCH64 || TARGRISCV64 || TARGX86_64
BlockNode *CGLowerer::LowerReturnStructUsingFakeParm(NaryStmtNode &retNode)
{
    BlockNode *blk = mirModule.CurFuncCodeMemPool()->New<BlockNode>();
    for (size_t i = 0; i < retNode.GetNopndSize(); ++i) {
        retNode.SetOpnd(LowerExpr(retNode, *retNode.GetNopndAt(i), *blk), i);
    }
    BaseNode *opnd0 = retNode.Opnd(0);
    if (!(opnd0 && opnd0->GetPrimType() == PTY_agg)) {
        /* It is possible function never returns and have a dummy return const instead of a struct. */
        maple::LogInfo::MapleLogger(kLlWarn) << "return struct should have a kid" << std::endl;
    }

    MIRFunction *curFunc = GetCurrentFunc();
    MIRSymbol *retSt = curFunc->GetFormal(0);
    MIRPtrType *retTy = static_cast<MIRPtrType *>(retSt->GetType());
    IassignNode *iassign = mirModule.CurFuncCodeMemPool()->New<IassignNode>();
    iassign->SetTyIdx(retTy->GetTypeIndex());
    DEBUG_ASSERT(opnd0 != nullptr, "opnd0 should not be nullptr");
    if ((beCommon.GetTypeSize(retTy->GetPointedTyIdx().GetIdx()) <= k16ByteSize) && (opnd0->GetPrimType() == PTY_agg)) {
        /* struct goes into register. */
        curFunc->SetStructReturnedInRegs();
    }
    iassign->SetFieldID(0);
    iassign->SetRHS(opnd0);
    if (retSt->IsPreg()) {
        RegreadNode *regNode = mirModule.GetMIRBuilder()->CreateExprRegread(
            GetLoweredPtrType(), curFunc->GetPregTab()->GetPregIdxFromPregno(retSt->GetPreg()->GetPregNo()));
        iassign->SetOpnd(regNode, 0);
    } else {
        AddrofNode *dreadNode = mirModule.CurFuncCodeMemPool()->New<AddrofNode>(OP_dread);
        dreadNode->SetPrimType(GetLoweredPtrType());
        dreadNode->SetStIdx(retSt->GetStIdx());
        iassign->SetOpnd(dreadNode, 0);
    }
    blk->AddStatement(iassign);
    retNode.GetNopnd().clear();
    retNode.SetNumOpnds(0);
    blk->AddStatement(&retNode);
    return blk;
}

#endif /* TARGARM32 || TARGAARCH64 || TARGX86_64 */

BlockNode *CGLowerer::LowerReturn(NaryStmtNode &retNode)
{
    BlockNode *blk = mirModule.CurFuncCodeMemPool()->New<BlockNode>();
    if (retNode.NumOpnds() != 0) {
        BaseNode *expr = retNode.Opnd(0);
        Opcode opr = expr->GetOpCode();
        if (opr == OP_dread) {
            AddrofNode *retExpr = static_cast<AddrofNode *>(expr);
            MIRFunction *mirFunc = mirModule.CurFunction();
            MIRSymbol *sym = mirFunc->GetLocalOrGlobalSymbol(retExpr->GetStIdx());
            if (sym->GetAttr(ATTR_localrefvar)) {
                mirFunc->InsertMIRSymbol(sym);
            }
        }
    }
    for (size_t i = 0; i < retNode.GetNopndSize(); ++i) {
        retNode.SetOpnd(LowerExpr(retNode, *retNode.GetNopndAt(i), *blk), i);
    }
    blk->AddStatement(&retNode);
    return blk;
}

StmtNode *CGLowerer::LowerDassignBitfield(DassignNode &dassign, BlockNode &newBlk)
{
    dassign.SetRHS(LowerExpr(dassign, *dassign.GetRHS(), newBlk));
    MIRSymbol *symbol = mirModule.CurFunction()->GetLocalOrGlobalSymbol(dassign.GetStIdx());
    MIRStructType *structTy = static_cast<MIRStructType *>(symbol->GetType());
    CHECK_FATAL(structTy != nullptr, "LowerDassignBitfield: non-zero fieldID for non-structure");
    TyIdx fTyIdx = structTy->GetFieldTyIdx(dassign.GetFieldID());
    CHECK_FATAL(fTyIdx != 0u, "LowerDassignBitField: field id out of range for the structure");
    MIRType *fType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(fTyIdx));
    if (fType->GetKind() != kTypeBitField) {
        return &dassign;
    }
    auto *builder = mirModule.GetMIRBuilder();
    auto *baseAddr = builder->CreateExprAddrof(0, dassign.GetStIdx());
    auto byteBitOffsets = beCommon.GetFieldOffset(*structTy, dassign.GetFieldID());
    return WriteBitField(byteBitOffsets, static_cast<MIRBitFieldType *>(fType), baseAddr, dassign.GetRHS(), &newBlk);
}

StmtNode *CGLowerer::LowerIassignBitfield(IassignNode &iassign, BlockNode &newBlk)
{
    DEBUG_ASSERT(iassign.Opnd(0) != nullptr, "iassign.Opnd(0) should not be nullptr");
    iassign.SetOpnd(LowerExpr(iassign, *iassign.Opnd(0), newBlk), 0);
    iassign.SetRHS(LowerExpr(iassign, *iassign.GetRHS(), newBlk));

    CHECK_FATAL(iassign.GetTyIdx() < GlobalTables::GetTypeTable().GetTypeTable().size(),
                "LowerIassignBitField: subscript out of range");
    uint32 index = iassign.GetTyIdx();
    MIRPtrType *pointerTy = static_cast<MIRPtrType *>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(index));
    CHECK_FATAL(pointerTy != nullptr, "LowerIassignBitField: type in iassign should be pointer type");
    MIRType *pointedTy = GlobalTables::GetTypeTable().GetTypeFromTyIdx(pointerTy->GetPointedTyIdx());
    /*
     * Here pointed type can be Struct or JArray
     * We should seriously consider make JArray also a Struct type
     */
    MIRStructType *structTy = nullptr;
    if (pointedTy->GetKind() != kTypeJArray) {
        structTy = static_cast<MIRStructType *>(pointedTy);
    } else {
        /* it's a Jarray type. using it's parent's field info: java.lang.Object */
        structTy = static_cast<MIRJarrayType *>(pointedTy)->GetParentType();
    }

    TyIdx fTyIdx = structTy->GetFieldTyIdx(iassign.GetFieldID());
    MIRType *fType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(fTyIdx));
    if (fType->GetKind() != kTypeBitField) {
        return &iassign;
    }
    auto byteBitOffsets = beCommon.GetFieldOffset(*structTy, iassign.GetFieldID());
    auto *bitFieldType = static_cast<MIRBitFieldType *>(fType);
    return WriteBitField(byteBitOffsets, bitFieldType, iassign.Opnd(0), iassign.GetRHS(), &newBlk);
}

void CGLowerer::LowerIassign(IassignNode &iassign, BlockNode &newBlk)
{
    StmtNode *newStmt = nullptr;
    if (iassign.GetFieldID() != 0) {
        newStmt = LowerIassignBitfield(iassign, newBlk);
    } else {
        LowerStmt(iassign, newBlk);
        newStmt = &iassign;
    }
    newBlk.AddStatement(newStmt);
}

static GStrIdx NewAsmTempStrIdx()
{
    static uint32 strIdxCount = 0;  // to create unique temporary symbol names
    std::string asmTempStr("asm_tempvar");
    asmTempStr += std::to_string(++strIdxCount);
    return GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(asmTempStr);
}

void CGLowerer::LowerAsmStmt(AsmNode *asmNode, BlockNode *newBlk)
{
    for (size_t i = 0; i < asmNode->NumOpnds(); i++) {
        BaseNode *opnd = LowerExpr(*asmNode, *asmNode->Opnd(i), *newBlk);
        if (opnd->NumOpnds() == 0) {
            asmNode->SetOpnd(opnd, i);
            continue;
        }
        // introduce a temporary to store the expression tree operand
        TyIdx tyIdxUsed = static_cast<TyIdx>(opnd->GetPrimType());
        if (opnd->op == OP_iread) {
            IreadNode *ireadNode = static_cast<IreadNode *>(opnd);
            tyIdxUsed = ireadNode->GetType()->GetTypeIndex();
        }
        StmtNode *assignNode = nullptr;
        BaseNode *readOpnd = nullptr;
        PrimType type = GlobalTables::GetTypeTable().GetTypeFromTyIdx(tyIdxUsed)->GetPrimType();
        if ((type != PTY_agg) && CGOptions::GetInstance().GetOptimizeLevel() >= CGOptions::kLevel2) {
            PregIdx pregIdx = mirModule.CurFunction()->GetPregTab()->CreatePreg(type);
            assignNode = mirBuilder->CreateStmtRegassign(type, pregIdx, opnd);
            readOpnd = mirBuilder->CreateExprRegread(type, pregIdx);
        } else {
            MIRSymbol *st = mirModule.GetMIRBuilder()->CreateSymbol(tyIdxUsed, NewAsmTempStrIdx(), kStVar, kScAuto,
                                                                    mirModule.CurFunction(), kScopeLocal);
            assignNode = mirModule.GetMIRBuilder()->CreateStmtDassign(*st, 0, opnd);
            readOpnd = mirBuilder->CreateExprDread(*st);
        }
        newBlk->AddStatement(assignNode);
        asmNode->SetOpnd(readOpnd, i);
    }
    newBlk->AddStatement(asmNode);
}

DassignNode *CGLowerer::SaveReturnValueInLocal(StIdx stIdx, uint16 fieldID)
{
    MIRSymbol *var;
    if (stIdx.IsGlobal()) {
        var = GlobalTables::GetGsymTable().GetSymbolFromStidx(stIdx.Idx());
    } else {
        var = GetCurrentFunc()->GetSymbolTabItem(stIdx.Idx());
    }
    CHECK_FATAL(var != nullptr, "var should not be nullptr");
    PrimType pType;
    if (var->GetAttr(ATTR_oneelem_simd)) {
        pType = PTY_f64;
    } else {
        pType = GlobalTables::GetTypeTable().GetTypeTable().at(var->GetTyIdx())->GetPrimType();
    }
    RegreadNode *regRead = mirModule.GetMIRBuilder()->CreateExprRegread(pType, -kSregRetval0);
    return mirModule.GetMIRBuilder()->CreateStmtDassign(*var, fieldID, regRead);
}

BaseNode *CGLowerer::LowerRem(BaseNode &expr, BlockNode &blk)
{
    auto &remExpr = static_cast<BinaryNode &>(expr);
    if (!IsPrimitiveFloat(remExpr.GetPrimType())) {
        return &expr;
    }
    ExtFuncT fmodFunc = remExpr.GetPrimType() == PTY_f32 ? kFmodFloat : kFmodDouble;
    uint32 i = 0;
    for (; i < extFuncs.size(); ++i) {
        if (extFuncs[i].first == fmodFunc) {
            break;
        }
    }
    CHECK_FATAL(i < extFuncs.size(), "rem expression primtype is not PTY_f32 nor PTY_f64.");
    MIRSymbol *ret =
        CreateNewRetVar(*GlobalTables::GetTypeTable().GetPrimType(remExpr.GetPrimType()), kIntrnRetValPrefix);
    MapleVector<BaseNode *> args(mirModule.GetMIRBuilder()->GetCurrentFuncCodeMpAllocator()->Adapter());
    args.emplace_back(remExpr.Opnd(0));
    args.emplace_back(remExpr.Opnd(1));
    CallNode *callStmt = mirModule.GetMIRBuilder()->CreateStmtCallAssigned(extFuncs[i].second, args, ret);
    blk.AppendStatementsFromBlock(*LowerCallAssignedStmt(*callStmt));
    MIRType *type = GlobalTables::GetTypeTable().GetPrimType(extFnDescrs[fmodFunc].retType);
    return mirModule.GetMIRBuilder()->CreateExprDread(*type, 0, *ret);
}

/* to lower call (including icall) and intrinsicall statements */
void CGLowerer::LowerCallStmt(StmtNode &stmt, StmtNode *&nextStmt, BlockNode &newBlk, MIRType *retty, bool uselvar,
                              bool isIntrinAssign)
{
    StmtNode *newStmt = nullptr;
    if (stmt.GetOpCode() == OP_intrinsiccall) {
        auto &intrnNode = static_cast<IntrinsiccallNode &>(stmt);
        newStmt = LowerIntrinsiccall(intrnNode, newBlk);
    } else {
        /* We note the function has a user-defined (i.e., not an intrinsic) call. */
        GetCurrentFunc()->SetHasCall();
        newStmt = &stmt;
    }

    if (newStmt == nullptr) {
        return;
    }

    if (newStmt->GetOpCode() == OP_call || newStmt->GetOpCode() == OP_icall || newStmt->GetOpCode() == OP_icallproto) {
        newStmt = LowerCall(static_cast<CallNode &>(*newStmt), nextStmt, newBlk, retty, uselvar);
    }
    newStmt->SetSrcPos(stmt.GetSrcPos());
    newBlk.AddStatement(newStmt);
    if (CGOptions::GetInstance().GetOptimizeLevel() >= CGOptions::kLevel2 && stmt.GetOpCode() == OP_intrinsiccall) {
        /* Try to expand memset and memcpy call lowered from intrinsiccall */
        /* Skip expansion if call returns a value that is used later. */
        BlockNode *blkLowered = isIntrinAssign ? nullptr : LowerMemop(*newStmt);
        if (blkLowered != nullptr) {
            newBlk.RemoveStmt(newStmt);
            newBlk.AppendStatementsFromBlock(*blkLowered);
        }
    }
}

StmtNode *CGLowerer::GenCallNode(const StmtNode &stmt, PUIdx &funcCalled, CallNode &origCall)
{
    CallNode *newCall = nullptr;
    if (stmt.GetOpCode() == OP_callassigned) {
        newCall = mirModule.GetMIRBuilder()->CreateStmtCall(origCall.GetPUIdx(), origCall.GetNopnd());
    } else if (stmt.GetOpCode() == OP_virtualcallassigned) {
        newCall = mirModule.GetMIRBuilder()->CreateStmtVirtualCall(origCall.GetPUIdx(), origCall.GetNopnd());
    } else if (stmt.GetOpCode() == OP_superclasscallassigned) {
        newCall = mirModule.GetMIRBuilder()->CreateStmtSuperclassCall(origCall.GetPUIdx(), origCall.GetNopnd());
    } else if (stmt.GetOpCode() == OP_interfacecallassigned) {
        newCall = mirModule.GetMIRBuilder()->CreateStmtInterfaceCall(origCall.GetPUIdx(), origCall.GetNopnd());
    }
    newCall->SetDeoptBundleInfo(origCall.GetDeoptBundleInfo());
    newCall->SetSrcPos(stmt.GetSrcPos());
    CHECK_FATAL(newCall != nullptr, "nullptr is not expected");
    funcCalled = origCall.GetPUIdx();
    CHECK_FATAL((newCall->GetOpCode() == OP_call || newCall->GetOpCode() == OP_interfacecall),
                "virtual call or super class call are not expected");
    if (newCall->GetOpCode() == OP_interfacecall) {
        std::cerr << "interfacecall found\n";
    }
    newCall->SetStmtAttrs(stmt.GetStmtAttrs());
    return newCall;
}

StmtNode *CGLowerer::GenIntrinsiccallNode(const StmtNode &stmt, PUIdx &funcCalled, bool &handledAtLowerLevel,
                                          IntrinsiccallNode &origCall)
{
    StmtNode *newCall = nullptr;
    handledAtLowerLevel = IsIntrinsicCallHandledAtLowerLevel(origCall.GetIntrinsic());
    if (handledAtLowerLevel) {
        /* If the lower level can handle the intrinsic, just let it pass through. */
        newCall = &origCall;
    } else {
        PUIdx bFunc = GetBuiltinToUse(origCall.GetIntrinsic());
        if (bFunc != kFuncNotFound) {
            newCall = mirModule.GetMIRBuilder()->CreateStmtCall(bFunc, origCall.GetNopnd());
        } else {
            if (stmt.GetOpCode() == OP_intrinsiccallassigned) {
                newCall =
                    mirModule.GetMIRBuilder()->CreateStmtIntrinsicCall(origCall.GetIntrinsic(), origCall.GetNopnd());
            } else if (stmt.GetOpCode() == OP_xintrinsiccallassigned) {
                newCall =
                    mirModule.GetMIRBuilder()->CreateStmtXintrinsicCall(origCall.GetIntrinsic(), origCall.GetNopnd());
            } else {
                newCall = mirModule.GetMIRBuilder()->CreateStmtIntrinsicCall(origCall.GetIntrinsic(),
                                                                             origCall.GetNopnd(), origCall.GetTyIdx());
            }
        }
        newCall->SetSrcPos(stmt.GetSrcPos());
        funcCalled = bFunc;
        CHECK_FATAL((newCall->GetOpCode() == OP_call || newCall->GetOpCode() == OP_intrinsiccall),
                    "xintrinsic and intrinsiccallwithtype call is not expected");
    }
    return newCall;
}

StmtNode *CGLowerer::GenIcallNode(PUIdx &funcCalled, IcallNode &origCall)
{
    IcallNode *newCall = nullptr;
    if (origCall.GetOpCode() == OP_icallassigned) {
        newCall = mirModule.GetMIRBuilder()->CreateStmtIcall(origCall.GetNopnd());
    } else {
        newCall = mirModule.GetMIRBuilder()->CreateStmtIcallproto(origCall.GetNopnd());
        newCall->SetRetTyIdx(static_cast<IcallNode &>(origCall).GetRetTyIdx());
    }
    newCall->SetDeoptBundleInfo(origCall.GetDeoptBundleInfo());
    newCall->SetStmtAttrs(origCall.GetStmtAttrs());
    newCall->SetSrcPos(origCall.GetSrcPos());
    CHECK_FATAL(newCall != nullptr, "nullptr is not expected");
    funcCalled = kFuncNotFound;
    return newCall;
}

BlockNode *CGLowerer::GenBlockNode(StmtNode &newCall, const CallReturnVector &p2nRets, const Opcode &opcode,
                                   const PUIdx &funcCalled, bool handledAtLowerLevel, bool uselvar)
{
    BlockNode *blk = mirModule.CurFuncCodeMemPool()->New<BlockNode>();
    blk->AddStatement(&newCall);
    if (!handledAtLowerLevel) {
        CHECK_FATAL(p2nRets.size() <= 1, "make sure p2nRets size <= 1");
        /* Create DassignStmt to save kSregRetval0. */
        StmtNode *dStmt = nullptr;
        MIRType *retType = nullptr;
        if (p2nRets.size() == 1) {
            MIRSymbol *sym = nullptr;
            StIdx stIdx = p2nRets[0].first;
            if (stIdx.IsGlobal()) {
                sym = GlobalTables::GetGsymTable().GetSymbolFromStidx(stIdx.Idx());
            } else {
                sym = GetCurrentFunc()->GetSymbolTabItem(stIdx.Idx());
            }
            bool sizeIs0 = false;
            if (sym != nullptr) {
                retType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(sym->GetTyIdx());
                if (beCommon.GetTypeSize(retType->GetTypeIndex().GetIdx()) == 0) {
                    sizeIs0 = true;
                }
            }
            if (!sizeIs0) {
                RegFieldPair regFieldPair = p2nRets[0].second;
                if (!regFieldPair.IsReg()) {
                    uint16 fieldID = static_cast<uint16>(regFieldPair.GetFieldID());
                    DassignNode *dn = SaveReturnValueInLocal(stIdx, fieldID);
                    CHECK_FATAL(dn->GetFieldID() == 0, "make sure dn's fieldID return 0");
                    LowerDassign(*dn, *blk);
                    CHECK_FATAL(&newCall == blk->GetLast() || newCall.GetNext() == blk->GetLast(), "");
                    dStmt = (&newCall == blk->GetLast()) ? nullptr : blk->GetLast();
                    CHECK_FATAL(newCall.GetNext() == dStmt, "make sure newCall's next equal dStmt");
                } else {
                    PregIdx pregIdx = static_cast<PregIdx>(regFieldPair.GetPregIdx());
                    MIRPreg *mirPreg = GetCurrentFunc()->GetPregTab()->PregFromPregIdx(pregIdx);
                    bool is64x1vec = beCommon.CallIsOfAttr(FUNCATTR_oneelem_simd, &newCall);
                    PrimType pType = is64x1vec ? PTY_f64 : mirPreg->GetPrimType();
                    RegreadNode *regNode = mirModule.GetMIRBuilder()->CreateExprRegread(pType, -kSregRetval0);
                    RegassignNode *regAssign;
                    if (is64x1vec && IsPrimitiveInteger(mirPreg->GetPrimType())) {  // not f64
                        MIRType *to;
                        if (IsUnsignedInteger(mirPreg->GetPrimType())) {
                            to = GlobalTables::GetTypeTable().GetUInt64();
                        } else {
                            to = GlobalTables::GetTypeTable().GetInt64();
                        }
                        MIRType *from = GlobalTables::GetTypeTable().GetDouble();
                        BaseNode *rNode = mirModule.GetMIRBuilder()->CreateExprRetype(*to, *from, regNode);
                        regAssign = mirModule.GetMIRBuilder()->CreateStmtRegassign(mirPreg->GetPrimType(),
                                                                                   regFieldPair.GetPregIdx(), rNode);
                    } else {
                        regAssign = mirModule.GetMIRBuilder()->CreateStmtRegassign(mirPreg->GetPrimType(),
                                                                                   regFieldPair.GetPregIdx(), regNode);
                    }
                    blk->AddStatement(regAssign);
                    dStmt = regAssign;
                }
            }
        }
        blk->ResetBlock();
        /* if VerboseCG, insert a comment */
        if (ShouldAddAdditionalComment()) {
            CommentNode *cmnt = mirModule.CurFuncCodeMemPool()->New<CommentNode>(mirModule);
            cmnt->SetComment(kOpcodeInfo.GetName(opcode).c_str());
            if (funcCalled == kFuncNotFound) {
                cmnt->Append(" : unknown");
            } else {
                cmnt->Append(" : ");
                cmnt->Append(GlobalTables::GetFunctionTable().GetFunctionFromPuidx(funcCalled)->GetName());
            }
            blk->AddStatement(cmnt);
        }
        CHECK_FATAL(dStmt == nullptr || dStmt->GetNext() == nullptr, "make sure dStmt or dStmt's next is nullptr");
        LowerCallStmt(newCall, dStmt, *blk, retType, uselvar ? true : false, opcode == OP_intrinsiccallassigned);
        if (!uselvar && dStmt != nullptr) {
            dStmt->SetSrcPos(newCall.GetSrcPos());
            blk->AddStatement(dStmt);
        }
    }
    return blk;
}

// try to expand memset and memcpy
BlockNode *CGLowerer::LowerMemop(StmtNode &stmt)
{
    auto memOpKind = SimplifyMemOp::ComputeMemOpKind(stmt);
    if (memOpKind == MEM_OP_unknown) {
        return nullptr;
    }
    auto *prev = stmt.GetPrev();
    auto *next = stmt.GetNext();
    auto *blk = mirModule.CurFuncCodeMemPool()->New<BlockNode>();
    blk->AddStatement(&stmt);
    uint32 oldTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    bool success = simplifyMemOp.AutoSimplify(stmt, *blk, true);
    uint32 newTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    if (newTypeTableSize != oldTypeTableSize) {
        beCommon.AddNewTypeAfterBecommon(oldTypeTableSize, newTypeTableSize);
    }
    stmt.SetPrev(prev);
    stmt.SetNext(next);  // recover callStmt's position
    if (!success) {
        return nullptr;
    }
    // lower new generated stmts
    auto *currStmt = blk->GetFirst();
    while (currStmt != nullptr) {
        auto *nextStmt = currStmt->GetNext();
        for (uint32 i = 0; i < currStmt->NumOpnds(); ++i) {
            currStmt->SetOpnd(LowerExpr(*currStmt, *currStmt->Opnd(i), *blk), i);
        }
        currStmt = nextStmt;
    }
    return blk;
}

BlockNode *CGLowerer::LowerIntrinsiccallAassignedToAssignStmt(IntrinsiccallNode &intrinsicCall)
{
    auto *builder = mirModule.GetMIRBuilder();
    auto *block = mirModule.CurFuncCodeMemPool()->New<BlockNode>();
    auto intrinsicID = intrinsicCall.GetIntrinsic();
    auto &opndVector = intrinsicCall.GetNopnd();
    auto returnPair = intrinsicCall.GetReturnVec().begin();
    auto regFieldPair = returnPair->second;
    if (regFieldPair.IsReg()) {
        auto regIdx = regFieldPair.GetPregIdx();
        auto primType = mirModule.CurFunction()->GetPregItem(static_cast<PregIdx>(regIdx))->GetPrimType();
        auto intrinsicOp = builder->CreateExprIntrinsicop(intrinsicID, OP_intrinsicop, primType, TyIdx(0), opndVector);
        auto regAssign = builder->CreateStmtRegassign(primType, regIdx, intrinsicOp);
        block->AddStatement(regAssign);
    } else {
        auto fieldID = regFieldPair.GetFieldID();
        auto stIdx = returnPair->first;
        auto *type = mirModule.CurFunction()->GetLocalOrGlobalSymbol(stIdx)->GetType();
        auto intrinsicOp = builder->CreateExprIntrinsicop(intrinsicID, OP_intrinsicop, *type, opndVector);
        auto dAssign = builder->CreateStmtDassign(stIdx, fieldID, intrinsicOp);
        block->AddStatement(dAssign);
    }
    return LowerBlock(*block);
}

BlockNode *CGLowerer::LowerCallAssignedStmt(StmtNode &stmt, bool uselvar)
{
    StmtNode *newCall = nullptr;
    CallReturnVector *p2nRets = nullptr;
    PUIdx funcCalled = kFuncNotFound;
    bool handledAtLowerLevel = false;
    switch (stmt.GetOpCode()) {
        case OP_callassigned:
        case OP_virtualcallassigned:
        case OP_superclasscallassigned:
        case OP_interfacecallassigned: {
            if (CGOptions::GetInstance().GetOptimizeLevel() >= CGOptions::kLevel2) {
                BlockNode *blkLowered = LowerMemop(stmt);
                if (blkLowered != nullptr) {
                    return blkLowered;
                }
            }
            auto &origCall = static_cast<CallNode &>(stmt);
            newCall = GenCallNode(stmt, funcCalled, origCall);
            p2nRets = &origCall.GetReturnVec();
            static_cast<CallNode *>(newCall)->SetReturnVec(*p2nRets);
            MIRFunction *curFunc = mirModule.CurFunction();
            curFunc->SetLastFreqMap(newCall->GetStmtID(),
                                    static_cast<uint32>(curFunc->GetFreqFromLastStmt(stmt.GetStmtID())));
            break;
        }
        case OP_intrinsiccallassigned:
        case OP_xintrinsiccallassigned: {
            IntrinsiccallNode &intrincall = static_cast<IntrinsiccallNode &>(stmt);
            auto intrinsicID = intrincall.GetIntrinsic();
            if (IntrinDesc::intrinTable[intrinsicID].IsAtomic()) {
                return LowerIntrinsiccallAassignedToAssignStmt(intrincall);
            }
            if (intrinsicID == INTRN_JAVA_POLYMORPHIC_CALL) {
                BaseNode *contextClassArg = GetBaseNodeFromCurFunc(*mirModule.CurFunction(), false);
                constexpr int kContextIdx = 4; /* stable index in MCC_DexPolymorphicCall, never out of range */
                intrincall.InsertOpnd(contextClassArg, kContextIdx);

                BaseNode *firstArg = intrincall.GetNopndAt(0);
                BaseNode *baseVal = mirBuilder->CreateExprBinary(OP_add, *GlobalTables::GetTypeTable().GetPtr(),
                                                                 firstArg, mirBuilder->CreateIntConst(1, PTY_ref));
                intrincall.SetNOpndAt(0, baseVal);
            }
            newCall = GenIntrinsiccallNode(stmt, funcCalled, handledAtLowerLevel, intrincall);
            p2nRets = &intrincall.GetReturnVec();
            static_cast<IntrinsiccallNode *>(newCall)->SetReturnVec(*p2nRets);
            break;
        }
        case OP_intrinsiccallwithtypeassigned: {
            auto &origCall = static_cast<IntrinsiccallNode &>(stmt);
            newCall = GenIntrinsiccallNode(stmt, funcCalled, handledAtLowerLevel, origCall);
            p2nRets = &origCall.GetReturnVec();
            static_cast<IntrinsiccallNode *>(newCall)->SetReturnVec(*p2nRets);
            break;
        }
        case OP_icallprotoassigned:
        case OP_icallassigned: {
            auto &origCall = static_cast<IcallNode &>(stmt);
            newCall = GenIcallNode(funcCalled, origCall);
            p2nRets = &origCall.GetReturnVec();
            static_cast<IcallNode *>(newCall)->SetReturnVec(*p2nRets);
            break;
        }
        default:
            CHECK_FATAL(false, "NIY");
            return nullptr;
    }

    /* transfer srcPosition location info */
    newCall->SetSrcPos(stmt.GetSrcPos());
    return GenBlockNode(*newCall, *p2nRets, stmt.GetOpCode(), funcCalled, handledAtLowerLevel, uselvar);
}

#if TARGAARCH64
static PrimType IsStructElementSame(MIRType *ty)
{
    if (ty->GetKind() == kTypeArray) {
        MIRArrayType *arrtype = static_cast<MIRArrayType *>(ty);
        MIRType *pty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(arrtype->GetElemTyIdx());
        if (pty->GetKind() == kTypeArray || pty->GetKind() == kTypeStruct) {
            return IsStructElementSame(pty);
        }
        return pty->GetPrimType();
    } else if (ty->GetKind() == kTypeStruct) {
        MIRStructType *sttype = static_cast<MIRStructType *>(ty);
        FieldVector fields = sttype->GetFields();
        PrimType oldtype = PTY_void;
        for (uint32 fcnt = 0; fcnt < fields.size(); ++fcnt) {
            TyIdx fieldtyidx = fields[fcnt].second.first;
            MIRType *fieldty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(fieldtyidx);
            PrimType ptype = IsStructElementSame(fieldty);
            if (oldtype != PTY_void && oldtype != ptype) {
                return PTY_void;
            } else {
                oldtype = ptype;
            }
        }
        return oldtype;
    } else {
        return ty->GetPrimType();
    }
}
#endif

// return true if successfully lowered; nextStmt is in/out, and is made to point
// to its following statement if lowering of the struct return is successful
bool CGLowerer::LowerStructReturn(BlockNode &newBlk, StmtNode *stmt, StmtNode *&nextStmt, bool &lvar, BlockNode *oldBlk)
{
    if (!nextStmt) {
        return false;
    }
    CallReturnVector *p2nrets = stmt->GetCallReturnVector();
    if (p2nrets->size() == 0) {
        return false;
    }
    CallReturnPair retPair = (*p2nrets)[0];
    if (retPair.second.IsReg()) {
        return false;
    }
    MIRSymbol *retSym = mirModule.CurFunction()->GetLocalOrGlobalSymbol(retPair.first);
    if (retSym->GetType()->GetPrimType() != PTY_agg) {
        return false;
    }
    if (nextStmt->op != OP_dassign) {
        // introduce a temporary and insert a dassign whose rhs is this temporary
        // and whose lhs is retSym
        MIRSymbol *temp = CreateNewRetVar(*retSym->GetType(), kUserRetValPrefix);
        BaseNode *rhs = mirModule.GetMIRBuilder()->CreateExprDread(*temp->GetType(), 0, *temp);
        DassignNode *dass =
            mirModule.GetMIRBuilder()->CreateStmtDassign(retPair.first, retPair.second.GetFieldID(), rhs);
        oldBlk->InsertBefore(nextStmt, dass);
        nextStmt = dass;
        // update CallReturnVector to the new temporary
        (*p2nrets)[0].first = temp->GetStIdx();
        (*p2nrets)[0].second.SetFieldID(0);
    }
    // now, it is certain that nextStmt is a dassign
    BaseNode *bnode = static_cast<DassignNode *>(nextStmt)->GetRHS();
    if (bnode->GetOpCode() != OP_dread) {
        return false;
    }
    DreadNode *dnode = static_cast<DreadNode *>(bnode);
    MIRType *dtype = mirModule.CurFunction()->GetLocalOrGlobalSymbol(dnode->GetStIdx())->GetType();
#if TARGAARCH64
    PrimType ty = IsStructElementSame(dtype);
    if (ty == PTY_f32 || ty == PTY_f64 || IsPrimitiveVector(ty)) {
        return false;
    }
#endif
    if (dnode->GetPrimType() != PTY_agg) {
        return false;
    }
    CallReturnPair pair = (*p2nrets)[0];
    if (pair.first != dnode->GetStIdx() || pair.second.GetFieldID() != dnode->GetFieldID()) {
        return false;
    }
    auto *dnodeStmt = static_cast<DassignNode *>(nextStmt);
    if (dnodeStmt->GetFieldID() != 0) {
        return false;
    }
    if (dtype->GetSize() > k16ByteSize) {
        (*p2nrets)[0].first = dnodeStmt->GetStIdx();
        (*p2nrets)[0].second.SetFieldID(dnodeStmt->GetFieldID());
        lvar = true;
        // set ATTR_firstarg_return for callee
        if (stmt->GetOpCode() == OP_callassigned) {
            CallNode *callNode = static_cast<CallNode *>(stmt);
            MIRFunction *f = GlobalTables::GetFunctionTable().GetFunctionFromPuidx(callNode->GetPUIdx());
            f->SetFirstArgReturn();
            f->GetMIRFuncType()->SetFirstArgReturn();
        } else {
            // for icall, front-end already set ATTR_firstarg_return
        }
    } else { /* struct <= 16 passed in regs lowered into
                            call &foo
                            regassign u64 %1 (regread u64 %%retval0)
                            regassign ptr %2 (addrof ptr $s)
                            iassign <* u64> 0 (regread ptr %2, regread u64 %1) */
        MIRSymbol *symbol = mirModule.CurFunction()->GetLocalOrGlobalSymbol(dnodeStmt->GetStIdx());
        auto *structType = static_cast<MIRStructType *>(symbol->GetType());
        auto size = static_cast<uint32>(structType->GetSize());
        if (stmt->GetOpCode() == OP_callassigned) {
            auto *callNode = static_cast<CallNode *>(stmt);
            for (size_t i = 0; i < callNode->GetNopndSize(); ++i) {
                BaseNode *newOpnd = LowerExpr(*callNode, *callNode->GetNopndAt(i), newBlk);
                callNode->SetOpnd(newOpnd, i);
            }
            CallNode *callStmt = mirModule.GetMIRBuilder()->CreateStmtCall(callNode->GetPUIdx(), callNode->GetNopnd());
            callStmt->SetSrcPos(callNode->GetSrcPos());
            newBlk.AddStatement(callStmt);
        } else if (stmt->GetOpCode() == OP_icallassigned || stmt->GetOpCode() == OP_icallprotoassigned) {
            auto *icallNode = static_cast<IcallNode *>(stmt);
            for (size_t i = 0; i < icallNode->GetNopndSize(); ++i) {
                BaseNode *newOpnd = LowerExpr(*icallNode, *icallNode->GetNopndAt(i), newBlk);
                icallNode->SetOpnd(newOpnd, i);
            }
            IcallNode *icallStmt = nullptr;
            if (stmt->GetOpCode() == OP_icallassigned) {
                icallStmt = mirModule.GetMIRBuilder()->CreateStmtIcall(icallNode->GetNopnd());
            } else {
                icallStmt = mirModule.GetMIRBuilder()->CreateStmtIcallproto(icallNode->GetNopnd());
                icallStmt->SetRetTyIdx(icallNode->GetRetTyIdx());
            }
            icallStmt->SetSrcPos(icallNode->GetSrcPos());
            newBlk.AddStatement(icallStmt);
        } else {
            return false;
        }

        uint32 origSize = size;
        PregIdx pIdxR, pIdx1R, pIdx2R;
        StmtNode *aStmt = nullptr;
        RegreadNode *reg = nullptr;

        /* save x0 */
        reg = mirBuilder->CreateExprRegread(PTY_u64, -kSregRetval0);
        pIdx1R = GetCurrentFunc()->GetPregTab()->CreatePreg(PTY_u64);
        aStmt = mirBuilder->CreateStmtRegassign(PTY_u64, pIdx1R, reg);
        newBlk.AddStatement(aStmt);

        /* save x1 */
        if (origSize > k8ByteSize) {
            reg = mirBuilder->CreateExprRegread(PTY_u64, -kSregRetval1);
            pIdx2R = GetCurrentFunc()->GetPregTab()->CreatePreg(PTY_u64);
            aStmt = mirBuilder->CreateStmtRegassign(PTY_u64, pIdx2R, reg);
            newBlk.AddStatement(aStmt);
        }

        /* save &s */
        BaseNode *regAddr = mirBuilder->CreateExprAddrof(0, *symbol);
        PregIdx pIdxL = GetCurrentFunc()->GetPregTab()->CreatePreg(GetLoweredPtrType());
        aStmt = mirBuilder->CreateStmtRegassign(PTY_a64, pIdxL, regAddr);
        newBlk.AddStatement(aStmt);

        uint32 curSize = 0;
        PregIdx pIdxS;
        while (size) {
            pIdxR = pIdx1R;
            if (curSize >= k8ByteSize) {
                pIdxR = pIdx2R;
            }
            BaseNode *addr;
            BaseNode *shift;
            BaseNode *regreadExp;
            if (origSize != size) {
                MIRType *addrType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(GetLoweredPtrType());
                addr = mirBuilder->CreateExprBinary(OP_add, *addrType,
                                                    mirBuilder->CreateExprRegread(GetLoweredPtrType(), pIdxL),
                                                    mirBuilder->CreateIntConst(origSize - size, PTY_i32));
            } else {
                addr = mirBuilder->CreateExprRegread(GetLoweredPtrType(), pIdxL);
            }
            if (size >= k8ByteSize) {
                aStmt = mirBuilder->CreateStmtIassign(
                    *beCommon.BeGetOrCreatePointerType(*GlobalTables::GetTypeTable().GetUInt64()), 0, addr,
                    mirBuilder->CreateExprRegread(PTY_u64, pIdxR));
                size -= k8ByteSize;
                curSize += k8ByteSize;
            } else if (size >= k4ByteSize) {
                MIRType *type = GlobalTables::GetTypeTable().GetTypeFromTyIdx(PTY_u64);

                if (CGOptions::IsBigEndian()) {
                    regreadExp =
                        mirBuilder->CreateExprBinary(OP_lshr, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                     mirBuilder->CreateIntConst(k64BitSize - k32BitSize, PTY_i32));
                } else {
                    regreadExp = mirBuilder->CreateExprRegread(PTY_u32, pIdxR);
                }

                aStmt = mirBuilder->CreateStmtIassign(
                    *beCommon.BeGetOrCreatePointerType(*GlobalTables::GetTypeTable().GetUInt32()), 0, addr, regreadExp);

                if (CGOptions::IsBigEndian()) {
                    shift = mirBuilder->CreateExprBinary(OP_shl, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                         mirBuilder->CreateIntConst(k32BitSize, PTY_i32));
                } else {
                    shift = mirBuilder->CreateExprBinary(OP_lshr, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                         mirBuilder->CreateIntConst(k32BitSize, PTY_i32));
                }

                pIdxS = GetCurrentFunc()->GetPregTab()->CreatePreg(PTY_u64);
                StmtNode *sStmp = mirBuilder->CreateStmtRegassign(PTY_u64, pIdxS, shift);

                pIdx1R = pIdx2R = pIdxS;
                newBlk.AddStatement(sStmp);
                size -= k4ByteSize;
                curSize += k4ByteSize;
            } else if (size >= k2ByteSize) {
                MIRType *type = GlobalTables::GetTypeTable().GetTypeFromTyIdx(PTY_u64);

                if (CGOptions::IsBigEndian()) {
                    regreadExp =
                        mirBuilder->CreateExprBinary(OP_lshr, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                     mirBuilder->CreateIntConst(k64BitSize - k16BitSize, PTY_i32));
                } else {
                    regreadExp = mirBuilder->CreateExprRegread(PTY_u16, pIdxR);
                }

                aStmt = mirBuilder->CreateStmtIassign(
                    *beCommon.BeGetOrCreatePointerType(*GlobalTables::GetTypeTable().GetUInt16()), 0, addr, regreadExp);

                if (CGOptions::IsBigEndian()) {
                    shift = mirBuilder->CreateExprBinary(OP_shl, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                         mirBuilder->CreateIntConst(k64BitSize - k16BitSize, PTY_i32));
                } else {
                    shift = mirBuilder->CreateExprBinary(OP_lshr, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                         mirBuilder->CreateIntConst(k16BitSize, PTY_i32));
                }

                pIdxS = GetCurrentFunc()->GetPregTab()->CreatePreg(PTY_u64);
                StmtNode *sStmp = mirBuilder->CreateStmtRegassign(PTY_u64, pIdxS, shift);

                pIdx1R = pIdx2R = pIdxS;
                newBlk.AddStatement(sStmp);
                size -= k2ByteSize;
                curSize += k2ByteSize;
            } else {
                MIRType *type = GlobalTables::GetTypeTable().GetTypeFromTyIdx(PTY_u64);

                if (CGOptions::IsBigEndian()) {
                    regreadExp =
                        mirBuilder->CreateExprBinary(OP_lshr, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                     mirBuilder->CreateIntConst(k64BitSize - k8BitSize, PTY_i32));
                } else {
                    regreadExp = mirBuilder->CreateExprRegread(PTY_u8, pIdxR);
                }

                aStmt = mirBuilder->CreateStmtIassign(
                    *beCommon.BeGetOrCreatePointerType(*GlobalTables::GetTypeTable().GetUInt8()), 0, addr, regreadExp);

                if (CGOptions::IsBigEndian()) {
                    shift = mirBuilder->CreateExprBinary(OP_shl, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                         mirBuilder->CreateIntConst(k64BitSize - k8BitSize, PTY_i32));
                } else {
                    shift = mirBuilder->CreateExprBinary(OP_lshr, *type, mirBuilder->CreateExprRegread(PTY_u64, pIdxR),
                                                         mirBuilder->CreateIntConst(k8BitSize, PTY_i32));
                }

                pIdxS = GetCurrentFunc()->GetPregTab()->CreatePreg(PTY_u64);
                StmtNode *sStmp = mirBuilder->CreateStmtRegassign(PTY_u64, pIdxS, shift);

                pIdx1R = pIdx2R = pIdxS;
                newBlk.AddStatement(sStmp);
                size -= k1ByteSize;
                curSize += k1ByteSize;
            }
            newBlk.AddStatement(aStmt);
        }
    }
    nextStmt = nextStmt->GetNext();  // skip the dassign
    return true;
}

void CGLowerer::LowerStmt(StmtNode &stmt, BlockNode &newBlk)
{
    for (size_t i = 0; i < stmt.NumOpnds(); ++i) {
        stmt.SetOpnd(LowerExpr(stmt, *stmt.Opnd(i), newBlk), i);
    }
}

void CGLowerer::LowerSwitchOpnd(StmtNode &stmt, BlockNode &newBlk)
{
    BaseNode *opnd = LowerExpr(stmt, *stmt.Opnd(0), newBlk);
    if (CGOptions::GetInstance().GetOptimizeLevel() >= CGOptions::kLevel2 && opnd->GetOpCode() != OP_regread) {
        PrimType ptyp = stmt.Opnd(0)->GetPrimType();
        PregIdx pIdx = GetCurrentFunc()->GetPregTab()->CreatePreg(ptyp);
        RegassignNode *regAss = mirBuilder->CreateStmtRegassign(ptyp, pIdx, opnd);
        newBlk.AddStatement(regAss);
        GetCurrentFunc()->SetLastFreqMap(regAss->GetStmtID(),
                                         static_cast<uint32>(GetCurrentFunc()->GetFreqFromLastStmt(stmt.GetStmtID())));
        stmt.SetOpnd(mirBuilder->CreateExprRegread(ptyp, pIdx), 0);
    } else {
        stmt.SetOpnd(LowerExpr(stmt, *stmt.Opnd(0), newBlk), 0);
    }
}

void CGLowerer::AddElemToPrintf(MapleVector<BaseNode *> &argsPrintf, int num, ...) const
{
    va_list argPtr;
    va_start(argPtr, num);
    for (int i = 0; i < num; ++i) {
        argsPrintf.push_back(va_arg(argPtr, BaseNode *));
    }
    va_end(argPtr);
}

void CGLowerer::SwitchAssertBoundary(StmtNode &stmt, MapleVector<BaseNode *> &argsPrintf)
{
    MIRSymbol *errMsg;
    MIRSymbol *fileNameSym;
    ConstvalNode *lineNum;
    fileNameSym = mirBuilder->CreateConstStringSymbol(GetFileNameSymbolName(AssertBoundaryGetFileName(stmt)),
                                                      AssertBoundaryGetFileName(stmt));
    lineNum = mirBuilder->CreateIntConst(stmt.GetSrcPos().LineNum(), PTY_u32);
    if (kOpcodeInfo.IsAssertLowerBoundary(stmt.GetOpCode())) {
        errMsg = mirBuilder->CreateConstStringSymbol(
            kOpAssertge, "%s:%d error: the pointer < the lower bounds when accessing the memory!\n");
        AddElemToPrintf(argsPrintf, 3 /* 3 parameters follow */, mirBuilder->CreateAddrof(*errMsg, PTY_a64),
                        mirBuilder->CreateAddrof(*fileNameSym, PTY_a64), lineNum);
    } else {
        if (kOpcodeInfo.IsAssertLeBoundary(stmt.GetOpCode())) {
            if (stmt.GetOpCode() == OP_callassertle) {
                auto &callStmt = static_cast<CallAssertBoundaryStmtNode &>(stmt);
                std::string param;
                MIRSymbol *funcName;
                MIRSymbol *paramNum;
                param = maple::GetNthStr(callStmt.GetParamIndex());
                errMsg = mirBuilder->CreateConstStringSymbol(kOpCallAssertle,
                                                             "%s:%d error: the pointer's bounds does not match the "
                                                             "function %s declaration for the %s argument!\n");
                funcName = mirBuilder->CreateConstStringSymbol(callStmt.GetFuncName() + kOpCallAssertle,
                                                               callStmt.GetFuncName());
                paramNum = mirBuilder->CreateConstStringSymbol(kOpCallAssertle + param, param);
                AddElemToPrintf(argsPrintf, 5 /* 5 parameters follow */, mirBuilder->CreateAddrof(*errMsg, PTY_a64),
                                mirBuilder->CreateAddrof(*fileNameSym, PTY_a64), lineNum,
                                mirBuilder->CreateAddrof(*funcName, PTY_a64),
                                mirBuilder->CreateAddrof(*paramNum, PTY_a64));
            } else if (stmt.GetOpCode() == OP_returnassertle) {
                auto &callStmt = static_cast<CallAssertBoundaryStmtNode &>(stmt);
                MIRSymbol *funcName;
                errMsg = mirBuilder->CreateConstStringSymbol(
                    kOpReturnAssertle,
                    "%s:%d error: return value's bounds does not match the function declaration for %s\n");
                funcName = mirBuilder->CreateConstStringSymbol(callStmt.GetFuncName() + kOpReturnAssertle,
                                                               callStmt.GetFuncName());
                AddElemToPrintf(argsPrintf, 4 /* 4 parameters follow */, mirBuilder->CreateAddrof(*errMsg, PTY_a64),
                                mirBuilder->CreateAddrof(*fileNameSym, PTY_a64), lineNum,
                                mirBuilder->CreateAddrof(*funcName, PTY_a64));
            } else {
                errMsg = mirBuilder->CreateConstStringSymbol(
                    kOpAssignAssertle, "%s:%d error: l-value boundary should not be larger than r-value boundary!\n");
                AddElemToPrintf(argsPrintf, 3 /* 3 parameters follow */, mirBuilder->CreateAddrof(*errMsg, PTY_a64),
                                mirBuilder->CreateAddrof(*fileNameSym, PTY_a64), lineNum);
            }
        } else {
            errMsg = mirBuilder->CreateConstStringSymbol(
                kOpAssertlt, "%s:%d error: the pointer >= the upper bounds when accessing the memory!\n");
            AddElemToPrintf(argsPrintf, 3 /* 3 parameters follow */, mirBuilder->CreateAddrof(*errMsg, PTY_a64),
                            mirBuilder->CreateAddrof(*fileNameSym, PTY_a64), lineNum);
        }
    }
}

void CGLowerer::LowerAssertBoundary(StmtNode &stmt, BlockNode &block, BlockNode &newBlk,
                                    std::vector<StmtNode *> &abortNode)
{
    MIRFunction *curFunc = mirModule.CurFunction();
    BaseNode *op0 = LowerExpr(stmt, *stmt.Opnd(0), block);
    BaseNode *op1 = LowerExpr(stmt, *stmt.Opnd(1), block);
    LabelIdx labIdx = GetLabelIdx(*curFunc);
    LabelNode *labelBC = mirBuilder->CreateStmtLabel(labIdx);
    Opcode op = OP_ge;
    if (kOpcodeInfo.IsAssertUpperBoundary(stmt.GetOpCode())) {
        op = (kOpcodeInfo.IsAssertLeBoundary(stmt.GetOpCode())) ? OP_le : OP_lt;
    }
    BaseNode *cond =
        mirBuilder->CreateExprCompare(op, *GlobalTables::GetTypeTable().GetUInt1(),
                                      *GlobalTables::GetTypeTable().GetPrimType(op0->GetPrimType()), op0, op1);
    CondGotoNode *brFalseNode = mirBuilder->CreateStmtCondGoto(cond, OP_brfalse, labIdx);

    MIRFunction *printf = mirBuilder->GetOrCreateFunction("printf", TyIdx(PTY_i32));
    printf->GetFuncSymbol()->SetAppearsInCode(true);
    beCommon.UpdateTypeTable(*printf->GetMIRFuncType());
    MapleVector<BaseNode *> argsPrintf(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
    uint32 oldTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    SwitchAssertBoundary(stmt, argsPrintf);
    uint32 newTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    if (newTypeTableSize != oldTypeTableSize) {
        beCommon.AddNewTypeAfterBecommon(oldTypeTableSize, newTypeTableSize);
    }
    StmtNode *callPrintf = mirBuilder->CreateStmtCall(printf->GetPuidx(), argsPrintf);
    UnaryStmtNode *abortModeNode = mirBuilder->CreateStmtUnary(OP_abort, nullptr);

    brFalseNode->SetSrcPos(stmt.GetSrcPos());
    labelBC->SetSrcPos(stmt.GetSrcPos());
    callPrintf->SetSrcPos(stmt.GetSrcPos());
    abortModeNode->SetSrcPos(stmt.GetSrcPos());

    newBlk.AddStatement(brFalseNode);
    abortNode.emplace_back(labelBC);
    abortNode.emplace_back(callPrintf);
    abortNode.emplace_back(abortModeNode);
}

BlockNode *CGLowerer::LowerBlock(BlockNode &block)
{
    BlockNode *newBlk = mirModule.CurFuncCodeMemPool()->New<BlockNode>();
    BlockNode *tmpBlockNode = nullptr;
    std::vector<StmtNode *> abortNode;
    if (block.GetFirst() == nullptr) {
        return newBlk;
    }

    StmtNode *nextStmt = block.GetFirst();
    do {
        StmtNode *stmt = nextStmt;
        nextStmt = stmt->GetNext();
        stmt->SetNext(nullptr);
        currentBlock = newBlk;

        switch (stmt->GetOpCode()) {
            case OP_switch: {
                LowerSwitchOpnd(*stmt, *newBlk);
                auto switchMp = std::make_unique<ThreadLocalMemPool>(memPoolCtrler, "switchlowere");
                MapleAllocator switchAllocator(switchMp.get());
                SwitchLowerer switchLowerer(mirModule, static_cast<SwitchNode &>(*stmt), switchAllocator);
                BlockNode *blk = switchLowerer.LowerSwitch();
                if (blk->GetFirst() != nullptr) {
                    newBlk->AppendStatementsFromBlock(*blk);
                }
                needBranchCleanup = true;
                break;
            }
            case OP_block:
                tmpBlockNode = LowerBlock(static_cast<BlockNode &>(*stmt));
                CHECK_FATAL(tmpBlockNode != nullptr, "nullptr is not expected");
                newBlk->AppendStatementsFromBlock(*tmpBlockNode);
                break;
            case OP_dassign: {
                LowerDassign(static_cast<DassignNode &>(*stmt), *newBlk);
                break;
            }
            case OP_regassign: {
                LowerRegassign(static_cast<RegassignNode &>(*stmt), *newBlk);
                break;
            }
                CASE_OP_ASSERT_BOUNDARY
                {
                    LowerAssertBoundary(*stmt, block, *newBlk, abortNode);
                    break;
                }
            case OP_iassign: {
                LowerIassign(static_cast<IassignNode &>(*stmt), *newBlk);
                break;
            }
            case OP_callassigned:
            case OP_icallassigned:
            case OP_icallprotoassigned: {
                // pass the addr of lvar if this is a struct call assignment
                bool lvar = false;
                // nextStmt could be changed by the call to LowerStructReturn
                if (!LowerStructReturn(*newBlk, stmt, nextStmt, lvar, &block)) {
                    newBlk->AppendStatementsFromBlock(*LowerCallAssignedStmt(*stmt, lvar));
                }
                break;
            }
            case OP_virtualcallassigned:
            case OP_superclasscallassigned:
            case OP_interfacecallassigned:
            case OP_intrinsiccallassigned:
            case OP_xintrinsiccallassigned:
            case OP_intrinsiccallwithtypeassigned:
                newBlk->AppendStatementsFromBlock(*LowerCallAssignedStmt(*stmt));
                break;
            case OP_intrinsiccall:
            case OP_call:
            case OP_icall:
            case OP_icallproto:
#if TARGARM32 || TARGAARCH64 || TARGRISCV64 || TARGX86_64
                // nextStmt could be changed by the call to LowerStructReturn
                LowerCallStmt(*stmt, nextStmt, *newBlk);
#else
                LowerStmt(*stmt, *newBlk);
#endif
                break;
            case OP_return: {
#if TARGARM32 || TARGAARCH64 || TARGRISCV64 || TARGX86_64
                if (GetCurrentFunc()->IsFirstArgReturn() && stmt->NumOpnds() > 0) {
                    newBlk->AppendStatementsFromBlock(
                        *LowerReturnStructUsingFakeParm(static_cast<NaryStmtNode &>(*stmt)));
                } else {
#endif
                    NaryStmtNode *retNode = static_cast<NaryStmtNode *>(stmt);
                    if (retNode->GetNopndSize() == 0) {
                        newBlk->AddStatement(stmt);
                    } else {
                        tmpBlockNode = LowerReturn(*retNode);
                        CHECK_FATAL(tmpBlockNode != nullptr, "nullptr is not expected");
                        newBlk->AppendStatementsFromBlock(*tmpBlockNode);
                    }
#if TARGARM32 || TARGAARCH64 || TARGRISCV64 || TARGX86_64
                }
#endif
                break;
            }
            case OP_comment:
                newBlk->AddStatement(stmt);
                break;
            case OP_try:
                LowerStmt(*stmt, *newBlk);
                newBlk->AddStatement(stmt);
                hasTry = true;
                break;
            case OP_endtry:
                LowerStmt(*stmt, *newBlk);
                newBlk->AddStatement(stmt);
                break;
            case OP_catch:
                LowerStmt(*stmt, *newBlk);
                newBlk->AddStatement(stmt);
                break;
            case OP_throw:
                if (mirModule.IsJavaModule()) {
                    if (GenerateExceptionHandlingCode()) {
                        LowerStmt(*stmt, *newBlk);
                        newBlk->AddStatement(stmt);
                    }
                } else {
                    LowerStmt(*stmt, *newBlk);
                    newBlk->AddStatement(stmt);
                }
                break;
            case OP_syncenter:
            case OP_syncexit: {
                LowerStmt(*stmt, *newBlk);
                StmtNode *tmp = LowerSyncEnterSyncExit(*stmt);
                CHECK_FATAL(tmp != nullptr, "nullptr is not expected");
                newBlk->AddStatement(tmp);
                break;
            }
            case OP_decrefreset: {
                /*
                 * only gconly can reach here
                 * lower stmt (decrefreset (addrof ptr %RegX_RXXXX)) to (dassign %RegX_RXXXX 0 (constval ref 0))
                 */
                CHECK_FATAL(CGOptions::IsGCOnly(), "OP_decrefreset is expected only in gconly.");
                LowerResetStmt(*stmt, *newBlk);
                break;
            }
            case OP_asm: {
                LowerAsmStmt(static_cast<AsmNode *>(stmt), newBlk);
                break;
            }
            default:
                LowerStmt(*stmt, *newBlk);
                newBlk->AddStatement(stmt);
                break;
        }
        CHECK_FATAL(beCommon.GetSizeOfTypeSizeTable() == GlobalTables::GetTypeTable().GetTypeTableSize(), "Error!");
    } while (nextStmt != nullptr);
    for (auto node : abortNode) {
        newBlk->AddStatement(node);
    }
    return newBlk;
}

void CGLowerer::SimplifyBlock(BlockNode &block) const
{
    if (block.GetFirst() == nullptr) {
        return;
    }
    StmtNode *nextStmt = block.GetFirst();
    do {
        StmtNode *stmt = nextStmt;
        nextStmt = stmt->GetNext();
        Opcode op = stmt->GetOpCode();
        switch (op) {
            case OP_call: {
                auto *callStmt = static_cast<CallNode *>(stmt);
                if (CGOptions::IsDuplicateAsmFileEmpty()) {
                    break;
                }
                auto *oldFunc = GlobalTables::GetFunctionTable().GetFunctionFromPuidx(callStmt->GetPUIdx());
                if (asmMap.find(oldFunc->GetName()) == asmMap.end()) {
                    break;
                }
                auto *newFunc = theMIRModule->GetMIRBuilder()->GetOrCreateFunction(asmMap.at(oldFunc->GetName()),
                                                                                   callStmt->GetTyIdx());
                MIRSymbol *funcSym = newFunc->GetFuncSymbol();
                funcSym->SetStorageClass(kScExtern);
                funcSym->SetAppearsInCode(true);
                callStmt->SetPUIdx(newFunc->GetPuidx());
                break;
            }
            default: {
                break;
            }
        }
    } while (nextStmt != nullptr);
    return;
}

MIRType *CGLowerer::GetArrayNodeType(BaseNode &baseNode)
{
    MIRType *baseType = nullptr;
    auto curFunc = mirModule.CurFunction();
    if (baseNode.GetOpCode() == OP_regread) {
        RegreadNode *rrNode = static_cast<RegreadNode *>(&baseNode);
        MIRPreg *pReg = curFunc->GetPregTab()->PregFromPregIdx(rrNode->GetRegIdx());
        if (pReg->IsRef()) {
            baseType = pReg->GetMIRType();
        }
    }
    if (baseNode.GetOpCode() == OP_dread) {
        DreadNode *dreadNode = static_cast<DreadNode *>(&baseNode);
        MIRSymbol *symbol = curFunc->GetLocalOrGlobalSymbol(dreadNode->GetStIdx());
        baseType = symbol->GetType();
    }
    MIRType *arrayElemType = nullptr;
    if (baseType != nullptr) {
        MIRType *stType =
            GlobalTables::GetTypeTable().GetTypeFromTyIdx(static_cast<MIRPtrType *>(baseType)->GetPointedTyIdx());
        while (stType->GetKind() == kTypeJArray) {
            MIRJarrayType *baseType1 = static_cast<MIRJarrayType *>(stType);
            MIRType *elemType = baseType1->GetElemType();
            if (elemType->GetKind() == kTypePointer) {
                const TyIdx &index = static_cast<MIRPtrType *>(elemType)->GetPointedTyIdx();
                stType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(index);
            } else {
                stType = elemType;
            }
        }

        arrayElemType = stType;
    }
    return arrayElemType;
}

void CGLowerer::SplitCallArg(CallNode &callNode, BaseNode *newOpnd, size_t i, BlockNode &newBlk)
{
    if (newOpnd->GetOpCode() != OP_regread && newOpnd->GetOpCode() != OP_constval && newOpnd->GetOpCode() != OP_dread &&
        newOpnd->GetOpCode() != OP_addrof && newOpnd->GetOpCode() != OP_iaddrof &&
        newOpnd->GetOpCode() != OP_constval && newOpnd->GetOpCode() != OP_conststr &&
        newOpnd->GetOpCode() != OP_conststr16) {
        if (CGOptions::GetInstance().GetOptimizeLevel() == CGOptions::kLevel0) {
            MIRType *type = GlobalTables::GetTypeTable().GetPrimType(newOpnd->GetPrimType());
            MIRSymbol *ret = CreateNewRetVar(*type, kIntrnRetValPrefix);
            DassignNode *dassignNode = mirBuilder->CreateStmtDassign(*ret, 0, newOpnd);
            newBlk.AddStatement(dassignNode);
            callNode.SetOpnd(mirBuilder->CreateExprDread(*type, 0, *ret), i);
        } else {
            PregIdx pregIdx = mirModule.CurFunction()->GetPregTab()->CreatePreg(newOpnd->GetPrimType());
            RegassignNode *temp = mirBuilder->CreateStmtRegassign(newOpnd->GetPrimType(), pregIdx, newOpnd);
            newBlk.AddStatement(temp);
            callNode.SetOpnd(mirBuilder->CreateExprRegread(newOpnd->GetPrimType(), pregIdx), i);
        }
    } else {
        callNode.SetOpnd(newOpnd, i);
    }
}

StmtNode *CGLowerer::LowerCall(CallNode &callNode, StmtNode *&nextStmt, BlockNode &newBlk, MIRType *retTy, bool uselvar)
{
    /*
     * nextStmt in-out
     * call $foo(constval u32 128)
     * dassign %jlt (dread agg %%retval)
     */
    bool isArrayStore = false;

    if (callNode.GetOpCode() == OP_call) {
        MIRFunction *calleeFunc = GlobalTables::GetFunctionTable().GetFunctionFromPuidx(callNode.GetPUIdx());
        if ((calleeFunc->GetName() == "MCC_WriteRefField") && (callNode.Opnd(1)->GetOpCode() == OP_iaddrof)) {
            IreadNode *addrExpr = static_cast<IreadNode *>(callNode.Opnd(1));
            if (addrExpr->Opnd(0)->GetOpCode() == OP_array) {
                isArrayStore = true;
            }
        }
    }

    for (size_t i = 0; i < callNode.GetNopndSize(); ++i) {
        BaseNode *newOpnd = LowerExpr(callNode, *callNode.GetNopndAt(i), newBlk);
#if TARGAARCH64 || TARGRISCV64 || TARGX86_64
        callNode.SetOpnd(newOpnd, i);
#else
        SplitCallArg(callNode, newOpnd, i, newBlk);
#endif
    }

    if (isArrayStore && checkLoadStore) {
        bool needCheckStore = true;
        MIRType *arrayElemType = GetArrayNodeType(*callNode.Opnd(0));
        MIRType *valueRealType = GetArrayNodeType(*callNode.Opnd(kNodeThirdOpnd));
        if ((arrayElemType != nullptr) && (valueRealType != nullptr) && (arrayElemType->GetKind() == kTypeClass) &&
            static_cast<MIRClassType *>(arrayElemType)->IsFinal() && (valueRealType->GetKind() == kTypeClass) &&
            static_cast<MIRClassType *>(valueRealType)->IsFinal() &&
            valueRealType->GetTypeIndex() == arrayElemType->GetTypeIndex()) {
            needCheckStore = false;
        }

        if (needCheckStore) {
            MIRFunction *fn =
                mirModule.GetMIRBuilder()->GetOrCreateFunction("MCC_Reflect_Check_Arraystore", TyIdx(PTY_void));
            fn->GetFuncSymbol()->SetAppearsInCode(true);
            beCommon.UpdateTypeTable(*fn->GetMIRFuncType());
            fn->AllocSymTab();
            MapleVector<BaseNode *> args(mirModule.GetMIRBuilder()->GetCurrentFuncCodeMpAllocator()->Adapter());
            args.emplace_back(callNode.Opnd(0));
            args.emplace_back(callNode.Opnd(kNodeThirdOpnd));
            StmtNode *checkStoreStmt = mirModule.GetMIRBuilder()->CreateStmtCall(fn->GetPuidx(), args);
            newBlk.AddStatement(checkStoreStmt);
        }
    }

    DassignNode *dassignNode = nullptr;
    if ((nextStmt != nullptr) && (nextStmt->GetOpCode() == OP_dassign)) {
        dassignNode = static_cast<DassignNode *>(nextStmt);
    }

    /* if nextStmt is not a dassign stmt, return */
    if (dassignNode == nullptr) {
        return &callNode;
    }

    if (!uselvar && retTy && beCommon.GetTypeSize(retTy->GetTypeIndex().GetIdx()) <= k16ByteSize) {
        /* return structure fitting in one or two regs. */
        return &callNode;
    }

    MIRType *retType = nullptr;
    if (callNode.op == OP_icall || callNode.op == OP_icallproto) {
        if (retTy == nullptr) {
            return &callNode;
        } else {
            retType = retTy;
        }
    }

    if (retType == nullptr) {
        MIRFunction *calleeFunc = GlobalTables::GetFunctionTable().GetFunctionFromPuidx(callNode.GetPUIdx());
        retType = calleeFunc->GetReturnType();
        if (calleeFunc->IsReturnStruct() && (retType->GetPrimType() == PTY_void)) {
            MIRPtrType *pretType = static_cast<MIRPtrType *>((calleeFunc->GetNthParamType(0)));
            CHECK_FATAL(pretType != nullptr, "nullptr is not expected");
            retType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(pretType->GetPointedTyIdx());
            CHECK_FATAL((retType->GetKind() == kTypeStruct) || (retType->GetKind() == kTypeUnion),
                        "make sure retType is a struct type");
        }
    }

    /* if return type is not of a struct, return */
    if ((retType->GetKind() != kTypeStruct) && (retType->GetKind() != kTypeUnion)) {
        return &callNode;
    }

    MIRSymbol *dsgnSt = mirModule.CurFunction()->GetLocalOrGlobalSymbol(dassignNode->GetStIdx());
    CHECK_FATAL(dsgnSt->GetType()->IsStructType(), "expects a struct type");
    MIRStructType *structTy = static_cast<MIRStructType *>(dsgnSt->GetType());
    if (structTy == nullptr) {
        return &callNode;
    }

    RegreadNode *regReadNode = nullptr;
    if (dassignNode->Opnd(0)->GetOpCode() == OP_regread) {
        regReadNode = static_cast<RegreadNode *>(dassignNode->Opnd(0));
    }
    if (regReadNode == nullptr || (regReadNode->GetRegIdx() != -kSregRetval0)) {
        return &callNode;
    }

    MapleVector<BaseNode *> newNopnd(mirModule.CurFuncCodeMemPoolAllocator()->Adapter());
    AddrofNode *addrofNode = mirModule.CurFuncCodeMemPool()->New<AddrofNode>(OP_addrof);
    addrofNode->SetPrimType(GetLoweredPtrType());
    addrofNode->SetStIdx(dsgnSt->GetStIdx());
    addrofNode->SetFieldID(0);

    if (callNode.op == OP_icall || callNode.op == OP_icallproto) {
        auto ond = callNode.GetNopnd().begin();
        newNopnd.emplace_back(*ond);
        newNopnd.emplace_back(addrofNode);
        for (++ond; ond != callNode.GetNopnd().end(); ++ond) {
            newNopnd.emplace_back(*ond);
        }
    } else {
        newNopnd.emplace_back(addrofNode);
        for (auto *opnd : callNode.GetNopnd()) {
            newNopnd.emplace_back(opnd);
        }
    }

    callNode.SetNOpnd(newNopnd);
    callNode.SetNumOpnds(static_cast<uint8>(newNopnd.size()));
    CHECK_FATAL(nextStmt != nullptr, "nullptr is not expected");
    nextStmt = nextStmt->GetNext();
    return &callNode;
}

void CGLowerer::LowerEntry(MIRFunction &func)
{
    // determine if needed to insert fake parameter to return struct for current function
    if (func.IsReturnStruct()) {
        MIRType *retType = func.GetReturnType();
#if TARGAARCH64
        PrimType pty = IsStructElementSame(retType);
        if (pty == PTY_f32 || pty == PTY_f64 || IsPrimitiveVector(pty)) {
            func.SetStructReturnedInRegs();
            return;
        }
#endif
        if (retType->GetPrimType() != PTY_agg) {
            return;
        }
        if (retType->GetSize() > k16ByteSize) {
            func.SetFirstArgReturn();
            func.GetMIRFuncType()->SetFirstArgReturn();
        } else {
            func.SetStructReturnedInRegs();
        }
    }
    if (func.IsFirstArgReturn() && func.GetReturnType()->GetPrimType() != PTY_void) {
        MIRSymbol *retSt = func.GetSymTab()->CreateSymbol(kScopeLocal);
        retSt->SetStorageClass(kScFormal);
        retSt->SetSKind(kStVar);
        std::string retName(".return.");
        MIRSymbol *funcSt = GlobalTables::GetGsymTable().GetSymbolFromStidx(func.GetStIdx().Idx());
        retName += funcSt->GetName();
        retSt->SetNameStrIdx(retName);
        MIRType *pointType = beCommon.BeGetOrCreatePointerType(*func.GetReturnType());

        retSt->SetTyIdx(pointType->GetTypeIndex());
        std::vector<MIRSymbol *> formals;
        formals.emplace_back(retSt);
        for (uint32 i = 0; i < func.GetFormalCount(); ++i) {
            auto formal = func.GetFormal(i);
            formals.emplace_back(formal);
        }
        func.SetFirstArgReturn();

        beCommon.AddElementToFuncReturnType(func, func.GetReturnTyIdx());

        func.UpdateFuncTypeAndFormalsAndReturnType(formals, TyIdx(PTY_void), true);
        auto *funcType = func.GetMIRFuncType();
        DEBUG_ASSERT(funcType != nullptr, "null ptr check");
        funcType->SetFirstArgReturn();
        beCommon.AddTypeSizeAndAlign(funcType->GetTypeIndex(), GetPrimTypeSize(funcType->GetPrimType()));
    }
}

void CGLowerer::LowerPseudoRegs(const MIRFunction &func) const
{
    for (uint32 i = 1; i < func.GetPregTab()->Size(); ++i) {
        MIRPreg *ipr = func.GetPregTab()->PregFromPregIdx(i);
        PrimType primType = ipr->GetPrimType();
        if (primType == PTY_u1) {
            ipr->SetPrimType(PTY_u32);
        }
    }
}

void CGLowerer::CleanupBranches(MIRFunction &func) const
{
    BlockNode *block = func.GetBody();
    StmtNode *prev = nullptr;
    StmtNode *next = nullptr;
    for (StmtNode *curr = block->GetFirst(); curr != nullptr; curr = next) {
        next = curr->GetNext();
        if (next != nullptr) {
            CHECK_FATAL(curr == next->GetPrev(), "unexpected node");
        }
        if ((next != nullptr) && (prev != nullptr) && (curr->GetOpCode() == OP_goto)) {
            /*
             * Skip until find a label.
             * Note that the CURRent 'goto' statement may be the last statement
             * when discounting comment statements.
             * Make sure we don't lose any comments.
             */
            StmtNode *cmtB = nullptr;
            StmtNode *cmtE = nullptr;
            bool isCleanable = true;
            while ((next != nullptr) && (next->GetOpCode() != OP_label)) {
                if ((next->GetOpCode() == OP_try) || (next->GetOpCode() == OP_endtry) ||
                    (next->GetOpCode() == OP_catch)) {
                    isCleanable = false;
                    break;
                }
                next = next->GetNext();
            }
            if ((next != nullptr) && (!isCleanable)) {
                prev = next->GetPrev();
                continue;
            }

            next = curr->GetNext();

            while ((next != nullptr) && (next->GetOpCode() != OP_label)) {
                if (next->GetOpCode() == OP_comment) {
                    if (cmtB == nullptr) {
                        cmtB = next;
                        cmtE = next;
                    } else {
                        CHECK_FATAL(cmtE != nullptr, "cmt_e is null in CGLowerer::CleanupBranches");
                        cmtE->SetNext(next);
                        next->SetPrev(cmtE);
                        cmtE = next;
                    }
                }
                next = next->GetNext();
            }

            curr->SetNext(next);

            if (next != nullptr) {
                next->SetPrev(curr);
            }

            StmtNode *insertAfter = nullptr;

            if ((next != nullptr) &&
                ((static_cast<GotoNode *>(curr))->GetOffset() == (static_cast<LabelNode *>(next))->GetLabelIdx())) {
                insertAfter = prev;
                prev->SetNext(next); /* skip goto statement (which is pointed by curr) */
                next->SetPrev(prev);
                curr = next;            /* make curr point to the label statement */
                next = next->GetNext(); /* advance next to the next statement of the label statement */
            } else {
                insertAfter = curr;
            }

            /* insert comments before 'curr' */
            if (cmtB != nullptr) {
                CHECK_FATAL(cmtE != nullptr, "nullptr is not expected");
                StmtNode *iaNext = insertAfter->GetNext();
                if (iaNext != nullptr) {
                    iaNext->SetPrev(cmtE);
                }
                cmtE->SetNext(iaNext);

                insertAfter->SetNext(cmtB);
                cmtB->SetPrev(insertAfter);

                if (insertAfter == curr) {
                    curr = cmtE;
                }
            }
            if (next == nullptr) {
                func.GetBody()->SetLast(curr);
            }
        }
        prev = curr;
    }
    CHECK_FATAL(func.GetBody()->GetLast() == prev, "make sure the return value of GetLast equal prev");
}

/*
 * We want to place catch blocks so that they don't come before any of java trys that refer to them.
 * In order to do that, we take advantage of the fact that the mpl. source we get is already flattened and
 * no java-try-end-try block is enclosed in any other java-try-end-try block. they appear in the mpl file.
 * We process each bb in bbList from the front to the end, and while doing so, we maintain a list of catch blocks
 * we have seen. When we get to an end-try block, we examine each catch block label it has (offsets),
 * and if we find any catch block in the "seen" list, we move the block after the end-try block.
 * Note that we need to find a basic block which does not have 'fallthruBranch' control path.
 * (Appending the catch block to any basic block that has the 'fallthruBranch' control path
 * will alter the program semantics)
 */
void CGLowerer::LowerTryCatchBlocks(BlockNode &body)
{
    if (!hasTry) {
        return;
    }

#if DEBUG
    BBT::ValidateStmtList(nullptr, nullptr);
#endif
    auto memPool = std::make_unique<ThreadLocalMemPool>(memPoolCtrler, "CreateNewBB mempool");
    TryCatchBlocksLower tryCatchLower(*memPool, body, mirModule);
    tryCatchLower.RecoverBasicBlock();
    bool generateEHCode = GenerateExceptionHandlingCode();
    tryCatchLower.SetGenerateEHCode(generateEHCode);
    tryCatchLower.TraverseBBList();
#if DEBUG
    tryCatchLower.CheckTryCatchPattern();
#endif
}

inline bool IsAccessingTheSameMemoryLocation(const DassignNode &dassign, const RegreadNode &rRead,
                                             const CGLowerer &cgLowerer)
{
    StIdx stIdx = cgLowerer.GetSymbolReferredToByPseudoRegister(rRead.GetRegIdx());
    return ((dassign.GetStIdx() == stIdx) && (dassign.GetFieldID() == 0));
}

inline bool IsAccessingTheSameMemoryLocation(const DassignNode &dassign, const DreadNode &dread)
{
    return ((dassign.GetStIdx() == dread.GetStIdx()) && (dassign.GetFieldID() == dread.GetFieldID()));
}

inline bool IsDassignNOP(const DassignNode &dassign)
{
    if (dassign.GetRHS()->GetOpCode() == OP_dread) {
        return IsAccessingTheSameMemoryLocation(dassign, static_cast<DreadNode &>(*dassign.GetRHS()));
    }
    return false;
}

inline bool IsConstvalZero(const BaseNode &n)
{
    return ((n.GetOpCode() == OP_constval) && static_cast<const ConstvalNode &>(n).GetConstVal()->IsZero());
}

#define NEXT_ID(x) ((x) + 1)
#define INTRN_FIRST_SYNC_ENTER NEXT_ID(INTRN_LAST)
#define INTRN_SECOND_SYNC_ENTER NEXT_ID(INTRN_FIRST_SYNC_ENTER)
#define INTRN_THIRD_SYNC_ENTER NEXT_ID(INTRN_SECOND_SYNC_ENTER)
#define INTRN_FOURTH_SYNC_ENTER NEXT_ID(INTRN_THIRD_SYNC_ENTER)
#define INTRN_YNC_EXIT NEXT_ID(INTRN_FOURTH_SYNC_ENTER)

std::vector<std::pair<CGLowerer::BuiltinFunctionID, PUIdx>> CGLowerer::builtinFuncIDs;
std::unordered_map<IntrinDesc *, PUIdx> CGLowerer::intrinFuncIDs;
std::unordered_map<std::string, size_t> CGLowerer::arrayClassCacheIndex;

MIRFunction *CGLowerer::RegisterFunctionVoidStarToVoid(BuiltinFunctionID id, const std::string &name,
                                                       const std::string &paramName)
{
    MIRFunction *func = mirBuilder->GetOrCreateFunction(name, GlobalTables::GetTypeTable().GetVoid()->GetTypeIndex());
    beCommon.UpdateTypeTable(*func->GetMIRFuncType());
    func->AllocSymTab();
    MIRSymbol *funcSym = func->GetFuncSymbol();
    funcSym->SetStorageClass(kScExtern);
    funcSym->SetAppearsInCode(true);
    MIRType *argTy = GlobalTables::GetTypeTable().GetPtr();
    MIRSymbol *argSt = func->GetSymTab()->CreateSymbol(kScopeLocal);
    argSt->SetNameStrIdx(mirBuilder->GetOrCreateStringIndex(paramName));
    argSt->SetTyIdx(argTy->GetTypeIndex());
    argSt->SetStorageClass(kScFormal);
    argSt->SetSKind(kStVar);
    func->GetSymTab()->AddToStringSymbolMap(*argSt);
    std::vector<MIRSymbol *> formals;
    formals.emplace_back(argSt);
    if ((name == "MCC_SyncEnterFast0") || (name == "MCC_SyncEnterFast1") || (name == "MCC_SyncEnterFast2") ||
        (name == "MCC_SyncEnterFast3") || (name == "MCC_SyncExitFast")) {
        MIRSymbol *argStMatch = func->GetSymTab()->CreateSymbol(kScopeLocal);
        argStMatch->SetNameStrIdx(mirBuilder->GetOrCreateStringIndex("monitor_slot"));
        argStMatch->SetTyIdx(argTy->GetTypeIndex());
        argStMatch->SetStorageClass(kScFormal);
        argStMatch->SetSKind(kStVar);
        func->GetSymTab()->AddToStringSymbolMap(*argStMatch);
        formals.emplace_back(argStMatch);
    }
    func->UpdateFuncTypeAndFormalsAndReturnType(formals, GlobalTables::GetTypeTable().GetVoid()->GetTypeIndex(), false);
    auto *funcType = func->GetMIRFuncType();
    DEBUG_ASSERT(funcType != nullptr, "null ptr check");
    beCommon.AddTypeSizeAndAlign(funcType->GetTypeIndex(), GetPrimTypeSize(funcType->GetPrimType()));

    builtinFuncIDs.emplace_back(std::pair<BuiltinFunctionID, PUIdx>(id, func->GetPuidx()));
    return func;
}

void CGLowerer::RegisterBuiltIns()
{
    for (uint32 i = 0; i < sizeof(cgBuiltins) / sizeof(cgBuiltins[0]); ++i) {
        BuiltinFunctionID id = cgBuiltins[i].first;
        IntrinDesc &desc = IntrinDesc::intrinTable[id];

        MIRFunction *func = mirBuilder->GetOrCreateFunction(cgBuiltins[i].second,
                                                            GlobalTables::GetTypeTable().GetVoid()->GetTypeIndex());
        beCommon.UpdateTypeTable(*func->GetMIRFuncType());
        func->AllocSymTab();
        MIRSymbol *funcSym = func->GetFuncSymbol();
        funcSym->SetStorageClass(kScExtern);
        funcSym->SetAppearsInCode(true);
        /* return type */
        MIRType *retTy = desc.GetReturnType();
        CHECK_FATAL(retTy != nullptr, "retTy should not be nullptr");
        /* use void* for PTY_dynany */
        if (retTy->GetPrimType() == PTY_dynany) {
            retTy = GlobalTables::GetTypeTable().GetPtr();
        }

        std::vector<MIRSymbol *> formals;
        const std::string params[IntrinDesc::kMaxArgsNum] = {"p0", "p1", "p2", "p3", "p4", "p5"};
        for (uint32 j = 0; j < IntrinDesc::kMaxArgsNum; ++j) {
            MIRType *argTy = desc.GetArgType(j);
            if (argTy == nullptr) {
                break;
            }
            /* use void* for PTY_dynany */
            if (argTy->GetPrimType() == PTY_dynany) {
                argTy = GlobalTables::GetTypeTable().GetPtr();
            }
            MIRSymbol *argSt = func->GetSymTab()->CreateSymbol(kScopeLocal);
            argSt->SetNameStrIdx(mirBuilder->GetOrCreateStringIndex(params[j]));
            argSt->SetTyIdx(argTy->GetTypeIndex());
            argSt->SetStorageClass(kScFormal);
            argSt->SetSKind(kStVar);
            func->GetSymTab()->AddToStringSymbolMap(*argSt);
            formals.emplace_back(argSt);
        }
        func->UpdateFuncTypeAndFormalsAndReturnType(formals, retTy->GetTypeIndex(), false);
        auto *funcType = func->GetMIRFuncType();
        DEBUG_ASSERT(funcType != nullptr, "null ptr check");
        beCommon.AddTypeSizeAndAlign(funcType->GetTypeIndex(), GetPrimTypeSize(funcType->GetPrimType()));

        builtinFuncIDs.emplace_back(std::pair<BuiltinFunctionID, PUIdx>(id, func->GetPuidx()));
    }

    /* register __builtin_sync_enter */
    static_cast<void>(RegisterFunctionVoidStarToVoid(INTRN_FIRST_SYNC_ENTER, "MCC_SyncEnterFast0", "obj"));
    static_cast<void>(RegisterFunctionVoidStarToVoid(INTRN_SECOND_SYNC_ENTER, "MCC_SyncEnterFast1", "obj"));
    static_cast<void>(RegisterFunctionVoidStarToVoid(INTRN_THIRD_SYNC_ENTER, "MCC_SyncEnterFast2", "obj"));
    static_cast<void>(RegisterFunctionVoidStarToVoid(INTRN_FOURTH_SYNC_ENTER, "MCC_SyncEnterFast3", "obj"));
    /* register __builtin_sync_exit */
    static_cast<void>(RegisterFunctionVoidStarToVoid(INTRN_YNC_EXIT, "MCC_SyncExitFast", "obj"));
}

/*
 * From Maple IR Document as of Apr 14, 2017
 * Type Conversion Expression Opcodes
 * Conversions between integer types of different sizes require the cvt opcode.
 * Conversion between signed and unsigned integers of the same size does not
 * require any operation, not even retype.
 * cvt :
 * Convert the operand's value from <from-type> to <to-type>.
 * If the sizes of the two types are the same, the conversion must involve
 * altering the bits.
 * retype:
 * <opnd0> is converted to <prim-type> which has derived type <type> without
 * changing any bits.  The size of <opnd0> and <prim-type> must be the same.
 * <opnd0> may be of aggregate type.
 */
BaseNode *CGLowerer::MergeToCvtType(PrimType dType, PrimType sType, BaseNode &src) const
{
    CHECK_FATAL(IsPrimitiveInteger(dType) || IsPrimitiveFloat(dType),
                "dtype should be primitiveInteger or primitiveFloat");
    CHECK_FATAL(IsPrimitiveInteger(sType) || IsPrimitiveFloat(sType),
                "sType should be primitiveInteger or primitiveFloat");
    /* src i32, dest f32; src i64, dest f64 */
    CHECK_FATAL(
        (IsPrimitiveInteger(sType) && IsPrimitiveFloat(dType) &&
         (GetPrimTypeBitSize(sType) == GetPrimTypeBitSize(dType))) ||
            (IsPrimitiveInteger(sType) && IsPrimitiveInteger(dType)),
        "when sType is primitiveInteger and dType is primitiveFloat, sType's primTypeBitSize must equal dType's,"
        " or both sType and dType should primitiveInteger");

    /* src & dest are both of float type */
    MIRType *toType = GlobalTables::GetTypeTable().GetPrimType(dType);
    MIRType *fromType = GlobalTables::GetTypeTable().GetPrimType(sType);
    if (IsPrimitiveInteger(sType) && IsPrimitiveFloat(dType) &&
        (GetPrimTypeBitSize(sType) == GetPrimTypeBitSize(dType))) {
        return mirBuilder->CreateExprRetype(*toType, *fromType, &src);
    } else if (IsPrimitiveInteger(sType) && IsPrimitiveInteger(dType)) {
        if (GetPrimTypeBitSize(sType) >= GetPrimTypeBitSize(dType)) {
            if (dType == PTY_u1) { /* e.g., type _Bool */
                toType = GlobalTables::GetTypeTable().GetPrimType(PTY_u8);
                return mirBuilder->CreateExprCompare(OP_ne, *toType, *fromType, &src,
                                                     mirBuilder->CreateIntConst(0, sType));
            } else if (GetPrimTypeBitSize(sType) > GetPrimTypeBitSize(dType)) {
                return mirBuilder->CreateExprTypeCvt(OP_cvt, *toType, *fromType, &src);
            } else if (IsSignedInteger(sType) != IsSignedInteger(dType)) {
                return mirBuilder->CreateExprTypeCvt(OP_cvt, *toType, *fromType, &src);
            }
            src.SetPrimType(dType);
            return &src;
            /*
             * Force type cvt here because we currently do not run constant folding
             * or contanst propagation before CG. We may revisit this decision later.
             */
        } else if (GetPrimTypeBitSize(sType) < GetPrimTypeBitSize(dType)) {
            return mirBuilder->CreateExprTypeCvt(OP_cvt, *toType, *fromType, &src);
        } else if (IsConstvalZero(src)) {
            return mirBuilder->CreateIntConst(0, dType);
        }
        CHECK_FATAL(false, "should not run here");
    }
    CHECK_FATAL(false, "should not run here");
}

IreadNode &CGLowerer::GetLenNode(BaseNode &opnd0)
{
    MIRIntConst *arrayHeaderNode = GlobalTables::GetIntConstTable().GetOrCreateIntConst(
        RTSupport::GetRTSupportInstance().GetArrayLengthOffset(),
        *GlobalTables::GetTypeTable().GetTypeFromTyIdx(opnd0.GetPrimType()));
    BaseNode *arrayHeaderCstNode = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(arrayHeaderNode);
    arrayHeaderCstNode->SetPrimType(opnd0.GetPrimType());
    MIRType *addrType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(opnd0.GetPrimType());
    BaseNode *refLenAddr = mirBuilder->CreateExprBinary(OP_add, *addrType, &opnd0, arrayHeaderCstNode);
    MIRType *infoLenType = GlobalTables::GetTypeTable().GetInt32();
    MIRType *ptrType = beCommon.BeGetOrCreatePointerType(*infoLenType);
    IreadNode *lenNode = mirBuilder->CreateExprIread(*infoLenType, *ptrType, 0, refLenAddr);
    return (*lenNode);
}

LabelIdx CGLowerer::GetLabelIdx(MIRFunction &curFunc) const
{
    std::string suffix = std::to_string(curFunc.GetLabelTab()->GetLabelTableSize());
    GStrIdx labelStrIdx = GlobalTables::GetStrTable().GetOrCreateStrIdxFromName("__label_BC_" + suffix);
    LabelIdx labIdx = curFunc.GetLabelTab()->AddLabel(labelStrIdx);
    return labIdx;
}

void CGLowerer::ProcessArrayExpr(BaseNode &expr, BlockNode &blkNode)
{
    bool needProcessArrayExpr = !ShouldOptarray() && mirModule.IsJavaModule();
    if (!needProcessArrayExpr) {
        return;
    }
    /* Array boundary check */
    MIRFunction *curFunc = mirModule.CurFunction();
    auto &arrayNode = static_cast<ArrayNode &>(expr);
    StmtNode *boundaryCheckStmt = nullptr;
    if (arrayNode.GetBoundsCheck()) {
        CHECK_FATAL(arrayNode.GetNopndSize() == kOperandNumBinary, "unexpected nOpnd size");
        BaseNode *opnd0 = arrayNode.GetNopndAt(0);
        if (opnd0->GetOpCode() == OP_iread) {
            PregIdx pregIdx = curFunc->GetPregTab()->CreatePreg(opnd0->GetPrimType());
            RegassignNode *temp = mirBuilder->CreateStmtRegassign(opnd0->GetPrimType(), pregIdx, opnd0);
            blkNode.InsertAfter(blkNode.GetLast(), temp);
            arrayNode.SetNOpndAt(0, mirBuilder->CreateExprRegread(opnd0->GetPrimType(), pregIdx));
        }
        IreadNode &lenNode = GetLenNode(*opnd0);
        PregIdx lenPregIdx = curFunc->GetPregTab()->CreatePreg(lenNode.GetPrimType());
        RegassignNode *lenRegassignNode = mirBuilder->CreateStmtRegassign(lenNode.GetPrimType(), lenPregIdx, &lenNode);
        BaseNode *lenRegreadNode = mirBuilder->CreateExprRegread(PTY_u32, lenPregIdx);

        LabelIdx labIdx = GetLabelIdx(*curFunc);
        LabelNode *labelBC = mirBuilder->CreateStmtLabel(labIdx);
        ;
        BaseNode *cond = mirBuilder->CreateExprCompare(OP_ge, *GlobalTables::GetTypeTable().GetUInt1(),
                                                       *GlobalTables::GetTypeTable().GetUInt32(),
                                                       arrayNode.GetNopndAt(1), lenRegreadNode);
        CondGotoNode *brFalseNode = mirBuilder->CreateStmtCondGoto(cond, OP_brfalse, labIdx);
        MIRFunction *fn = mirBuilder->GetOrCreateFunction("MCC_Array_Boundary_Check", TyIdx(PTY_void));
        fn->GetFuncSymbol()->SetAppearsInCode(true);
        beCommon.UpdateTypeTable(*fn->GetMIRFuncType());
        fn->AllocSymTab();
        MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
        args.emplace_back(arrayNode.GetNopndAt(0));
        args.emplace_back(arrayNode.GetNopndAt(1));
        boundaryCheckStmt = mirBuilder->CreateStmtCall(fn->GetPuidx(), args);
        blkNode.InsertAfter(blkNode.GetLast(), lenRegassignNode);
        blkNode.InsertAfter(blkNode.GetLast(), brFalseNode);
        blkNode.InsertAfter(blkNode.GetLast(), boundaryCheckStmt);
        blkNode.InsertAfter(blkNode.GetLast(), labelBC);
    }
}

BaseNode *CGLowerer::LowerExpr(BaseNode &parent, BaseNode &expr, BlockNode &blkNode)
{
    bool isCvtU1Expr = (expr.GetOpCode() == OP_cvt && expr.GetPrimType() == PTY_u1 &&
                        static_cast<TypeCvtNode &>(expr).FromType() != PTY_u1);
    if (expr.GetPrimType() == PTY_u1) {
        expr.SetPrimType(PTY_u8);
    }
    if (expr.GetOpCode() == OP_intrinsicopwithtype) {
        return LowerIntrinsicopwithtype(parent, static_cast<IntrinsicopNode &>(expr), blkNode);
    }

    if (expr.GetOpCode() == OP_iread && expr.Opnd(0)->GetOpCode() == OP_array) {
        /* iread ptr <* <$MUIDDataDefTabEntry>> 1 (
         *     array 0 ptr <* <[5] <$MUIDDataDefTabEntry>>> (addrof ...
         * ==>
         * intrinsicop a64 MPL_READ_STATIC_OFFSET_TAB (addrof ..
         */
        BaseNode *node = LowerExpr(expr, *expr.Opnd(0), blkNode);
        if (node->GetOpCode() == OP_intrinsicop) {
            auto *binNode = static_cast<IntrinsicopNode *>(node);
            CHECK_FATAL(binNode->GetIntrinsic() == INTRN_MPL_READ_STATIC_OFFSET_TAB, "Something wrong here");
            return binNode;
        } else {
            expr.SetOpnd(node, 0);
        }
    } else {
        for (size_t i = 0; i < expr.NumOpnds(); ++i) {
            expr.SetOpnd(LowerExpr(expr, *expr.Opnd(i), blkNode), i);
        }
    }
    // Convert `cvt u1 xx <expr>` to `ne u8 xx (<expr>, constval xx 0)`
    // No need to convert `cvt u1 u1 <expr>`
    if (isCvtU1Expr) {
        auto &cvtExpr = static_cast<TypeCvtNode &>(expr);
        PrimType fromType = cvtExpr.FromType();
        auto *fromMIRType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(fromType));
        // We use u8 instead of u1 because codegen can't recognize u1
        auto *toMIRType = GlobalTables::GetTypeTable().GetUInt8();
        auto *zero = GlobalTables::GetIntConstTable().GetOrCreateIntConst(0, *fromMIRType);
        auto *converted = mirBuilder->CreateExprCompare(OP_ne, *toMIRType, *fromMIRType, cvtExpr.Opnd(0),
                                                        mirBuilder->CreateConstval(zero));
        return converted;
    }
    switch (expr.GetOpCode()) {
        case OP_array: {
            ProcessArrayExpr(expr, blkNode);
            if (!mirModule.IsCModule()) {
                return LowerArray(static_cast<ArrayNode &>(expr), parent);
            } else {
                return LowerCArray(static_cast<ArrayNode &>(expr));
            }
        }

        case OP_dread:
            return LowerDread(static_cast<DreadNode &>(expr), blkNode);

        case OP_addrof:
            return LowerAddrof(static_cast<AddrofNode &>(expr));

        case OP_iread:
            return LowerIread(static_cast<IreadNode &>(expr));

        case OP_iaddrof:
            return LowerIaddrof(static_cast<IreadNode &>(expr));

        case OP_select:
            if (IsComplexSelect(static_cast<TernaryNode &>(expr))) {
                return LowerComplexSelect(static_cast<TernaryNode &>(expr), parent, blkNode);
            } else if (mirModule.GetFlavor() != kFlavorLmbc) {
                return SplitTernaryNodeResult(static_cast<TernaryNode &>(expr), parent, blkNode);
            } else {
                return &expr;
            }

        case OP_sizeoftype: {
            CHECK(static_cast<SizeoftypeNode &>(expr).GetTyIdx() < beCommon.GetSizeOfTypeSizeTable(),
                  "index out of range in CGLowerer::LowerExpr");
            int64 typeSize = beCommon.GetTypeSize(static_cast<SizeoftypeNode &>(expr).GetTyIdx());
            return mirModule.GetMIRBuilder()->CreateIntConst(typeSize, PTY_u32);
        }

        case OP_fieldsdist: {
            auto &fdNode = static_cast<FieldsDistNode &>(expr);
            CHECK(fdNode.GetTyIdx() < beCommon.GetSizeOfTypeSizeTable(), "index out of range in CGLowerer::LowerExpr");
            MIRType *ty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(fdNode.GetTyIdx());
            CHECK(ty->GetKind() == kTypeClass, "wrong type for FieldsDistNode");
            MIRClassType *classType = static_cast<MIRClassType *>(ty);
            const JClassLayout &layout = beCommon.GetJClassLayout(*classType);
            DEBUG_ASSERT(!layout.empty(), "container should not be empty");
            int32 i1 = fdNode.GetFieldID1() > 0 ? fdNode.GetFieldID1() - 1 : 0;
            int32 i2 = fdNode.GetFieldID2() > 0 ? fdNode.GetFieldID2() - 1 : 0;
            int64 offset = layout[i2].GetOffset() - layout[i1].GetOffset();
            return mirModule.GetMIRBuilder()->CreateIntConst(offset, PTY_u32);
        }

        case OP_intrinsicop:
            if (IsIntrinsicOpHandledAtLowerLevel(static_cast<IntrinsicopNode &>(expr).GetIntrinsic())) {
                return &expr;
            }
            return LowerIntrinsicop(parent, static_cast<IntrinsicopNode &>(expr), blkNode);

        case OP_alloca: {
            GetCurrentFunc()->SetVlaOrAlloca(true);
            return &expr;
        }
        case OP_rem:
            return LowerRem(expr, blkNode);

        case OP_cand:
            expr.SetOpCode(OP_land);
            return SplitBinaryNodeOpnd1(static_cast<BinaryNode &>(expr), blkNode);
        case OP_cior:
            expr.SetOpCode(OP_lior);
            return SplitBinaryNodeOpnd1(static_cast<BinaryNode &>(expr), blkNode);
        case OP_cvt:
        case OP_retype:
        case OP_zext:
        case OP_sext:
            return LowerCastExpr(expr);
        default:
            return &expr;
    }
}

BaseNode *CGLowerer::LowerDread(DreadNode &dread, const BlockNode &block)
{
    /* use PTY_u8 for boolean type in dread/iread */
    if (dread.GetPrimType() == PTY_u1) {
        dread.SetPrimType(PTY_u8);
    }
    return (dread.GetFieldID() == 0 ? LowerDreadToThreadLocal(dread, block) : LowerDreadBitfield(dread));
}

void CGLowerer::LowerRegassign(RegassignNode &regNode, BlockNode &newBlk)
{
    BaseNode *rhsOpnd = regNode.Opnd(0);
    Opcode op = rhsOpnd->GetOpCode();
    if ((op == OP_gcmalloc) || (op == OP_gcpermalloc)) {
        LowerGCMalloc(regNode, static_cast<GCMallocNode &>(*rhsOpnd), newBlk, op == OP_gcpermalloc);
        return;
    } else if ((op == OP_gcmallocjarray) || (op == OP_gcpermallocjarray)) {
        LowerJarrayMalloc(regNode, static_cast<JarrayMallocNode &>(*rhsOpnd), newBlk, op == OP_gcpermallocjarray);
        return;
    } else {
        regNode.SetOpnd(LowerExpr(regNode, *rhsOpnd, newBlk), 0);
        newBlk.AddStatement(&regNode);
    }
}

BaseNode *CGLowerer::ExtractSymbolAddress(const StIdx &stIdx)
{
    auto builder = mirModule.GetMIRBuilder();
    return builder->CreateExprAddrof(0, stIdx);
}

BaseNode *CGLowerer::LowerDreadToThreadLocal(BaseNode &expr, const BlockNode &block)
{
    auto *result = &expr;
    if (expr.GetOpCode() != maple::OP_dread) {
        return result;
    }
    uint32 oldTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    auto dread = static_cast<DreadNode &>(expr);
    StIdx stIdx = dread.GetStIdx();
    if (!stIdx.IsGlobal()) {
        return result;
    }
    MIRSymbol *symbol = GlobalTables::GetGsymTable().GetSymbolFromStidx(stIdx.Idx());

    if (symbol->IsThreadLocal()) {
        //  iread <* u32> 0 (regread u64 %addr)
        auto addr = ExtractSymbolAddress(stIdx);
        auto ptrType = GlobalTables::GetTypeTable().GetOrCreatePointerType(*symbol->GetType());
        auto iread = mirModule.GetMIRBuilder()->CreateExprIread(*symbol->GetType(), *ptrType, dread.GetFieldID(), addr);
        result = iread;
    }
    uint32 newTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    if (newTypeTableSize != oldTypeTableSize) {
        beCommon.AddNewTypeAfterBecommon(oldTypeTableSize, newTypeTableSize);
    }
    return result;
}

StmtNode *CGLowerer::LowerDassignToThreadLocal(StmtNode &stmt, const BlockNode &block)
{
    StmtNode *result = &stmt;
    if (stmt.GetOpCode() != maple::OP_dassign) {
        return result;
    }
    uint32 oldTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    auto dAssign = static_cast<DassignNode &>(stmt);
    StIdx stIdx = dAssign.GetStIdx();
    if (!stIdx.IsGlobal()) {
        return result;
    }
    MIRSymbol *symbol = GlobalTables::GetGsymTable().GetSymbolFromStidx(stIdx.Idx());
    if (symbol->IsThreadLocal()) {
        //  iassign <* u32> 0 (regread u64 %addr, dread u32 $x)
        auto addr = ExtractSymbolAddress(stIdx);
        auto ptrType = GlobalTables::GetTypeTable().GetOrCreatePointerType(*symbol->GetType());
        auto iassign =
            mirModule.GetMIRBuilder()->CreateStmtIassign(*ptrType, dAssign.GetFieldID(), addr, dAssign.GetRHS());
        result = iassign;
    }
    uint32 newTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    if (newTypeTableSize != oldTypeTableSize) {
        beCommon.AddNewTypeAfterBecommon(oldTypeTableSize, newTypeTableSize);
    }
    return result;
}

void CGLowerer::LowerDassign(DassignNode &dsNode, BlockNode &newBlk)
{
    StmtNode *newStmt = nullptr;
    BaseNode *rhs = nullptr;
    Opcode op = dsNode.GetRHS()->GetOpCode();
    if (dsNode.GetFieldID() != 0) {
        newStmt = LowerDassignBitfield(dsNode, newBlk);
    } else if (op == OP_intrinsicop) {
        IntrinsicopNode *intrinNode = static_cast<IntrinsicopNode *>(dsNode.GetRHS());
        MIRType *retType = IntrinDesc::intrinTable[intrinNode->GetIntrinsic()].GetReturnType();
        CHECK_FATAL(retType != nullptr, "retType should not be nullptr");
        if (retType->GetKind() == kTypeStruct) {
            newStmt = LowerIntrinsicopDassign(dsNode, *intrinNode, newBlk);
        } else {
            rhs = LowerExpr(dsNode, *intrinNode, newBlk);
            dsNode.SetRHS(rhs);
            CHECK_FATAL(dsNode.GetRHS() != nullptr, "dsNode->rhs is null in CGLowerer::LowerDassign");
            if (!IsDassignNOP(dsNode)) {
                newStmt = &dsNode;
            }
        }
    } else if ((op == OP_gcmalloc) || (op == OP_gcpermalloc)) {
        LowerGCMalloc(dsNode, static_cast<GCMallocNode &>(*dsNode.GetRHS()), newBlk, op == OP_gcpermalloc);
        return;
    } else if ((op == OP_gcmallocjarray) || (op == OP_gcpermallocjarray)) {
        LowerJarrayMalloc(dsNode, static_cast<JarrayMallocNode &>(*dsNode.GetRHS()), newBlk,
                          op == OP_gcpermallocjarray);
        return;
    } else {
        rhs = LowerExpr(dsNode, *dsNode.GetRHS(), newBlk);
        dsNode.SetRHS(rhs);
        newStmt = &dsNode;
    }

    if (newStmt != nullptr) {
        newBlk.AddStatement(LowerDassignToThreadLocal(*newStmt, newBlk));
    }
}

// Lower stmt Form
// Initial form: decrefreset (addrof ptr %RegX_RXXXX)
// Convert to form: dassign %RegX_RXXXX 0 (constval ref 0)
// Final form: str xzr, [x29,#XX]
void CGLowerer::LowerResetStmt(StmtNode &stmt, BlockNode &block)
{
    UnaryStmtNode &unaryStmtNode = static_cast<UnaryStmtNode &>(stmt);
    AddrofNode *addrofNode = static_cast<AddrofNode *>(unaryStmtNode.GetRHS());
    MIRType &type = *GlobalTables::GetTypeTable().GetPrimType(PTY_ref);
    MIRConst *constVal = GlobalTables::GetIntConstTable().GetOrCreateIntConst(0, type);
    ConstvalNode *exprConst = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>();
    exprConst->SetPrimType(type.GetPrimType());
    exprConst->SetConstVal(constVal);
    DassignNode *dassignNode = mirModule.CurFuncCodeMemPool()->New<DassignNode>();
    dassignNode->SetStIdx(addrofNode->GetStIdx());
    dassignNode->SetRHS(exprConst);
    dassignNode->SetFieldID(addrofNode->GetFieldID());
    block.AddStatement(dassignNode);
}

StmtNode *CGLowerer::LowerIntrinsicopDassign(const DassignNode &dsNode, IntrinsicopNode &intrinNode, BlockNode &newBlk)
{
    for (size_t i = 0; i < intrinNode.GetNumOpnds(); ++i) {
        DEBUG_ASSERT(intrinNode.Opnd(i) != nullptr, "intrinNode.Opnd(i) should not be nullptr");
        intrinNode.SetOpnd(LowerExpr(intrinNode, *intrinNode.Opnd(i), newBlk), i);
    }
    MIRIntrinsicID intrnID = intrinNode.GetIntrinsic();
    IntrinDesc *intrinDesc = &IntrinDesc::intrinTable[intrnID];
    MIRSymbol *st = GlobalTables::GetGsymTable().CreateSymbol(kScopeGlobal);
    const std::string name = intrinDesc->name;
    CHECK_FATAL(intrinDesc->name != nullptr, "intrinDesc's name should not be nullptr");
    st->SetNameStrIdx(name);
    st->SetStorageClass(kScText);
    st->SetSKind(kStFunc);
    MIRFunction *fn = mirModule.GetMemPool()->New<MIRFunction>(&mirModule, st->GetStIdx());
    MapleVector<BaseNode *> &nOpnds = intrinNode.GetNopnd();
    st->SetFunction(fn);
    std::vector<TyIdx> fnTyVec;
    std::vector<TypeAttrs> fnTaVec;
    CHECK_FATAL(intrinDesc->IsJsOp(), "intrinDesc should be JsOp");
    /* setup parameters */
    for (uint32 i = 0; i < nOpnds.size(); ++i) {
        fnTyVec.emplace_back(GlobalTables::GetTypeTable().GetTypeFromTyIdx(PTY_a32)->GetTypeIndex());
        fnTaVec.emplace_back(TypeAttrs());
        BaseNode *addrNode = beCommon.GetAddressOfNode(*nOpnds[i]);
        CHECK_FATAL(addrNode != nullptr, "addrNode should not be nullptr");
        nOpnds[i] = addrNode;
    }
    MIRSymbol *dst = mirModule.CurFunction()->GetLocalOrGlobalSymbol(dsNode.GetStIdx());
    MIRType *ty = dst->GetType();
    MIRType *fnType = beCommon.BeGetOrCreateFunctionType(ty->GetTypeIndex(), fnTyVec, fnTaVec);
    st->SetTyIdx(fnType->GetTypeIndex());
    fn->SetMIRFuncType(static_cast<MIRFuncType *>(fnType));
    fn->SetReturnTyIdx(ty->GetTypeIndex());
    CHECK_FATAL(ty->GetKind() == kTypeStruct, "ty's kind should be struct type");
    CHECK_FATAL(dsNode.GetFieldID() == 0, "dsNode's filedId should equal");
    AddrofNode *addrofNode = mirBuilder->CreateAddrof(*dst, PTY_a32);
    MapleVector<BaseNode *> newOpnd(mirModule.CurFuncCodeMemPoolAllocator()->Adapter());
    newOpnd.emplace_back(addrofNode);
    (void)newOpnd.insert(newOpnd.end(), nOpnds.begin(), nOpnds.end());
    CallNode *callStmt = mirModule.CurFuncCodeMemPool()->New<CallNode>(mirModule, OP_call);
    callStmt->SetPUIdx(st->GetFunction()->GetPuidx());
    callStmt->SetNOpnd(newOpnd);
    return callStmt;
}

/*   From maple_ir/include/d ex2mpl/dexintrinsic.def
 *   JAVA_ARRAY_LENGTH
 *   JAVA_ARRAY_FILL
 *   JAVA_FILL_NEW_ARRAY
 *   JAVA_CHECK_CAST
 *   JAVA_CONST_CLASS
 *   JAVA_INSTANCE_OF
 *   JAVA_MERGE
 *   JAVA_RANDOM
 *   #if DEXHACK
 *   JAVA_PRINTLN
 *   #endif
 *   INTRN_<<name>>
 *   intrinsic
 */
BaseNode *CGLowerer::LowerJavascriptIntrinsicop(IntrinsicopNode &intrinNode, const IntrinDesc &desc)
{
    MIRSymbol *st = GlobalTables::GetGsymTable().CreateSymbol(kScopeGlobal);
    CHECK_FATAL(desc.name != nullptr, "desc's name should not be nullptr");
    const std::string name = desc.name;
    st->SetNameStrIdx(name);
    st->SetStorageClass(kScText);
    st->SetSKind(kStFunc);
    MIRFunction *fn = mirModule.GetMemPool()->New<MIRFunction>(&mirModule, st->GetStIdx());
    MapleVector<BaseNode *> &nOpnds = intrinNode.GetNopnd();
    st->SetFunction(fn);
    std::vector<TyIdx> fnTyVec;
    std::vector<TypeAttrs> fnTaVec;
    CHECK_FATAL(desc.IsJsOp(), "desc should be jsOp");
    /* setup parameters */
    for (uint32 i = 0; i < nOpnds.size(); ++i) {
        fnTyVec.emplace_back(GlobalTables::GetTypeTable().GetTypeFromTyIdx(PTY_a32)->GetTypeIndex());
        fnTaVec.emplace_back(TypeAttrs());
        BaseNode *addrNode = beCommon.GetAddressOfNode(*nOpnds[i]);
        CHECK_FATAL(addrNode != nullptr, "can not get address");
        nOpnds[i] = addrNode;
    }

    MIRType *retType = desc.GetReturnType();
    CHECK_FATAL(retType != nullptr, "retType should not be nullptr");
    if (retType->GetKind() == kTypeStruct) {
        /* create a local symbol and dread it; */
        std::string tmpstr("__ret_struct_tmp_st");
        static uint32 tmpIdx = 0;
        tmpstr += std::to_string(tmpIdx++);
        MIRSymbol *tmpSt = mirBuilder->GetOrCreateDeclInFunc(tmpstr, *retType, *mirModule.CurFunction());
        MIRType *fnType = beCommon.BeGetOrCreateFunctionType(retType->GetTypeIndex(), fnTyVec, fnTaVec);
        st->SetTyIdx(fnType->GetTypeIndex());
        fn->SetMIRFuncType(static_cast<MIRFuncType *>(fnType));
        AddrofNode *addrofNode = mirBuilder->CreateAddrof(*tmpSt, PTY_a32);
        MapleVector<BaseNode *> newOpnd(mirModule.CurFuncCodeMemPoolAllocator()->Adapter());
        newOpnd.emplace_back(addrofNode);
        (void)newOpnd.insert(newOpnd.end(), nOpnds.begin(), nOpnds.end());
        CallNode *callStmt = mirModule.CurFuncCodeMemPool()->New<CallNode>(mirModule, OP_call);
        callStmt->SetPUIdx(st->GetFunction()->GetPuidx());
        callStmt->SetNOpnd(newOpnd);
        currentBlock->AddStatement(callStmt);
        /* return the dread */
        AddrofNode *drRetSt = mirBuilder->CreateDread(*tmpSt, PTY_agg);
        return drRetSt;
    }
    CHECK_FATAL(st->GetStIdx().FullIdx() != 0, "the fullIdx of st's stIdx should not equal 0");
    CallNode *callStmt = static_cast<CallNode *>(mirBuilder->CreateStmtCall(st->GetStIdx().FullIdx(), nOpnds));
    currentBlock->AddStatement(callStmt);
    PrimType promotedPrimType = intrinNode.GetPrimType() == PTY_u1 ? PTY_u32 : intrinNode.GetPrimType();
    BaseNode *drRetSt = mirBuilder->CreateExprRegread(promotedPrimType, -kSregRetval0);
    /*
     * for safty dassign the return value to a register and return the dread to that register
     * to avoid such code:
     * call $__js_int32 (addrof ptr %temp_var_8 0)
     * call $__jsop_getelem (addrof a32 %temp_var_9 0, addrof a32 $arr 0, dread i32 %%retval 0)
     * for many target, the first actual parameter and return value would use R0, which would cause the above
     * case fail
     */
    PregIdx tmpRegIdx = GetCurrentFunc()->GetPregTab()->CreatePreg(promotedPrimType);
    RegassignNode *dstoReg = mirBuilder->CreateStmtRegassign(promotedPrimType, tmpRegIdx, drRetSt);
    currentBlock->AddStatement(dstoReg);
    RegreadNode *outDsNode = mirBuilder->CreateExprRegread(promotedPrimType, tmpRegIdx);
    return outDsNode;
}

StmtNode *CGLowerer::CreateStmtCallWithReturnValue(const IntrinsicopNode &intrinNode, const MIRSymbol &ret, PUIdx bFunc,
                                                   BaseNode *extraInfo) const
{
    MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
    for (size_t i = 0; i < intrinNode.NumOpnds(); ++i) {
        args.emplace_back(intrinNode.Opnd(i));
    }
    if (extraInfo != nullptr) {
        args.emplace_back(extraInfo);
    }
    return mirBuilder->CreateStmtCallAssigned(bFunc, args, &ret, OP_callassigned);
}

StmtNode *CGLowerer::CreateStmtCallWithReturnValue(const IntrinsicopNode &intrinNode, PregIdx retpIdx, PUIdx bFunc,
                                                   BaseNode *extraInfo) const
{
    MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
    for (size_t i = 0; i < intrinNode.NumOpnds(); ++i) {
        args.emplace_back(intrinNode.Opnd(i));
    }
    if (extraInfo != nullptr) {
        args.emplace_back(extraInfo);
    }
    return mirBuilder->CreateStmtCallRegassigned(bFunc, args, retpIdx, OP_callassigned);
}

BaseNode *CGLowerer::LowerIntrinJavaMerge(const BaseNode &parent, IntrinsicopNode &intrinNode)
{
    BaseNode *resNode = &intrinNode;
    CHECK_FATAL(intrinNode.GetNumOpnds() > 0, "invalid JAVA_MERGE intrinsic node");
    BaseNode *candidate = intrinNode.Opnd(0);
    DEBUG_ASSERT(candidate != nullptr, "candidate should not be nullptr");
    resNode = candidate;
    if (parent.GetOpCode() == OP_regassign) {
        PrimType sTyp = resNode->GetPrimType();
        auto &regAssign = static_cast<const RegassignNode &>(parent);
        PrimType pType = GetCurrentFunc()->GetPregTab()->PregFromPregIdx(regAssign.GetRegIdx())->GetPrimType();
        if (sTyp != pType) {
            resNode = MergeToCvtType(pType, sTyp, *resNode);
        }
        return resNode;
    }
    if (parent.GetOpCode() == OP_dassign) {
        auto &dassign = static_cast<const DassignNode &>(parent);
        if (candidate->GetOpCode() == OP_constval) {
            MIRSymbol *dest = GetCurrentFunc()->GetLocalOrGlobalSymbol(dassign.GetStIdx());
            MIRType *toType = dest->GetType();
            PrimType dTyp = toType->GetPrimType();
            PrimType sTyp = resNode->GetPrimType();
            if (dTyp != sTyp) {
                resNode = MergeToCvtType(dTyp, sTyp, *resNode);
            }
            return resNode;
        }
        CHECK_FATAL((candidate->GetOpCode() == OP_dread) || (candidate->GetOpCode() == OP_regread),
                    "candidate's opcode should be OP_dread or OP_regread");
        bool differentLocation =
            (candidate->GetOpCode() == OP_dread)
                ? !IsAccessingTheSameMemoryLocation(dassign, static_cast<DreadNode &>(*candidate))
                : !IsAccessingTheSameMemoryLocation(dassign, static_cast<RegreadNode &>(*candidate), *this);
        if (differentLocation) {
            bool simpleMove = false;
            /* res_node already contains the 0-th operand. */
            for (size_t i = 1; i < intrinNode.GetNumOpnds(); ++i) {
                candidate = intrinNode.Opnd(i);
                DEBUG_ASSERT(candidate != nullptr, "candidate should not be nullptr");
                bool sameLocation =
                    (candidate->GetOpCode() == OP_dread)
                        ? IsAccessingTheSameMemoryLocation(dassign, static_cast<DreadNode &>(*candidate))
                        : IsAccessingTheSameMemoryLocation(dassign, static_cast<RegreadNode &>(*candidate), *this);
                if (sameLocation) {
                    simpleMove = true;
                    resNode = candidate;
                    break;
                }
            }
            if (!simpleMove) {
                /* if source and destination types don't match, insert 'retype' */
                MIRSymbol *dest = GetCurrentFunc()->GetLocalOrGlobalSymbol(dassign.GetStIdx());
                MIRType *toType = dest->GetType();
                PrimType dTyp = toType->GetPrimType();
                CHECK_FATAL((dTyp != PTY_agg) && (dassign.GetFieldID() <= 0),
                            "dType should not be PTY_agg and dassign's filedId <= 0");
                PrimType sType = resNode->GetPrimType();
                if (dTyp != sType) {
                    resNode = MergeToCvtType(dTyp, sType, *resNode);
                }
            }
        }
        return resNode;
    }
    CHECK_FATAL(false, "should not run here");
    return resNode;
}

BaseNode *CGLowerer::LowerIntrinJavaArrayLength(const BaseNode &parent, IntrinsicopNode &intrinNode)
{
    BaseNode *resNode = &intrinNode;
    PUIdx bFunc = GetBuiltinToUse(intrinNode.GetIntrinsic());
    CHECK_FATAL(bFunc != kFuncNotFound, "bFunc should not be kFuncNotFound");
    MIRFunction *biFunc = GlobalTables::GetFunctionTable().GetFunctionFromPuidx(bFunc);

    BaseNode *arrAddr = intrinNode.Opnd(0);
    DEBUG_ASSERT(arrAddr != nullptr, "arrAddr should not be nullptr");
    if (((arrAddr->GetPrimType() == PTY_a64) || (arrAddr->GetPrimType() == PTY_ref)) &&
        ((parent.GetOpCode() == OP_regassign) || (parent.GetOpCode() == OP_dassign) || (parent.GetOpCode() == OP_ge))) {
        MIRType *addrType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(TyIdx(arrAddr->GetPrimType()));
        MIRIntConst *arrayHeaderNode = GlobalTables::GetIntConstTable().GetOrCreateIntConst(
            RTSupport::GetRTSupportInstance().GetArrayLengthOffset(), *addrType);
        BaseNode *arrayHeaderCstNode = mirModule.CurFuncCodeMemPool()->New<ConstvalNode>(arrayHeaderNode);
        arrayHeaderCstNode->SetPrimType(arrAddr->GetPrimType());

        BaseNode *refLenAddr = mirBuilder->CreateExprBinary(OP_add, *addrType, arrAddr, arrayHeaderCstNode);
        MIRType *infoLenType = GlobalTables::GetTypeTable().GetInt32();
        MIRType *ptrType = beCommon.BeGetOrCreatePointerType(*infoLenType);
        resNode = mirBuilder->CreateExprIread(*infoLenType, *ptrType, 0, refLenAddr);
        auto curFunc = mirModule.CurFunction();
        std::string suffix = std::to_string(curFunc->GetLabelTab()->GetLabelTableSize());
        GStrIdx labelStrIdx = GlobalTables::GetStrTable().GetOrCreateStrIdxFromName("__label_nonnull_" + suffix);
        LabelIdx labIdx = curFunc->GetLabelTab()->AddLabel(labelStrIdx);
        LabelNode *labelNonNull = mirBuilder->CreateStmtLabel(labIdx);

        BaseNode *cond = mirBuilder->CreateExprCompare(OP_ne, *GlobalTables::GetTypeTable().GetUInt1(),
                                                       *GlobalTables::GetTypeTable().GetRef(), arrAddr,
                                                       mirBuilder->CreateIntConst(0, PTY_ref));
        CondGotoNode *brtureNode = mirBuilder->CreateStmtCondGoto(cond, OP_brtrue, labIdx);

        MIRFunction *newFunc = mirBuilder->GetOrCreateFunction("MCC_ThrowNullArrayNullPointerException",
                                                               GlobalTables::GetTypeTable().GetVoid()->GetTypeIndex());
        newFunc->GetFuncSymbol()->SetAppearsInCode(true);
        beCommon.UpdateTypeTable(*newFunc->GetMIRFuncType());
        newFunc->AllocSymTab();
        MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
        StmtNode *call = mirBuilder->CreateStmtCallAssigned(newFunc->GetPuidx(), args, nullptr, OP_callassigned);

        currentBlock->AddStatement(brtureNode);
        currentBlock->AppendStatementsFromBlock(*LowerCallAssignedStmt(*call));
        currentBlock->AddStatement(labelNonNull);
        return resNode;
    }

    if (parent.GetOpCode() == OP_regassign) {
        auto &regAssign = static_cast<const RegassignNode &>(parent);
        StmtNode *biCall = CreateStmtCallWithReturnValue(intrinNode, regAssign.GetRegIdx(), bFunc);
        currentBlock->AppendStatementsFromBlock(*LowerCallAssignedStmt(*biCall));
        PrimType pType = GetCurrentFunc()->GetPregTab()->PregFromPregIdx(regAssign.GetRegIdx())->GetPrimType();
        resNode = mirBuilder->CreateExprRegread(pType, regAssign.GetRegIdx());
        return resNode;
    }

    if (parent.GetOpCode() == OP_dassign) {
        auto &dassign = static_cast<const DassignNode &>(parent);
        MIRSymbol *ret = GetCurrentFunc()->GetLocalOrGlobalSymbol(dassign.GetStIdx());
        StmtNode *biCall = CreateStmtCallWithReturnValue(intrinNode, *ret, bFunc);
        currentBlock->AppendStatementsFromBlock(*LowerCallAssignedStmt(*biCall));
        resNode = mirBuilder->CreateExprDread(*biFunc->GetReturnType(), 0, *ret);
        return resNode;
    }
    CHECK_FATAL(false, "should not run here");
    return resNode;
}

BaseNode *CGLowerer::LowerIntrinsicop(const BaseNode &parent, IntrinsicopNode &intrinNode)
{
    BaseNode *resNode = &intrinNode;
    if (intrinNode.GetIntrinsic() == INTRN_JAVA_MERGE) {
        resNode = LowerIntrinJavaMerge(parent, intrinNode);
    } else if (intrinNode.GetIntrinsic() == INTRN_JAVA_ARRAY_LENGTH) {
        resNode = LowerIntrinJavaArrayLength(parent, intrinNode);
    }

    return resNode;
}

void CGLowerer::ProcessClassInfo(MIRType &classType, bool &classInfoFromRt, std::string &classInfo) const
{
    MIRPtrType &ptrType = static_cast<MIRPtrType &>(classType);
    MIRType *pType = ptrType.GetPointedType();
    CHECK_FATAL(pType != nullptr, "Class type not found for INTRN_JAVA_CONST_CLASS");
    MIRType *typeScalar = nullptr;

    if (pType->GetKind() == kTypeScalar) {
        typeScalar = pType;
    } else if (classType.GetKind() == kTypeScalar) {
        typeScalar = &classType;
    }
    if (typeScalar != nullptr) {
        std::string eName(GetPrimTypeJavaName(typeScalar->GetPrimType()));
        classInfo = PRIMITIVECLASSINFO_PREFIX_STR + eName;
    }
    if ((pType->GetKind() == kTypeByName) || (pType->GetKind() == kTypeClass) || (pType->GetKind() == kTypeInterface)) {
        MIRStructType *classTypeSecond = static_cast<MIRStructType *>(pType);
        classInfo = CLASSINFO_PREFIX_STR + classTypeSecond->GetName();
    } else if ((pType->GetKind() == kTypeArray) || (pType->GetKind() == kTypeJArray)) {
        MIRJarrayType *jarrayType = static_cast<MIRJarrayType *>(pType);
        CHECK_FATAL(jarrayType != nullptr, "jarrayType is null in CGLowerer::LowerIntrinsicopWithType");
        std::string baseName = jarrayType->GetJavaName();
        if (jarrayType->IsPrimitiveArray() && (jarrayType->GetDim() <= kThreeDimArray)) {
            classInfo = PRIMITIVECLASSINFO_PREFIX_STR + baseName;
        } else if (arrayNameForLower::kArrayBaseName.find(baseName) != arrayNameForLower::kArrayBaseName.end()) {
            classInfo = CLASSINFO_PREFIX_STR + baseName;
        } else {
            classInfoFromRt = true;
            classInfo = baseName;
        }
    }
}

BaseNode *CGLowerer::GetBaseNodeFromCurFunc(MIRFunction &curFunc, bool isFromJarray)
{
    BaseNode *baseNode = nullptr;
    if (curFunc.IsStatic()) {
        /*
         * it's a static function.
         * pass caller functions's classinfo directly
         */
        std::string callerName = CLASSINFO_PREFIX_STR;
        callerName += mirModule.CurFunction()->GetBaseClassName();
        GStrIdx strIdx = GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(callerName);
        MIRSymbol *callerClassInfoSym = GlobalTables::GetGsymTable().GetSymbolFromStrIdx(strIdx);
        if (callerClassInfoSym == nullptr) {
            if (isFromJarray) {
                MIRType *mType = GlobalTables::GetTypeTable().GetVoidPtr();
                CHECK_FATAL(mType != nullptr, "type is null in CGLowerer::LowerJarrayMalloc");
                callerClassInfoSym = mirBuilder->CreateGlobalDecl(callerName.c_str(), *mType);
                callerClassInfoSym->SetStorageClass(kScExtern);
            } else {
                callerClassInfoSym = GlobalTables::GetGsymTable().CreateSymbol(kScopeGlobal);
                callerClassInfoSym->SetNameStrIdx(strIdx);
                callerClassInfoSym->SetStorageClass(kScGlobal);
                callerClassInfoSym->SetSKind(kStVar);
                /* it must be a local symbol */
                GlobalTables::GetGsymTable().AddToStringSymbolMap(*callerClassInfoSym);
                callerClassInfoSym->SetTyIdx(static_cast<TyIdx>(PTY_ptr));
            }
        }

        baseNode = mirBuilder->CreateExprAddrof(0, *callerClassInfoSym);
    } else {
        /*
         * it's an instance function.
         * pass caller function's this pointer
         */
        CHECK_FATAL(curFunc.GetFormalCount() != 0, "index out of range in CGLowerer::GetBaseNodeFromCurFunc");
        MIRSymbol *formalSt = curFunc.GetFormal(0);
        if (formalSt->IsPreg()) {
            if (isFromJarray) {
                baseNode = mirBuilder->CreateExprRegread(
                    formalSt->GetType()->GetPrimType(),
                    curFunc.GetPregTab()->GetPregIdxFromPregno(formalSt->GetPreg()->GetPregNo()));
            } else {
                CHECK_FATAL(curFunc.GetParamSize() != 0, "index out of range in CGLowerer::GetBaseNodeFromCurFunc");
                baseNode = mirBuilder->CreateExprRegread(
                    (curFunc.GetNthParamType(0))->GetPrimType(),
                    curFunc.GetPregTab()->GetPregIdxFromPregno(formalSt->GetPreg()->GetPregNo()));
            }
        } else {
            baseNode = mirBuilder->CreateExprDread(*formalSt);
        }
    }
    return baseNode;
}

BaseNode *CGLowerer::GetClassInfoExprFromRuntime(const std::string &classInfo)
{
    /*
     * generate runtime call to get class information
     * jclass __mrt_getclass(jobject caller, const char *name)
     * if the calling function is an instance function, it's the calling obj
     * if the calling function is a static function, it's the calling class
     */
    BaseNode *classInfoExpr = nullptr;
    PUIdx getClassFunc = GetBuiltinToUse(INTRN_JAVA_GET_CLASS);
    CHECK_FATAL(getClassFunc != kFuncNotFound, "classfunc is not found");
    /* return jclass */
    MIRType *voidPtrType = GlobalTables::GetTypeTable().GetPtr();
    MIRSymbol *ret0 = CreateNewRetVar(*voidPtrType, kIntrnRetValPrefix);

    BaseNode *arg0 = GetBaseNodeFromCurFunc(*mirModule.CurFunction(), false);
    BaseNode *arg1 = nullptr;
    /* classname */
    std::string klassJavaDescriptor;
    namemangler::DecodeMapleNameToJavaDescriptor(classInfo, klassJavaDescriptor);
    UStrIdx classNameStrIdx = GlobalTables::GetUStrTable().GetOrCreateStrIdxFromName(klassJavaDescriptor);
    arg1 = mirModule.GetMemPool()->New<ConststrNode>(classNameStrIdx);
    arg1->SetPrimType(PTY_ptr);

    MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
    args.emplace_back(arg0);
    args.emplace_back(arg1);
    StmtNode *getClassCall = mirBuilder->CreateStmtCallAssigned(getClassFunc, args, ret0, OP_callassigned);
    currentBlock->AppendStatementsFromBlock(*LowerCallAssignedStmt(*getClassCall));
    classInfoExpr = mirBuilder->CreateExprDread(*voidPtrType, 0, *ret0);
    return classInfoExpr;
}

BaseNode *CGLowerer::GetClassInfoExprFromArrayClassCache(const std::string &classInfo)
{
    std::string klassJavaDescriptor;
    namemangler::DecodeMapleNameToJavaDescriptor(classInfo, klassJavaDescriptor);
    if (arrayClassCacheIndex.find(klassJavaDescriptor) == arrayClassCacheIndex.end()) {
        return nullptr;
    }
    GStrIdx strIdx = GlobalTables::GetStrTable().GetStrIdxFromName(namemangler::kArrayClassCacheTable +
                                                                   mirModule.GetFileNameAsPostfix());
    MIRSymbol *arrayClassSt = GlobalTables::GetGsymTable().GetSymbolFromStrIdx(strIdx);
    if (arrayClassSt == nullptr) {
        return nullptr;
    }
    auto index = arrayClassCacheIndex[klassJavaDescriptor];
#ifdef USE_32BIT_REF
    const int32 width = 4;
#else
    const int32 width = 8;
#endif /* USE_32BIT_REF */
    int64 offset = static_cast<int64>(index) * width;
    ConstvalNode *offsetExpr = mirBuilder->CreateIntConst(offset, PTY_u32);
    AddrofNode *baseExpr = mirBuilder->CreateExprAddrof(0, *arrayClassSt, mirModule.GetMemPool());
    MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
    args.emplace_back(baseExpr);
    args.emplace_back(offsetExpr);
    return mirBuilder->CreateExprIntrinsicop(INTRN_MPL_READ_ARRAYCLASS_CACHE_ENTRY, OP_intrinsicop,
                                             *GlobalTables::GetTypeTable().GetPrimType(PTY_ref), args);
}

BaseNode *CGLowerer::GetClassInfoExpr(const std::string &classInfo) const
{
    BaseNode *classInfoExpr = nullptr;
    GStrIdx strIdx = GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(classInfo);
    MIRSymbol *classInfoSym = GlobalTables::GetGsymTable().GetSymbolFromStrIdx(strIdx);
    if (classInfoSym != nullptr) {
        classInfoExpr = mirBuilder->CreateExprAddrof(0, *classInfoSym);
    } else {
        classInfoSym = GlobalTables::GetGsymTable().CreateSymbol(kScopeGlobal);
        classInfoSym->SetNameStrIdx(strIdx);
        classInfoSym->SetStorageClass(kScGlobal);
        classInfoSym->SetSKind(kStVar);
        if (CGOptions::IsPIC()) {
            classInfoSym->SetStorageClass(kScExtern);
        } else {
            classInfoSym->SetAttr(ATTR_weak);
        }
        GlobalTables::GetGsymTable().AddToStringSymbolMap(*classInfoSym);
        classInfoSym->SetTyIdx(static_cast<TyIdx>(PTY_ptr));

        classInfoExpr = mirBuilder->CreateExprAddrof(0, *classInfoSym);
    }
    return classInfoExpr;
}

BaseNode *CGLowerer::LowerIntrinsicopWithType(const BaseNode &parent, IntrinsicopNode &intrinNode)
{
    BaseNode *resNode = &intrinNode;
    if ((intrinNode.GetIntrinsic() == INTRN_JAVA_CONST_CLASS) ||
        (intrinNode.GetIntrinsic() == INTRN_JAVA_INSTANCE_OF)) {
        PUIdx bFunc = GetBuiltinToUse(intrinNode.GetIntrinsic());
        CHECK_FATAL(bFunc != kFuncNotFound, "bFunc not founded");
        MIRFunction *biFunc = GlobalTables::GetFunctionTable().GetFunctionFromPuidx(bFunc);
        MIRType *classType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(intrinNode.GetTyIdx());
        std::string classInfo;
        BaseNode *classInfoExpr = nullptr;
        bool classInfoFromRt = false; /* whether the classinfo is generated by RT */
        ProcessClassInfo(*classType, classInfoFromRt, classInfo);
        if (classInfoFromRt) {
            classInfoExpr = GetClassInfoExprFromArrayClassCache(classInfo);
            if (classInfoExpr == nullptr) {
                classInfoExpr = GetClassInfoExprFromRuntime(classInfo);
            }
        } else {
            classInfoExpr = GetClassInfoExpr(classInfo);
        }

        if (intrinNode.GetIntrinsic() == INTRN_JAVA_CONST_CLASS) {
            CHECK_FATAL(classInfoExpr != nullptr, "classInfoExpr should not be nullptr");
            resNode = classInfoExpr;
            return resNode;
        }

        if (parent.GetOpCode() == OP_regassign) {
            auto &regAssign = static_cast<const RegassignNode &>(parent);
            StmtNode *biCall = CreateStmtCallWithReturnValue(intrinNode, regAssign.GetRegIdx(), bFunc, classInfoExpr);
            currentBlock->AppendStatementsFromBlock(*LowerCallAssignedStmt(*biCall));
            PrimType pTyp = GetCurrentFunc()->GetPregTab()->PregFromPregIdx(regAssign.GetRegIdx())->GetPrimType();
            resNode = mirBuilder->CreateExprRegread(pTyp, regAssign.GetRegIdx());
            return resNode;
        }

        if (parent.GetOpCode() == OP_dassign) {
            auto &dassign = static_cast<const DassignNode &>(parent);
            MIRSymbol *ret = GetCurrentFunc()->GetLocalOrGlobalSymbol(dassign.GetStIdx());
            StmtNode *biCall = CreateStmtCallWithReturnValue(intrinNode, *ret, bFunc, classInfoExpr);
            currentBlock->AppendStatementsFromBlock(*LowerCallAssignedStmt(*biCall));
            resNode = mirBuilder->CreateExprDread(*biFunc->GetReturnType(), 0, *ret);
            return resNode;
        }
        CHECK_FATAL(false, "should not run here");
    }
    CHECK_FATAL(false, "should not run here");
    return resNode;
}

BaseNode *CGLowerer::LowerIntrinsicop(const BaseNode &parent, IntrinsicopNode &intrinNode, BlockNode &newBlk)
{
    for (size_t i = 0; i < intrinNode.GetNumOpnds(); ++i) {
        intrinNode.SetOpnd(LowerExpr(intrinNode, *intrinNode.Opnd(i), newBlk), i);
    }

    MIRIntrinsicID intrnID = intrinNode.GetIntrinsic();
    IntrinDesc &intrinDesc = IntrinDesc::intrinTable[intrnID];
    if (intrinDesc.IsJS()) {
        return LowerJavascriptIntrinsicop(intrinNode, intrinDesc);
    }
    if (intrinDesc.IsJava()) {
        return LowerIntrinsicop(parent, intrinNode);
    }
    if (intrinNode.GetIntrinsic() == INTRN_MPL_READ_OVTABLE_ENTRY_LAZY) {
        return &intrinNode;
    }
    if (intrinNode.GetIntrinsic() == INTRN_MPL_READ_ARRAYCLASS_CACHE_ENTRY) {
        return &intrinNode;
    }
    if (intrnID == INTRN_C_constant_p) {
        BaseNode *opnd = intrinNode.Opnd(0);
        int64 val = (opnd->op == OP_constval || opnd->op == OP_sizeoftype || opnd->op == OP_conststr ||
                     opnd->op == OP_conststr16)
                        ? 1
                        : 0;
        return mirModule.GetMIRBuilder()->CreateIntConst(val, PTY_i32);
    }
    if (intrnID == INTRN_C___builtin_expect) {
        return intrinNode.Opnd(0);
    }
    if (intrinDesc.IsVectorOp() || intrinDesc.IsAtomic()) {
        return &intrinNode;
    }
    CHECK_FATAL(false, "unexpected intrinsic type in CGLowerer::LowerIntrinsicop");
    return &intrinNode;
}

BaseNode *CGLowerer::LowerIntrinsicopwithtype(const BaseNode &parent, IntrinsicopNode &intrinNode, BlockNode &blk)
{
    for (size_t i = 0; i < intrinNode.GetNumOpnds(); ++i) {
        intrinNode.SetOpnd(LowerExpr(intrinNode, *intrinNode.Opnd(i), blk), i);
    }
    MIRIntrinsicID intrnID = intrinNode.GetIntrinsic();
    IntrinDesc *intrinDesc = &IntrinDesc::intrinTable[intrnID];
    CHECK_FATAL(!intrinDesc->IsJS(), "intrinDesc should not be js");
    if (intrinDesc->IsJava()) {
        return LowerIntrinsicopWithType(parent, intrinNode);
    }
    CHECK_FATAL(false, "should not run here");
    return &intrinNode;
}

StmtNode *CGLowerer::LowerIntrinsicMplClearStack(const IntrinsiccallNode &intrincall, BlockNode &newBlk)
{
    StmtNode *newStmt =
        mirBuilder->CreateStmtIassign(*beCommon.BeGetOrCreatePointerType(*GlobalTables::GetTypeTable().GetUInt8()), 0,
                                      intrincall.Opnd(0), mirBuilder->GetConstUInt8(0));
    newBlk.AddStatement(newStmt);

    BaseNode *length = intrincall.Opnd(1);
    PrimType pType = PTY_i64;
    PregIdx pIdx = GetCurrentFunc()->GetPregTab()->CreatePreg(pType);
    newStmt = mirBuilder->CreateStmtRegassign(pType, pIdx, mirBuilder->CreateIntConst(1, pType));
    newBlk.AddStatement(newStmt);
    MIRFunction *func = GetCurrentFunc();

    const std::string &name = func->GetName() + std::string("_Lalloca_");
    LabelIdx label1 = GetCurrentFunc()->GetOrCreateLableIdxFromName(name + std::to_string(labelIdx++));
    LabelIdx label2 = GetCurrentFunc()->GetOrCreateLableIdxFromName(name + std::to_string(labelIdx++));

    newStmt = mirBuilder->CreateStmtGoto(OP_goto, label2);
    newBlk.AddStatement(newStmt);
    LabelNode *ln = mirBuilder->CreateStmtLabel(label1);
    newBlk.AddStatement(ln);

    RegreadNode *regLen = mirBuilder->CreateExprRegread(pType, pIdx);

    BinaryNode *addr =
        mirBuilder->CreateExprBinary(OP_add, *GlobalTables::GetTypeTable().GetAddr64(), intrincall.Opnd(0), regLen);

    newStmt =
        mirBuilder->CreateStmtIassign(*beCommon.BeGetOrCreatePointerType(*GlobalTables::GetTypeTable().GetUInt8()), 0,
                                      addr, mirBuilder->GetConstUInt8(0));
    newBlk.AddStatement(newStmt);

    BinaryNode *subLen = mirBuilder->CreateExprBinary(OP_add, *GlobalTables::GetTypeTable().GetPrimType(pType), regLen,
                                                      mirBuilder->CreateIntConst(1, pType));
    newStmt = mirBuilder->CreateStmtRegassign(pType, pIdx, subLen);
    newBlk.AddStatement(newStmt);

    ln = mirBuilder->CreateStmtLabel(label2);
    newBlk.AddStatement(ln);

    CompareNode *cmpExp =
        mirBuilder->CreateExprCompare(OP_lt, *GlobalTables::GetTypeTable().GetUInt32(),
                                      *GlobalTables::GetTypeTable().GetPrimType(pType), regLen, length);
    newStmt = mirBuilder->CreateStmtCondGoto(cmpExp, OP_brtrue, label1);

    return newStmt;
}

StmtNode *CGLowerer::LowerIntrinsicRCCall(const IntrinsiccallNode &intrincall)
{
    /* If GCONLY enabled, lowering RC intrinsics in another way. */
    MIRIntrinsicID intrnID = intrincall.GetIntrinsic();
    IntrinDesc *intrinDesc = &IntrinDesc::intrinTable[intrnID];

    /* convert intrinsic call into function call. */
    if (intrinFuncIDs.find(intrinDesc) == intrinFuncIDs.end()) {
        /* add funcid into map */
        MIRFunction *fn = mirBuilder->GetOrCreateFunction(intrinDesc->name, TyIdx(PTY_void));
        fn->GetFuncSymbol()->SetAppearsInCode(true);
        beCommon.UpdateTypeTable(*fn->GetMIRFuncType());
        fn->AllocSymTab();
        intrinFuncIDs[intrinDesc] = fn->GetPuidx();
    }
    CallNode *callStmt = mirModule.CurFuncCodeMemPool()->New<CallNode>(mirModule, OP_call);
    callStmt->SetPUIdx(intrinFuncIDs.at(intrinDesc));
    for (size_t i = 0; i < intrincall.GetNopndSize(); ++i) {
        callStmt->GetNopnd().emplace_back(intrincall.GetNopndAt(i));
        callStmt->SetNumOpnds(callStmt->GetNumOpnds() + 1);
    }
    return callStmt;
}

void CGLowerer::LowerArrayStore(const IntrinsiccallNode &intrincall, BlockNode &newBlk)
{
    bool needCheckStore = true;
    BaseNode *arrayNode = intrincall.Opnd(0);
    MIRType *arrayElemType = GetArrayNodeType(*arrayNode);
    BaseNode *valueNode = intrincall.Opnd(kNodeThirdOpnd);
    MIRType *valueRealType = GetArrayNodeType(*valueNode);
    if ((arrayElemType != nullptr) && (valueRealType != nullptr) && (arrayElemType->GetKind() == kTypeClass) &&
        static_cast<MIRClassType *>(arrayElemType)->IsFinal() && (valueRealType->GetKind() == kTypeClass) &&
        static_cast<MIRClassType *>(valueRealType)->IsFinal() &&
        (valueRealType->GetTypeIndex() == arrayElemType->GetTypeIndex())) {
        needCheckStore = false;
    }

    if (needCheckStore) {
        MIRFunction *fn = mirBuilder->GetOrCreateFunction("MCC_Reflect_Check_Arraystore", TyIdx(PTY_void));
        fn->GetFuncSymbol()->SetAppearsInCode(true);
        beCommon.UpdateTypeTable(*fn->GetMIRFuncType());
        fn->AllocSymTab();
        MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
        args.emplace_back(intrincall.Opnd(0));
        args.emplace_back(intrincall.Opnd(kNodeThirdOpnd));
        StmtNode *checkStoreStmt = mirBuilder->CreateStmtCall(fn->GetPuidx(), args);
        newBlk.AddStatement(checkStoreStmt);
    }
}

StmtNode *CGLowerer::LowerDefaultIntrinsicCall(IntrinsiccallNode &intrincall, MIRSymbol &st, MIRFunction &fn)
{
    MIRIntrinsicID intrnID = intrincall.GetIntrinsic();
    IntrinDesc *intrinDesc = &IntrinDesc::intrinTable[intrnID];
    std::vector<TyIdx> funcTyVec;
    std::vector<TypeAttrs> fnTaVec;
    MapleVector<BaseNode *> &nOpnds = intrincall.GetNopnd();
    MIRType *retTy = intrinDesc->GetReturnType();
    CHECK_FATAL(retTy != nullptr, "retTy should not be nullptr");
    if (retTy->GetKind() == kTypeStruct) {
        funcTyVec.emplace_back(beCommon.BeGetOrCreatePointerType(*retTy)->GetTypeIndex());
        fnTaVec.emplace_back(TypeAttrs());
        fn.SetReturnStruct();
    }
    for (uint32 i = 0; i < nOpnds.size(); ++i) {
        MIRType *argTy = intrinDesc->GetArgType(i);
        CHECK_FATAL(argTy != nullptr, "argTy should not be nullptr");
        if (argTy->GetKind() == kTypeStruct) {
            funcTyVec.emplace_back(GlobalTables::GetTypeTable().GetTypeFromTyIdx(PTY_a32)->GetTypeIndex());
            fnTaVec.emplace_back(TypeAttrs());
            BaseNode *addrNode = beCommon.GetAddressOfNode(*nOpnds[i]);
            CHECK_FATAL(addrNode != nullptr, "can not get address");
            nOpnds[i] = addrNode;
        } else {
            funcTyVec.emplace_back(argTy->GetTypeIndex());
            fnTaVec.emplace_back(TypeAttrs());
        }
    }
    MIRType *funcType = beCommon.BeGetOrCreateFunctionType(retTy->GetTypeIndex(), funcTyVec, fnTaVec);
    st.SetTyIdx(funcType->GetTypeIndex());
    fn.SetMIRFuncType(static_cast<MIRFuncType *>(funcType));
    if (retTy->GetKind() == kTypeStruct) {
        fn.SetReturnTyIdx(static_cast<TyIdx>(PTY_void));
    } else {
        fn.SetReturnTyIdx(retTy->GetTypeIndex());
    }
    return static_cast<CallNode *>(mirBuilder->CreateStmtCall(fn.GetPuidx(), nOpnds));
}

StmtNode *CGLowerer::LowerIntrinsicMplCleanupLocalRefVarsSkip(IntrinsiccallNode &intrincall)
{
    MIRFunction *mirFunc = mirModule.CurFunction();
    BaseNode *skipExpr = intrincall.Opnd(intrincall.NumOpnds() - 1);

    CHECK_FATAL(skipExpr != nullptr, "should be dread");
    CHECK_FATAL(skipExpr->GetOpCode() == OP_dread, "should be dread");
    DreadNode *refNode = static_cast<DreadNode *>(skipExpr);
    MIRSymbol *skipSym = mirFunc->GetLocalOrGlobalSymbol(refNode->GetStIdx());
    if (skipSym->GetAttr(ATTR_localrefvar)) {
        mirFunc->InsertMIRSymbol(skipSym);
    }
    return &intrincall;
}

StmtNode *CGLowerer::LowerIntrinsiccall(IntrinsiccallNode &intrincall, BlockNode &newBlk)
{
    MIRIntrinsicID intrnID = intrincall.GetIntrinsic();
    for (size_t i = 0; i < intrincall.GetNumOpnds(); ++i) {
        intrincall.SetOpnd(LowerExpr(intrincall, *intrincall.Opnd(i), newBlk), i);
    }
    if (intrnID == INTRN_MPL_CLEAR_STACK) {
        return LowerIntrinsicMplClearStack(intrincall, newBlk);
    }
    if (intrnID == INTRN_C_va_start) {
        return &intrincall;
    }
    IntrinDesc *intrinDesc = &IntrinDesc::intrinTable[intrnID];
    if (intrinDesc->IsSpecial() || intrinDesc->IsAtomic()) {
        /* For special intrinsics we leave them to CGFunc::SelectIntrinCall() */
        return &intrincall;
    }
    /* default lowers intrinsic call to real function call. */
    MIRSymbol *st = GlobalTables::GetGsymTable().CreateSymbol(kScopeGlobal);
    CHECK_FATAL(intrinDesc->name != nullptr, "intrinsic's name should not be nullptr");
    const std::string name = intrinDesc->name;
    st->SetNameStrIdx(name);
    st->SetStorageClass(kScText);
    st->SetSKind(kStFunc);
    MIRFunction *fn = mirBuilder->GetOrCreateFunction(intrinDesc->name, TyIdx(0));
    beCommon.UpdateTypeTable(*fn->GetMIRFuncType());
    fn->AllocSymTab();
    st->SetFunction(fn);
    st->SetAppearsInCode(true);
    return LowerDefaultIntrinsicCall(intrincall, *st, *fn);
}

StmtNode *CGLowerer::LowerSyncEnterSyncExit(StmtNode &stmt)
{
    CHECK_FATAL(stmt.GetOpCode() == OP_syncenter || stmt.GetOpCode() == OP_syncexit,
                "stmt's opcode should be OP_syncenter or OP_syncexit");

    auto &nStmt = static_cast<NaryStmtNode &>(stmt);
    BuiltinFunctionID id;
    if (nStmt.GetOpCode() == OP_syncenter) {
        if (nStmt.NumOpnds() == 1) {
            /* Just as ParseNaryStmt do for syncenter */
            MIRType &intType = *GlobalTables::GetTypeTable().GetTypeFromTyIdx(static_cast<TyIdx>(PTY_i32));
            /* default 2 for __sync_enter_fast() */
            MIRIntConst *intConst = GlobalTables::GetIntConstTable().GetOrCreateIntConst(2, intType);
            ConstvalNode *exprConst = mirModule.GetMemPool()->New<ConstvalNode>();
            exprConst->SetPrimType(PTY_i32);
            exprConst->SetConstVal(intConst);
            nStmt.GetNopnd().emplace_back(exprConst);
            nStmt.SetNumOpnds(nStmt.GetNopndSize());
        }
        CHECK_FATAL(nStmt.NumOpnds() == kOperandNumBinary, "wrong args for syncenter");
        CHECK_FATAL(nStmt.Opnd(1)->GetOpCode() == OP_constval, "wrong 2nd arg type for syncenter");
        ConstvalNode *cst = static_cast<ConstvalNode *>(nStmt.GetNopndAt(1));
        MIRIntConst *intConst = safe_cast<MIRIntConst>(cst->GetConstVal());
        switch (intConst->GetExtValue()) {
            case kMCCSyncEnterFast0:
                id = INTRN_FIRST_SYNC_ENTER;
                break;
            case kMCCSyncEnterFast1:
                id = INTRN_SECOND_SYNC_ENTER;
                break;
            case kMCCSyncEnterFast2:
                id = INTRN_THIRD_SYNC_ENTER;
                break;
            case kMCCSyncEnterFast3:
                id = INTRN_FOURTH_SYNC_ENTER;
                break;
            default:
                CHECK_FATAL(false, "wrong kind for syncenter");
                break;
        }
    } else {
        CHECK_FATAL(nStmt.NumOpnds() == 1, "wrong args for syncexit");
        id = INTRN_YNC_EXIT;
    }
    PUIdx bFunc = GetBuiltinToUse(id);
    CHECK_FATAL(bFunc != kFuncNotFound, "bFunc should be found");

    MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
    args.emplace_back(nStmt.Opnd(0));
    return mirBuilder->CreateStmtCall(bFunc, args);
}

PUIdx CGLowerer::GetBuiltinToUse(BuiltinFunctionID id) const
{
    /*
     * use std::vector & linear search as the number of entries is small.
     * we may revisit it if the number of entries gets larger.
     */
    for (const auto &funcID : builtinFuncIDs) {
        if (funcID.first == id) {
            return funcID.second;
        }
    }
    return kFuncNotFound;
}

void CGLowerer::LowerGCMalloc(const BaseNode &node, const GCMallocNode &gcmalloc, BlockNode &blkNode, bool perm)
{
    MIRFunction *func =
        mirBuilder->GetOrCreateFunction((perm ? "MCC_NewPermanentObject" : "MCC_NewObj_fixed_class"), (TyIdx)(PTY_ref));
    func->GetFuncSymbol()->SetAppearsInCode(true);
    beCommon.UpdateTypeTable(*func->GetMIRFuncType());
    func->AllocSymTab();
    /* Get the classinfo */
    MIRStructType *classType =
        static_cast<MIRStructType *>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(gcmalloc.GetTyIdx()));
    std::string classInfoName = CLASSINFO_PREFIX_STR + classType->GetName();
    MIRSymbol *classSym =
        GlobalTables::GetGsymTable().GetSymbolFromStrIdx(GlobalTables::GetStrTable().GetStrIdxFromName(classInfoName));
    if (classSym == nullptr) {
        MIRType *pointerType = beCommon.BeGetOrCreatePointerType(*GlobalTables::GetTypeTable().GetVoid());
        classSym = mirBuilder->CreateGlobalDecl(classInfoName, *pointerType);
        classSym->SetStorageClass(kScExtern);
    }
    CallNode *callAssign = nullptr;
    auto *curFunc = mirModule.CurFunction();
    if (classSym->GetAttr(ATTR_abstract) || classSym->GetAttr(ATTR_interface)) {
        MIRFunction *funcSecond =
            mirBuilder->GetOrCreateFunction("MCC_Reflect_ThrowInstantiationError", static_cast<TyIdx>(PTY_ref));
        funcSecond->GetFuncSymbol()->SetAppearsInCode(true);
        beCommon.UpdateTypeTable(*funcSecond->GetMIRFuncType());
        funcSecond->AllocSymTab();
        BaseNode *arg = mirBuilder->CreateExprAddrof(0, *classSym);
        if (node.GetOpCode() == OP_dassign) {
            auto &dsNode = static_cast<const DassignNode &>(node);
            MIRSymbol *ret = curFunc->GetLocalOrGlobalSymbol(dsNode.GetStIdx());
            MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
            args.emplace_back(arg);
            callAssign = mirBuilder->CreateStmtCallAssigned(funcSecond->GetPuidx(), args, ret, OP_callassigned);
        } else {
            CHECK_FATAL(node.GetOpCode() == OP_regassign, "regassign expected");
            callAssign = mirBuilder->CreateStmtCallRegassigned(
                funcSecond->GetPuidx(), static_cast<const RegassignNode &>(node).GetRegIdx(), OP_callassigned, arg);
        }
        blkNode.AppendStatementsFromBlock(*LowerCallAssignedStmt(*callAssign));
        return;
    }
    BaseNode *arg = mirBuilder->CreateExprAddrof(0, *classSym);

    if (node.GetOpCode() == OP_dassign) {
        MIRSymbol *ret = curFunc->GetLocalOrGlobalSymbol(static_cast<const DassignNode &>(node).GetStIdx());
        MapleVector<BaseNode *> args(mirBuilder->GetCurrentFuncCodeMpAllocator()->Adapter());
        args.emplace_back(arg);
        callAssign = mirBuilder->CreateStmtCallAssigned(func->GetPuidx(), args, ret, OP_callassigned);
    } else {
        CHECK_FATAL(node.GetOpCode() == OP_regassign, "regassign expected");
        callAssign = mirBuilder->CreateStmtCallRegassigned(
            func->GetPuidx(), static_cast<const RegassignNode &>(node).GetRegIdx(), OP_callassigned, arg);
    }
    blkNode.AppendStatementsFromBlock(*LowerCallAssignedStmt(*callAssign));
}

std::string CGLowerer::GetNewArrayFuncName(const uint32 elemSize, const bool perm) const
{
    if (elemSize == k1ByteSize) {
        return perm ? "MCC_NewPermArray8" : "MCC_NewArray8";
    }
    if (elemSize == k2ByteSize) {
        return perm ? "MCC_NewPermArray16" : "MCC_NewArray16";
    }
    if (elemSize == k4ByteSize) {
        return perm ? "MCC_NewPermArray32" : "MCC_NewArray32";
    }
    CHECK_FATAL((elemSize == k8ByteSize), "Invalid elemSize.");
    return perm ? "MCC_NewPermArray64" : "MCC_NewArray64";
}

void CGLowerer::LowerJarrayMalloc(const StmtNode &stmt, const JarrayMallocNode &node, BlockNode &blkNode, bool perm)
{
    /* Extract jarray type */
    TyIdx tyIdx = node.GetTyIdx();
    MIRType *type = GlobalTables::GetTypeTable().GetTypeFromTyIdx(tyIdx);
    CHECK_FATAL(type->GetKind() == kTypeJArray, "Type param of gcmallocjarray is not a MIRJarrayType");
    auto jaryType = static_cast<MIRJarrayType *>(type);
    CHECK_FATAL(jaryType != nullptr, "Type param of gcmallocjarray is not a MIRJarrayType");

    /* Inspect element type */
    MIRType *elemType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(jaryType->GetElemTyIdx());
    PrimType elemPrimType = elemType->GetPrimType();
    uint32 elemSize = GetPrimTypeSize(elemPrimType);
    if (elemType->GetKind() != kTypeScalar) { /* element is reference */
        elemSize = static_cast<uint32>(RTSupport::GetRTSupportInstance().GetFieldSize());
    }

    std::string klassName = jaryType->GetJavaName();
    std::string arrayClassInfoName;
    bool isPredefinedArrayClass = false;
    BaseNode *arrayCacheNode = nullptr;
    if (jaryType->IsPrimitiveArray() && (jaryType->GetDim() <= kThreeDimArray)) {
        arrayClassInfoName = PRIMITIVECLASSINFO_PREFIX_STR + klassName;
        isPredefinedArrayClass = true;
    } else if (arrayNameForLower::kArrayKlassName.find(klassName) != arrayNameForLower::kArrayKlassName.end()) {
        arrayClassInfoName = CLASSINFO_PREFIX_STR + klassName;
        isPredefinedArrayClass = true;
    } else {
        arrayCacheNode = GetClassInfoExprFromArrayClassCache(klassName);
    }

    std::string funcName;
    MapleVector<BaseNode *> args(mirModule.GetMPAllocator().Adapter());
    auto *curFunc = mirModule.CurFunction();
    if (isPredefinedArrayClass || (arrayCacheNode != nullptr)) {
        funcName = GetNewArrayFuncName(elemSize, perm);
        args.emplace_back(node.Opnd(0)); /* n_elems */
        if (isPredefinedArrayClass) {
            GStrIdx strIdx = GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(arrayClassInfoName);
            MIRSymbol *arrayClassSym = GlobalTables::GetGsymTable().GetSymbolFromStrIdx(
                GlobalTables::GetStrTable().GetStrIdxFromName(arrayClassInfoName));
            if (arrayClassSym == nullptr) {
                arrayClassSym = GlobalTables::GetGsymTable().CreateSymbol(kScopeGlobal);
                arrayClassSym->SetNameStrIdx(strIdx);
                arrayClassSym->SetStorageClass(kScGlobal);
                arrayClassSym->SetSKind(kStVar);
                if (CGOptions::IsPIC()) {
                    arrayClassSym->SetStorageClass(kScExtern);
                } else {
                    arrayClassSym->SetAttr(ATTR_weak);
                }
                GlobalTables::GetGsymTable().AddToStringSymbolMap(*arrayClassSym);
                arrayClassSym->SetTyIdx(static_cast<TyIdx>(PTY_ptr));
            }
            args.emplace_back(mirBuilder->CreateExprAddrof(0, *arrayClassSym));
        } else {
            args.emplace_back(arrayCacheNode);
        }
    } else {
        funcName = perm ? "MCC_NewPermanentArray" : "MCC_NewObj_flexible_cname";
        args.emplace_back(mirBuilder->CreateIntConst(elemSize, PTY_u32)); /* elem_size */
        args.emplace_back(node.Opnd(0));                                  /* n_elems */
        std::string klassJavaDescriptor;
        namemangler::DecodeMapleNameToJavaDescriptor(klassName, klassJavaDescriptor);
        UStrIdx classNameStrIdx = GlobalTables::GetUStrTable().GetOrCreateStrIdxFromName(klassJavaDescriptor);
        ConststrNode *classNameExpr = mirModule.GetMemPool()->New<ConststrNode>(classNameStrIdx);
        classNameExpr->SetPrimType(PTY_ptr);
        args.emplace_back(classNameExpr); /* class_name */
        args.emplace_back(GetBaseNodeFromCurFunc(*curFunc, true));
        /* set class flag 0 */
        args.emplace_back(mirBuilder->CreateIntConst(0, PTY_u32));
    }
    MIRFunction *func = mirBuilder->GetOrCreateFunction(funcName, static_cast<TyIdx>(PTY_ref));
    func->GetFuncSymbol()->SetAppearsInCode(true);
    beCommon.UpdateTypeTable(*func->GetMIRFuncType());
    func->AllocSymTab();
    CallNode *callAssign = nullptr;
    if (stmt.GetOpCode() == OP_dassign) {
        auto &dsNode = static_cast<const DassignNode &>(stmt);
        MIRSymbol *ret = curFunc->GetLocalOrGlobalSymbol(dsNode.GetStIdx());

        callAssign = mirBuilder->CreateStmtCallAssigned(func->GetPuidx(), args, ret, OP_callassigned);
    } else {
        auto &regNode = static_cast<const RegassignNode &>(stmt);
        callAssign =
            mirBuilder->CreateStmtCallRegassigned(func->GetPuidx(), args, regNode.GetRegIdx(), OP_callassigned);
    }
    blkNode.AppendStatementsFromBlock(*LowerCallAssignedStmt(*callAssign));
}

bool CGLowerer::IsIntrinsicCallHandledAtLowerLevel(MIRIntrinsicID intrinsic) const
{
    switch (intrinsic) {
        case INTRN_MPL_ATOMIC_EXCHANGE_PTR:
        // js
        case INTRN_ADD_WITH_OVERFLOW:
        case INTRN_SUB_WITH_OVERFLOW:
        case INTRN_MUL_WITH_OVERFLOW:
            return true;
        default: {
            return false;
        }
    }
}

bool CGLowerer::IsIntrinsicOpHandledAtLowerLevel(MIRIntrinsicID intrinsic) const
{
    switch (intrinsic) {
#if TARGAARCH64 || TARGX86_64
        case INTRN_C_cos:
        case INTRN_C_cosf:
        case INTRN_C_cosh:
        case INTRN_C_coshf:
        case INTRN_C_acos:
        case INTRN_C_acosf:
        case INTRN_C_sin:
        case INTRN_C_sinf:
        case INTRN_C_sinh:
        case INTRN_C_sinhf:
        case INTRN_C_asin:
        case INTRN_C_asinf:
        case INTRN_C_atan:
        case INTRN_C_atanf:
        case INTRN_C_exp:
        case INTRN_C_expf:
        case INTRN_C_ffs:
        case INTRN_C_log:
        case INTRN_C_logf:
        case INTRN_C_log10:
        case INTRN_C_log10f:
        case INTRN_C_clz32:
        case INTRN_C_clz64:
        case INTRN_C_ctz32:
        case INTRN_C_ctz64:
        case INTRN_C_popcount32:
        case INTRN_C_popcount64:
        case INTRN_C_parity32:
        case INTRN_C_parity64:
        case INTRN_C_clrsb32:
        case INTRN_C_clrsb64:
        case INTRN_C_isaligned:
        case INTRN_C_alignup:
        case INTRN_C_aligndown:
        case INTRN_C___sync_add_and_fetch_1:
        case INTRN_C___sync_add_and_fetch_2:
        case INTRN_C___sync_add_and_fetch_4:
        case INTRN_C___sync_add_and_fetch_8:
        case INTRN_C___sync_sub_and_fetch_1:
        case INTRN_C___sync_sub_and_fetch_2:
        case INTRN_C___sync_sub_and_fetch_4:
        case INTRN_C___sync_sub_and_fetch_8:
        case INTRN_C___sync_fetch_and_add_1:
        case INTRN_C___sync_fetch_and_add_2:
        case INTRN_C___sync_fetch_and_add_4:
        case INTRN_C___sync_fetch_and_add_8:
        case INTRN_C___sync_fetch_and_sub_1:
        case INTRN_C___sync_fetch_and_sub_2:
        case INTRN_C___sync_fetch_and_sub_4:
        case INTRN_C___sync_fetch_and_sub_8:
        case INTRN_C___sync_bool_compare_and_swap_1:
        case INTRN_C___sync_bool_compare_and_swap_2:
        case INTRN_C___sync_bool_compare_and_swap_4:
        case INTRN_C___sync_bool_compare_and_swap_8:
        case INTRN_C___sync_val_compare_and_swap_1:
        case INTRN_C___sync_val_compare_and_swap_2:
        case INTRN_C___sync_val_compare_and_swap_4:
        case INTRN_C___sync_val_compare_and_swap_8:
        case INTRN_C___sync_lock_test_and_set_1:
        case INTRN_C___sync_lock_test_and_set_2:
        case INTRN_C___sync_lock_test_and_set_4:
        case INTRN_C___sync_lock_test_and_set_8:
        case INTRN_C___sync_lock_release_8:
        case INTRN_C___sync_lock_release_4:
        case INTRN_C___sync_lock_release_2:
        case INTRN_C___sync_lock_release_1:
        case INTRN_C___sync_fetch_and_and_1:
        case INTRN_C___sync_fetch_and_and_2:
        case INTRN_C___sync_fetch_and_and_4:
        case INTRN_C___sync_fetch_and_and_8:
        case INTRN_C___sync_fetch_and_or_1:
        case INTRN_C___sync_fetch_and_or_2:
        case INTRN_C___sync_fetch_and_or_4:
        case INTRN_C___sync_fetch_and_or_8:
        case INTRN_C___sync_fetch_and_xor_1:
        case INTRN_C___sync_fetch_and_xor_2:
        case INTRN_C___sync_fetch_and_xor_4:
        case INTRN_C___sync_fetch_and_xor_8:
        case INTRN_C___sync_fetch_and_nand_1:
        case INTRN_C___sync_fetch_and_nand_2:
        case INTRN_C___sync_fetch_and_nand_4:
        case INTRN_C___sync_fetch_and_nand_8:
        case INTRN_C___sync_and_and_fetch_1:
        case INTRN_C___sync_and_and_fetch_2:
        case INTRN_C___sync_and_and_fetch_4:
        case INTRN_C___sync_and_and_fetch_8:
        case INTRN_C___sync_or_and_fetch_1:
        case INTRN_C___sync_or_and_fetch_2:
        case INTRN_C___sync_or_and_fetch_4:
        case INTRN_C___sync_or_and_fetch_8:
        case INTRN_C___sync_xor_and_fetch_1:
        case INTRN_C___sync_xor_and_fetch_2:
        case INTRN_C___sync_xor_and_fetch_4:
        case INTRN_C___sync_xor_and_fetch_8:
        case INTRN_C___sync_nand_and_fetch_1:
        case INTRN_C___sync_nand_and_fetch_2:
        case INTRN_C___sync_nand_and_fetch_4:
        case INTRN_C___sync_nand_and_fetch_8:
        case INTRN_C___sync_synchronize:
        case INTRN_C__builtin_return_address:
        case INTRN_C__builtin_extract_return_addr:
        case INTRN_C_memcmp:
        case INTRN_C_strlen:
        case INTRN_C_strcmp:
        case INTRN_C_strncmp:
        case INTRN_C_strchr:
        case INTRN_C_strrchr:
        case INTRN_C_rev16_2:
        case INTRN_C_rev_4:
        case INTRN_C_rev_8:
            return true;
#endif
        default:
            return false;
    }
}

void CGLowerer::InitArrayClassCacheTableIndex()
{
    MIRSymbol *reflectStrtabSym =
        GlobalTables::GetGsymTable().GetSymbolFromStrIdx(GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(
            namemangler::kReflectionStrtabPrefixStr + mirModule.GetFileNameAsPostfix()));
    MIRSymbol *reflectStartHotStrtabSym =
        GlobalTables::GetGsymTable().GetSymbolFromStrIdx(GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(
            namemangler::kReflectionStartHotStrtabPrefixStr + mirModule.GetFileNameAsPostfix()));
    MIRSymbol *reflectBothHotStrtabSym =
        GlobalTables::GetGsymTable().GetSymbolFromStrIdx(GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(
            namemangler::kReflectionBothHotStrTabPrefixStr + mirModule.GetFileNameAsPostfix()));
    MIRSymbol *reflectRunHotStrtabSym =
        GlobalTables::GetGsymTable().GetSymbolFromStrIdx(GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(
            namemangler::kReflectionRunHotStrtabPrefixStr + mirModule.GetFileNameAsPostfix()));
    MIRSymbol *arrayCacheNameTableSym =
        GlobalTables::GetGsymTable().GetSymbolFromStrIdx(GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(
            namemangler::kArrayClassCacheNameTable + mirModule.GetFileNameAsPostfix()));
    if (arrayCacheNameTableSym == nullptr) {
        return;
    }
    MIRAggConst &aggConst = static_cast<MIRAggConst &>(*(arrayCacheNameTableSym->GetKonst()));
    MIRSymbol *strTab = nullptr;
    for (size_t i = 0; i < aggConst.GetConstVec().size(); ++i) {
        MIRConst *elemConst = aggConst.GetConstVecItem(i);
        uint32 intValue = static_cast<uint32>(((safe_cast<MIRIntConst>(elemConst))->GetExtValue()) & 0xFFFFFFFF);
        bool isHotReflectStr = (intValue & 0x00000003) != 0; /* use the last two bits of intValue in this expression */
        if (isHotReflectStr) {
            uint32 tag =
                (intValue & 0x00000003) - kCStringShift; /* use the last two bits of intValue in this expression */
            if (tag == kLayoutBootHot) {
                strTab = reflectStartHotStrtabSym;
            } else if (tag == kLayoutBothHot) {
                strTab = reflectBothHotStrtabSym;
            } else {
                strTab = reflectRunHotStrtabSym;
            }
        } else {
            strTab = reflectStrtabSym;
        }
        DEBUG_ASSERT(strTab != nullptr, "strTab is nullptr");
        std::string arrayClassName;
        MIRAggConst *strAgg = static_cast<MIRAggConst *>(strTab->GetKonst());
        for (auto start = (intValue >> 2); start < strAgg->GetConstVec().size();
             ++start) { /* the last two bits is flag */
            MIRIntConst *oneChar = static_cast<MIRIntConst *>(strAgg->GetConstVecItem(start));
            if ((oneChar != nullptr) && !oneChar->IsZero()) {
                arrayClassName += static_cast<char>(oneChar->GetExtValue());
            } else {
                break;
            }
        }
        arrayClassCacheIndex[arrayClassName] = i;
    }
}

void CGLowerer::LowerFunc(MIRFunction &func)
{
    labelIdx = 0;
    SetCurrentFunc(&func);
    hasTry = false;
    LowerEntry(func);
    LowerPseudoRegs(func);
    BlockNode *origBody = func.GetBody();
    CHECK_FATAL(origBody != nullptr, "origBody should not be nullptr");

    BlockNode *newBody = LowerBlock(*origBody);
    func.SetBody(newBody);
    if (needBranchCleanup) {
        CleanupBranches(func);
    }

    if (mirModule.IsJavaModule() && func.GetBody()->GetFirst() && GenerateExceptionHandlingCode()) {
        LowerTryCatchBlocks(*func.GetBody());
    }
    uint32 oldTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    // We do the simplify work here because now all the intrinsic calls and potential expansion work of memcpy or other
    // functions are handled well. So we can concentrate to do the replacement work.
    SimplifyBlock(*newBody);
    uint32 newTypeTableSize = GlobalTables::GetTypeTable().GetTypeTableSize();
    if (newTypeTableSize != oldTypeTableSize) {
        beCommon.AddNewTypeAfterBecommon(oldTypeTableSize, newTypeTableSize);
    }
}
} /* namespace maplebe */
