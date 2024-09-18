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

#include <cstdio>

#include "ecmascript/dfx/cpu_profiler/cpu_profiler.h"
#include "ecmascript/tests/test_helper.h"

using namespace panda::ecmascript;

namespace panda::ecmascript {
class CpuProfilerFriendTest {
public:
    explicit CpuProfilerFriendTest(const EcmaVM *vm, const int interval) : cpu_profiler(vm, interval) {}

    void SetIsProfiling(bool isProfiling)
    {
        cpu_profiler.isProfiling_ = isProfiling;
    }

    void SetOutToFile(bool outToFile)
    {
        cpu_profiler.outToFile_ = outToFile;
    }

    bool StartCpuProfilerForInfoTest()
    {
        return cpu_profiler.StartCpuProfilerForInfo();
    }

    bool StopCpuProfilerForInfoTest(std::unique_ptr<struct ProfileInfo> &profileInfo)
    {
        return cpu_profiler.StopCpuProfilerForInfo(profileInfo);
    }

    bool StartCpuProfilerForFileTest(const std::string &fileName)
    {
        return cpu_profiler.StartCpuProfilerForFile(fileName);
    }

    bool StopCpuProfilerForFileTest()
    {
        return cpu_profiler.StopCpuProfilerForFile();
    }

    bool GetStackBeforeCallNapiTest(JSThread *thread)
    {
        return cpu_profiler.GetStackBeforeCallNapi(thread);
    }

private:
    CpuProfiler cpu_profiler;
};
}

namespace panda::test {
class CpuprofilerTest : public testing::Test {
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
        TestHelper::CreateEcmaVMWithScope(instance, thread, scope);
        instance->SetEnableForceGC(false);
    }

    void TearDown() override
    {
        TestHelper::DestroyEcmaVMWithScope(instance, scope);
    }

    EcmaVM *instance {nullptr};
    EcmaHandleScope *scope {nullptr};
    JSThread *thread {nullptr};
};

HWTEST_F_L0(CpuprofilerTest, TestStopCpuProfilerForFile)
{
    int interval = 1;
    bool ret;
    CpuProfilerFriendTest CpuProfilerFriend(instance, interval);
    ret = CpuProfilerFriend.StopCpuProfilerForFileTest();
    EXPECT_TRUE(ret);

    CpuProfilerFriend.SetIsProfiling(true);
    CpuProfilerFriend.StartCpuProfilerForInfoTest();
    ret = CpuProfilerFriend.StopCpuProfilerForFileTest();
    EXPECT_FALSE(ret);
}

HWTEST_F_L0(CpuprofilerTest, TestStartCpuProfilerForFile)
{
    int interval = 1;
    bool ret;
    CpuProfilerFriendTest CpuProfilerFriend(instance, interval);
    std::string filename1("__CpuprofilerTest.abc");
    std::string filename2("");
    std::string filename3(PATH_MAX + 1, ' ');

    ret = CpuProfilerFriend.StartCpuProfilerForFileTest(filename1);
    EXPECT_FALSE(ret);
    ret = CpuProfilerFriend.StartCpuProfilerForFileTest(filename2);
    EXPECT_FALSE(ret);
    ret = CpuProfilerFriend.StartCpuProfilerForFileTest(filename3);
    EXPECT_FALSE(ret);

    CpuProfilerFriend.SetIsProfiling(true);
    ret = CpuProfilerFriend.StartCpuProfilerForFileTest(filename1);
    EXPECT_FALSE(ret);
}

HWTEST_F_L0(CpuprofilerTest, TestStopCpuProfilerForInfo)
{
    int interval = 1;
    bool ret;
    CpuProfilerFriendTest CpuProfilerFriend(instance, interval);
    std::unique_ptr<ProfileInfo> profileInfo;

    ret = CpuProfilerFriend.StopCpuProfilerForInfoTest(profileInfo);
    EXPECT_TRUE(ret);

    CpuProfilerFriend.StartCpuProfilerForInfoTest();
    ret = CpuProfilerFriend.StopCpuProfilerForInfoTest(profileInfo);
    EXPECT_TRUE(ret);
    
    CpuProfilerFriend.SetIsProfiling(true);
    CpuProfilerFriend.SetOutToFile(true);
    ret = CpuProfilerFriend.StopCpuProfilerForInfoTest(profileInfo);
    EXPECT_FALSE(ret);
}

HWTEST_F_L0(CpuprofilerTest, TestGetStackBeforeCallNapi)
{
    int interval = 100;
    CpuProfilerFriendTest CpuProfilerFriend(instance, interval);

    bool flag = CpuProfilerFriend.GetStackBeforeCallNapiTest(thread);
    EXPECT_TRUE(flag);

    flag = CpuProfilerFriend.GetStackBeforeCallNapiTest(thread);
    EXPECT_FALSE(flag);
}
}  // namespace panda::test