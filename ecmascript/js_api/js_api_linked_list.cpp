/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#include "ecmascript/js_api/js_api_linked_list.h"

#include "ecmascript/containers/containers_errors.h"
#include "ecmascript/js_api/js_api_linked_list_iterator.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/object_factory.h"
#include "ecmascript/tagged_array-inl.h"
#include "ecmascript/tagged_list.h"

namespace panda::ecmascript {
using ContainerError = containers::ContainerError;
using ErrorFlag = containers::ErrorFlag;
JSTaggedValue JSAPILinkedList::Insert(JSThread *thread, const JSHandle<JSAPILinkedList> &list,
                                      const JSHandle<JSTaggedValue> &value, const int index)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    int nodeLength = doubleList->Length();
    if (index < 0 || index > nodeLength) {
        std::ostringstream oss;
        oss << "The value of \"index\" is out of range. It must be >= 0 && <= " << nodeLength
            << ". Received value is: " << index;
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::RANGE_ERROR, oss.str().c_str());
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    JSTaggedValue newList = TaggedDoubleList::Insert(thread, doubleList, value, index);
    list->SetDoubleList(thread, newList);
    return JSTaggedValue::True();
}

void JSAPILinkedList::Clear(JSThread *thread)
{
    TaggedDoubleList *doubleList = TaggedDoubleList::Cast(GetDoubleList().GetTaggedObject());
    if (doubleList->NumberOfNodes() > 0) {
        doubleList->Clear(thread);
    }
}

JSHandle<JSAPILinkedList> JSAPILinkedList::Clone(JSThread *thread, const JSHandle<JSAPILinkedList> &list)
{
    JSTaggedValue doubleListTaggedValue = list->GetDoubleList();
    JSHandle<TaggedDoubleList> srcDoubleList(thread, doubleListTaggedValue);
    JSHandle<TaggedArray> srcTaggedArray(thread, doubleListTaggedValue);
    ASSERT(!srcDoubleList->IsDictionaryMode());
    uint32_t numberOfNodes = static_cast<uint32_t>(srcDoubleList->NumberOfNodes());
    uint32_t numberOfDeletedNodes = static_cast<uint32_t>(srcDoubleList->NumberOfDeletedNodes());
    uint32_t effectiveCapacity = TaggedDoubleList::ELEMENTS_START_INDEX +
                            (numberOfNodes + numberOfDeletedNodes + 1) * TaggedDoubleList::ENTRY_SIZE;
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSAPILinkedList> newLinkedList = factory->NewJSAPILinkedList();
    JSHandle<TaggedArray> dstTaggedArray = factory->NewAndCopyTaggedArray(srcTaggedArray,
                                                                          effectiveCapacity, effectiveCapacity);
    newLinkedList->SetDoubleList(thread, dstTaggedArray.GetTaggedValue());
    return newLinkedList;
}

JSTaggedValue JSAPILinkedList::RemoveFirst(JSThread *thread, const JSHandle<JSAPILinkedList> &list)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    int nodeLength = doubleList->Length();
    if (nodeLength <= 0) {
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::IS_EMPTY_ERROR, "Container is empty");
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    return doubleList->RemoveFirst(thread);
}

JSTaggedValue JSAPILinkedList::RemoveLast(JSThread *thread, const JSHandle<JSAPILinkedList> &list)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    int nodeLength = doubleList->Length();
    if (nodeLength <= 0) {
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::IS_EMPTY_ERROR, "Container is empty");
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    return doubleList->RemoveLast(thread);
}

JSTaggedValue JSAPILinkedList::RemoveByIndex(JSThread *thread, JSHandle<JSAPILinkedList> &list, const int index)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    int nodeLength = doubleList->Length();
    if (nodeLength <= 0) {
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::RANGE_ERROR, "Container is empty");
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    if (index < 0 || index >= nodeLength) {
        int size = (nodeLength > 0) ? (nodeLength - 1) : 0;
        std::ostringstream oss;
        oss << "The value of \"index\" is out of range. It must be >= 0 && <= " << size
            << ". Received value is: " << index;
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::RANGE_ERROR, oss.str().c_str());
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    return doubleList->RemoveByIndex(thread, index);
}

