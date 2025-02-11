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

#include "ecmascript/base/number_helper.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <sys/time.h>

#include "ecmascript/base/builtins_base.h"
#include "ecmascript/base/string_helper.h"
#include "ecmascript/ecma_string_table.h"
#include "ecmascript/js_tagged_value-inl.h"
#include "ecmascript/object_factory.h"

namespace panda::ecmascript::base {
enum class Sign { NONE, NEG, POS };
thread_local uint64_t RandomGenerator::randomState_ {0};
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RETURN_IF_CONVERSION_END(p, end, result) \
    if ((p) == (end)) {                          \
        return (result);                         \
    }

constexpr char CHARS[] = "0123456789abcdefghijklmnopqrstuvwxyz";  // NOLINT (modernize-avoid-c-arrays)
constexpr uint64_t MAX_MANTISSA = 0x1ULL << 52U;

static const double POWERS_OF_TEN[] = {
    1.0,                      // 10^0
    10.0,
    100.0,
    1000.0,
    10000.0,
    100000.0,
    1000000.0,
    10000000.0,
    100000000.0,
    1000000000.0,
    10000000000.0,            // 10^10
    100000000000.0,
    1000000000000.0,
    10000000000000.0,
    100000000000000.0,
    1000000000000000.0,
    10000000000000000.0,
    100000000000000000.0,
    1000000000000000000.0,
    10000000000000000000.0,
    100000000000000000000.0,  // 10^20
    1000000000000000000000.0,
    10000000000000000000000.0 // 10^22
};
static const int POWERS_OF_TEN_SIZE = 23;

static inline uint8_t ToDigit(uint8_t c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + DECIMAL;
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + DECIMAL;
    }
    return '$';
}

