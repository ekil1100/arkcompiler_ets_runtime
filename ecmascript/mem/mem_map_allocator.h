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

#ifndef ECMASCRIPT_MEM_MEM_MAP_ALLOCATOR_H
#define ECMASCRIPT_MEM_MEM_MAP_ALLOCATOR_H

#include <deque>
#include <map>
#include <set>

#include "ecmascript/platform/map.h"
#include "ecmascript/mem/mem.h"
#include "ecmascript/mem/mem_common.h"
#include "ecmascript/log_wrapper.h"

#include "ecmascript/platform/mutex.h"

namespace panda::ecmascript {
// Regular region with length of DEFAULT_REGION_SIZE(256kb)
class MemMapPool {
public:
    MemMapPool() = default;
    ~MemMapPool() = default;

    void Finalize()
    {
        LockHolder lock(lock_);
        for (auto &it : memMapVector_) {
            PageUnmap(it);
        }
        for (auto &it : regularMapCommitted_) {
            PageUnmap(it);
        }
        regularMapCommitted_.clear();
        memMapVector_.clear();
        memMapCache_.clear();
    }

    NO_COPY_SEMANTIC(MemMapPool);
    NO_MOVE_SEMANTIC(MemMapPool);

    MemMap GetMemFromCache([[maybe_unused]] size_t size)
    {
        ASSERT(size == REGULAR_MMAP_SIZE);
        LockHolder lock(lock_);
        if (!memMapCache_.empty()) {
            MemMap mem = memMapCache_.front();
            memMapCache_.pop_front();
            return mem;
        }
        return MemMap();
    }

    MemMap GetRegularMemFromCommitted([[maybe_unused]] size_t size)
    {
        ASSERT(size == REGULAR_MMAP_SIZE);
        LockHolder lock(lock_);
        if (!regularMapCommitted_.empty()) {
            MemMap mem = regularMapCommitted_.back();
            regularMapCommitted_.pop_back();
            return mem;
        }
        return MemMap();
    }

    bool IsRegularCommittedFull(size_t cachedSize) {
        LockHolder lock(lock_);
        size_t size = regularMapCommitted_.size();
        return size > (cachedSize / REGULAR_MMAP_SIZE) ? true : false;
    }

    int ShouldFreeMore(size_t cachedSize) {
        LockHolder lock(lock_);
        int result = static_cast<int>(regularMapCommitted_.size());
        return result - static_cast<int>(cachedSize / REGULAR_MMAP_SIZE);
    }

    void AddMemToCommittedCache(void *mem, size_t size)
    {
        ASSERT(size == REGULAR_MMAP_SIZE);
        LockHolder lock(lock_);
        regularMapCommitted_.emplace_back(mem, size);
    }


    void AddMemToCache(void *mem, size_t size)
    {
        ASSERT(size == REGULAR_MMAP_SIZE);
        LockHolder lock(lock_);
        memMapCache_.emplace_back(mem, size);
    }

    MemMap SplitMemFromCache(MemMap memMap)
    {
        LockHolder lock(lock_);
        auto remainderMem = reinterpret_cast<uintptr_t>(memMap.GetMem()) + REGULAR_MMAP_SIZE;
        size_t remainderSize = AlignDown(memMap.GetSize() - REGULAR_MMAP_SIZE, REGULAR_MMAP_SIZE);
        size_t count = remainderSize / REGULAR_MMAP_SIZE;
        while (count-- > 0) {
            memMapCache_.emplace_back(reinterpret_cast<void *>(remainderMem), REGULAR_MMAP_SIZE);
            remainderMem = remainderMem + REGULAR_MMAP_SIZE;
        }
        return MemMap(memMap.GetMem(), REGULAR_MMAP_SIZE);
    }

    void InsertMemMap(MemMap memMap)
    {
        LockHolder lock(lock_);
        memMapVector_.emplace_back(memMap);
    }

private:
    static constexpr size_t REGULAR_MMAP_SIZE = 256_KB;
    Mutex lock_;
    std::deque<MemMap> memMapCache_;
    std::vector<MemMap> regularMapCommitted_;
    std::vector<MemMap> memMapVector_;
};

// Non regular region with length of DEFAULT_REGION_SIZE(256kb) multiple
class MemMapFreeList {
public:
    MemMapFreeList() = default;
    ~MemMapFreeList() = default;

    void Initialize(MemMap memMap, size_t capacity)
    {
        memMaps_.emplace_back(memMap);
        freeList_.emplace(memMap.GetSize(), memMap);
        capacity_ = capacity;
    }

    void Finalize()
    {
        for (auto &memMap : memMaps_) {
            PageUnmap(memMap);
        }
        memMaps_.clear();
        freeList_.clear();
    }

    NO_COPY_SEMANTIC(MemMapFreeList);
    NO_MOVE_SEMANTIC(MemMapFreeList);

