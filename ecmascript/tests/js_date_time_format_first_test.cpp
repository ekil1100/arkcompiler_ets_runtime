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

/**
 * @tc.name: GetIcuLocale & SetIcuLocale
 * @tc.desc: Set and obtain localization labels compatible with ICU Libraries.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F_L0(JSDateTimeFormatTest, Set_Get_IcuLocale)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    JSHandle<JSTaggedValue> ctor = env->GetDateTimeFormatFunction();
    JSHandle<JSDateTimeFormat> dtf =
        JSHandle<JSDateTimeFormat>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(ctor), ctor));

    icu::Locale locale1("ko", "Kore", "KR");
    JSDateTimeFormat::SetIcuLocale(thread, dtf, locale1, JSDateTimeFormat::FreeIcuLocale);
    icu::Locale *resLocale1 = dtf->GetIcuLocale();
    EXPECT_STREQ(resLocale1->getBaseName(), "ko_Kore_KR");

    icu::Locale locale2("zh", "Hans", "Cn");
    JSDateTimeFormat::SetIcuLocale(thread, dtf, locale2, JSDateTimeFormat::FreeIcuLocale);
    icu::Locale *resLocale2 = dtf->GetIcuLocale();
    EXPECT_STREQ(resLocale2->getBaseName(), "zh_Hans_CN");
}

/**
 * @tc.name: SetIcuSimpleDateFormat & GetIcuSimpleDateFormat
 * @tc.desc: Set and obtain a simple time and date format compatible with ICU Libraries.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F_L0(JSDateTimeFormatTest, Set_Get_IcuSimpleDateFormat)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    const icu::UnicodeString timeZoneId("Asia/Shanghai");
    icu::TimeZone *tz = icu::TimeZone::createTimeZone(timeZoneId);
    icu::TimeZone::adoptDefault(tz);
    JSHandle<JSTaggedValue> ctor = env->GetDateTimeFormatFunction();
    JSHandle<JSDateTimeFormat> dtf =
        JSHandle<JSDateTimeFormat>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(ctor), ctor));
    UErrorCode status = UErrorCode::U_ZERO_ERROR;
    icu::UnicodeString dateTime1("2022.05.25 11:09:34");
    icu::UnicodeString dateTime2("2022.May.25 11:09:34");
    icu::UnicodeString dateTime3("2022.May.25 AD 11:09:34 AM");

    icu::UnicodeString pattern("yyyy.MM.dd HH:mm:ss");
    icu::SimpleDateFormat sdf(pattern, status);
    JSDateTimeFormat::SetIcuSimpleDateFormat(thread, dtf, sdf, JSDateTimeFormat::FreeSimpleDateFormat);
    icu::SimpleDateFormat *resSdf = dtf->GetIcuSimpleDateFormat();
    UDate timeStamp = resSdf->parse(dateTime1, status);
    EXPECT_EQ(timeStamp, 1653448174000);
    status = UErrorCode::U_ZERO_ERROR;
    timeStamp = resSdf->parse(dateTime2, status);
    EXPECT_EQ(timeStamp, 1653448174000);
    status = UErrorCode::U_ZERO_ERROR;
    timeStamp = resSdf->parse(dateTime3, status);
    EXPECT_EQ(timeStamp, 0);

    status = UErrorCode::U_ZERO_ERROR;
    icu::UnicodeString pattern2("yyyyy.MMMMM.dd GGG hh:mm::ss aaa");
    icu::SimpleDateFormat sdf2(pattern2, status);
    JSDateTimeFormat::SetIcuSimpleDateFormat(thread, dtf, sdf2, JSDateTimeFormat::FreeSimpleDateFormat);
    icu::SimpleDateFormat *resSdf2 = dtf->GetIcuSimpleDateFormat();
    timeStamp = resSdf2->parse(dateTime1, status);
    EXPECT_EQ(timeStamp, 0);
    status = UErrorCode::U_ZERO_ERROR;
    timeStamp = resSdf2->parse(dateTime2, status);
    EXPECT_EQ(timeStamp, 0);
    status = UErrorCode::U_ZERO_ERROR;
    timeStamp = resSdf2->parse(dateTime3, status);
    EXPECT_EQ(timeStamp, 1653448174000);
}

/**
 * @tc.name: InitializeDateTimeFormat
 * @tc.desc: Initialize the time and date format through localization label locales and options.
 *           Options can include year, month, day, hour, minute, second, time zone and weekday, etc.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F_L0(JSDateTimeFormatTest, InitializeDateTimeFormat)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    // Create locales.
    JSHandle<JSTaggedValue> localeCtor = env->GetLocaleFunction();
    JSHandle<JSLocale> locales =
        JSHandle<JSLocale>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(localeCtor), localeCtor));
    icu::Locale icuLocale("zh", "Hans", "Cn", "calendar=chinese");
    factory->NewJSIntlIcuData(locales, icuLocale, JSLocale::FreeIcuLocale);
    // Create options.
    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    options = JSDateTimeFormat::ToDateTimeOptions(
        thread, JSHandle<JSTaggedValue>::Cast(options), RequiredOption::ANY, DefaultsOption::ALL);
    // Initialize DateTimeFormat.
    JSHandle<JSTaggedValue> dtfCtor = env->GetDateTimeFormatFunction();
    JSHandle<JSDateTimeFormat> dtf =
        JSHandle<JSDateTimeFormat>::Cast(factory->NewJSObjectByConstructor(JSHandle<JSFunction>(dtfCtor), dtfCtor));
    dtf = JSDateTimeFormat::InitializeDateTimeFormat(
        thread, dtf, JSHandle<JSTaggedValue>::Cast(locales), JSHandle<JSTaggedValue>::Cast(options));

    JSHandle<JSTaggedValue> localeTagVal(thread, dtf->GetLocale());
    JSHandle<EcmaString> localeEcmaStr = JSHandle<EcmaString>::Cast(localeTagVal);
    std::string localeStr = LocaleHelper::ConvertToStdString(localeEcmaStr);
    EXPECT_STREQ(localeStr.c_str(), "zh-Hans-CN-u-ca-chinese");
}

/**
 * @tc.name: ToDateTimeOptions
 * @tc.desc: Empty or incomplete option objects are supplemented according to the required option and default option.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F_L0(JSDateTimeFormatTest, ToDateTimeOptions_001)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    auto globalConst = thread->GlobalConstants();

    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSTaggedValue> yearKey = globalConst->GetHandledYearString();
    JSHandle<JSTaggedValue> monthKey = globalConst->GetHandledMonthString();
    JSHandle<JSTaggedValue> dayKey = globalConst->GetHandledDayString();
    JSHandle<JSTaggedValue> hourKey = globalConst->GetHandledHourString();
    JSHandle<JSTaggedValue> minuteKey = globalConst->GetHandledMinuteString();
    JSHandle<JSTaggedValue> secondKey = globalConst->GetHandledSecondString();

    // When the option value is blank, it will be set according to the default option,
    // including the year, month, day, hour, minute and second, and the values are all numeric.
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    options = JSDateTimeFormat::ToDateTimeOptions(
        thread, JSHandle<JSTaggedValue>::Cast(options), RequiredOption::ANY, DefaultsOption::ALL); // test "ALL"
    auto yearEcmaStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, yearKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(yearEcmaStr).c_str(), "numeric");
    auto monthEcmaStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, monthKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(monthEcmaStr).c_str(), "numeric");
    auto dayEcmaStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, dayKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dayEcmaStr).c_str(), "numeric");
    auto hourEcmaStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, hourKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(hourEcmaStr).c_str(), "numeric");
    auto minuteEcmaStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, minuteKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(minuteEcmaStr).c_str(), "numeric");
    auto secondEcmaStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, secondKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(secondEcmaStr).c_str(), "numeric");
}

HWTEST_F_L0(JSDateTimeFormatTest, ToDateTimeOptions_002)
{
    auto vm = thread->GetEcmaVM();
    auto factory = vm->GetFactory();
    auto env = vm->GetGlobalEnv();
    auto globalConst = thread->GlobalConstants();

    JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
    JSHandle<JSTaggedValue> weekdayKey = globalConst->GetHandledWeekdayString();
    JSHandle<JSTaggedValue> yearKey = globalConst->GetHandledYearString();
    JSHandle<JSTaggedValue> monthKey = globalConst->GetHandledMonthString();
    JSHandle<JSTaggedValue> dayKey = globalConst->GetHandledDayString();
    JSHandle<JSTaggedValue> dayPeriodKey = globalConst->GetHandledDayPeriodString();
    JSHandle<JSTaggedValue> hourKey = globalConst->GetHandledHourString();
    JSHandle<JSTaggedValue> minuteKey = globalConst->GetHandledMinuteString();
    JSHandle<JSTaggedValue> secondKey = globalConst->GetHandledSecondString();
    JSHandle<JSTaggedValue> fracSecKey = globalConst->GetHandledFractionalSecondDigitsString();

    // When the option value is not empty, it will be set according to the required options.
    JSHandle<JSObject> options = factory->NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
    std::map<std::string, std::string> dateOptionsMap {
        { "weekday", "narrow" },
        { "year", "2-digit" },
        { "month", "2-digit" },
        { "day", "2-digit" }
    };
    std::map<std::string, std::string> timeOptionsMap {
        { "dayPeriod", "narrow" },
        { "hour", "2-digit" },
        { "minute", "2-digit" },
        { "second", "2-digit" },
        { "fractionalSecond", "1" }
    };
    SetDateOptionsTest(thread, options, dateOptionsMap);
    SetTimeOptionsTest(thread, options, timeOptionsMap);
    options = JSDateTimeFormat::ToDateTimeOptions(
        thread, JSHandle<JSTaggedValue>::Cast(options), RequiredOption::ANY, DefaultsOption::ALL); // test "ANY"
    auto weekdayStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, weekdayKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(weekdayStr).c_str(), "narrow");
    auto yearStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, yearKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(yearStr).c_str(), "2-digit");
    auto monthStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, monthKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(monthStr).c_str(), "2-digit");
    auto dayStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, dayKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dayStr).c_str(), "2-digit");

    auto dayPeriodStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, dayPeriodKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dayPeriodStr).c_str(), "narrow");
    auto hourStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, hourKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(hourStr).c_str(), "2-digit");
    auto minuteStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, minuteKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(minuteStr).c_str(), "2-digit");
    auto secondStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, secondKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(secondStr).c_str(), "2-digit");
    auto fracSecStr = JSHandle<EcmaString>::Cast(JSObject::GetProperty(thread, options, fracSecKey).GetValue());
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(fracSecStr).c_str(), "1");
}

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

/**
 * @tc.name: FormatDateTime
 * @tc.desc: Convert floating-point timestamp to fixed format time date through time date format.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F_L0(JSDateTimeFormatTest, FormatDateTime_001)
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
    JSHandle<JSDateTimeFormat> dtf = CreateDateTimeFormatTest(thread, icuLocale, options);

    double timeStamp1 = 1653448174000; // test "2022-05-25 11:09:34.000"
    double timeStamp2 = 1653921012999; // test "2022-05-30 22:30:12.999"

    // When the option is blank, the default format is "yyyy/MM/dd", the year, month and day are all numeric,
    // because the default option in initialization is "DefaultsOption::DATE".
    JSHandle<EcmaString> dateTimeEcamStr1 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp1);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr1).c_str(), "2022/5/25");
    JSHandle<EcmaString> dateTimeEcamStr2 = JSDateTimeFormat::FormatDateTime(thread, dtf, timeStamp2);
    EXPECT_STREQ(LocaleHelper::ConvertToStdString(dateTimeEcamStr2).c_str(), "2022/5/30");
}
} // namespace panda::test
