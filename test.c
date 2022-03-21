#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BN_LENGTH 1000

typedef struct {
    uint64_t val[BN_LENGTH];
} bn_t;

static inline void bn_init(bn_t *a)
{
    memset(a->val, 0, sizeof(uint64_t) * BN_LENGTH);
}

static inline void bn_set_with_pos(bn_t *a, uint64_t num, unsigned pos)
{
    a->val[pos] = num;
}

static inline void bn_swap(bn_t *a, bn_t *b)
{
    bn_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void bn_srl(bn_t *a, bn_t *res, unsigned n)
{
    for (int i = 0; i < BN_LENGTH - 1; i++) {
        res->val[i] = a->val[i] >> n | a->val[i + 1] << (64 - n);
    }
    res->val[BN_LENGTH - 1] = a->val[BN_LENGTH - 1] >> n;
}

static void bn_sll(bn_t *a, bn_t *res, unsigned n)
{
    for (int i = BN_LENGTH - 1; i > 0; i--) {
        res->val[i] = a->val[i] << n | a->val[i - 1] >> (64 - n);
    }
    res->val[0] = a->val[0] << n;
}

static void bn_add(bn_t *a, bn_t *b, bn_t *res)
{
    unsigned c = 0;
    for (int i = 0; i < BN_LENGTH; i++) {
        unsigned c1 =
            __builtin_uaddl_overflow(a->val[i], b->val[i], &res->val[i]);
        unsigned c2 = __builtin_uaddl_overflow(res->val[i], c, &res->val[i]);
        c = c1 | c2;
    }
}

static char *bn2string(bn_t *a)
{
    char str[64 * BN_LENGTH / 3 + 2];

    memset(str, '0', sizeof(str) - 1);
    str[sizeof(str) - 1] = '\0';

    for (int i = 0; i < 64 * BN_LENGTH; i++) {
        unsigned carry = a->val[BN_LENGTH - 1] >> 63;
        for (int j = sizeof(str) - 2; j >= 0; j--) {
            str[j] += str[j] - '0' + carry;
            carry = (str[j] > '9');
            if (carry)
                str[j] -= 10;
        }
        bn_sll(a, a, 1);
    }

    // search for numbers
    int i = 0;
    while (i < sizeof(str) - 2 && str[i] == '0')
        i++;

    // passing string back
    char *p = malloc(sizeof(str) - i);
    memcpy(p, str + i, sizeof(str) - i);

    return p;
}

static bn_t fib_sequence(long long k)
{
    bn_t a, b, c;

    bn_init(&a);
    bn_init(&b);

    bn_set_with_pos(&b, 1, 0);

    for (int i = 2; i <= k; i++) {
        bn_add(&a, &b, &c);
        bn_swap(&a, &b);
        bn_swap(&b, &c);
    }

    return b;
}

int main(void)
{
    bn_t a = fib_sequence(1000);
    printf("%s\n", bn2string(&a));
    return 0;
}
