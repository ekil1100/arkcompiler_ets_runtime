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

#include "ecmascript/taskpool/runner.h"

#include "ecmascript/platform/mutex.h"
#include "libpandabase/os/thread.h"
#ifdef ENABLE_QOS
#include "qos.h"
#endif

namespace panda::ecmascript {
Runner::Runner(uint32_t threadNum, const std::function<void(os::thread::native_handle_type)> prologueHook,
    const std::function<void(os::thread::native_handle_type)> epilogueHook)
    : totalThreadNum_(threadNum),
    prologueHook_(prologueHook),
    epilogueHook_(epilogueHook)
{
    for (uint32_t i = 0; i < threadNum; i++) {
        // main thread is 0;
        std::unique_ptr<std::thread> thread = std::make_unique<std::thread>(&Runner::Run, this, i + 1);
        threadPool_.emplace_back(std::move(thread));
    }

    for (uint32_t i = 0; i < runningTask_.size(); i++) {
        runningTask_[i] = nullptr;
    }
}

void Runner::TerminateTask(int32_t id, TaskType type)
{
    taskQueue_.TerminateTask(id, type);
    LockHolder holder(mtx_);
    for (uint32_t i = 0; i < runningTask_.size(); i++) {
        if (runningTask_[i] != nullptr) {
            if (id != ALL_TASK_ID && id != runningTask_[i]->GetId()) {
                continue;
            }
            if (type != TaskType::ALL && type != runningTask_[i]->GetTaskType()) {
                continue;
            }
            runningTask_[i]->Terminated();
        }
    }
}

void Runner::TerminateThread()
{
    TerminateTask(ALL_TASK_ID, TaskType::ALL);
    taskQueue_.Terminate();

    LockHolder holder(mtxPool_);
    uint32_t threadNum = threadPool_.size();
    for (uint32_t i = 0; i < threadNum; i++) {
        threadPool_.at(i)->join();
    }
    threadPool_.clear();
}

void Runner::ForEachTask(const std::function<void(Task*)> &f)
{
    taskQueue_.ForEachTask(f);
    LockHolder holder(mtx_);
    for (uint32_t i = 0; i < runningTask_.size(); i++) {
        if (runningTask_[i] != nullptr) {
            f(runningTask_[i]);
        }
    }
}

void Runner::SetQosPriority([[maybe_unused]] bool isForeground)
{
#ifdef ENABLE_QOS
    if (isForeground) {
        for (uint32_t threadId : gcThreadId_) {
            OHOS::QOS::SetQosForOtherThread(OHOS::QOS::QosLevel::QOS_USER_INITIATED, threadId);
        }
    } else {
        for (uint32_t threadId : gcThreadId_) {
            OHOS::QOS::ResetQosForOtherThread(threadId);
        }
    }
#endif
}

void Runner::RecordThreadId()
{
    LockHolder holder(mtx_);
    gcThreadId_.emplace_back(os::thread::GetCurrentThreadId());
}

void Runner::SetRunTask(uint32_t threadId, Task *task)
{
    LockHolder holder(mtx_);
    runningTask_[threadId] = task;
}

bool Runner::PreRun(std::unique_ptr<Task>& task)
{
    if (task->GetTaskType() == TaskType::PGO_PROFILER_TASK) {
        if (pgoProfilerTaskCount_ < GetPGOProfilerTaskMaxCount()) {
            pgoProfilerTaskCount_++;
            return true;
        } else {
            return false;
        }
    }
    return true;
}

void Runner::PostRun(std::unique_ptr<Task>& task)
{
    if (task->GetTaskType() == TaskType::PGO_PROFILER_TASK) {
        pgoProfilerTaskCount_--;
    }
}

void Runner::Run(uint32_t threadId)
{
    os::thread::native_handle_type thread = os::thread::GetNativeHandle();
    os::thread::SetThreadName(thread, "OS_GC_Thread");
    PrologueHook(thread);
    RecordThreadId();
    while (std::unique_ptr<Task> task = taskQueue_.PopTask()) {
        if (!PreRun(task)) {
            continue;
        }
        SetRunTask(threadId, task.get());
        task->Run(threadId);
        PostRun(task);
        SetRunTask(threadId, nullptr);
    }
    EpilogueHook(thread);
}
}  // namespace panda::ecmascript
