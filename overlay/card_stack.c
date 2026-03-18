#include "card_stack.h"

void card_stack_init(CardStack *cs)
{
    for (int i = 0; i < 4; i++)
        cs->cards[i] = 0;
}

void card_stack_push(CardStack *cs, CardType type)
{
    if (type > CARD_NONE && type <= CARD_SESSION_SWITCHER)
        cs->cards[type] = 1;
}

void card_stack_pop(CardStack *cs, CardType type)
{
    if (type > CARD_NONE && type <= CARD_SESSION_SWITCHER)
        cs->cards[type] = 0;
}

CardType card_stack_top(CardStack *cs)
{
    for (int i = CARD_SESSION_SWITCHER; i >= 1; i--) {
        if (cs->cards[i])
            return (CardType)i;
    }
    return CARD_NONE;
}

int card_stack_has(CardStack *cs, CardType type)
{
    if (type > CARD_NONE && type <= CARD_SESSION_SWITCHER)
        return cs->cards[type];
    return 0;
}
