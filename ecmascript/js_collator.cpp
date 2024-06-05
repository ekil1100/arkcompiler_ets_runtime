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

#include "ecmascript/js_collator.h"

#include "ecmascript/intl/locale_helper.h"
#include "ecmascript/global_env.h"
#include "ecmascript/mem/c_string.h"
#include "ecmascript/mem/barriers-inl.h"
#include "ecmascript/object_factory-inl.h"
#include "ecmascript/ecma_string-inl.h"

#include "unicode/udata.h"

namespace panda::ecmascript {
// NOLINTNEXTLINE (readability-identifier-naming, fuchsia-statically-constructed-objects)
const CString JSCollator::uIcuDataColl = U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll";
const std::map<std::string, CaseFirstOption> JSCollator::caseFirstMap = {
    {"upper", CaseFirstOption::UPPER},
    {"lower", CaseFirstOption::LOWER},
    {"false", CaseFirstOption::FALSE_OPTION}
};
const std::map<CaseFirstOption, UColAttributeValue> JSCollator::uColAttributeValueMap = {
    {CaseFirstOption::UPPER, UCOL_UPPER_FIRST},
    {CaseFirstOption::LOWER, UCOL_LOWER_FIRST},
    {CaseFirstOption::FALSE_OPTION, UCOL_OFF},
    {CaseFirstOption::UNDEFINED, UCOL_OFF}
};

JSHandle<TaggedArray> JSCollator::GetAvailableLocales(JSThread *thread, bool enableLocaleCache)
{
    const char *key = nullptr;
    const char *path = JSCollator::uIcuDataColl.c_str();
    // key and path are const, so we can cache the result
    if (enableLocaleCache) {
        JSHandle<JSTaggedValue> cachedLocales = thread->GlobalConstants()->GetHandledCachedJSCollatorLocales();
        if (cachedLocales->IsHeapObject()) {
            return JSHandle<TaggedArray>(cachedLocales);
        }
    }
    std::vector<std::string> availableStringLocales = intl::LocaleHelper::GetAvailableLocales(thread, key, path);
    JSHandle<TaggedArray> availableLocales = JSLocale::ConstructLocaleList(thread, availableStringLocales);
    if (enableLocaleCache) {
        GlobalEnvConstants *constants = const_cast<GlobalEnvConstants *>(thread->GlobalConstants());
        constants->SetCachedLocales(availableLocales.GetTaggedValue());
    }
    return availableLocales;
}

/* static */
void JSCollator::SetIcuCollator(JSThread *thread, const JSHandle<JSCollator> &collator,
    icu::Collator *icuCollator, const NativePointerCallback &callback)
{
    EcmaVM *ecmaVm = thread->GetEcmaVM();
    ObjectFactory *factory = ecmaVm->GetFactory();

    ASSERT(icuCollator != nullptr);
    JSTaggedValue data = collator->GetIcuField();
    if (data.IsJSNativePointer()) {
        JSNativePointer *native = JSNativePointer::Cast(data.GetTaggedObject());
        native->ResetExternalPointer(thread, icuCollator);
        return;
    }
    JSHandle<JSNativePointer> pointer = factory->NewJSNativePointer(icuCollator, callback);
    collator->SetIcuField(thread, pointer.GetTaggedValue());
}

JSHandle<JSCollator> JSCollator::InitializeCollator(JSThread *thread,
                                                    const JSHandle<JSCollator> &collator,
                                                    const JSHandle<JSTaggedValue> &locales,
                                                    const JSHandle<JSTaggedValue> &options,
                                                    bool forIcuCache,
                                                    bool enableLocaleCache)
{
    EcmaVM *ecmaVm = thread->GetEcmaVM();
    ObjectFactory *factory = ecmaVm->GetFactory();
    const GlobalEnvConstants *globalConst = thread->GlobalConstants();
    // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
    JSHandle<TaggedArray> requestedLocales = intl::LocaleHelper::CanonicalizeLocaleList(thread, locales);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);

