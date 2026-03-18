#include "test_harness.h"
#include "../card_stack.h"

void test_init_empty(void)
{
    CardStack cs;
    card_stack_init(&cs);
    ASSERT_EQ(card_stack_top(&cs), CARD_NONE, "empty stack returns CARD_NONE");
    ASSERT_EQ(card_stack_has(&cs, CARD_TOAST), 0, "no toast");
    ASSERT_EQ(card_stack_has(&cs, CARD_PERMISSION), 0, "no permission");
}

void test_push_and_top(void)
{
    CardStack cs;
    card_stack_init(&cs);
    card_stack_push(&cs, CARD_TOAST);
    ASSERT_EQ(card_stack_top(&cs), CARD_TOAST, "toast is top");
    card_stack_push(&cs, CARD_PERMISSION);
    ASSERT_EQ(card_stack_top(&cs), CARD_PERMISSION, "permission beats toast");
    card_stack_push(&cs, CARD_SESSION_SWITCHER);
    ASSERT_EQ(card_stack_top(&cs), CARD_SESSION_SWITCHER, "switcher beats permission");
}

void test_pop(void)
{
    CardStack cs;
    card_stack_init(&cs);
    card_stack_push(&cs, CARD_TOAST);
    card_stack_push(&cs, CARD_PERMISSION);
    card_stack_pop(&cs, CARD_PERMISSION);
    ASSERT_EQ(card_stack_top(&cs), CARD_TOAST, "after pop permission, toast is top");
    ASSERT_EQ(card_stack_has(&cs, CARD_PERMISSION), 0, "permission removed");
}

void test_pop_middle(void)
{
    CardStack cs;
    card_stack_init(&cs);
    card_stack_push(&cs, CARD_TOAST);
    card_stack_push(&cs, CARD_PERMISSION);
    card_stack_push(&cs, CARD_SESSION_SWITCHER);
    card_stack_pop(&cs, CARD_PERMISSION);
    ASSERT_EQ(card_stack_top(&cs), CARD_SESSION_SWITCHER, "switcher still top");
    ASSERT_EQ(card_stack_has(&cs, CARD_TOAST), 1, "toast still present");
}

void test_push_invalid(void)
{
    CardStack cs;
    card_stack_init(&cs);
    card_stack_push(&cs, CARD_NONE);
    ASSERT_EQ(card_stack_top(&cs), CARD_NONE, "pushing NONE does nothing");
}

void test_has(void)
{
    CardStack cs;
    card_stack_init(&cs);
    card_stack_push(&cs, CARD_TOAST);
    ASSERT_EQ(card_stack_has(&cs, CARD_TOAST), 1, "has toast");
    ASSERT_EQ(card_stack_has(&cs, CARD_PERMISSION), 0, "no permission");
    ASSERT_EQ(card_stack_has(&cs, CARD_NONE), 0, "has(NONE) returns 0");
}

void test_double_push(void)
{
    CardStack cs;
    card_stack_init(&cs);
    card_stack_push(&cs, CARD_TOAST);
    card_stack_push(&cs, CARD_TOAST);
    ASSERT_EQ(card_stack_has(&cs, CARD_TOAST), 1, "still has toast");
    card_stack_pop(&cs, CARD_TOAST);
    ASSERT_EQ(card_stack_has(&cs, CARD_TOAST), 0, "single pop removes it");
}

int main(void)
{
    printf("card_stack tests:\n");
    RUN_TEST(test_init_empty);
    RUN_TEST(test_push_and_top);
    RUN_TEST(test_pop);
    RUN_TEST(test_pop_middle);
    RUN_TEST(test_push_invalid);
    RUN_TEST(test_has);
    RUN_TEST(test_double_push);
    TEST_REPORT();
}
