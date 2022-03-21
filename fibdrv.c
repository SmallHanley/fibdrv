#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>

#include "bn.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ssize_t time1, time2, time3;

static bn_t fib_sequence(long long k)
{
    bn_t a, b, c;

    bn_init(&a);
    bn_init(&b);

    if (!k) {
        return a;
    }

    bn_set_with_pos(&b, 1, 0);

    for (int i = 2; i <= k; i++) {
        bn_add(&a, &b, &c);
        bn_swap(&a, &b);
        bn_swap(&b, &c);
    }

    return b;
}

static bn_t fib_fast_exp(long long k)
{
    bn_t M[2][2], f[2];

    bn_init(&M[0][0]);
    bn_init(&M[0][1]);
    bn_init(&M[1][0]);
    bn_init(&M[1][1]);
    bn_init(&f[0]);
    bn_init(&f[1]);

    bn_set_with_pos(&M[0][0], 1, 0);
    bn_set_with_pos(&M[0][1], 1, 0);
    bn_set_with_pos(&M[1][0], 1, 0);
    bn_set_with_pos(&f[0], 1, 0);

    while (k) {
        if (k & 1) {
            bn_t t[] = {f[0], f[1]};
            bn_t t1, t2;
            bn_mul(M[0][0], t[0], &t1);
            bn_mul(M[0][1], t[1], &t2);
            bn_add(&t1, &t2, &f[0]);
            bn_mul(M[1][0], t[0], &t1);
            bn_mul(M[1][1], t[1], &t2);
            bn_add(&t1, &t2, &f[1]);
        }

        bn_t p[][2] = {{M[0][0], M[0][1]}, {M[1][0], M[1][1]}};
        bn_t t1, t2;
        bn_mul(p[0][0], p[0][0], &t1);
        bn_mul(p[0][1], p[1][0], &t2);
        bn_add(&t1, &t2, &M[0][0]);
        bn_mul(p[0][0], p[0][1], &t1);
        bn_mul(p[0][1], p[1][1], &t2);
        bn_add(&t1, &t2, &M[0][1]);
        bn_mul(p[1][0], p[0][0], &t1);
        bn_mul(p[1][1], p[1][0], &t2);
        bn_add(&t1, &t2, &M[1][0]);
        bn_mul(p[1][0], p[0][1], &t1);
        bn_mul(p[1][1], p[1][1], &t2);
        bn_add(&t1, &t2, &M[1][1]);

        k >>= 1;
    }

    return f[1];
}

bn_t fib_fast_doubling(int n)
{
    if (!n) {
        bn_t a;
        bn_init(&a);
        return a;
    }
    bn_t a, b;
    bn_init(&a);
    bn_init(&b);
    bn_set_with_pos(&b, 1, 0);
    long long m = 1 << (63 - __builtin_clz(n));
    while (m) {
        bn_t t1, t2;
        bn_t tmp1, tmp2;
        bn_sll(&b, &tmp1, 1);
        bn_sub(&tmp1, &a, &tmp1);
        bn_mul(a, tmp1, &t1);
        bn_mul(b, b, &tmp1);
        bn_mul(a, a, &tmp2);
        bn_add(&tmp1, &tmp2, &t2);
        a = t1;
        b = t2;
        if (n & m) {
            bn_add(&a, &b, &t1);
            a = b;
            b = t1;
        }
        m >>= 1;
    }
    return a;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    ktime_t kt1 = ktime_get();
    bn_t res1 = fib_sequence(*offset);
    kt1 = ktime_sub(ktime_get(), kt1);
    ktime_t kt2 = ktime_get();
    bn_t res2 = fib_fast_exp(*offset);
    kt2 = ktime_sub(ktime_get(), kt2);
    ktime_t kt3 = ktime_get();
    bn_t res3 = fib_fast_doubling(*offset);
    kt3 = ktime_sub(ktime_get(), kt3);
    time1 = ktime_to_ns(kt1);
    time2 = ktime_to_ns(kt2);
    time3 = ktime_to_ns(kt3);
    char *s1 = bn2string(&res1);
    char *s2 = bn2string(&res2);
    char *s3 = bn2string(&res3);
    if (copy_to_user(buf, s1, strlen(s1) + 1)) {
        pr_info("copy_to_user failed\n");
        return 0;
    }
    return (!strcmp(s1, s2) && !strcmp(s2, s3)) ? 1 : 0;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    switch (*offset) {
    case 1:
        return time1;
    case 2:
        return time2;
    case 3:
        return time3;
    default:
        return current->pid;
    }
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
