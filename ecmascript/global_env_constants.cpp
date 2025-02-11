/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "ecmascript/global_env_constants.h"

#include "ecmascript/accessor_data.h"
#include "ecmascript/builtins/builtins.h"
#include "ecmascript/builtins/builtins_global.h"
#include "ecmascript/ecma_string-inl.h"
#include "ecmascript/ecma_vm.h"
#include "ecmascript/free_object.h"
#include "ecmascript/global_env.h"
#include "ecmascript/global_env_constants-inl.h"
#include "ecmascript/ic/ic_handler.h"
#include "ecmascript/ic/property_box.h"
#include "ecmascript/ic/proto_change_details.h"
#include "ecmascript/jobs/micro_job_queue.h"
#include "ecmascript/jobs/pending_job.h"
#include "ecmascript/js_api/js_api_arraylist_iterator.h"
#include "ecmascript/js_api/js_api_deque_iterator.h"
#include "ecmascript/js_api/js_api_lightweightmap_iterator.h"
#include "ecmascript/js_api/js_api_lightweightset_iterator.h"
#include "ecmascript/js_api/js_api_linked_list_iterator.h"
#include "ecmascript/js_api/js_api_list_iterator.h"
#include "ecmascript/js_api/js_api_plain_array_iterator.h"
#include "ecmascript/js_api/js_api_queue_iterator.h"
#include "ecmascript/js_api/js_api_stack_iterator.h"
#include "ecmascript/js_api/js_api_hashmap_iterator.h"
#include "ecmascript/js_api/js_api_hashset_iterator.h"
#include "ecmascript/js_api/js_api_tree_map_iterator.h"
#include "ecmascript/js_api/js_api_tree_set_iterator.h"
#include "ecmascript/js_api/js_api_vector_iterator.h"
#include "ecmascript/js_async_generator_object.h"
#include "ecmascript/jspandafile/class_info_extractor.h"
#include "ecmascript/jspandafile/class_literal.h"
#include "ecmascript/jspandafile/program_object.h"
#include "ecmascript/js_arguments.h"
#include "ecmascript/js_array.h"
#include "ecmascript/js_array_iterator.h"
#include "ecmascript/js_arraybuffer.h"
#include "ecmascript/js_finalization_registry.h"
#include "ecmascript/js_for_in_iterator.h"
#include "ecmascript/js_generator_object.h"
#include "ecmascript/js_hclass.h"
#include "ecmascript/js_iterator.h"
#include "ecmascript/js_map.h"
#include "ecmascript/js_map_iterator.h"
#include "ecmascript/js_primitive_ref.h"
#include "ecmascript/js_promise.h"
#include "ecmascript/js_proxy.h"
#include "ecmascript/js_realm.h"
#include "ecmascript/js_regexp.h"
#include "ecmascript/js_regexp_iterator.h"
#include "ecmascript/js_set.h"
#include "ecmascript/js_set_iterator.h"
#include "ecmascript/js_symbol.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/marker_cell.h"
#include "ecmascript/method.h"
#include "ecmascript/module/js_module_source_text.h"
#include "ecmascript/object_factory.h"
#include "ecmascript/subtyping_operator.h"
#include "ecmascript/tagged_node.h"
#include "ecmascript/ts_types/ts_type.h"

