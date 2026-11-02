// Host unit test for common/r39_compliance.hpp.
// Build and run from the repository root:
//   g++ -std=c++17 -Wall -Wextra -Werror -I common common/test/r39_compliance_test.cpp -o /tmp/r39_test && /tmp/r39_test

#include "r39_compliance.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

static int failures = 0;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static bool near(float a, float b, float tol = 1e-3f) { return std::fabs(a - b) <= tol; }

static void test_defaults()
{
    constexpr r39::RawParams d = r39::defaults();
    static_assert(d.in_range(), "defaults must be in range");
    CHECK(d.en == 0);
    CHECK(d.a_bp == 400);
    CHECK(d.b_ckph == 200);
    const r39::Params p = d.to_params();
    CHECK(!p.enabled);
    CHECK(near(p.a, 1.04f, 1e-6f));
    CHECK(near(p.b_kph, 2.0f, 1e-6f));
}

static void test_in_range()
{
    CHECK((r39::RawParams{1, 0, 0}.in_range()));
    CHECK((r39::RawParams{1, 1000, 400}.in_range()));
    CHECK(!(r39::RawParams{2, 400, 200}.in_range()));
    CHECK(!(r39::RawParams{1, 1001, 200}.in_range()));
    CHECK(!(r39::RawParams{1, 400, 401}.in_range()));
    CHECK(!(r39::RawParams{1, 400, -1}.in_range()));
}

static void test_disabled_passthrough()
{
    r39::Params p = r39::defaults().to_params();
    CHECK(near(p.apply(0.0f), 0.0f));
    CHECK(near(p.apply(42.5f), 42.5f));
    CHECK(near(p.apply(500.0f), r39::SPEED_MAX_KPH));
    CHECK(near(p.apply(-3.0f), 0.0f));
    CHECK(near(p.apply(std::numeric_limits<float>::quiet_NaN()), 0.0f));
    CHECK(near(p.apply(std::numeric_limits<float>::infinity()), 0.0f));
}

static void test_enabled_curve()
{
    r39::Params p = r39::RawParams{1, 400, 200}.to_params(10.0f);
    CHECK(near(p.apply(0.0f), 0.0f));
    CHECK(near(p.apply(5.0f), 6.2f));
    CHECK(near(p.apply(10.0f), 12.4f));
    CHECK(near(p.apply(40.0f), 43.6f));
    CHECK(near(p.apply(120.0f), 126.8f));
    CHECK(near(p.apply(399.0f), r39::SPEED_MAX_KPH));
    CHECK(near(p.apply(std::numeric_limits<float>::quiet_NaN()), 0.0f));
}

// Monotonic, never under-reads, never over-reads beyond 10 % + 4 km/h, for every allowed A and B.
static void test_envelope()
{
    for (int a_bp = 0; a_bp <= r39::A_BP_MAX; a_bp += 50)
    {
        for (int b = r39::B_CKPH_MIN; b <= r39::B_CKPH_MAX; b += 25)
        {
            const r39::Params p = r39::RawParams{1, (uint16_t)a_bp, (int16_t)b}.to_params(10.0f);
            float prev = 0.0f;
            for (int i = 0; i <= 36000; i++)
            {
                const float v = i * 0.01f;
                const float out = p.apply(v);
                if (out + 1e-4f < prev || out + 1e-4f < v || out - v > 0.1f * v + 4.0f + 1e-3f)
                {
                    std::printf("FAIL envelope a_bp=%d b_ckph=%d v=%.2f out=%.4f prev=%.4f\n", a_bp, b, v, out, prev);
                    failures++;
                    return;
                }
                prev = out;
            }
        }
    }
}

static void test_hundredths_from_user()
{
    int32_t h = -1;
    CHECK(r39::hundredths_from_user(4.0, 1000, &h) && h == 400);
    CHECK(r39::hundredths_from_user(4.25, 1000, &h) && h == 425);
    CHECK(r39::hundredths_from_user(0.0, 1000, &h) && h == 0);
    CHECK(r39::hundredths_from_user(10.0, 1000, &h) && h == 1000);
    CHECK(r39::hundredths_from_user(0.07, 400, &h) && h == 7);
    CHECK(!r39::hundredths_from_user(10.01, 1000, &h));
    CHECK(!r39::hundredths_from_user(-0.01, 1000, &h));
    CHECK(!r39::hundredths_from_user(4.005, 1000, &h));
    CHECK(!r39::hundredths_from_user(std::numeric_limits<double>::quiet_NaN(), 1000, &h));
    CHECK(!r39::hundredths_from_user(std::numeric_limits<double>::infinity(), 1000, &h));
}

int main()
{
    test_defaults();
    test_in_range();
    test_disabled_passthrough();
    test_enabled_curve();
    test_envelope();
    test_hundredths_from_user();
    if (failures)
    {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("r39_compliance: all tests passed\n");
    return 0;
}