    // 2. If options is undefined, then
    //      a. Let options be ObjectCreate(null).
    // 3. Else,
    //      a. Let options be ? ToObject(options).
    JSHandle<JSObject> optionsObject;
    if (options->IsUndefined()) {
        optionsObject = factory->CreateNullJSObject();
    } else {
        optionsObject = JSTaggedValue::ToObject(thread, options);
        RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);
    }
    // 4. Let usage be ? GetOption(options, "usage", "string", « "sort", "search" », "sort").
    auto usage = JSLocale::GetOptionOfString<UsageOption>(thread, optionsObject, globalConst->GetHandledUsageString(),
                                                          {UsageOption::SORT, UsageOption::SEARCH}, {"sort", "search"},
                                                          UsageOption::SORT);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);
    collator->SetUsage(usage);

    // 5. Let matcher be ? GetOption(options, "localeMatcher", "string", « "lookup", "best fit" », "best fit").
    auto matcher = JSLocale::GetOptionOfString<LocaleMatcherOption>(
        thread, optionsObject, globalConst->GetHandledLocaleMatcherString(),
        {LocaleMatcherOption::LOOKUP, LocaleMatcherOption::BEST_FIT}, {"lookup", "best fit"},
        LocaleMatcherOption::BEST_FIT);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);

    // 6. Let collation be ? GetOption(options, "collation", "string", undefined, undefined).
    // 7. If collation is not undefined, then
    //    a. If collation does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
    JSHandle<JSTaggedValue> collation =
        JSLocale::GetOption(thread, optionsObject, globalConst->GetHandledCollationString(), OptionType::STRING,
                            globalConst->GetHandledUndefined(), globalConst->GetHandledUndefined());
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);
    collator->SetCollation(thread, collation);
    std::string collationStr;
    if (!collation->IsUndefined()) {
        JSHandle<EcmaString> collationEcmaStr = JSHandle<EcmaString>::Cast(collation);
        collationStr = intl::LocaleHelper::ConvertToStdString(collationEcmaStr);
        if (!JSLocale::IsWellAlphaNumList(collationStr)) {
            THROW_RANGE_ERROR_AND_RETURN(thread, "invalid collation", collator);
        }
    }

    // 8. Let numeric be ? GetOption(options, "numeric", "boolean", undefined, undefined).
    bool numeric = false;
    bool foundNumeric =
        JSLocale::GetOptionOfBool(thread, optionsObject, globalConst->GetHandledNumericString(), false, &numeric);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);
    collator->SetNumeric(numeric);

    // 14. Let caseFirst be ? GetOption(options, "caseFirst", "string", « "upper", "lower", "false" », undefined).
    CaseFirstOption caseFirst = JSLocale::GetOptionOfString<CaseFirstOption>(
        thread, optionsObject, globalConst->GetHandledCaseFirstString(),
        {CaseFirstOption::UPPER, CaseFirstOption::LOWER, CaseFirstOption::FALSE_OPTION}, {"upper", "lower", "false"},
        CaseFirstOption::UNDEFINED);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);
    collator->SetCaseFirst(caseFirst);

    // 16. Let relevantExtensionKeys be %Collator%.[[RelevantExtensionKeys]].
    std::set<std::string> relevantExtensionKeys = {"co", "kn", "kf"};

    // 17. Let r be ResolveLocale(%Collator%.[[AvailableLocales]], requestedLocales, opt,
    //     %Collator%.[[RelevantExtensionKeys]], localeData).
    JSHandle<TaggedArray> availableLocales;
    if (requestedLocales->GetLength() == 0) {
        availableLocales = factory->EmptyArray();
    } else {
        availableLocales = GetAvailableLocales(thread, enableLocaleCache);
    }
    ResolvedLocale r =
        JSLocale::ResolveLocale(thread, availableLocales, requestedLocales, matcher, relevantExtensionKeys);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);
    icu::Locale icuLocale = r.localeData;
    JSHandle<EcmaString> localeStr = intl::LocaleHelper::ToLanguageTag(thread, icuLocale);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);
    collator->SetLocale(thread, localeStr.GetTaggedValue());
    ASSERT_PRINT(!icuLocale.isBogus(), "icuLocale is bogus");

    // If collation is undefined iterate RelevantExtensionKeys to find "co"
    //  if found, set ICU collator UnicodeKeyword to iterator->second
    UErrorCode status = U_ZERO_ERROR;
    if (!collation->IsUndefined()) {
        auto extensionIter = r.extensions.find("co");
        if (extensionIter != r.extensions.end() && extensionIter->second != collationStr) {
            icuLocale.setUnicodeKeywordValue("co", nullptr, status);
            ASSERT_PRINT(U_SUCCESS(status), "icuLocale set co failed");
        }
    }

    // If usage is serach set co-serach to icu locale key word value
    // Eles set collation string to icu locale key word value
    if (usage == UsageOption::SEARCH) {
        icuLocale.setUnicodeKeywordValue("co", "search", status);
        ASSERT(U_SUCCESS(status));
    } else {
        if (!collationStr.empty() && JSLocale::IsWellCollation(icuLocale, collationStr)) {
            icuLocale.setUnicodeKeywordValue("co", collationStr, status);
            ASSERT(U_SUCCESS(status));
        }
    }

    std::unique_ptr<icu::Collator> icuCollator(icu::Collator::createInstance(icuLocale, status));
    if (U_FAILURE(status) || icuCollator == nullptr) {  // NOLINT(readability-implicit-bool-conversion)
        if (status == UErrorCode::U_MISSING_RESOURCE_ERROR) {
            THROW_REFERENCE_ERROR_AND_RETURN(thread, "can not find icu data resources", collator);
        }
        status = U_ZERO_ERROR;
        icu::Locale localeName(icuLocale.getBaseName());
        icuCollator.reset(icu::Collator::createInstance(localeName, status));
        if (U_FAILURE(status) || icuCollator == nullptr) {  // NOLINT(readability-implicit-bool-conversion)
            THROW_RANGE_ERROR_AND_RETURN(thread, "invalid collation", collator);
        }
    }
    ASSERT(U_SUCCESS(status));
    icu::Locale collatorLocale(icuCollator->getLocale(ULOC_VALID_LOCALE, status));

    icuCollator->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, status);
    ASSERT(U_SUCCESS(status));

    // If numeric is found set ICU collator UCOL_NUMERIC_COLLATION to numeric
    // Else iterate RelevantExtensionKeys to find "kn"
    //  if found, set ICU collator UCOL_NUMERIC_COLLATION to iterator->second
    status = U_ZERO_ERROR;
    if (foundNumeric) {
        ASSERT(icuCollator.get() != nullptr);
        icuCollator.get()->setAttribute(UCOL_NUMERIC_COLLATION, numeric ? UCOL_ON : UCOL_OFF, status);
        ASSERT(U_SUCCESS(status));
    } else {
        auto extensionIter = r.extensions.find("kn");
        if (extensionIter != r.extensions.end()) {
            ASSERT(icuCollator.get() != nullptr);
            bool found = (extensionIter->second == "true");
            collator->SetNumeric(found);
            icuCollator.get()->setAttribute(UCOL_NUMERIC_COLLATION, found ? UCOL_ON : UCOL_OFF, status);
            ASSERT(U_SUCCESS(status));
        }
    }

    // If caseFirst is not undefined set ICU collator UColAttributeValue to caseFirst
    // Else iterate RelevantExtensionKeys to find "kf"
    //  if found, set ICU collator UColAttributeValue to iterator->second
    status = U_ZERO_ERROR;
    if (caseFirst != CaseFirstOption::UNDEFINED) {
        ASSERT(icuCollator.get() != nullptr);
        icuCollator.get()->setAttribute(UCOL_CASE_FIRST, OptionToUColAttribute(caseFirst), status);
        ASSERT(U_SUCCESS(status));
    } else {
        auto extensionIter = r.extensions.find("kf");
        if (extensionIter != r.extensions.end()) {
            ASSERT(icuCollator.get() != nullptr);
            auto mapIter = caseFirstMap.find(extensionIter->second);
            if (mapIter != caseFirstMap.end()) {
                icuCollator.get()->setAttribute(UCOL_CASE_FIRST, OptionToUColAttribute(mapIter->second), status);
                collator->SetCaseFirst(mapIter->second);
            } else {
                icuCollator.get()->setAttribute(UCOL_CASE_FIRST, OptionToUColAttribute(CaseFirstOption::UNDEFINED),
                                                status);
            }
            ASSERT(U_SUCCESS(status));
        }
    }

    // 24. Let sensitivity be ? GetOption(options, "sensitivity", "string", « "base", "accent", "case", "variant" »,
    //     undefined).
    SensitivityOption sensitivity = JSLocale::GetOptionOfString<SensitivityOption>(
        thread, optionsObject, globalConst->GetHandledSensitivityString(),
        {SensitivityOption::BASE, SensitivityOption::ACCENT, SensitivityOption::CASE, SensitivityOption::VARIANT},
        {"base", "accent", "case", "variant"}, SensitivityOption::UNDEFINED);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSCollator, thread);
    // 25. If sensitivity is undefined, then
    //     a. If usage is "sort", then
    //        i. Let sensitivity be "variant".
    if (sensitivity == SensitivityOption::UNDEFINED) {
        if (usage == UsageOption::SORT) {
            sensitivity = SensitivityOption::VARIANT;
        }
    }
    collator->SetSensitivity(sensitivity);

    // Trans SensitivityOption to Icu strength option
    switch (sensitivity) {
        case SensitivityOption::BASE:
            icuCollator->setStrength(icu::Collator::PRIMARY);
            break;
        case SensitivityOption::ACCENT:
            icuCollator->setStrength(icu::Collator::SECONDARY);
            break;
        case SensitivityOption::CASE:
            icuCollator->setStrength(icu::Collator::PRIMARY);
            icuCollator->setAttribute(UCOL_CASE_LEVEL, UCOL_ON, status);
            break;
        case SensitivityOption::VARIANT:
            icuCollator->setStrength(icu::Collator::TERTIARY);
            break;
        case SensitivityOption::UNDEFINED:
            break;
        case SensitivityOption::EXCEPTION:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }

    // 27. Let ignorePunctuation be ? GetOption(options, "ignorePunctuation", "boolean", undefined, false).
    // 28. Set collator.[[IgnorePunctuation]] to ignorePunctuation.
    bool ignorePunctuation = false;
    bool defaultIgnorePunctuation = false;
    // If the ignorePunctuation is not defined, which in "th" locale that is true but false on other locales.
    JSHandle<EcmaString> thKey = factory->NewFromUtf8("th");
    if (JSTaggedValue::Equal(thread, JSHandle<JSTaggedValue>::Cast(thKey), locales)) {
        defaultIgnorePunctuation = true;
    }
    JSLocale::GetOptionOfBool(thread, optionsObject, globalConst->GetHandledIgnorePunctuationString(),
                              defaultIgnorePunctuation, &ignorePunctuation);
    collator->SetIgnorePunctuation(ignorePunctuation);
    if (ignorePunctuation) {
        status = U_ZERO_ERROR;
        icuCollator->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, status);
        ASSERT(U_SUCCESS(status));
    }

    if (forIcuCache) {
        std::string cacheEntry =
            locales->IsUndefined() ? "" : EcmaStringAccessor(locales.GetTaggedValue()).ToStdString();
        thread->GetCurrentEcmaContext()->SetIcuFormatterToCache(IcuFormatterType::COLLATOR,
            cacheEntry, icuCollator.release(), JSCollator::FreeIcuCollator);
    } else {
        SetIcuCollator(thread, collator, icuCollator.release(), JSCollator::FreeIcuCollator);
    }
    collator->SetBoundCompare(thread, JSTaggedValue::Undefined());
    // 29. Return collator.
    return collator;
}

