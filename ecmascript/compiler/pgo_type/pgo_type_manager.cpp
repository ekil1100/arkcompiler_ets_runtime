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

#include "ecmascript/compiler/pgo_type/pgo_type_manager.h"

#include "ecmascript/ecma_vm.h"
#include "ecmascript/jspandafile/program_object.h"
#include "ecmascript/layout_info-inl.h"
#include "ecmascript/object_factory.h"
#include "index_accessor.h"

namespace panda::ecmascript::kungfu {
void PGOTypeManager::Iterate(const RootVisitor &v)
{
    for (auto &iter : hcData_) {
        for (auto &hclassIter : iter.second) {
            v(Root::ROOT_VM, ObjectSlot(reinterpret_cast<uintptr_t>(&(hclassIter.second))));
        }
    }
    aotSnapshot_.Iterate(v);
    for (auto &iter : hclassInfoLocal_) {
        v(Root::ROOT_VM, ObjectSlot(reinterpret_cast<uintptr_t>(&(iter))));
    }
}

uint32_t PGOTypeManager::GetConstantPoolIDByMethodOffset(const uint32_t methodOffset) const
{
    ASSERT(curJSPandaFile_!=nullptr);
    panda_file::IndexAccessor indexAccessor(*curJSPandaFile_->GetPandaFile(),
                                            panda_file::File::EntityId(methodOffset));
    return static_cast<uint32_t>(indexAccessor.GetHeaderIndex());
}

JSTaggedValue PGOTypeManager::GetConstantPoolByMethodOffset(const uint32_t methodOffset) const
{
    uint32_t cpId = GetConstantPoolIDByMethodOffset(methodOffset);
    return thread_->GetCurrentEcmaContext()->FindConstpool(curJSPandaFile_, cpId);
}

JSTaggedValue PGOTypeManager::GetStringFromConstantPool(const uint32_t methodOffset, const uint16_t cpIdx) const
{
    JSTaggedValue cp = GetConstantPoolByMethodOffset(methodOffset);
    return ConstantPool::GetStringFromCache(thread_, cp, cpIdx);
}

void PGOTypeManager::InitAOTSnapshot(uint32_t compileFilesCount)
{
    aotSnapshot_.InitSnapshot(compileFilesCount);
    GenHClassInfo();
    GenSymbolInfo();
    GenArrayInfo();
    GenConstantIndexInfo();
}

uint32_t PGOTypeManager::GetSymbolCountFromHClassData()
{
    uint32_t count = 0;
    for (auto& root: hcData_) {
        for (auto& child: root.second) {
            if (!JSTaggedValue(child.second).IsJSHClass()) {
                continue;
            }
            JSHClass* hclass = JSHClass::Cast(JSTaggedValue(child.second).GetTaggedObject());
            if (!hclass->HasTransitions()) {
                LayoutInfo* layoutInfo = LayoutInfo::GetLayoutInfoFromHClass(hclass);
                uint32_t len = hclass->NumberOfProps();
                for (uint32_t i = 0; i < len; i++) {
                    JSTaggedValue key = layoutInfo->GetKey(i);
                    if (key.IsSymbol()) {
                        count++;
                    }
                }
            }
        }
    }
    return count;
}

void PGOTypeManager::GenSymbolInfo()
{
    uint32_t count = GetSymbolCountFromHClassData();
    ObjectFactory* factory = thread_->GetEcmaVM()->GetFactory();
    JSHandle<TaggedArray> symbolInfo = factory->NewTaggedArray(count * 2); // 2: symbolId, symbol
    uint32_t pos = 0;

    for (auto& root: hcData_) {
        ProfileType rootType = root.first;
        for (auto& child: root.second) {
            if (!JSTaggedValue(child.second).IsJSHClass()) {
                continue;
            }
            ProfileType childType = child.first;
            JSHClass* hclass = JSHClass::Cast(JSTaggedValue(child.second).GetTaggedObject());
            if (!hclass->HasTransitions()) {
                LayoutInfo* layoutInfo = LayoutInfo::GetLayoutInfoFromHClass(hclass);
                uint32_t len = hclass->NumberOfProps();
                for (uint32_t i = 0; i < len; i++) {
                    JSTaggedValue symbol = layoutInfo->GetKey(i);
                    if (symbol.IsSymbol()) {
                        JSSymbol* symbolPtr = JSSymbol::Cast(symbol.GetTaggedObject());
                        uint64_t symbolId = symbolPtr->GetPrivateId();
                        uint64_t slotIndex = JSSymbol::GetSlotIndex(symbolId);
                        ProfileTypeTuple symbolIdKey = std::make_tuple(rootType, childType, slotIndex);
                        profileTypeToSymbolId_.emplace(symbolIdKey, symbolId);
                        symbolInfo->Set(thread_, pos++, JSTaggedValue(symbolId));
                        symbolInfo->Set(thread_, pos++, symbol);
                    }
                }
            }
        }
    }

    aotSnapshot_.StoreSymbolInfo(symbolInfo);
}

void PGOTypeManager::GenHClassInfo()
{
    uint32_t count = 1; // For object literal hclass cache
    for (auto& root: hcData_) {
        count += root.second.size();
    }

    ObjectFactory* factory = thread_->GetEcmaVM()->GetFactory();
    JSHandle<TaggedArray> hclassInfo = factory->NewTaggedArray(count);
    uint32_t pos = 0;
    profileTyperToHClassIndex_.clear();
    for (auto& root: hcData_) {
        ProfileType rootType = root.first;
        for (auto& child: root.second) {
            ProfileType childType = child.first;
            JSTaggedType hclass = child.second;
            ProfileTyper key = std::make_pair(rootType, childType);
            profileTyperToHClassIndex_.emplace(key, pos);
            hclassInfo->Set(thread_, pos++, JSTaggedValue(hclass));
        }
    }
    // The cache of Object literal serializes to last index of AOTHClassInfo.
    JSHandle<GlobalEnv> env = thread_->GetEcmaVM()->GetGlobalEnv();
    JSHandle<JSTaggedValue> maybeCache = env->GetObjectLiteralHClassCache();
    if (!maybeCache->IsHole()) {
        // It cannot be serialized if object in global env.
        JSHandle<TaggedArray> array(maybeCache);
        auto cloneResult = factory->CopyArray(array, array->GetLength(), array->GetLength());
        hclassInfo->Set(thread_, pos++, cloneResult);
    }

    aotSnapshot_.StoreHClassInfo(hclassInfo);
}


JSHandle<TaggedArray> PGOTypeManager::GenJITHClassInfo()
{
    uint32_t count = hclassInfoLocal_.size();
    ObjectFactory *factory = thread_->GetEcmaVM()->GetFactory();
    JSHandle<TaggedArray> hclassInfo = factory->NewTaggedArray(count);
    for (uint32_t pos = 0; pos < count; pos++) {
        hclassInfo->Set(thread_, pos, JSTaggedValue(hclassInfoLocal_[pos]));
    }
    return hclassInfo;
}

void PGOTypeManager::GenArrayInfo()
{
    ObjectFactory *factory = thread_->GetEcmaVM()->GetFactory();
    JSHandle<TaggedArray> arrayInfo = factory->EmptyArray();
    aotSnapshot_.StoreArrayInfo(arrayInfo);
}

void PGOTypeManager::GenConstantIndexInfo()
{
    uint32_t count = constantIndexData_.size();
    ObjectFactory *factory = thread_->GetEcmaVM()->GetFactory();
    JSHandle<TaggedArray> constantIndexInfo = factory->NewTaggedArray(count);
    for (uint32_t pos = 0; pos < count; pos++) {
        constantIndexInfo->Set(thread_, pos, JSTaggedValue(constantIndexData_[pos]));
    }
    aotSnapshot_.StoreConstantIndexInfo(constantIndexInfo);
}

void PGOTypeManager::RecordHClass(ProfileType rootType, ProfileType childType, JSTaggedType hclass, bool update)
{
    LockHolder lock(mutex_);
    auto iter = hcData_.find(rootType);
    if (iter == hcData_.end()) {
        auto map = TransIdToHClass();
        map.emplace(childType, hclass);
        hcData_.emplace(rootType, map);
        profileTyperToHClassIndex_.emplace(std::make_pair(rootType, childType), pos_++);
        hclassInfoLocal_.emplace_back(JSTaggedValue(hclass));
        return;
    }

    auto &hclassMap = iter->second;
    auto hclassIter = hclassMap.find(childType);
    if (hclassIter != hclassMap.end()) {
        if (!update) {
            ASSERT(hclass == hclassIter->second);
            return;
        } else {
            hclassMap[childType]= hclass;
            auto index = GetHClassIndexByProfileType(std::pair(rootType, childType));
            ASSERT(index >= 0);
            hclassInfoLocal_[index] = JSTaggedValue(hclass);
            return;
        }
    }
    profileTyperToHClassIndex_.emplace(std::make_pair(rootType, childType), pos_++);
    hclassInfoLocal_.emplace_back(JSTaggedValue(hclass));
    hclassMap.emplace(childType, hclass);
}

void PGOTypeManager::RecordConstantIndex(uint32_t bcAbsoluteOffset, uint32_t index)
{
    constantIndexData_.emplace_back(bcAbsoluteOffset);
    constantIndexData_.emplace_back(index);
}

uint32_t PGOTypeManager::GetHClassIndexByProfileType(ProfileTyper type) const
{
    uint32_t index = -1;
    auto iter = profileTyperToHClassIndex_.find(type);
    if (iter != profileTyperToHClassIndex_.end()) {
        index = iter->second;
    }
    return index;
}

std::optional<uint64_t> PGOTypeManager::GetSymbolIdByProfileType(ProfileTypeTuple type) const
{
    auto iter = profileTypeToSymbolId_.find(type);
    if (iter != profileTypeToSymbolId_.end()) {
        return iter->second;
    }
    return std::nullopt;
}

JSTaggedValue PGOTypeManager::QueryHClass(ProfileType rootType, ProfileType childType)
{
    LockHolder lock(mutex_);
    JSTaggedValue result = JSTaggedValue::Undefined();
    auto iter = hcData_.find(rootType);
    if (iter != hcData_.end()) {
        auto hclassMap = iter->second;
        auto hclassIter = hclassMap.find(childType);
        if (hclassIter != hclassMap.end()) {
            result = JSTaggedValue(hclassIter->second);
        }
    }
    return result;
}

JSTaggedValue PGOTypeManager::QueryHClassByIndexForJIT(uint32_t hclassIndex)
{
    ASSERT(hclassIndex < profileTyperToHClassIndex_.size());
    return hclassInfoLocal_[hclassIndex];
}

}  // namespace panda::ecmascript