namespace panda::ecmascript {
void GlobalEnvConstants::Init(JSThread *thread, JSHClass *hClass)
{
    InitRootsClass(thread, hClass);
    InitGlobalConstant(thread);
    InitGlobalCaches();
}

void GlobalEnvConstants::InitRootsClass(JSThread *thread, JSHClass *hClass)
{
    // Global constants are readonly.
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();

    SetConstant(ConstantIndex::HCLASS_CLASS_INDEX, JSTaggedValue(hClass));
    // To reverse the order, the hclass of string needs to load default supers
    SetConstant(ConstantIndex::ARRAY_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::TAGGED_ARRAY));
    SetConstant(ConstantIndex::DEFAULT_SUPERS_INDEX,
                WeakVector::Create(thread, SubtypingOperator::DEFAULT_SUPERS_CAPACITY, MemSpaceType::NON_MOVABLE));
    SetConstant(ConstantIndex::FREE_OBJECT_WITH_NONE_FIELD_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, FreeObject::NEXT_OFFSET, JSType::FREE_OBJECT_WITH_NONE_FIELD));
    SetConstant(ConstantIndex::FREE_OBJECT_WITH_ONE_FIELD_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, FreeObject::SIZE_OFFSET, JSType::FREE_OBJECT_WITH_ONE_FIELD));
    SetConstant(ConstantIndex::FREE_OBJECT_WITH_TWO_FIELD_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, FreeObject::SIZE, JSType::FREE_OBJECT_WITH_TWO_FIELD));
    SetConstant(ConstantIndex::LINE_STRING_CLASS_INDEX, factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::LINE_STRING));
    SetConstant(ConstantIndex::SLICED_STRING_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::SLICED_STRING));
    SetConstant(ConstantIndex::CONSTANT_STRING_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::CONSTANT_STRING));
    SetConstant(ConstantIndex::TREE_STRING_CLASS_INDEX, factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::TREE_STRING));
    SetConstant(ConstantIndex::BYTE_ARRAY_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::BYTE_ARRAY));
    SetConstant(ConstantIndex::CONSTANT_POOL_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::CONSTANT_POOL));
    SetConstant(ConstantIndex::PROFILE_TYPE_INFO_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::PROFILE_TYPE_INFO));
    SetConstant(ConstantIndex::AOT_LITERAL_INFO_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::AOT_LITERAL_INFO));
    SetConstant(ConstantIndex::VTABLE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::VTABLE));
    SetConstant(ConstantIndex::MUTANT_TAGGED_ARRAY_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::MUTANT_TAGGED_ARRAY));
    InitGlobalConstantSpecial(thread);
    SetConstant(ConstantIndex::DICTIONARY_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::TAGGED_DICTIONARY));
    SetConstant(ConstantIndex::COW_ARRAY_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::COW_TAGGED_ARRAY));
    SetConstant(ConstantIndex::BIGINT_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, BigInt::SIZE, JSType::BIGINT));
    SetConstant(ConstantIndex::JS_NATIVE_POINTER_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, JSNativePointer::SIZE, JSType::JS_NATIVE_POINTER));
    SetConstant(ConstantIndex::ENV_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::LEXICAL_ENV));
    SetConstant(ConstantIndex::SYMBOL_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, JSSymbol::SIZE, JSType::SYMBOL));
    SetConstant(ConstantIndex::ACCESSOR_DATA_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, AccessorData::SIZE, JSType::ACCESSOR_DATA));
    SetConstant(ConstantIndex::INTERNAL_ACCESSOR_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, AccessorData::SIZE, JSType::INTERNAL_ACCESSOR));
    SetConstant(ConstantIndex::JS_PROXY_ORDINARY_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSProxy::SIZE, JSType::JS_PROXY));
    SetConstant(ConstantIndex::COMPLETION_RECORD_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, CompletionRecord::SIZE, JSType::COMPLETION_RECORD));
    SetConstant(ConstantIndex::GENERATOR_CONTEST_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, GeneratorContext::SIZE, JSType::JS_GENERATOR_CONTEXT));
    SetConstant(ConstantIndex::ASYNC_GENERATOR_REQUEST_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, AsyncGeneratorRequest::SIZE,
                                                 JSType::ASYNC_GENERATOR_REQUEST));
    SetConstant(ConstantIndex::ASYNC_ITERATOR_RECORD_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, AsyncIteratorRecord::SIZE, JSType::ASYNC_ITERATOR_RECORD));
    SetConstant(ConstantIndex::CAPABILITY_RECORD_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, PromiseCapability::SIZE, JSType::PROMISE_CAPABILITY));
    SetConstant(ConstantIndex::REACTIONS_RECORD_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, PromiseReaction::SIZE, JSType::PROMISE_REACTIONS));
    SetConstant(ConstantIndex::PROMISE_ITERATOR_RECORD_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, PromiseIteratorRecord::SIZE,
                                                 JSType::PROMISE_ITERATOR_RECORD));
    SetConstant(ConstantIndex::PROMISE_RECORD_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, PromiseRecord::SIZE, JSType::PROMISE_RECORD));
    SetConstant(ConstantIndex::PROMISE_RESOLVING_FUNCTIONS_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, ResolvingFunctionsRecord::SIZE,
                                                 JSType::RESOLVING_FUNCTIONS_RECORD));
    SetConstant(ConstantIndex::MICRO_JOB_QUEUE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, job::MicroJobQueue::SIZE, JSType::MICRO_JOB_QUEUE));
    SetConstant(ConstantIndex::PENDING_JOB_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, job::PendingJob::SIZE, JSType::PENDING_JOB));
    SetConstant(ConstantIndex::PROTO_CHANGE_MARKER_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, ProtoChangeMarker::SIZE, JSType::PROTO_CHANGE_MARKER));
    SetConstant(ConstantIndex::PROTO_CHANGE_DETAILS_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, ProtoChangeDetails::SIZE, JSType::PROTOTYPE_INFO));
    SetConstant(ConstantIndex::MARKER_CELL_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, MarkerCell::SIZE, JSType::MARKER_CELL));
    SetConstant(ConstantIndex::TRACK_INFO_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TrackInfo::SIZE, JSType::TRACK_INFO));
    SetConstant(ConstantIndex::PROTOTYPE_HANDLER_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, PrototypeHandler::SIZE, JSType::PROTOTYPE_HANDLER));
    SetConstant(ConstantIndex::TRANSITION_HANDLER_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TransitionHandler::SIZE, JSType::TRANSITION_HANDLER));
    SetConstant(ConstantIndex::TRANS_WITH_PROTO_HANDLER_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TransWithProtoHandler::SIZE, JSType::TRANS_WITH_PROTO_HANDLER));
    SetConstant(ConstantIndex::STORE_TS_HANDLER_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, StoreTSHandler::SIZE, JSType::STORE_TS_HANDLER));
    SetConstant(ConstantIndex::PROPERTY_BOX_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, PropertyBox::SIZE, JSType::PROPERTY_BOX));
    SetConstant(ConstantIndex::PROGRAM_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, Program::SIZE, JSType::PROGRAM));
    SetConstant(
        ConstantIndex::IMPORT_ENTRY_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, ImportEntry::SIZE, JSType::IMPORTENTRY_RECORD));
    SetConstant(
        ConstantIndex::LOCAL_EXPORT_ENTRY_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, LocalExportEntry::SIZE, JSType::LOCAL_EXPORTENTRY_RECORD));
    SetConstant(
        ConstantIndex::INDIRECT_EXPORT_ENTRY_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, IndirectExportEntry::SIZE,
                                       JSType::INDIRECT_EXPORTENTRY_RECORD));
    SetConstant(
        ConstantIndex::STAR_EXPORT_ENTRY_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, StarExportEntry::SIZE, JSType::STAR_EXPORTENTRY_RECORD));
    SetConstant(
        ConstantIndex::SOURCE_TEXT_MODULE_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, SourceTextModule::SIZE, JSType::SOURCE_TEXT_MODULE_RECORD));
    SetConstant(
        ConstantIndex::RESOLVED_BINDING_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, ResolvedBinding::SIZE, JSType::RESOLVEDBINDING_RECORD));
    SetConstant(
        ConstantIndex::RESOLVED_INDEX_BINDING_CLASS_INDEX,
        factory->NewEcmaReadOnlyHClass(hClass, ResolvedIndexBinding::SIZE, JSType::RESOLVEDINDEXBINDING_RECORD));

    JSHClass *jsProxyCallableClass = *factory->NewEcmaHClass(hClass, JSProxy::SIZE, JSType::JS_PROXY);

    jsProxyCallableClass->SetCallable(true);
    SetConstant(ConstantIndex::JS_PROXY_CALLABLE_CLASS_INDEX, JSTaggedValue(jsProxyCallableClass));

    JSHClass *jsProxyConstructClass = *factory->NewEcmaHClass(hClass, JSProxy::SIZE, JSType::JS_PROXY);

    jsProxyConstructClass->SetCallable(true);
    jsProxyConstructClass->SetConstructor(true);
    SetConstant(ConstantIndex::JS_PROXY_CONSTRUCT_CLASS_INDEX, JSTaggedValue(jsProxyConstructClass));

    SetConstant(ConstantIndex::JS_REALM_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSRealm::SIZE, JSType::JS_REALM));
    SetConstant(ConstantIndex::MACHINE_CODE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, 0, JSType::MACHINE_CODE_OBJECT));
    SetConstant(ConstantIndex::CLASS_INFO_EXTRACTOR_HCLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, ClassInfoExtractor::SIZE,
                                                 JSType::CLASS_INFO_EXTRACTOR));
    SetConstant(ConstantIndex::TS_OBJECT_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSObjectType::SIZE, JSType::TS_OBJECT_TYPE));
    SetConstant(ConstantIndex::TS_CLASS_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSClassType::SIZE, JSType::TS_CLASS_TYPE));
    SetConstant(ConstantIndex::TS_UNION_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSUnionType::SIZE, JSType::TS_UNION_TYPE));
    SetConstant(ConstantIndex::TS_INTERFACE_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSInterfaceType::SIZE, JSType::TS_INTERFACE_TYPE));
    SetConstant(ConstantIndex::TS_CLASS_INSTANCE_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSClassInstanceType::SIZE,
                                                 JSType::TS_CLASS_INSTANCE_TYPE));
    SetConstant(ConstantIndex::TS_FUNCTION_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSFunctionType::SIZE, JSType::TS_FUNCTION_TYPE));
    SetConstant(ConstantIndex::TS_ARRAY_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSArrayType::SIZE, JSType::TS_ARRAY_TYPE));
    SetConstant(ConstantIndex::TS_ITERATOR_INSTANCE_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSIteratorInstanceType::SIZE,
                JSType::TS_ITERATOR_INSTANCE_TYPE));
    SetConstant(ConstantIndex::TS_NAMESPACE_TYPE_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, TSNamespaceType::SIZE, JSType::TS_NAMESPACE_TYPE));
    SetConstant(ConstantIndex::JS_REGEXP_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSRegExpIterator::SIZE, JSType::JS_REG_EXP_ITERATOR));
    SetConstant(ConstantIndex::JS_SET_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSSetIterator::SIZE, JSType::JS_SET_ITERATOR, 0)); // 0: no inlined props
    SetConstant(ConstantIndex::JS_MAP_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSMapIterator::SIZE, JSType::JS_MAP_ITERATOR, 0)); // 0: no inlined props
    SetConstant(ConstantIndex::JS_ARRAY_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSArrayIterator::SIZE, JSType::JS_ARRAY_ITERATOR, 0));
    SetConstant(
        ConstantIndex::JS_API_ARRAYLIST_ITERATOR_CLASS_INDEX,
        factory->NewEcmaHClass(hClass, JSAPIArrayListIterator::SIZE, JSType::JS_API_ARRAYLIST_ITERATOR));
    SetConstant(ConstantIndex::JS_API_DEQUE_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPIDequeIterator::SIZE, JSType::JS_API_DEQUE_ITERATOR));
    SetConstant(ConstantIndex::JS_API_LIGHTWEIGHTMAP_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPILightWeightMapIterator::SIZE,
                JSType::JS_API_LIGHT_WEIGHT_MAP_ITERATOR));
    SetConstant(ConstantIndex::JS_API_LIGHTWEIGHTSET_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPILightWeightSetIterator::SIZE,
                JSType::JS_API_LIGHT_WEIGHT_SET_ITERATOR));
    SetConstant(
        ConstantIndex::JS_API_LINKED_LIST_ITERATOR_CLASS_INDEX,
        factory->NewEcmaHClass(hClass, JSAPILinkedListIterator::SIZE, JSType::JS_API_LINKED_LIST_ITERATOR));
    SetConstant(ConstantIndex::JS_API_LIST_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPIListIterator::SIZE, JSType::JS_API_LIST_ITERATOR));
    SetConstant(
        ConstantIndex::JS_API_PLAIN_ARRAY_ITERATOR_CLASS_INDEX,
        factory->NewEcmaHClass(hClass, JSAPIPlainArrayIterator::SIZE, JSType::JS_API_PLAIN_ARRAY_ITERATOR));
    SetConstant(ConstantIndex::JS_API_QUEUE_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPIQueueIterator::SIZE, JSType::JS_API_QUEUE_ITERATOR));
    SetConstant(ConstantIndex::JS_API_STACK_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPIStackIterator::SIZE, JSType::JS_API_STACK_ITERATOR));
    SetConstant(ConstantIndex::JS_API_VECTOR_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPIVectorIterator::SIZE, JSType::JS_API_VECTOR_ITERATOR));
    SetConstant(ConstantIndex::JS_API_HASH_MAP_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPIHashMapIterator::SIZE, JSType::JS_API_HASHMAP_ITERATOR));
    SetConstant(ConstantIndex::JS_API_HASH_SET_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPIHashSetIterator::SIZE, JSType::JS_API_HASHSET_ITERATOR));
    SetConstant(ConstantIndex::JS_API_TREE_MAP_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPITreeMapIterator::SIZE, JSType::JS_API_TREEMAP_ITERATOR));
    SetConstant(ConstantIndex::JS_API_TREE_SET_ITERATOR_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, JSAPITreeSetIterator::SIZE, JSType::JS_API_TREESET_ITERATOR));
    SetConstant(ConstantIndex::LINKED_NODE_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, LinkedNode::SIZE, JSType::LINKED_NODE));
    SetConstant(ConstantIndex::RB_TREENODE_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, RBTreeNode::SIZE, JSType::RB_TREENODE));
    SetConstant(ConstantIndex::CELL_RECORD_CLASS_INDEX,
                factory->NewEcmaReadOnlyHClass(hClass, CellRecord::SIZE, JSType::CELL_RECORD));
    SetConstant(ConstantIndex::OBJECT_HCLASS_INDEX, factory->NewEcmaHClass(JSObject::SIZE, JSType::JS_OBJECT));
    SetConstant(ConstantIndex::METHOD_CLASS_INDEX,
                factory->NewEcmaHClass(hClass, Method::SIZE, JSType::METHOD));
    SetConstant(ConstantIndex::CLASS_LITERAL_HCLASS_INDEX,
                factory->NewEcmaHClass(hClass, ClassLiteral::SIZE, JSType::CLASS_LITERAL));
}

