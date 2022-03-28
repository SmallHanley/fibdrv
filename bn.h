#ifndef BN_H
#define BN_H

#include <linux/slab.h>
#include <linux/types.h>

#define BN_LENGTH 11
#define unlikely(x) __builtin_expect(!!(x), 0)

typedef struct {
    unsigned long val[BN_LENGTH];
    unsigned size;
} bn_t;

static inline void bn_init(bn_t *a)
{
    memset(a->val, 0, sizeof(a->val));
    a->size = 0;
}

static inline void bn_set_with_pos(bn_t *a, uint64_t num, unsigned pos)
{
    a->val[pos] = num;
    if (unlikely(!pos && !num && a->size == 1)) {
        a->size = 0;
    } else if (pos + 1 > a->size) {
        a->size = pos + 1;
    }
}

static inline void bn_swap(bn_t *a, bn_t *b)
{
    bn_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void bn_srl(bn_t *a, bn_t *res, unsigned n)
{
    if (!a->size) {
        bn_init(res);
        return;
    }
    if (!n) {
        *res = *a;
        return;
    }
    for (int i = 0; i < a->size - 1; i++) {
        res->val[i] = a->val[i] >> n | a->val[i + 1] << (64 - n);
    }
    res->val[a->size - 1] = a->val[a->size - 1] >> n;
    res->size = (res->val[a->size - 1]) ? a->size : a->size - 1;
}

static void bn_sll(bn_t *a, bn_t *res, unsigned n)
{
    if (!a->size) {
        bn_init(res);
        return;
    }
    if (!n) {
        *res = *a;
        return;
    }
    unsigned sz = (a->size > BN_LENGTH - 1) ? BN_LENGTH - 1 : a->size;
    for (int i = sz; i > 0; i--) {
        res->val[i] = a->val[i] << n | a->val[i - 1] >> (64 - n);
    }
    res->val[0] = a->val[0] << n;
    res->size = (res->val[sz]) ? sz + 1 : sz;
}

static void bn_add(bn_t *a, bn_t *b, bn_t *res)
{
    unsigned c = 0;
    unsigned sz = (a->size > b->size) ? a->size : b->size;
    for (int i = 0; i < sz; i++) {
        unsigned c1 =
            __builtin_uaddl_overflow(a->val[i], b->val[i], &res->val[i]);
        unsigned c2 = __builtin_uaddl_overflow(res->val[i], c, &res->val[i]);
        c = c1 | c2;
    }
    if (sz < BN_LENGTH) {
        res->val[sz] = c;
        res->size = (res->val[sz]) ? sz + 1 : sz;
    } else {
        res->size = BN_LENGTH;
    }
}

static void bn_sub(bn_t *a, bn_t *b, bn_t *res)
{
    unsigned c = 0;
    for (int i = 0; i < a->size; i++) {
        unsigned c1 =
            __builtin_usubl_overflow(a->val[i], b->val[i], &res->val[i]);
        unsigned c2 = __builtin_usubl_overflow(res->val[i], c, &res->val[i]);
        c = c1 | c2;
    }
    for (int i = a->size - 1; i >= 0; i--) {
        if (res->val[i]) {
            res->size = i + 1;
            return;
        }
    }
    res->size = 0;
}

static void bn_mul(bn_t a, bn_t b, bn_t *res)
{
    bn_init(res);
    while (b.size) {
        if (b.val[0]) {
            unsigned z = __builtin_ctzl(b.val[0]);
            bn_sll(&a, &a, z);
            bn_srl(&b, &b, z);
            bn_add(res, &a, res);
            b.val[0] &= ~(1ul);
        } else {
            bn_sll(&a, &a, 63);
            bn_srl(&b, &b, 63);
        }
    }
}

static char *bn2string(bn_t a)
{
    char str[64 * BN_LENGTH / 3 + 2];

    memset(str, '0', sizeof(str) - 1);
    str[sizeof(str) - 1] = '\0';

    unsigned sz = a.size;
    for (int i = 0; i < 64 * sz; i++) {
        unsigned carry = a.val[sz - 1] >> 63;
        for (int j = sizeof(str) - 2; j >= 0; j--) {
            str[j] += str[j] - '0' + carry;
            carry = (str[j] > '9');
            if (carry)
                str[j] -= 10;
        }
        bn_sll(&a, &a, 1);
    }

    // search for numbers
    int i = 0;
    while (i < sizeof(str) - 2 && str[i] == '0')
        i++;

    // passing string back
    char *p = kmalloc(sizeof(str) - i, GFP_KERNEL);
    memcpy(p, str + i, sizeof(str) - i);

    return p;
}

#endif  // BN_H
