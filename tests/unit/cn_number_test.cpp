// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/unit/cn_number_test.cpp --- 中文数字/大写金额 纯函数单测（M4-7）
//
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §5

#include <cstdio>
#include <cstdlib>
#include <string>

#include "cn_number.hpp"

namespace {

int g_failures = 0;

void CheckEq(const std::string& actual, const std::string& expect, const char* what) {
    if (actual != expect) {
        std::fprintf(stderr, "FAIL: %s —— 期望 \"%s\"，实际 \"%s\"\n", what, expect.c_str(),
                    actual.c_str());
        ++g_failures;
    } else {
        std::printf("OK: %s -> %s\n", what, actual.c_str());
    }
}

}  // namespace

int main() {
    using namespace myabc::shared;

    // ---- PlaceValueChinese（数量读法）--------------------------------
    CheckEq(PlaceValueChinese(0), "零", "PlaceValue(0)");
    CheckEq(PlaceValueChinese(10), "十", "PlaceValue(10)");
    CheckEq(PlaceValueChinese(100), "一百", "PlaceValue(100)");
    CheckEq(PlaceValueChinese(1005), "一千零五", "PlaceValue(1005)");
    CheckEq(PlaceValueChinese(20000), "两万", "PlaceValue(20000)");
    CheckEq(PlaceValueChinese(-5), "负五", "PlaceValue(-5) 负数");
    CheckEq(PlaceValueChinese(2025), "两千零二十五", "PlaceValue(2025) M4-3 期望之一");
    CheckEq(PlaceValueChinese(115), "一百一十五", "PlaceValue(115) 非首位不省略\"一\"");
    CheckEq(PlaceValueChinese(100015), "十万零一十五", "PlaceValue(100015) 跨组补零");

    // ---- DigitByDigitChinese（逐位/年份读法）--------------------------
    CheckEq(*DigitByDigitChinese("2025"), "二〇二五", "DigitByDigit(2025) M4-3 期望之二");
    if (DigitByDigitChinese("12a3")) {
        std::fprintf(stderr, "FAIL: DigitByDigit 应拒绝非数字字符\n");
        ++g_failures;
    }

    // ---- UppercaseCurrency（大写金额，GB/T 15835 数字用法）------------
    CheckEq(UppercaseCurrency(123456), "壹仟贰佰叁拾肆圆伍角陆分", "Currency(1234.56) M4-4");
    CheckEq(UppercaseCurrency(10000), "壹佰圆整", "Currency(100.00) M4-5，整");
    CheckEq(UppercaseCurrency(10005), "壹佰圆零伍分", "Currency(100.05)，角为零补零");
    CheckEq(UppercaseCurrency(0), "零圆整", "Currency(0.00) 边界");
    CheckEq(UppercaseCurrency(-500), "负伍圆整", "Currency(-5.00) 负数");

    // ---- ParseDecimalToCents / ParseInteger（解析 + 四舍五入到分）-----
    if (const auto v = ParseDecimalToCents("1234.56"); !v || *v != 123456) {
        std::fprintf(stderr, "FAIL: ParseDecimalToCents(1234.56)\n");
        ++g_failures;
    } else {
        std::printf("OK: ParseDecimalToCents(1234.56) -> %lld\n", static_cast<long long>(*v));
    }
    if (const auto v = ParseDecimalToCents("1.999"); !v || *v != 200) {
        std::fprintf(stderr, "FAIL: ParseDecimalToCents(1.999) 应四舍五入进位到 2.00\n");
        ++g_failures;
    } else {
        std::printf("OK: ParseDecimalToCents(1.999) -> %lld（四舍五入进位）\n",
                    static_cast<long long>(*v));
    }
    if (ParseDecimalToCents("abc") || ParseDecimalToCents("")) {
        std::fprintf(stderr, "FAIL: ParseDecimalToCents 应拒绝非法输入\n");
        ++g_failures;
    }
    if (const auto v = ParseInteger("2025"); !v || *v != 2025) {
        std::fprintf(stderr, "FAIL: ParseInteger(2025)\n");
        ++g_failures;
    }
    if (ParseInteger("1.5")) {
        std::fprintf(stderr, "FAIL: ParseInteger 不应接受带小数点的输入\n");
        ++g_failures;
    }

    if (g_failures == 0) {
        std::puts("cn_number_test: all checks passed");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "cn_number_test: %d check(s) failed\n", g_failures);
    return EXIT_FAILURE;
}