bool NumberHelper::GotoNonspace(uint8_t **ptr, const uint8_t *end)
{
    while (*ptr < end) {
        uint16_t c = **ptr;
        size_t size = 1;
        if (c > INT8_MAX) {
            size = 0;
            uint16_t utf8Bit = INT8_MAX + 1;  // equal 0b1000'0000
            while (utf8Bit > 0 && (c & utf8Bit) == utf8Bit) {
                ++size;
                utf8Bit >>= 1UL;
            }
            if (base::utf_helper::ConvertRegionUtf8ToUtf16(*ptr, &c, end - *ptr, 1, 0) <= 0) {
                return true;
            }
        }
        if (!StringHelper::IsNonspace(c)) {
            return true;
        }
        *ptr += size;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return false;
}

static inline double SignedZero(Sign sign)
{
    return sign == Sign::NEG ? -0.0 : 0.0;
}

bool NumberHelper::IsEmptyString(const uint8_t *start, const uint8_t *end)
{
    auto p = const_cast<uint8_t *>(start);
    return !NumberHelper::GotoNonspace(&p, end);
}

/*
*  This Function Translate from number 0-9 to number '0'-'9'
*                               number 10-35 to number 'a'-'z'
*/
uint32_t NumberHelper::ToCharCode(uint32_t number)
{
    ASSERT(number < 36); // total number of '0'-'9' + 'a' -'z'
    return number < 10 ? (number + 48): // 48 == '0'; 10: '0' - '9';
                         (number - 10 + 97); // 97 == 'a'; 'a' - 'z'
}

JSTaggedValue NumberHelper::Int32ToString(JSThread *thread, int32_t number, uint32_t radix)
{
    bool isNegative = number < 0;
    uint32_t n = 0;
    if (!isNegative) {
        n = static_cast<uint32_t>(number);
        if (n < radix) {
            if (n == 0) {
                return thread->GlobalConstants()->GetHandledZeroString().GetTaggedValue();
            }
            JSHandle<SingleCharTable> singleCharTable(thread, thread->GetSingleCharTable());
            return singleCharTable->GetStringFromSingleCharTable(ToCharCode(n));
        }
    } else {
        n = static_cast<uint32_t>(-number);
    }
    uint32_t temp = n;
    uint32_t length = isNegative ? 1 : 0;
    // calculate length
    while (temp > 0) {
        temp = temp / radix;
        length = length + 1;
    }
    std::string buf;
    buf.resize(length);
    uint32_t index = length - 1;
    uint32_t digit = 0;
    while (n > 0) {
        digit = n % radix;
        n /= radix;
        buf[index] = ToCharCode(digit) + 0X00;
        index--;
    }
    if (isNegative) {
        ASSERT(index == 0);
        buf[index] = '-';
    }
    return thread->GetEcmaVM()->GetFactory()->NewFromUtf8(buf).GetTaggedValue();
}

bool inline IsDenormal(uint64_t x)
{
    return (x & kINFINITY) == 0;
}

int inline Exponent(double x)
{
    uint64_t value =  base::bit_cast<uint64_t>(x);
    if (IsDenormal(value)) {
        return kDENORMAL;
    }
    int biased = static_cast<int>((value & kINFINITY) >> DOUBLE_SIGNIFICAND_SIZE);
    return biased - EXPONENTBIAS;
}

JSTaggedValue NumberHelper::DoubleToString(JSThread *thread, double number, int radix)
{
    static constexpr int BUFFER_SIZE = 2240; // 2240: The size of the character array buffer
    static constexpr int HALF_BUFFER_SIZE = BUFFER_SIZE >> 1;
    char buffer[BUFFER_SIZE];
    size_t integerCursor = HALF_BUFFER_SIZE;
    size_t fractionCursor = integerCursor;

    bool negative = number < 0.0;
    if (negative) {
        number = -number;
    }

    double integer = std::floor(number);
    double fraction = number - integer;

    auto value = bit_cast<uint64_t>(number);
    value += 1;
    double delta = HALF * (bit_cast<double>(value) - number);
    delta = std::max(delta, bit_cast<double>(static_cast<uint64_t>(1))); // 1 : The binary of the smallest double is 1
    if (fraction != 0 && fraction >= delta) {
        buffer[fractionCursor++] = '.';
        while (fraction >= delta) {
            fraction *= radix;
            delta *= radix;
            int64_t digit = std::floor(fraction);
            fraction -= digit;
            buffer[fractionCursor++] = CHARS[digit];
            bool needCarry = (fraction > HALF) && (fraction + delta > 1);
            if (needCarry) {
                size_t fractionEnd = fractionCursor - 1;
                buffer[fractionEnd] = Carry(buffer[fractionEnd], radix);
                for (; fractionEnd > HALF_BUFFER_SIZE; fractionEnd--) {
                    if (buffer[fractionEnd] == '0') {
                        buffer[fractionEnd - 1] = Carry(buffer[fractionEnd - 1], radix);
                    } else {
                        break;
                    }
                }
                if (fractionEnd == HALF_BUFFER_SIZE) {
                    ++integer;
                }
                break;
            }
        }
        // delete 0 in the end
        size_t fractionEnd = fractionCursor - 1;
        while (buffer[fractionEnd] == '0') {
            --fractionEnd;
        }
        fractionCursor = fractionEnd + 1;
    }

    ASSERT(radix >= MIN_RADIX && radix <= MAX_RADIX);
    while (Exponent(integer / radix) > 0) {
        integer /= radix;
        buffer[--integerCursor] = '0';
    }
    do {
        double remainder = std::fmod(integer, radix);
        buffer[--integerCursor] = CHARS[static_cast<int>(remainder)];
        integer = (integer - remainder) / radix;
    } while (integer > 0);

    if (negative) {
        buffer[--integerCursor] = '-';
    }
    buffer[fractionCursor++] = '\0';

    size_t size = fractionCursor - integerCursor;
    std::unique_ptr<char[]> result = std::make_unique<char[]>(size);
    if (memcpy_s(result.get(), size, buffer + integerCursor, size) != EOK) {
        LOG_FULL(FATAL) << "memcpy_s failed";
        UNREACHABLE();
    }
    return BuiltinsBase::GetTaggedString(thread, result.get());
}

JSTaggedValue NumberHelper::DoubleToExponential(JSThread *thread, double number, int digit)
{
    CStringStream ss;
    if (digit < 0) {
        ss << std::setiosflags(std::ios::scientific) << std::setprecision(base::MAX_PRECISION) << number;
    } else {
        ss << std::setiosflags(std::ios::scientific) << std::setprecision(digit) << number;
    }
    CString result = ss.str();
    size_t found = result.find_last_of('e');
    if (found != CString::npos && found < result.size() - 2 && result[found + 2] == '0') {
        result.erase(found + 2, 1); // 2:offset of e
    }
    if (digit < 0) {
        size_t end = found;
        while (--found > 0) {
            if (result[found] != '0') {
                break;
            }
        }
        if (result[found] == '.') {
            found--;
        }
        if (found < end - 1) {
            result.erase(found + 1, end - found - 1);
        }
    }
    return BuiltinsBase::GetTaggedString(thread, result.c_str());
}

JSTaggedValue NumberHelper::DoubleToFixed(JSThread *thread, double number, int digit)
{
    CStringStream ss;
    ss << std::setiosflags(std::ios::fixed) << std::setprecision(digit) << number;
    return BuiltinsBase::GetTaggedString(thread, ss.str().c_str());
}

JSTaggedValue NumberHelper::DoubleToPrecision(JSThread *thread, double number, int digit)
{
    if (number == 0.0) {
        return DoubleToFixed(thread, number, digit - 1);
    }
    CStringStream ss;
    double positiveNumber = number > 0 ? number : -number;
    int logDigit = std::floor(log10(positiveNumber));
    int radixDigit = digit - logDigit - 1;
    const int MAX_EXPONENT_DIGIT = 6;
    if ((logDigit >= 0 && radixDigit >= 0) || (logDigit < 0 && radixDigit <= MAX_EXPONENT_DIGIT)) {
        return DoubleToFixed(thread, number, std::abs(radixDigit));
    }
    return DoubleToExponential(thread, number, digit - 1);
}

JSTaggedValue NumberHelper::StringToDoubleWithRadix(const uint8_t *start, const uint8_t *end, int radix, bool *negative)
{
    auto p = const_cast<uint8_t *>(start);
    JSTaggedValue nanResult = BuiltinsBase::GetTaggedDouble(NAN_VALUE);
    // 1. skip space and line terminal
    if (!NumberHelper::GotoNonspace(&p, end)) {
        return nanResult;
    }

    // 2. sign bit
    if (*p == '-') {
        *negative = true;
        RETURN_IF_CONVERSION_END(++p, end, nanResult);
    } else if (*p == '+') {
        RETURN_IF_CONVERSION_END(++p, end, nanResult);
    }
    // 3. 0x or 0X
    bool stripPrefix = true;
    // 4. If R  0, then
    //     a. If R < 2 or R > 36, return NaN.
    //     b. If R  16, let stripPrefix be false.
    if (radix != 0) {
        if (radix < MIN_RADIX || radix > MAX_RADIX) {
            return nanResult;
        }
        if (radix != HEXADECIMAL) {
            stripPrefix = false;
        }
    } else {
        radix = DECIMAL;
    }
    int size = 0;
    if (stripPrefix) {
        if (*p == '0') {
            size++;
            if (++p != end && (*p == 'x' || *p == 'X')) {
                RETURN_IF_CONVERSION_END(++p, end, nanResult);
                radix = HEXADECIMAL;
            }
        }
    }

    double result = 0;
    bool isDone = false;
    do {
        double part = 0;
        uint32_t multiplier = 1;
        for (; p != end; ++p) {
            // The maximum value to ensure that uint32_t will not overflow
            const uint32_t MAX_MULTIPER = 0xffffffffU / 36;
            uint32_t m = multiplier * static_cast<uint32_t>(radix);
            if (m > MAX_MULTIPER) {
                break;
            }

            int currentBit = static_cast<int>(ToDigit(*p));
            if (currentBit >= radix) {
                isDone = true;
                break;
            }
            size++;
            part = part * radix + currentBit;
            multiplier = m;
        }
        result = result * multiplier + part;
        if (isDone) {
            break;
        }
    } while (p != end);

    if (size == 0) {
        return nanResult;
    }

    if (*negative) {
        result = -result;
    }
    return BuiltinsBase::GetTaggedDouble(result);
}

char NumberHelper::Carry(char current, int radix)
{
    int digit = static_cast<int>((current > '9') ? (current - 'a' + DECIMAL) : (current - '0'));
    digit = (digit == (radix - 1)) ? 0 : digit + 1;
    return CHARS[digit];
}

CString NumberHelper::IntegerToString(double number, int radix)
{
    ASSERT(radix >= MIN_RADIX && radix <= MAX_RADIX);
    CString result;
    while (number / radix > MAX_MANTISSA) {
        number /= radix;
        result = CString("0").append(result);
    }
    do {
        double remainder = std::fmod(number, radix);
        result = CHARS[static_cast<int>(remainder)] + result;
        number = (number - remainder) / radix;
    } while (number > 0);
    return result;
}

CString NumberHelper::IntToString(int number)
{
    return ToCString(number);
}

JSHandle<EcmaString> NumberHelper::IntToEcmaString(const JSThread *thread, int number)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    return factory->NewFromASCII(ToCString(number));
}