JSTaggedValue JSAPILinkedList::Remove(JSThread *thread, const JSTaggedValue &element)
{
    TaggedDoubleList *doubleList = TaggedDoubleList::Cast(GetDoubleList().GetTaggedObject());
    int nodeLength = doubleList->Length();
    if (nodeLength < 0) {
        return JSTaggedValue::False();
    }
    return doubleList->Remove(thread, element);
}

JSTaggedValue JSAPILinkedList::RemoveFirstFound(JSThread *thread, JSHandle<JSAPILinkedList> &list,
                                                const JSTaggedValue &element)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    int nodeLength = doubleList->Length();
    if (nodeLength <= 0) {
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::IS_EMPTY_ERROR, "Container is empty");
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    return TaggedDoubleList::RemoveFirstFound(thread, doubleList, element);
}

JSTaggedValue JSAPILinkedList::RemoveLastFound(JSThread *thread, JSHandle<JSAPILinkedList> &list,
                                               const JSTaggedValue &element)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    int nodeLength = doubleList->Length();
    if (nodeLength < 0) {
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::IS_EMPTY_ERROR, "Container is empty");
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    return TaggedDoubleList::RemoveLastFound(thread, doubleList, element);
}

void JSAPILinkedList::Add(JSThread *thread, const JSHandle<JSAPILinkedList> &list, const JSHandle<JSTaggedValue> &value)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    JSTaggedValue newLinkedList = TaggedDoubleList::Add(thread, doubleList, value);
    list->SetDoubleList(thread, newLinkedList);
}

void JSAPILinkedList::AddFirst(JSThread *thread, const JSHandle<JSAPILinkedList> &list,
                               const JSHandle<JSTaggedValue> &value)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    JSTaggedValue newLinkedList = TaggedDoubleList::AddFirst(thread, doubleList, value);
    list->SetDoubleList(thread, newLinkedList);
}

JSTaggedValue JSAPILinkedList::GetFirst()
{
    JSTaggedValue res = TaggedDoubleList::Cast(GetDoubleList().GetTaggedObject())->GetFirst();
    if (res.IsHole()) {
        return JSTaggedValue::Undefined();
    }
    return res;
}

JSTaggedValue JSAPILinkedList::GetLast()
{
    JSTaggedValue res = TaggedDoubleList::Cast(GetDoubleList().GetTaggedObject())->GetLast();
    if (res.IsHole()) {
        return JSTaggedValue::Undefined();
    }
    return res;
}

JSTaggedValue JSAPILinkedList::Set(JSThread *thread, const JSHandle<JSAPILinkedList> &list,
                                   const int index, const JSHandle<JSTaggedValue> &value)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    int nodeLength = doubleList->Length();
    if (nodeLength <= 0) {
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::RANGE_ERROR, "Container is empty");
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    if (index < 0 || index >= nodeLength) {
        std::ostringstream oss;
        oss << "The value of \"index\" is out of range. It must be >= 0 && <= " << (nodeLength - 1)
            << ". Received value is: " << index;
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::RANGE_ERROR, oss.str().c_str());
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, JSTaggedValue::Exception());
    }
    TaggedDoubleList::Set(thread, doubleList, index, value);
    RETURN_EXCEPTION_IF_ABRUPT_COMPLETION(thread);
    return value.GetTaggedValue();
}

bool JSAPILinkedList::Has(const JSTaggedValue &element)
{
    TaggedDoubleList *doubleList = TaggedDoubleList::Cast(GetDoubleList().GetTaggedObject());
    return doubleList->Has(element);
}

JSTaggedValue JSAPILinkedList::Get(const int index)
{
    TaggedDoubleList *doubleList = TaggedDoubleList::Cast(GetDoubleList().GetTaggedObject());
    int nodeLength = doubleList->Length();
    if (index < 0 || index >= nodeLength) {
        return JSTaggedValue::Undefined();
    }
    return doubleList->Get(index);
}

