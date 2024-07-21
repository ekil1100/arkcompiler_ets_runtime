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
std::shared_ptr<FunctionCallTimer> FunctionCallTimer::timer_ = nullptr;

std::shared_ptr<FunctionCallTimer> FunctionCallTimer::Create(const std::string& bundleName)
{
    auto timer = std::make_shared<FunctionCallTimer>(bundleName);
    RegisterHandler(timer);
    return timer;
}

FunctionCallTimer::FunctionCallTimer([[maybe_unused]] const std::string& bundleName)
{
#ifdef AOT_ESCAPE_ENABLE
    if (ListHelper::GetInstance()->IsEnableAot(bundleName) || ListHelper::GetInstance()->IsEnableJit(bundleName)) {
        enable_ = true;
    }
#else
    enable_ = true;
#endif
}

void FunctionCallTimer::StartCount(Method* method, bool isAot, std::string tag)
{
    if (!IsEnable()) {
        return;
    }
    if (method == nullptr) {
        return;
    }
    size_t id = method->GetMethodId().GetOffset();
    CString name = GetFullName(method);
    FunctionCallStat* stat = nullptr;
    if (isAot) {
        stat = TryGetAotStat(name, id, isAot, tag);
    } else {
        stat = TryGetIntStat(name, id, isAot, tag);
    }
    statStack_.push(stat);
    PandaRuntimeTimer* caller = nullptr;
    if (!timerStack_.empty()) {
        caller = &timerStack_.top();
    }
    PandaRuntimeTimer callee;
    callee.SetParent(caller);
    callee.Start(stat, caller);
    timerStack_.push(callee);
    PrintMethodInfo(method, isAot, "start", tag);
}

void FunctionCallTimer::StopCount(Method* method, bool isAot, std::string tag)
{
    if (!IsEnable()) {
        return;
    }
    if (method == nullptr) {
        return;
    }
    size_t id = method->GetMethodId().GetOffset();
    auto name = GetFullName(method);
    if (timerStack_.empty() || statStack_.empty()) {
        return;
    }
    PandaRuntimeTimer* callee = &timerStack_.top();
    FunctionCallStat* currentStat = statStack_.top();
    if (currentStat->GetId() != id) {
        // PrintStatStack();
        LOG_TRACE(ERROR) << FUNCTIMER << "method not match, end"
                         << " at " << tag << ", method: " << name << ":" << id << ", is aot: " << isAot
                         << ", last start"
                         << " at " << currentStat->GetTag() << ", method: " << currentStat->Name() << ":"
                         << currentStat->GetId() << ", is aot: " << currentStat->IsAot();
        if (count_[id] > 0) {
            while (currentStat->GetId() != id) {
                callee->Stop();
                statStack_.pop();
                timerStack_.pop();
                if (statStack_.empty() || timerStack_.empty()) {
                    return;
                }
                callee = &timerStack_.top();
                currentStat = statStack_.top();
            }
        } else {
            return;
        }
    }
    callee->Stop();
    statStack_.pop();
    timerStack_.pop();
    PrintMethodInfo(method, isAot, "end", tag);
}

void FunctionCallTimer::FinishFunctionTimer()
{
    finished_ = true;
    while (!timerStack_.empty()) {
        PandaRuntimeTimer* callee = &timerStack_.top();
        callee->Stop();
        statStack_.pop();
        timerStack_.pop();
    }
}

std::string FunctionCallTimer::StatToString(FunctionCallStat* stat)
{
    if (stat == nullptr) {
        return "[stat] nullptr";
    }
    std::ostringstream oss;
    oss << "[stat] start at " << stat->GetTag() << ", name: " << stat->Name() << ", id: " << stat->GetId()
        << ", is aot: " << stat->IsAot();
    return oss.str();
}

void FunctionCallTimer::PrintStatStack()
{
    std::ostringstream oss;
    oss << FUNCTIMER << "[stat stack] size: " << statStack_.size() << "\n";
    while (statStack_.size() > 0) {
        oss << FUNCTIMER << StatToString(statStack_.top()) << "\n";
        statStack_.pop();
    }
    LOG_TRACE(DEBUG) << oss.str();
}