// 7.1.12.1 ToString Applied to the Number Type
JSHandle<EcmaString> NumberHelper::NumberToString(const JSThread *thread, JSTaggedValue number)
{
    ASSERT(number.IsNumber());
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    if (number.IsInt()) {
        int intVal = number.GetInt();
        if (intVal == 0) {
            return JSHandle<EcmaString>::Cast(thread->GlobalConstants()->GetHandledZeroString());
        }
        return factory->NewFromASCII(IntToString(intVal));
    }

    double d = number.GetDouble();
    if (std::isnan(d)) {
        return JSHandle<EcmaString>::Cast(thread->GlobalConstants()->GetHandledNanCapitalString());
    }
    if (d == 0.0) {
        return JSHandle<EcmaString>::Cast(thread->GlobalConstants()->GetHandledZeroString());
    }
    if (d >= INT32_MIN + 1 && d <= INT32_MAX && d == static_cast<double>(static_cast<int32_t>(d))) {
        return factory->NewFromASCII(IntToString(static_cast<int32_t>(d)));
    }

    std::string result;
    if (d < 0) {
        result += "-";
        d = -d;
    }

    if (std::isinf(d)) {
        result += "Infinity";
        return factory->NewFromASCII(result.c_str());
    }

    ASSERT(d > 0);

    // 5. Otherwise, let n, k, and s be integers such that k ≥ 1, 10k−1 ≤ s < 10k, the Number value for s × 10n−k is m,
    // and k is as small as possible. If there are multiple possibilities for s, choose the value of s for which s ×
    // 10n−k is closest in value to m. If there are two such possible values of s, choose the one that is even. Note
    // that k is the number of digits in the decimal representation of s and that s is not divisible by 10.
    if (0.1 <= d && d < 1) {  // 0.1: 10 ** -1
        // Fast path. In this case, n==0, just need to calculate k and s.
        std::string resultFast = "0.";
        int64_t sFast = 0;
        int kFast = 1;
        int64_t power = 1;
        while (kFast <= DOUBLE_MAX_PRECISION) {
            power *= 10;  // 10: base 10
            int digitFast = static_cast<int64_t>(d * power) % 10;  // 10: base 10
            ASSERT(0 <= digitFast && digitFast <= 9);  // 9: single digit max
            sFast = sFast * 10 + digitFast;  // 10: base 10
            resultFast += (digitFast + '0');
            if (sFast / static_cast<double>(power) == d) {  // s * (10 ** -k)
                result += resultFast;
                return factory->NewFromASCII(result.c_str());
            }
            kFast++;
        }
    }
    char buffer[JS_DTOA_BUF_SIZE] = {0};
    int n = 0;
    int k = GetMinmumDigits(d, &n, buffer);
    std::string base = buffer;
    if (n > 0 && n <= 21) {  // NOLINT(readability-magic-numbers)
        base.erase(1, 1);
        if (k <= n) {
            // 6. If k ≤ n ≤ 21, return the String consisting of the code units of the k digits of the decimal
            // representation of s (in order, with no leading zeroes), followed by n−k occurrences of the code unit
            // 0x0030 (DIGIT ZERO).
            base += std::string(n - k, '0');
        } else {
            // 7. If 0 < n ≤ 21, return the String consisting of the code units of the most significant n digits of the
            // decimal representation of s, followed by the code unit 0x002E (FULL STOP), followed by the code units of
            // the remaining k−n digits of the decimal representation of s.
            base.insert(n, 1, '.');
        }
    } else if (-6 < n && n <= 0) {  // NOLINT(readability-magic-numbers)
        // 8. If −6 < n ≤ 0, return the String consisting of the code unit 0x0030 (DIGIT ZERO), followed by the code
        // unit 0x002E (FULL STOP), followed by −n occurrences of the code unit 0x0030 (DIGIT ZERO), followed by the
        // code units of the k digits of the decimal representation of s.
        base.erase(1, 1);
        base = std::string("0.") + std::string(-n, '0') + base;
    } else {
        if (k == 1) {
            // 9. Otherwise, if k = 1, return the String consisting of the code unit of the single digit of s
            base.erase(1, 1);
        }
        // followed by code unit 0x0065 (LATIN SMALL LETTER E), followed by the code unit 0x002B (PLUS SIGN) or the code
        // unit 0x002D (HYPHEN-MINUS) according to whether n−1 is positive or negative, followed by the code units of
        // the decimal representation of the integer abs(n−1) (with no leading zeroes).
        base += "e" + (n >= 1 ? std::string("+") : "") + std::to_string(n - 1);
    }
    result += base;
    return factory->NewFromASCII(result.c_str());
}