JSTaggedValue JSAPILinkedList::GetIndexOf(const JSTaggedValue &element)
{
    TaggedDoubleList *doubleList = TaggedDoubleList::Cast(GetDoubleList().GetTaggedObject());
    return JSTaggedValue(doubleList->GetIndexOf(element));
}

JSTaggedValue JSAPILinkedList::GetLastIndexOf(const JSTaggedValue &element)
{
    TaggedDoubleList *doubleList = TaggedDoubleList::Cast(GetDoubleList().GetTaggedObject());
    return JSTaggedValue(doubleList->GetLastIndexOf(element));
}

JSTaggedValue JSAPILinkedList::ConvertToArray(const JSThread *thread, const JSHandle<JSAPILinkedList> &list)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    return TaggedDoubleList::ConvertToArray(thread, doubleList);
}

JSHandle<TaggedArray> JSAPILinkedList::OwnKeys(JSThread *thread, const JSHandle<JSAPILinkedList> &list)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList().GetTaggedObject());
    return TaggedDoubleList::OwnKeys(thread, doubleList);
}

bool JSAPILinkedList::GetOwnProperty(JSThread *thread, const JSHandle<JSAPILinkedList> &list,
                                     const JSHandle<JSTaggedValue> &key)
{
    uint32_t index = 0;
    if (UNLIKELY(!JSTaggedValue::ToElementIndex(key.GetTaggedValue(), &index))) {
        JSHandle<EcmaString> result = JSTaggedValue::ToString(thread, key.GetTaggedValue());
        RETURN_VALUE_IF_ABRUPT_COMPLETION(thread, false);
        CString errorMsg =
            "The type of \"index\" can not obtain attributes of no-number type. Received value is: "
            + ConvertToString(*result);
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::TYPE_ERROR, errorMsg.c_str());
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, false);
    }
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    uint32_t length = static_cast<uint32_t>(doubleList->Length());
    if (index >= length) {
        ASSERT(length > 0);
        std::ostringstream oss;
        oss << "The value of \"index\" is out of range. It must be > " << (length - 1)
            << ". Received value is: " << index;
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::RANGE_ERROR, oss.str().c_str());
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, false);
    }

    list->Get(index);
    return true;
}

OperationResult JSAPILinkedList::GetProperty(JSThread *thread, const JSHandle<JSAPILinkedList> &list,
                                             const JSHandle<JSTaggedValue> &key)
{
    JSHandle<TaggedDoubleList> doubleList(thread, list->GetDoubleList());
    int nodeLength = doubleList->Length();
    int index = key->GetInt();
    if (index < 0 || index >= nodeLength) {
        std::ostringstream oss;
        oss << "The value of \"index\" is out of range. It must be >= 0 && <= " << (nodeLength - 1)
            << ". Received value is: " << index;
        JSTaggedValue error = ContainerError::BusinessError(thread, ErrorFlag::RANGE_ERROR, oss.str().c_str());
        THROW_NEW_ERROR_AND_RETURN_VALUE(thread, error, OperationResult(thread,
                                                                        JSTaggedValue::Exception(),
                                                                        PropertyMetaData(false)));
    }

    return OperationResult(thread, doubleList->Get(index), PropertyMetaData(false));
}

bool JSAPILinkedList::SetProperty(JSThread *thread, const JSHandle<JSAPILinkedList> &obj,
                                  const JSHandle<JSTaggedValue> &key,
                                  const JSHandle<JSTaggedValue> &value)
{
    JSHandle<TaggedDoubleList> doubleList(thread, obj->GetDoubleList());
    int nodeLength = doubleList->Length();
    JSHandle<JSTaggedValue> indexKey = key;
    if (indexKey->IsDouble()) {
        indexKey = JSHandle<JSTaggedValue>(thread, JSTaggedValue::TryCastDoubleToInt32(indexKey->GetDouble()));
    }
    if (!indexKey->IsInt()) {
        return false;
    }
    int index = indexKey->GetInt();
    if (index < 0 || index >= nodeLength) {
        return false;
    }

    TaggedDoubleList::Set(thread, doubleList, index, value);
    return true;
}
}  // namespace panda::ecmascript
