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

#ifndef ECMASCRIPT_DFX_VMSTAT_FCUNTION_CALL_TIMER_H
#define ECMASCRIPT_DFX_VMSTAT_FCUNTION_CALL_TIMER_H

#include "ecmascript/dfx/vmstat/caller_stat.h"
#include "ecmascript/mem/c_containers.h"
#include "ecmascript/mem/c_string.h"
#include "ecmascript/method.h"

namespace panda::ecmascript {
class EcmaVM;
using CallType = uint32_t;
static constexpr CallType CALL_TYPE_AOT = 0;
static constexpr CallType CALL_TYPE_INT = 1;
static constexpr CallType CALL_TYPE_NATIVE = 2;

// Description:
// FunctionCallTimer is a tool used to count the number of calls, maximum time, total time, and average time of JS&TS
// functions in an application or use case.

// Use:
// If you want to use FunctionCallTimer, open the ECMASCRIPT_ENABLE_FUNCTION_CALL_TIMER macro.

// Implementation:
// In AOT, StartCallTimer and EndCallTimer are inserted before and after the function call and generator reentry into
// aot. In ASM interpreter, StartCallTimer is inserted into JSCallDispatch and Resumegenerator instruction. EndCallTimer
// is inserted into the Return, ReturnUndefined and suspend related instruction.
// It should be particularly pointed out that native functions are not counted separately at present considering
// the performance overhead during statistics.
// The specific calculation method is given in the following example.

// T1(StartCallTimer)
// foo() {
//     T2(StartCallTimer)
//     bar();
//     T3(EndCallTimer)
// }
// T4(EndCallTimer)

// bar's self time is (T3 - T2).
// foo's self time is (T2 - T1) + (T4 - T3).

class FunctionCallStat : public PandaRuntimeCallerStat {
public:
    FunctionCallStat(Method* method, uint32_t id, CallType type)
        : PandaRuntimeCallerStat(method->GetMethodName()), type_(type), id_(id), uniqueId_(GenUniqueId(type, id))
    {
    }
    FunctionCallStat(const char* name, uint32_t id, CallType type)
        : PandaRuntimeCallerStat(name), type_(type), id_(id), uniqueId_(GenUniqueId(type, id))
    {
    }
    FunctionCallStat() = default;
    ~FunctionCallStat() = default;

    size_t GetId() const
    {
        return id_;
    }

    CallType GetType() const
    {
        return type_;
    }

    uint32_t GetUniqueId() const
    {
        return uniqueId_;
    }

    uint32_t GenUniqueId(CallType type, uint32_t id)
    {
        if (type == CALL_TYPE_NATIVE) {
            return id | NATIVE_MASK;
        } else {
            return id;
        }
    }

    static uint32_t GenUniqueId(CallType type, uint32_t id, uint32_t nativeId)
    {
        if (type == CALL_TYPE_NATIVE) {
            return nativeId | NATIVE_MASK;
        } else {
            return id;
        }
    }

private:
    static constexpr const uint32_t NATIVE_MASK = 1U << 31;
    CallType type_ {CALL_TYPE_AOT};
    uint32_t id_ {0};
    uint32_t uniqueId_ {0};
};

class FunctionCallTimer {
public:
    static constexpr const int SIGNO = 39;
    static constexpr const char* FUNCTIMER = "[FunctionTimer] ";
    static std::shared_ptr<FunctionCallTimer> Create(const std::string& bundleName);
    PUBLIC_API static uint32_t GetAndIncreaseNativeCallId();
    FunctionCallTimer(const std::string& bundleName);
    ~FunctionCallTimer() = default;
    void StartCount(Method* method, CallType type = CALL_TYPE_AOT, uint32_t nativeCallId = 0);
    void StopCount(Method* method, CallType type = CALL_TYPE_AOT, uint32_t nativeCallId = 0);
    void PrintAllStats();
    void ResetStat();

private:
    static void RegisteFunctionTimerSignal(int signo);
    static void RegisterHandler(std::shared_ptr<FunctionCallTimer> timer);
    FunctionCallStat* TryGetAotStat(Method* method, CallType type = CALL_TYPE_AOT);
    FunctionCallStat* TryGetIntStat(Method* method, CallType type = CALL_TYPE_AOT);
    FunctionCallStat* TryGetNativeStat(uint32_t id);
    std::string StatToString(FunctionCallStat* stat);
    void FunctionTimerSignalHandler(int signo);
    void CountMethod(bool isStart, uint32_t id);
    inline bool IsEnable()
    {
        return enable_;
    }

    static std::weak_ptr<FunctionCallTimer> timer_;
    static std::atomic_uint32_t nativeCallId_;
    CUnorderedMap<uint32_t, FunctionCallStat> aotCallStat_ {};
    CUnorderedMap<uint32_t, FunctionCallStat> intCallStat_ {};
    CUnorderedMap<uint32_t, FunctionCallStat> nativeCallStat_ {};
    CUnorderedMap<uint32_t, int> count_ {};
    bool enable_ = false;
    PandaRuntimeTimer* parent_ {nullptr};
};
}
#endif // ECMASCRIPT_DFX_VMSTAT_FCUNTION_CALL_TIMER_H