double NumberHelper::TruncateDouble(double d)
{
    if (std::isnan(d)) {
        return 0;
    }
    if (!std::isfinite(d)) {
        return d;
    }
    // -0 to +0
    if (d == 0.0) {
        return 0;
    }
    double ret = (d >= 0) ? std::floor(d) : std::ceil(d);
    if (ret == 0.0) {
        ret = 0;
    }
    return ret;
}

int64_t NumberHelper::DoubleToInt64(double d)
{
    if (d >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
        return std::numeric_limits<int64_t>::max();
    }
    if (d <= static_cast<double>(std::numeric_limits<int64_t>::min())) {
        return std::numeric_limits<int64_t>::min();
    }
    return static_cast<int64_t>(d);
}

bool NumberHelper::IsDigitalString(const uint8_t *start, const uint8_t *end)
{
    int len = end - start;
    for (int i = 0; i < len; i++) {
        if (*(start + i) < '0' || *(start + i) > '9') {
            return false;
        }
    }
    return true;
}

int NumberHelper::StringToInt(const uint8_t *start, const uint8_t *end)
{
    int num = *start - '0';
    for (int i = 1; i < (end - start); i++) {
        num = 10 * num + (*(start + i) - '0'); // 10 : 10 represents the base of the decimal system
    }
    return num;
}

