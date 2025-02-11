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

#include <chrono>
#include <iostream>
#include <memory>
#include <signal.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)
#include <vector>

#include "ecmascript/base/string_helper.h"
#include "ecmascript/compiler/aot_compiler_preprocessor.h"
#include "ecmascript/compiler/aot_file/aot_file_manager.h"
#include "ecmascript/compiler/pass_manager.h"
#include "ecmascript/ecma_string.h"
#include "ecmascript/js_runtime_options.h"
#include "ecmascript/jspandafile/js_pandafile_manager.h"
#include "ecmascript/jspandafile/program_object.h"
#include "ecmascript/log.h"
#include "ecmascript/log_wrapper.h"
#include "ecmascript/module/js_module_manager.h"
#include "ecmascript/napi/include/jsnapi.h"
#include "ecmascript/ohos/ohos_pkg_args.h"
#include "ecmascript/platform/file.h"

namespace panda::ecmascript::kungfu {
namespace {
void CompileValidFiles(PassManager &passManager, AOTFileGenerator &generator, bool &ret,
                       const CVector<AbcFileInfo> &fileInfos)
{
    for (const AbcFileInfo &fileInfo : fileInfos) {
        JSPandaFile *jsPandaFile = fileInfo.jsPandaFile_.get();
        const std::string &extendedFilePath = fileInfo.extendedFilePath_;
        LOG_COMPILER(INFO) << "AOT compile: " << extendedFilePath;
        generator.SetCurrentCompileFileName(jsPandaFile->GetNormalizedFileDesc());
        if (passManager.Compile(jsPandaFile, extendedFilePath, generator) == false) {
            ret = false;
            continue;
        }
    }
}
} // namespace

int Main(const int argc, const char **argv)
{
    auto startTime =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
                    .count();
    LOG_ECMA(DEBUG) << "Print ark_aot_compiler received args:";
    for (int i = 0; i < argc; i++) {
        LOG_ECMA(DEBUG) << argv[i];
    }

    if (argc < 2) { // 2: at least have two arguments
        LOG_COMPILER(ERROR) << AotCompilerPreprocessor::GetHelper();
        return -1;
    }

    JSRuntimeOptions runtimeOptions;
    bool retOpt = runtimeOptions.ParseCommand(argc, argv);
    if (!retOpt) {
        LOG_COMPILER(ERROR) << AotCompilerPreprocessor::GetHelper();
        return 1;
    }

    if (runtimeOptions.IsStartupTime()) {
        LOG_COMPILER(DEBUG) << "Startup start time: " << startTime;
    }

    bool ret = true;
    // ark_aot_compiler running need disable asm interpreter to disable the loading of AOT files.
    runtimeOptions.SetEnableAsmInterpreter(false);
    runtimeOptions.DisableReportModuleResolvingFailure();
    runtimeOptions.SetOptionsForTargetCompilation();
    EcmaVM *vm = JSNApi::CreateEcmaVM(runtimeOptions);
    if (vm == nullptr) {
        LOG_COMPILER(ERROR) << "Cannot Create vm";
        return -1;
    }

    {
        LocalScope scope(vm);
        arg_list_t pandaFileNames {};
        std::map<std::string, std::shared_ptr<OhosPkgArgs>> pkgArgsMap;
        CompilationOptions cOptions(vm, runtimeOptions);

        CompilerLog log(cOptions.logOption_);
        log.SetEnableCompilerLogTime(cOptions.compilerLogTime_);
        AotMethodLogList logList(cOptions.logMethodsList_);
        PGOProfilerDecoder profilerDecoder;

        AotCompilerPreprocessor cPreprocessor(vm, runtimeOptions, pkgArgsMap, profilerDecoder, pandaFileNames);
        if (!cPreprocessor.HandleTargetCompilerMode(cOptions) || !cPreprocessor.HandlePandaFileNames(argc, argv)) {
            return 1;
        }
        if (runtimeOptions.IsPartialCompilerMode() && cOptions.profilerIn_.empty()) {
            // no need to compile in partial mode without any ap files.
            return 0;
        }
        profilerDecoder.SetHotnessThreshold(cOptions.hotnessThreshold_);
        profilerDecoder.SetInPath(cOptions.profilerIn_);
        cPreprocessor.AOTInitialize();
        cPreprocessor.SetShouldCollectLiteralInfo(cOptions, &log);
        if (!cPreprocessor.GenerateAbcFileInfos()) {
            return 1;
        }
        cPreprocessor.GenerateGlobalTypes(cOptions);
        cPreprocessor.GeneratePGOTypes(cOptions);
        cPreprocessor.SnapshotInitialize();
        ret = cPreprocessor.GetCompilerResult();

        PassOptions::Builder optionsBuilder;
        PassOptions passOptions =
            optionsBuilder.EnableArrayBoundsCheckElimination(cOptions.isEnableArrayBoundsCheckElimination_)
                .EnableTypeLowering(cOptions.isEnableTypeLowering_)
                .EnableEarlyElimination(cOptions.isEnableEarlyElimination_)
                .EnableLaterElimination(cOptions.isEnableLaterElimination_)
                .EnableValueNumbering(cOptions.isEnableValueNumbering_)
                .EnableTypeInfer(cOptions.isEnableTypeInfer_)
                .EnableOptInlining(cOptions.isEnableOptInlining_)
                .EnableOptPGOType(cOptions.isEnableOptPGOType_)
                .EnableOptTrackField(cOptions.isEnableOptTrackField_)
                .EnableOptLoopPeeling(cOptions.isEnableOptLoopPeeling_)
                .EnableOptLoopInvariantCodeMotion(cOptions.isEnableOptLoopInvariantCodeMotion_)
                .EnableCollectLiteralInfo(cOptions.isEnableCollectLiteralInfo_)
                .EnableOptConstantFolding(cOptions.isEnableOptConstantFolding_)
                .EnableLexenvSpecialization(cOptions.isEnableLexenvSpecialization_)
                .EnableInlineNative(cOptions.isEnableNativeInline_)
                .EnableLoweringBuiltin(cOptions.isEnableLoweringBuiltin_)
                .Build();

        PassManager passManager(vm,
                                cOptions.triple_,
                                cOptions.optLevel_,
                                cOptions.relocMode_,
                                &log,
                                &logList,
                                cOptions.maxAotMethodSize_,
                                cOptions.maxMethodsInModule_,
                                profilerDecoder,
                                &passOptions);

        bool isEnableLiteCG = runtimeOptions.IsCompilerEnableLiteCG();
        AOTFileGenerator generator(&log, &logList, vm, cOptions.triple_, isEnableLiteCG);
        const auto &fileInfos = cPreprocessor.GetAbcFileInfo();
        CompileValidFiles(passManager, generator, ret, fileInfos);
        std::string appSignature = cPreprocessor.GetMainPkgArgsAppSignature();
        generator.SaveAOTFile(cOptions.outputFileName_ + AOTFileManager::FILE_EXTENSION_AN, appSignature);
        generator.SaveSnapshotFile();
        log.Print();
    }

    LOG_COMPILER(INFO) << (ret ? "ts aot compile success" : "ts aot compile failed");
    JSNApi::DestroyJSVM(vm);
    return ret ? 0 : -1;
}
} // namespace panda::ecmascript::kungfu

int main(const int argc, const char **argv)
{
    auto result = panda::ecmascript::kungfu::Main(argc, argv);
    return result;
}
