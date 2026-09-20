/*
 * pipe.c — Multi-stage pipeline engine
 *
 * Supports arbitrary-depth pipelines: cmd1 | cmd2 | cmd3 | ... | cmdN
 * Uses a single pass to locate all pipe symbols, allocates N-1 pipe
 * file-descriptor pairs, and forks N children with correctly wired
 * stdin/stdout via dup2().  The parent waits for all children.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "pipe.h"

#define MAX_PIPE_STAGES 16

/*
 * handle_pipe()
 *
 * Returns 0 if no pipe symbol is found (caller handles normally).
 * Returns 1 after executing the pipeline.
 */
int handle_pipe(char **args) {
    /* ── 1. Find all pipe positions ─────────────────────────────── */
    int pipe_pos[MAX_PIPE_STAGES];
    int n_pipes = 0;

    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "|") == 0) {
            if (n_pipes >= MAX_PIPE_STAGES - 1) {
                fprintf(stderr, "nova: too many pipes (max %d stages)\n",
                        MAX_PIPE_STAGES);
                return 1;
            }
            pipe_pos[n_pipes++] = i;
        }
    }

    if (n_pipes == 0) return 0;   /* no pipe — tell caller to handle */

    int n_cmds = n_pipes + 1;     /* number of pipeline stages        */

    /* ── 2. Build sub-command argv arrays ───────────────────────── */
    /* Mark pipe symbols as terminators by NULLing them             */
    char **cmds[MAX_PIPE_STAGES];
    cmds[0] = &args[0];
    for (int p = 0; p < n_pipes; p++) {
        args[pipe_pos[p]] = NULL;           /* terminate left segment */
        cmds[p + 1] = &args[pipe_pos[p] + 1]; /* start right segment */
    }

    /* ── 3. Create N-1 pipes ────────────────────────────────────── */
    int fds[MAX_PIPE_STAGES][2];
    for (int p = 0; p < n_pipes; p++) {
        if (pipe(fds[p]) < 0) {
            perror("nova: pipe");
            return 1;
        }
    }

    /* ── 4. Fork one child per stage ────────────────────────────── */
    pid_t pids[MAX_PIPE_STAGES];

    for (int s = 0; s < n_cmds; s++) {
        pids[s] = fork();

        if (pids[s] < 0) {
            perror("nova: fork");
            return 1;
        }

        if (pids[s] == 0) {
            /* Child: restore default SIGINT */
            signal(SIGINT, SIG_DFL);

            /* Wire stdin from left pipe (all but first stage) */
            if (s > 0) {
                dup2(fds[s-1][0], STDIN_FILENO);
            }
            /* Wire stdout to right pipe (all but last stage) */
            if (s < n_cmds - 1) {
                dup2(fds[s][1], STDOUT_FILENO);
            }

            /* Close all pipe fds in child — only the dup'd ones remain */
            for (int p = 0; p < n_pipes; p++) {
                close(fds[p][0]);
                close(fds[p][1]);
            }

            execvp(cmds[s][0], cmds[s]);
            fprintf(stderr, "\033[1;31mnova: %s: command not found\033[0m\n",
                    cmds[s][0]);
            exit(127);
        }
    }

    /* ── 5. Parent: close all pipe fds and wait ─────────────────── */
    for (int p = 0; p < n_pipes; p++) {
        close(fds[p][0]);
        close(fds[p][1]);
    }
    for (int s = 0; s < n_cmds; s++) {
        waitpid(pids[s], NULL, 0);
    }

    return 1;
}