// only for string is ordinary string and using UTF8 encoding
// Fast path for short integer and some special value
std::pair<bool, JSTaggedNumber> NumberHelper::FastStringToNumber(const uint8_t *start,
    const uint8_t *end, JSTaggedValue string)
{
    ASSERT(start < end);
    EcmaStringAccessor strAccessor(string);
    bool minus = (start[0] == '-');
    int pos = (minus ? 1 : 0);

    if (pos == (end - start)) {
        return {true, JSTaggedNumber(NAN_VALUE)};
    } else if (*(start + pos) > '9') {
        // valid number's codes not longer than '9', except 'I' and non-breaking space.
        if (*(start + pos) != 'I' && *(start + pos) != 0xA0) {
            return {true, JSTaggedNumber(NAN_VALUE)};
        }
    } else if ((end - (start + pos)) <= MAX_ELEMENT_INDEX_LEN && IsDigitalString((start + pos), end)) {
        int num = StringToInt((start + pos), end);
        if (minus) {
            if (num == 0) {
                return {true, JSTaggedNumber(SignedZero(Sign::NEG))};
            }
            num = -num;
        } else {
            if (num != 0 || (num == 0 && (end - start == 1))) {
                strAccessor.TryToSetIntegerHash(num);
            }
        }
        return {true, JSTaggedNumber(num)};
    }

    return {false, JSTaggedNumber(NAN_VALUE)};
}

