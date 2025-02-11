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

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"

#include "ecmascript/napi/include/jsnapi.h"
#include "ecmascript/ohos/ohos_pkg_args.h"
#include "ecmascript/ohos/white_list_helper.h"
#include "ecmascript/platform/file.h"
#include "ecmascript/tests/test_helper.h"

using namespace panda;
using namespace panda::ecmascript;
using namespace panda::ecmascript::kungfu;

namespace panda::test {
class OhosTest : public testing::Test {
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
        runtimeOptions_.SetPGOProfilerPath("");
        runtimeOptions_.SetTargetCompilerMode("partial");
        runtimeOptions_.SetAOTOutputFile("/data/local/ark-cache/com.ohos.test/arm64/phone");
        runtimeOptions_.SetCompilerPkgJsonInfo(BuildOhosPkgJson("/data/local/ark-profile/100/com.ohos.test"));
        runtimeOptions_.SetCompilerEnableExternalPkg(true);
        runtimeOptions_.SetCompilerExternalPkgJsonInfo(BuildOhosExternalPkgJson());
        vm_ = JSNApi::CreateEcmaVM(runtimeOptions_);
        ASSERT(vm_ != nullptr);
    }

    void TearDown() override
    {
        WhiteListHelper::GetInstance()->Clear();
        JSNApi::DestroyJSVM(vm_);
        vm_ = nullptr;
    }

    static std::string BuildOhosExternalPkgJson()
    {
        return "[" + BuildOhosPkgJson("", TEST_BUNDLE_NAME, "entry1") + ", " +
               BuildOhosPkgJson("", TEST_BUNDLE_NAME, "entry2") + "]";
    }

    static std::string BuildOhosPkgJson(const std::string &pgoDir, const std::string &bundleName = TEST_BUNDLE_NAME,
                                        const std::string &moduleName = "entry")
    {
        std::string pkgJson;
        pkgJson += R"({"bundleName": ")" + bundleName + "\"";
        pkgJson += R"(,"moduleName": ")" + moduleName + "\"";
        pkgJson += R"(,"pkgPath": "/data/app/el1/bundle/public/com.ohos.test/entry.hap","abcName": "ets/modules.abc")";
        pkgJson += R"(,"abcOffset": "0x6bb3c","abcSize": "0x51adfc")";
        if (!pgoDir.empty()) {
            pkgJson += R"(,"pgoDir": ")" + pgoDir + "\"";
        }
        return pkgJson + "}";
    }

    static void AddWhiteList()
    {
        auto helper = WhiteListHelper::GetInstance();
        helper->AddWhiteListEntry(TEST_BUNDLE_NAME);
    }

protected:
    static constexpr const char *TEST_BUNDLE_NAME = "com.ohos.test";
    JSRuntimeOptions runtimeOptions_;
    EcmaVM *vm_ {nullptr};
};

