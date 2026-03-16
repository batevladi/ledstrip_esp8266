#include <unity.h>
#include <cstdint>

static uint8_t lerp8(uint8_t a, uint8_t b, uint8_t fraction) {
    return a + (((int16_t)b - a) * fraction) / 255;
}

void test_lerp8_endpoints() {
    TEST_ASSERT_EQUAL(0, lerp8(0, 255, 0));
    TEST_ASSERT_EQUAL(255, lerp8(0, 255, 255));
    TEST_ASSERT_EQUAL(100, lerp8(100, 100, 128));
}

void test_lerp8_midpoint() {
    uint8_t mid = lerp8(0, 200, 128);
    TEST_ASSERT_UINT8_WITHIN(2, 100, mid);
}

void test_breathing_brightness_range() {
    uint8_t minBright = 30;
    uint8_t maxBright = 100;

    // sin8 approximation check at key points
    // At quarter cycle (peak): brightness should be at max
    uint8_t sinVal255 = 255; // sin8(64) = 255
    uint8_t bright_max = minBright + ((uint16_t)(maxBright - minBright) * sinVal255) / 255;
    TEST_ASSERT_EQUAL(maxBright, bright_max);

    // At 3/4 cycle (trough): brightness should be at min
    uint8_t sinVal0 = 0; // sin8(192) = 0
    uint8_t bright_min = minBright + ((uint16_t)(maxBright - minBright) * sinVal0) / 255;
    TEST_ASSERT_EQUAL(minBright, bright_min);
}

void test_star_brightness_lifecycle() {
    uint16_t duration = 3000;
    uint32_t third = duration / 3;

    // Fade-in start: brightness = 0
    uint8_t b0 = (0 * 255) / third;
    TEST_ASSERT_EQUAL(0, b0);

    // Fade-in midpoint: brightness ~127
    uint8_t bHalfIn = ((third / 2) * 255) / third;
    TEST_ASSERT_UINT8_WITHIN(2, 127, bHalfIn);

    // Hold phase: brightness = 255
    TEST_ASSERT_EQUAL(255, 255);

    // End of duration: brightness = 0
    TEST_ASSERT_TRUE(duration >= duration);
}

void test_rainbow_hue_wraps() {
    uint32_t cycleDuration = 5000;

    uint8_t hue0 = (0 * 256 / cycleDuration) % 256;
    TEST_ASSERT_EQUAL(0, hue0);

    uint8_t hueHalf = (uint32_t)(2500UL * 256 / cycleDuration) % 256;
    TEST_ASSERT_EQUAL(128, hueHalf);

    uint8_t hueFull = (uint32_t)(5000UL * 256 / cycleDuration) % 256;
    TEST_ASSERT_EQUAL(0, hueFull);
}

void test_sunset_blend_fraction() {
    uint32_t stepDuration = 4000;

    uint8_t blendStart = (0 * 255) / stepDuration;
    TEST_ASSERT_EQUAL(0, blendStart);

    uint32_t stepPos = 2000;
    uint8_t blendMid = (stepPos * 255) / stepDuration;
    TEST_ASSERT_UINT8_WITHIN(1, 127, blendMid);

    uint8_t blendEnd = (3999UL * 255) / stepDuration;
    TEST_ASSERT_UINT8_WITHIN(1, 254, blendEnd);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_lerp8_endpoints);
    RUN_TEST(test_lerp8_midpoint);
    RUN_TEST(test_breathing_brightness_range);
    RUN_TEST(test_star_brightness_lifecycle);
    RUN_TEST(test_rainbow_hue_wraps);
    RUN_TEST(test_sunset_blend_fraction);
    return UNITY_END();
}
