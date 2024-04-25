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

#include "ecmascript/intl/locale_helper.h"
#include "ecmascript/global_env.h"
#include "ecmascript/js_date.h"
#include "ecmascript/js_date_time_format.h"
#include "ecmascript/js_locale.h"
#include "ecmascript/object_factory-inl.h"
#include "ecmascript/tests/test_helper.h"

using namespace panda;
using namespace panda::ecmascript;
using namespace panda::ecmascript::base;
using LocaleHelper = panda::ecmascript::intl::LocaleHelper;

namespace panda::test {
class JSDateTimeFormatTest : public BaseTestWithScope<true> {
};

JSHandle<JSDateTimeFormat> CreateDateTimeFormatTest(JSThread *thread, icu::Locale icuLocale, JSHandle<JSObject> options)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();

    JSHandle<JSTaggedValue> localeCtor = env->GetLocaleFunction();
    JSHandle<JSTaggedValue> dtfCtor = env->GetDateTimeFormatFunction();
    JSHandle<JSLocale> locales =
        JSHandle<JSLocale>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(localeCtor), localeCtor));
    JSHandle<JSDateTimeFormat> dtf =
        JSHandle<JSDateTimeFormat>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(dtfCtor), dtfCtor));

    JSHandle<JSTaggedValue> optionsVal = JSHandle<JSTaggedValue>::Cast(options);
    factory->NewJSIntlIcuData(locales, icuLocale, JSLocale::FreeIcuLocale);
    dtf = JSDateTimeFormat::InitializeDateTimeFormat(thread, dtf, JSHandle<JSTaggedValue>::Cast(locales), optionsVal);
    return dtf;
}

void SetDateOptionsTest(JSThread *thread, JSHandle<JSObject> &optionsObj,
    std::map<std::string, std::string> &dateOptions)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto globalConst = thread->GlobalConstants();
    // Date options keys.
    JSHandle<JSTaggedValue> weekdayKey = globalConst->GetHandledWeekdayString();
    JSHandle<JSTaggedValue> yearKey = globalConst->GetHandledYearString();
    JSHandle<JSTaggedValue> monthKey = globalConst->GetHandledMonthString();
    JSHandle<JSTaggedValue> dayKey = globalConst->GetHandledDayString();
    // Date options values.
    JSHandle<JSTaggedValue> weekdayValue(factory->NewFromASCII(dateOptions["weekday"].c_str()));
    JSHandle<JSTaggedValue> yearValue(factory->NewFromASCII(dateOptions["year"].c_str()));
    JSHandle<JSTaggedValue> monthValue(factory->NewFromASCII(dateOptions["month"].c_str()));
    JSHandle<JSTaggedValue> dayValue(factory->NewFromASCII(dateOptions["day"].c_str()));
    // Set date options.
    JSObject::SetProperty(thread, optionsObj, weekdayKey, weekdayValue);
    JSObject::SetProperty(thread, optionsObj, yearKey, yearValue);
    JSObject::SetProperty(thread, optionsObj, monthKey, monthValue);
    JSObject::SetProperty(thread, optionsObj, dayKey, dayValue);
}

void SetTimeOptionsTest(JSThread *thread, JSHandle<JSObject> &optionsObj,
    std::map<std::string, std::string> &timeOptionsMap)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto globalConst = thread->GlobalConstants();
    // Time options keys.
    JSHandle<JSTaggedValue> dayPeriodKey = globalConst->GetHandledDayPeriodString();
    JSHandle<JSTaggedValue> hourKey = globalConst->GetHandledHourString();
    JSHandle<JSTaggedValue> minuteKey = globalConst->GetHandledMinuteString();
    JSHandle<JSTaggedValue> secondKey = globalConst->GetHandledSecondString();
    JSHandle<JSTaggedValue> fractionalSecondDigitsKey = globalConst->GetHandledFractionalSecondDigitsString();
    // Time options values.
    JSHandle<JSTaggedValue> dayPeriodValue(factory->NewFromASCII(timeOptionsMap["dayPeriod"].c_str()));
    JSHandle<JSTaggedValue> hourValue(factory->NewFromASCII(timeOptionsMap["hour"].c_str()));
    JSHandle<JSTaggedValue> minuteValue(factory->NewFromASCII(timeOptionsMap["minute"].c_str()));
    JSHandle<JSTaggedValue> secondValue(factory->NewFromASCII(timeOptionsMap["second"].c_str()));
    JSHandle<JSTaggedValue> fractionalSecondDigitsValue(
        factory->NewFromASCII(timeOptionsMap["fractionalSecond"].c_str()));
    // Set time options.
    JSObject::SetProperty(thread, optionsObj, dayPeriodKey, dayPeriodValue);
    JSObject::SetProperty(thread, optionsObj, hourKey, hourValue);
    JSObject::SetProperty(thread, optionsObj, minuteKey, minuteValue);
    JSObject::SetProperty(thread, optionsObj, secondKey, secondValue);
    JSObject::SetProperty(thread, optionsObj, fractionalSecondDigitsKey, fractionalSecondDigitsValue);
}

