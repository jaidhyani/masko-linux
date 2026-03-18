#include "test_harness.h"
#include "../toast.h"

void test_init(void)
{
    ToastUI t;
    toast_init(&t);
    ASSERT_EQ(t.visible, 0, "starts hidden");
}

void test_show(void)
{
    ToastUI t;
    toast_init(&t);
    toast_show(&t, "Hello!", 3000);
    ASSERT_EQ(t.visible, 1, "visible after show");
    ASSERT_STR_EQ(t.text, "Hello!", "text set");
    ASSERT_EQ(t.duration_ms, 3000, "duration set");
    ASSERT_EQ(t.elapsed_ms, 0, "elapsed starts at 0");
}

void test_tick_still_visible(void)
{
    ToastUI t;
    toast_init(&t);
    toast_show(&t, "Test", 5000);
    int result = toast_tick(&t, 1000);
    ASSERT_EQ(result, 1, "still visible after 1s of 5s");
    ASSERT_EQ(t.elapsed_ms, 1000, "elapsed updated");
    ASSERT_EQ(t.visible, 1, "still visible");
}

void test_tick_expires(void)
{
    ToastUI t;
    toast_init(&t);
    toast_show(&t, "Test", 3000);
    toast_tick(&t, 2000);
    int result = toast_tick(&t, 1500);
    ASSERT_EQ(result, 0, "expired after exceeding duration");
    ASSERT_EQ(t.visible, 0, "hidden after expiry");
}

void test_tick_exact_duration(void)
{
    ToastUI t;
    toast_init(&t);
    toast_show(&t, "Test", 1000);
    int result = toast_tick(&t, 1000);
    ASSERT_EQ(result, 0, "expires at exact duration");
}

void test_tick_when_hidden(void)
{
    ToastUI t;
    toast_init(&t);
    int result = toast_tick(&t, 100);
    ASSERT_EQ(result, 0, "tick on hidden toast returns 0");
}

void test_hide(void)
{
    ToastUI t;
    toast_init(&t);
    toast_show(&t, "Test", 5000);
    toast_hide(&t);
    ASSERT_EQ(t.visible, 0, "hidden after hide()");
}

void test_text_truncation(void)
{
    ToastUI t;
    toast_init(&t);
    char long_text[1024];
    memset(long_text, 'A', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    toast_show(&t, long_text, 1000);
    ASSERT_EQ(t.visible, 1, "doesn't crash with long text");
    ASSERT_EQ((int)strlen(t.text), 511, "text truncated to buffer size");
}

int main(void)
{
    printf("toast tests:\n");
    RUN_TEST(test_init);
    RUN_TEST(test_show);
    RUN_TEST(test_tick_still_visible);
    RUN_TEST(test_tick_expires);
    RUN_TEST(test_tick_exact_duration);
    RUN_TEST(test_tick_when_hidden);
    RUN_TEST(test_hide);
    RUN_TEST(test_text_truncation);
    TEST_REPORT();
}