double NumberHelper::StringToDouble(const uint8_t *start, const uint8_t *end, uint8_t radix, uint32_t flags)
{
    auto p = const_cast<uint8_t *>(start);
    // 1. skip space and line terminal
    if (!NumberHelper::GotoNonspace(&p, end)) {
        return 0.0;
    }

    // 2. get number sign
    Sign sign = Sign::NONE;
    if (*p == '+') {
        RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
        sign = Sign::POS;
    } else if (*p == '-') {
        RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
        sign = Sign::NEG;
    }
    bool ignoreTrailing = (flags & IGNORE_TRAILING) != 0;

    // 3. judge Infinity
    static const char INF[] = "Infinity";  // NOLINT(modernize-avoid-c-arrays)
    if (*p == INF[0]) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        for (const char *i = &INF[1]; *i != '\0'; ++i) {
            if (++p == end || *p != *i) {
                return NAN_VALUE;
            }
        }
        ++p;
        if (!ignoreTrailing && NumberHelper::GotoNonspace(&p, end)) {
            return NAN_VALUE;
        }
        return sign == Sign::NEG ? -POSITIVE_INFINITY : POSITIVE_INFINITY;
    }

    // 4. get number radix
    bool leadingZero = false;
    bool prefixRadix = false;
    if (*p == '0' && radix == 0) {
        RETURN_IF_CONVERSION_END(++p, end, SignedZero(sign));
        if (*p == 'x' || *p == 'X') {
            if ((flags & ALLOW_HEX) == 0) {
                return ignoreTrailing ? SignedZero(sign) : NAN_VALUE;
            }
            RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
            if (sign != Sign::NONE) {
                return NAN_VALUE;
            }
            prefixRadix = true;
            radix = HEXADECIMAL;
        } else if (*p == 'o' || *p == 'O') {
            if ((flags & ALLOW_OCTAL) == 0) {
                return ignoreTrailing ? SignedZero(sign) : NAN_VALUE;
            }
            RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
            if (sign != Sign::NONE) {
                return NAN_VALUE;
            }
            prefixRadix = true;
            radix = OCTAL;
        } else if (*p == 'b' || *p == 'B') {
            if ((flags & ALLOW_BINARY) == 0) {
                return ignoreTrailing ? SignedZero(sign) : NAN_VALUE;
            }
            RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
            if (sign != Sign::NONE) {
                return NAN_VALUE;
            }
            prefixRadix = true;
            radix = BINARY;
        } else {
            leadingZero = true;
        }
    }

    if (radix == 0) {
        radix = DECIMAL;
    }
    auto pStart = p;
    // 5. skip leading '0'
    while (*p == '0') {
        RETURN_IF_CONVERSION_END(++p, end, SignedZero(sign));
        leadingZero = true;
    }
    // 6. parse to number
    uint64_t intNumber = 0;
    uint64_t numberMax = (UINT64_MAX - (radix - 1)) / radix;
    int digits = 0;
    int exponent = 0;
    do {
        uint8_t c = ToDigit(*p);
        if (c >= radix) {
            if (!prefixRadix || ignoreTrailing || (pStart != p && !NumberHelper::GotoNonspace(&p, end))) {
                break;
            }
            // "0b" "0x1.2" "0b1e2" ...
            return NAN_VALUE;
        }
        ++digits;
        if (intNumber < numberMax) {
            intNumber = intNumber * radix + c;
        } else {
            ++exponent;
        }
    } while (++p != end);

    auto number = static_cast<double>(intNumber);
    if (sign == Sign::NEG) {
        if (number == 0) {
            number = -0.0;
        } else {
            number = -number;
        }
    }

    // 7. deal with other radix except DECIMAL
    if (p == end || radix != DECIMAL) {
        if ((digits == 0 && !leadingZero) || (p != end && !ignoreTrailing && NumberHelper::GotoNonspace(&p, end))) {
            // no digits there, like "0x", "0xh", or error trailing of "0x3q"
            return NAN_VALUE;
        }
        return number * std::pow(radix, exponent);
    }

    // 8. parse '.'
    exponent = 0;
    if (radix == DECIMAL && *p == '.') {
        RETURN_IF_CONVERSION_END(++p, end, (digits > 0 || (digits == 0 && leadingZero)) ?
                                           (number * std::pow(radix, exponent)) : NAN_VALUE);
        while (ToDigit(*p) < radix) {
            --exponent;
            ++digits;
            if (++p == end) {
                break;
            }
        }
    }
    if (digits == 0 && !leadingZero) {
        // no digits there, like ".", "sss", or ".e1"
        return NAN_VALUE;
    }
    auto pEnd = p;

    // 9. parse 'e/E' with '+/-'
    char exponentSign = '+';
    int additionalExponent = 0;
    constexpr int MAX_EXPONENT = INT32_MAX / 2;
    if (radix == DECIMAL && (p != end && (*p == 'e' || *p == 'E'))) {
        RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);

        // 10. parse exponent number
        if (*p == '+' || *p == '-') {
            exponentSign = static_cast<char>(*p);
            RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
        }
        uint8_t digit;
        while ((digit = ToDigit(*p)) < radix) {
            if (additionalExponent > static_cast<int>(MAX_EXPONENT / radix)) {
                additionalExponent = MAX_EXPONENT;
            } else {
                additionalExponent = additionalExponent * static_cast<int>(radix) + static_cast<int>(digit);
            }
            if (++p == end) {
                break;
            }
        }
    }
    exponent += (exponentSign == '-' ? -additionalExponent : additionalExponent);
    if (!ignoreTrailing && NumberHelper::GotoNonspace(&p, end)) {
        return NAN_VALUE;
    }

    // 10. build StringNumericLiteral string
    CString buffer;
    if (sign == Sign::NEG) {
        buffer += "-";
    }
    for (uint8_t *i = pStart; i < pEnd; ++i) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (*i != static_cast<uint8_t>('.')) {
            buffer += *i;
        }
    }

    // 11. convert none-prefix radix string
    return Strtod(buffer.c_str(), exponent, radix);
}

