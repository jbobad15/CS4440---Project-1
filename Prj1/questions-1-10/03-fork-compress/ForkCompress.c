#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    pid_t pid;
    int wait_status;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source file> <destination file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        execl("./MyCompress", "MyCompress", argv[1], argv[2], (char *)NULL);
        perror("execl");
        _exit(127);
    }

    (void)pid;

    for (;;) {
        if (wait(&wait_status) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("wait");
            return EXIT_FAILURE;
        }
        break;
    }

    if (WIFEXITED(wait_status)) {
        return WEXITSTATUS(wait_status);
    }

    if (WIFSIGNALED(wait_status)) {
        fprintf(stderr, "MyCompress terminated by signal %d\n", WTERMSIG(wait_status));
    } else {
        fprintf(stderr, "MyCompress did not exit normally\n");
    }

    return EXIT_FAILURE;
}
