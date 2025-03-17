#include "ext_2.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int read_superblock(const char *device, struct ext2_superblock *sb) {
    int fd = open(device, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    // Superblock starts at offset 1024
    lseek(fd, 1024, SEEK_SET);
    read(fd, sb, sizeof(*sb));

    close(fd);
    return 0;
}