double NumberHelper::Strtod(const char *str, int exponent, uint8_t radix)
{
    ASSERT(str != nullptr);
    ASSERT(radix >= base::MIN_RADIX && radix <= base::MAX_RADIX);
    auto p = const_cast<char *>(str);
    Sign sign = Sign::NONE;
    uint64_t number = 0;
    uint64_t numberMax = (UINT64_MAX - (radix - 1)) / radix;
    double result = 0.0;
    if (*p == '-') {
        sign = Sign::NEG;
        ++p;
    }
    while (*p == '0') {
        ++p;
    }
    while (*p != '\0') {
        uint8_t digit = ToDigit(static_cast<uint8_t>(*p));
        if (digit >= radix) {
            break;
        }
        if (number < numberMax) {
            number = number * radix + digit;
        } else {
            ++exponent;
        }
        ++p;
    }

    // cal pow
    int exponentAbs = exponent < 0 ? -exponent : exponent;
    double powVal = ((radix == DECIMAL) && (exponentAbs < POWERS_OF_TEN_SIZE)) ?
        POWERS_OF_TEN[exponentAbs] : std::pow(radix, exponentAbs);
    if (exponent < 0) {
        result = number / powVal;
    } else {
        result = number * powVal;
    }
    return sign == Sign::NEG ? -result : result;
}

int32_t NumberHelper::DoubleToInt(double d, size_t bits)
{
    int32_t ret = 0;
    auto u64 = bit_cast<uint64_t>(d);
    int exp = static_cast<int>((u64 & DOUBLE_EXPONENT_MASK) >> DOUBLE_SIGNIFICAND_SIZE) - DOUBLE_EXPONENT_BIAS;
    if (exp < static_cast<int>(bits - 1)) {
        // smaller than INT<bits>_MAX, fast conversion
        ret = static_cast<int32_t>(d);
    } else if (exp < static_cast<int>(bits + DOUBLE_SIGNIFICAND_SIZE)) {
        // Still has significand bits after mod 2^<bits>
        // Get low <bits> bits by shift left <64 - bits> and shift right <64 - bits>
        uint64_t value = (((u64 & DOUBLE_SIGNIFICAND_MASK) | DOUBLE_HIDDEN_BIT)
                          << (static_cast<uint32_t>(exp) - DOUBLE_SIGNIFICAND_SIZE + INT64_BITS - bits)) >>
                         (INT64_BITS - bits);
        ret = static_cast<int32_t>(value);
        if ((u64 & DOUBLE_SIGN_MASK) == DOUBLE_SIGN_MASK && ret != INT32_MIN) {
            ret = -ret;
        }
    } else {
        // No significand bits after mod 2^<bits>, contains NaN and INF
        ret = 0;
    }
    return ret;
}

int32_t NumberHelper::DoubleInRangeInt32(double d)
{
    if (d > INT_MAX) {
        return INT_MAX;
    }
    if (d < INT_MIN) {
        return INT_MIN;
    }
    return base::NumberHelper::DoubleToInt(d, base::INT32_BITS);
}

JSTaggedValue NumberHelper::StringToBigInt(JSThread *thread, JSHandle<JSTaggedValue> strVal)
{
    auto strObj = static_cast<EcmaString *>(strVal->GetTaggedObject());
    uint32_t strLen = EcmaStringAccessor(strObj).GetLength();
    if (strLen == 0) {
        return BigInt::Int32ToBigInt(thread, 0).GetTaggedValue();
    }
    CVector<uint8_t> buf;
    Span<const uint8_t> str = EcmaStringAccessor(strObj).ToUtf8Span(buf);

    auto p = const_cast<uint8_t *>(str.begin());
    auto end = str.end();
    // 1. skip space and line terminal
    if (!NumberHelper::GotoNonspace(&p, end)) {
        return BigInt::Int32ToBigInt(thread, 0).GetTaggedValue();
    }
    // 2. get bigint sign
    Sign sign = Sign::NONE;
    if (*p == '+') {
        RETURN_IF_CONVERSION_END(++p, end, JSTaggedValue(NAN_VALUE));
        sign = Sign::POS;
    } else if (*p == '-') {
        RETURN_IF_CONVERSION_END(++p, end, JSTaggedValue(NAN_VALUE));
        sign = Sign::NEG;
    }
    // 3. bigint not allow Infinity, decimal points, or exponents.
    if (isalpha(*p)) {
        return JSTaggedValue(NAN_VALUE);
    }
    // 4. get bigint radix
    uint8_t radix = DECIMAL;
    if (*p == '0') {
        if (++p == end) {
            return BigInt::Int32ToBigInt(thread, 0).GetTaggedValue();
        }
        if (*p == 'x' || *p == 'X') {
            RETURN_IF_CONVERSION_END(++p, end, JSTaggedValue(NAN_VALUE));
            if (sign != Sign::NONE) {
                return JSTaggedValue(NAN_VALUE);
            }
            radix = HEXADECIMAL;
        } else if (*p == 'o' || *p == 'O') {
            RETURN_IF_CONVERSION_END(++p, end, JSTaggedValue(NAN_VALUE));
            if (sign != Sign::NONE) {
                return JSTaggedValue(NAN_VALUE);
            }
            radix = OCTAL;
        } else if (*p == 'b' || *p == 'B') {
            RETURN_IF_CONVERSION_END(++p, end, JSTaggedValue(NAN_VALUE));
            if (sign != Sign::NONE) {
                return JSTaggedValue(NAN_VALUE);
            }
            radix = BINARY;
        }
    }

    // 5. skip leading '0'
    while (*p == '0') {
        if (++p == end) {
            return BigInt::Int32ToBigInt(thread, 0).GetTaggedValue();
        }
    }
    // 6. parse to bigint
    CString buffer;
    do {
        uint8_t c = ToDigit(*p);
        if (c < radix) {
            buffer += *p;
        } else if (NumberHelper::GotoNonspace(&p, end)) {
            // illegal character
            return JSTaggedValue(NAN_VALUE);
        }
        // tail of string is space
    } while (++p < end);
    if (buffer.size() == 0) {
        return BigInt::Uint32ToBigInt(thread, 0).GetTaggedValue();
    }
    if (sign == Sign::NEG) {
        return BigIntHelper::SetBigInt(thread, "-" + buffer, radix).GetTaggedValue();
    }
    return BigIntHelper::SetBigInt(thread, buffer, radix).GetTaggedValue();
}