CString FunctionCallTimer::GetFullName(Method* method)
{
    if (method == nullptr) {
        return CString("unknown");
    }
    CString funcName(method->GetMethodName());
    CString recordName = method->GetRecordNameStr();
    CString fullName = recordName + "." + funcName;
    return fullName;
}

void FunctionCallTimer::PrintAllStats()
{
    if (!IsEnable()) {
        return;
    }
    static constexpr int nameRightAdjustment = 45;
    static constexpr int numberRightAdjustment = 15;
    static constexpr int length = nameRightAdjustment + numberRightAdjustment * 6;
    std::string separator(length, '=');

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

    std::ostringstream oss;
    oss << FUNCTIMER << "function call stat, total count: " << callStatVec.size() << "\n";

    oss << FUNCTIMER << separator << "\n";
    oss << FUNCTIMER << std::left << std::setw(nameRightAdjustment) << "FunctionName" << std::right
        << std::setw(numberRightAdjustment) << "ID" << std::setw(numberRightAdjustment) << "Type"
        << std::setw(numberRightAdjustment) << "Time(ns)" << std::setw(numberRightAdjustment) << "Count"
        << std::setw(numberRightAdjustment) << "MaxTime(ns)" << std::setw(numberRightAdjustment) << "AvgTime(ns)"
        << "\n";

    for (auto& stat: callStatVec) {
        if (stat.TotalCount() != 0) {
            CString type = stat.IsAot() ? "Aot" : "Interpreter";
            oss << FUNCTIMER << std::left << std::setw(nameRightAdjustment) << stat.Name() << std::right
                << std::setw(numberRightAdjustment) << stat.GetId() << std::setw(numberRightAdjustment) << type
                << std::setw(numberRightAdjustment) << stat.TotalTime() << std::setw(numberRightAdjustment)
                << stat.TotalCount() << std::setw(numberRightAdjustment) << stat.MaxTime()
                << std::setw(numberRightAdjustment) << stat.TotalTime() / stat.TotalCount() << "\n";
        }
    }
    oss << FUNCTIMER << separator << "\n";
    LOG_TRACE(INFO) << oss.str();
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
}

void FunctionCallTimer::PrintMethodInfo(Method* method, bool isAot, std::string state, std::string tag)
{
    if (method == nullptr) {
        return;
    }
    auto name = GetFullName(method);
    auto id = method->GetMethodId().GetOffset();
    if (state == "start") {
        count_[id]++;
    }
    auto count = std::to_string(count_[id]) + ":" + std::to_string(timerStack_.size());
    LOG_TRACE(DEBUG) << FUNCTIMER << std::left << std::setw(5) << count << " " << state << " at " << tag
                     << ", method: " << name << ", id: " << id << ", is aot: " << isAot;
    if (state == "end") {
        count_[id]--;
    }
    if (count_[id] < 0) {
        LOG_TRACE(ERROR) << FUNCTIMER << "count_[id] < 0";
    }
}

void FunctionCallTimer::RegisterHandler(std::shared_ptr<FunctionCallTimer> timer)
{
    if (timer_ == nullptr) {
        timer_ = timer;
    }
    signal(SIGNO, RegisteFunctionTimerSignal);
}

void FunctionCallTimer::RegisteFunctionTimerSignal(int signo)
{
    if (timer_) {
        timer_->FunctionTimerSignalHandler(signo);
    }
}

void FunctionCallTimer::FunctionTimerSignalHandler(int signo)
{
    if (signo == SIGNO) {
        PrintAllStats();
        ResetStat();
    }
}

FunctionCallStat* FunctionCallTimer::TryGetAotStat(CString name, size_t id, bool isAot, std::string tag)
{
    auto iter = aotCallStat_.find(id);
    if (iter == aotCallStat_.end()) {
        FunctionCallStat stat(name, id, isAot, tag);
        aotCallStat_[id] = stat;
    } else {
        iter->second.SetTag(tag);
    }
    return &aotCallStat_[id];
}

FunctionCallStat* FunctionCallTimer::TryGetIntStat(CString name, size_t id, bool isAot, std::string tag)
{
    auto iter = intCallStat_.find(id);
    if (iter == intCallStat_.end()) {
        FunctionCallStat stat(name, id, isAot, tag);
        intCallStat_[id] = stat;
    } else {
        iter->second.SetTag(tag);
    }
    return &intCallStat_[id];
}
} // namespace panda::ecmascript