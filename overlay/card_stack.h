#ifndef MASKO_CARD_STACK_H
#define MASKO_CARD_STACK_H

typedef enum {
    CARD_NONE = 0,
    CARD_TOAST = 1,
    CARD_PERMISSION = 2,
    CARD_SESSION_SWITCHER = 3,
    CARD_CONTEXT_MENU = 4,
} CardType;

typedef struct {
    int cards[5]; /* 1 if active, indexed by CardType */
} CardStack;

void     card_stack_init(CardStack *cs);
void     card_stack_push(CardStack *cs, CardType type);
void     card_stack_pop(CardStack *cs, CardType type);
CardType card_stack_top(CardStack *cs);
int      card_stack_has(CardStack *cs, CardType type);

#endif
