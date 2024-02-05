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

#ifndef ECMASCRIPT_GLOBAL_ENV_CONSTANTS_H
#define ECMASCRIPT_GLOBAL_ENV_CONSTANTS_H

#include <cstdint>

#include "ecmascript/mem/visitor.h"
#include "libpandabase/macros.h"

namespace panda::ecmascript {
// Forward Declaration
template<typename T>
class JSHandle;
class JSHClass;
class JSThread;
class ObjectFactory;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GLOBAL_ENV_CONSTANT_CLASS(V)                                                                                  \
    /* GC Root */                                                                                                     \
    V(JSTaggedValue, HClassClass, HCLASS_CLASS_INDEX, ecma_roots_class)                                               \
    V(JSTaggedValue, FreeObjectWithNoneFieldClass, FREE_OBJECT_WITH_NONE_FIELD_CLASS_INDEX, ecma_roots_class)         \
    V(JSTaggedValue, FreeObjectWithOneFieldClass, FREE_OBJECT_WITH_ONE_FIELD_CLASS_INDEX, ecma_roots_class)           \
    V(JSTaggedValue, FreeObjectWithTwoFieldClass, FREE_OBJECT_WITH_TWO_FIELD_CLASS_INDEX, ecma_roots_class)           \
    V(JSTaggedValue, LineStringClass, LINE_STRING_CLASS_INDEX, ecma_roots_class)                                      \
    V(JSTaggedValue, SlicedStringClass, SLICED_STRING_CLASS_INDEX, ecma_roots_class)                                  \
    V(JSTaggedValue, ConstantStringClass, CONSTANT_STRING_CLASS_INDEX, ecma_roots_class)                              \
    V(JSTaggedValue, TreeStringClass, TREE_STRING_CLASS_INDEX, ecma_roots_class)                                      \
    V(JSTaggedValue, ArrayClass, ARRAY_CLASS_INDEX, ecma_roots_class)                                                 \
    V(JSTaggedValue, ByteArrayClass, BYTE_ARRAY_CLASS_INDEX, ecma_roots_class)                                        \
    V(JSTaggedValue, ConstantPoolClass, CONSTANT_POOL_CLASS_INDEX, ecma_roots_class)                                  \
    V(JSTaggedValue, ProfileTypeInfoClass, PROFILE_TYPE_INFO_CLASS_INDEX, ecma_roots_class)                           \
    V(JSTaggedValue, DictionaryClass, DICTIONARY_CLASS_INDEX, ecma_roots_class)                                       \
    V(JSTaggedValue, COWArrayClass, COW_ARRAY_CLASS_INDEX, ecma_roots_class)                                          \
    V(JSTaggedValue, MutantTaggedArrayClass, MUTANT_TAGGED_ARRAY_CLASS_INDEX, ecma_roots_class)                       \
    V(JSTaggedValue, COWMutantTaggedArrayClass, COW_MUTANT_TAGGED_ARRAY_CLASS_INDEX, ecma_roots_class)                \
    V(JSTaggedValue, BigIntClass, BIGINT_CLASS_INDEX, ecma_roots_class)                                               \
    V(JSTaggedValue, JSNativePointerClass, JS_NATIVE_POINTER_CLASS_INDEX, ecma_roots_class)                           \
    V(JSTaggedValue, EnvClass, ENV_CLASS_INDEX, ecma_roots_class)                                                     \
    V(JSTaggedValue, SymbolClass, SYMBOL_CLASS_INDEX, ecma_roots_class)                                               \
    V(JSTaggedValue, AccessorDataClass, ACCESSOR_DATA_CLASS_INDEX, ecma_roots_class)                                  \
    V(JSTaggedValue, InternalAccessorClass, INTERNAL_ACCESSOR_CLASS_INDEX, ecma_roots_class)                          \
    V(JSTaggedValue, JSProxyOrdinaryClass, JS_PROXY_ORDINARY_CLASS_INDEX, ecma_roots_class)                           \
    V(JSTaggedValue, CompletionRecordClass, COMPLETION_RECORD_CLASS_INDEX, ecma_roots_class)                          \
    V(JSTaggedValue, GeneratorContextClass, GENERATOR_CONTEST_INDEX, ecma_roots_class)                                \
    V(JSTaggedValue, AsyncGeneratorRequestRecordClass, ASYNC_GENERATOR_REQUEST_CLASS_INDEX, ecma_roots_class)         \
    V(JSTaggedValue, AsyncIteratorRecordClass, ASYNC_ITERATOR_RECORD_CLASS_INDEX, ecma_roots_class)                   \
    V(JSTaggedValue, CapabilityRecordClass, CAPABILITY_RECORD_CLASS_INDEX, ecma_roots_class)                          \
    V(JSTaggedValue, ReactionsRecordClass, REACTIONS_RECORD_CLASS_INDEX, ecma_roots_class)                            \
    V(JSTaggedValue, PromiseIteratorRecordClass, PROMISE_ITERATOR_RECORD_CLASS_INDEX, ecma_roots_class)               \
    V(JSTaggedValue, PromiseRecordClass, PROMISE_RECORD_CLASS_INDEX, ecma_roots_class)                                \
    V(JSTaggedValue, PromiseResolvingFunctionsRecordClass, PROMISE_RESOLVING_FUNCTIONS_CLASS_INDEX, ecma_roots_class) \
    V(JSTaggedValue, MicroJobQueueClass, MICRO_JOB_QUEUE_CLASS_INDEX, ecma_roots_class)                               \
    V(JSTaggedValue, PendingJobClass, PENDING_JOB_CLASS_INDEX, ecma_roots_class)                                      \
    V(JSTaggedValue, ProtoChangeMarkerClass, PROTO_CHANGE_MARKER_CLASS_INDEX, ecma_roots_class)                       \
    V(JSTaggedValue, ProtoChangeDetailsClass, PROTO_CHANGE_DETAILS_CLASS_INDEX, ecma_roots_class)                     \
    V(JSTaggedValue, MarkerCellClass, MARKER_CELL_CLASS_INDEX, ecma_roots_class)                                      \
    V(JSTaggedValue, TrackInfoClass, TRACK_INFO_CLASS_INDEX, ecma_roots_class)                                        \
    V(JSTaggedValue, PrototypeHandlerClass, PROTOTYPE_HANDLER_CLASS_INDEX, ecma_roots_class)                          \
    V(JSTaggedValue, TransitionHandlerClass, TRANSITION_HANDLER_CLASS_INDEX, ecma_roots_class)                        \
    V(JSTaggedValue, TransWithProtoHandlerClass, TRANS_WITH_PROTO_HANDLER_CLASS_INDEX, ecma_roots_class)              \
    V(JSTaggedValue, StoreTSHandlerClass, STORE_TS_HANDLER_CLASS_INDEX, ecma_roots_class)                             \
    V(JSTaggedValue, PropertyBoxClass, PROPERTY_BOX_CLASS_INDEX, ecma_roots_class)                                    \
    V(JSTaggedValue, ProgramClass, PROGRAM_CLASS_INDEX, ecma_roots_class)                                             \
    V(JSTaggedValue, ImportEntryClass, IMPORT_ENTRY_CLASS_INDEX, ecma_roots_class)                                    \
    V(JSTaggedValue, LocalExportEntryClass, LOCAL_EXPORT_ENTRY_CLASS_INDEX, ecma_roots_class)                         \
    V(JSTaggedValue, IndirectExportEntryClass, INDIRECT_EXPORT_ENTRY_CLASS_INDEX, ecma_roots_class)                   \
    V(JSTaggedValue, StarExportEntryClass, STAR_EXPORT_ENTRY_CLASS_INDEX, ecma_roots_class)                           \
    V(JSTaggedValue, SourceTextModuleClass, SOURCE_TEXT_MODULE_CLASS_INDEX, ecma_roots_class)                         \
    V(JSTaggedValue, ResolvedBindingClass, RESOLVED_BINDING_CLASS_INDEX, ecma_roots_class)                            \
    V(JSTaggedValue, ResolvedIndexBindingClass, RESOLVED_INDEX_BINDING_CLASS_INDEX, ecma_roots_class)                 \
    V(JSTaggedValue, JSProxyCallableClass, JS_PROXY_CALLABLE_CLASS_INDEX, ecma_roots_class)                           \
    V(JSTaggedValue, JSProxyConstructClass, JS_PROXY_CONSTRUCT_CLASS_INDEX, ecma_roots_class)                         \
    V(JSTaggedValue, JSRealmClass, JS_REALM_CLASS_INDEX, ecma_roots_class)                                            \
    V(JSTaggedValue, JSRegExpClass, JS_REGEXP_CLASS_INDEX, ecma_roots_class)                                          \
    V(JSTaggedValue, MachineCodeClass, MACHINE_CODE_CLASS_INDEX, ecma_roots_class)                                    \
    V(JSTaggedValue, ClassInfoExtractorHClass, CLASS_INFO_EXTRACTOR_HCLASS_INDEX, ecma_roots_class)                   \
    V(JSTaggedValue, TSObjectTypeClass, TS_OBJECT_TYPE_CLASS_INDEX, ecma_roots_class)                                 \
    V(JSTaggedValue, TSClassTypeClass, TS_CLASS_TYPE_CLASS_INDEX, ecma_roots_class)                                   \
    V(JSTaggedValue, TSUnionTypeClass, TS_UNION_TYPE_CLASS_INDEX, ecma_roots_class)                                   \
    V(JSTaggedValue, TSInterfaceTypeClass, TS_INTERFACE_TYPE_CLASS_INDEX, ecma_roots_class)                           \
    V(JSTaggedValue, TSClassInstanceTypeClass, TS_CLASS_INSTANCE_TYPE_CLASS_INDEX, ecma_roots_class)                  \
    V(JSTaggedValue, TSFunctionTypeClass, TS_FUNCTION_TYPE_CLASS_INDEX, ecma_roots_class)                             \
    V(JSTaggedValue, TSArrayTypeClass, TS_ARRAY_TYPE_CLASS_INDEX, ecma_roots_class)                                   \
    V(JSTaggedValue, TSIteratorInstanceTypeClass, TS_ITERATOR_INSTANCE_TYPE_CLASS_INDEX, ecma_roots_class)            \
    V(JSTaggedValue, TSNamespaceTypeClass, TS_NAMESPACE_TYPE_CLASS_INDEX, ecma_roots_class)                           \
    V(JSTaggedValue, JSSetIteratorClass, JS_SET_ITERATOR_CLASS_INDEX, ecma_roots_class)                               \
    V(JSTaggedValue, JSRegExpIteratorClass, JS_REGEXP_ITERATOR_CLASS_INDEX, ecma_roots_class)                         \
    V(JSTaggedValue, JSMapIteratorClass, JS_MAP_ITERATOR_CLASS_INDEX, ecma_roots_class)                               \
    V(JSTaggedValue, JSArrayIteratorClass, JS_ARRAY_ITERATOR_CLASS_INDEX, ecma_roots_class)                           \
    V(JSTaggedValue, JSAPIArrayListIteratorClass, JS_API_ARRAYLIST_ITERATOR_CLASS_INDEX, ecma_roots_class)            \
    V(JSTaggedValue, JSAPIDequeIteratorClass, JS_API_DEQUE_ITERATOR_CLASS_INDEX, ecma_roots_class)                    \
    V(JSTaggedValue, JSAPILightWeightMapIteratorClass, JS_API_LIGHTWEIGHTMAP_ITERATOR_CLASS_INDEX, ecma_roots_class)  \
    V(JSTaggedValue, JSAPILightWeightSetIteratorClass, JS_API_LIGHTWEIGHTSET_ITERATOR_CLASS_INDEX, ecma_roots_class)  \
    V(JSTaggedValue, JSAPILinkedListIteratorClass, JS_API_LINKED_LIST_ITERATOR_CLASS_INDEX, ecma_roots_class)         \
    V(JSTaggedValue, JSAPIListIteratorClass, JS_API_LIST_ITERATOR_CLASS_INDEX, ecma_roots_class)                      \
    V(JSTaggedValue, JSAPIPlainArrayIteratorClass, JS_API_PLAIN_ARRAY_ITERATOR_CLASS_INDEX, ecma_roots_class)         \
    V(JSTaggedValue, JSAPIQueueIteratorClass, JS_API_QUEUE_ITERATOR_CLASS_INDEX, ecma_roots_class)                    \
    V(JSTaggedValue, JSAPIStackIteratorClass, JS_API_STACK_ITERATOR_CLASS_INDEX, ecma_roots_class)                    \
    V(JSTaggedValue, JSAPIVectorIteratorClass, JS_API_VECTOR_ITERATOR_CLASS_INDEX, ecma_roots_class)                  \
    V(JSTaggedValue, JSAPIHashMapIteratorClass, JS_API_HASH_MAP_ITERATOR_CLASS_INDEX, ecma_roots_class)               \
    V(JSTaggedValue, JSAPIHashSetIteratorClass, JS_API_HASH_SET_ITERATOR_CLASS_INDEX, ecma_roots_class)               \
    V(JSTaggedValue, JSAPITreeMapIteratorClass, JS_API_TREE_MAP_ITERATOR_CLASS_INDEX, ecma_roots_class)               \
    V(JSTaggedValue, JSAPITreeSetIteratorClass, JS_API_TREE_SET_ITERATOR_CLASS_INDEX, ecma_roots_class)               \
    V(JSTaggedValue, LinkedNode, LINKED_NODE_CLASS_INDEX, ecma_roots_class)                                           \
    V(JSTaggedValue, RBTreeNode, RB_TREENODE_CLASS_INDEX, ecma_roots_class)                                           \
    V(JSTaggedValue, JSAPIIteratorFuncHClass, JS_API_ITERATOR_FUNC_CLASS_INDEX, ecma_roots_class)                     \
    V(JSTaggedValue, JSAPIAsyncIteratorFuncHClass, JS_API_ASYNCITERATOR_FUNC_CLASS_INDEX, ecma_roots_class)           \
    V(JSTaggedValue, CellRecordClass, CELL_RECORD_CLASS_INDEX, ecma_roots_class)                                      \
    V(JSTaggedValue, ObjectClass, OBJECT_HCLASS_INDEX, initial_object_hclass)                                         \
    V(JSTaggedValue, IteratorResultClass, ITERATOR_RESULT_CLASS, ecma_roots_class)                                    \
    V(JSTaggedValue, MethodClass, METHOD_CLASS_INDEX, ecma_roots_class)                                               \
    V(JSTaggedValue, ClassPrototypeClass, CLASS_PROTOTYPE_HCLASS_INDEX, ecma_roots_class)                             \
    V(JSTaggedValue, ClassConstructorClass, CLASS_CONSTRUCTOR_HCLASS_INDEX, ecma_roots_class)                         \
    V(JSTaggedValue, AOTLiteralInfoClass, AOT_LITERAL_INFO_CLASS_INDEX, ecma_roots_class)                             \
    V(JSTaggedValue, VTableClass, VTABLE_CLASS_INDEX, ecma_roots_class)                                               \
    V(JSTaggedValue, ClassLiteralClass, CLASS_LITERAL_HCLASS_INDEX, ecma_roots_class)                                 \
    V(JSTaggedValue, ElementNoneClass, ELEMENT_NONE_HCLASS_INDEX, ecma_roots_class)                                   \
    V(JSTaggedValue, ElementHoleClass, ELEMENT_HOLE_HCLASS_INDEX, ecma_roots_class)                                   \
    V(JSTaggedValue, ElementIntClass, ELEMENT_INT_HCLASS_INDEX, ecma_roots_class)                                     \
    V(JSTaggedValue, ElementNumberClass, ELEMENT_NUMBER_HCLASS_INDEX, ecma_roots_class)                               \
    V(JSTaggedValue, ElementStringClass, ELEMENT_STRING_HCLASS_INDEX, ecma_roots_class)                               \
    V(JSTaggedValue, ElementObjectClass, ELEMENT_OBJECT_HCLASS_INDEX, ecma_roots_class)                               \
    V(JSTaggedValue, ElementTaggedClass, ELEMENT_TAGGED_HCLASS_INDEX, ecma_roots_class)                               \
    V(JSTaggedValue, ElementHoleIntClass, ELEMENT_HOLE_INT_HCLASS_INDEX, ecma_roots_class)                            \
    V(JSTaggedValue, ElementHoleNumberClass, ELEMENT_HOLE_NUMBER_HCLASS_INDEX, ecma_roots_class)                      \
    V(JSTaggedValue, ElementHoleStringClass, ELEMENT_HOLE_STRING_HCLASS_INDEX, ecma_roots_class)                      \
    V(JSTaggedValue, ElementHoleObjectClass, ELEMENT_HOLE_OBJECT_HCLASS_INDEX, ecma_roots_class)                      \
    V(JSTaggedValue, ElementHoleTaggedClass, ELEMENT_HOLE_TAGGED_HCLASS_INDEX, ecma_roots_class)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GLOBAL_ENV_CONSTANT_SPECIAL(V)                                                                 \
    V(JSTaggedValue, Undefined, UNDEFINED_INDEX, ecma_roots_special)                                   \
    V(JSTaggedValue, Null, NULL_INDEX, ecma_roots_special)                                             \
    V(JSTaggedValue, True, TRUE_INDEX, ecma_roots_special)                                             \
    V(JSTaggedValue, False, FALSE_INDEX, ecma_roots_special)                                           \
    V(JSTaggedValue, EmptyString, EMPTY_STRING_OBJECT_INDEX, ecma_roots_special)                       \
    V(JSTaggedValue, EmptyLayoutInfo, EMPTY_LAYOUT_INFO_OBJECT_INDEX, ecma_roots_special)              \
    V(JSTaggedValue, EmptyArray, EMPTY_ARRAY_OBJECT_INDEX, ecma_roots_special)                         \
    V(JSTaggedValue, EmptyMutantArray, EMPTY_MUTANT_ARRAY_OBJECT_INDEX, ecma_roots_special)            \
    V(JSTaggedValue, DefaultSupers, DEFAULT_SUPERS_INDEX, ecma_roots_special)                          \
    V(JSTaggedValue, EmptyTaggedQueue, EMPTY_TAGGED_QUEUE_OBJECT_INDEX, ecma_roots_special)            \
    V(JSTaggedValue, UndefinedCompletionRecord, UNDEFINED_COMPLRTION_RECORD_INDEX, ecma_roots_special) \
    V(JSTaggedValue, MathSqrtFunction, MATH_SQRT_FUNCTION_INDEX, ecma_roots_special)                   \
    V(JSTaggedValue, MathCos, MATH_COS_INDEX, ecma_roots_special)                                      \
    V(JSTaggedValue, MathSin, MATH_SIN_INDEX, ecma_roots_special)                                      \
    V(JSTaggedValue, MathACosFunction, MATH_ACOS_FUNCTION_INDEX, ecma_roots_special)                   \
    V(JSTaggedValue, MathATanFunction, MATH_ATAN_FUNCTION_INDEX, ecma_roots_special)                   \
    V(JSTaggedValue, MathAbsFunction, MATH_ABS_FUNCTION_INDEX, ecma_roots_special)                     \
    V(JSTaggedValue, MathFloorFunction, MATH_FLOOR_FUNCTION_INDEX, ecma_roots_special)                 \
    V(JSTaggedValue, LocaleCompareFunction, LOCALE_COMPARE_FUNCTION_INDEX, ecma_roots_special)         \
    V(JSTaggedValue, ArraySortFunction, ARRAY_SORT_FUNCTION_INDEX, ecma_roots_special)                 \
    V(JSTaggedValue, JsonStringifyFunction, JSON_STRINGIFY_FUNCTION_INDEX, ecma_roots_special)         \
    V(JSTaggedValue, MapIteratorProtoNext, MAP_ITERATOR_PROTO_NEXT_INDEX, ecma_roots_special)          \
    V(JSTaggedValue, SetIteratorProtoNext, SET_ITERATOR_PROTO_NEXT_INDEX, ecma_roots_special)          \
    V(JSTaggedValue, StringIteratorProtoNext, STRING_ITERATOR_PROTO_NEXT_INDEX, ecma_roots_special)    \
    V(JSTaggedValue, ArrayIteratorProtoNext, ARRAY_ITERATOR_PROTO_NEXT_INDEX, ecma_roots_special)      \
    V(JSTaggedValue, IteratorProtoReturn, ITERATOR_PROTO_RETURN_INDEX, ecma_roots_special)             \
    V(JSTaggedValue, StringFromCharCode, STRING_FROM_CHAR_CODE_INDEX, ecma_roots_special)

// All of type JSTaggedValue
#define GLOBAL_ENV_CONSTANT_STRING(V) \
    V(ConstructorString,              CONSTRUCTOR_STRING_INDEX,              "constructor")                 \
    V(PrototypeString,                PROTOTYPE_STRING_INDEX,                "prototype")                   \
    V(LengthString,                   LENGTH_STRING_INDEX,                   "length")                      \
    V(ValueString,                    VALUE_STRING_INDEX,                    "value")                       \
    V(SetString,                      SET_STRING_INDEX,                      "set")                         \
    V(GetString,                      GET_STRING_INDEX,                      "get")                         \
    V(WritableString,                 WRITABLE_STRING_INDEX,                 "writable")                    \
    V(EnumerableString,               ENUMERABLE_STRING_INDEX,               "enumerable")                  \
    V(ConfigurableString,             CONFIGURABLE_STRING_INDEX,             "configurable")                \
    V(NameString,                     NAME_STRING_INDEX,                     "name")                        \
    /* SymbolTable * RegisterSymbols */                                                                     \
    V(GetPrototypeOfString,           GETPROTOTYPEOF_STRING_INDEX,           "getPrototypeOf")              \
    V(SetPrototypeOfString,           SETPROTOTYPEOF_STRING_INDEX,           "setPrototypeOf")              \
    V(IsExtensibleString,             ISEXTENSIBLE_STRING_INDEX,             "isExtensible")                \
    V(PreventExtensionsString,        PREVENTEXTENSIONS_STRING_INDEX,        "preventExtensions")           \
    V(GetOwnPropertyDescriptorString, GETOWNPROPERTYDESCRIPTOR_STRING_INDEX, "getOwnPropertyDescriptor")    \
    V(DefinePropertyString,           DEFINEPROPERTY_STRING_INDEX,           "defineProperty")              \
    V(HasString,                      HAS_STRING_INDEX,                      "has")                         \
    V(DeletePropertyString,           DELETEPROPERTY_STRING_INDEX,           "deleteProperty")              \
    V(EnumerateString,                ENUMERATE_STRING_INDEX,                "enumerate")                   \
    V(OwnKeysString,                  OWNKEYS_STRING_INDEX,                  "ownKeys")                     \
    V(ApplyString,                    APPLY_STRING_INDEX,                    "apply")                       \
    V(NegativeZeroString,             NEGATIVE_ZERO_STRING_INDEX,            "-0")                          \
    V(DoneString,                     DONE_STRING_INDEX,                     "done")                        \
    V(ProxyString,                    PROXY_STRING_INDEX,                    "proxy")                       \
    V(RevokeString,                   REVOKE_STRING_INDEX,                   "revoke")                      \
    V(NextString,                     NEXT_STRING_INDEX,                     "next")                        \
    V(ToStringString,                 TO_STRING_STRING_INDEX,                "toString")                    \
    V(ToLocaleStringString,           TO_LOCALE_STRING_STRING_INDEX,         "toLocaleString")              \
    V(ValueOfString,                  VALUE_OF_STRING_INDEX,                 "valueOf")                     \
    V(UndefinedString,                UNDEFINED_STRING_INDEX,                "undefined")                   \
    V(NullString,                     NULL_STRING_INDEX,                     "null")                        \
    V(BooleanString,                  BOOLEAN_STRING_INDEX,                  "boolean")                     \
    V(NumberString,                   NUMBER_STRING_INDEX,                   "number")                      \
    V(BigIntString,                   BIGINT_STRING_INDEX,                   "bigint")                      \
    V(FunctionString,                 FUNCTION_STRING_INDEX,                 "function")                    \
    V(StringString,                   STRING_STRING_INDEX,                   "string")                      \
    V(SymbolString,                   SYMBOL_STRING_INDEX,                   "symbol")                      \
    V(ObjectString,                   OBJECT_STRING_INDEX,                   "object")                      \
    V(TrueString,                     TRUE_STRING_INDEX,                     "true")                        \
    V(FalseString,                    FALSE_STRING_INDEX,                    "false")                       \
    V(ReturnString,                   RETURN_STRING_INDEX,                   "return")                      \
    V(ProxyConstructString,           PROXY_CONSTRUCT_STRING_INDEX,          "construct")                   \
    V(ProxyCallString,                PROXY_CALL_STRING_INDEX,               "call")                        \
    V(PromiseThenString,              PROMISE_THEN_STRING_INDEX,             "then")                        \
    V(PromiseCatchString,             PROMISE_CATCH_STRING_INDEX,            "catch")                       \
    V(PromiseFinallyString,           PROMISE_FINALLY_STRING_INDEX,          "finally")                     \
    V(PromiseStatusString,            PROMISE_STATUS_STRING_INDEX,           "status")                      \
    V(PromiseFulfilledString,         PROMISE_FULFILLED_STRING_INDEX,        "fulfilled")                   \
    V(PromiseRejectedString,          PROMISE_REJECTED_STRING_INDEX,         "rejected")                    \
    V(PromiseReasonString,            PROMISE_REASON_STRING_INDEX,           "reason")                      \
    V(ScriptJobString,                SCRIPT_JOB_STRING_INDEX,               "ScriptJobs")                  \
    V(PromiseString,                  PROMISE_STRING_INDEX,                  "PrimiseJobs")                 \
    V(ThrowerString,                  THROWER_STRING_INDEX,                  "Thrower")                     \
    V(IdentityString,                 IDENTITY_STRING_INDEX,                 "Identity")                    \
    V(CallerString,                   CALLER_STRING_INDEX,                   "caller")                      \
    V(CalleeString,                   CALLEE_STRING_INDEX,                   "callee")                      \
    V(Int8ArrayString,                INT8_ARRAY_STRING_INDEX,               "Int8Array")                   \
    V(Uint8ArrayString,               UINT8_ARRAY_STRING_INDEX,              "Uint8Array")                  \
    V(Uint8ClampedArrayString,        UINT8_CLAMPED_ARRAY_STRING_INDEX,      "Uint8ClampedArray")           \
    V(Int16ArrayString,               INT16_ARRAY_STRING_INDEX,              "Int16Array")                  \
    V(Uint16ArrayString,              UINT16_ARRAY_STRING_INDEX,             "Uint16Array")                 \
    V(Int32ArrayString,               INT32_ARRAY_STRING_INDEX,              "Int32Array")                  \
    V(Uint32ArrayString,              UINT32_ARRAY_STRING_INDEX,             "Uint32Array")                 \
    V(Float32ArrayString,             FLOAT32_ARRAY_STRING_INDEX,            "Float32Array")                \
    V(Float64ArrayString,             FLOAT64_ARRAY_STRING_INDEX,            "Float64Array")                \
    V(BigInt64ArrayString,            BIGINT64_ARRAY_STRING_INDEX,           "BigInt64Array")               \
    V(BigUint64ArrayString,           BIGUINT64_ARRAY_STRING_INDEX,          "BigUint64Array")              \
    V(AsyncFunctionString,            ASYNC_FUNCTION_STRING_INDEX,           "AsyncFunction")               \
    V(PromiseResolveString,           PROMISE_RESOLVE_STRING_INDEX,          "resolve")                     \
    V(IdString,                       ID_STRING_INDEX,                       "id")                          \
    V(MethodString,                   METHOD_STRING_INDEX,                   "method")                      \
    V(ParamsString,                   PARAMS_STRING_INDEX,                   "params")                      \
    V(ResultString,                   RESULT_STRING_INDEX,                   "result")                      \
    V(ToJsonString,                   TO_JSON_STRING_INDEX,                  "toJSON")                      \
    V(GlobalString,                   GLOBAL_STRING_INDEX,                   "global")                      \
    V(MessageString,                  MESSAGE_STRING_INDEX,                  "message")                     \
    V(CauseString,                    CAUSE_STRING_INDEX,                    "cause")                       \
    V(ErrorString,                    ERROR_STRING_INDEX,                    "Error")                       \
    V(ErrorsString,                   ERRORS_STRING_INDEX,                   "errors")                      \
    V(AggregateErrorString,           AGGREGATE_ERROR_STRING_INDEX,          "AggregateError")              \
    V(RangeErrorString,               RANGE_ERROR_STRING_INDEX,              "RangeError")                  \
    V(ReferenceErrorString,           REFERENCE_ERROR_STRING_INDEX,          "ReferenceError")              \
    V(TypeErrorString,                TYPE_ERROR_STRING_INDEX,               "TypeError")                   \
    V(URIErrorString,                 URI_ERROR_STRING_INDEX,                "URIError")                    \
    V(SyntaxErrorString,              SYNTAX_ERROR_STRING_INDEX,             "SyntaxError")                 \
    V(EvalErrorString,                EVAL_ERROR_STRING_INDEX,               "EvalError")                   \
    V(OOMErrorString,                 OOM_ERROR_STRING_INDEX,                "OutOfMemoryError")            \
    V(TerminationErrorString,         TERMINATION_ERROR_STRING_INDEX,        "TerminationError")            \
    V(ErrorFuncString,                ERROR_FUNC_STRING_INDEX,               "errorfunc")                   \
    V(StackString,                    STACK_STRING_INDEX,                    "stack")                       \
    V(TopStackString,                 TOP_STACK_STRING_INDEX,                "topstack")                    \
    V(StackEmptyString,               STACK_EMPTY_STRING_INDEX,              "stackisempty")                \
    V(ObjNotCoercibleString,          OBJ_NOT_COERCIBLE_STRING_INDEX,        "objectnotcoercible")          \
    /* for Intl. */                                                                                         \
    V(LanguageString,                 LANGUAGE_STRING_CLASS_INDEX,           "language")                    \
    V(ScriptString,                   SCRIPT_STRING_CLASS_INDEX,             "script")                      \
    V(RegionString,                   REGION_STRING_CLASS_INDEX,             "region")                      \
    V(BaseNameString,                 BASE_NAME_STRING_CLASS_INDEX,          "baseName")                    \
    V(CalendarString,                 CALENDAR_STRING_CLASS_INDEX,           "calendar")                    \
    V(CollationString,                COLLATION_STRING_CLASS_INDEX,          "collation")                   \
    V(HourCycleString,                HOUR_CYCLE_STRING_CLASS_INDEX,         "hourCycle")                   \
    V(CaseFirstString,                CASE_FIRST_STRING_CLASS_INDEX,         "caseFirst")                   \
    V(NumericString,                  NUMERIC_STRING_CLASS_INDEX,            "numeric")                     \
    V(NumberingSystemString,          NUMBERING_SYSTEM_STRING_CLASS_INDEX,   "numberingSystem")             \
    V(TypeString,                     TYPE_STRING_INDEX,                     "type")                        \
    V(GranularityString,              GRANULARITY_STRING_INDEX,              "granularity")                 \
    V(GraphemeString,                 GRAPHEME_STRING_INDEX,                 "grapheme")                    \
    V(WordString,                     WORD_STRING_INDEX,                     "word")                        \
    V(SentenceString,                 SENTENCE_STRING_INDEX,                 "sentence")                    \
    V(SegmentString,                  SEGMENT_STRING_INDEX,                  "segment")                     \
    V(IsWordLikeString,               ISWORDLIKE_STRING_INDEX,               "isWordLike")                  \
    V(LocaleMatcherString,            LOCALE_MATCHER_STRING_INDEX,           "localeMatcher")               \
    V(FormatMatcherString,            FORMAT_MATCHER_STRING_INDEX,           "formatMatcher")               \
    V(Hour12String,                   HOUR12_STRING_INDEX,                   "hour12")                      \
    V(H11String,                      H11_STRING_INDEX,                      "h11")                         \
    V(H12String,                      H12_STRING_INDEX,                      "h12")                         \
    V(H23String,                      H23_STRING_INDEX,                      "h23")                         \
    V(H24String,                      H24_STRING_INDEX,                      "h24")                         \
    V(WeekdayString,                  WEEK_DAY_STRING_INDEX,                 "weekday")                     \
    V(EraString,                      ERA_STRING_INDEX,                      "era")                         \
    V(YearString,                     YEAR_STRING_INDEX,                     "year")                        \
    V(QuarterString,                  QUARTER_STRING_INDEX,                  "quarter")                     \
    V(MonthString,                    MONTH_STRING_INDEX,                    "month")                       \
    V(DayString,                      DAY_STRING_INDEX,                      "day")                         \
    V(HourString,                     HOUR_STRING_INDEX,                     "hour")                        \
    V(MinuteString,                   MINUTE_STRING_INDEX,                   "minute")                      \
    V(SecondString,                   SECOND_STRING_INDEX,                   "second")                      \
    V(YearsString,                    YEARS_STRING_INDEX,                    "years")                       \
    V(QuartersString,                 QUARTERS_STRING_INDEX,                 "quarters")                    \
    V(MonthsString,                   MONTHS_STRING_INDEX,                   "months")                      \
    V(DaysString,                     DAYS_STRING_INDEX,                     "days")                        \
    V(HoursString,                    HOURS_STRING_INDEX,                    "hours")                       \
    V(MinutesString,                  MINUTES_STRING_INDEX,                  "minutes")                     \
    V(SecondsString,                  SECONDS_STRING_INDEX,                  "seconds")                     \
    V(TimeZoneNameString,             TIME_ZONE_NAME_STRING_INDEX,           "timeZoneName")                \
    V(LocaleString,                   LOCALE_STRING_INDEX,                   "locale")                      \
    V(TimeZoneString,                 TIME_ZONE_STRING_INDEX,                "timeZone")                    \
    V(LiteralString,                  LITERAL_STRING_INDEX,                  "literal")                     \
    V(YearNameString,                 YEAR_NAME_STRING_INDEX,                "yearName")                    \
    V(DayPeriodString,                DAY_PERIOD_STRING_INDEX,               "dayPeriod")                   \
    V(FractionalSecondDigitsString,   FRACTIONAL_SECOND_DIGITS_STRING_INDEX, "fractionalSecondDigits")      \
    V(FractionalSecondString,         FRACTIONAL_SECOND_STRING_INDEX,        "fractionalSecond")            \
    V(RelatedYearString,              RELATED_YEAR_STRING_INDEX,             "relatedYear")                 \
    V(LookUpString,                   LOOK_UP_STRING_INDEX,                  "lookup")                      \
    V(BestFitString,                  BEST_FIT_STRING_INDEX,                 "bestfit")                     \
    V(DateStyleString,                DATE_STYLE_STRING_INDEX,               "dateStyle")                   \
    V(TimeStyleString,                TIME_STYLE_STRING_INDEX,               "timeStyle")                   \
    V(UTCString,                      UTC_STRING_INDEX,                      "UTC")                         \
    V(WeekString,                     WEEK_STRING_INDEX,                     "week")                        \
    V(WeeksString,                    WEEKS_STRING_INDEX,                    "weeks")                       \
    V(SourceString,                   SOURCE_STRING_INDEX,                   "source")                      \
    V(FormatString,                   FORMAT_STRING_INDEX,                   "format")                      \
    V(EnUsString,                     EN_US_STRING_INDEX,                    "en-US")                       \
    V(UndString,                      UND_STRING_INDEX,                      "und")                         \
    V(LatnString,                     LATN_STRING_INDEX,                     "latn")                        \
    V(StyleString,                    STYLE_STRING_INDEX,                    "style")                       \
    V(UnitString,                     UNIT_STRING_INDEX,                     "unit")                        \
    V(IntegerString,                  INTEGER_STRING_INDEX,                  "integer")                     \
    V(NanString,                      NAN_STRING_INDEX,                      "nan")                         \
    V(InfinityString,                 INFINITY_STRING_INDEX,                 "infinity")                    \
    V(FractionString,                 FRACTION_STRING_INDEX,                 "fraction")                    \
    V(DecimalString,                  DECIMAL_STRING_INDEX,                  "decimal")                     \
    V(GroupString,                    GROUP_STRING_INDEX,                    "group")                       \
    V(GroupsString,                   GROUPS_STRING_INDEX,                   "groups")                      \
    V(CurrencyString,                 CURRENCY_STRING_INDEX,                 "currency")                    \
    V(CurrencySignString,             CURRENCY_SIGN_STRING_INDEX,            "currencySign")                \
    V(CurrencyDisplayString,          CURRENCY_DISPLAY_STRING_INDEX,         "currencyDisplay")             \
    V(PercentSignString,              PERCENT_SIGN_STRING_INDEX,             "percentSign")                 \
    V(PercentString,                  PERCENT_STRING_INDEX,                  "percent")                     \
    V(MinusSignString,                MINUS_SIGN_STRING_INDEX,               "minusSign")                   \
    V(PlusSignString,                 PLUS_SIGN_STRING_INDEX,                "plusSign")                    \
    V(ExponentSeparatorString,        EXPONENT_SEPARATOR_STRING_INDEX,       "exponentSeparator")           \
    V(ExponentMinusSignString,        EXPONENT_MINUS_SIGN_INDEX,             "exponentMinusSign")           \
    V(ExponentIntegerString,          EXPONENT_INTEGER_STRING_INDEX,         "exponentInteger")             \
    V(LongString,                     LONG_STRING_INDEX,                     "long")                        \
    V(ShortString,                    SHORT_STRING_INDEX,                    "short")                       \
    V(FullString,                     FULL_STRING_INDEX,                     "full")                        \
    V(MediumString,                   MEDIUM_STRING_INDEX,                   "medium")                      \
    V(NarrowString,                   NARROW_STRING_INDEX,                   "narrow")                      \
    V(AlwaysString,                   ALWAYS_STRING_INDEX,                   "always")                      \
    V(AutoString,                     AUTO_STRING_INDEX,                     "auto")                        \
    V(ThrowString,                    THROW_STRING_INDEX,                    "throw")                       \
    V(UnitDisplayString,              UNIT_DISPLAY_INDEX,                    "unitDisplay")                 \
    V(NotationString,                 NOTATION_INDEX,                        "notation")                    \
    V(CompactDisplayString,           COMPACT_DISPALY_INDEX,                 "compactDisplay")              \
    V(UserGroupingString,             USER_GROUPING_INDEX,                   "useGrouping")                 \
    V(SignDisplayString,              SIGN_DISPLAY_INDEX,                    "signDisplay")                 \
    V(CodeString,                     CODE_INDEX,                            "code")                        \
    V(NarrowSymbolString,             NARROW_SYMBOL_INDEX,                   "narrowSymbol")                \
    V(StandardString,                 STANDARD_INDEX,                        "standard")                    \
    V(AccountingString,               ACCOUNTING_INDEX,                      "accounting")                  \
    V(ScientificString,               SCIENTIFIC_INDEX,                      "scientific")                  \
    V(EngineeringString,              ENGINEERING_INDEX,                     "engineering")                 \
    V(CompactString,                  COMPACT_STRING_INDEX,                  "compact")                     \
    V(NeverString,                    NEVER_INDEX,                           "never")                       \
    V(ExceptZeroString,               EXPECT_ZERO_INDEX,                     "exceptZero")                  \
    V(MinimumIntegerDigitsString,     MINIMUM_INTEGER_DIGITS_INDEX,          "minimumIntegerDigits")        \
    V(MinimumFractionDigitsString,    MINIMUM_FRACTIONDIGITS_INDEX,          "minimumFractionDigits")       \
    V(MaximumFractionDigitsString,    MAXIMUM_FRACTIONDIGITS_INDEX,          "maximumFractionDigits")       \
    V(MinimumSignificantDigitsString, MINIMUM_SIGNIFICANTDIGITS_INDEX,       "minimumSignificantDigits")    \
    V(MaximumSignificantDigitsString, MAXIMUM_SIGNIFICANTDIGITS_INDEX,       "maximumSignificantDigits")    \
    V(InvalidDateString,              INVALID_DATE_INDEX,                    "Invalid Date")                \
    V(UsageString,                    USAGE_INDEX,                           "usage")                       \
    V(CompareString,                  COMPARE_INDEX,                         "compare")                     \
    V(SensitivityString,              SENSITIVITY_INDEX,                     "sensitivity")                 \
    V(IgnorePunctuationString,        IGNORE_PUNCTUATION_INDEX,              "ignorePunctuation")           \
    V(CardinalString,                 CARDINAL_INDEX,                        "cardinal")                    \
    V(OrdinalString,                  ORDINAL_INDEX,                         "ordinal")                     \
    V(ExecString,                     EXEC_INDEX,                            "exec")                        \
    V(LastIndexString,                LAST_INDEX_INDEX,                      "lastIndex")                   \
    V(PluralCategoriesString,         PLURAL_CATEGORIES_INDEX,               "pluralCategories")            \
    V(SortString,                     SORT_INDEX,                            "sort")                        \
    V(SearchString,                   SEARCH_INDEX,                          "search")                      \
    V(BaseString,                     BASE_INDEX,                            "base")                        \
    V(AccentString,                   ACCENT_INDEX,                          "accent")                      \
    V(CaseString,                     CASE_INDEX,                            "case")                        \
    V(VariantString,                  VARIANT_INDEX,                         "variant")                     \
    V(EnUsPosixString,                EN_US_POSIX_STRING_INDEX,              "en-US-POSIX")                 \
    V(UpperString,                    UPPER_INDEX,                           "upper")                       \
    V(LowerString,                    LOWER_INDEX,                           "lower")                       \
    V(DefaultString,                  DEFAULT_INDEX,                         "default")                     \
    V(SharedString,                   SHARED_INDEX,                          "shared")                      \
    V(StartRangeString,               START_RANGE_INDEX,                     "startRange")                  \
    V(EndRangeString,                 END_RANGE_INDEX,                       "endRange")                    \
    V(Iso8601String,                  ISO8601_INDEX,                         "iso8601")                     \
    V(GregoryString,                  GREGORY_INDEX,                         "gregory")                     \
    V(EthioaaString,                  ETHIOAA_INDEX,                         "ethioaa")                     \
    V(StickyString,                   STICKY_INDEX,                          "sticky")                      \
    V(HasIndicesString,               HAS_INDICES_INDEX,                     "hasIndices")                  \
    V(IndicesString,                  INDICES_INDEX,                         "indices")                     \
    V(UString,                        U_INDEX,                               "u")                           \
    V(IndexString,                    INDEX_INDEX,                           "index")                       \
    V(InputString,                    INPUT_INDEX,                           "input")                       \
    V(UnicodeString,                  UNICODE_INDEX,                         "unicode")                     \
    V(ZeroString,                     ZERO_INDEX,                            "0")                           \
    V(ValuesString,                   VALUES_INDEX,                          "values")                      \
    V(AddString,                      ADD_INDEX,                             "add")                         \
    V(AmbiguousString,                AMBIGUOUS_INDEX,                       "ambiguous")                   \
    V(ModuleString,                   MODULE_INDEX,                          "Module")                      \
    V(StarString,                     STAR_INDEX,                            "*")                           \
    V(DateTimeFieldString,            DATETIMEFIELD_INDEX,                   "datetimefield")               \
    V(ConjunctionString,              CONJUNCTION_INDEX,                     "conjunction")                 \
    V(NoneString,                     NONE_INDEX,                            "none")                        \
    V(FallbackString,                 FALLBACK_INDEX,                        "fallback")                    \
    V(DisjunctionString,              DISJUNCTION_INDEX,                     "disjunction")                 \
    V(ElementString,                  ELEMENT_INDEX,                         "element")                     \
    V(FlagsString,                    FLAGS_INDEX,                           "flags")                       \
    V(GString,                        G_INDEX,                               "g")                           \
    V(NfcString,                      NFC_INDEX,                             "NFC")                         \
    V(EntriesString,                  ENTRIES_INDEX,                         "entries")                     \
    V(LeftSquareBracketString,        LEFT_SQUARE_BRACKET_INDEX,             "[")                           \
    V(RightSquareBracketString,       RIGHT_SQUARE_BRACKET_INDEX,            "]")                           \
    V(YString,                        Y_INDEX,                               "y")                           \
    V(DollarString,                   DOLLAR_INDEX,                          "$")                           \
    V(CommaString,                    COMMA_INDEX,                           ",")                           \
    V(JoinString,                     JOIN_INDEX,                            "join")                        \
    V(CopyWithinString,               COPY_WITHIN_INDEX,                     "copyWithin")                  \
    V(FillString,                     FILL_INDEX,                            "fill")                        \
    V(FindString,                     FIND_INDEX,                            "find")                        \
    V(FindIndexString,                FIND_INDEX_INDEX,                      "findIndex")                   \
    V(FlatString,                     FLAT_INDEX,                            "flat")                        \
    V(FlatMapString,                  FLATMAP_INDEX,                         "flatMap")                     \
    V(IncludesString,                 INCLUDES_INDEX,                        "includes")                    \
    V(KeysString,                     KEYS_INDEX,                            "keys")                        \
    V(BoundString,                    BOUND_INDEX,                           "bound")                       \
    V(BackslashString,                BACKSLASH_INDEX,                       "/")                           \
    V(SpaceString,                    SPACE_INDEX,                           " ")                           \
    V(NanCapitalString,               NAN_INDEX,                             "NaN")                         \
    V(NotEqualString,                 NOT_EQUAL_INDEX,                       "not-equal")                   \
    V(OkString,                       OK_INDEX,                              "ok")                          \
    V(TimeoutString,                  TIMEOUT_INDEX,                         "timed-out")                   \
    V(CjsExportsString,               CJS_EXPORTS_INDEX,                     "exports")                     \
    V(CjsCacheString,                 CJS_CACHE_INDEX,                       "_cache")                      \
    V(NapiWrapperString,              NAPI_WRAPPER_INDEX,                    "_napiwrapper")                \
    /* for require native module */                                                                         \
    V(RequireNativeModuleString,      REQUIRE_NATIVE_MOUDULE_FUNC_INDEX,     "requireNativeModule")         \
    V(RequireNapiString,              REQUIRE_NAPI_FUNC_INDEX,               "requireNapi")                 \
    V(DollarStringOne,                DOLLAR_STRING_ONE_INDEX,               "$1")                          \
    V(DollarStringTwo,                DOLLAR_STRING_TWO_INDEX,               "$2")                          \
    V(DollarStringThree,              DOLLAR_STRING_THREE_INDEX,             "$3")                          \
    V(DollarStringFour,               DOLLAR_STRING_FOUR_INDEX,              "$4")                          \
    V(DollarStringFive,               DOLLAR_STRING_FIVE_INDEX,              "$5")                          \
    V(DollarStringSix,                DOLLAR_STRING_SIX_INDEX,               "$6")                          \
    V(DollarStringSeven,              DOLLAR_STRING_SEVEN_INDEX,             "$7")                          \
    V(DollarStringEight,              DOLLAR_STRING_EIGHT_INDEX,             "$8")                          \
    V(DollarStringNine,               DOLLAR_STRING_NINE_INDEX,              "$9")                          \
    /* for object to string */                                                                              \
    V(UndefinedToString,              UNDEFINED_TO_STRING_INDEX,             "[object Undefined]")          \
    V(NullToString,                   NULL_TO_STRING_INDEX,                  "[object Null]")               \
    V(ObjectToString,                 OBJECT_TO_STRING_INDEX,                "[object Object]")             \
    V(ArrayToString,                  ARRAY_TO_STRING_INDEX,                 "[object Array]")              \
    V(StringToString,                 STRING_TO_STRING_INDEX,                "[object String]")             \
    V(BooleanToString,                BOOLEAN_TO_STRING_INDEX,               "[object Boolean]")            \
    V(NumberToString,                 NUMBER_TO_STRING_INDEX,                "[object Number]")             \
    V(ArgumentsToString,              ARGUMENTS_TO_STRING_INDEX,             "[object Arguments]")          \
    V(FunctionToString,               FUNCTION_TO_STRING_INDEX,              "[object Function]")           \
    V(DateToString,                   DATE_TO_STRING_INDEX,                  "[object Date]")               \
    V(ErrorToString,                  ERROR_TO_STRING_INDEX,                 "[object Error]")              \
    V(RegExpToString,                 REGEXP_TO_STRING_INDEX,                "[object RegExp]")

/* GlobalConstant */
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GLOBAL_ENV_CONSTANT_CONSTANT(V)                                                                     \
    /* non ECMA standard jsapi containers iterators */                                                      \
    V(JSTaggedValue, ArrayListFunction, ARRAYLIST_FUNCTION_INDEX, ArrayListFunction)                        \
    V(JSTaggedValue, ArrayListIteratorPrototype, ARRAYLIST_ITERATOR_PROTOTYPE_INDEX, ArrayListIterator)     \
    V(JSTaggedValue, HashMapIteratorPrototype, HASHMAP_ITERATOR_PROTOTYPE_INDEX, HashMapIterator)           \
    V(JSTaggedValue, HashSetIteratorPrototype, HASHSET_ITERATOR_PROTOTYPE_INDEX, HashSetIterator)           \
    V(JSTaggedValue, LightWeightMapIteratorPrototype,                                                       \
        LIGHTWEIGHTMAP_ITERATOR_PROTOTYPE_INDEX, LightWeightMapIterator)                                    \
    V(JSTaggedValue, LightWeightSetIteratorPrototype,                                                       \
        LIGHTWEIGHTSET_ITERATOR_PROTOTYPE_INDEX, LightWeightSetIterator)                                    \
    V(JSTaggedValue, TreeMapIteratorPrototype, TREEMAP_ITERATOR_PROTOTYPE_INDEX, TreeMapIterator)           \
    V(JSTaggedValue, TreeSetIteratorPrototype, TREESET_ITERATOR_PROTOTYPE_INDEX, TreeSetIterator)           \
    V(JSTaggedValue, VectorFunction, VECTOR_FUNCTION_INDEX, VectorFunction)                                 \
    V(JSTaggedValue, VectorIteratorPrototype, VECTOR_ITERATOR_PROTOTYPE_INDEX, VectorIterator)              \
    V(JSTaggedValue, QueueIteratorPrototype, QUEUE_ITERATOR_PROTOTYPE_INDEX, QueueIterator)                 \
    V(JSTaggedValue, PlainArrayIteratorPrototype, PLAIN_ARRAY_ITERATOR_PROTOTYPE_INDEX, PlainArrayIterator) \
    V(JSTaggedValue, PlainArrayFunction, PLAIN_ARRAY_FUNCTION_INDEX, PlainArrayFunction)                    \
    V(JSTaggedValue, DequeIteratorPrototype, DEQUE_ITERATOR_PROTOTYPE_INDEX, DequeIterator)                 \
    V(JSTaggedValue, StackIteratorPrototype, STACK_ITERATOR_PROTOTYPE_INDEX, StackIterator)                 \
    V(JSTaggedValue, ListFunction, LIST_FUNCTION_INDEX, ListFunction)                                       \
    V(JSTaggedValue, LinkedListFunction, LINKED_LIST_FUNCTION_INDEX, LinkedListFunction)                    \
    V(JSTaggedValue, ListIteratorPrototype, LIST_ITERATOR_PROTOTYPE_INDEX, ListIterator)                    \
    V(JSTaggedValue, UndefinedIterResult, UNDEFINED_INTERATOR_RESULT_INDEX, UndefinedIterResult)            \
    V(JSTaggedValue, LinkedListIteratorPrototype, LINKED_LIST_ITERATOR_PROTOTYPE_INDEX, LinkedListIterator)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GLOBAL_ENV_CONSTANT_ACCESSOR(V)                                                           \
    V(JSTaggedValue, FunctionPrototypeAccessor, FUNCTION_PROTOTYPE_ACCESSOR, ecma_roots_accessor) \
    V(JSTaggedValue, FunctionNameAccessor, FUNCTION_NAME_ACCESSOR, ecma_roots_accessor)           \
    V(JSTaggedValue, FunctionLengthAccessor, FUNCTION_LENGTH_ACCESSOR, ecma_roots_accessor)       \
    V(JSTaggedValue, ArrayLengthAccessor, ARRAY_LENGTH_ACCESSOR, ecma_roots_accessor)

#define GLOBAL_ENV_CACHES(V)                \
    V(JSTaggedValue, CachedJSCollatorLocales, CACHED_JSCOLLATOR_LOCALES_INDEX, cachedCollatorLocales)

// ConstantIndex used for explicit visit each constant.
enum class ConstantIndex : size_t {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define INDEX_FILTER_COMMON(Index) Index,
#define INDEX_FILTER_WITH_TYPE(Type, Name, Index, Desc) INDEX_FILTER_COMMON(Index)
    GLOBAL_ENV_CONSTANT_CLASS(INDEX_FILTER_WITH_TYPE)
    GLOBAL_ENV_CONSTANT_SPECIAL(INDEX_FILTER_WITH_TYPE)
    GLOBAL_ENV_CONSTANT_CONSTANT(INDEX_FILTER_WITH_TYPE)
    GLOBAL_ENV_CONSTANT_ACCESSOR(INDEX_FILTER_WITH_TYPE)
    GLOBAL_ENV_CACHES(INDEX_FILTER_WITH_TYPE)
#undef INDEX_FILTER_WITH_TYPE

#define INDEX_FILTER_STRING(Name, Index, Token) INDEX_FILTER_COMMON(Index)
    GLOBAL_ENV_CONSTANT_STRING(INDEX_FILTER_STRING)
#undef INDEX_FILTER_STRING
#undef INDEX_FILTER_COMMON

