// DupShell.c - shell with one pipe, using dup2 to connect two commands

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static void split_args(char *cmd, char *argv[]) {
    int i = 0;
    char *tok = strtok(cmd, " \t");
    while (tok && i < 63) {
        argv[i++] = tok;
        tok = strtok(NULL, " \t");
    }
    argv[i] = NULL;
}

static void run_plain(char *line) {
    char *argv[64];
    split_args(line, argv);
    if (!argv[0]) return;
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    }
    waitpid(pid, NULL, 0);
}

static void run_pipe(char *left, char *right) {
    int fd[2];
    char *a1[64], *a2[64];
    split_args(left, a1);
    split_args(right, a2);
    if (!a1[0] || !a2[0]) return;
    if (pipe(fd) < 0) { perror("pipe"); exit(1); }

    pid_t p1 = fork();
    if (p1 < 0) { perror("fork"); exit(1); }
    if (p1 == 0) {
        dup2(fd[1], STDOUT_FILENO);      // first command writes into pipe
        close(fd[0]); close(fd[1]);
        execvp(a1[0], a1);
        perror("execvp");
        exit(1);
    }

    pid_t p2 = fork();
    if (p2 < 0) { perror("fork"); exit(1); }
    if (p2 == 0) {
        dup2(fd[0], STDIN_FILENO);       // second command reads from pipe
        close(fd[0]); close(fd[1]);
        execvp(a2[0], a2);
        perror("execvp");
        exit(1);
    }

    close(fd[0]); close(fd[1]);
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
}

int main(void) {
    char line[1024];
    printf("DupShell started. Type 'exit' to quit.\n");
    while (1) {
        printf("mysh> "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "exit") == 0) break;
        if (line[0] == '\0') continue;

        char *bar = strchr(line, '|');
        if (bar) {
            *bar = '\0';
            run_pipe(line, bar + 1);
        } else {
            run_plain(line);
        }
    }
    printf("exiting.\n");
    return 0;
}
