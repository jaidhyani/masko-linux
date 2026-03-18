#include "test_harness.h"
#include "../permission_ui.h"

void test_init(void)
{
    PermissionUI ui;
    permission_ui_init(&ui);
    ASSERT_EQ(ui.visible, 0, "starts hidden");
    ASSERT_EQ(permission_ui_hit_test(&ui, 50, 50), 0, "no hit when hidden");
}

void test_show(void)
{
    PermissionUI ui;
    permission_ui_init(&ui);
    permission_ui_show(&ui, "Bash", "rm -rf /", "perm-1");
    ASSERT_EQ(ui.visible, 1, "visible after show");
    ASSERT_STR_EQ(ui.tool_name, "Bash", "tool name set");
    ASSERT_STR_EQ(ui.tool_input, "rm -rf /", "tool input set");
    ASSERT_STR_EQ(ui.perm_id, "perm-1", "perm id set");
}

void test_hide(void)
{
    PermissionUI ui;
    permission_ui_init(&ui);
    permission_ui_show(&ui, "Bash", "ls", "p1");
    permission_ui_hide(&ui);
    ASSERT_EQ(ui.visible, 0, "hidden after hide");
}

void test_hit_test(void)
{
    PermissionUI ui;
    permission_ui_init(&ui);
    permission_ui_show(&ui, "Bash", "ls", "p1");

    /* Simulate button positions as draw() would set them */
    ui.approve_x = 30;  ui.approve_y = 150; ui.approve_w = 80; ui.approve_h = 32;
    ui.deny_x    = 120; ui.deny_y    = 150; ui.deny_w    = 80; ui.deny_h    = 32;

    ASSERT_EQ(permission_ui_hit_test(&ui, 50, 160), 1, "hit approve button");
    ASSERT_EQ(permission_ui_hit_test(&ui, 150, 160), 2, "hit deny button");
    ASSERT_EQ(permission_ui_hit_test(&ui, 10, 10), 0, "miss both buttons");
}

void test_hover(void)
{
    PermissionUI ui;
    permission_ui_init(&ui);
    permission_ui_show(&ui, "Bash", "ls", "p1");
    ui.approve_x = 30;  ui.approve_y = 150; ui.approve_w = 80; ui.approve_h = 32;
    ui.deny_x    = 120; ui.deny_y    = 150; ui.deny_w    = 80; ui.deny_h    = 32;

    permission_ui_hover(&ui, 50, 160);
    ASSERT_EQ(ui.hover_approve, 1, "hovering approve");
    ASSERT_EQ(ui.hover_deny, 0, "not hovering deny");

    permission_ui_hover(&ui, 150, 160);
    ASSERT_EQ(ui.hover_approve, 0, "not hovering approve");
    ASSERT_EQ(ui.hover_deny, 1, "hovering deny");
}

void test_hover_when_hidden(void)
{
    PermissionUI ui;
    permission_ui_init(&ui);
    ui.approve_x = 30;  ui.approve_y = 150; ui.approve_w = 80; ui.approve_h = 32;

    permission_ui_hover(&ui, 50, 160);
    ASSERT_EQ(ui.hover_approve, 0, "hover ignored when hidden");
}

int main(void)
{
    printf("permission_ui tests:\n");
    RUN_TEST(test_init);
    RUN_TEST(test_show);
    RUN_TEST(test_hide);
    RUN_TEST(test_hit_test);
    RUN_TEST(test_hover);
    RUN_TEST(test_hover_when_hidden);
    TEST_REPORT();
}
