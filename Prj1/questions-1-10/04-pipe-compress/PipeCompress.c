// PipeCompress.c - uses forked processes and a pipe to compress file data

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
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

static int copy_file_to_pipe(const char *source_name, int pipe_write_fd)
{
    int source_fd = open(source_name, O_RDONLY);
    char buffer[BUFFER_SIZE];

    if (source_fd < 0) {
        return -1;
    }

    for (;;) {
        ssize_t bytes_read = read(source_fd, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(source_fd);
            return -1;
        }
        if (bytes_read == 0) {
            break;
        }
        if (write_all(pipe_write_fd, buffer, (size_t)bytes_read) < 0) {
            close(source_fd);
            return -1;
        }
    }

    if (close(source_fd) < 0) {
        return -1;
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

static int compress_stream(int source_fd, int dest_fd)
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

static int compress_pipe_to_file(int pipe_read_fd, const char *dest_name)
{
    int dest_fd = open(dest_name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    int result;

    if (dest_fd < 0) {
        return -1;
    }

    result = compress_stream(pipe_read_fd, dest_fd);
    if (close(dest_fd) < 0) {
        result = -1;
    }

    return result;
}

static int wait_for_child(pid_t pid)
{
    int status;

    for (;;) {
        if (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        break;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "Child %ld terminated by signal %d\n", (long)pid, WTERMSIG(status));
    } else {
        fprintf(stderr, "Child %ld did not exit normally\n", (long)pid);
    }

    return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
    int pipe_fds[2];
    pid_t reader_pid;
    pid_t writer_pid;
    int reader_status;
    int writer_status;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source file> <destination file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (pipe(pipe_fds) < 0) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    reader_pid = fork();
    if (reader_pid < 0) {
        perror("fork reader");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return EXIT_FAILURE;
    }

    if (reader_pid == 0) {
        close(pipe_fds[0]);
        if (copy_file_to_pipe(argv[1], pipe_fds[1]) < 0) {
            perror("reader child");
            close(pipe_fds[1]);
            _exit(EXIT_FAILURE);
        }
        if (close(pipe_fds[1]) < 0) {
            perror("close pipe write");
            _exit(EXIT_FAILURE);
        }
        _exit(EXIT_SUCCESS);
    }

    writer_pid = fork();
    if (writer_pid < 0) {
        perror("fork writer");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        wait_for_child(reader_pid);
        return EXIT_FAILURE;
    }

    if (writer_pid == 0) {
        close(pipe_fds[1]);
        if (compress_pipe_to_file(pipe_fds[0], argv[2]) < 0) {
            perror("writer child");
            close(pipe_fds[0]);
            _exit(EXIT_FAILURE);
        }
        if (close(pipe_fds[0]) < 0) {
            perror("close pipe read");
            _exit(EXIT_FAILURE);
        }
        _exit(EXIT_SUCCESS);
    }

    close(pipe_fds[0]);
    close(pipe_fds[1]);

    reader_status = wait_for_child(reader_pid);
    if (reader_status < 0) {
        perror("wait reader");
        reader_status = EXIT_FAILURE;
    }

    writer_status = wait_for_child(writer_pid);
    if (writer_status < 0) {
        perror("wait writer");
        writer_status = EXIT_FAILURE;
    }

    return reader_status == EXIT_SUCCESS && writer_status == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
