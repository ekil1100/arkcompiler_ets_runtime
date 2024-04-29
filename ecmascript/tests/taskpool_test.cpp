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

#include "ecmascript/tests/test_helper.h"

using namespace panda;
using namespace panda::ecmascript;

namespace panda::test {
class TaskpoolTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        GTEST_LOG_(INFO) << "SetUpTestCase";
    }

    static void TearDownTestCase()
    {
        GTEST_LOG_(INFO) << "TearDownCase";
    }

    void SetUp() override
    {
        Taskpool::GetCurrentTaskpool()->Initialize();
    }

    void TearDown() override
    {
        Taskpool::GetCurrentTaskpool()->Destroy(ALL_TASK_ID);
    }

    EcmaVM* instance {nullptr};
    EcmaHandleScope* scope {nullptr};
    JSThread* thread {nullptr};
};

static std::atomic<uint32_t> TASK_COUNT = 0;

class MonkTask : public Task {
public:
    explicit MonkTask(int32_t id, int time): Task(id), time_(time) {};
    virtual ~MonkTask() override = default;

    bool Run([[maybe_unused]] uint32_t threadIndex) override
    {
        std::cout << "task " << GetId() << " start" << std::endl;
        TASK_COUNT.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(time_));
        TASK_COUNT.fetch_sub(1);
        return true;
    }

    TaskType GetTaskType() const override
    {
        return TaskType::PGO_PROFILER_TASK;
    }

    NO_COPY_SEMANTIC(MonkTask);
    NO_MOVE_SEMANTIC(MonkTask);

private:
    int time_ {0};
};

HWTEST_F_L0(TaskpoolTest, PGOProfilerTaskCount)
{
    Taskpool::GetCurrentTaskpool()->PostTask(std::make_unique<MonkTask>(0, 0));
    Taskpool::GetCurrentTaskpool()->PostTask(std::make_unique<MonkTask>(1, 200));
    Taskpool::GetCurrentTaskpool()->PostTask(std::make_unique<MonkTask>(2, 200));
    Taskpool::GetCurrentTaskpool()->PostTask(std::make_unique<MonkTask>(3, 200));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_LE(TASK_COUNT, MAX_PGO_PROFILER_TASK_COUNT);
}
} // namespace panda::test
