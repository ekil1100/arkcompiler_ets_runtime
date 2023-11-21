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

#include "printing.h"
#include "mir_module.h"
#include "types_def.h"

namespace maple {
const std::string kBlankString = "                                                                                ";
constexpr int kIndentunit = 2;  // number of blank chars of each indentation

void PrintIndentation(int32 indent)
{
    int64 indentAmount = static_cast<int64>(indent) * kIndentunit;
    do {
        LogInfo::MapleLogger() << kBlankString.substr(0, indentAmount);
        indentAmount -= kBlankString.length();
    } while (indentAmount > 0);
}

void PrintString(const std::string &str)
{
    size_t i = 0;
    LogInfo::MapleLogger() << " \"";
    while (i < str.length()) {
        unsigned char c = str[i++];
        // differentiate printable and non-printable charactors
        if (c >= 0x20 && c <= 0x7e) {
            // escape "
            switch (c) {
                case '"':
                    LogInfo::MapleLogger() << "\\\"";
                    break;
                case '\\':
                    LogInfo::MapleLogger() << "\\\\";
                    break;
                default:
                    LogInfo::MapleLogger() << c;
                    break;
            }
        } else {
            constexpr int kFieldWidth = 2;
            LogInfo::MapleLogger() << "\\x" << std::hex << std::setfill('0') << std::setw(kFieldWidth)
                                   << static_cast<unsigned int>(c) << std::dec;
        }
    }
    LogInfo::MapleLogger() << "\"";
}
}  // namespace maple