void GlobalEnvConstants::InitGlobalConstantSpecial(JSThread *thread)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    // SPECIAL INIT
    SetConstant(ConstantIndex::UNDEFINED_INDEX, JSTaggedValue::Undefined());
    SetConstant(ConstantIndex::NULL_INDEX, JSTaggedValue::Null());
    SetConstant(ConstantIndex::TRUE_INDEX, JSTaggedValue::True());
    SetConstant(ConstantIndex::FALSE_INDEX, JSTaggedValue::False());
    auto vm = thread->GetEcmaVM();
    SetConstant(ConstantIndex::EMPTY_STRING_OBJECT_INDEX, JSTaggedValue(EcmaStringAccessor::CreateEmptyString(vm)));
    SetConstant(ConstantIndex::EMPTY_ARRAY_OBJECT_INDEX, factory->NewEmptyArray());
    SetConstant(ConstantIndex::EMPTY_MUTANT_ARRAY_OBJECT_INDEX, factory->NewEmptyMutantArray());
    SetConstant(ConstantIndex::EMPTY_LAYOUT_INFO_OBJECT_INDEX, factory->CreateLayoutInfo(0));
    SetConstant(ConstantIndex::EMPTY_TAGGED_QUEUE_OBJECT_INDEX, factory->NewTaggedQueue(0));
}