    CONSTANT_COUNT,

    CONSTANT_BEGIN = 0,
    CONSTANT_END = CONSTANT_COUNT,

    READ_ONLY_CONSTANT_BEGIN = CONSTANT_BEGIN,
    READ_ONLY_CONSTANT_END = CONSTANT_END,
    JSAPI_CONTAINERS_BEGIN = ARRAYLIST_FUNCTION_INDEX,
    JSAPI_CONTAINERS_END = LINKED_LIST_ITERATOR_PROTOTYPE_INDEX,

    // ...
};
// clang-format on

class GlobalEnvConstants {
public:
    GlobalEnvConstants() = default;
    ~GlobalEnvConstants() = default;

    DEFAULT_MOVE_SEMANTIC(GlobalEnvConstants);
    DEFAULT_COPY_SEMANTIC(GlobalEnvConstants);

    const JSTaggedValue *BeginSlot() const;

    const JSTaggedValue *EndSlot() const;

    void Init(JSThread *thread, JSHClass *hClass);

    void InitRootsClass(JSThread *thread, JSHClass *hClass);
    void InitGlobalConstantSpecial(JSThread *thread);

    void InitGlobalConstant(JSThread *thread);
    void InitGlobalCaches();
    void InitJSAPIContainers();

    void InitSpecialForSnapshot();