icu::Collator *JSCollator::GetCachedIcuCollator(JSThread *thread, const JSTaggedValue &locales)
{
    std::string cacheEntry = locales.IsUndefined() ? "" : EcmaStringAccessor(locales).ToStdString();
    void *cachedCollator =
        thread->GetCurrentEcmaContext()->GetIcuFormatterFromCache(IcuFormatterType::COLLATOR, cacheEntry);
    if (cachedCollator != nullptr) {
        return reinterpret_cast<icu::Collator*>(cachedCollator);
    }
    return nullptr;
}

icu::Collator *JSCollator::GetCachedIcuCollator(JSThread *thread, const JSHandle<JSTaggedValue> &locales)
{
    return GetCachedIcuCollator(thread, locales.GetTaggedValue());
}

UColAttributeValue JSCollator::OptionToUColAttribute(CaseFirstOption caseFirstOption)
{
    auto iter = uColAttributeValueMap.find(caseFirstOption);
    if (iter != uColAttributeValueMap.end()) {
        return iter->second;
    }
    LOG_ECMA(FATAL) << "this branch is unreachable";
    UNREACHABLE();
}

JSHandle<JSTaggedValue> OptionsToEcmaString(JSThread *thread, UsageOption usage)
{
    JSMutableHandle<JSTaggedValue> result(thread, JSTaggedValue::Undefined());
    auto globalConst = thread->GlobalConstants();
    switch (usage) {
        case UsageOption::SORT:
            result.Update(globalConst->GetSortString());
            break;
        case UsageOption::SEARCH:
            result.Update(globalConst->GetSearchString());
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
    return result;
}

JSHandle<JSTaggedValue> OptionsToEcmaString(JSThread *thread, SensitivityOption sensitivity)
{
    JSMutableHandle<JSTaggedValue> result(thread, JSTaggedValue::Undefined());
    auto globalConst = thread->GlobalConstants();
    switch (sensitivity) {
        case SensitivityOption::BASE:
            result.Update(globalConst->GetBaseString());
            break;
        case SensitivityOption::ACCENT:
            result.Update(globalConst->GetAccentString());
            break;
        case SensitivityOption::CASE:
            result.Update(globalConst->GetCaseString());
            break;
        case SensitivityOption::VARIANT:
            result.Update(globalConst->GetVariantString());
            break;
        case SensitivityOption::UNDEFINED:
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
    return result;
}

JSHandle<JSTaggedValue> OptionsToEcmaString(JSThread *thread, CaseFirstOption caseFirst)
{
    JSMutableHandle<JSTaggedValue> result(thread, JSTaggedValue::Undefined());
    auto globalConst = thread->GlobalConstants();
    switch (caseFirst) {
        case CaseFirstOption::UPPER:
            result.Update(globalConst->GetUpperString());
            break;
        case CaseFirstOption::LOWER:
            result.Update(globalConst->GetLowerString());
            break;
        case CaseFirstOption::FALSE_OPTION:
            result.Update(globalConst->GetFalseString());
            break;
        case CaseFirstOption::UNDEFINED:
            result.Update(globalConst->GetUpperString());
            break;
        default:
            LOG_ECMA(FATAL) << "this branch is unreachable";
            UNREACHABLE();
    }
    return result;
}

// 11.3.4 Intl.Collator.prototype.resolvedOptions ()
JSHandle<JSObject> JSCollator::ResolvedOptions(JSThread *thread, const JSHandle<JSCollator> &collator)
{
    auto ecmaVm = thread->GetEcmaVM();
    auto globalConst = thread->GlobalConstants();
    ObjectFactory *factory = ecmaVm->GetFactory();
    JSHandle<GlobalEnv> env = ecmaVm->GetGlobalEnv();
    JSHandle<JSFunction> funCtor(env->GetObjectFunction());
    JSHandle<JSObject> options(factory->NewJSObjectByConstructor(funCtor));

    // [[Locale]]
    JSHandle<JSTaggedValue> property = globalConst->GetHandledLocaleString();
    JSHandle<JSTaggedValue> locale(thread, collator->GetLocale());
    JSObject::CreateDataPropertyOrThrow(thread, options, property, locale);
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSObject, thread);

    // [[Usage]]
    UsageOption usageOption = collator->GetUsage();
    JSHandle<JSTaggedValue> usageValue = OptionsToEcmaString(thread, usageOption);
    JSObject::CreateDataProperty(thread, options, globalConst->GetHandledUsageString(), usageValue);

    // [[Sensitivity]]
    auto sentivityOption = collator->GetSensitivity();
    JSHandle<JSTaggedValue> sensitivityValue = OptionsToEcmaString(thread, sentivityOption);
    JSObject::CreateDataProperty(thread, options, globalConst->GetHandledSensitivityString(), sensitivityValue);

    // [[IgnorePunctuation]]
    JSHandle<JSTaggedValue> ignorePunctuationValue(thread, JSTaggedValue(collator->GetIgnorePunctuation()));
    JSObject::CreateDataProperty(thread, options, globalConst->GetHandledIgnorePunctuationString(),
                                 ignorePunctuationValue);

    // [[Collation]]
    JSMutableHandle<JSTaggedValue> collationValue(thread, collator->GetCollation());
    UErrorCode status = U_ZERO_ERROR;
    icu::Collator *icuCollator = collator->GetIcuCollator();
    icu::Locale icu_locale(icuCollator->getLocale(ULOC_VALID_LOCALE, status));
    std::string collation_value =
        icu_locale.getUnicodeKeywordValue<std::string>("co", status);
    if (collationValue->IsUndefined()) {
        if (collation_value != "search" && collation_value != "") {
            collationValue.Update(factory->NewFromStdString(collation_value).GetTaggedValue());
        } else {
            collationValue.Update(globalConst->GetDefaultString());
        }
    }
    JSObject::CreateDataProperty(thread, options, globalConst->GetHandledCollationString(), collationValue);

    // [[Numeric]]
    JSHandle<JSTaggedValue> numericValue(thread, JSTaggedValue(collator->GetNumeric()));
    JSObject::CreateDataProperty(thread, options, globalConst->GetHandledNumericString(), numericValue);

    // [[CaseFirst]]
    CaseFirstOption caseFirstOption = collator->GetCaseFirst();
    // In Ecma402 spec, caseFirst is an optional property so we set it to Upper when input is undefined
    // the requirement maybe change in the future
    JSHandle<JSTaggedValue> caseFirstValue = OptionsToEcmaString(thread, caseFirstOption);
    JSObject::CreateDataProperty(thread, options, globalConst->GetHandledCaseFirstString(), caseFirstValue);
    return options;
}

ARK_INLINE icu::UnicodeString EcmaStringToUString(EcmaString *string)
{
    CVector<uint8_t> buf;
    Span<const uint8_t> span = EcmaStringAccessor(string).ToUtf8Span(buf);
    icu::StringPiece sp(reinterpret_cast<const char*>(span.begin()), span.size());
    icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sp);
    return uString;
}

