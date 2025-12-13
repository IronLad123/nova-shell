#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "pipe.h"
#include "monitor.h"
#include "safety.h"
#include "summary.h"
#include "suggest.h"
#include "history.h"

#define GREEN "\033[32m"
#define RESET "\033[0m"

#define MAX_CMD 1024
#define MAX_ARGS 64

int main() {
    char input[MAX_CMD];

    summary_init();
    history_init();
    signal(SIGINT, SIG_IGN);

    while (1) {
        printf(GREEN "nova-shell> " RESET);
        fflush(stdout);

        if (!fgets(input, MAX_CMD, stdin))
            break;

        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0)
            continue;

        history_add(input);

        if (strcmp(input, "exit") == 0) {
            summary_print();
            break;
        }

        char *args[MAX_ARGS];
        int i = 0;
        char *tok = strtok(input, " ");
        while (tok && i < MAX_ARGS - 1) {
            args[i++] = tok;
            tok = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (strcmp(args[0], "history") == 0) {
            history_print();
            continue;
        }

        if (is_dangerous_command(args)) {
            printf("Dangerous command blocked\n");
            continue;
        }

        int background = 0;
        if (i > 0 && strcmp(args[i - 1], "&") == 0) {
            background = 1;
            args[i - 1] = NULL;
            summary_background();
        }

        if (handle_pipe(args)) {
            summary_pipe();
            continue;
        }

        int redirect = -1;
        for (int j = 0; args[j]; j++) {
            if (strcmp(args[j], ">") == 0) {
                redirect = j;
                break;
            }
        }

        summary_command();
        monitor_start();

        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);

            if (redirect != -1) {
                int fd = open(args[redirect + 1],
                              O_CREAT | O_WRONLY | O_TRUNC, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[redirect] = NULL;
            }

            execvp(args[0], args);
            exit(127);
        }
        else if (pid > 0) {
            if (!background) {
                int status;
                wait(&status);
                monitor_end();

                if (WEXITSTATUS(status) == 127) {
                    printf("❌ Command not found: %s\n", args[0]);
                    suggest_command(args[0]);
                }
            }
        }
    }
    return 0;
}