    void InitElementKindHClass(const JSThread *thread, JSHandle<JSHClass> originHClass);

    void SetCachedLocales(JSTaggedValue value);

    void SetConstant(ConstantIndex index, JSTaggedValue value);

    template<typename T>
    void SetConstant(ConstantIndex index, JSHandle<T> value);

    uintptr_t GetGlobalConstantAddr(ConstantIndex index) const;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DECL_GET_COMMON(Type, Name)                 \
    const Type Get##Name() const;                   \
    const JSHandle<Type> GetHandled##Name() const;  \
    static size_t GetOffsetOf##Name();

#define DECL_GET_WITH_TYPE(Type, Name, Index, Desc) DECL_GET_COMMON(Type, Name)
    GLOBAL_ENV_CONSTANT_CLASS(DECL_GET_WITH_TYPE)
    GLOBAL_ENV_CONSTANT_SPECIAL(DECL_GET_WITH_TYPE)
    GLOBAL_ENV_CONSTANT_CONSTANT(DECL_GET_WITH_TYPE)
    GLOBAL_ENV_CONSTANT_ACCESSOR(DECL_GET_WITH_TYPE)
    GLOBAL_ENV_CACHES(DECL_GET_WITH_TYPE)
#undef DECL_GET_WITH_TYPE

#define DECL_GET_STRING(Name, Index, Token) DECL_GET_COMMON(JSTaggedValue, Name)
    GLOBAL_ENV_CONSTANT_STRING(DECL_GET_STRING)
#undef DECL_GET_STRING

#undef DECL_GET_COMMON

