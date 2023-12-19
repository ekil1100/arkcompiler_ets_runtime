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

#include "ecmascript/pgo_profiler/pgo_loading_history.h"
#include <cstdint>

namespace panda::ecmascript::pgo {
void PGOLoadingHistory::ParseFromBinary(void* buffer, SectionInfo* const info)
{
    void* ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(buffer) + info->offset_);
    LOG_ECMA(DEBUG) << TAG << "parse from binary";
    for (uint32_t i = 0; i < info->number_; i++) {
        int pid = base::ReadBuffer<int>(&ptr, sizeof(int));
        size_t size = base::ReadBuffer<size_t>(&ptr, sizeof(size_t));
        std::vector<Timestamp> timestamps;
        for (size_t j = 0; j < size; j++) {
            Timestamp ts = base::ReadBuffer<Timestamp>(&ptr, sizeof(Timestamp));
            timestamps.push_back(ts);
        }
        LOG_ECMA(DEBUG) << TAG << pid << ": " << TimestampsToString(timestamps);
        history_[pid] = timestamps;
    }
}

void PGOLoadingHistory::ProcessToBinary(std::fstream& fileStream, SectionInfo* const info) const
{
    fileStream.seekp(info->offset_, std::ios::cur);
    info->offset_ = static_cast<uint32_t>(fileStream.tellp());
    LOG_ECMA(DEBUG) << TAG << "process to binary";
    for (const auto& pair: history_) {
        int pid = pair.first;
        fileStream.write(reinterpret_cast<const char*>(&pid), sizeof(pid));
        size_t size = pair.second.size();
        fileStream.write(reinterpret_cast<const char*>(&size), sizeof(size));
        for (const auto& ts: pair.second) {
            fileStream.write(reinterpret_cast<const char*>(&ts), sizeof(ts));
        }
        LOG_ECMA(DEBUG) << TAG << pid << ": " << TimestampsToString(pair.second);
    }
    info->size_ = static_cast<uint32_t>(fileStream.tellp()) - info->offset_;
    info->number_ = history_.size();
}

void PGOLoadingHistory::Merge(const PGOLoadingHistory& other)
{
    LOG_ECMA(DEBUG) << TAG << "merging history";
    for (const auto& item: other.history_) {
        auto pid = item.first;
        LOG_ECMA(DEBUG) << TAG << pid << ": " << TimestampsToString(item.second);
        history_[pid].insert(history_[pid].end(), item.second.begin(), item.second.end());
    }
}

void PGOLoadingHistory::ProcessToText(std::ofstream& stream) const
{
    stream << DumpUtils::NEW_LINE << "Loading History:" << DumpUtils::NEW_LINE;
    for (const auto& pair: history_) {
        int pid = pair.first;
        stream << "PID " << std::dec << pid << ": ";
        stream << TimestampsToString(pair.second);
        stream << DumpUtils::NEW_LINE;
    }
}
}