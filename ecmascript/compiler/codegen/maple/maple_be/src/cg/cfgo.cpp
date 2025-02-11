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

#include "cfgo.h"
#include "cgbb.h"
#include "cg.h"
#include "mpl_logging.h"

/*
 * This phase traverses all basic block of cgFunc and finds special
 * basic block patterns, like continuous fallthrough basic block, continuous
 * uncondition jump basic block, unreachable basic block and empty basic block,
 * then do basic mergering, basic block placement transformations,
 * unnecessary jumps elimination, and remove unreachable or empty basic block.
 * This optimization is done on control flow graph basis.
 */
namespace maplebe {
using namespace maple;

#define CFGO_DUMP_NEWPM CG_DEBUG_FUNC(f)

/* return true if to is put after from and there is no real insns between from and to, */
bool ChainingPattern::NoInsnBetween(const BB &from, const BB &to) const
{
    const BB *bb = nullptr;
    for (bb = from.GetNext(); bb != nullptr && bb != &to && bb != cgFunc->GetLastBB(); bb = bb->GetNext()) {
        if (!bb->IsEmptyOrCommentOnly() || bb->IsUnreachable() || bb->GetKind() != BB::kBBFallthru) {
            return false;
        }
    }
    return (bb == &to);
}

/* return true if insns in bb1 and bb2 are the same except the last goto insn. */
bool ChainingPattern::DoSameThing(const BB &bb1, const Insn &last1, const BB &bb2, const Insn &last2) const
{
    const Insn *insn1 = bb1.GetFirstInsn();
    const Insn *insn2 = bb2.GetFirstInsn();
    while (insn1 != nullptr && insn1 != last1.GetNext() && insn2 != nullptr && insn2 != last2.GetNext()) {
        if (!insn1->IsMachineInstruction()) {
            insn1 = insn1->GetNext();
            continue;
        }
        if (!insn2->IsMachineInstruction()) {
            insn2 = insn2->GetNext();
            continue;
        }
        if (insn1->GetMachineOpcode() != insn2->GetMachineOpcode()) {
            return false;
        }
        uint32 opndNum = insn1->GetOperandSize();
        for (uint32 i = 0; i < opndNum; ++i) {
            Operand &op1 = insn1->GetOperand(i);
            Operand &op2 = insn2->GetOperand(i);
            if (&op1 == &op2) {
                continue;
            }
            if (!op1.Equals(op2)) {
                return false;
            }
        }
        insn1 = insn1->GetNext();
        insn2 = insn2->GetNext();
    }
    return (insn1 == last1.GetNext() && insn2 == last2.GetNext());
}

/*
 * BB2 can be merged into BB1, if
 *   1. BB1's kind is fallthrough;
 *   2. BB2 has only one predecessor which is BB1 and BB2 is not the lastbb
 *   3. BB2 is neither catch BB nor switch case BB
 */
bool ChainingPattern::MergeFallthuBB(BB &curBB)
{
    BB *sucBB = curBB.GetNext();
    if (sucBB == nullptr || IsLabelInLSDAOrSwitchTable(sucBB->GetLabIdx()) ||
        !cgFunc->GetTheCFG()->CanMerge(curBB, *sucBB)) {
        return false;
    }
    Log(curBB.GetId());
    if (checkOnly) {
        return false;
    }
    if (sucBB == cgFunc->GetLastBB()) {
        cgFunc->SetLastBB(curBB);
    }
    cgFunc->GetTheCFG()->MergeBB(curBB, *sucBB, *cgFunc);
    keepPosition = true;
    return true;
}

bool ChainingPattern::MergeGotoBB(BB &curBB, BB &sucBB)
{
    Log(curBB.GetId());
    if (checkOnly) {
        return false;
    }
    cgFunc->GetTheCFG()->MergeBB(curBB, sucBB, *cgFunc);
    keepPosition = true;
    return true;
}

bool ChainingPattern::MoveSuccBBAsCurBBNext(BB &curBB, BB &sucBB)
{
    /*
     * without the judge below, there is
     * Assembler Error: CFI state restore without previous remember
     */
    if (sucBB.GetHasCfi() || (sucBB.GetFirstInsn() != nullptr && sucBB.GetFirstInsn()->IsCfiInsn())) {
        return false;
    }
    Log(curBB.GetId());
    if (checkOnly) {
        return false;
    }
    /* put sucBB as curBB's next. */
    DEBUG_ASSERT(sucBB.GetPrev() != nullptr, "the target of current goto BB will not be the first bb");
    sucBB.GetPrev()->SetNext(sucBB.GetNext());
    if (sucBB.GetNext() != nullptr) {
        sucBB.GetNext()->SetPrev(sucBB.GetPrev());
    }
    sucBB.SetNext(curBB.GetNext());
    DEBUG_ASSERT(curBB.GetNext() != nullptr, "current goto BB will not be the last bb");
    curBB.GetNext()->SetPrev(&sucBB);
    sucBB.SetPrev(&curBB);
    curBB.SetNext(&sucBB);
    curBB.RemoveInsn(*curBB.GetLastInsn());
    curBB.SetKind(BB::kBBFallthru);
    return true;
}

bool ChainingPattern::RemoveGotoInsn(BB &curBB, BB &sucBB)
{
    Log(curBB.GetId());
    if (checkOnly) {
        return false;
    }
    if (&sucBB != curBB.GetNext()) {
        DEBUG_ASSERT(curBB.GetNext() != nullptr, "nullptr check");
        curBB.RemoveSuccs(sucBB);
        curBB.PushBackSuccs(*curBB.GetNext());
        curBB.GetNext()->PushBackPreds(curBB);
        sucBB.RemovePreds(curBB);
    }
    curBB.RemoveInsn(*curBB.GetLastInsn());
    curBB.SetKind(BB::kBBFallthru);
    return true;
}

bool ChainingPattern::ClearCurBBAndResetTargetBB(BB &curBB, BB &sucBB)
{
    if (curBB.GetHasCfi() || (curBB.GetFirstInsn() != nullptr && curBB.GetFirstInsn()->IsCfiInsn())) {
        return false;
    }
    Insn *brInsn = nullptr;
    for (brInsn = curBB.GetLastInsn(); brInsn != nullptr; brInsn = brInsn->GetPrev()) {
        if (brInsn->IsUnCondBranch()) {
            break;
        }
    }
    DEBUG_ASSERT(brInsn != nullptr, "goto BB has no branch");
    BB *newTarget = sucBB.GetPrev();
    DEBUG_ASSERT(newTarget != nullptr, "get prev bb failed in ChainingPattern::ClearCurBBAndResetTargetBB");
    Insn *last1 = newTarget->GetLastInsn();
    if (newTarget->GetKind() == BB::kBBGoto) {
        Insn *br = nullptr;
        for (br = newTarget->GetLastInsn(); br != newTarget->GetFirstInsn()->GetPrev(); br = br->GetPrev()) {
            if (br->IsUnCondBranch()) {
                break;
            }
        }
        DEBUG_ASSERT(br != nullptr, "goto BB has no branch");
        last1 = br->GetPrev();
    }
    if (last1 == nullptr || !DoSameThing(*newTarget, *last1, curBB, *brInsn->GetPrev())) {
        return false;
    }

    Log(curBB.GetId());
    if (checkOnly) {
        return false;
    }

    LabelIdx tgtLabIdx = newTarget->GetLabIdx();
    if (newTarget->GetLabIdx() == MIRLabelTable::GetDummyLabel()) {
        tgtLabIdx = cgFunc->CreateLabel();
        newTarget->AddLabel(tgtLabIdx);
        cgFunc->SetLab2BBMap(tgtLabIdx, *newTarget);
    }
    LabelOperand &brTarget = cgFunc->GetOrCreateLabelOperand(tgtLabIdx);
    brInsn->SetOperand(0, brTarget);
    curBB.RemoveInsnSequence(*curBB.GetFirstInsn(), *brInsn->GetPrev());

    curBB.RemoveFromSuccessorList(sucBB);
    curBB.PushBackSuccs(*newTarget);
    sucBB.RemoveFromPredecessorList(curBB);
    newTarget->PushBackPreds(curBB);

    sucBB.GetPrev()->SetUnreachable(false);
    keepPosition = true;
    return true;
}

/*
 * Following optimizations are performed:
 * 1. Basic block merging
 * 2. unnecessary jumps elimination
 * 3. Remove duplicates Basic block.
 */
bool ChainingPattern::Optimize(BB &curBB)
{
    if (curBB.GetKind() == BB::kBBFallthru) {
        return MergeFallthuBB(curBB);
    }

    if (curBB.GetKind() == BB::kBBGoto && !curBB.IsEmpty()) {
        Insn *last = curBB.GetLastInsn();
        if (last->IsTailCall()) {
            return false;
        }

        BB *sucBB = cgFunc->GetTheCFG()->GetTargetSuc(curBB);
        /*
         * BB2 can be merged into BB1, if
         *   1. BB1 ends with a goto;
         *   2. BB2 has only one predecessor which is BB1
         *   3. BB2 is of goto kind. Otherwise, the original fall through will be broken
         *   4. BB2 is neither catch BB nor switch case BB
         */
        if (sucBB == nullptr || curBB.GetEhSuccs().size() != sucBB->GetEhSuccs().size()) {
            return false;
        }
        if (!curBB.GetEhSuccs().empty() && (curBB.GetEhSuccs().front() != sucBB->GetEhSuccs().front())) {
            return false;
        }
        if (sucBB->GetKind() == BB::kBBGoto && !IsLabelInLSDAOrSwitchTable(sucBB->GetLabIdx()) &&
            cgFunc->GetTheCFG()->CanMerge(curBB, *sucBB)) {
            return MergeGotoBB(curBB, *sucBB);
        } else if (sucBB != &curBB && curBB.GetNext() != sucBB && sucBB != cgFunc->GetLastBB() &&
                   !sucBB->IsPredecessor(*sucBB->GetPrev()) &&
                   !(sucBB->GetNext() != nullptr && sucBB->GetNext()->IsPredecessor(*sucBB)) &&
                   !IsLabelInLSDAOrSwitchTable(sucBB->GetLabIdx()) && sucBB->GetEhSuccs().empty() &&
                   sucBB->GetKind() != BB::kBBThrow) {
            return MoveSuccBBAsCurBBNext(curBB, *sucBB);
        }
        /*
         * Last goto instruction can be removed, if:
         *  1. The goto target is physically the next one to current BB.
         */
        else if (sucBB == curBB.GetNext() ||
                 (NoInsnBetween(curBB, *sucBB) && !IsLabelInLSDAOrSwitchTable(curBB.GetNext()->GetLabIdx()))) {
            return RemoveGotoInsn(curBB, *sucBB);
        }
        /*
         * Clear curBB and target it to sucBB->GetPrev()
         *  if sucBB->GetPrev() and curBB's insns are the same.
         *
         * curBB:           curBB:
         *   insn_x0          b prevbb
         *   b sucBB        ...
         * ...         ==>  prevbb:
         * prevbb:            insn_x0
         *   insn_x0        sucBB:
         * sucBB:
         */
        else if (sucBB != curBB.GetNext() && !curBB.IsSoloGoto() && !IsLabelInLSDAOrSwitchTable(curBB.GetLabIdx()) &&
                 sucBB->GetKind() == BB::kBBReturn && sucBB->GetPreds().size() > 1 && sucBB->GetPrev() != nullptr &&
                 sucBB->IsPredecessor(*sucBB->GetPrev()) &&
                 (sucBB->GetPrev()->GetKind() == BB::kBBFallthru || sucBB->GetPrev()->GetKind() == BB::kBBGoto)) {
            return ClearCurBBAndResetTargetBB(curBB, *sucBB);
        }
    }
    return false;
}

/*
 * curBB:             curBB:
 *   insn_x0            insn_x0
 *   b targetBB         b BB
 * ...           ==>  ...
 * targetBB:          targetBB:
 *   b BB               b BB
 * ...                ...
 * BB:                BB:
 * *------------------------------
 * curBB:             curBB:
 *   insn_x0            insn_x0
 *   cond_br brBB       cond_br BB
 * ...                ...
 * brBB:         ==>  brBB:
 *   b BB               b BB
 * ...                ...
 * BB:                BB:
 *
 * conditions:
 *   1. only goto and comment in brBB;
 */
bool SequentialJumpPattern::Optimize(BB &curBB)
{
    if (curBB.IsUnreachable()) {
        return false;
    }
    if (curBB.GetKind() == BB::kBBGoto && !curBB.IsEmpty()) {
        BB *sucBB = cgFunc->GetTheCFG()->GetTargetSuc(curBB);
        CHECK_FATAL(sucBB != nullptr, "sucBB is null in SequentialJumpPattern::Optimize");
        BB *tragetBB = CGCFG::GetTargetSuc(*sucBB);
        if ((sucBB != &curBB) && sucBB->IsSoloGoto() && tragetBB != nullptr && tragetBB != sucBB) {
            Log(curBB.GetId());
            if (checkOnly) {
                return false;
            }
            cgFunc->GetTheCFG()->RetargetJump(*sucBB, curBB);
            SkipSucBB(curBB, *sucBB);
            return true;
        }
    } else if (curBB.GetKind() == BB::kBBIf) {
        for (BB *sucBB : curBB.GetSuccs()) {
            BB *tragetBB = CGCFG::GetTargetSuc(*sucBB);
            if (sucBB != curBB.GetNext() && sucBB->IsSoloGoto() && tragetBB != nullptr && tragetBB != sucBB) {
                Log(curBB.GetId());
                if (checkOnly) {
                    return false;
                }
                cgFunc->GetTheCFG()->RetargetJump(*sucBB, curBB);
                SkipSucBB(curBB, *sucBB);
                return true;
            }
        }
    } else if (curBB.GetKind() == BB::kBBRangeGoto) {
        bool changed = false;
        for (BB *sucBB : curBB.GetSuccs()) {
            if (sucBB != curBB.GetNext() && sucBB->IsSoloGoto() &&
                cgFunc->GetTheCFG()->GetTargetSuc(*sucBB) != nullptr) {
                Log(curBB.GetId());
                if (checkOnly) {
                    return false;
                }
                UpdateSwitchSucc(curBB, *sucBB);
                cgFunc->GetTheCFG()->FlushUnReachableStatusAndRemoveRelations(*sucBB, *cgFunc);
                changed = true;
            }
        }
        return changed;
    }
    return false;
}

void SequentialJumpPattern::UpdateSwitchSucc(BB &curBB, BB &sucBB)
{
    BB *gotoTarget = cgFunc->GetTheCFG()->GetTargetSuc(sucBB);
    CHECK_FATAL(gotoTarget != nullptr, "gotoTarget is null in SequentialJumpPattern::UpdateSwitchSucc");
    const MapleVector<LabelIdx> &labelVec = curBB.GetRangeGotoLabelVec();
    bool isPred = false;
    for (auto label : labelVec) {
        if (label == gotoTarget->GetLabIdx()) {
            isPred = true;
            break;
        }
    }
    for (size_t i = 0; i < labelVec.size(); ++i) {
        if (labelVec[i] == sucBB.GetLabIdx()) {
            curBB.SetRangeGotoLabel(i, gotoTarget->GetLabIdx());
        }
    }
    cgFunc->UpdateEmitSt(curBB, sucBB.GetLabIdx(), gotoTarget->GetLabIdx());

    /* connect curBB, gotoTarget */
    for (auto it = gotoTarget->GetPredsBegin(); it != gotoTarget->GetPredsEnd(); ++it) {
        if (*it == &sucBB) {
            auto origIt = it;
            if (isPred) {
                break;
            }
            if (origIt != gotoTarget->GetPredsBegin()) {
                --origIt;
                gotoTarget->InsertPred(origIt, curBB);
            } else {
                gotoTarget->PushFrontPreds(curBB);
            }
            break;
        }
    }
    for (auto it = curBB.GetSuccsBegin(); it != curBB.GetSuccsEnd(); ++it) {
        if (*it == &sucBB) {
            auto origIt = it;
            curBB.EraseSuccs(it);
            if (isPred) {
                break;
            }
            if (origIt != curBB.GetSuccsBegin()) {
                --origIt;
                curBB.InsertSucc(origIt, *gotoTarget);
            } else {
                curBB.PushFrontSuccs(*gotoTarget);
            }
            break;
        }
    }
    /* cut curBB -> sucBB */
    for (auto it = sucBB.GetPredsBegin(); it != sucBB.GetPredsEnd(); ++it) {
        if (*it == &curBB) {
            sucBB.ErasePreds(it);
        }
    }
    for (auto it = curBB.GetSuccsBegin(); it != curBB.GetSuccsEnd(); ++it) {
        if (*it == &sucBB) {
            curBB.EraseSuccs(it);
        }
    }
}

/*
 * preCond:
 * sucBB is one of curBB's successor.
 *
 * Change curBB's successor to sucBB's successor
 */
void SequentialJumpPattern::SkipSucBB(BB &curBB, BB &sucBB)
{
    BB *gotoTarget = cgFunc->GetTheCFG()->GetTargetSuc(sucBB);
    CHECK_FATAL(gotoTarget != nullptr, "gotoTarget is null in SequentialJumpPattern::SkipSucBB");
    curBB.RemoveSuccs(sucBB);
    curBB.PushBackSuccs(*gotoTarget);
    sucBB.RemovePreds(curBB);
    gotoTarget->PushBackPreds(curBB);
    cgFunc->GetTheCFG()->FlushUnReachableStatusAndRemoveRelations(sucBB, *cgFunc);
}

/*
 * Found pattern
 * curBB:                      curBB:
 *       ...            ==>          ...
 *       cond_br brBB                cond1_br ftBB
 * ftBB:                       brBB:
 *       bl throwfunc                ...
 * brBB:                       retBB:
 *       ...                         ...
 * retBB:                      ftBB:
 *       ...                         bl throwfunc
 */
void FlipBRPattern::RelocateThrowBB(BB &curBB)
{
    BB *ftBB = curBB.GetNext();
    CHECK_FATAL(ftBB != nullptr, "ifBB has a fall through BB");
    CGCFG *theCFG = cgFunc->GetTheCFG();
    CHECK_FATAL(theCFG != nullptr, "nullptr check");
    BB *retBB = theCFG->FindLastRetBB();
    CHECK_FATAL(retBB != nullptr, "must have a return BB");
    if (ftBB->GetKind() != BB::kBBThrow || !ftBB->GetEhSuccs().empty() ||
        IsLabelInLSDAOrSwitchTable(ftBB->GetLabIdx()) || !retBB->GetEhSuccs().empty()) {
        return;
    }
    BB *brBB = theCFG->GetTargetSuc(curBB);
    if (brBB != ftBB->GetNext()) {
        return;
    }

    EHFunc *ehFunc = cgFunc->GetEHFunc();
    if (ehFunc != nullptr && ehFunc->GetLSDACallSiteTable() != nullptr) {
        const MapleVector<LSDACallSite *> &callsiteTable = ehFunc->GetLSDACallSiteTable()->GetCallSiteTable();
        for (size_t i = 0; i < callsiteTable.size(); ++i) {
            LSDACallSite *lsdaCallsite = callsiteTable[i];
            BB *endTry = cgFunc->GetBBFromLab2BBMap(lsdaCallsite->csLength.GetEndOffset()->GetLabelIdx());
            BB *startTry = cgFunc->GetBBFromLab2BBMap(lsdaCallsite->csLength.GetStartOffset()->GetLabelIdx());
            if (retBB->GetId() >= startTry->GetId() && retBB->GetId() <= endTry->GetId()) {
                if (retBB->GetNext()->GetId() < startTry->GetId() || retBB->GetNext()->GetId() > endTry->GetId() ||
                    curBB.GetId() < startTry->GetId() || curBB.GetId() > endTry->GetId()) {
                    return;
                }
            } else {
                if ((retBB->GetNext()->GetId() >= startTry->GetId() && retBB->GetNext()->GetId() <= endTry->GetId()) ||
                    (curBB.GetId() >= startTry->GetId() && curBB.GetId() <= endTry->GetId())) {
                    return;
                }
            }
        }
    }
    /* get branch insn of curBB */
    Insn *curBBBranchInsn = theCFG->FindLastCondBrInsn(curBB);
    CHECK_FATAL(curBBBranchInsn != nullptr, "curBB(it is a kBBif) has no branch");

    /* Reverse the branch */
    uint32 targetIdx = GetJumpTargetIdx(*curBBBranchInsn);
    MOperator mOp = FlipConditionOp(curBBBranchInsn->GetMachineOpcode());
    LabelOperand &brTarget = cgFunc->GetOrCreateLabelOperand(*ftBB);
    curBBBranchInsn->SetMOP(cgFunc->GetCG()->GetTargetMd(mOp));
    curBBBranchInsn->SetOperand(targetIdx, brTarget);

    /* move ftBB after retBB */
    curBB.SetNext(brBB);
    brBB->SetPrev(&curBB);

    retBB->GetNext()->SetPrev(ftBB);
    ftBB->SetNext(retBB->GetNext());
    ftBB->SetPrev(retBB);
    retBB->SetNext(ftBB);
}

/*
 * 1. relocate goto BB
 * Found pattern             (1) ftBB->GetPreds().size() == 1
 * curBB:                      curBB: cond1_br target
 *       ...            ==>    brBB:
 *       cond_br brBB           ...
 * ftBB:                       targetBB: (ftBB,targetBB)
 *       goto target         (2) ftBB->GetPreds().size() > 1
 * brBB:                       curBB : cond1_br ftBB
 *       ...                   brBB:
 * targetBB                      ...
 *                            ftBB
 *                            targetBB
 *
 * 2. relocate throw BB in RelocateThrowBB()
 */
bool FlipBRPattern::Optimize(BB &curBB)
{
    if (curBB.GetKind() == BB::kBBIf && !curBB.IsEmpty()) {
        BB *ftBB = curBB.GetNext();
        DEBUG_ASSERT(ftBB != nullptr, "ftBB is null in  FlipBRPattern::Optimize");
        BB *brBB = cgFunc->GetTheCFG()->GetTargetSuc(curBB);
        DEBUG_ASSERT(brBB != nullptr, "brBB is null in  FlipBRPattern::Optimize");
        /* Check if it can be optimized */
        if (ftBB->GetKind() == BB::kBBGoto && ftBB->GetNext() == brBB) {
            if (!ftBB->GetEhSuccs().empty()) {
                return false;
            }
            Insn *curBBBranchInsn = nullptr;
            for (curBBBranchInsn = curBB.GetLastInsn(); curBBBranchInsn != nullptr;
                 curBBBranchInsn = curBBBranchInsn->GetPrev()) {
                if (curBBBranchInsn->IsBranch()) {
                    break;
                }
            }
            DEBUG_ASSERT(curBBBranchInsn != nullptr, "FlipBRPattern: curBB has no branch");
            Insn *brInsn = nullptr;
            for (brInsn = ftBB->GetLastInsn(); brInsn != nullptr; brInsn = brInsn->GetPrev()) {
                if (brInsn->IsUnCondBranch()) {
                    break;
                }
            }
            DEBUG_ASSERT(brInsn != nullptr, "FlipBRPattern: ftBB has no branch");

            /* Reverse the branch */
            uint32 targetIdx = GetJumpTargetIdx(*curBBBranchInsn);
            MOperator mOp = FlipConditionOp(curBBBranchInsn->GetMachineOpcode());
            if (mOp == 0) {
                return false;
            }
            auto it = ftBB->GetSuccsBegin();
            BB *tgtBB = *it;
            if (ftBB->GetPreds().size() == 1 &&
                (ftBB->IsSoloGoto() ||
                 (!IsLabelInLSDAOrSwitchTable(tgtBB->GetLabIdx()) && cgFunc->GetTheCFG()->CanMerge(*ftBB, *tgtBB)))) {
                curBBBranchInsn->SetMOP(cgFunc->GetCG()->GetTargetMd(mOp));
                Operand &brTarget = brInsn->GetOperand(GetJumpTargetIdx(*brInsn));
                curBBBranchInsn->SetOperand(targetIdx, brTarget);
                /* Insert ftBB's insn at the beginning of tgtBB. */
                if (!ftBB->IsSoloGoto()) {
                    ftBB->RemoveInsn(*brInsn);
                    tgtBB->InsertAtBeginning(*ftBB);
                }
                /* Patch pred and succ lists */
                ftBB->EraseSuccs(it);
                ftBB->PushBackSuccs(*brBB);
                it = curBB.GetSuccsBegin();
                CHECK_FATAL(*it != nullptr, "nullptr check");
                if (*it == brBB) {
                    curBB.EraseSuccs(it);
                    curBB.PushBackSuccs(*tgtBB);
                } else {
                    ++it;
                    curBB.EraseSuccs(it);
                    curBB.PushFrontSuccs(*tgtBB);
                }
                for (it = tgtBB->GetPredsBegin(); it != tgtBB->GetPredsEnd(); ++it) {
                    if (*it == ftBB) {
                        tgtBB->ErasePreds(it);
                        break;
                    }
                }
                tgtBB->PushBackPreds(curBB);
                for (it = brBB->GetPredsBegin(); it != brBB->GetPredsEnd(); ++it) {
                    if (*it == &curBB) {
                        brBB->ErasePreds(it);
                        break;
                    }
                }
                brBB->PushFrontPreds(*ftBB);
                /* Remove instructions from ftBB so curBB falls thru to brBB */
                ftBB->SetFirstInsn(nullptr);
                ftBB->SetLastInsn(nullptr);
                ftBB->SetKind(BB::kBBFallthru);
            } else if (!IsLabelInLSDAOrSwitchTable(ftBB->GetLabIdx()) && !tgtBB->IsPredecessor(*tgtBB->GetPrev())) {
                curBBBranchInsn->SetMOP(cgFunc->GetCG()->GetTargetMd(mOp));
                LabelIdx tgtLabIdx = ftBB->GetLabIdx();
                if (ftBB->GetLabIdx() == MIRLabelTable::GetDummyLabel()) {
                    tgtLabIdx = cgFunc->CreateLabel();
                    ftBB->AddLabel(tgtLabIdx);
                    cgFunc->SetLab2BBMap(tgtLabIdx, *ftBB);
                }
                LabelOperand &brTarget = cgFunc->GetOrCreateLabelOperand(tgtLabIdx);
                curBBBranchInsn->SetOperand(targetIdx, brTarget);
                curBB.SetNext(brBB);
                brBB->SetPrev(&curBB);
                ftBB->SetPrev(tgtBB->GetPrev());
                tgtBB->GetPrev()->SetNext(ftBB);
                ftBB->SetNext(tgtBB);
                tgtBB->SetPrev(ftBB);

                ftBB->RemoveInsn(*brInsn);
                ftBB->SetKind(BB::kBBFallthru);
            }
        } else {
            RelocateThrowBB(curBB);
        }
    }
    return false;
}

/* remove a basic block that contains nothing */
bool EmptyBBPattern::Optimize(BB &curBB)
{
    if (curBB.IsUnreachable()) {
        return false;
    }
    /* Empty bb but do not have cleanup label. */
    if (curBB.GetPrev() != nullptr && curBB.GetFirstStmt() != cgFunc->GetCleanupLabel() &&
        curBB.GetFirstInsn() == nullptr && curBB.GetLastInsn() == nullptr && &curBB != cgFunc->GetLastBB() &&
        curBB.GetKind() != BB::kBBReturn && !IsLabelInLSDAOrSwitchTable(curBB.GetLabIdx())) {
        Log(curBB.GetId());
        if (checkOnly) {
            return false;
        }

        BB *sucBB = cgFunc->GetTheCFG()->GetTargetSuc(curBB);
        if (sucBB == nullptr || sucBB->GetFirstStmt() == cgFunc->GetCleanupLabel()) {
            return false;
        }
        cgFunc->GetTheCFG()->RemoveBB(curBB);
        /* removeBB may do nothing. since no need to repeat, always ret false here. */
        return false;
    }
    return false;
}

/*
 * remove unreachable BB
 * condition:
 *   1. unreachable BB can't have cfi instruction when postcfgo.
 */
bool UnreachBBPattern::Optimize(BB &curBB)
{
    if (curBB.IsUnreachable()) {
        Log(curBB.GetId());
        if (checkOnly) {
            return false;
        }
        /* if curBB in exitbbsvec,return false. */
        if (cgFunc->IsExitBB(curBB)) {
            curBB.SetUnreachable(false);
            return false;
        }

        if (curBB.GetHasCfi() || (curBB.GetFirstInsn() != nullptr && curBB.GetFirstInsn()->IsCfiInsn())) {
            return false;
        }

        EHFunc *ehFunc = cgFunc->GetEHFunc();
        /* if curBB InLSDA ,replace curBB's label with nextReachableBB before remove it. */
        if (ehFunc != nullptr && ehFunc->NeedFullLSDA() && cgFunc->GetTheCFG()->InLSDA(curBB.GetLabIdx(), *ehFunc)) {
            /* find nextReachableBB */
            BB *nextReachableBB = nullptr;
            for (BB *bb = &curBB; bb != nullptr; bb = bb->GetNext()) {
                if (!bb->IsUnreachable()) {
                    nextReachableBB = bb;
                    break;
                }
            }
            CHECK_FATAL(nextReachableBB != nullptr, "nextReachableBB not be nullptr");
            if (nextReachableBB->GetLabIdx() == 0) {
                LabelIdx labIdx = cgFunc->CreateLabel();
                nextReachableBB->AddLabel(labIdx);
                cgFunc->SetLab2BBMap(labIdx, *nextReachableBB);
            }

            ehFunc->GetLSDACallSiteTable()->UpdateCallSite(curBB, *nextReachableBB);
        }

        if (curBB.GetSuccs().empty() && curBB.GetEhSuccs().empty()) {
            return false;
        }

        if (curBB.GetPrev() != nullptr) {
            curBB.GetPrev()->SetNext(curBB.GetNext());
        }
        if (curBB.GetNext() != nullptr) {
            curBB.GetNext()->SetPrev(curBB.GetPrev());
        }

        /* flush after remove; */
        for (BB *bb : curBB.GetSuccs()) {
            bb->RemovePreds(curBB);
            cgFunc->GetTheCFG()->FlushUnReachableStatusAndRemoveRelations(*bb, *cgFunc);
        }
        for (BB *bb : curBB.GetEhSuccs()) {
            bb->RemoveEhPreds(curBB);
            cgFunc->GetTheCFG()->FlushUnReachableStatusAndRemoveRelations(*bb, *cgFunc);
        }
        curBB.ClearSuccs();
        curBB.ClearEhSuccs();
        return true;
    }
    return false;
}

/* BB_pred1:        BB_pred1:
 *   b curBB          insn_x0
 * ...                b BB2
 * BB_pred2:   ==>  ...
 *   b curBB        BB_pred2:
 * ...                insn_x0
 * curBB:             b BB2
 *   insn_x0        ...
 *   b BB2          curBB:
 *                    insn_x0
 *                    b BB2
 * condition:
 *   1. The number of instruct in curBB
 *        is less than THRESHOLD;
 *   2. curBB can't have cfi instruction when postcfgo.
 */
bool DuplicateBBPattern::Optimize(BB &curBB)
{
    if (curBB.IsUnreachable()) {
        return false;
    }
    if (CGOptions::IsNoDupBB() || CGOptions::OptimizeForSize()) {
        return false;
    }

    /* curBB can't be in try block */
    if (curBB.GetKind() != BB::kBBGoto || IsLabelInLSDAOrSwitchTable(curBB.GetLabIdx()) ||
        !curBB.GetEhSuccs().empty()) {
        return false;
    }

#if TARGARM32
    FOR_BB_INSNS(insn, (&curBB)) {
        if (insn->IsPCLoad() || insn->IsClinit()) {
            return false;
        }
    }
#endif
    /* It is possible curBB jump to itself */
    uint32 numPreds = curBB.NumPreds();
    for (BB *bb : curBB.GetPreds()) {
        if (bb == &curBB) {
            numPreds--;
        }
    }

    if (numPreds > 1 && cgFunc->GetTheCFG()->GetTargetSuc(curBB) != nullptr &&
        cgFunc->GetTheCFG()->GetTargetSuc(curBB)->NumPreds() > 1) {
        std::vector<BB *> candidates;
        for (BB *bb : curBB.GetPreds()) {
            if (bb->GetKind() == BB::kBBGoto && bb->GetNext() != &curBB && bb != &curBB && !bb->IsEmpty()) {
                candidates.emplace_back(bb);
            }
        }
        if (candidates.empty()) {
            return false;
        }
        if (curBB.NumInsn() <= kThreshold) {
            if (curBB.GetHasCfi() || (curBB.GetFirstInsn() != nullptr && curBB.GetFirstInsn()->IsCfiInsn())) {
                return false;
            }
            Log(curBB.GetId());
            if (checkOnly) {
                return false;
            }
            bool changed = false;
            for (BB *bb : candidates) {
                if (curBB.GetEhSuccs().size() != bb->GetEhSuccs().size()) {
                    continue;
                }
                if (!curBB.GetEhSuccs().empty() && (curBB.GetEhSuccs().front() != bb->GetEhSuccs().front())) {
                    continue;
                }
                bb->RemoveInsn(*bb->GetLastInsn());
                FOR_BB_INSNS(insn, (&curBB)) {
                    Insn *clonedInsn = cgFunc->GetTheCFG()->CloneInsn(*insn);
                    clonedInsn->SetPrev(nullptr);
                    clonedInsn->SetNext(nullptr);
                    clonedInsn->SetBB(nullptr);
                    bb->AppendInsn(*clonedInsn);
                }
                bb->RemoveSuccs(curBB);
                for (BB *item : curBB.GetSuccs()) {
                    bb->PushBackSuccs(*item);
                    item->PushBackPreds(*bb);
                }
                curBB.RemovePreds(*bb);
                changed = true;
            }
            cgFunc->GetTheCFG()->FlushUnReachableStatusAndRemoveRelations(curBB, *cgFunc);
            return changed;
        }
    }
    return false;
}

/* === new pm === */
bool CgCfgo::PhaseRun(maplebe::CGFunc &f)
{
    CFGOptimizer *cfgOptimizer = f.GetCG()->CreateCFGOptimizer(*GetPhaseMemPool(), f);
    const std::string &funcClass = f.GetFunction().GetBaseClassName();
    const std::string &funcName = f.GetFunction().GetBaseFuncName();
    const std::string &name = funcClass + funcName;
    if (CFGO_DUMP_NEWPM) {
        DotGenerator::GenerateDot("before-cfgo", f, f.GetMirModule());
    }
    cfgOptimizer->Run(name);
    if (CFGO_DUMP_NEWPM) {
        DotGenerator::GenerateDot("after-cfgo", f, f.GetMirModule());
    }
    return false;
}
MAPLE_TRANSFORM_PHASE_REGISTER_CANSKIP(CgCfgo, cfgo)

bool CgPostCfgo::PhaseRun(maplebe::CGFunc &f)
{
    CFGOptimizer *cfgOptimizer = f.GetCG()->CreateCFGOptimizer(*GetPhaseMemPool(), f);
    const std::string &funcClass = f.GetFunction().GetBaseClassName();
    const std::string &funcName = f.GetFunction().GetBaseFuncName();
    const std::string &name = funcClass + funcName;
    if (CFGO_DUMP_NEWPM) {
        DotGenerator::GenerateDot("before-postcfgo", f, f.GetMirModule());
    }
    cfgOptimizer->Run(name);
    if (CFGO_DUMP_NEWPM) {
        DotGenerator::GenerateDot("after-postcfgo", f, f.GetMirModule());
    }
    return false;
}
MAPLE_TRANSFORM_PHASE_REGISTER_CANSKIP(CgPostCfgo, postcfgo)
} /* namespace maplebe */