// NOLINTNEXTLINE(readability-function-size)
void GlobalEnvConstants::InitGlobalConstant(JSThread *thread)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    /* non ECMA standard jsapi containers iterators, init to Undefined first */
    InitJSAPIContainers();

#define INIT_GLOBAL_ENV_CONSTANT_STRING(Name, Index, Token) \
    SetConstant(ConstantIndex::Index, factory->NewFromASCIINonMovable(Token));

    GLOBAL_ENV_CONSTANT_STRING(INIT_GLOBAL_ENV_CONSTANT_STRING)
#undef INIT_GLOBAL_ENV_CONSTANT_STRING

    auto accessor = factory->NewInternalAccessor(reinterpret_cast<void *>(JSFunction::PrototypeSetter),
                                                 reinterpret_cast<void *>(JSFunction::PrototypeGetter));
    SetConstant(ConstantIndex::FUNCTION_PROTOTYPE_ACCESSOR, accessor);
    accessor = factory->NewInternalAccessor(nullptr, reinterpret_cast<void *>(JSFunction::NameGetter));
    SetConstant(ConstantIndex::FUNCTION_NAME_ACCESSOR, accessor);
    accessor = factory->NewInternalAccessor(nullptr, reinterpret_cast<void *>(JSFunction::LengthGetter));
    SetConstant(ConstantIndex::FUNCTION_LENGTH_ACCESSOR, accessor);
    accessor = factory->NewInternalAccessor(reinterpret_cast<void *>(JSArray::LengthSetter),
                                            reinterpret_cast<void *>(JSArray::LengthGetter));
    SetConstant(ConstantIndex::ARRAY_LENGTH_ACCESSOR, accessor);
    SetConstant(ConstantIndex::CLASS_PROTOTYPE_HCLASS_INDEX,
                factory->CreateDefaultClassPrototypeHClass(JSHClass::Cast(GetHClassClass().GetTaggedObject())));
    SetConstant(ConstantIndex::CLASS_CONSTRUCTOR_HCLASS_INDEX,
                factory->CreateDefaultClassConstructorHClass(JSHClass::Cast(GetHClassClass().GetTaggedObject())));
}

