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

#ifndef ECMASCRIPT_COMPILER_JIT_COMPILER_H
#define ECMASCRIPT_COMPILER_JIT_COMPILER_H

#include "ecmascript/compiler/pass_manager.h"
#include "ecmascript/ecma_vm.h"
#include "ecmascript/jit/jit_task.h"

namespace panda::ecmascript::kungfu {
extern "C" {
PUBLIC_API void *CreateJitCompiler(EcmaVM *vm, JitTask *jitTask);
PUBLIC_API bool JitCompile(void *compiler, JitTask *jitTask);
PUBLIC_API bool JitFinalize(void *compiler, JitTask *jitTask);
PUBLIC_API void DeleteJitCompile(void *handle);
};

struct JitCompilationOptions {
    JitCompilationOptions(JSRuntimeOptions &runtimeOptions, EcmaVM *vm);
    JitCompilationOptions() = default;

    std::string triple_;
    std::string outputFileName_;
    size_t optLevel_;
    size_t relocMode_;
    std::string logOption_;
    std::string logMethodsList_;
    bool compilerLogTime_;
    size_t maxAotMethodSize_;
    size_t maxMethodsInModule_;
    uint32_t hotnessThreshold_;
    std::string profilerIn_;
    bool isEnableArrayBoundsCheckElimination_;
    bool isEnableTypeLowering_;
    bool isEnableEarlyElimination_;
    bool isEnableLaterElimination_;
    bool isEnableValueNumbering_;
    bool isEnableOptInlining_;
    bool isEnableTypeInfer_;
    bool isEnableOptPGOType_;
    bool isEnableOptTrackField_;
    bool isEnableOptLoopPeeling_;
    bool isEnableOptOnHeapCheck_;
    bool isEnableOptLoopInvariantCodeMotion_;
    bool isEnableCollectLiteralInfo_;
    bool isEnableOptConstantFolding_;
    bool isEnableLexenvSpecialization_;
    bool isEnableNativeInline_;
    bool isEnableLoweringBuiltin_;
};

class JitCompiler {
public:
    static JitCompiler *Create(EcmaVM *vm, JitTask *jitTask);
    explicit JitCompiler(EcmaVM *vm, JSHandle<JSFunction> jsFunction)
        : vm_(vm),
          jsFunction_(jsFunction),
          jitOptions_(vm->GetJSOptions(), vm),
          log_(jitOptions_.logOption_),
          logList_(jitOptions_.logMethodsList_),
          profilerDecoder_(jitOptions_.profilerIn_, jitOptions_.hotnessThreshold_)
    {
        Init();
    }
    ~JitCompiler();
    void Init();

    bool Compile();
    bool Finalize(JitTask *jitTask);

private:
    EcmaVM *vm_;
    JSHandle<JSFunction> jsFunction_;
    JitCompilationOptions jitOptions_;
    CompilerLog log_;
    AotMethodLogList logList_;
    PGOProfilerDecoder profilerDecoder_;
    PassOptions passOptions_;
    JitPassManager *passManager_;
    AOTFileGenerator *aotFileGenerator_;
};
}  // namespace panda::ecmascript::kungfu
#endif  // ECMASCRIPT_COMPILER_JIT_COMPILER_H
