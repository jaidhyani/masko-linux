#include "test_harness.h"
#include "../session_switcher.h"

void test_init(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    ASSERT_EQ(sw.visible, 0, "starts hidden");
    ASSERT_EQ(sw.count, 0, "no sessions");
}

void test_show_from_json(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);

    cJSON *arr = cJSON_CreateArray();
    cJSON *s1 = cJSON_CreateObject();
    cJSON_AddStringToObject(s1, "id", "sess-1");
    cJSON_AddStringToObject(s1, "cwd", "/home/user/project-a");
    cJSON_AddStringToObject(s1, "model", "opus");
    cJSON_AddStringToObject(s1, "phase", "working");
    cJSON_AddItemToArray(arr, s1);

    cJSON *s2 = cJSON_CreateObject();
    cJSON_AddStringToObject(s2, "id", "sess-2");
    cJSON_AddStringToObject(s2, "cwd", "/home/user/project-b");
    cJSON_AddStringToObject(s2, "model", "sonnet");
    cJSON_AddStringToObject(s2, "phase", "idle");
    cJSON_AddItemToArray(arr, s2);

    session_switcher_show(&sw, arr, 0);
    ASSERT_EQ(sw.visible, 1, "visible after show");
    ASSERT_EQ(sw.count, 2, "2 sessions parsed");
    ASSERT_EQ(sw.selected, 0, "initial selection");
    ASSERT_STR_EQ(sw.sessions[0].id, "sess-1", "first session id");
    ASSERT_STR_EQ(sw.sessions[1].model, "sonnet", "second session model");
    cJSON_Delete(arr);
}

static cJSON *make_session_array(int n)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *s = cJSON_CreateObject();
        char id[16];
        snprintf(id, sizeof(id), "s%d", i);
        cJSON_AddStringToObject(s, "id", id);
        cJSON_AddStringToObject(s, "cwd", "/tmp");
        cJSON_AddStringToObject(s, "model", "opus");
        cJSON_AddStringToObject(s, "phase", "working");
        cJSON_AddItemToArray(arr, s);
    }
    return arr;
}

void test_nav_next(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    cJSON *arr = make_session_array(3);
    session_switcher_show(&sw, arr, 0);

    session_switcher_nav(&sw, "next");
    ASSERT_EQ(sw.selected, 1, "next goes to 1");
    session_switcher_nav(&sw, "next");
    ASSERT_EQ(sw.selected, 2, "next goes to 2");
    session_switcher_nav(&sw, "next");
    ASSERT_EQ(sw.selected, 0, "next wraps to 0");
    cJSON_Delete(arr);
}

void test_nav_prev(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    cJSON *arr = make_session_array(3);
    session_switcher_show(&sw, arr, 0);

    session_switcher_nav(&sw, "prev");
    ASSERT_EQ(sw.selected, 2, "prev wraps to last");
    session_switcher_nav(&sw, "prev");
    ASSERT_EQ(sw.selected, 1, "prev goes to 1");
    cJSON_Delete(arr);
}

void test_hide(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    cJSON *arr = make_session_array(1);
    session_switcher_show(&sw, arr, 0);
    session_switcher_hide(&sw);
    ASSERT_EQ(sw.visible, 0, "hidden");
    cJSON_Delete(arr);
}

void test_hit_test(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    cJSON *arr = make_session_array(3);
    session_switcher_show(&sw, arr, 0);

    /* Panel geometry for 400x400 window with 3 rows:
       pw = 400 - 32 = 368, ph = 32 + 3*36 = 140
       px = 16, py = (400 - 140) / 2 = 130
       Row area: row_x = 32, first row_y = 130 + 16 = 146 */
    int result = session_switcher_hit_test(&sw, 100, 155, 400, 400);
    ASSERT_EQ(result, 0, "hit first row");

    result = session_switcher_hit_test(&sw, 100, 192, 400, 400);
    ASSERT_EQ(result, 1, "hit second row");

    ASSERT_EQ(session_switcher_hit_test(&sw, 0, 0, 400, 400), -1, "miss outside panel");
    cJSON_Delete(arr);
}

void test_nav_when_hidden(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    session_switcher_nav(&sw, "next");
    ASSERT_EQ(sw.selected, 0, "nav when hidden is no-op");
}

void test_show_empty_array(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    cJSON *arr = cJSON_CreateArray();
    session_switcher_show(&sw, arr, 0);
    ASSERT_EQ(sw.visible, 1, "visible even with empty array");
    ASSERT_EQ(sw.count, 0, "zero sessions");
    cJSON_Delete(arr);
}

void test_show_null(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    session_switcher_show(&sw, NULL, 0);
    ASSERT_EQ(sw.visible, 1, "visible");
    ASSERT_EQ(sw.count, 0, "zero sessions from null");
}

void test_nav_on_empty(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    cJSON *arr = cJSON_CreateArray();
    session_switcher_show(&sw, arr, 0);
    session_switcher_nav(&sw, "next");
    ASSERT_EQ(sw.selected, 0, "nav on empty list is no-op");
    cJSON_Delete(arr);
}

void test_initial_selected_clamped(void)
{
    SessionSwitcherUI sw;
    session_switcher_init(&sw);
    cJSON *arr = make_session_array(2);
    session_switcher_show(&sw, arr, 10);
    ASSERT_EQ(sw.selected, 1, "initial_selected clamped to count-1");
    cJSON_Delete(arr);
}

int main(void)
{
    printf("session_switcher tests:\n");
    RUN_TEST(test_init);
    RUN_TEST(test_show_from_json);
    RUN_TEST(test_nav_next);
    RUN_TEST(test_nav_prev);
    RUN_TEST(test_hide);
    RUN_TEST(test_hit_test);
    RUN_TEST(test_nav_when_hidden);
    RUN_TEST(test_show_empty_array);
    RUN_TEST(test_show_null);
    RUN_TEST(test_nav_on_empty);
    RUN_TEST(test_initial_selected_clamped);
    TEST_REPORT();
}
