#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include "pipe.h"

int handle_pipe(char **args) {
    int idx = -1;
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "|") == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1)
        return 0;

    int fd[2];
    pipe(fd);

    if (fork() == 0) {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]); close(fd[1]);
        args[idx] = NULL;
        execvp(args[0], args);
        exit(1);
    }

    if (fork() == 0) {
        dup2(fd[0], STDIN_FILENO);
        close(fd[1]); close(fd[0]);
        execvp(args[idx + 1], &args[idx + 1]);
        exit(1);
    }

    close(fd[0]); close(fd[1]);
    wait(NULL); wait(NULL);
    return 1;
}