void NumberHelper::GetBase(double d, int digits, int *decpt, char *buf, char *bufTmp, int size)
{
    int result = snprintf_s(bufTmp, size, size - 1, "%+.*e", digits - 1, d);
    if (result == -1) {
        LOG_FULL(FATAL) << "snprintf_s failed";
        UNREACHABLE();
    }
    // mantissa
    buf[0] = bufTmp[1];
    if (digits > 1) {
        if (memcpy_s(buf + 1, digits, bufTmp + 2, digits) != EOK) { // 2 means add the point char to buf
            LOG_FULL(FATAL) << "memcpy_s failed";
            UNREACHABLE();
        }
    }
    buf[digits + 1] = '\0';
    // exponent
    *decpt = atoi(bufTmp + digits + 2 + (digits > 1)) + 1; // 2 means ignore the integer and point
}

int NumberHelper::GetMinmumDigits(double d, int *decpt, char *buf)
{
    int digits = 0;
    char bufTmp[JS_DTOA_BUF_SIZE] = {0};

    // find the minimum amount of digits
    int MinDigits = 1;
    int MaxDigits = DOUBLE_MAX_PRECISION;
    while (MinDigits < MaxDigits) {
        digits = (MinDigits + MaxDigits) / 2;
        GetBase(d, digits, decpt, buf, bufTmp, sizeof(bufTmp));
        if (strtod(bufTmp, NULL) == d) {
            // no need to keep the trailing zeros
            while (digits >= 2 && buf[digits] == '0') { // 2 means ignore the integer and point
                digits--;
            }
            MaxDigits = digits;
        } else {
            MinDigits = digits + 1;
        }
    }
    digits = MaxDigits;
    GetBase(d, digits, decpt, buf, bufTmp, sizeof(bufTmp));

    return digits;
}

uint64_t RandomGenerator::XorShift64(uint64_t *pVal)
{
    uint64_t x = *pVal;
    x ^= x >> RIGHT12;
    x ^= x << LEFT25;
    x ^= x >> RIGHT27;
    *pVal = x;
    return x * GET_MULTIPLY;
}

void RandomGenerator::InitRandom()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    randomState_ = static_cast<uint64_t>((tv.tv_sec * SECONDS_TO_SUBTLE) + tv.tv_usec);
    // the state must be non zero
    if (randomState_ == 0) {
        randomState_ = 1;
    }
}

double RandomGenerator::NextDouble()
{
    uint64_t val = XorShift64(&randomState_);
    return ToDouble(val);
}

double RandomGenerator::ToDouble(uint64_t state)
{
    uint64_t random = (state >> base::RIGHT12) | EXPONENTBITS_RANGE_IN_ONE_AND_TWO;
    return base::bit_cast<double>(random) - 1;
}

int32_t RandomGenerator::Next(int bits)
{
    uint64_t val = XorShift64(&randomState_);
    return static_cast<int32_t>(val >> (INT64_BITS - bits));
}

int32_t RandomGenerator::GenerateIdentityHash()
{
    return RandomGenerator::Next(INT32_BITS) & INT32_MAX;
}
}  // namespace panda::ecmascript::base