    void VisitRangeSlot(const RootRangeVisitor &visitor)
    {
        visitor(ecmascript::Root::ROOT_VM, ObjectSlot(ToUintPtr(BeginSlot())), ObjectSlot(ToUintPtr(EndSlot())));
    }

    JSTaggedValue GetGlobalConstantObject(size_t index) const
    {
        ASSERT(static_cast<ConstantIndex>(index) < ConstantIndex::CONSTANT_COUNT);
        return constants_[index];
    }

    size_t GetConstantCount() const
    {
        return static_cast<size_t>(ConstantIndex::CONSTANT_COUNT);
    }

    size_t GetEmptyArrayIndex() const
    {
        return static_cast<size_t>(ConstantIndex::EMPTY_ARRAY_OBJECT_INDEX);
    }

    size_t GetEmptyMutantArrayIndex() const
    {
        return static_cast<size_t>(ConstantIndex::EMPTY_MUTANT_ARRAY_OBJECT_INDEX);
    }

    size_t GetJSAPIContainersBegin() const
    {
        return static_cast<size_t>(ConstantIndex::JSAPI_CONTAINERS_BEGIN);
    }

    size_t GetJSAPIContainersEnd() const
    {
        return static_cast<size_t>(ConstantIndex::JSAPI_CONTAINERS_END);
    }

