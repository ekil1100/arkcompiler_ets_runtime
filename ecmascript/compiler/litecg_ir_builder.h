/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_COMPILER_LITECG_IR_BUILDER_H
#define ECMASCRIPT_COMPILER_LITECG_IR_BUILDER_H

#include <map>
#include <vector>

#include "ecmascript/compiler/circuit.h"
#include "ecmascript/compiler/gate.h"
#include "ecmascript/compiler/stub_builder.h"
#include "ecmascript/compiler/call_signature.h"
#include "ecmascript/compiler/common_stubs.h"
#include "ecmascript/compiler/interpreter_stub.h"
#include "ecmascript/compiler/rt_call_signature.h"
#include "ecmascript/compiler/ir_builder.h"
#include "ecmascript/compiler/ir_module.h"
#include "ecmascript/jspandafile/method_literal.h"
#include "lmir_builder.h"

namespace panda::ecmascript::kungfu {
class LMIRModule : public IRModule {
public:
    static constexpr int kDeoptEntryOffset = 0;
    LMIRModule(NativeAreaAllocator *allocator, const std::string &name, bool logDbg, const std::string &triple,
         bool isJit)
        : IRModule(allocator, logDbg, triple)
    {
        moduleName = name;
        module = isJit ? nullptr : maple::litecg::CreateModuleWithName(name);
    }

    ~LMIRModule()
    {
        if (module != nullptr) {
            maple::litecg::ReleaseModule(module);
        }
    }

    void JitCreateLitecgModule()
    {
        ASSERT(module == nullptr);
        module = maple::litecg::CreateModuleWithName(moduleName);
    }

    maple::litecg::Module *GetModule()
    {
        ASSERT(module != nullptr);
        return module;
    }

    ModuleKind GetModuleKind() const override
    {
        return MODULE_LITECG;
    }

    void SetFunction(size_t index, std::string funcName, bool isFastCall)
    {
        funcIndexMap_.emplace_back(std::make_tuple(index, funcName, isFastCall));
    }

