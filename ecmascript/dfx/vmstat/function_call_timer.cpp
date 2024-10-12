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

#include "ecmascript/dfx/vmstat/function_call_timer.h"

#include <csignal>
#include <iomanip>

#include "ecmascript/jspandafile/js_pandafile_manager.h"
#include "ecmascript/ohos/enable_aot_list_helper.h"

namespace panda::ecmascript {
using ListHelper = ohos::EnableAotJitListHelper;
std::weak_ptr<FunctionCallTimer> FunctionCallTimer::timer_;
std::atomic_uint32_t FunctionCallTimer::nativeCallId_ = 0;

uint32_t FunctionCallTimer::GetAndIncreaseNativeCallId()
{
    return nativeCallId_++;
}

std::shared_ptr<FunctionCallTimer> FunctionCallTimer::Create(const std::string& bundleName)
{
    auto timer = std::make_shared<FunctionCallTimer>(bundleName);
    RegisterHandler(timer);
    return timer;
}

FunctionCallTimer::FunctionCallTimer([[maybe_unused]] const std::string& bundleName)
{
#ifdef OHOS_FUNCTION_TIMER_ENABLE
    if (ListHelper::GetInstance()->IsEnableAot(bundleName) || ListHelper::GetInstance()->IsEnableJit(bundleName)) {
        LOG_TRACE(INFO) << FUNCTIMER << "function call timer enabled for bundle: " << bundleName;
        enable_ = true;
    }
#else
    LOG_TRACE(INFO) << FUNCTIMER << "function call timer enabled";
    enable_ = true;
#endif
}

void FunctionCallTimer::StartCount(Method* method, CallType type, uint32_t nativeCallId)
{
    if (!IsEnable() || method == nullptr) {
        return;
    }
    FunctionCallStat* stat = nullptr;
    switch (type) {
        case CALL_TYPE_AOT:
            stat = TryGetAotStat(method, type);
            break;
        case CALL_TYPE_INT:
            stat = TryGetIntStat(method, type);
            break;
        case CALL_TYPE_NATIVE:
            stat = TryGetNativeStat(nativeCallId);
            break;
        default:
            break;
    }
    auto callee = new PandaRuntimeTimer();
    callee->Start(stat, parent_);
    parent_ = callee;
    CountMethod(true, stat->GetUniqueId());
}

void FunctionCallTimer::StopCount(Method* method, CallType type, uint32_t nativeCallId)
{
    if (!IsEnable() || method == nullptr || !parent_) {
        return;
    }
    PandaRuntimeTimer* callee = parent_;
    FunctionCallStat* start = static_cast<FunctionCallStat*>(callee->GetStat());
    auto stopId = FunctionCallStat::GenUniqueId(type, method->GetMethodId().GetOffset(), nativeCallId);
    if (start->GetUniqueId() != stopId) {
        LOG_TRACE(ERROR) << FUNCTIMER << "method not match, end stat: [id: " << stopId << ", type: " << type
                         << "], start stat: " << StatToString(start);
        if (count_[stopId] > 0) {
            while (start->GetUniqueId() != stopId) {
                callee = callee->Stop();
                start = static_cast<FunctionCallStat*>(callee->GetStat());
                delete callee;
            }
        } else {
            return;
        }
    }
    parent_ = callee->Stop();
    delete callee;
    CountMethod(false, stopId);
}

std::string FunctionCallTimer::StatToString(FunctionCallStat* stat)
{
    if (stat == nullptr) {
        return "[null]";
    }
    std::ostringstream oss;
    oss << "[name: " << stat->Name() << ", id: " << stat->GetId() << ", type: " << stat->GetType() << "]";
    return oss.str();
}

void FunctionCallTimer::PrintAllStats()
{
    if (!IsEnable()) {
        return;
    }
    static constexpr int NAME_RIGHT_ADJUSTMENT = 45;
    static constexpr int NUMBER_RIGHT_ADJUSTMENT = 15;
    static constexpr int LENGTH = NAME_RIGHT_ADJUSTMENT + NUMBER_RIGHT_ADJUSTMENT * 6;
    static constexpr size_t MAX_STAT_SIZE = 50;

    std::string separator(LENGTH, '=');
    CVector<FunctionCallStat> callStatVec;
    for (auto& stat: aotCallStat_) {
        callStatVec.emplace_back(stat.second);
    }
    for (auto& stat: intCallStat_) {
        callStatVec.emplace_back(stat.second);
    }
    // Sort by TotalTime
    std::sort(
        callStatVec.begin(), callStatVec.end(), [](const FunctionCallStat& a, const FunctionCallStat& b) -> bool {
            return a.TotalTime() > b.TotalTime();
        });
    auto count = std::min(callStatVec.size(), MAX_STAT_SIZE);

    LOG_TRACE(INFO)
        << FUNCTIMER
        << "only print top 50 function calls, sort by total time, adjust the value by changing MAX_STAT_SIZE";
    LOG_TRACE(INFO) << FUNCTIMER << "function call total count: " << callStatVec.size();
    LOG_TRACE(INFO) << FUNCTIMER << separator;
    LOG_TRACE(INFO) << FUNCTIMER << std::left << std::setw(NAME_RIGHT_ADJUSTMENT) << "FunctionName" << std::right
                    << std::setw(NUMBER_RIGHT_ADJUSTMENT) << "ID" << std::setw(NUMBER_RIGHT_ADJUSTMENT) << "Type"
                    << std::setw(NUMBER_RIGHT_ADJUSTMENT) << "Time(ns)" << std::setw(NUMBER_RIGHT_ADJUSTMENT)
                    << "Count" << std::setw(NUMBER_RIGHT_ADJUSTMENT) << "MaxTime(ns)"
                    << std::setw(NUMBER_RIGHT_ADJUSTMENT) << "AvgTime(ns)";
    for (size_t i = 0; i < count; i++) {
        auto& stat = callStatVec[i];
        if (stat.TotalCount() != 0) {
            CString type = stat.GetType() == CALL_TYPE_AOT ? "AOT" : "INT";
            LOG_TRACE(INFO) << FUNCTIMER << std::left << std::setw(NAME_RIGHT_ADJUSTMENT) << stat.Name()
                            << std::right << std::setw(NUMBER_RIGHT_ADJUSTMENT) << stat.GetId()
                            << std::setw(NUMBER_RIGHT_ADJUSTMENT) << type << std::setw(NUMBER_RIGHT_ADJUSTMENT)
                            << stat.TotalTime() << std::setw(NUMBER_RIGHT_ADJUSTMENT) << stat.TotalCount()
                            << std::setw(NUMBER_RIGHT_ADJUSTMENT) << stat.MaxTime()
                            << std::setw(NUMBER_RIGHT_ADJUSTMENT) << stat.TotalTime() / stat.TotalCount();
        }
    }
    LOG_TRACE(INFO) << FUNCTIMER << separator;
}

void FunctionCallTimer::ResetStat()
{
    if (!IsEnable()) {
        return;
    }
    for (auto& stat: aotCallStat_) {
        stat.second.Reset();
    }
    for (auto& stat: intCallStat_) {
        stat.second.Reset();
    }
    for (auto& stat: nativeCallStat_) {
        stat.second.Reset();
    }
}

void FunctionCallTimer::CountMethod(bool isStart, uint32_t id)
{
    if (isStart) {
        count_[id]++;
    } else {
        count_[id]--;
    }
    if (count_[id] < 0) {
        LOG_TRACE(ERROR) << FUNCTIMER << "count_[strId] < 0";
    }
}

void FunctionCallTimer::RegisterHandler(std::shared_ptr<FunctionCallTimer> timer)
{
    if (!timer_.lock()) {
        timer_ = timer;
    }
    if (signal(SIGNO, RegisteFunctionTimerSignal) == SIG_ERR) {
        LOG_TRACE(ERROR) << FUNCTIMER << "register signal failed";
    }
}

void FunctionCallTimer::RegisteFunctionTimerSignal(int signo)
{
    if (timer_.lock()) {
        timer_.lock()->FunctionTimerSignalHandler(signo);
    }
}

void FunctionCallTimer::FunctionTimerSignalHandler(int signo)
{
    if (signo == SIGNO) {
        PrintAllStats();
        ResetStat();
    }
}

FunctionCallStat* FunctionCallTimer::TryGetAotStat(Method* method, CallType type)
{
    auto id = method->GetMethodId().GetOffset();
    const auto& result = aotCallStat_.try_emplace(id, method, id, type);
    return &result.first->second;
}

FunctionCallStat* FunctionCallTimer::TryGetIntStat(Method* method, CallType type)
{
    auto id = method->GetMethodId().GetOffset();
    const auto& result = intCallStat_.try_emplace(id, method, id, type);
    return &result.first->second;
}

FunctionCallStat* FunctionCallTimer::TryGetNativeStat(uint32_t id)
{
    const auto& result = nativeCallStat_.try_emplace(id, "native", id, CALL_TYPE_NATIVE);
    return &result.first->second;
}
} // namespace panda::ecmascript