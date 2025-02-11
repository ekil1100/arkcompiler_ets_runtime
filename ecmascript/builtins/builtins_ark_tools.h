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

#ifndef ECMASCRIPT_BUILTINS_BUILTINS_ARK_TOOLS_H
#define ECMASCRIPT_BUILTINS_BUILTINS_ARK_TOOLS_H

#include "ecmascript/base/builtins_base.h"
#include "ecmascript/js_thread.h"

// List of functions in ArkTools, extension of ArkTS engine.
// V(name, func, length, stubIndex)
// where BuiltinsArkTools::func refers to the native implementation of ArkTools[name].
//       kungfu::BuiltinsStubCSigns::stubIndex refers to the builtin stub index, or INVALID if no stub available.
#define BUILTIN_ARK_TOOLS_FUNCTIONS_COMMON(V)                                             \
    V("compareHClass",                 CompareHClass,                 2, INVALID)         \
    V("dumpHClass",                    DumpHClass,                    1, INVALID)         \
    V("excutePendingJob",              ExcutePendingJob,              0, INVALID)         \
    V("forceFullGC",                   ForceFullGC,                   0, INVALID)         \
    V("getHClass",                     GetHClass,                     1, INVALID)         \
    V("getLexicalEnv",                 GetLexicalEnv,                 1, INVALID)         \
    V("hasTSSubtyping",                HasTSSubtyping,                1, INVALID)         \
    V("hiddenStackSourceFile",         HiddenStackSourceFile,         0, INVALID)         \
    V("hintGC",                        HintGC,                        0, INVALID)         \
    V("isNotHoleProperty",             IsNotHoleProperty,             2, INVALID)         \
    V("isPrototype",                   IsPrototype,                   1, INVALID)         \
    V("isRegExpReplaceDetectorValid",  IsRegExpReplaceDetectorValid,  0, INVALID)         \
    V("isSymbolIteratorDetectorValid", IsSymbolIteratorDetectorValid, 1, INVALID)         \
    V("isTSHClass",                    IsTSHClass,                    1, INVALID)         \
    V("pgoAssertType",                 PGOAssertType,                 2, INVALID)         \
    V("print",                         ObjectDump,                    0, INVALID)         \
    V("removeAOTFlag",                 RemoveAOTFlag,                 1, INVALID)         \
    V("timeInUs",                      TimeInUs,                      0, INVALID)         \
    V("getElementsKind",               GetElementsKind,               1, INVALID)         \
    V("isAOTCompiled",                 IsAOTCompiled,                 1, INVALID)

#define BUILTIN_ARK_TOOLS_FUNCTIONS_REGRESS(V)                                                                \
    V("prepareFunctionForOptimization",            PrepareFunctionForOptimization,            1, INVALID)     \
    V("optimizeFunctionOnNextCall",                OptimizeFunctionOnNextCall,                1, INVALID)     \
    V("optimizeMaglevOnNextCall",                  OptimizeMaglevOnNextCall,                  1, INVALID)     \
    V("deoptimizeFunction",                        DeoptimizeFunction,                        1, INVALID)     \
    V("optimizeOsr",                               OptimizeOsr,                               1, INVALID)     \
    V("neverOptimizeFunction",                     NeverOptimizeFunction,                     1, INVALID)     \
    V("heapObjectVerify",                          HeapObjectVerify,                          1, INVALID)     \
    V("disableOptimizationFinalization",           DisableOptimizationFinalization,           0, INVALID)     \
    V("deoptimizeNow",                             DeoptimizeNow,                             0, INVALID)     \
    V("deoptimize_now",                            DeoptimizeNow,                             0, INVALID)     \
    V("waitForBackgroundOptimization",             WaitForBackgroundOptimization,             0, INVALID)     \
    V("gc",                                        Gc,                                        0, INVALID)     \
    V("toLength",                                  ToLength,                                  1, INVALID)     \
    V("hasHoleyElements",                          HasHoleyElements,                          1, INVALID)     \
    V("hasDictionaryElements",                     HasDictionaryElements,                     1, INVALID)     \
    V("hasSmiElements",                            HasSmiElements,                            1, INVALID)     \
    V("hasDoubleElements",                         HasDoubleElements,                         1, INVALID)     \
    V("hasObjectElements",                         HasObjectElements,                         1, INVALID)     \
    V("arrayBufferDetach",                         ArrayBufferDetach,                         1, INVALID)     \
    V("haveSameMap",                               HaveSameMap,                               2, INVALID)

#ifdef ECMASCRIPT_SUPPORT_CPUPROFILER
#define BUILTIN_ARK_TOOLS_FUNCTIONS_CPUPROFILER(V)      \
    V("startCpuProf", StartCpuProfiler, 0, INVALID)     \
    V("stopCpuProf",  StopCpuProfiler,  0, INVALID)
#else
#define BUILTIN_ARK_TOOLS_FUNCTIONS_CPUPROFILER(V) // Nothing
#endif

