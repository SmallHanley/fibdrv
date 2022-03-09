#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[1];
    // char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    FILE *fp = fopen("time/time", "w");
    if (!fp) {
        perror("Failed to open time file");
        exit(1);
    }

    /*
    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }
    */

    for (int i = 0; i <= offset; i++) {
        long long sz;
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
        lseek(fd, 1, SEEK_SET);
        long long sz1 = write(fd, buf, 1);
        lseek(fd, 2, SEEK_SET);
        long long sz2 = write(fd, buf, 1);
        lseek(fd, 3, SEEK_SET);
        long long sz3 = write(fd, buf, 1);
        fprintf(fp, "%d %lld %lld %lld\n", i, sz1, sz2, sz3);
    }

    /*
    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }
    */

    close(fd);
    fclose(fp);
    return 0;
}
