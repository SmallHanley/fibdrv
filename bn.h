#ifndef BN_H
#define BN_H

#include <linux/types.h>

#define BN_LENGTH 11

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

static void bn_sub(bn_t *a, bn_t *b, bn_t *res)
{
    unsigned c = 0;
    for (int i = 0; i < BN_LENGTH; i++) {
        unsigned c1 =
            __builtin_usubl_overflow(a->val[i], b->val[i], &res->val[i]);
        unsigned c2 = __builtin_usubl_overflow(res->val[i], c, &res->val[i]);
        c = c1 | c2;
    }
}

static void bn_mul(bn_t a, bn_t b, bn_t *res)
{
    bn_init(res);
    for (int i = 0; i < 64 * BN_LENGTH; i++) {
        if (b.val[0] & 1) {
            bn_add(res, &a, res);
        }
        bn_sll(&a, &a, 1);
        bn_srl(&b, &b, 1);
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

#endif  // BN_H