    size_t GetLineStringClassIndex() const
    {
        return static_cast<size_t>(ConstantIndex::LINE_STRING_CLASS_INDEX);
    }

    size_t GetConstStringClassIndex() const
    {
        return static_cast<size_t>(ConstantIndex::CONSTANT_STRING_CLASS_INDEX);
    }

    bool IsSpecialOrUndefined(size_t index) const
    {
        size_t specialBegin = static_cast<size_t>(ConstantIndex::UNDEFINED_INDEX);
        size_t specialEnd = static_cast<size_t>(ConstantIndex::NULL_INDEX);
        size_t undefinedBegin = GetJSAPIContainersBegin();
        size_t undefinedEnd = GetJSAPIContainersEnd();
        return (index >= specialBegin && index <= specialEnd) || (index >= undefinedBegin && index <= undefinedEnd);
    }

    static constexpr size_t SizeArch32 =
        JSTaggedValue::TaggedTypeSize() * static_cast<size_t>(ConstantIndex::CONSTANT_COUNT);
    static constexpr size_t SizeArch64 =
        JSTaggedValue::TaggedTypeSize() * static_cast<size_t>(ConstantIndex::CONSTANT_COUNT);

private:
    JSTaggedValue constants_[static_cast<int>(ConstantIndex::CONSTANT_COUNT)];  // NOLINT(modernize-avoid-c-arrays)
};
STATIC_ASSERT_EQ_ARCH(sizeof(GlobalEnvConstants), GlobalEnvConstants::SizeArch32, GlobalEnvConstants::SizeArch64);
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_GLOBAL_ENV_CONSTANTS_H
