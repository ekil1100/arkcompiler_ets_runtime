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

#include "ecmascript/js_runtime_options.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <getopt.h>

#include "ecmascript/compiler/aot_file/an_file_data_manager.h"
#include "ecmascript/mem/mem_common.h"

namespace panda::ecmascript {
const std::string PUBLIC_API COMMON_HELP_HEAD_MSG =
    "Usage: jsvm  <option>  <filename.abc>\n"
    "\n"
    "Options:\n";

const std::string PUBLIC_API COMPILER_HELP_HEAD_MSG =
    "Usage: ark_aot_compiler  <option>  --aot-file=<filename>  <filename.abc>\n"
    "\n"
    "Options:\n";

const std::string PUBLIC_API STUB_HELP_HEAD_MSG =
    "Usage: ark_stub_compiler <option>\n"
    "\n"
    "Options:\n";

const std::string PUBLIC_API HELP_OPTION_MSG =
    "--aot-file:                           Path (file suffix not needed) to AOT output file. Default: 'aot_file'\n"
    "--ark-properties:                     Set ark properties\n"
    "--ark-bundle-name:                    Set ark bundle name\n"
    "--asm-interpreter:                    Enable asm interpreter. Default: 'true'\n"
    "--asm-opcode-disable-range:           Opcode range when asm interpreter is enabled.\n"
    "--compiler-assert-types:              Enable type assertion for type inference tests. Default: 'false'\n"
    "--builtins-dts:                       Builtins.d.abc file path for AOT.\n"
    "--builtins-lazy:                      Load some builtins function later.This option is only valid in workervm.\n"
    "--compiler-log:                       Log Option For aot compiler and stub compiler,\n"
    "                                      'none': no log,\n"
    "                                      'allllircirasm' or 'all012': print all log for all methods,\n"
    "                                      'allcir' or 'all0': print IR info for all methods,\n"
    "                                      'allllir' or 'all1': print llir info for all methods,\n"
    "                                      'allasm' or 'all2': print asm log for all methods,\n"
    "                                      'alltype' or 'all3': print type infer log for all methods,\n"
    "                                      'cerllircirasm' or 'cer0112': print all log for certain method defined "
    "                                      in 'mlist-for-log',\n"
    "                                      'cercir' or 'cer0': print IR for methods in 'mlist-for-log',\n"
    "                                      'cerasm' or 'cer2': print log for methods in 'mlist-for-log',\n"
    "                                      Default: 'none'\n"
    "--compiler-log-methods:               Specific method list for compiler log, only used when compiler-log. "
    "                                      Default: 'none'\n"
    "--compiler-type-threshold:            enable to skip methods whose type is no more than threshold. Default: -1\n"
    "--compiler-log-snapshot:              Enable to print snapshot information. Default: 'false'\n"
    "--compiler-opt-global-typeinfer:      Enable global typeinfer for aot compiler: Default: 'false'\n"
    "--compiler-log-time:                  Enable to print pass compiler time. Default: 'false'\n"
    "--enable-ark-tools:                   Enable ark tools to debug. Default: 'false'\n"
    "--compiler-trace-bc:                  Enable tracing bytecode for aot runtime. Default: 'false'\n"
    "--compiler-trace-deopt:               Enable tracing deopt for aot runtime. Default: 'false'\n"
    "--compiler-trace-inline:              Enable tracing inline function for aot runtime. Default: 'false'\n"
    "--compiler-trace-value-numbering:     Enable tracing value numbering for aot runtime. Default: 'false'\n"
    "--compiler-max-inline-bytecodes       Set max bytecodes count which aot function can be inlined. Default: '25'\n"
    "--compiler-deopt-threshold:           Set max count which aot function can occur deoptimization. Default: '10'\n"
    "--compiler-stress-deopt:              Enable stress deopt for aot compiler. Default: 'false'\n"
    "--compiler-opt-code-profiler:         Enable opt code Bytecode Statistics for aot runtime. Default: 'false'\n"
    "--enable-force-gc:                    Enable force gc when allocating object. Default: 'true'\n"
    "--enable-ic:                          Switch of inline cache. Default: 'true'\n"
    "--enable-runtime-stat:                Enable statistics of runtime state. Default: 'false'\n"
    "--compiler-opt-array-bounds-check-elimination: Enable Index Check elimination. Default: 'false'\n"
    "--compiler-opt-constant-folding:      Enable constant folding. Default: 'true'\n"
    "--compiler-opt-type-lowering:         Enable all type optimization pass for aot compiler. Default: 'true'\n"
    "--compiler-opt-early-elimination:     Enable EarlyElimination for aot compiler. Default: 'true'\n"
    "--compiler-opt-later-elimination:     Enable LaterElimination for aot compiler. Default: 'true'\n"
    "--compiler-opt-value-numbering:       Enable ValueNumbering for aot compiler. Default: 'true'\n"
    "--compiler-opt-inlining:              Enable inlining function for aot compiler: Default: 'true'\n"
    "--compiler-opt-pgotype:               Enable pgo type for aot compiler: Default: 'true'\n"
    "--compiler-opt-track-field:           Enable track field for aot compiler: Default: 'false'\n"
    "--entry-point:                        Full name of entrypoint function. Default: '_GLOBAL::func_main_0'\n"
    "--force-full-gc:                      If true trigger full gc, else trigger semi and old gc. Default: 'true'\n"
    "--framework-abc-file:                 Snapshot file. Default: 'strip.native.min.abc'\n"
    "--gc-long-paused-time:                Set gc's longPauseTime in millisecond. Default: '40'\n"
    "--gc-thread-num:                      Set gc thread number. Default: '7'\n"
    "--heap-size-limit:                    Max heap size (MB). Default: '512'\n"
    "--help:                               Print this message and exit\n"
    "--icu-data-path:                      Path to generated icu data file. Default: 'default'\n"
    "--enable-worker:                      Whether is worker vm. Default: 'false'\n"
    "--log-level:                          Log level: ['debug', 'info', 'warning', 'error', 'fatal'].\n"
    "--log-components:                     Enable logs from specified components: ['all', 'gc', 'ecma','interpreter',\n"
    "                                      'debugger', 'compiler', 'builtins', 'trace', 'jit', 'all']. \n"
    "                                      Default: 'all'\n"
    "--log-debug:                          Enable debug or above logs for components: ['all', 'gc', 'ecma',\n"
    "                                      'interpreter', 'debugger', 'compiler', 'builtins', 'trace', 'jit', 'all'].\n"
    "                                      Default: 'all'\n"
    "--log-error:                          Enable error log for components: ['all', 'gc', 'ecma',\n"
    "                                      'interpreter', 'debugger', 'compiler', 'builtins', 'trace', 'jit', 'all'].\n"
    "                                      Default: 'all'\n"
    "--log-fatal:                          Enable fatal log for components: ['all', 'gc', 'ecma',\n"
    "                                      'interpreter', 'debugger', 'compiler', 'builtins', 'trace', 'jit', 'all'].\n"
    "                                      Default: 'all'\n"
    "--log-info:                           Enable info log for components: ['all', 'gc', 'ecma',\n"
    "                                      'interpreter', 'debugger', 'compiler', 'builtins', 'trace', 'jit', 'all'].\n"
    "                                      Default: 'all'\n"
    "--log-warning:                        Enable warning log for components: ['all', 'gc', 'ecma',\n"
    "                                      'interpreter', 'debugger', 'compiler', 'trace', 'jit', 'builtins', \n"
    "                                      'all']. Default: 'all'\n"
    "--compiler-opt-max-method:            Enable aot compiler to skip method larger than limit (KB). Default: '32'\n"
    "--compiler-module-methods:            The number of max compiled methods in a module. Default: '100'\n"
    "--max-unmovable-space:                Set max unmovable space capacity\n"
    "--merge-abc:                          ABC file is merge abc. Default: 'false'\n"
    "--compiler-opt-level:                 Optimization level configuration of aot compiler. Default: '3'\n"
    "--options:                            Print compiler and runtime options\n"
    "--compiler-print-type-info:           Enable print type info. Default: 'false'\n"
    "--serializer-buffer-size-limit:       Max serializer buffer size used by the VM in Byte. Default size is 2GB\n"
    "--snapshot-file:                      Snapshot file. Default: '/system/etc/snapshot'\n"
    "--startup-time:                       Print the start time of command execution. Default: 'false'\n"
    "--stub-file:                          Path of file includes common stubs module compiled by stub compiler. "
    "                                      Default: 'stub.an'\n"
    "--enable-pgo-profiler:                Enable pgo profiler to sample jsfunction call and output to file. "
    "                                      Default: 'false'\n"
    "--enable-elements-kind:               Enable elementsKind sampling and usage. Default: 'false'\n"
    "--compiler-pgo-hotness-threshold:     Set hotness threshold for pgo in aot compiler. Default: '2'\n"
    "--compiler-pgo-profiler-path:         The pgo file output dir or the pgo file dir of AOT compiler. Default: ''\n"
    "--compiler-pgo-save-min-interval:     Set the minimum time interval for automatically saving profile, "
    "                                      Unit seconds. Default: '30s'\n"
    "--enable-pgo-loading-history:         Enable recording PGO loading history. Default: true\n"
    "--pgo-max-colletion-times:            Set the maximum number of times collect runtime ap (less 0 means infinity). "
    "                                      Default: -1\n"
    "--compiler-target-triple:             CPU triple for aot compiler or stub compiler. \n"
    "                                      values: ['x86_64-unknown-linux-gnu', 'arm-unknown-linux-gnu', \n"
    "                                      'aarch64-unknown-linux-gnu'], Default: 'x86_64-unknown-linux-gnu'\n"
    "--enable-print-execute-time:          Enable print execute panda file spent time\n"
    "--compiler-verify-vtable:             Verify vtable result for aot compiler. Default: 'false'\n"
    "--compiler-select-methods             Compiler selected methods for aot. Only work in full compiling mode\n"
    "                                      Format:--compile-methods=record1:m1,m2,record2:m3\n"
    "--compiler-skip-methods               Compiler skpped methods for aot. Only work in full compiling mode\n"
    "                                      Format:--compile-skip-methods=record1:m1,m2,record2:m3\n"
    "--target-compiler-mode                The compilation mode at the device side, including partial, full and none."
    "                                      Default: ''\n"
    "--hap-path(Deprecated)                The path of the app hap. Default: ''\n"
    "--hap-abc-offset(Deprecated)          The offset of the abc file in app hap. Default: '0'\n"
    "--hap-abc-size(Deprecated)            The size of the abc file in app hap. Default: '0'\n"
    "--compiler-fast-compile               Disable some time-consuming pass. Default: 'true'\n"
    "--compiler-no-check                   Enable remove checks for aot compiler. Default: 'false'\n"
    "--compiler-opt-loop-peeling:          Enable loop peeling for aot compiler: Default: 'false'\n"
    "--compiler-pkg-info                   Specify the package json info for ark aot compiler\n"
    "--compiler-external-pkg-info          Specify the external package json info for ark aot compiler\n"
    "--compiler-enable-external-pkg        Enable compile with external package for ark aot compiler\n"
    "--compiler-enable-lexenv-specialization: Enable replace ldlexvar with specific values: Default: 'true'\n"
    "--compiler-enable-native-inline:      Enable inline native function: Default: 'false'\n"
    "--compiler-enable-lowering-builtin:   Enable lowering global object: Default: 'false'\n"
    "--compiler-opt-array-onheap-check:    Enable TypedArray on heap check for aot compiler: Default: 'false'\n"
    "--compiler-enable-litecg:             Enable LiteCG: Default: 'false'\n"
    "--compiler-enable-jit:                         Enable jit: Default: 'false'\n"
    "--compiler-jit-hotness-threshold:              Set hotness threshold for jit. Default: '2'\n"
    "--compiler-force-jit-compile-main:             Enable jit compile main function: Default: 'false'\n"
    "--compiler-trace-jit:                 Enable trace jit: Default: 'false'\n\n";

bool JSRuntimeOptions::ParseCommand(const int argc, const char **argv)
{
    const struct option longOptions[] = {
        {"aot-file", required_argument, nullptr, OPTION_AOT_FILE},
        {"ark-properties", required_argument, nullptr, OPTION_ARK_PROPERTIES},
        {"ark-bundleName", required_argument, nullptr, OPTION_ARK_BUNDLENAME},
        {"asm-interpreter", required_argument, nullptr, OPTION_ENABLE_ASM_INTERPRETER},
        {"asm-opcode-disable-range", required_argument, nullptr, OPTION_ASM_OPCODE_DISABLE_RANGE},
        {"compiler-assert-types", required_argument, nullptr, OPTION_COMPILER_ASSERT_TYPES},
        {"builtins-dts", required_argument, nullptr, OPTION_BUILTINS_DTS},
        {"builtins-lazy", required_argument, nullptr, OPTION_ENABLE_BUILTINS_LAZY},
        {"compiler-log", required_argument, nullptr, OPTION_COMPILER_LOG_OPT},
        {"compiler-log-methods", required_argument, nullptr, OPTION_COMPILER_LOG_METHODS},
        {"compiler-log-snapshot", required_argument, nullptr, OPTION_COMPILER_LOG_SNAPSHOT},
        {"compiler-log-time", required_argument, nullptr, OPTION_COMPILER_LOG_TIME},
        {"compiler-opt-global-typeinfer", required_argument, nullptr, OPTION_COMPILER_OPT_GLOBAL_TYPEINFER},
        {"compiler-type-threshold", required_argument, nullptr, OPTION_COMPILER_TYPE_THRESHOLD},
        {"enable-ark-tools", required_argument, nullptr, OPTION_ENABLE_ARK_TOOLS},
        {"compiler-trace-bc", required_argument, nullptr, OPTION_COMPILER_TRACE_BC},
        {"compiler-trace-deopt", required_argument, nullptr, OPTION_COMPILER_TRACE_DEOPT},
        {"compiler-trace-inline", required_argument, nullptr, OPTION_COMPILER_TRACE_INLINE},
        {"compiler-trace-value-numbering", required_argument, nullptr, OPTION_COMPILER_TRACE_VALUE_NUMBERING},
        {"compiler-trace-instruction-combine", required_argument, nullptr, OPTION_COMPILER_TRACE_INSTRUCTION_COMBINE},
        {"compiler-max-inline-bytecodes", required_argument, nullptr, OPTION_COMPILER_MAX_INLINE_BYTECODES},
        {"compiler-deopt-threshold", required_argument, nullptr, OPTION_COMPILER_DEOPT_THRESHOLD},
        {"compiler-stress-deopt", required_argument, nullptr, OPTION_COMPILER_STRESS_DEOPT},
        {"compiler-opt-code-profiler", required_argument, nullptr, OPTION_COMPILER_OPT_CODE_PROFILER},
        {"enable-force-gc", required_argument, nullptr, OPTION_ENABLE_FORCE_GC},
        {"enable-ic", required_argument, nullptr, OPTION_ENABLE_IC},
        {"enable-runtime-stat", required_argument, nullptr, OPTION_ENABLE_RUNTIME_STAT},
        {"compiler-opt-constant-folding", required_argument, nullptr, OPTION_COMPILER_OPT_CONSTANT_FOLDING},
        {"compiler-opt-array-bounds-check-elimination",
         required_argument,
         nullptr,
         OPTION_COMPILER_OPT_ARRAY_BOUNDS_CHECK_ELIMINATION},
        {"compiler-opt-type-lowering", required_argument, nullptr, OPTION_COMPILER_OPT_TYPE_LOWERING},
        {"compiler-opt-early-elimination", required_argument, nullptr, OPTION_COMPILER_OPT_EARLY_ELIMINATION},
        {"compiler-opt-later-elimination", required_argument, nullptr, OPTION_COMPILER_OPT_LATER_ELIMINATION},
        {"compiler-opt-instr-combine", required_argument, nullptr, OPTION_COMPILER_OPT_INSTRUCTIONE_COMBINE},
        {"compiler-opt-value-numbering", required_argument, nullptr, OPTION_COMPILER_OPT_VALUE_NUMBERING},
        {"compiler-opt-new-value-numbering", required_argument, nullptr, OPTION_COMPILER_OPT_NEW_VALUE_NUMBERING},
        {"compiler-opt-inlining", required_argument, nullptr, OPTION_COMPILER_OPT_INLINING},
        {"compiler-opt-pgotype", required_argument, nullptr, OPTION_COMPILER_OPT_PGOTYPE},
        {"compiler-opt-track-field", required_argument, nullptr, OPTION_COMPILER_OPT_TRACK_FIELD},
        {"entry-point", required_argument, nullptr, OPTION_ENTRY_POINT},
        {"force-full-gc", required_argument, nullptr, OPTION_FORCE_FULL_GC},
        {"gc-thread-num", required_argument, nullptr, OPTION_GC_THREADNUM},
        {"heap-size-limit", required_argument, nullptr, OPTION_HEAP_SIZE_LIMIT},
        {"help", no_argument, nullptr, OPTION_HELP},
        {"icu-data-path", required_argument, nullptr, OPTION_ICU_DATA_PATH},
        {"enable-worker", required_argument, nullptr, OPTION_ENABLE_WORKER},
        {"log-components", required_argument, nullptr, OPTION_LOG_COMPONENTS},
        {"log-debug", required_argument, nullptr, OPTION_LOG_DEBUG},
        {"log-error", required_argument, nullptr, OPTION_LOG_ERROR},
        {"log-fatal", required_argument, nullptr, OPTION_LOG_FATAL},
        {"log-info", required_argument, nullptr, OPTION_LOG_INFO},
        {"log-level", required_argument, nullptr, OPTION_LOG_LEVEL},
        {"log-warning", required_argument, nullptr, OPTION_LOG_WARNING},
        {"gc-long-paused-time", required_argument, nullptr, OPTION_GC_LONG_PAUSED_TIME},
        {"compiler-opt-max-method", required_argument, nullptr, OPTION_COMPILER_OPT_MAX_METHOD},
        {"compiler-module-methods", required_argument, nullptr, OPTION_COMPILER_MODULE_METHODS},
        {"max-unmovable-space", required_argument, nullptr, OPTION_MAX_UNMOVABLE_SPACE},
        {"merge-abc", required_argument, nullptr, OPTION_MERGE_ABC},
        {"enable-context", required_argument, nullptr, OPTION_ENABLE_CONTEXT},
        {"compiler-opt-level", required_argument, nullptr, OPTION_ASM_OPT_LEVEL},
        {"options", no_argument, nullptr, OPTION_OPTIONS},
        {"compiler-print-type-info", required_argument, nullptr, OPTION_COMPILER_PRINT_TYPE_INFO},
        {"reloc-mode", required_argument, nullptr, OPTION_RELOCATION_MODE},
        {"serializer-buffer-size-limit", required_argument, nullptr, OPTION_SERIALIZER_BUFFER_SIZE_LIMIT},
        {"startup-time", required_argument, nullptr, OPTION_STARTUP_TIME},
        {"stub-file", required_argument, nullptr, OPTION_STUB_FILE},
        {"compiler-target-triple", required_argument, nullptr, OPTION_COMPILER_TARGET_TRIPLE},
        {"enable-print-execute-time", required_argument, nullptr, OPTION_PRINT_EXECUTE_TIME},
        {"enable-pgo-profiler", required_argument, nullptr, OPTION_ENABLE_PGO_PROFILER},
        {"enable-elements-kind", required_argument, nullptr, OPTION_ENABLE_ELEMENTSKIND},
        {"compiler-pgo-profiler-path", required_argument, nullptr, OPTION_COMPILER_PGO_PROFILER_PATH},
        {"compiler-pgo-hotness-threshold", required_argument, nullptr, OPTION_COMPILER_PGO_HOTNESS_THRESHOLD},
        {"compiler-pgo-save-min-interval", required_argument, nullptr, OPTION_COMPILER_PGO_SAVE_MIN_INTERVAL},
        {"compiler-verify-vtable", required_argument, nullptr, OPTION_COMPILER_VERIFY_VTABLE},
        {"compiler-select-methods", required_argument, nullptr, OPTION_COMPILER_SELECT_METHODS},
        {"compiler-skip-methods", required_argument, nullptr, OPTION_COMPILER_SKIP_METHODS},
        {"target-compiler-mode", required_argument, nullptr, OPTION_TARGET_COMPILER_MODE},
        {"hap-path", required_argument, nullptr, OPTION_HAP_PATH},
        {"hap-abc-offset", required_argument, nullptr, OPTION_HAP_ABC_OFFSET},
        {"hap-abc-size", required_argument, nullptr, OPTION_HAP_ABC_SIZE},
        {"compiler-no-check", required_argument, nullptr, OPTION_COMPILER_NOCHECK},
        {"compiler-fast-compile", required_argument, nullptr, OPTION_FAST_AOT_COMPILE_MODE},
        {"compiler-opt-loop-peeling", required_argument, nullptr, OPTION_COMPILER_OPT_LOOP_PEELING},
        {"compiler-opt-array-onheap-check", required_argument, nullptr, OPTION_COMPILER_OPT_ON_HEAP_CHECK},
        {"compiler-pkg-info", required_argument, nullptr, OPTION_COMPILER_PKG_INFO},
        {"compiler-external-pkg-info", required_argument, nullptr, OPTION_COMPILER_EXTERNAL_PKG_INFO},
        {"compiler-enable-external-pkg", required_argument, nullptr, OPTION_COMPILER_ENABLE_EXTERNAL_PKG},
        {"compiler-enable-lexenv-specialization",
         required_argument,
         nullptr,
         OPTION_COMPILER_ENABLE_LEXENV_SPECIALIZATION},
        {"compiler-enable-native-inline", required_argument, nullptr, OPTION_COMPILER_ENABLE_NATIVE_INLINE},
        {"compiler-enable-lowering-builtin", required_argument, nullptr, OPTION_COMPILER_ENABLE_LOWERING_BUILTIN},
        {"compiler-enable-litecg", required_argument, nullptr, OPTION_COMPILER_ENABLE_LITECG},
        {"compiler-enable-jit", required_argument, nullptr, OPTION_COMPILER_ENABLE_JIT},
        {"compiler-jit-hotness-threshold", required_argument, nullptr, OPTION_COMPILER_JIT_HOTNESS_THRESHOLD},
        {"compiler-force-jit-compile-main", required_argument, nullptr, OPTION_COMPILER_FORCE_JIT_COMPILE_MAIN},
        {"enable-pgo-loading-history", required_argument, nullptr, OPTION_ENABLE_PGO_LOADING_HISTORY},
        {"pgo-max-collection-times", required_argument, nullptr, OPTION_PGO_MAX_COLLECTION_TIMES},
        {nullptr, 0, nullptr, 0},
    };

    int index = 0;
    opterr = 0;
    bool ret = false;
    int option = 0;
    arg_list_t argListStr;
    uint32_t argUint32 = 0;
    uint64_t argUInt64 = 0;
    int argInt = 0;
    bool argBool = false;
    double argDouble = 0.0;
    static std::string COLON = ":";

    if (argc <= 1) {
        return true;
    }

    while (true) {
        option = getopt_long_only(argc, const_cast<char **>(argv), "", longOptions, &index);
        LOG_ECMA(INFO) << "option: " << option << ", optopt: " << optopt << ", optind: " << optind;

        if (optind <= 0 || optind > argc) {
            return false;
        }

        if (option == -1) {
            return true;
        }

        // unknown option or required_argument option has no argument
        if (option == OPTION_DEFAULT) {
            ret = SetDefaultValue(const_cast<char *>(argv[optind - 1]));
            if (ret) {
                continue;
            } else {
                return ret;
            }
        }

        WasSet(option);
        switch (option) {
            case OPTION_AOT_FILE:
                SetAOTOutputFile(optarg);
                ecmascript::AnFileDataManager::GetInstance()->SetEnable(true);
                break;
            case OPTION_ARK_PROPERTIES:
                ret = ParseIntParam("ark-properties", &argInt);
                if (ret) {
                    SetArkProperties(argInt);
                } else {
                    return false;
                }
                break;
            case OPTION_ARK_BUNDLENAME:
                SetArkBundleName(optarg);
                break;
            case OPTION_ENABLE_ASM_INTERPRETER:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableAsmInterpreter(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_ASM_OPCODE_DISABLE_RANGE:
                SetAsmOpcodeDisableRange(optarg);
                break;
            case OPTION_COMPILER_ASSERT_TYPES:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetAssertTypes(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_BUILTINS_DTS:
                SetBuiltinsDTS(optarg);
                break;
            case OPTION_ENABLE_BUILTINS_LAZY:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableBuiltinsLazy(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_LOG_OPT:
                SetCompilerLogOption(optarg);
                break;
            case OPTION_COMPILER_LOG_SNAPSHOT:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetCompilerLogSnapshot(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_LOG_TIME:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetCompilerLogTime(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_ENABLE_ARK_TOOLS:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableArkTools(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_TRACE_BC:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetTraceBc(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_TRACE_DEOPT:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetTraceDeopt(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_TRACE_INLINE:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetTraceInline(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_TRACE_VALUE_NUMBERING:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetTraceValueNumbering(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_TRACE_INSTRUCTION_COMBINE:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetTraceInstructionCombine(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_MAX_INLINE_BYTECODES:
                ret = ParseUint32Param("max-inline-bytecodes", &argUint32);
                if (ret) {
                    SetMaxInlineBytecodes(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_DEOPT_THRESHOLD:
                ret = ParseUint32Param("deopt-threshol", &argUint32);
                if (ret) {
                    SetDeoptThreshold(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_STRESS_DEOPT:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetStressDeopt(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_CODE_PROFILER:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetOptCodeProfiler(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_ENABLE_FORCE_GC:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableForceGC(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_FORCE_FULL_GC:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetForceFullGC(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_ENABLE_IC:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableIC(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_ENABLE_RUNTIME_STAT:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableRuntimeStat(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_GC_THREADNUM:
                ret = ParseUint32Param("gc-thread-num", &argUint32);
                if (ret) {
                    SetGcThreadNum(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_HEAP_SIZE_LIMIT:
                ret = ParseUint32Param("heap-size-limit", &argUint32);
                if (ret) {
                    SetHeapSizeLimit(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_HELP:
                return false;
            case OPTION_ICU_DATA_PATH:
                SetIcuDataPath(optarg);
                break;
            case OPTION_ENABLE_WORKER:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetIsWorker(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_LOG_COMPONENTS:
                ParseListArgParam(optarg, &argListStr, COLON);
                SetLogComponents(argListStr);
                break;
            case OPTION_LOG_DEBUG:
                ParseListArgParam(optarg, &argListStr, COLON);
                SetLogDebug(argListStr);
                break;
            case OPTION_LOG_ERROR:
                ParseListArgParam(optarg, &argListStr, COLON);
                SetLogError(argListStr);
                break;
            case OPTION_LOG_FATAL:
                ParseListArgParam(optarg, &argListStr, COLON);
                SetLogFatal(argListStr);
                break;
            case OPTION_LOG_INFO:
                ParseListArgParam(optarg, &argListStr, COLON);
                SetLogInfo(argListStr);
                break;
            case OPTION_LOG_LEVEL:
                SetLogLevel(optarg);
                break;
            case OPTION_LOG_WARNING:
                ParseListArgParam(optarg, &argListStr, COLON);
                SetLogWarning(argListStr);
                break;
            case OPTION_GC_LONG_PAUSED_TIME:
                ret = ParseUint32Param("gc-long-paused-time", &argUint32);
                if (ret) {
                    SetLongPauseTime(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_MAX_METHOD:
                ret = ParseUint32Param("compiler-opt-max-method", &argUint32);
                if (ret) {
                    SetMaxAotMethodSize(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_MODULE_METHODS:
                ret = ParseUint32Param("compiler-module-methods", &argUint32);
                if (!ret) {
                    return false;
                }
                SetCompilerModuleMethods(argUint32);
                break;
            case OPTION_COMPILER_TYPE_THRESHOLD:
                ret = ParseDoubleParam("compiler-type-threshold", &argDouble);
                if (ret) {
                    SetTypeThreshold(argDouble);
                } else {
                    return false;
                }
                break;
            case OPTION_MAX_UNMOVABLE_SPACE:
                ret = ParseUint32Param("max-unmovable-space", &argUint32);
                if (ret) {
                    SetMaxNonmovableSpaceCapacity(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_LOG_METHODS:
                SetMethodsListForLog(optarg);
                break;
            case OPTION_ASM_OPT_LEVEL:
                ret = ParseUint32Param("compiler-opt-level", &argUint32);
                if (ret) {
                    SetOptLevel(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_PRINT_TYPE_INFO:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetPrintTypeInfo(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_PRINT_EXECUTE_TIME:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnablePrintExecuteTime(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_ENABLE_PGO_PROFILER:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnablePGOProfiler(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_ENABLE_ELEMENTSKIND:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableElementsKind(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_PGO_PROFILER_PATH:
                if (*optarg == '\0') {
                    return false;
                }
                SetPGOProfilerPath(optarg);
                break;
            case OPTION_COMPILER_PGO_HOTNESS_THRESHOLD:
                ret = ParseUint32Param("pgo-hotness-threshold", &argUint32);
                if (ret) {
                    SetPGOHotnessThreshold(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_PGO_SAVE_MIN_INTERVAL:
                ret = ParseUint32Param("compiler-pgo-save-min-interval)", &argUint32);
                if (ret) {
                    SetPGOSaveMinInterval(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_ENABLE_PGO_LOADING_HISTORY:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnablePGOLoadingHistory(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_PGO_MAX_COLLECTION_TIMES:
                ret = ParseIntParam("pgo-max-collection-times", &argInt);
                if (ret) {
                    SetPGOMaxCollectionTimes(argInt);
                } else {
                    return false;
                }
                break;
            case OPTION_RELOCATION_MODE:
                ret = ParseUint32Param("reloc-mode", &argUint32);
                if (ret) {
                    SetRelocMode(argUint32);
                } else {
                    return false;
                }
                break;
            case OPTION_SERIALIZER_BUFFER_SIZE_LIMIT:
                ret = ParseUint64Param("serializer-buffer-size-limit", &argUInt64);
                if (ret) {
                    SetSerializerBufferSizeLimit(argUInt64);
                } else {
                    return false;
                }
                break;
            case OPTION_STARTUP_TIME:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetStartupTime(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_STUB_FILE:
                SetStubFile(optarg);
                break;
            case OPTION_COMPILER_TARGET_TRIPLE:
                SetTargetTriple(optarg);
                break;
            case OPTION_ENTRY_POINT:
                SetEntryPoint(optarg);
                break;
            case OPTION_MERGE_ABC:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetMergeAbc(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_ENABLE_CONTEXT:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableContext(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_TYPE_LOWERING:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableTypeLowering(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_ARRAY_BOUNDS_CHECK_ELIMINATION:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableArrayBoundsCheckElimination(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_EARLY_ELIMINATION:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableEarlyElimination(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_LATER_ELIMINATION:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableLaterElimination(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_INSTRUCTIONE_COMBINE:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableInstrcutionCombine(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_VALUE_NUMBERING:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableValueNumbering(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_NEW_VALUE_NUMBERING:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableNewValueNumbering(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_INLINING:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableOptInlining(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_PGOTYPE:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableOptPGOType(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_TRACK_FIELD:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableOptTrackField(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_GLOBAL_TYPEINFER:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableGlobalTypeInfer(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_VERIFY_VTABLE:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetVerifyVTable(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_SELECT_METHODS:
                SetCompilerSelectMethods(optarg);
                break;
            case OPTION_COMPILER_SKIP_METHODS:
                SetCompilerSkipMethods(optarg);
                break;
            case OPTION_TARGET_COMPILER_MODE:
                SetTargetCompilerMode(optarg);
                break;
            case OPTION_HAP_PATH:
                SetHapPath(optarg);
                break;
            case OPTION_HAP_ABC_OFFSET:
                ret = ParseUint32Param("hap-abc-offset", &argUint32);
                if (!ret) {
                    return false;
                }
                SetHapAbcOffset(argUint32);
                break;
            case OPTION_HAP_ABC_SIZE:
                ret = ParseUint32Param("hap-abc-size", &argUint32);
                if (!ret) {
                    return false;
                }
                SetHapAbcSize(argUint32);
                break;
            case OPTION_COMPILER_NOCHECK:
                ret = ParseBoolParam(&argBool);
                if (!ret) {
                    return false;
                }
                SetCompilerNoCheck(argBool);
                break;
            case OPTION_FAST_AOT_COMPILE_MODE:
                ret = ParseBoolParam(&argBool);
                if (!ret) {
                    return false;
                }
                SetFastAOTCompileMode(argBool);
                break;
            case OPTION_COMPILER_OPT_CONSTANT_FOLDING:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableOptConstantFolding(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_LOOP_PEELING:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableOptLoopPeeling(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_OPT_ON_HEAP_CHECK:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableOptOnHeapCheck(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_PKG_INFO:
                SetCompilerPkgJsonInfo(optarg);
                break;
            case OPTION_COMPILER_EXTERNAL_PKG_INFO:
                SetCompilerExternalPkgJsonInfo(optarg);
                break;
            case OPTION_COMPILER_ENABLE_EXTERNAL_PKG:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetCompilerEnableExternalPkg(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_ENABLE_LEXENV_SPECIALIZATION:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableLexenvSpecialization(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_ENABLE_NATIVE_INLINE:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableNativeInline(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_ENABLE_LOWERING_BUILTIN:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableLoweringBuiltin(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_ENABLE_LITECG:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetCompilerEnableLiteCG(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_ENABLE_JIT:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetEnableJIT(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_JIT_HOTNESS_THRESHOLD:
                ret = ParseUint32Param("compiler-jit-hotness-threshold", &argUint32);
                if (ret) {
                    uint16_t val = argUint32 > std::numeric_limits<uint16_t>::max() ?
                        std::numeric_limits<uint16_t>::max() : static_cast<uint16_t>(argUint32);
                    SetJitHotnessThreshold(val);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_FORCE_JIT_COMPILE_MAIN:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetForceJitCompileMain(argBool);
                } else {
                    return false;
                }
                break;
            case OPTION_COMPILER_TRACE_JIT:
                ret = ParseBoolParam(&argBool);
                if (ret) {
                    SetTraceJIT(argBool);
                } else {
                    return false;
                }
                break;
            default:
                LOG_ECMA(ERROR) << "Invalid option\n";
                return false;
        }
    }
}

bool JSRuntimeOptions::SetDefaultValue(char* argv)
{
    WasSet(optopt);

    if (optopt == OPTION_DEFAULT) { // unknown option
        LOG_ECMA(ERROR) << " Invalid option \"" << argv << "\"";
        return false;
    }

    if (optopt > OPTION_OPTIONS) { // unknown argument
        LOG_ECMA(ERROR) << "getopt: \"" << argv <<"\" argument has invalid parameter value \n";
        return false;
    }
    return true;
}

bool JSRuntimeOptions::ParseBoolParam(bool* argBool)
{
    if ((strcmp(optarg, "false") == 0) || (strcmp(optarg, "0") == 0)) {
        *argBool = false;
    } else {
        *argBool = true;
    }
    return true;
}

bool JSRuntimeOptions::ParseDoubleParam(const std::string &option, double *argDouble)
{
    *argDouble = std::stod(optarg, nullptr);
    if (errno == ERANGE) {
        LOG_ECMA(ERROR) << "getopt: \"" << option << "\" argument has invalid parameter value \"" << optarg <<"\"\n";
        return false;
    }
    return true;
}

bool JSRuntimeOptions::ParseIntParam(const std::string &option, int *argInt)
{
    if (StartsWith(optarg, "0x")) {
        const int HEX = 16;
        *argInt = std::stoi(optarg, nullptr, HEX);
    } else {
        *argInt = std::stoi(optarg);
    }

    if (errno == ERANGE) {
        LOG_ECMA(ERROR) << "getopt: \"" << option << "\" argument has invalid parameter value \"" << optarg <<"\"\n";
        return false;
    }
    return true;
}

bool JSRuntimeOptions::ParseUint32Param(const std::string &option, uint32_t *argUInt32)
{
    if (StartsWith(optarg, "0x")) {
        const int HEX = 16;
        *argUInt32 = std::strtoull(optarg, nullptr, HEX);
    } else {
        const int DEC = 10;
        *argUInt32 = std::strtoull(optarg, nullptr, DEC);
    }

    if (errno == ERANGE) {
        LOG_ECMA(ERROR) << "getopt: \"" << option << "\" argument has invalid parameter value \"" << optarg <<"\"\n";
        return false;
    }
    return true;
}

bool JSRuntimeOptions::ParseUint64Param(const std::string &option, uint64_t *argUInt64)
{
    if (StartsWith(optarg, "0x")) {
        const int HEX = 16;
        *argUInt64 = std::strtoull(optarg, nullptr, HEX);
    } else {
        const int DEC = 10;
        *argUInt64 = std::strtoull(optarg, nullptr, DEC);
    }

    if (errno == ERANGE) {
        LOG_ECMA(ERROR) << "getopt: \"" << option << "\" argument has invalid parameter value \"" << optarg <<"\"\n";
        return false;
    }
    return true;
}

void JSRuntimeOptions::ParseListArgParam(const std::string &option, arg_list_t *argListStr, std::string delimiter)
{
    argListStr->clear();
    std::size_t strIndex = 0;
    std::size_t pos = option.find_first_of(delimiter, strIndex);
    while ((pos < option.size()) && (pos > strIndex)) {
        argListStr->push_back(option.substr(strIndex, pos - strIndex));
        strIndex = pos;
        strIndex = option.find_first_not_of(delimiter, strIndex);
        pos = option.find_first_of(delimiter, strIndex);
    }
    if (pos > strIndex) {
        argListStr->push_back(option.substr(strIndex, pos - strIndex));
    }
    return;
}

void JSRuntimeOptions::SetTargetBuiltinsDtsPath()
{
    WasSet(CommandValues::OPTION_BUILTINS_DTS);
    std::string builtinsDtsPath = TARGET_BUILTINS_DTS_PATH;
    SetBuiltinsDTS(builtinsDtsPath);
}

void JSRuntimeOptions::SetOptionsForTargetCompilation()
{
    if (IsTargetCompilerMode()) {
        SetTargetBuiltinsDtsPath();
        SetTargetTriple("aarch64-unknown-linux-gnu");
        if (IsPartialCompilerMode()) {
            SetEnableOptPGOType(true);
            if (IsPGOProfilerPathEmpty()) {
                LOG_ECMA(ERROR) << "no pgo profile file in partial mode!";
            }
        } else {
            SetEnableOptPGOType(false);
            SetPGOProfilerPath("");
        }
    }
}
}
