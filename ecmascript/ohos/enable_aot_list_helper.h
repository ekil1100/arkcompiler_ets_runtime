/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef ECMASCRIPT_OHOS_ENABLE_AOT_LIST_HELPER_H
#define ECMASCRIPT_OHOS_ENABLE_AOT_LIST_HELPER_H

#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ohos_constants.h"
#include "ecmascript/base/string_helper.h"
#include "ecmascript/log_wrapper.h"
#include "ecmascript/platform/file.h"
#include "ecmascript/ohos/aot_runtime_info.h"
#include "macros.h"

#ifdef AOT_ESCAPE_ENABLE
#include "parameters.h"
#endif
namespace panda::ecmascript::ohos {
class EnableAotJitListHelper {
constexpr static const char *const AOT_BUILD_COUNT_DISABLE = "ark.aot.build.count.disable";
public:
    static std::shared_ptr<EnableAotJitListHelper> GetInstance()
    {
        static auto helper = std::make_shared<EnableAotJitListHelper>(ENABLE_LIST_NAME);
        return helper;
    }

    explicit EnableAotJitListHelper(const std::string &enableListName)
    {
        ReadEnableList(enableListName);
    }

    EnableAotJitListHelper() = default;
    ~EnableAotJitListHelper() = default;

    bool IsEnableAot(const std::string &candidate)
    {
        return (enableList_.find(candidate) != enableList_.end()) ||
               (enableList_.find(candidate + ":aot") != enableList_.end());
    }

    bool IsEnableJit(const std::string &candidate)
    {
        return (enableList_.find(candidate) != enableList_.end()) ||
               (enableList_.find(candidate + ":jit") != enableList_.end());
    }

    void AddEnableListEntry(const std::string &entry)
    {
        enableList_.insert(entry);
    }

    void Clear()
    {
        enableList_.clear();
    }

    static bool GetAotBuildCountDisable()
    {
#ifdef AOT_ESCAPE_ENABLE
        return OHOS::system::GetBoolParameter(AOT_BUILD_COUNT_DISABLE, false);
#endif
        return false;
    }

    void AddEnableListCount(const std::string &pgoPath = "") const
    {
        ohos::AotRuntimeInfo aotRuntimeInfo;
        aotRuntimeInfo.BuildCompileRuntimeInfo(
            ohos::AotRuntimeInfo::GetRuntimeInfoTypeStr(ohos::RuntimeInfoType::AOT_BUILD), pgoPath);
    }

    bool IsAotCompileSuccessOnce() const
    {
        if (GetAotBuildCountDisable()) {
            return false;
        }
        ohos::AotRuntimeInfo aotRuntimeInfo;
        int count = aotRuntimeInfo.GetCompileCountByType(ohos::RuntimeInfoType::AOT_BUILD);
        if (count > 0) {
            return true;
        }
        return false;
    }

private:
    NO_COPY_SEMANTIC(EnableAotJitListHelper);
    NO_MOVE_SEMANTIC(EnableAotJitListHelper);

    static void Trim(std::string &data)
    {
        if (data.empty()) {
            return;
        }
        data.erase(0, data.find_first_not_of(' '));
        data.erase(data.find_last_not_of(' ') + 1);
    }

    void ReadEnableList(const std::string &aotJitListName)
    {
        if (!panda::ecmascript::FileExist(aotJitListName.c_str())) {
            LOG_ECMA(DEBUG) << "bundle enable list not exist and will pass by all. file: " << aotJitListName;
            return;
        }

        std::ifstream inputFile(aotJitListName);

        if (!inputFile.is_open()) {
            LOG_ECMA(ERROR) << "bundle enable list open failed! file: " << aotJitListName << ", errno: " << errno;
            return;
        }

        std::string line;
        while (getline(inputFile, line)) {
            auto appName = line;
            Trim(appName);
            // skip empty line
            if (appName.empty()) {
                continue;
            }
            // skip comment line
            if (appName.at(0) == '#') {
                continue;
            }
            AddEnableListEntry(appName);
        }
    }
    std::set<std::string> enableList_ {};
    PUBLIC_API static const std::string ENABLE_LIST_NAME;
};
}  // namespace panda::ecmascript::ohos
#endif  // ECMASCRIPT_OHOS_ENABLE_AOT_LIST_HELPER_H