HWTEST_F_L0(OhosTest, AotWhiteListTest)
{
    const char *whiteListTestDir = "ohos-whiteList/";
    const char *whiteListName = "ohos-whiteList/app_aot_white_list.conf";
    std::string bundleScope = "com.bundle.scope.test";
    std::string moduleScope = "com.module.scope.test";
    mkdir(whiteListTestDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    std::ofstream file(whiteListName);
    file << bundleScope << std::endl;
    file << moduleScope << ":entry" << std::endl;
    file << " # " <<moduleScope << ":entryComment" << std::endl;

    file.close();
    auto helper = std::make_unique<WhiteListHelper>(whiteListName);
    ASSERT_TRUE(helper->IsEnable(bundleScope));
    ASSERT_TRUE(helper->IsEnable(bundleScope, "entry"));
    ASSERT_TRUE(helper->IsEnable(moduleScope, "entry"));
    ASSERT_FALSE(helper->IsEnable(moduleScope, "entry1"));
    ASSERT_FALSE(helper->IsEnable(moduleScope, "entryComment"));
    unlink(whiteListName);
    rmdir(whiteListTestDir);
}

HWTEST_F_L0(OhosTest, AotWhiteListPassBy)
{
    const char *whiteListName = "ohos-AotWhiteListPassBy/app_aot_white_list.conf";
    std::string bundleScope = "com.bundle.scope.test";
    std::string moduleScope = "com.module.scope.test";

    auto helper = std::make_unique<WhiteListHelper>(whiteListName);
    ASSERT_TRUE(helper->IsEnable(bundleScope));
    ASSERT_TRUE(helper->IsEnable(bundleScope, "entry"));
    ASSERT_TRUE(helper->IsEnable(moduleScope, "entry"));
    ASSERT_TRUE(helper->IsEnable(moduleScope, "entry1"));
    ASSERT_TRUE(helper->IsEnable(moduleScope, "entryCommentNotExist"));
}

HWTEST_F_L0(OhosTest, OhosPkgArgsParse)
{
    const char *pgoDir = "ohos-OhosPkgArgsParse";
    std::string runtimeAp = std::string(pgoDir) + "/rt_entry.ap";
    mkdir(pgoDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    std::ofstream file(runtimeAp);
    file.close();
    AddWhiteList();

    arg_list_t pandaFileNames {};
    std::map<std::string, std::shared_ptr<OhosPkgArgs>> pkgArgsMap;
    PGOProfilerDecoder decoder;

    runtimeOptions_.SetPGOProfilerPath(runtimeAp);
    runtimeOptions_.SetCompilerPkgJsonInfo(BuildOhosPkgJson(""));

    CompilationOptions cOptions(vm_, runtimeOptions_);
    AotCompilerPreprocessor preProcessor(vm_, runtimeOptions_, pkgArgsMap, decoder, pandaFileNames);
    OhosPkgArgs::ParseArgs(preProcessor, cOptions);

    ASSERT_EQ(preProcessor.GetMainPkgArgs()->GetBundleName(), TEST_BUNDLE_NAME);
    ASSERT_EQ(preProcessor.GetMainPkgArgs()->GetModuleName(), "entry");
    ASSERT_EQ(preProcessor.GetMainPkgArgs()->GetPath(), "/data/app/el1/bundle/public/com.ohos.test/entry.hap");
    ASSERT_EQ(preProcessor.GetMainPkgArgs()->GetSize(), 0x51adfc);
    ASSERT_EQ(preProcessor.GetMainPkgArgs()->GetOffset(), 0x6bb3c);
    ASSERT_EQ(preProcessor.GetMainPkgArgs()->GetPgoDir(), pgoDir);

    ASSERT_EQ(preProcessor.GetPkgsArgs().size(), 3);

    unlink(runtimeAp.c_str());
    rmdir(pgoDir);
}

HWTEST_F_L0(OhosTest, OhosPkgArgsParsePgoDir)
{
    arg_list_t pandaFileNames {};
    std::map<std::string, std::shared_ptr<OhosPkgArgs>> pkgArgsMap;
    PGOProfilerDecoder decoder;
    CompilationOptions cOptions(vm_, runtimeOptions_);
    AotCompilerPreprocessor preProcessor(vm_, runtimeOptions_, pkgArgsMap, decoder, pandaFileNames);
    OhosPkgArgs::ParseArgs(preProcessor, cOptions);

    ASSERT_EQ(preProcessor.GetMainPkgArgs()->GetPgoDir(), "/data/local/ark-profile/100/com.ohos.test");
}

HWTEST_F_L0(OhosTest, UseBaselineApFromPgoDir)
{
    const char *pgoDir = "ohos-UseBaselineApFromPgoDir";
    std::string baselineAp = std::string(pgoDir) + "/entry.ap";
    std::string runtimeAp = std::string(pgoDir) + "/rt_entry.ap";
    mkdir(pgoDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    std::ofstream file(baselineAp);
    file.close();
    file.open(runtimeAp);
    file.close();
    // do not add to white list

    arg_list_t pandaFileNames {};
    std::map<std::string, std::shared_ptr<OhosPkgArgs>> pkgArgsMap;
    PGOProfilerDecoder decoder;
    CompilationOptions cOptions(vm_, runtimeOptions_);
    AotCompilerPreprocessor preProcessor(vm_, runtimeOptions_, pkgArgsMap, decoder, pandaFileNames);
    runtimeOptions_.SetCompilerPkgJsonInfo(BuildOhosPkgJson(pgoDir));
    // Input format is correct, but there is no available ap file for pgoDir, return success.
    ASSERT_TRUE(preProcessor.HandleTargetCompilerMode(cOptions));

    ASSERT_TRUE(cOptions.profilerIn_.empty());
    unlink(runtimeAp.c_str());
    unlink(baselineAp.c_str());
    rmdir(pgoDir);
}

HWTEST_F_L0(OhosTest, UseRuntimeApWhenOnlyRuntimeExits)
{
    const char *pgoDir = "ohos-UseRuntimeApWhenOnlyRuntimeExits";
    std::string baselineAp = std::string(pgoDir) + "/entry.ap";
    std::string runtimeAp = std::string(pgoDir) + "/rt_entry.ap";
    std::string mergedAp = std::string(pgoDir) + "/merged_entry.ap";
    mkdir(pgoDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    std::ofstream file(baselineAp);
    file.close();
    file.open(runtimeAp);
    file.close();

    AddWhiteList();
    arg_list_t pandaFileNames {};
    std::map<std::string, std::shared_ptr<OhosPkgArgs>> pkgArgsMap;
    PGOProfilerDecoder decoder;
    CompilationOptions cOptions(vm_, runtimeOptions_);
    AotCompilerPreprocessor preProcessor(vm_, runtimeOptions_, pkgArgsMap, decoder, pandaFileNames);
    runtimeOptions_.SetCompilerPkgJsonInfo(BuildOhosPkgJson(pgoDir));
    ASSERT_TRUE(FileExist(runtimeAp.c_str()));
    ASSERT_TRUE(preProcessor.HandleTargetCompilerMode(cOptions));

    ASSERT_EQ(cOptions.profilerIn_, mergedAp);
    ASSERT_TRUE(FileExist(mergedAp.c_str()));
    ASSERT_TRUE(FileExist(baselineAp.c_str()));
    ASSERT_FALSE(FileExist(runtimeAp.c_str()));
    unlink(baselineAp.c_str());
    unlink(mergedAp.c_str());
    rmdir(pgoDir);
}

HWTEST_F_L0(OhosTest, UseMergedApWhenOnlyMergedExist)
{
    const char *pgoDir = "ohos-UseMergedApWhenOnlyMergedExist";
    std::string baselineAp = std::string(pgoDir) + "/entry.ap";
    std::string mergedAp = std::string(pgoDir) + "/merged_entry.ap";
    mkdir(pgoDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    std::ofstream file(baselineAp);
    file.close();
    file.open(mergedAp);
    file.close();

    AddWhiteList();
    arg_list_t pandaFileNames {};
    std::map<std::string, std::shared_ptr<OhosPkgArgs>> pkgArgsMap;
    PGOProfilerDecoder decoder;
    CompilationOptions cOptions(vm_, runtimeOptions_);
    AotCompilerPreprocessor preProcessor(vm_, runtimeOptions_, pkgArgsMap, decoder, pandaFileNames);
    runtimeOptions_.SetCompilerPkgJsonInfo(BuildOhosPkgJson(pgoDir));
    ASSERT_TRUE(preProcessor.HandleTargetCompilerMode(cOptions));

    ASSERT_EQ(cOptions.profilerIn_, mergedAp);
    ASSERT_TRUE(FileExist(mergedAp.c_str()));
    ASSERT_TRUE(FileExist(baselineAp.c_str()));
    unlink(baselineAp.c_str());
    unlink(mergedAp.c_str());
    rmdir(pgoDir);
}

HWTEST_F_L0(OhosTest, UseMergedApWhenBothRuntimeAndMergedExist)
{
    const char *pgoDir = "ohos-UseMergedApWhenBothRuntimeAndMergedExist";
    std::string baselineAp = std::string(pgoDir) + "/entry.ap";
    std::string runtimeAp = std::string(pgoDir) + "/rt_entry.ap";
    std::string mergedAp = std::string(pgoDir) + "/merged_entry.ap";
    mkdir(pgoDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    std::ofstream file(baselineAp);
    file.close();
    file.open(mergedAp);
    file.close();
    file.open(runtimeAp);
    file.close();

    AddWhiteList();
    arg_list_t pandaFileNames {};
    std::map<std::string, std::shared_ptr<OhosPkgArgs>> pkgArgsMap;
    PGOProfilerDecoder decoder;
    CompilationOptions cOptions(vm_, runtimeOptions_);
    AotCompilerPreprocessor preProcessor(vm_, runtimeOptions_, pkgArgsMap, decoder, pandaFileNames);
    runtimeOptions_.SetCompilerPkgJsonInfo(BuildOhosPkgJson(pgoDir));
    ASSERT_TRUE(OhosPkgArgs::ParseArgs(preProcessor, cOptions));

    ASSERT_EQ(cOptions.profilerIn_, mergedAp + ":" + runtimeAp);
    ASSERT_TRUE(FileExist(mergedAp.c_str()));
    ASSERT_TRUE(FileExist(baselineAp.c_str()));
    unlink(baselineAp.c_str());
    unlink(runtimeAp.c_str());
    unlink(mergedAp.c_str());
    rmdir(pgoDir);
}
}  // namespace panda::test