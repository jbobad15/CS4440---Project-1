// MiniShell.c - minimal shell that runs argument-less commands via fork/execlp

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void) {
    char cmd[256];
    printf("MiniShell started. Type 'exit' to quit.\n");
    while (1) {
        printf("mysh> "); fflush(stdout);
        if (fgets(cmd, sizeof(cmd), stdin) == NULL) { printf("\n"); break; }
        cmd[strcspn(cmd, "\n")] = '\0';
        if (strlen(cmd) == 0) continue;
        if (strcmp(cmd, "exit") == 0) break;

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) {
            execlp(cmd, cmd, (char *)NULL);
            fprintf(stderr, "command not found: %s\n", cmd);
            exit(1);
        }
        int status;
        if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); exit(1); }
    }
    printf("exiting.\n");
    return 0;
}
