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

#ifndef ECMASCRIPT_MODULE_JS_MODULE_SOURCE_TEXT_H
#define ECMASCRIPT_MODULE_JS_MODULE_SOURCE_TEXT_H

#include "ecmascript/base/string_helper.h"
#include "ecmascript/mem/c_containers.h"
#include "ecmascript/module/js_module_record.h"
#include "ecmascript/module/js_module_entry.h"
#include "ecmascript/tagged_array.h"

namespace panda::ecmascript {
enum class ModuleStatus : uint8_t {
    UNINSTANTIATED = 0x01,
    INSTANTIATING,
    INSTANTIATED,
    EVALUATING,
    EVALUATING_ASYNC,
    EVALUATED
};

enum class ModuleTypes : uint8_t {
    ECMA_MODULE = 0x01,
    CJS_MODULE,
    JSON_MODULE,
    NATIVE_MODULE,
    OHOS_MODULE,
    APP_MODULE,
    INTERNAL_MODULE,
    UNKNOWN
};

enum class LoadingTypes : uint8_t {
    STABLE_MODULE = 0x01,
    DYNAMITC_MODULE,
    OTHERS
};

class SourceTextModule final : public ModuleRecord {
public:
    static constexpr int UNDEFINED_INDEX = -1;
    static constexpr int MODULE_ERROR = 1;
    static constexpr size_t DEFAULT_DICTIONART_CAPACITY = 2;
    static constexpr size_t DEFAULT_ARRAY_CAPACITY = 2;
    static constexpr uint8_t DEREGISTER_MODULE_TAG = 1;
    static constexpr uint32_t FIRST_ASYNC_EVALUATING_ORDINAL = 2;
    static constexpr uint32_t NOT_ASYNC_EVALUATED = 0;
    static constexpr uint32_t ASYNC_EVALUATE_DID_FINISH = 1;
    struct AsyncEvaluatingOrdinalCompare {
        bool operator()(const JSHandle<SourceTextModule> &lhs, const JSHandle<SourceTextModule> &rhs) const
        {
            return lhs->GetAsyncEvaluatingOrdinal() < rhs->GetAsyncEvaluatingOrdinal();
        }
    };
    using AsyncParentCompletionSet =
      CSet<JSHandle<SourceTextModule>, AsyncEvaluatingOrdinalCompare>;

    CAST_CHECK(SourceTextModule, IsSourceTextModule);

    // 15.2.1.17 Runtime Semantics: HostResolveImportedModule ( referencingModule, specifier )
    static JSHandle<JSTaggedValue> HostResolveImportedModule(JSThread *thread,
                                                             const JSHandle<SourceTextModule> &module,
                                                             const JSHandle<JSTaggedValue> &moduleRequest);
    static JSHandle<JSTaggedValue> HostResolveImportedModuleWithMerge(JSThread *thread,
                                                                      const JSHandle<SourceTextModule> &module,
                                                                      const JSHandle<JSTaggedValue> &moduleRequest);

    // 15.2.1.16.2 GetExportedNames(exportStarSet)
    static CVector<std::string> GetExportedNames(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                                 const JSHandle<TaggedArray> &exportStarSet);

    // 15.2.1.16.3 ResolveExport(exportName, resolveVector)
    static JSHandle<JSTaggedValue> ResolveExport(JSThread *thread, const JSHandle<SourceTextModule> &module,
        const JSHandle<JSTaggedValue> &exportName,
        CVector<std::pair<JSHandle<SourceTextModule>, JSHandle<JSTaggedValue>>> &resolveVector);
    static JSHandle<JSTaggedValue> ResolveExportObject(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                                       const JSHandle<JSTaggedValue> &exportObject,
                                                       const JSHandle<JSTaggedValue> &exportName);
    // 15.2.1.16.4.1 InnerModuleInstantiation ( module, stack, index )
    static int InnerModuleInstantiation(JSThread *thread,
        const JSHandle<ModuleRecord> &moduleRecord, CVector<JSHandle<SourceTextModule>> &stack,
        int index, bool excuteFromJob = false);

