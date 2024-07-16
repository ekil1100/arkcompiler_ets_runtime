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
#include "ecmascript/jspandafile/js_pandafile_manager.h"

#include <iomanip>

namespace panda::ecmascript {
void FunctionCallTimer::StartCount(Method* method, bool isAot, std::string tag)
{
    size_t id = method->GetMethodId().GetOffset();
    CString name = GetFullName(method);
    FunctionCallStat* stat = nullptr;
    if (isAot) {
        stat = TryGetAotStat(name, id, isAot, tag);
    } else {
        stat = TryGetIntStat(name, id, isAot, tag);
    }
    statStack_.push(stat);
    PandaRuntimeTimer* caller = &timerStack_.top();
    PandaRuntimeTimer callee;
    callee.SetParent(caller);
    callee.Start(stat, caller);
    timerStack_.push(callee);
    PrintMethodInfo(method, isAot, "start", tag);
}

void FunctionCallTimer::StopCount(Method* method, bool isAot, std::string tag)
{
    size_t id = method->GetMethodId().GetOffset();
    auto name = GetFullName(method);
    PandaRuntimeTimer* callee = &timerStack_.top();
    FunctionCallStat* currentStat = statStack_.top();
    if (currentStat->GetId() != id) {
        PrintStatStack();
        LOG_TRACE(FATAL) << "[FunctionTimer] method not match, end"
                         << " at " << tag << ", method: " << name << ":" << id << ", is aot: " << isAot
                         << ", last start"
                         << " at " << currentStat->GetTag() << ", method: " << currentStat->Name() << ":"
                         << currentStat->GetId() << ", is aot: " << currentStat->IsAot();
        return;
    }
    callee->Stop();
    statStack_.pop();
    timerStack_.pop();
    PrintMethodInfo(method, isAot, "end", tag);
}

void FunctionCallTimer::PrintStat(FunctionCallStat* stat)
{
    LOG_TRACE(DEBUG) << "[stat:" << statStack_.size() << "] start at " << stat->GetTag()
                     << ", name: " << stat->Name() << ", id: " << stat->GetId() << ", is aot: " << stat->IsAot();
}

void FunctionCallTimer::PrintStatStack()
{
    LOG_TRACE(DEBUG) << "[stat stack] size: " << statStack_.size();
    while (statStack_.size() > 0) {
        PrintStat(statStack_.top());
        statStack_.pop();
    }
}

CString FunctionCallTimer::GetFullName(Method *method)
{
    CString funcName(method->GetMethodName());
    CString recordName = method->GetRecordNameStr();
    CString fullName = recordName + "." + funcName;
    return fullName;
}

void FunctionCallTimer::PrintAllStats()
{
    static constexpr int nameRightAdjustment = 45;
    static constexpr int numberRightAdjustment = 15;
    static constexpr int length = nameRightAdjustment + numberRightAdjustment * 6;
    std::string separator(length, '=');

    CVector<FunctionCallStat> callStatVec;
    for (auto &stat : aotCallStat_) {
        callStatVec.emplace_back(stat.second);
    }
    for (auto &stat : intCallStat_) {
        callStatVec.emplace_back(stat.second);
    }
    // Sort by TotalTime
    std::sort(callStatVec.begin(), callStatVec.end(),
        [](const FunctionCallStat &a, const FunctionCallStat &b) -> bool {
            return a.TotalTime() > b.TotalTime();
    });

    LOG_TRACE(INFO) << "function call stat, total count: " << callStatVec.size();

    LOG_TRACE(INFO) << separator;
    LOG_TRACE(INFO) << std::left << std::setw(nameRightAdjustment) << "FunctionName" << std::right
                    << std::setw(numberRightAdjustment) << "ID" << std::setw(numberRightAdjustment) << "Type"
                    << std::setw(numberRightAdjustment) << "Time(ns)" << std::setw(numberRightAdjustment)
                    << "Count" << std::setw(numberRightAdjustment) << "MaxTime(ns)"
                    << std::setw(numberRightAdjustment) << "AvgTime(ns)";

    for (auto &stat : callStatVec) {
        if (stat.TotalCount() != 0) {
            CString type = stat.IsAot() ? "Aot" : "Interpreter";
            LOG_TRACE(INFO) << std::left << std::setw(nameRightAdjustment) << stat.Name() << std::right
                            << std::setw(numberRightAdjustment) << stat.GetId()
                            << std::setw(numberRightAdjustment) << type << std::setw(numberRightAdjustment)
                            << stat.TotalTime() << std::setw(numberRightAdjustment) << stat.TotalCount()
                            << std::setw(numberRightAdjustment) << stat.MaxTime()
                            << std::setw(numberRightAdjustment) << stat.TotalTime() / stat.TotalCount();
        }
    }
    LOG_TRACE(INFO) << separator;
}

void FunctionCallTimer::ResetStat()
{
    for (auto &stat : aotCallStat_) {
        stat.second.Reset();
    }

    for (auto &stat : intCallStat_) {
        stat.second.Reset();
    }
}

void FunctionCallTimer::PrintMethodInfo(Method* method, bool isAot, std::string state, std::string tag)
{
    auto name = GetFullName(method);
    auto id = method->GetMethodId().GetOffset();
    if (state == "start") {
        count_[id]++;
    }
    auto count = std::to_string(count_[id]) + ":" + std::to_string(timerStack_.size());
    LOG_TRACE(DEBUG) << std::left << std::setw(5) << count << state << " at " << tag << ", method: " << name
                     << ", id: " << id << ", is aot: " << isAot;
    if (state == "end") {
        count_[id]--;
    }
    if (count_[id] < 0) {
        LOG_TRACE(FATAL) << "count_[id] < 0";
    }
}

void FunctionCallTimer::RegisteFunctionTimerSignal()
{
    signal(SIGNO, FunctionTimerSignalHandler);
}

void FunctionCallTimer::FunctionTimerSignalHandler(int signo)
{
    if (signo == SIGNO) {
        GetInstance().PrintAllStats();
        GetInstance().ResetStat();
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