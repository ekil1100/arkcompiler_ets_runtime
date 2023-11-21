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

#include "pressure.h"
#if TARGAARCH64
#include "aarch64_schedule.h"
#elif TARGRISCV64
#include "riscv64_schedule.h"
#endif
#include "deps.h"

namespace maplebe {
/* ------- RegPressure function -------- */
int32 RegPressure::maxRegClassNum = 0;

/* print regpressure information */
void RegPressure::DumpRegPressure() const
{
    PRINT_STR_VAL("Priority: ", priority);
    PRINT_STR_VAL("maxDepth: ", maxDepth);
    PRINT_STR_VAL("near: ", near);
    PRINT_STR_VAL("callNum: ", callNum);

    LogInfo::MapleLogger() << "\n";
}
} /* namespace maplebe */