HWTEST_F_L0(JSDateTimeFormatTest, FormatDateTime_002)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    auto globalConst = thread->GlobalConstants();

    icu::Locale icuLocale("zh", "Hans", "Cn");
    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    JSHandle<JSTaggedValue> hourCycleKey = globalConst->GetHandledHourCycleString();
    JSHandle<JSTaggedValue> hourCycleValue(factory->NewFromASCII("h12"));
    JSHandle<JSTaggedValue> timeZoneKey = globalConst->GetHandledTimeZoneString();
    JSHandle<JSTaggedValue> timeZoneValue(factory->NewFromASCII("ETC/GMT-8"));
    JSObject::SetProperty(thread, options, timeZoneKey, timeZoneValue);
    JSObject::SetProperty(thread, options, hourCycleKey, hourCycleValue);
    options = JSDateTimeFormat::ToDateTimeOptions(
        thread, JSHandle<JSTaggedValue>::Cast(options), RequiredOption::ANY, DefaultsOption::ALL);
    JSHandle<JSDateTimeFormat> dtf = CreateDateTimeFormatTest(thread, icuLocale, options);

    double timeStamp1 = 1653448174000; // test "2022-05-25 11:09:34.000"
    double timeStamp2 = 1653921012999; // test "2022-05-30 22:30:12.999"

    // Format to include all options by "DefaultsOption::ALL".
    JSHandle<EcmaString> dateTimeEcamStr1 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp1);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr1).c_str(), "2022/5/25 上午11:09:34");
    JSHandle<EcmaString> dateTimeEcamStr2 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp2);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr2).c_str(), "2022/5/30 下午10:30:12");
}

HWTEST_F_L0(JSDateTimeFormatTest, FormatDateTime_003)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    auto globalConst = thread->GlobalConstants();

    icu::Locale icuLocale("zh", "Hans", "Cn");
    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    JSHandle<JSTaggedValue> hourCycleKey = globalConst->GetHandledHourCycleString();
    JSHandle<JSTaggedValue> hourCycleValue(factory->NewFromASCII("h12"));
    JSHandle<JSTaggedValue> timeZoneKey = globalConst->GetHandledTimeZoneString();
    JSHandle<JSTaggedValue> timeZoneValue(factory->NewFromASCII("ETC/GMT-8"));
    JSObject::SetProperty(thread, options, timeZoneKey, timeZoneValue);
    JSObject::SetProperty(thread, options, hourCycleKey, hourCycleValue);

    // Set custom date time format.
    std::map<std::string, std::string> dateOptionsMap {
        { "weekday", "long" },
        { "year", "2-digit" },
        { "month", "2-digit" },
        { "day", "2-digit" }
    };
    std::map<std::string, std::string> timeOptionsMap {
        { "dayPeriod", "long" },
        { "hour", "2-digit" },
        { "minute", "2-digit" },
        { "second", "2-digit" },
        { "fractionalSecond", "3" }
    };
    SetDateOptionsTest(thread, options, dateOptionsMap);
    SetTimeOptionsTest(thread, options, timeOptionsMap);
    options = JSDateTimeFormat::ToDateTimeOptions(
        thread, JSHandle<JSTaggedValue>::Cast(options), RequiredOption::ANY, DefaultsOption::ALL);
    JSHandle<JSDateTimeFormat> dtf = CreateDateTimeFormatTest(thread, icuLocale, options);

    double timeStamp1 = 1653448174000; // test "2022-05-25 11:09:34.000"
    double timeStamp2 = 1653921012999; // test "2022-05-30 22:30:12.999"

    JSHandle<EcmaString> dateTimeEcamStr1 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp1);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr1).c_str(), "22年05月25日星期三 上午11:09:34.000");
    JSHandle<EcmaString> dateTimeEcamStr2 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp2);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr2).c_str(), "22年05月30日星期一 晚上10:30:12.999");
}