#define BUILTIN_ARK_TOOLS_FUNCTIONS(V)                  \
    BUILTIN_ARK_TOOLS_FUNCTIONS_COMMON(V)               \
    BUILTIN_ARK_TOOLS_FUNCTIONS_CPUPROFILER(V)          \
    BUILTIN_ARK_TOOLS_FUNCTIONS_REGRESS(V)

namespace panda::ecmascript::builtins {
class BuiltinsArkTools : public base::BuiltinsBase {
public:
    // Make sure the enable-ark-tools has been opened before use it
    // Use through ArkTools.print(msg, [obj1, obj2, ... objn]) in js
    static JSTaggedValue ObjectDump(EcmaRuntimeCallInfo *info);

    static JSTaggedValue ExcutePendingJob(EcmaRuntimeCallInfo *info);

    static JSTaggedValue GetLexicalEnv(EcmaRuntimeCallInfo *info);

    static JSTaggedValue CompareHClass(EcmaRuntimeCallInfo *info);

    static JSTaggedValue DumpHClass(EcmaRuntimeCallInfo *info);

    // return whether the hclass used for object is created by static ts type
    static JSTaggedValue IsTSHClass(EcmaRuntimeCallInfo *info);

    static JSTaggedValue GetHClass(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HasTSSubtyping(EcmaRuntimeCallInfo *info);

    static JSTaggedValue IsNotHoleProperty(EcmaRuntimeCallInfo *info);

    static JSTaggedValue ForceFullGC(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HintGC(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HiddenStackSourceFile(EcmaRuntimeCallInfo *info);

    static JSTaggedValue RemoveAOTFlag(EcmaRuntimeCallInfo *info);

#if defined(ECMASCRIPT_SUPPORT_CPUPROFILER)
    static JSTaggedValue StartCpuProfiler(EcmaRuntimeCallInfo *info);

    static JSTaggedValue StopCpuProfiler(EcmaRuntimeCallInfo *info);

    static std::string GetProfileName();

    static bool CreateFile(std::string &fileName);
#endif
    // ArkTools.isPrototype(object)
    static JSTaggedValue IsPrototype(EcmaRuntimeCallInfo *info);

    // ArkTools.isAOTCompiledAssert(func)
    static JSTaggedValue IsAOTCompiled(EcmaRuntimeCallInfo *info);

    // ArkTools.GetElementsKind(array)
    static JSTaggedValue GetElementsKind(EcmaRuntimeCallInfo *info);

    static JSTaggedValue IsRegExpReplaceDetectorValid(EcmaRuntimeCallInfo *info);

    static JSTaggedValue IsSymbolIteratorDetectorValid(EcmaRuntimeCallInfo *info);

    static JSTaggedValue TimeInUs(EcmaRuntimeCallInfo *info);

    static JSTaggedValue PrepareFunctionForOptimization(EcmaRuntimeCallInfo *info);

    static JSTaggedValue OptimizeFunctionOnNextCall(EcmaRuntimeCallInfo *info);

    static JSTaggedValue OptimizeMaglevOnNextCall(EcmaRuntimeCallInfo *info);

    static JSTaggedValue DeoptimizeFunction(EcmaRuntimeCallInfo *info);

    static JSTaggedValue OptimizeOsr(EcmaRuntimeCallInfo *info);

    static JSTaggedValue NeverOptimizeFunction(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HeapObjectVerify(EcmaRuntimeCallInfo *info);

    static JSTaggedValue DisableOptimizationFinalization(EcmaRuntimeCallInfo *info);

    static JSTaggedValue DeoptimizeNow(EcmaRuntimeCallInfo *info);

    static JSTaggedValue WaitForBackgroundOptimization(EcmaRuntimeCallInfo *info);

    static JSTaggedValue Gc(EcmaRuntimeCallInfo *info);

    static JSTaggedValue PGOAssertType(EcmaRuntimeCallInfo *info);

    static JSTaggedValue ToLength(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HasHoleyElements(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HasDictionaryElements(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HasSmiElements(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HasDoubleElements(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HasObjectElements(EcmaRuntimeCallInfo *info);

    static JSTaggedValue ArrayBufferDetach(EcmaRuntimeCallInfo *info);

    static JSTaggedValue HaveSameMap(EcmaRuntimeCallInfo *info);

    static Span<const base::BuiltinFunctionEntry> GetArkToolsFunctions()
    {
        return Span<const base::BuiltinFunctionEntry>(ARK_TOOLS_FUNCTIONS);
    }

private:
#define BUILTINS_ARK_TOOLS_FUNCTION_ENTRY(name, method, length, id) \
    base::BuiltinFunctionEntry::Create(name, BuiltinsArkTools::method, length, kungfu::BuiltinsStubCSigns::id),

    static constexpr std::array ARK_TOOLS_FUNCTIONS  = {
        BUILTIN_ARK_TOOLS_FUNCTIONS(BUILTINS_ARK_TOOLS_FUNCTION_ENTRY)
    };
#undef BUILTINS_ARK_TOOLS_FUNCTION_ENTRY
};
}  // namespace panda::ecmascript::builtins

#endif  // ECMASCRIPT_BUILTINS_BUILTINS_ARK_TOOLS_H
