// ParFork.c - splits input into N chunks, forks N children to compress in parallel, assembles output

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#define DEFAULT_PROCS 4
#define MIN_RUN 16
#define BUF_SIZE 4096

#define FLUSH_RUN() do { \
    if (run_len == 0) break; \
    if (run_len >= MIN_RUN) { \
        char tmp[32]; \
        int n = snprintf(tmp, sizeof(tmp), "%c%ld%c", (cur_char == '1') ? '+' : '-', run_len, (cur_char == '1') ? '+' : '-'); \
        if (write(dst_fd, tmp, n) != n) { perror("write"); return -1; } \
    } else { \
        for (long _i = 0; _i < run_len; _i++) \
            if (write(dst_fd, &cur_char, 1) != 1) { perror("write"); return -1; } \
    } \
    run_len = 0; \
} while (0)

static int compress_segment(int src_fd, off_t offset, size_t length, int dst_fd) {
    if (lseek(src_fd, offset, SEEK_SET) == (off_t)-1) { perror("lseek"); return -1; }
    char cur_char = '\0', buf[BUF_SIZE];
    long run_len = 0;
    size_t total_read = 0;
    while (total_read < length) {
        size_t to_read = (BUF_SIZE < length - total_read) ? BUF_SIZE : length - total_read;
        ssize_t nr = read(src_fd, buf, to_read);
        if (nr < 0) { perror("read"); return -1; }
        if (nr == 0) break;
        for (ssize_t i = 0; i < nr; i++) {
            char c = buf[i];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
            if (c != '0' && c != '1') continue;
            if (c == cur_char) { run_len++; }
            else { FLUSH_RUN(); cur_char = c; run_len = 1; }
        }
        total_read += nr;
    }
    FLUSH_RUN();
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) { fprintf(stderr, "Usage: %s <source> <dest> [num_procs]\n", argv[0]); exit(1); }
    const char *src_path = argv[1], *dst_path = argv[2];
    int n_procs = (argc == 4) ? atoi(argv[3]) : DEFAULT_PROCS;
    if (n_procs < 1 || n_procs > 64) { fprintf(stderr, "num_procs must be 1-64\n"); exit(1); }

    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) { perror("open source"); exit(1); }
    struct stat st;
    if (fstat(src_fd, &st) < 0) { perror("fstat"); close(src_fd); exit(1); }
    off_t file_size = st.st_size;
    if (file_size == 0) { fprintf(stderr, "source file is empty\n"); close(src_fd); exit(1); }

    pid_t parent_pid = getpid();
    off_t chunk_size = file_size / n_procs;
    if (chunk_size == 0) { n_procs = 1; chunk_size = file_size; }

    pid_t pids[64];
    char tmp_paths[64][256];

    for (int i = 0; i < n_procs; i++) {
        off_t offset = (off_t)i * chunk_size;
        size_t length = (i == n_procs - 1) ? (size_t)(file_size - offset) : (size_t)chunk_size;
        snprintf(tmp_paths[i], sizeof(tmp_paths[i]), "/tmp/parfork_%d_%d.tmp", (int)parent_pid, i);

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); for (int j = 0; j < i; j++) waitpid(pids[j], NULL, 0); close(src_fd); exit(1); }
        if (pid == 0) {
            int csrc = open(src_path, O_RDONLY);
            if (csrc < 0) { perror("child open source"); exit(1); }
            int tfd = open(tmp_paths[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (tfd < 0) { perror("child open tmp"); close(csrc); exit(1); }
            if (compress_segment(csrc, offset, length, tfd) != 0) { close(csrc); close(tfd); exit(1); }
            close(csrc); close(tfd);
            exit(0);
        }
        pids[i] = pid;
    }

    int all_ok = 1;
    for (int i = 0; i < n_procs; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "child %d failed\n", i); all_ok = 0;
        }
    }
    close(src_fd);
    if (!all_ok) { fprintf(stderr, "aborting assembly\n"); exit(1); }

    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dst_fd < 0) { perror("open dest"); exit(1); }

    char cbuf[BUF_SIZE];
    for (int i = 0; i < n_procs; i++) {
        int tfd = open(tmp_paths[i], O_RDONLY);
        if (tfd < 0) { perror("open tmp"); close(dst_fd); exit(1); }
        ssize_t nr;
        while ((nr = read(tfd, cbuf, BUF_SIZE)) > 0)
            if (write(dst_fd, cbuf, nr) != nr) { perror("write assembly"); close(tfd); close(dst_fd); exit(1); }
        if (nr < 0) { perror("read tmp"); close(tfd); close(dst_fd); exit(1); }
        close(tfd);
        unlink(tmp_paths[i]);
    }
    close(dst_fd);
    printf("done: %s\n", dst_path);
    return 0;
}