HWTEST_F_L0(JSDateTimeFormatTest, FormatDateTime_004)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    auto globalConst = thread->GlobalConstants();

    icu::Locale icuLocale("en", "Latn", "US");
    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    JSHandle<JSTaggedValue> hourCycleKey = globalConst->GetHandledHourCycleString();
    JSHandle<JSTaggedValue> hourCycleValue(factory->NewFromASCII("h12"));
    JSHandle<JSTaggedValue> timeZoneKey = globalConst->GetHandledTimeZoneString();
    JSHandle<JSTaggedValue> timeZoneValue(factory->NewFromASCII("ETC/GMT-8"));
    JSObject::SetProperty(thread, options, timeZoneKey, timeZoneValue);
    JSObject::SetProperty(thread, options, hourCycleKey, hourCycleValue);

    // Set custom date time format.
    std::map<std::string, std::string> dateOptionsMap {
        { "weekday", "long" },
        { "year", "2-digit" },
        { "month", "2-digit" },
        { "day", "2-digit" }
    };
    std::map<std::string, std::string> timeOptionsMap {
        { "dayPeriod", "long" },
        { "hour", "2-digit" },
        { "minute", "2-digit" },
        { "second", "2-digit" },
        { "fractionalSecond", "3" }
    };
    SetDateOptionsTest(thread, options, dateOptionsMap);
    SetTimeOptionsTest(thread, options, timeOptionsMap);
    options = JSDateTimeFormat::ToDateTimeOptions(
        thread, JSHandle<JSTaggedValue>::Cast(options), RequiredOption::ANY, DefaultsOption::ALL);
    JSHandle<JSDateTimeFormat> dtf = CreateDateTimeFormatTest(thread, icuLocale, options);

    double timeStamp1 = 1653448174000; // test "2022-05-25 11:09:34.000"
    double timeStamp2 = 1653921012999; // test "2022-05-30 22:30:12.999"

    JSHandle<EcmaString> dateTimeEcamStr1 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp1);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr1).c_str(),
        "Wednesday, 05/25/22, 11:09:34.000 in the morning");
    JSHandle<EcmaString> dateTimeEcamStr2 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp2);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr2).c_str(),
        "Monday, 05/30/22, 10:30:12.999 at night");
}

std::string GetDateTimePartStringTest(JSThread *thread, JSHandle<JSTaggedValue> key, JSHandle<JSTaggedValue> part)
{
    JSHandle<JSObject> partObj = JSHandle<JSObject>::Cast(part);
    JSHandle<JSTaggedValue> partValue = JSObject::GetProperty(thread, partObj, key).GetValue();
    JSHandle<EcmaString> partEcmaStr = JSHandle<EcmaString>::Cast(partValue);
    std::string partStr = LocaleHelper::ConvertToStdString(partEcmaStr);
    return partStr;
}