    // 15.2.1.16.4.2 ModuleDeclarationEnvironmentSetup ( module )
    static void ModuleDeclarationEnvironmentSetup(JSThread *thread, const JSHandle<SourceTextModule> &module);
    static void ModuleDeclarationArrayEnvironmentSetup(JSThread *thread, const JSHandle<SourceTextModule> &module);

    // 15.2.1.16.5.1 InnerModuleEvaluation ( module, stack, index )
    static int InnerModuleEvaluation(JSThread *thread, const JSHandle<ModuleRecord> &moduleRecord,
        CVector<JSHandle<SourceTextModule>> &stack, int index, const void *buffer = nullptr,
        size_t size = 0, bool excuteFromJob = false);

    // 15.2.1.16.5.2 ModuleExecution ( module )
    static Expected<JSTaggedValue, bool> ModuleExecution(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                 const void *buffer = nullptr, size_t size = 0, bool excuteFromJob = false);
 
    // 16.2.1.5.3.2 ExecuteAsyncModule ( module )
    static void ExecuteAsyncModule(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                   const void *buffer = nullptr, size_t size = 0, bool excuteFromJob = false);

    // 16.2.1.5.3.3 GatherAvailableAncestors ( module, execList )
    static void GatherAvailableAncestors(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                         AsyncParentCompletionSet &execList);

    // 16.2.1.5.3.4 AsyncModuleExecutionFulfilled ( module )
    static void AsyncModuleExecutionFulfilled(JSThread *thread, const JSHandle<SourceTextModule> &module);

    // 16.2.1.5.3.5 AsyncModuleExecutionRejected ( module, error )
    static void AsyncModuleExecutionRejected(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                             JSTaggedValue error);

    static JSTaggedValue AsyncModuleFulfilledFunc(EcmaRuntimeCallInfo *argv);
    static JSTaggedValue AsyncModuleRejectedFunc(EcmaRuntimeCallInfo *argv);
    static void AddAsyncParentModule(JSThread *thread, JSHandle<SourceTextModule> &module,
                                     JSHandle<SourceTextModule> &parent);
    // 15.2.1.18 Runtime Semantics: GetModuleNamespace ( module )
    static JSHandle<JSTaggedValue> GetModuleNamespace(JSThread *thread, const JSHandle<SourceTextModule> &module);

    static void AddImportEntry(JSThread *thread, const JSHandle<SourceTextModule> &module,
                               const JSHandle<ImportEntry> &importEntry, size_t idx, uint32_t len);
    static void AddLocalExportEntry(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                    const JSHandle<LocalExportEntry> &exportEntry, size_t idx, uint32_t len);
    static void AddIndirectExportEntry(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                       const JSHandle<IndirectExportEntry> &exportEntry, size_t idx, uint32_t len);
    static void AddStarExportEntry(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                   const JSHandle<StarExportEntry> &exportEntry, size_t idx, uint32_t len);
    static std::pair<bool, ModuleTypes> CheckNativeModule(const CString &moduleRequestName);
    static Local<JSValueRef> GetRequireNativeModuleFunc(EcmaVM *vm, ModuleTypes moduleType);
    static void MakeAppArgs(const EcmaVM *vm, std::vector<Local<JSValueRef>> &arguments, const CString &moduleName);
    static void MakeInternalArgs(const EcmaVM *vm, std::vector<Local<JSValueRef>> &arguments,
                                 const CString &moduleRequestName);
    static bool LoadNativeModule(JSThread *thread, JSHandle<SourceTextModule> &requiredModule,
                                 ModuleTypes moduleType);
    inline static bool IsNativeModule(ModuleTypes moduleType)
    {
        return moduleType == ModuleTypes::OHOS_MODULE ||
               moduleType == ModuleTypes::APP_MODULE ||
               moduleType == ModuleTypes::NATIVE_MODULE ||
               moduleType == ModuleTypes::INTERNAL_MODULE;
    }

