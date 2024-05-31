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

#ifndef ECMASCRIPT_MEM_REGION_INL_H
#define ECMASCRIPT_MEM_REGION_INL_H

#include "ecmascript/mem/region.h"

#include "ecmascript/js_thread.h"
#include "ecmascript/mem/mem.h"
#include "ecmascript/mem/native_area_allocator.h"

namespace panda::ecmascript {
inline RememberedSet *Region::CreateRememberedSet()
{
    auto bitSize = GCBitset::SizeOfGCBitset(GetCapacity());
    auto setAddr = nativeAreaAllocator_->Allocate(bitSize + RememberedSet::GCBITSET_DATA_OFFSET);
    auto ret = new (setAddr) RememberedSet(bitSize);
    ret->ClearAll();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return ret;
}

inline RememberedSet *Region::GetOrCreateCrossRegionRememberedSet()
{
    if (UNLIKELY(crossRegionSet_ == nullptr)) {
        LockHolder lock(*lock_);
        if (crossRegionSet_ == nullptr) {
            crossRegionSet_ = CreateRememberedSet();
        }
    }
    return crossRegionSet_;
}

inline RememberedSet *Region::GetOrCreateNewToEdenRememberedSet()
{
    if (UNLIKELY(packedData_.newToEdenSet_ == nullptr)) {
        LockHolder lock(*lock_);
        if (packedData_.newToEdenSet_ == nullptr) {
            packedData_.newToEdenSet_ = CreateRememberedSet();
        }
    }
    return packedData_.newToEdenSet_;
}

inline RememberedSet *Region::GetOrCreateOldToNewRememberedSet()
{
    if (UNLIKELY(packedData_.oldToNewSet_ == nullptr)) {
        LockHolder lock(*lock_);
        if (packedData_.oldToNewSet_ == nullptr) {
            if (sweepingOldToNewRSet_ != nullptr && IsGCFlagSet(RegionGCFlags::HAS_BEEN_SWEPT)) {
                packedData_.oldToNewSet_ = sweepingOldToNewRSet_;
                sweepingOldToNewRSet_ = nullptr;
            } else {
                packedData_.oldToNewSet_ = CreateRememberedSet();
            }
        }
    }
    return packedData_.oldToNewSet_;
}

inline RememberedSet *Region::GetOrCreateLocalToShareRememberedSet()
{
    if (UNLIKELY(packedData_.localToShareSet_ == nullptr)) {
        LockHolder lock(*lock_);
        if (packedData_.localToShareSet_ == nullptr) {
            if (sweepingLocalToShareRSet_ != nullptr && IsGCFlagSet(RegionGCFlags::HAS_BEEN_SWEPT)) {
                packedData_.localToShareSet_ = sweepingLocalToShareRSet_;
                sweepingLocalToShareRSet_ = nullptr;
            } else {
                packedData_.localToShareSet_ = CreateRememberedSet();
            }
        }
    }
    return packedData_.localToShareSet_;
}

inline void Region::MergeLocalToShareRSetForCS()
{
    if (sweepingLocalToShareRSet_ == nullptr) {
        return;
    }
    if (packedData_.localToShareSet_ == nullptr) {
        packedData_.localToShareSet_ = sweepingLocalToShareRSet_;
        sweepingLocalToShareRSet_ = nullptr;
    } else {
        packedData_.localToShareSet_->Merge(sweepingLocalToShareRSet_);
        DeleteSweepingLocalToShareRSet();
        sweepingLocalToShareRSet_ = nullptr;
    }
}

inline void Region::MergeOldToNewRSetForCS()
{
    if (sweepingOldToNewRSet_ == nullptr) {
        return;
    }
    if (packedData_.oldToNewSet_ == nullptr) {
        packedData_.oldToNewSet_ = sweepingOldToNewRSet_;
        sweepingOldToNewRSet_   = nullptr;
    } else {
        packedData_.oldToNewSet_->Merge(sweepingOldToNewRSet_);
        DeleteSweepingOldToNewRSet();
        sweepingOldToNewRSet_ = nullptr;
    }
}

inline GCBitset *Region::GetMarkGCBitset() const
{
    return packedData_.markGCBitset_;
}

inline bool Region::AtomicMark(void *address)
{
    auto addrPtr = reinterpret_cast<uintptr_t>(address);
    ASSERT(InRange(addrPtr));
    return packedData_.markGCBitset_->SetBit<AccessType::ATOMIC>(
        (addrPtr & DEFAULT_REGION_MASK) >> TAGGED_TYPE_SIZE_LOG);
}

inline void Region::ClearMark(void *address)
{
    auto addrPtr = reinterpret_cast<uintptr_t>(address);
    ASSERT(InRange(addrPtr));
    packedData_.markGCBitset_->ClearBit((addrPtr & DEFAULT_REGION_MASK) >> TAGGED_TYPE_SIZE_LOG);
}

inline bool Region::Test(void *addr) const
{
    auto addrPtr = reinterpret_cast<uintptr_t>(addr);
    ASSERT(InRange(addrPtr));
    return packedData_.markGCBitset_->TestBit((addrPtr & DEFAULT_REGION_MASK) >> TAGGED_TYPE_SIZE_LOG);
}

// ONLY used for heap verification.
inline bool Region::TestNewToEden(uintptr_t addr)
{
    ASSERT(InRange(addr));
    // Only used for heap verification, so donot need to use lock
    auto set = packedData_.newToEdenSet_;
    if (set == nullptr) {
        return false;
    }
    return set->TestBit(ToUintPtr(this), addr);
}

// ONLY used for heap verification.
inline bool Region::TestOldToNew(uintptr_t addr)
{
    ASSERT(InRange(addr));
    // Only used for heap verification, so donot need to use lock
    auto set = packedData_.oldToNewSet_;
    if (set == nullptr) {
        return false;
    }
    return set->TestBit(ToUintPtr(this), addr);
}

// ONLY used for heap verification.
inline bool Region::TestLocalToShare(uintptr_t addr)
{
    ASSERT(InRange(addr));
    // Only used for heap verification, so donot need to use lock
    if (packedData_.localToShareSet_ == nullptr) {
        return false;
    }
    return packedData_.localToShareSet_->TestBit(ToUintPtr(this), addr);
}

template <typename Visitor>
inline void Region::IterateAllMarkedBits(Visitor visitor) const
{
    packedData_.markGCBitset_->IterateMarkedBitsConst(
        reinterpret_cast<uintptr_t>(this), packedData_.bitsetSize_, visitor);
}

inline void Region::ClearMarkGCBitset()
{
    if (packedData_.markGCBitset_ != nullptr) {
        packedData_.markGCBitset_->Clear(packedData_.bitsetSize_);
    }
}

inline void Region::InsertCrossRegionRSet(uintptr_t addr)
{
    auto set = GetOrCreateCrossRegionRememberedSet();
    set->Insert(ToUintPtr(this), addr);
}

inline void Region::AtomicInsertCrossRegionRSet(uintptr_t addr)
{
    auto set = GetOrCreateCrossRegionRememberedSet();
    set->AtomicInsert(ToUintPtr(this), addr);
}

inline bool Region::HasLocalToShareRememberedSet() const
{
    return packedData_.localToShareSet_ != nullptr;
}

inline void Region::InsertLocalToShareRSet(uintptr_t addr)
{
    auto set = GetOrCreateLocalToShareRememberedSet();
    set->Insert(ToUintPtr(this), addr);
}

inline void Region::AtomicInsertLocalToShareRSet(uintptr_t addr)
{
    auto set = GetOrCreateLocalToShareRememberedSet();
    set->AtomicInsert(ToUintPtr(this), addr);
}

inline void Region::ClearLocalToShareRSetInRange(uintptr_t start, uintptr_t end)
{
    if (packedData_.localToShareSet_ != nullptr) {
        packedData_.localToShareSet_->ClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::AtomicClearLocalToShareRSetInRange(uintptr_t start, uintptr_t end)
{
    if (packedData_.localToShareSet_ != nullptr) {
        packedData_.localToShareSet_->AtomicClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::DeleteLocalToShareRSet()
{
    if (packedData_.localToShareSet_ != nullptr) {
        nativeAreaAllocator_->Free(packedData_.localToShareSet_, packedData_.localToShareSet_->Size());
        packedData_.localToShareSet_ = nullptr;
    }
}

inline void Region::AtomicClearSweepingLocalToShareRSetInRange(uintptr_t start, uintptr_t end)
{
    if (sweepingLocalToShareRSet_ != nullptr) {
        sweepingLocalToShareRSet_->AtomicClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::DeleteSweepingLocalToShareRSet()
{
    if (sweepingLocalToShareRSet_!= nullptr) {
        nativeAreaAllocator_->Free(sweepingLocalToShareRSet_, sweepingLocalToShareRSet_->Size());
        sweepingLocalToShareRSet_ = nullptr;
    }
}

template <typename Visitor>
inline void Region::IterateAllLocalToShareBits(Visitor visitor)
{
    if (packedData_.localToShareSet_ != nullptr) {
        packedData_.localToShareSet_->IterateAllMarkedBits(ToUintPtr(this), visitor);
    }
}

template <typename Visitor>
inline void Region::IterateAllCrossRegionBits(Visitor visitor) const
{
    if (crossRegionSet_ != nullptr) {
        crossRegionSet_->IterateAllMarkedBitsConst(ToUintPtr(this), visitor);
    }
}

inline void Region::ClearCrossRegionRSet()
{
    if (crossRegionSet_ != nullptr) {
        crossRegionSet_->ClearAll();
    }
}

inline void Region::ClearCrossRegionRSetInRange(uintptr_t start, uintptr_t end)
{
    if (crossRegionSet_ != nullptr) {
        crossRegionSet_->ClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::AtomicClearCrossRegionRSetInRange(uintptr_t start, uintptr_t end)
{
    if (crossRegionSet_ != nullptr) {
        crossRegionSet_->AtomicClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::DeleteCrossRegionRSet()
{
    if (crossRegionSet_ != nullptr) {
        nativeAreaAllocator_->Free(crossRegionSet_, crossRegionSet_->Size());
        crossRegionSet_ = nullptr;
    }
}

inline void Region::InsertNewToEdenRSet(uintptr_t addr)
{
    auto set = GetOrCreateNewToEdenRememberedSet();
    set->Insert(ToUintPtr(this), addr);
}

inline void Region::AtomicInsertNewToEdenRSet(uintptr_t addr)
{
    auto set = GetOrCreateNewToEdenRememberedSet();
    set->AtomicInsert(ToUintPtr(this), addr);
}

inline void Region::ClearNewToEdenRSet(uintptr_t addr)
{
    auto set = GetOrCreateNewToEdenRememberedSet();
    set->ClearBit(ToUintPtr(this), addr);
}

inline void Region::InsertOldToNewRSet(uintptr_t addr)
{
    auto set = GetOrCreateOldToNewRememberedSet();
    set->Insert(ToUintPtr(this), addr);
}

inline void Region::ClearOldToNewRSet(uintptr_t addr)
{
    auto set = GetOrCreateOldToNewRememberedSet();
    set->ClearBit(ToUintPtr(this), addr);
}

template <typename Visitor>
inline void Region::IterateAllNewToEdenBits(Visitor visitor)
{
    if (packedData_.newToEdenSet_ != nullptr) {
        packedData_.newToEdenSet_->IterateAllMarkedBits(ToUintPtr(this), visitor);
    }
}

template <typename Visitor>
inline void Region::IterateAllOldToNewBits(Visitor visitor)
{
    if (packedData_.oldToNewSet_ != nullptr) {
        packedData_.oldToNewSet_->IterateAllMarkedBits(ToUintPtr(this), visitor);
    }
}

template <typename Visitor>
inline void Region::AtomicIterateAllSweepingRSetBits(Visitor visitor)
{
    if (sweepingOldToNewRSet_ != nullptr) {
        sweepingOldToNewRSet_->AtomicIterateAllMarkedBits(ToUintPtr(this), visitor);
    }
}

template <typename Visitor>
inline void Region::IterateAllSweepingRSetBits(Visitor visitor)
{
    if (sweepingOldToNewRSet_ != nullptr) {
        sweepingOldToNewRSet_->IterateAllMarkedBits(ToUintPtr(this), visitor);
    }
}

inline RememberedSet *Region::GetNewToEdenRSet()
{
    return  packedData_.newToEdenSet_;
}

inline void Region::ClearNewToEdenRSet()
{
    if (packedData_.newToEdenSet_ != nullptr) {
        packedData_.newToEdenSet_->ClearAll();
    }
}

inline void Region::ClearNewToEdenRSetInRange(uintptr_t start, uintptr_t end)
{
    if (packedData_.newToEdenSet_ != nullptr) {
        packedData_.newToEdenSet_->ClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::DeleteNewToEdenRSet()
{
    if (packedData_.newToEdenSet_ != nullptr) {
        nativeAreaAllocator_->Free(packedData_.newToEdenSet_, packedData_.newToEdenSet_->Size());
        packedData_.newToEdenSet_ = nullptr;
    }
}

inline void Region::ClearOldToNewRSet()
{
    if (packedData_.oldToNewSet_ != nullptr) {
        packedData_.oldToNewSet_->ClearAll();
    }
}

inline void Region::ClearOldToNewRSetInRange(uintptr_t start, uintptr_t end)
{
    if (packedData_.oldToNewSet_ != nullptr) {
        packedData_.oldToNewSet_->ClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::DeleteOldToNewRSet()
{
    if (packedData_.oldToNewSet_ != nullptr) {
        nativeAreaAllocator_->Free(packedData_.oldToNewSet_, packedData_.oldToNewSet_->Size());
        packedData_.oldToNewSet_ = nullptr;
    }
}

inline void Region::AtomicClearSweepingOldToNewRSetInRange(uintptr_t start, uintptr_t end)
{
    if (sweepingOldToNewRSet_ != nullptr) {
        sweepingOldToNewRSet_->AtomicClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::ClearSweepingOldToNewRSetInRange(uintptr_t start, uintptr_t end)
{
    if (sweepingOldToNewRSet_ != nullptr) {
        sweepingOldToNewRSet_->ClearRange(ToUintPtr(this), start, end);
    }
}

inline void Region::DeleteSweepingOldToNewRSet()
{
    if (sweepingOldToNewRSet_ != nullptr) {
        nativeAreaAllocator_->Free(sweepingOldToNewRSet_, sweepingOldToNewRSet_->Size());
        sweepingOldToNewRSet_ = nullptr;
    }
}
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_MEM_REGION_INL_H