/**
 * @tc.name: FormatDateTimeToParts
 * @tc.desc: Convert floating-point timestamp to fixed format time date through time date format.
 *           The "FormatDateTimeToParts" method converts the output result into an array containing various time and
 *           date attributes.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F_L0(JSDateTimeFormatTest, FormatDateTimeToParts_001)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    auto globalConst = thread->GlobalConstants();

    JSHandle<JSTaggedValue> typeKey = globalConst->GetHandledTypeString();
    JSHandle<JSTaggedValue> valueKey = globalConst->GetHandledValueString();

    icu::Locale icuLocale("zh", "Hans", "Cn");
    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    JSHandle<JSTaggedValue> hourCycleKey = globalConst->GetHandledHourCycleString();
    JSHandle<JSTaggedValue> hourCycleValue(factory->NewFromASCII("h12"));
    JSHandle<JSTaggedValue> timeZoneKey = globalConst->GetHandledTimeZoneString();
    JSHandle<JSTaggedValue> timeZoneValue(factory->NewFromASCII("ETC/GMT-8"));
    JSObject::SetProperty(thread, options, timeZoneKey, timeZoneValue);
    JSObject::SetProperty(thread, options, hourCycleKey, hourCycleValue);
    JSHandle<JSDateTimeFormat> dtf = CreateDateTimeFormatTest(thread, icuLocale, options);

    double timeStamp = 1653448174123; // test "2022-05-25 11:09:34.123"
    // Use default date time format and format date and time to parts.
    JSHandle<EcmaString> dateTimeEcamStr1 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr1).c_str(), "2022/5/25");
    JSHandle<JSArray> dateTimeArray1 = JSDateTimeFormat::FormatDateTimeToParts(thread, dtf, timeStamp);
    auto year = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray1), 0).GetValue();
    auto literal1 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray1), 1).GetValue();
    auto month = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray1), 2).GetValue();
    auto literal2 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray1), 3).GetValue();
    auto day = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray1), 4).GetValue();
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, year).c_str(), "year");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, year).c_str(), "2022");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal1).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal1).c_str(), "/");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, month).c_str(), "month");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, month).c_str(), "5");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal2).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal2).c_str(), "/");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, day).c_str(), "day");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, day).c_str(), "25");
}

HWTEST_F_L0(JSDateTimeFormatTest, FormatDateTimeToParts_002)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    auto globalConst = thread->GlobalConstants();

    JSHandle<JSTaggedValue> typeKey = globalConst->GetHandledTypeString();
    JSHandle<JSTaggedValue> valueKey = globalConst->GetHandledValueString();
    icu::Locale icuLocale("zh", "Hans", "Cn");
    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    JSHandle<JSTaggedValue> hourCycleKey = globalConst->GetHandledHourCycleString();
    JSHandle<JSTaggedValue> hourCycleValue(factory->NewFromASCII("h12"));
    JSHandle<JSTaggedValue> timeZoneKey = globalConst->GetHandledTimeZoneString();
    JSHandle<JSTaggedValue> timeZoneValue(factory->NewFromASCII("ETC/GMT-8"));
    JSObject::SetProperty(thread, options, timeZoneKey, timeZoneValue);
    JSObject::SetProperty(thread, options, hourCycleKey, hourCycleValue);

    double timeStamp = 1653448174123; // test "2022-05-25 11:09:34.123"
    // Set custom date time format and format date and time to parts.
    std::map<std::string, std::string> dateOptionsMap {
        { "weekday", "long" },
        { "year", "2-digit" },
        { "month", "2-digit" },
        { "day", "2-digit" }
    };
    std::map<std::string, std::string> timeOptionsMap {
        { "dayPeriod", "long" },
        { "hour", "2-digit" },
        { "minute", "2-digit" },
        { "second", "2-digit" },
        { "fractionalSecond", "3" }
    };
    SetDateOptionsTest(thread, options, dateOptionsMap);
    SetTimeOptionsTest(thread, options, timeOptionsMap);
    options = JSDateTimeFormat::ToDateTimeOptions(
        thread, JSHandle<JSTaggedValue>::Cast(options), RequiredOption::ANY, DefaultsOption::ALL);
    JSHandle<JSDateTimeFormat> dtf = CreateDateTimeFormatTest(thread, icuLocale, options);
    JSHandle<EcmaString> dateTimeEcamStr = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr).c_str(), "22年05月25日星期三 上午11:09:34.123");

    JSHandle<JSArray> dateTimeArray = JSDateTimeFormat::FormatDateTimeToParts(thread, dtf, timeStamp);
    auto year = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 0).GetValue();
    auto literal1 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 1).GetValue();
    auto month = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 2).GetValue();
    auto literal2 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 3).GetValue();
    auto day = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 4).GetValue();
    auto literal3 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 5).GetValue();
    auto weekday = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 6).GetValue();
    auto literal4 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 7).GetValue();
    auto dayPeriod = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 8).GetValue();
    auto hour = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 9).GetValue();
    auto literal5 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 10).GetValue();
    auto minute = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 11).GetValue();
    auto literal6 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 12).GetValue();
    auto second = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 13).GetValue();
    auto literal7 = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 14).GetValue();
    auto fracSec = JSTaggedValue::GetProperty(thread, JSHandle<JSTaggedValue>::Cast(dateTimeArray), 15).GetValue();
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, year).c_str(), "year");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, year).c_str(), "22");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal1).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal1).c_str(), "年");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, month).c_str(), "month");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, month).c_str(), "05");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal2).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal2).c_str(), "月");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, day).c_str(), "day");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, day).c_str(), "25");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal3).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal3).c_str(), "日");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, weekday).c_str(), "weekday");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, weekday).c_str(), "星期三");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal4).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal4).c_str(), " ");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, dayPeriod).c_str(), "dayPeriod");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, dayPeriod).c_str(), "上午");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, hour).c_str(), "hour");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, hour).c_str(), "11");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal5).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal5).c_str(), ":");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, minute).c_str(), "minute");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, minute).c_str(), "09");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal6).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal6).c_str(), ":");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, second).c_str(), "second");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, second).c_str(), "34");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, literal7).c_str(), "literal");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, literal7).c_str(), ".");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, typeKey, fracSec).c_str(), "fractionalSecond");
    EXPECT_STREQ(GetDateTimePartStringTest(thread, valueKey, fracSec).c_str(), "123");
}
/**
 * @tc.name: GainAvailableLocales
 * @tc.desc: Get the available localized label array. If the global time date localized label is not set, return an
 *           array containing all available labels. Otherwise, return an array containing self-defined labels.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F_L0(JSDateTimeFormatTest, GainAvailableLocales)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();

    // The date and time format locales is not initialized,
    // then get all available locales and save them in a 'TaggedArray'.
    JSHandle<JSTaggedValue> dateTimeFormatLocales = env->GetDateTimeFormatLocales();
    EXPECT_EQ(dateTimeFormatLocales.GetTaggedValue(), JSTaggedValue::Undefined());

    const char *key = "calendar";
    const char *path = nullptr;
    std::vector<std::string> availableStringLocales = intl::LocaleHelper::GetAvailableLocales(thread, key, path);
    JSHandle<TaggedArray> availableLocales = JSLocale::ConstructLocaleList(thread, availableStringLocales);
    env->SetDateTimeFormatLocales(thread, availableLocales);
    JSHandle<TaggedArray> gainLocales1 = JSDateTimeFormat::GainAvailableLocales(thread);
    EXPECT_EQ(JSHandle<JSTaggedValue>::Cast(gainLocales1).GetTaggedValue().GetRawData(),
        JSHandle<JSTaggedValue>::Cast(availableLocales).GetTaggedValue().GetRawData());

    // The date and time format locales has already been initialized,
    // then get custom locale and save it in a 'TaggedArray'.
    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSTaggedValue> localeCtor = env->GetLocaleFunction();
    JSHandle<JSTaggedValue> dtfCtor = env->GetDateTimeFormatFunction();

    JSHandle<JSLocale> locales =
        JSHandle<JSLocale>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(localeCtor), localeCtor));
    icu::Locale icuLocale("zh", "Hans", "Cn", "calendar=chinese");
    factory->NewJSIntlIcuData(locales, icuLocale, JSLocale::FreeIcuLocale);
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    options = JSDateTimeFormat::ToDateTimeOptions(
        thread, JSHandle<JSTaggedValue>::Cast(options), RequiredOption::ANY, DefaultsOption::ALL);
    JSHandle<JSDateTimeFormat> dtf =
        JSHandle<JSDateTimeFormat>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(dtfCtor), dtfCtor));
    dtf = JSDateTimeFormat::InitializeDateTimeFormat(
        thread, dtf, JSHandle<JSTaggedValue>::Cast(locales), JSHandle<JSTaggedValue>::Cast(options));

    JSHandle<JSTaggedValue> localeTagVal(thread, dtf->GetLocale());
    JSHandle<TaggedArray> localesTagArr = factory->NewTaggedArray(1);
    localesTagArr->Set(thread, 0, localeTagVal);
    env->SetDateTimeFormatLocales(thread, localesTagArr);
    JSHandle<TaggedArray> gainLocales2 = JSDateTimeFormat::GainAvailableLocales(thread);
    EXPECT_EQ(gainLocales2->GetLength(), 1U);
    EXPECT_STREQ(EcmaStringAccessor(gainLocales2->Get(0)).ToCString().c_str(),
        "zh-Hans-CN-u-ca-chinese");
}
} // namespace panda::test