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
#include "ecmascript/stackmap/litecg_stackmap_type.h"

namespace panda::ecmascript::kungfu {
void LiteCGStackMapInfo::ConvertToLLVMStackMapInfo(
    std::vector<LLVMStackMapType::Pc2CallSiteInfo> &pc2StackMapsVec,
    std::vector<LLVMStackMapType::Pc2Deopt> &pc2DeoptInfoVec, Triple triple) const
{
    auto fpReg = GCStackMapRegisters::GetFpRegByTriple(triple);
    for (const auto &callSiteInfo : pc2CallSiteInfoVec_) {
        LLVMStackMapType::Pc2CallSiteInfo pc2CallSiteInfo;
        for (const auto &elem : callSiteInfo) {
            const std::vector<uint64_t> &litecgCallSiteInfo = elem.second;
            LLVMStackMapType::CallSiteInfo llvmCallSiteInfo;
            // parse std::vector<uint64_t>
            for (size_t i = 0; i < litecgCallSiteInfo.size(); i += 2) { // add 2 each time for kind and value
                uint64_t kind = litecgCallSiteInfo[i];
                uint64_t value = litecgCallSiteInfo[i + 1];
                if (kind == 2) {  // kind is 2 means register
                    llvmCallSiteInfo.push_back(std::pair<uint16_t, uint32_t>(0xFFFFU, static_cast<int32_t>(value)));
                } else if (kind == 1) { // stack
                    llvmCallSiteInfo.push_back(std::pair<uint16_t, uint32_t>(fpReg, static_cast<int32_t>(value)));
                } else {
                    LOG_ECMA(FATAL) << "only stack and reigster is supported currently";
                    UNREACHABLE();
                }
            }
            uintptr_t pc = static_cast<uintptr_t>(elem.first);
            pc2CallSiteInfo[pc] = llvmCallSiteInfo;
        }
        pc2StackMapsVec.push_back(pc2CallSiteInfo);
    }
    for (const auto &deoptInfo : pc2DeoptVec_) {
        LLVMStackMapType::Pc2Deopt pc2DeoptInfo;
        for (const auto &elem : deoptInfo) {
            const std::vector<uint64_t> &litecgDeoptInfo = elem.second;
            LLVMStackMapType::DeoptInfoType llvmDeoptInfo;
            // parse std::vector<uint64_t>
            for (size_t i = 0; i < litecgDeoptInfo.size(); i += 3) { // add 3 each time for deoptVreg, kind and value
                uint64_t deoptVreg = litecgDeoptInfo[i];
                uint64_t kind = litecgDeoptInfo[i + 1];
                uint64_t value = litecgDeoptInfo[i + 2];
                llvmDeoptInfo.push_back(static_cast<int32_t>(deoptVreg));
                if (kind == 2) { // kind is 2 means register
                    llvmDeoptInfo.push_back(std::pair<uint16_t, uint32_t>(0xFFFFU, static_cast<int32_t>(value)));
                } else if (kind == 1) { // stack
                    llvmDeoptInfo.push_back(std::pair<uint16_t, uint32_t>(fpReg, static_cast<int32_t>(value)));
                } else { // imm
                    llvmDeoptInfo.push_back(static_cast<int32_t>(value));
                }
            }
            pc2DeoptInfo[elem.first] = llvmDeoptInfo;
        }

        pc2DeoptInfoVec.push_back(pc2DeoptInfo);
    }
}
}  // namespace panda::ecmascript::kungfu