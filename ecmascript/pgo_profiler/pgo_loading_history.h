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

#ifndef ECMASCRIPT_PGO_LOADING_HISTORY_H
#define ECMASCRIPT_PGO_LOADING_HISTORY_H

#include "ecmascript/pgo_profiler/ap_file/pgo_file_info.h"
#include <chrono>
#include <fstream>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace panda::ecmascript::pgo {
class PGOLoadingHistory {
public:
    using Timestamp = std::chrono::system_clock::time_point;
    using HistoryMap = std::unordered_map<std::string, std::vector<Timestamp>>;

    static constexpr const char* TAG = "[PGOLoadingHistory] ";

    void AddHistory(std::string& id, const Timestamp& newTimestamp)
    {
        history_[id].push_back(newTimestamp);
    }

    std::vector<Timestamp> GetHistories(std::string& id) const
    {
        auto it = history_.find(id);
        if (it != history_.end()) {
            return it->second;
        }
        return {};
    }

    int GetPid() const
    {
#if defined(_WIN32) || defined(_WIN64)
        return static_cast<int>(GetCurrentProcessId());
#else
        return static_cast<int>(getpid());
#endif
    }

    std::string GetId(std::string& bundleName) const
    {
        if (bundleName.empty()) {
            return std::to_string(GetPid());
        } else {
            return bundleName;
        }
    }

    Timestamp GetTimestamp()
    {
        return std::chrono::system_clock::now();
    }

    void ParseFromBinary(void* buffer, SectionInfo* const info);
    void ProcessToBinary(std::fstream& fileStream, SectionInfo* const info) const;
    void ProcessToText(std::ofstream& stream) const;
    void Merge(const PGOLoadingHistory& other);

    std::string TimestampsToString(const std::vector<Timestamp>& tss) const
    {
        std::ostringstream oss;
        for (const auto& ts: tss) {
            oss << ConvertTimestampToISO(ts) << ", ";
        }
        return oss.str().substr(0, oss.str().size() - 2);
    }

    void CopyHistory(HistoryMap& other)
    {
        for (const auto& pair: history_) {
            other[pair.first] = pair.second;
        }
    }

    void Clear()
    {
        history_.clear();
    }

    int GetIngestTimes(const std::string& bundleName)
    {
        std::string id = std::to_string(GetPid());
        if (!bundleName.empty()) {
            id = bundleName;
        }
        auto iter = history_.find(id);
        if (iter == history_.end()) {
            return 0;
        } else {
            return iter->second.size();
        }
    }

private:
    HistoryMap history_;

    std::string ConvertTimestampToISO(const Timestamp& ts) const
    {
        auto timeT = std::chrono::system_clock::to_time_t(ts);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&timeT), "%FT%T%z"); // ISO 8601 格式
        return ss.str();
    }
};
}
#endif