    template <class Callback>
    void IteratefuncIndexMap(const Callback &cb) const
    {
        for (auto record : funcIndexMap_) {
            // 2: 3nd param
            cb(std::get<0>(record), std::get<1>(record), std::get<2>(record));
        }
    }

private:
    std::string moduleName;
    maple::litecg::Module *module;
    std::vector<std::tuple<size_t, std::string, bool>> funcIndexMap_;
};

class LiteCGIRBuilder {
public:
    LiteCGIRBuilder(const std::vector<std::vector<GateRef>> *schedule, Circuit *circuit, LMIRModule *module,
                    const CompilationConfig *cfg, CallSignature::CallConv callConv, bool enableLog,
                    bool enableOptInlining, const panda::ecmascript::MethodLiteral *methodLiteral,
                    const JSPandaFile *jsPandaFile, const std::string &funcName);
    ~LiteCGIRBuilder();
    void Build();

private:
    struct PhiDesc {
        int predBBId;
        GateRef operand;
        maple::litecg::PregIdx phi;
    };
    const std::vector<std::vector<GateRef>> *scheduledGates_ {nullptr};
    const Circuit *circuit_ {nullptr};
    LMIRModule *lmirModule_ {nullptr};
    const CompilationConfig *compCfg_ {nullptr};
    CallSignature::CallConv callConv_ = CallSignature::CallConv::CCallConv;
    bool enableLog_ {false};
    bool enableOptInlining_ {false};
    const panda::ecmascript::MethodLiteral *methodLiteral_ {nullptr};
    const JSPandaFile *jsPandaFile_ {nullptr};
    std::string funcName_;
    GateAccessor acc_;
    maple::litecg::LMIRBuilder *lmirBuilder_ {nullptr};
    std::unordered_map<GateRef, maple::litecg::LiteCGValue> gate2Expr_;
    std::unordered_map<OpCode, void (LiteCGIRBuilder::*)(GateRef gate)> opHandlers_;
    std::set<OpCode> illegalOpHandlers_;
    std::map<GateId, int> instID2bbID_;
    std::map<int, maple::litecg::BB *> bbID2BB_;
    int slotSize_ {-1};
    maple::litecg::Type *slotType_ {nullptr};
    std::map<int, std::vector<PhiDesc>> bbID2unmergedPhis_;

#define DECLAREVISITLOWEROPCODE(name, signature) void Visit##name signature;
    OPCODES(DECLAREVISITLOWEROPCODE)
#undef DECLAREVISITLOWEROPCODE
#define DECLAREHANDLELOWEROPCODE(name, ignore) void Handle##name(GateRef gate);
    OPCODES(DECLAREHANDLELOWEROPCODE)
#undef DECLAREHANDLELOWEROPCODE
    void SaveGate2Expr(GateRef gate, maple::litecg::Expr expr);
    maple::litecg::Expr GetExprFromGate(GateRef gate);
    maple::litecg::Expr GetConstant(GateRef gate);
    void BuildInstID2BBIDMap();
    maple::litecg::BB &GetOrCreateBB(int bbID);
    maple::litecg::BB &GetFirstBB();
    maple::litecg::BB &CreateBB();
    void AddPhiDesc(int bbID, PhiDesc &desc);
    maple::litecg::Type *ConvertLiteCGTypeFromGate(GateRef gate, bool isSigned = true) const;
    maple::litecg::IntCmpCondition ConvertLiteCGPredicateFromICMP(ICmpCondition cond) const;
    maple::litecg::FloatCmpCondition ConvertLiteCGPredicateFromFCMP(FCmpCondition cond) const;
    void InitializeHandlers();
    maple::litecg::Expr GetGlue(const std::vector<GateRef> &inList);
    maple::litecg::Expr GetRTStubOffset(maple::litecg::Expr glue, int index);
    maple::litecg::Type *ConvertLiteCGTypeFromVariableType(VariableType type) const;
    maple::litecg::Type *GenerateFuncType(const std::vector<maple::litecg::Expr> &params,
                                          const CallSignature *stubDescriptor);
    maple::litecg::Type *GetFuncType(const CallSignature *stubDescriptor) const;
    maple::litecg::Expr GetFunction(maple::litecg::BB &bb, maple::litecg::Expr glue, const CallSignature *signature,
                                    maple::litecg::Expr rtbaseoffset, const std::string &realName = "") const;
    bool IsOptimizedJSFunction() const;
    bool IsOptimized() const;
    CallExceptionKind GetCallExceptionKind(size_t index, OpCode op) const;
    maple::litecg::Expr GetRTStubOffset(maple::litecg::Expr glue, int index) const;
    maple::litecg::Expr GetCoStubOffset(maple::litecg::Expr glue, int index) const;
    maple::litecg::Expr GetCallee(maple::litecg::BB &bb, const std::vector<GateRef> &inList,
                                  const CallSignature *signature, const std::string &realName);
    maple::litecg::Expr CanonicalizeToPtr(maple::litecg::Expr expr, maple::litecg::Type *type);
    // maple::litecg::Expr PointerAdd(maple::litecg::Expr baseAddr,
    //                                 maple::litecg::Expr offset, LiteCGType *type);
    maple::litecg::Expr CanonicalizeToInt(GateRef gate);
    int64_t GetBitWidthFromMachineType(MachineType machineType) const;
    int LookupPredBB(GateRef start, int bbID);
    maple::litecg::Expr GetBuiltinsStubOffset(maple::litecg::Expr glue);
    void UpdateLeaveFrame(maple::litecg::Expr glue);
    maple::litecg::Expr GetLeaveFrameOffset(maple::litecg::Expr glue);
    maple::litecg::Expr CallingFp(bool isCaller);
    maple::litecg::Expr GetBaseOffset(GateRef gate, maple::litecg::Expr glue);
    maple::litecg::Expr GetBCDebugStubOffset(maple::litecg::Expr glue);
    maple::litecg::Expr GetBCStubOffset(maple::litecg::Expr glue);
    maple::litecg::Type *GetExperimentalDeoptTy();
    maple::litecg::Function *GetExperimentalDeopt();
    void GenDeoptEntry(std::string funcName);
    void SaveFrameTypeOnFrame(maple::litecg::BB &bb, FrameType frameType);
    maple::litecg::Expr ConvertToTagged(GateRef gate);
    maple::litecg::Expr ConvertInt32ToTaggedInt(maple::litecg::Expr value);
    maple::litecg::Expr ConvertBoolToTaggedBoolean(GateRef gate);
    maple::litecg::Expr ConvertFloat64ToTaggedDouble(GateRef gate);
    void SaveDeoptVregInfo(std::unordered_map<int, maple::litecg::LiteCGValue> &deoptBundleInfo, maple::litecg::BB &bb,
                           int32_t index, size_t curDepth, size_t shift, GateRef gate);
    void SaveDeoptVregInfoWithI64(std::unordered_map<int, maple::litecg::LiteCGValue> &deoptBundleInfo,
                                  maple::litecg::BB &bb, int32_t index, size_t curDepth, size_t shift, GateRef gate);
    maple::litecg::Type *GetMachineRepType(MachineRep rep) const;

    maple::litecg::ConvAttr ConvertCallAttr(const CallSignature::CallConv callConv);
    void CollectExraCallSiteInfo(std::unordered_map<int, maple::litecg::LiteCGValue> &deoptBundleInfo,
                                 maple::litecg::Expr pcOffset, GateRef frameArgs);
    void GenPrologue(maple::litecg::Function &function);
    void SaveJSFuncOnOptJSFuncFrame(maple::litecg::Var &value);
    void SaveFrameTypeOnFrame(FrameType frameType);
    bool IsInterpreted() const;
    void AddFunc();
    bool IsLogEnabled() const
    {
        return enableLog_;
    }
};
}  // namespace panda::ecmascript::kungfu
#endif  // ECMASCRIPT_COMPILER_LITECG_IR_BUILDER_H
