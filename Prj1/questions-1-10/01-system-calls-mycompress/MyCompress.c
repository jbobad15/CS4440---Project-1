// MyCompress.c - compresses runs of 0s and 1s using Unix file system calls

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define MIN_COMPRESS_RUN 16

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

static int flush_run(int dest_fd, char bit, size_t count)
{
    char marker;
    char encoded[64];
    int encoded_len;

    if (count == 0) {
        return 0;
    }

    if (count < MIN_COMPRESS_RUN) {
        char chunk[BUFFER_SIZE];
        size_t remaining = count;

        memset(chunk, bit, sizeof(chunk));
        while (remaining > 0) {
            size_t to_write = remaining < sizeof(chunk) ? remaining : sizeof(chunk);

            if (write_all(dest_fd, chunk, to_write) < 0) {
                return -1;
            }
            remaining -= to_write;
        }

        return 0;
    }

    marker = bit == '1' ? '+' : '-';
    encoded_len = snprintf(encoded, sizeof(encoded), "%c%zu%c", marker, count, marker);
    if (encoded_len < 0 || (size_t)encoded_len >= sizeof(encoded)) {
        errno = EOVERFLOW;
        return -1;
    }

    return write_all(dest_fd, encoded, (size_t)encoded_len);
}

int compress(int source_fd, int dest_fd)
{
    char buffer[BUFFER_SIZE];
    char current_bit = '\0';
    size_t current_count = 0;

    for (;;) {
        ssize_t bytes_read = read(source_fd, buffer, sizeof(buffer));
        ssize_t i;

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (bytes_read == 0) {
            break;
        }

        for (i = 0; i < bytes_read; i++) {
            char ch = buffer[i];

            if (ch == '0' || ch == '1') {
                if (current_count == 0) {
                    current_bit = ch;
                    current_count = 1;
                } else if (ch == current_bit) {
                    current_count++;
                } else {
                    if (flush_run(dest_fd, current_bit, current_count) < 0) {
                        return -1;
                    }
                    current_bit = ch;
                    current_count = 1;
                }
            } else {
                if (flush_run(dest_fd, current_bit, current_count) < 0) {
                    return -1;
                }
                current_bit = '\0';
                current_count = 0;

                if (write_all(dest_fd, &ch, 1) < 0) {
                    return -1;
                }
            }
        }
    }

    return flush_run(dest_fd, current_bit, current_count);
}

int main(int argc, char *argv[])
{
    int source_fd;
    int dest_fd;
    int status = EXIT_SUCCESS;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source file> <destination file>\n", argv[0]);
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

    if (compress(source_fd, dest_fd) < 0) {
        perror("compress");
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