void GlobalEnvConstants::InitGlobalCaches()
{
    SetConstant(ConstantIndex::CACHED_JSCOLLATOR_LOCALES_INDEX, JSTaggedValue::Undefined());
}

void GlobalEnvConstants::SetCachedLocales(JSTaggedValue value)
{
    JSTaggedValue cached = GetCachedJSCollatorLocales();
    if (cached.IsUndefined()) {
        SetConstant(ConstantIndex::CACHED_JSCOLLATOR_LOCALES_INDEX, value);
    }
}

void GlobalEnvConstants::InitJSAPIContainers()
{
    for (size_t i = GetJSAPIContainersBegin(); i <= GetJSAPIContainersEnd(); i++) {
        SetConstant(static_cast<ConstantIndex>(i), JSTaggedValue::Undefined());
    }
}

void GlobalEnvConstants::InitSpecialForSnapshot()
{
    SetConstant(ConstantIndex::UNDEFINED_INDEX, JSTaggedValue::Undefined());
    SetConstant(ConstantIndex::NULL_INDEX, JSTaggedValue::Null());
    InitJSAPIContainers();
}

void GlobalEnvConstants::InitElementKindHClass(const JSThread *thread, JSHandle<JSHClass> originHClass)
{
    auto map = thread->GetArrayHClassIndexMap();
    for (auto iter : map) {
        JSHandle<JSHClass> hclass = originHClass;
        if (iter.first != ElementsKind::GENERIC) {
            hclass = JSHClass::Clone(thread, originHClass);
            hclass->SetElementsKind(iter.first);
        }
        SetConstant(iter.second, hclass);
    }
}
}  // namespace panda::ecmascript
