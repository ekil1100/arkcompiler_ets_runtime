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
        size_t idSize = base::ReadBuffer<size_t>(&ptr, sizeof(size_t));
        std::unique_ptr<char[]> idBuffer(new char[idSize + 1]);
        memcpy(idBuffer.get(), ptr, idSize);
        idBuffer[idSize] = '\0';
        std::string id(idBuffer.get());
        ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + idSize);

        size_t size = base::ReadBuffer<size_t>(&ptr, sizeof(size_t));
        std::vector<Timestamp> timestamps;
        for (size_t j = 0; j < size; j++) {
            Timestamp ts = base::ReadBuffer<Timestamp>(&ptr, sizeof(Timestamp));
            timestamps.push_back(ts);
        }

        LOG_ECMA(DEBUG) << TAG << id << "[" << timestamps.size() << "]"
                        << ": " << TimestampsToString(timestamps);
        history_[id] = timestamps;
    }
}

void PGOLoadingHistory::ProcessToBinary(std::fstream& fileStream, SectionInfo* const info) const
{
    fileStream.seekp(info->offset_, std::ios::cur);
    info->offset_ = static_cast<uint32_t>(fileStream.tellp());

    LOG_ECMA(DEBUG) << TAG << "process to binary";
    for (const auto& pair: history_) {
        std::string id = pair.first;
        size_t idSize = id.size();
        fileStream.write(reinterpret_cast<const char*>(&idSize), sizeof(idSize));
        fileStream.write(id.data(), idSize);

        size_t size = pair.second.size();
        fileStream.write(reinterpret_cast<const char*>(&size), sizeof(size));
        for (const auto& ts: pair.second) {
            fileStream.write(reinterpret_cast<const char*>(&ts), sizeof(ts));
        }

        LOG_ECMA(DEBUG) << TAG << id << "[" << pair.second.size() << "]"
                        << ": " << TimestampsToString(pair.second);
    }
    info->size_ = static_cast<uint32_t>(fileStream.tellp()) - info->offset_;
    info->number_ = history_.size();
}

void PGOLoadingHistory::Merge(const PGOLoadingHistory& other)
{
    LOG_ECMA(DEBUG) << TAG << "merging history";
    for (const auto& pair: other.history_) {
        std::string id = pair.first;
        LOG_ECMA(DEBUG) << TAG << id << "[" << pair.second.size() << "]"
                        << ": " << TimestampsToString(pair.second);
        history_[id].insert(history_[id].end(), pair.second.begin(), pair.second.end());
    }
}

void PGOLoadingHistory::ProcessToText(std::ofstream& stream) const
{
    stream << DumpUtils::NEW_LINE << "PGOLoadingHistory:" << DumpUtils::NEW_LINE;
    for (const auto& pair: history_) {
        std::string id = pair.first;
        stream << id << "[" << pair.second.size() << "]"
               << ": " << TimestampsToString(pair.second) << DumpUtils::NEW_LINE;
    }
}
}