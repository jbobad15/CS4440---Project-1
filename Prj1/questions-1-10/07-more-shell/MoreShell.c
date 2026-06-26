// MoreShell.c - shell with argument support, tokenizes input and runs via fork/execvp

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void) {
    char input[1024];
    char *argv[128];
    printf("MoreShell started. Type 'exit' to quit.\n");
    while (1) {
        printf("mysh> "); fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) { printf("\n"); break; }
        input[strcspn(input, "\n")] = '\0';
        if (strlen(input) == 0) continue;
        if (strcmp(input, "exit") == 0) break;

        int argc = 0;
        char *tok = strtok(input, " \t");
        while (tok && argc < 127) { argv[argc++] = tok; tok = strtok(NULL, " \t"); }
        argv[argc] = NULL;
        if (argc == 0) continue;

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) {
            execvp(argv[0], argv);
            fprintf(stderr, "command not found: %s\n", argv[0]);
            exit(1);
        }
        int status;
        if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); exit(1); }
    }
    printf("exiting.\n");
    return 0;
}
