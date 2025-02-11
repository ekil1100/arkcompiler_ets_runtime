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

#include "becommon.h"
#include "x64_cgfunc.h"
#include "x64_reg_info.h"

namespace maplebe {
using namespace maple;
using namespace x64;
void X64RegInfo::Init()
{
    for (regno_t regNO = kRinvalid; regNO < kMaxRegNum; ++regNO) {
        /* when yieldpoint is enabled, the RYP(R12) can not be used. */
        if (IsYieldPointReg(static_cast<X64reg>(regNO))) {
            continue;
        }
        if (!x64::IsAvailableReg(static_cast<X64reg>(regNO))) {
            continue;
        }
        if (x64::IsGPRegister(static_cast<X64reg>(regNO))) {
            AddToIntRegs(regNO);
        } else {
            AddToFpRegs(regNO);
        }
        AddToAllRegs(regNO);
    }
    return;
}

void X64RegInfo::SaveCalleeSavedReg(MapleSet<regno_t> savedRegs)
{
    X64CGFunc *x64CGFunc = static_cast<X64CGFunc *>(GetCurrFunction());
    for (auto reg : savedRegs) {
        x64CGFunc->AddtoCalleeSaved(static_cast<X64reg>(reg));
    }
}

bool X64RegInfo::IsSpecialReg(regno_t regno) const
{
    X64reg reg = static_cast<X64reg>(regno);
    if ((reg == RBP) || (reg == RSP)) {
        return true;
    }

    /* when yieldpoint is enabled, the dedicated register(RYP) can not be allocated. */
    if (IsYieldPointReg(reg)) {
        return true;
    }
    return false;
}

bool X64RegInfo::IsCalleeSavedReg(regno_t regno) const
{
    return x64::IsCalleeSavedReg(static_cast<X64reg>(regno));
}

bool X64RegInfo::IsYieldPointReg(regno_t regno) const
{
    return false;
}

bool X64RegInfo::IsUnconcernedReg(regno_t regNO) const
{
    /* when yieldpoint is enabled, the RYP(R12) can not be used. */
    if (IsYieldPointReg(static_cast<X64reg>(regNO))) {
        return true;
    }
    return false;
}

bool X64RegInfo::IsUnconcernedReg(const RegOperand &regOpnd) const
{
    RegType regType = regOpnd.GetRegisterType();
    if (regType == kRegTyCc || regType == kRegTyVary) {
        return true;
    }
    uint32 regNO = regOpnd.GetRegisterNumber();
    return IsUnconcernedReg(regNO);
}

void X64RegInfo::Fini() {}

ListOperand *X64RegInfo::CreateListOperand()
{
    CHECK_FATAL(false, "CreateListOperand, unsupported");
    return nullptr;
}

Insn *X64RegInfo::BuildMovInstruction(Operand &opnd0, Operand &opnd1)
{
    CHECK_FATAL(false, "BuildMovInstruction, unsupported");
    return nullptr;
}

RegOperand *X64RegInfo::GetOrCreatePhyRegOperand(regno_t regNO, uint32 size, RegType kind, uint32 flag)
{
    return &(GetCurrFunction()->GetOpndBuilder()->CreatePReg(regNO, size, kind));
}

Insn *X64RegInfo::BuildStrInsn(uint32 regSize, PrimType stype, RegOperand &phyOpnd, MemOperand &memOpnd)
{
    X64MOP_t mOp = x64::MOP_begin;
    switch (regSize) {
        case k8BitSize:
            mOp = (phyOpnd.GetRegisterType() == kRegTyInt) ? x64::MOP_movb_r_m: x64::MOP_begin;
            break;
        case k16BitSize:
            mOp = (phyOpnd.GetRegisterType() == kRegTyInt) ? x64::MOP_movw_r_m : x64::MOP_begin;
            break;
        case k32BitSize:
            mOp = (phyOpnd.GetRegisterType() == kRegTyInt) ? x64::MOP_movl_r_m : x64::MOP_movfs_r_m;
            break;
        case k64BitSize:
            mOp = (phyOpnd.GetRegisterType() == kRegTyInt) ? x64::MOP_movq_r_m : x64::MOP_movfd_r_m;
            break;
        default:
            CHECK_FATAL(false, "NIY");
            break;
    }
    CHECK_FATAL(mOp != x64::MOP_begin, "NIY");
    Insn &insn = GetCurrFunction()->GetInsnBuilder()->BuildInsn(mOp, X64CG::kMd[mOp]);
    insn.AddOpndChain(phyOpnd).AddOpndChain(memOpnd);
    return &insn;
}

Insn *X64RegInfo::BuildLdrInsn(uint32 regSize, PrimType stype, RegOperand &phyOpnd, MemOperand &memOpnd)
{
    X64MOP_t mOp = x64::MOP_begin;
    switch (regSize) {
        case k8BitSize:
            mOp = (phyOpnd.GetRegisterType() == kRegTyInt) ? x64::MOP_movb_m_r : x64::MOP_begin;
            break;
        case k16BitSize:
            mOp = (phyOpnd.GetRegisterType() == kRegTyInt) ? x64::MOP_movw_m_r : x64::MOP_begin;
            break;
        case k32BitSize:
            mOp = (phyOpnd.GetRegisterType() == kRegTyInt) ? x64::MOP_movl_m_r : x64::MOP_movfs_m_r;
            break;
        case k64BitSize:
            mOp = (phyOpnd.GetRegisterType() == kRegTyInt) ? x64::MOP_movq_m_r : x64::MOP_movfd_m_r;
            break;
        default:
            CHECK_FATAL(false, "NIY");
            break;
    }
    CHECK_FATAL(mOp != x64::MOP_begin, "should not happen");
    Insn &insn = GetCurrFunction()->GetInsnBuilder()->BuildInsn(mOp, X64CG::kMd[mOp]);
    insn.AddOpndChain(memOpnd).AddOpndChain(phyOpnd);
    return &insn;
}

Insn *X64RegInfo::BuildCommentInsn(const std::string &comment)
{
    CHECK_FATAL(false, "Comment Insn, unsupported");
    GetCurrFunction()->GetOpndBuilder()->CreateComment(comment);
    return nullptr;
}

void X64RegInfo::FreeSpillRegMem(regno_t vrNum)
{
    X64CGFunc *x64CGFunc = static_cast<X64CGFunc *>(GetCurrFunction());
    x64CGFunc->FreeSpillRegMem(vrNum);
}

MemOperand *X64RegInfo::GetOrCreatSpillMem(regno_t vrNum, uint32 bitSize)
{
    X64CGFunc *x64CGFunc = static_cast<X64CGFunc *>(GetCurrFunction());
    return x64CGFunc->GetOrCreatSpillMem(vrNum, bitSize);
}

MemOperand *X64RegInfo::AdjustMemOperandIfOffsetOutOfRange(MemOperand *memOpnd, regno_t vrNum, bool isDest, Insn &insn,
                                                           regno_t regNum, bool &isOutOfRange)
{
    isOutOfRange = false;
    return memOpnd;
}

} /* namespace maplebe */