    static constexpr size_t SOURCE_TEXT_MODULE_OFFSET = ModuleRecord::SIZE;
    ACCESSORS(Environment, SOURCE_TEXT_MODULE_OFFSET, NAMESPACE_OFFSET);
    ACCESSORS(Namespace, NAMESPACE_OFFSET, ECMA_MODULE_FILENAME);
    ACCESSORS(EcmaModuleFilename, ECMA_MODULE_FILENAME, ECMA_MODULE_RECORDNAME);
    ACCESSORS(EcmaModuleRecordName, ECMA_MODULE_RECORDNAME, REQUESTED_MODULES_OFFSET);
    ACCESSORS(RequestedModules, REQUESTED_MODULES_OFFSET, IMPORT_ENTRIES_OFFSET);
    ACCESSORS(ImportEntries, IMPORT_ENTRIES_OFFSET, LOCAL_EXPORT_ENTTRIES_OFFSET);
    ACCESSORS(LocalExportEntries, LOCAL_EXPORT_ENTTRIES_OFFSET, INDIRECT_EXPORT_ENTTRIES_OFFSET);
    ACCESSORS(IndirectExportEntries, INDIRECT_EXPORT_ENTTRIES_OFFSET, START_EXPORT_ENTTRIES_OFFSET);
    ACCESSORS(StarExportEntries, START_EXPORT_ENTTRIES_OFFSET, NAME_DICTIONARY_OFFSET);
    ACCESSORS(NameDictionary, NAME_DICTIONARY_OFFSET, CYCLE_ROOT_OFFSET);
    ACCESSORS(CycleRoot, CYCLE_ROOT_OFFSET, TOP_LEVEL_CAPABILITY_OFFSET);
    ACCESSORS(TopLevelCapability, TOP_LEVEL_CAPABILITY_OFFSET, ASYNC_PARENT_MODULES_OFFSET);
    ACCESSORS(AsyncParentModules, ASYNC_PARENT_MODULES_OFFSET, EVALUATION_ERROR_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(EvaluationError, int32_t, EVALUATION_ERROR_OFFSET, DFS_ANCESTOR_INDEX_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(DFSAncestorIndex, int32_t, DFS_ANCESTOR_INDEX_OFFSET, DFS_INDEX_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(DFSIndex, int32_t, DFS_INDEX_OFFSET, ASYNC_EVALUATION_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(AsyncEvaluatingOrdinal, uint32_t, ASYNC_EVALUATION_OFFSET, PENDING_DEPENDENCIES_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(PendingAsyncDependencies, int32_t, PENDING_DEPENDENCIES_OFFSET, BIT_FIELD_OFFSET);
    ACCESSORS_BIT_FIELD(BitField, BIT_FIELD_OFFSET, LAST_OFFSET)

    DEFINE_ALIGN_SIZE(LAST_OFFSET);

    // define BitField
    static constexpr size_t STATUS_BITS = 3;
    static constexpr size_t MODULE_TYPE_BITS = 4;
    static constexpr size_t IS_NEW_BC_VERSION_BITS = 1;
    static constexpr size_t HASTLA_BITS = 1;
    static constexpr size_t LOADING_TYPE_BITS = 3;
    static constexpr uint16_t REGISTER_COUNTS = 16;

    FIRST_BIT_FIELD(BitField, Status, ModuleStatus, STATUS_BITS)
    NEXT_BIT_FIELD(BitField, Types, ModuleTypes, MODULE_TYPE_BITS, Status)
    NEXT_BIT_FIELD(BitField, IsNewBcVersion, bool, IS_NEW_BC_VERSION_BITS, Types)
    NEXT_BIT_FIELD(BitField, HasTLA, bool, HASTLA_BITS, IsNewBcVersion)
    NEXT_BIT_FIELD(BitField, LoadingTypes, LoadingTypes, LOADING_TYPE_BITS, HasTLA)
    NEXT_BIT_FIELD(BitField, RegisterCounts, uint16_t, REGISTER_COUNTS, LoadingTypes)

    DECL_DUMP()
    DECL_VISIT_OBJECT(SOURCE_TEXT_MODULE_OFFSET, EVALUATION_ERROR_OFFSET)

    // 15.2.1.16.5 Evaluate()
    static JSTaggedValue Evaluate(JSThread *thread, const JSHandle<SourceTextModule> &module,
                         const void *buffer = nullptr, size_t size = 0, bool excuteFromJob = false);

    // 15.2.1.16.4 Instantiate()
    static int Instantiate(JSThread *thread, const JSHandle<JSTaggedValue> &moduleHdl,
        bool excuteFromJob = false);
    static void InstantiateCJS(JSThread *thread, const JSHandle<SourceTextModule> &currentModule,
                               const JSHandle<SourceTextModule> &requiredModule);
    static void InstantiateNativeModule(JSThread *thread, JSHandle<SourceTextModule> &currentModule,
                                        JSHandle<SourceTextModule> &requiredModule,
                                        const JSHandle<JSTaggedValue> &moduleRequest,
                                        ModuleTypes moduleType);

    JSTaggedValue GetModuleValue(JSThread *thread, int32_t index, bool isThrow);
    void StoreModuleValue(JSThread *thread, int32_t index, const JSHandle<JSTaggedValue> &value);

    JSTaggedValue GetModuleValue(JSThread *thread, JSTaggedValue key, bool isThrow);
    void StoreModuleValue(JSThread *thread, const JSHandle<JSTaggedValue> &key, const JSHandle<JSTaggedValue> &value);
    static JSHandle<JSTaggedValue> ResolveIndirectExport(JSThread *thread, const JSHandle<JSTaggedValue> &exportEntry,
                                                         const JSHandle<JSTaggedValue> &exportName,
                                                         const JSHandle<SourceTextModule> &module,
                                                         CVector<std::pair<JSHandle<SourceTextModule>,
                                                         JSHandle<JSTaggedValue>>> &resolveVector);
    static JSTaggedValue GetModuleName(JSTaggedValue currentModule);

    static bool IsDynamicModule(LoadingTypes types);

    // taskpool
    static std::optional<std::set<uint32_t>> GetConcurrentRequestedModules(const JSHandle<Method> &method);
    static int InstantiateForConcurrent(JSThread *thread, const JSHandle<JSTaggedValue> &moduleHdl,
                                        const JSHandle<Method> &method);
    static int ModuleInstantiation(JSThread *thread, const JSHandle<ModuleRecord> &moduleRecord,
                                   CVector<JSHandle<SourceTextModule>> &stack, int index,
                                   const JSHandle<Method> &method);
    static int EvaluateForConcurrent(JSThread *thread, const JSHandle<SourceTextModule> &module,
                                     const JSHandle<Method> &method);
    static int ModuleEvaluation(JSThread *thread, const JSHandle<ModuleRecord> &moduleRecord,
                                int index, const JSHandle<Method> &method);

private:
    static void SetExportName(JSThread *thread,
                              const JSHandle<JSTaggedValue> &moduleRequest, const JSHandle<SourceTextModule> &module,
                              CVector<std::string> &exportedNames, JSHandle<TaggedArray> &newExportStarSet);
    static JSHandle<JSTaggedValue> GetStarResolution(JSThread *thread, const JSHandle<JSTaggedValue> &exportName,
                                                     const JSHandle<JSTaggedValue> &moduleRequest,
                                                     const JSHandle<SourceTextModule> &module,
                                                     JSMutableHandle<JSTaggedValue> &starResolution,
                                                     CVector<std::pair<JSHandle<SourceTextModule>,
                                                     JSHandle<JSTaggedValue>>> &resolveVector);
    template <typename T>
    static void AddExportName(JSThread *thread, const JSTaggedValue &exportEntry, CVector<std::string> &exportedNames);
    static JSHandle<JSTaggedValue> ResolveLocalExport(JSThread *thread, const JSHandle<JSTaggedValue> &exportEntry,
                                                      const JSHandle<JSTaggedValue> &exportName,
                                                      const JSHandle<SourceTextModule> &module);
    static JSHandle<JSTaggedValue> ResolveElementOfObject(JSThread *thread,
                                                         const JSHandle<JSHClass> &hclass,
                                                         const JSHandle<JSTaggedValue> &exportName,
                                                         const JSHandle<SourceTextModule> &module);
    static bool CheckCircularImport(const JSHandle<SourceTextModule> &module,
        const JSHandle<JSTaggedValue> &exportName,
        CVector<std::pair<JSHandle<SourceTextModule>, JSHandle<JSTaggedValue>>> &resolveVector);
    static void InitializeEnvironment(JSThread *thread, const JSHandle<SourceTextModule> &currentModule,
        JSHandle<JSTaggedValue> &moduleName, JSHandle<JSTaggedValue> &exports, bool isBundle);

    static void CheckResolvedBinding(JSThread *thread, const JSHandle<SourceTextModule> &module);
    static void CheckResolvedIndexBinding(JSThread *thread, const JSHandle<SourceTextModule> &module);
    static JSTaggedValue FindByExport(const JSTaggedValue &exportEntriesTv, const JSTaggedValue &key,
                                      const JSTaggedValue &dictionary);
    static JSHandle<SourceTextModule> GetModuleFromBinding(JSThread *thread, const JSTaggedValue &JSTaggedValue);
    static void DFSModuleInstantiation(JSHandle<SourceTextModule> &module,
                                       CVector<JSHandle<SourceTextModule>> &stack);
    static std::optional<int> HandleInnerModuleInstantiation(JSThread *thread,
                                                             JSHandle<SourceTextModule> &module,
                                                             JSMutableHandle<JSTaggedValue> &required,
                                                             CVector<JSHandle<SourceTextModule>> &stack,
                                                             int &index, bool excuteFromJob);
    static int HandleInstantiateException(JSHandle<SourceTextModule> &module,
                                          const CVector<JSHandle<SourceTextModule>> &stack, int result);
    static void HandleEvaluateResult(JSThread *thread, JSHandle<SourceTextModule> &module,
                                     JSHandle<PromiseCapability> &capability,
                                     const CVector<JSHandle<SourceTextModule>> &stack, int result);
    static void HandleConcurrentEvaluateResult(JSThread *thread, JSHandle<SourceTextModule> &module,
                                     const CVector<JSHandle<SourceTextModule>> &stack, int result);
    bool IsAsyncEvaluating();
};

class ResolvedBinding final : public Record {
public:
    CAST_CHECK(ResolvedBinding, IsResolvedBinding);

    static constexpr size_t MODULE_OFFSET = Record::SIZE;
    ACCESSORS(Module, MODULE_OFFSET, BINDING_NAME_OFFSET);
    ACCESSORS(BindingName, BINDING_NAME_OFFSET, SIZE);

    DECL_DUMP()
    DECL_VISIT_OBJECT(MODULE_OFFSET, SIZE)
};
class ResolvedIndexBinding final : public Record {
public:
    CAST_CHECK(ResolvedIndexBinding, IsResolvedIndexBinding);

    static constexpr size_t MODULE_OFFSET = Record::SIZE;
    ACCESSORS(Module, MODULE_OFFSET, INDEX_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(Index, int32_t, INDEX_OFFSET, END_OFFSET);
    DEFINE_ALIGN_SIZE(END_OFFSET);

    DECL_DUMP()
    DECL_VISIT_OBJECT(MODULE_OFFSET, INDEX_OFFSET)
};
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_MODULE_JS_MODULE_SOURCE_TEXT_H