icu::UnicodeString EcmaStringToUString(const JSHandle<EcmaString> &string)
{
    return EcmaStringToUString(string.GetObject<EcmaString>());
}

JSTaggedValue JSCollator::CompareStrings(const icu::Collator *icuCollator, const JSHandle<EcmaString> &string1,
                                         const JSHandle<EcmaString> &string2)
{
    return CompareStrings(icuCollator, string1.GetObject<EcmaString>(), string2.GetObject<EcmaString>());
}

JSTaggedValue JSCollator::CompareStrings(const icu::Collator *icuCollator, EcmaString *string1, EcmaString *string2)
{
    UCollationResult result;
    UErrorCode status = U_ZERO_ERROR;
    if (string1 == string2) {
        return JSTaggedValue(UCollationResult::UCOL_EQUAL);
    }
    {
        EcmaStringAccessor string1Acc(string1);
        EcmaStringAccessor string2Acc(string2);
        if (string1Acc.IsUtf8() && string2Acc.IsUtf8()) {
            icu::StringPiece stringPiece1;
            icu::StringPiece stringPiece2;
            CVector<uint8_t> buf1;
            CVector<uint8_t> buf2;
            if (string1Acc.IsLineOrConstantString()) {
                stringPiece1 = icu::StringPiece(reinterpret_cast<const char*>(string1Acc.GetDataUtf8()),
                                                string1Acc.GetLength());
            } else {
                Span<const uint8_t> span = string1Acc.ToUtf8Span(buf1);
                stringPiece1 = icu::StringPiece(reinterpret_cast<const char*>(span.begin()), span.size());
            }

            if (string2Acc.IsLineOrConstantString()) {
                stringPiece2 = icu::StringPiece(reinterpret_cast<const char*>(string2Acc.GetDataUtf8()),
                                                string2Acc.GetLength());
            } else {
                Span<const uint8_t> span = string2Acc.ToUtf8Span(buf2);
                stringPiece2 = icu::StringPiece(reinterpret_cast<const char*>(span.begin()), span.size());
            }
            result = icuCollator->compareUTF8(stringPiece1, stringPiece2, status);
            return JSTaggedValue(result);
        }
    }
    icu::UnicodeString uString1 = EcmaStringToUString(string1);
    icu::UnicodeString uString2 = EcmaStringToUString(string2);

    result = icuCollator->compare(uString1, uString2, status);
    ASSERT(U_SUCCESS(status));

    return JSTaggedValue(result);
}