    void MergeList()
    {
        auto it = freeList_.begin();
        while (it != freeList_.end()) {
            bool isEqual = false;
            void *startMem = (*it).second.GetMem();
            size_t newSize = (*it).second.GetSize();
            auto startIt = it++;
            if (it == freeList_.end()) {
                break;
            }
            auto next = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(startMem) + newSize);
            while (it != freeList_.end() && next == (*it).second.GetMem()) {
                newSize += (*it).second.GetSize();
                it = freeList_.erase(it);
                next = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(startMem) + newSize);
                isEqual = true;
            }
            if (isEqual) {
                freeList_.erase(startIt);
                freeList_.emplace(newSize, MemMap(startMem, newSize));
            }
        }
    }

    MemMap GetMemFromList(size_t size)
    {
        if (freeListPoolSize_ + size > capacity_) {
            LOG_GC(ERROR) << "Freelist pool oom: overflow(" << freeListPoolSize_ << ")";
            return MemMap();
        }
        LockHolder lock(lock_);
        auto iterate = freeList_.lower_bound(size);
        if (iterate == freeList_.end()) {
            MergeList();
            iterate = freeList_.lower_bound(size);
            // Unable to get memory from freeList, use PageMap
            if (iterate == freeList_.end()) {
                size_t incrementCapacity = std::max(size, INCREMENT_HUGE_OBJECT_CAPACITY);
                MemMap smemMap = PageMap(incrementCapacity, PAGE_PROT_NONE, DEFAULT_REGION_SIZE);
                LOG_GC(INFO) << "Huge object mem pool increase PageMap size: " << smemMap.GetSize();
                memMaps_.emplace_back(smemMap);
                freeList_.emplace(smemMap.GetSize(), smemMap);
                iterate = freeList_.lower_bound(size);
                ASSERT(iterate != freeList_.end());
            }
        }
        MemMap memMap = iterate->second;
        size_t remainderSize = memMap.GetSize() - size;
        freeList_.erase(iterate);
        if (remainderSize >= DEFAULT_REGION_SIZE) {
            auto next = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(memMap.GetMem()) + size);
            freeList_.emplace(remainderSize, MemMap(next, remainderSize));
        }
        freeListPoolSize_ += size;
        return MemMap(memMap.GetMem(), size);
    }

    void AddMemToList(MemMap memMap)
    {
        LockHolder lock(lock_);
        freeListPoolSize_ -= memMap.GetSize();
        freeList_.emplace(memMap.GetSize(), memMap);
    }

private:
    Mutex lock_;
    std::vector<MemMap> memMaps_;
    std::multimap<size_t, MemMap> freeList_;
    std::atomic_size_t freeListPoolSize_ {0};
    size_t capacity_ {0};
};

class MemMapAllocator {
public:
    MemMapAllocator() = default;
    ~MemMapAllocator() = default;

    NO_COPY_SEMANTIC(MemMapAllocator);
    NO_MOVE_SEMANTIC(MemMapAllocator);

    void Initialize(size_t alignment)
    {
        AdapterSuitablePoolCapacity();
        memMapTotalSize_ = 0;
        size_t initialHugeObjectCapacity = std::min(capacity_ / 2, INITIAL_HUGE_OBJECT_CAPACITY);
        MemMap memMap = PageMap(initialHugeObjectCapacity, PAGE_PROT_NONE, alignment);
        PageTag(memMap.GetMem(), memMap.GetSize(), PageTagType::MEMPOOL_CACHE);
        PageRelease(memMap.GetMem(), memMap.GetSize());
        memMapFreeList_.Initialize(memMap, capacity_);
    }

    void Finalize()
    {
        memMapTotalSize_ = 0;
        capacity_ = 0;
        memMapFreeList_.Finalize();
        memMapPool_.Finalize();
    }

    size_t GetCapacity()
    {
        return capacity_;
    }

    void IncreaseAndCheckReserved(size_t size)
    {
        if (reserved_ + size > capacity_) {
            LOG_GC(ERROR) << "pool is empty, reserved = " << reserved_ << ", capacity_ = " <<
                capacity_ << ", size = " << size;
        }
        reserved_ += size;
        LOG_GC(DEBUG) << "Ark IncreaseAndCheckReserved reserved = " << reserved_ << ", capacity_ = " << capacity_;
    }

    void DecreaseReserved(size_t size)
    {
        reserved_ -= size;
        LOG_GC(DEBUG) << "Ark DecreaseReserved reserved = " << reserved_ << ", capacity_ = " << capacity_;
    }

    static MemMapAllocator *GetInstance();

    MemMap Allocate(const uint32_t threadId, size_t size, size_t alignment, bool regular, bool isMachineCode);

    void CacheOrFree(void *mem, size_t size, bool isRegular, size_t cachedSize);

private:
    static constexpr size_t REGULAR_REGION_MMAP_SIZE = 4_MB;

    void AdapterSuitablePoolCapacity();
    void Free(void *mem, size_t size, bool isRegular);
    MemMapPool memMapPool_;
    MemMapFreeList memMapFreeList_;
    std::atomic_size_t memMapTotalSize_ {0};
    size_t capacity_ {0};
    size_t reserved_ {0};
};
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_MEM_MEM_MAP_ALLOCATOR_H
