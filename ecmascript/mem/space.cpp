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

#include "ecmascript/mem/space-inl.h"

#include "ecmascript/ecma_vm.h"
#include "ecmascript/js_tagged_value-inl.h"
#include "ecmascript/mem/heap_region_allocator.h"
#include "ecmascript/mem/mem_controller.h"
#include "ecmascript/mem/region-inl.h"
#include "ecmascript/mem/space.h"
#include "ecmascript/mem/jit_fort.h"
#include "ecmascript/mem/machine_code.h"
#include "ecmascript/platform/os.h"

namespace panda::ecmascript {
Space::Space(BaseHeap* heap, HeapRegionAllocator *heapRegionAllocator,
             MemSpaceType spaceType, size_t initialCapacity,
             size_t maximumCapacity)
    : heap_(heap),
      heapRegionAllocator_(heapRegionAllocator),
      spaceType_(spaceType),
      initialCapacity_(initialCapacity),
      maximumCapacity_(maximumCapacity),
      committedSize_(0)
{
}

void Space::AddAllocationInspector(AllocationInspector* inspector)
{
    allocationCounter_.AddAllocationInspector(inspector);
}

void Space::ClearAllocationInspector()
{
    allocationCounter_.ClearAllocationInspector();
}

void Space::SwapAllocationCounter(Space *space)
{
    std::swap(allocationCounter_, space->allocationCounter_);
}

void Space::Destroy()
{
    ReclaimRegions();
}

void Space::ReclaimRegions(size_t cachedSize)
{
    EnumerateRegions([this, &cachedSize](Region *current) { ClearAndFreeRegion(current, cachedSize); });
    regionList_.Clear();
    committedSize_ = 0;
}

void Space::ClearAndFreeRegion(Region *region, size_t cachedSize)
{
    LOG_ECMA_MEM(DEBUG) << "Clear region from:" << region << " to " << ToSpaceTypeName(spaceType_);
    region->DeleteCrossRegionRSet();
    region->DeleteNewToEdenRSet();
    region->DeleteOldToNewRSet();
    region->DeleteLocalToShareRSet();
    region->DeleteSweepingOldToNewRSet();
    region->DeleteSweepingLocalToShareRSet();
    DecreaseCommitted(region->GetCapacity());
    DecreaseObjectSize(region->GetSize());
    if (spaceType_ == MemSpaceType::OLD_SPACE || spaceType_ == MemSpaceType::NON_MOVABLE ||
        spaceType_ == MemSpaceType::MACHINE_CODE_SPACE || spaceType_ == MemSpaceType::LOCAL_SPACE ||
        spaceType_ == MemSpaceType::APPSPAWN_SPACE || spaceType_ == MemSpaceType::SHARED_NON_MOVABLE ||
        spaceType_ == MemSpaceType::SHARED_OLD_SPACE) {
        region->DestroyFreeObjectSets();
    }
    // regions of EdenSpace are allocated in EdenSpace constructor and fixed, not allocate by heapRegionAllocator_
    if (spaceType_ != MemSpaceType::EDEN_SPACE) {
        heapRegionAllocator_->FreeRegion(region, cachedSize);
    }
}

HugeObjectSpace::HugeObjectSpace(Heap *heap, HeapRegionAllocator *heapRegionAllocator,
                                 size_t initialCapacity, size_t maximumCapacity)
    : Space(heap, heapRegionAllocator, MemSpaceType::HUGE_OBJECT_SPACE, initialCapacity, maximumCapacity)
{
}

HugeObjectSpace::HugeObjectSpace(Heap *heap, HeapRegionAllocator *heapRegionAllocator,
                                 size_t initialCapacity, size_t maximumCapacity, MemSpaceType spaceType)
    : Space(heap, heapRegionAllocator, spaceType, initialCapacity, maximumCapacity)
{
}

HugeMachineCodeSpace::HugeMachineCodeSpace(Heap *heap, HeapRegionAllocator *heapRegionAllocator,
                                           size_t initialCapacity, size_t maximumCapacity)
    : HugeObjectSpace(heap, heapRegionAllocator, initialCapacity,
        maximumCapacity, MemSpaceType::HUGE_MACHINE_CODE_SPACE)
{
}

uintptr_t HugeMachineCodeSpace::GetMachineCodeObject(uintptr_t pc) const
{
    uintptr_t machineCode = 0;
    EnumerateRegions([&](Region *region) {
        if (machineCode != 0) {
            return;
        }
        if (!region->InRange(pc)) {
            return;
        }
        uintptr_t curPtr = region->GetBegin();
        auto obj = MachineCode::Cast(reinterpret_cast<TaggedObject*>(curPtr));
        if (obj->IsInText(pc)) {
            machineCode = curPtr;
        }
    });
    return machineCode;
}

Region *HugeMachineCodeSpace::AllocateFort(size_t objectSize, JSThread *thread, void *pDesc)
{
    // A Huge machine code object is consisted of contiguous 256Kb aligned blocks.
    // For JitFort, a huge machine code object starts with a page aligned mutable area
    // (which holds Region and MachineCode object header, FuncEntryDesc and StackMap), followed
    // by a page aligned immutable (JitFort space) area for JIT generated native instructions code.
    //
    // allocation sizes for Huge Machine Code:
    //     a: mutable area size (aligned up to PageSize()) =
    //         sizeof(Region) + HUGE_OBJECT_BITSET_SIZE + MachineCode::SIZE + payLoadSize - instructionsSize
    //         (note: payLoadSize = funcDesc size + stackMap size + instructionsSize)
    //     b: immutable area (starts on native page boundary) size = instructionsSize
    //     c: size to mmap for huge machine code object = Alignup(a + b, 256 Kbyte)
    //
    // mmap to enable JIT_FORT rights control:
    //     1. first mmap (without JIT_FORT option flag) region of size c above
    //     2. then mmap immutable area with MAP_FIXED and JIT_FORT option flag (to be used by codesigner verify/copy)
    MachineCodeDesc *desc = reinterpret_cast<MachineCodeDesc *>(pDesc);
    size_t mutableSize = AlignUp(
        objectSize + sizeof(Region) + HUGE_OBJECT_BITSET_SIZE - desc->instructionsSize, PageSize());
    size_t allocSize = AlignUp(mutableSize + desc->instructionsSize, PANDA_POOL_ALIGNMENT_IN_BYTES);
    if (heap_->OldSpaceExceedCapacity(allocSize)) {
        LOG_ECMA_MEM(INFO) << "Committed size " << committedSize_ << " of huge object space is too big.";
        return 0;
    }
    Region *region = heapRegionAllocator_->AllocateAlignedRegion(this, allocSize, thread, heap_);
    desc->instructionsAddr = region->GetAllocateBase() + mutableSize;

    // Enabe JitFort rights control
    [[maybe_unused]] void *addr = PageMapExecFortSpace((void *)desc->instructionsAddr, allocSize - mutableSize,
        PageProtectProt(reinterpret_cast<Heap *>(heap_)->GetEcmaVM()->GetJSOptions().GetDisableCodeSign()));

    ASSERT(addr == (void *)desc->instructionsAddr);
    return region;
}


uintptr_t HugeMachineCodeSpace::Allocate(size_t objectSize, JSThread *thread, void *pDesc,
    AllocateEventType allocType)
{
    // JitFort path
#if ECMASCRIPT_ENABLE_THREAD_STATE_CHECK
    if (UNLIKELY(!thread->IsInRunningStateOrProfiling())) {
        LOG_ECMA(FATAL) << "Allocate must be in jsthread running state";
        UNREACHABLE();
    }
#endif
    if (allocType == AllocateEventType::NORMAL) {
        thread->CheckSafepointIfSuspended();
    }
    Region *region = reinterpret_cast<Heap *>(heap_)->GetEcmaVM()->GetJSOptions().GetEnableAsyncCopyToFort() ?
        reinterpret_cast<Region *>(reinterpret_cast<MachineCodeDesc *>(pDesc)->hugeObjRegion) :
        AllocateFort(objectSize, thread, pDesc);
    AddRegion(region);
    // It need to mark unpoison when huge object being allocated.
    ASAN_UNPOISON_MEMORY_REGION(reinterpret_cast<void *>(region->GetBegin()), objectSize);
#ifdef ECMASCRIPT_SUPPORT_HEAPSAMPLING
    InvokeAllocationInspector(region->GetBegin(), objectSize);
#endif
    return region->GetBegin();
}

uintptr_t HugeMachineCodeSpace::Allocate(size_t objectSize, JSThread *thread)
{
    // non JitFort path
    return HugeObjectSpace::Allocate(objectSize, thread);
}

uintptr_t HugeObjectSpace::Allocate(size_t objectSize, JSThread *thread, AllocateEventType allocType)
{
#if ECMASCRIPT_ENABLE_THREAD_STATE_CHECK
    if (UNLIKELY(!thread->IsInRunningStateOrProfiling())) {
        LOG_ECMA(FATAL) << "Allocate must be in jsthread running state";
        UNREACHABLE();
    }
#endif
    if (allocType == AllocateEventType::NORMAL) {
        thread->CheckSafepointIfSuspended();
    }
    // In HugeObject allocation, we have a revervation of 8 bytes for markBitSet in objectSize.
    // In case Region is not aligned by 16 bytes, HUGE_OBJECT_BITSET_SIZE is 8 bytes more.
    size_t alignedSize = AlignUp(objectSize + sizeof(Region) + HUGE_OBJECT_BITSET_SIZE, PANDA_POOL_ALIGNMENT_IN_BYTES);
    if (heap_->OldSpaceExceedCapacity(alignedSize)) {
        LOG_ECMA_MEM(INFO) << "Committed size " << committedSize_ << " of huge object space is too big.";
        return 0;
    }
    Region *region = heapRegionAllocator_->AllocateAlignedRegion(this, alignedSize, thread, heap_);
    AddRegion(region);
    // It need to mark unpoison when huge object being allocated.
    ASAN_UNPOISON_MEMORY_REGION(reinterpret_cast<void *>(region->GetBegin()), objectSize);
#ifdef ECMASCRIPT_SUPPORT_HEAPSAMPLING
    InvokeAllocationInspector(region->GetBegin(), objectSize);
#endif
    return region->GetBegin();
}

void HugeObjectSpace::Sweep()
{
    Region *currentRegion = GetRegionList().GetFirst();
    while (currentRegion != nullptr) {
        Region *next = currentRegion->GetNext();
        bool isMarked = false;
        currentRegion->IterateAllMarkedBits([&isMarked]([[maybe_unused]] void *mem) { isMarked = true; });
        if (!isMarked) {
            GetRegionList().RemoveNode(currentRegion);
            hugeNeedFreeList_.AddNode(currentRegion);
        }
        currentRegion = next;
    }
}

size_t HugeObjectSpace::GetHeapObjectSize() const
{
    return committedSize_;
}

void HugeObjectSpace::IterateOverObjects(const std::function<void(TaggedObject *object)> &objectVisitor) const
{
    EnumerateRegions([&](Region *region) {
        uintptr_t curPtr = region->GetBegin();
        objectVisitor(reinterpret_cast<TaggedObject *>(curPtr));
    });
}

void HugeObjectSpace::ReclaimHugeRegion()
{
    if (hugeNeedFreeList_.IsEmpty()) {
        return;
    }
    do {
        Region *last = hugeNeedFreeList_.PopBack();
        ClearAndFreeRegion(last);
    } while (!hugeNeedFreeList_.IsEmpty());
}

void HugeObjectSpace::InvokeAllocationInspector(Address object, size_t objectSize)
{
    if (LIKELY(!allocationCounter_.IsActive())) {
        return;
    }
    if (objectSize >= allocationCounter_.NextBytes()) {
        allocationCounter_.InvokeAllocationInspector(object, objectSize, objectSize);
    }
    allocationCounter_.AdvanceAllocationInspector(objectSize);
}
}  // namespace panda::ecmascript