JSTaggedValue JSCollator::FastCompareStrings(JSThread *thread, const icu::Collator *icuCollator,
                                             const JSHandle<EcmaString> &string1,
                                             const JSHandle<EcmaString> &string2)
{
    if (*string1 == *string2) {
        return JSTaggedValue(UCollationResult::UCOL_EQUAL);
    }

    auto flatString1 = JSHandle<EcmaString>(thread, EcmaStringAccessor::Flatten(thread->GetEcmaVM(), string1));
    auto flatString2 = JSHandle<EcmaString>(thread, EcmaStringAccessor::Flatten(thread->GetEcmaVM(), string2));

    UCollationResult result;
    UErrorCode status = U_ZERO_ERROR;
    {
        DISALLOW_GARBAGE_COLLECTION;
        CString str1 = ConvertToString(*flatString1, StringConvertedUsage::LOGICOPERATION);
        icu::StringPiece stringPiece1(str1.c_str());
        if (!stringPiece1.empty()) {
            CString str2 = ConvertToString(*flatString2, StringConvertedUsage::LOGICOPERATION);
            icu::StringPiece stringPiece2(str2.c_str());
            if (!stringPiece2.empty()) {
                result = icuCollator->compareUTF8(stringPiece1, stringPiece2, status);
                return JSTaggedValue(result);
            }
        }

        icu::UnicodeString uString1 = EcmaStringToUString(flatString1);
        icu::UnicodeString uString2 = EcmaStringToUString(flatString2);

        result = icuCollator->compare(uString1, uString2, status);
        ASSERT(U_SUCCESS(status));
    }
    return JSTaggedValue(result);
}
}  // namespace panda::ecmascript
