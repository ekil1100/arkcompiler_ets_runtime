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

#ifndef ECMASCRIPT_COMPILER_BITSET_H
#define ECMASCRIPT_COMPILER_BITSET_H

#include <cstddef>
#include <cstdint>

namespace panda::ecmascript::kungfu {
class BitSet {
public:
    static constexpr uint32_t BYTE_PER_WORD = sizeof(uint64_t);
    static constexpr uint32_t BYTE_PER_WORD_LOG2 = 3;
    static constexpr uint32_t BIT_PER_BYTE = 8;
    static constexpr uint32_t BIT_PER_WORD = BYTE_PER_WORD * BIT_PER_BYTE;
    static constexpr uint32_t BIT_PER_WORD_LOG2 = 6;
    static constexpr uint32_t BIT_PER_WORD_MASK = BIT_PER_WORD - 1;

    explicit BitSet(size_t bitSize)
    {
        wordCount_ = SizeOf(bitSize);
        if (UseWords()) {
            data_.words_ = new uint64_t[wordCount_];
        }
    }

    ~BitSet()
    {
        if (UseWords()) {
            delete data_.words_;
            data_.words_ = nullptr;
        }
    }

    bool TestBit(size_t offset) const
    {
        if (!UseWords()) {
            return data_.inlineWord_ & Mask(offset);
        }  else {
            return data_.words_[Index(offset)] & Mask(IndexInWord(offset));
        }
    }

    void SetBit(size_t offset)
    {
        if (!UseWords()) {
            data_.inlineWord_ |= Mask(offset);
        } else {
            data_.words_[Index(offset)] |= Mask(IndexInWord(offset));
        }
    }

    void ClearBit(size_t offset)
    {
        if (!UseWords()) {
            data_.inlineWord_ &= ~Mask(offset);
        } else {
            data_.words_[Index(offset)] &= ~Mask(IndexInWord(offset));
        }
    }

    void Union(const BitSet &bitset)
    {
        if (!UseWords()) {
            data_.inlineWord_ |= bitset.data_.inlineWord_;
        } else {
            for (size_t i = 0; i < wordCount_; i++) {
                data_.words_[i] |= bitset.data_.words_[i];
            }
        }
    }

    bool UnionWithChanged(const BitSet &bitset)
    {
        if (!UseWords()) {
            auto oldValue = data_.inlineWord_;
            data_.inlineWord_ |= bitset.data_.inlineWord_;
            return data_.inlineWord_ != oldValue;
        } else {
            bool changed = false;
            for (size_t i = 0; i < wordCount_; i++) {
                auto oldValue = data_.words_[i];
                data_.words_[i] |= bitset.data_.words_[i];
                if (!changed && data_.words_[i] != oldValue) {
                    changed = true;
                }
            }
            return changed;
        }
    }

    void CopyFrom(const BitSet &other)
    {
        wordCount_ = other.wordCount_;
        data_.inlineWord_ = other.data_.inlineWord_;
        if (!UseWords()) {
            return;
        }
        for (size_t i = 0; i < wordCount_; i++) {
            data_.words_[i] = other.data_.words_[i];
        }
    }
private:
    union Data {
        uint64_t inlineWord_;
        uint64_t *words_ {nullptr};
    };
    static size_t SizeOf(size_t bitSize)
    {
        // +1: for word 1
        return ((bitSize - 1) >> BIT_PER_WORD_LOG2) + 1;
    }

    uint64_t Mask(size_t index) const
    {
        return 1 << index;
    }

    size_t IndexInWord(size_t offset) const
    {
        return offset & BIT_PER_WORD_MASK;
    }

    size_t Index(size_t offset) const
    {
        return offset >> BIT_PER_WORD_LOG2;
    }

    size_t WordCount(size_t size) const
    {
        return size >> BYTE_PER_WORD_LOG2;
    }

    bool UseWords() const {
        return wordCount_ > 1;
    }

    uint32_t wordCount_ {0};
    Data data_;
};
}  // panda::ecmascript::kungfu
#endif  // ECMASCRIPT_COMPILER_BITSET_H