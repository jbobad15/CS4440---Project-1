#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

static int write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t written = write(fd, buf, len);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (written == 0) {
            errno = EIO;
            return -1;
        }

        buf += written;
        len -= (size_t)written;
    }

    return 0;
}

static int write_repeat(int fd, char ch, size_t count)
{
    char chunk[BUFFER_SIZE];
    size_t remaining = count;

    memset(chunk, ch, sizeof(chunk));
    while (remaining > 0) {
        size_t to_write = remaining < sizeof(chunk) ? remaining : sizeof(chunk);

        if (write_all(fd, chunk, to_write) < 0) {
            return -1;
        }
        remaining -= to_write;
    }

    return 0;
}

static int read_one(int fd, char *ch)
{
    for (;;) {
        ssize_t bytes_read = read(fd, ch, 1);

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        return bytes_read == 1 ? 1 : 0;
    }
}

int decompress(int source_fd, int dest_fd)
{
    char ch;

    while (read_one(source_fd, &ch) == 1) {
        if (ch == '+' || ch == '-') {
            char marker = ch;
            char bit = marker == '+' ? '1' : '0';
            size_t count = 0;
            int saw_digit = 0;

            for (;;) {
                int read_result = read_one(source_fd, &ch);

                if (read_result < 0) {
                    return -1;
                }
                if (read_result == 0) {
                    errno = EINVAL;
                    return -1;
                }
                if (ch == marker) {
                    break;
                }
                if (!isdigit((unsigned char)ch)) {
                    errno = EINVAL;
                    return -1;
                }

                saw_digit = 1;
                if (count > (SIZE_MAX - (size_t)(ch - '0')) / 10) {
                    errno = EOVERFLOW;
                    return -1;
                }
                count = count * 10 + (size_t)(ch - '0');
            }

            if (!saw_digit) {
                errno = EINVAL;
                return -1;
            }
            if (write_repeat(dest_fd, bit, count) < 0) {
                return -1;
            }
        } else {
            if (write_all(dest_fd, &ch, 1) < 0) {
                return -1;
            }
        }
    }

    if (errno != 0) {
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int source_fd;
    int dest_fd;
    int status = EXIT_SUCCESS;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <compressed source file> <destination file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    source_fd = open(argv[1], O_RDONLY);
    if (source_fd < 0) {
        perror("open source");
        return EXIT_FAILURE;
    }

    dest_fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dest_fd < 0) {
        perror("open destination");
        close(source_fd);
        return EXIT_FAILURE;
    }

    errno = 0;
    if (decompress(source_fd, dest_fd) < 0) {
        perror("decompress");
        status = EXIT_FAILURE;
    }

    if (close(source_fd) < 0) {
        perror("close source");
        status = EXIT_FAILURE;
    }

    if (close(dest_fd) < 0) {
        perror("close destination");
        status = EXIT_FAILURE;
    }

    return status;
}
