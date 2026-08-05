#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>

#include "pipe.h"
#include "monitor.h"
#include "safety.h"
#include "summary.h"
#include "suggest.h"
#include "history.h"

#define GREEN  "\033[1;32m"
#define CYAN   "\033[1;36m"
#define YELLOW "\033[1;33m"
#define RED    "\033[1;31m"
#define RESET  "\033[0m"

#define MAX_CMD 1024
#define MAX_ARGS 64

static char prev_dir[PATH_MAX] = "";

// Format path for prompt (replace $HOME with ~)
static void get_formatted_cwd(char *buffer, size_t size) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        snprintf(buffer, size, "unknown");
        return;
    }

    const char *home = getenv("HOME");
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(buffer, size, "~%s", cwd + strlen(home));
    } else {
        snprintf(buffer, size, "%s", cwd);
    }
}

// Builtin command: cd
static int handle_cd(char **args) {
    if (!args[0] || strcmp(args[0], "cd") != 0)
        return 0;

    char current_cwd[PATH_MAX];
    getcwd(current_cwd, sizeof(current_cwd));

    const char *target = args[1];
    if (!target || strcmp(target, "~") == 0) {
        target = getenv("HOME");
        if (!target) target = "/";
    } else if (strcmp(target, "-") == 0) {
        if (strlen(prev_dir) == 0) {
            printf(RED "cd: OLDPWD not set\n" RESET);
            return 1;
        }
        target = prev_dir;
        printf("%s\n", target);
    }

    if (chdir(target) != 0) {
        perror(RED "cd failed" RESET);
    } else {
        strncpy(prev_dir, current_cwd, sizeof(prev_dir));
        setenv("OLDPWD", current_cwd, 1);
        char new_cwd[PATH_MAX];
        if (getcwd(new_cwd, sizeof(new_cwd))) {
            setenv("PWD", new_cwd, 1);
        }
    }
    return 1;
}

// Builtin command: pwd
static int handle_pwd(char **args) {
    if (!args[0] || strcmp(args[0], "pwd") != 0)
        return 0;

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
    } else {
        perror("pwd failed");
    }
    return 1;
}

// Builtin command: help
static int handle_help(char **args) {
    if (!args[0] || strcmp(args[0], "help") != 0)
        return 0;

    printf("\n" CYAN "🌟 NOVASHELL ENTERPRISE COMMAND MANUAL" RESET "\n");
    printf("=========================================\n");
    printf(GREEN "  cd [dir]" RESET "         Change working directory (~ for home, - for previous)\n");
    printf(GREEN "  pwd" RESET "              Print current working directory\n");
    printf(GREEN "  history" RESET "          Display command execution log\n");
    printf(GREEN "  help" RESET "             Display this help guide\n");
    printf(GREEN "  exit" RESET "             Print session summary and exit NovaShell\n");
    printf("\n" YELLOW "Features & Syntax:" RESET "\n");
    printf("  • Piping         : command1 | command2\n");
    printf("  • Output Redir   : command > file.txt  (truncate)\n");
    printf("  • Append Redir   : command >> file.txt (append)\n");
    printf("  • Input Redir    : command < file.txt  (stdin)\n");
    printf("  • Background Job : command &\n");
    printf("  • Env Variables  : $HOME, $USER, $PWD expansion\n");
    printf("  • Safety Layer   : Blocks dangerous root commands (rm -rf, shutdown)\n\n");
    return 1;
}

// Expand environment variables ($VAR)
static void expand_env_vars(char **args) {
    for (int i = 0; args[i]; i++) {
        if (args[i][0] == '$' && strlen(args[i]) > 1) {
            char *val = getenv(args[i] + 1);
            if (val) {
                args[i] = val;
            }
        }
    }
}

int main() {
    char input[MAX_CMD];

    summary_init();
    history_init();
    signal(SIGINT, SIG_IGN);

    while (1) {
        char formatted_cwd[PATH_MAX];
        get_formatted_cwd(formatted_cwd, sizeof(formatted_cwd));

        printf(GREEN "nova-shell " CYAN "[%s]" GREEN "> " RESET, formatted_cwd);
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

        expand_env_vars(args);

        // Check builtin commands
        if (handle_cd(args)) continue;
        if (handle_pwd(args)) continue;
        if (handle_help(args)) continue;

        if (strcmp(args[0], "history") == 0) {
            history_print();
            continue;
        }

        if (is_dangerous_command(args)) {
            printf(RED "⚠️ Safety Alert: Dangerous command blocked for system security.\n" RESET);
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

        int redirect_out = -1;
        int redirect_append = -1;
        int redirect_in = -1;

        for (int j = 0; args[j]; j++) {
            if (strcmp(args[j], ">>") == 0) {
                redirect_append = j;
                break;
            } else if (strcmp(args[j], ">") == 0) {
                redirect_out = j;
                break;
            } else if (strcmp(args[j], "<") == 0) {
                redirect_in = j;
                break;
            }
        }

        summary_command();
        monitor_start();

        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);

            if (redirect_append != -1) {
                int fd = open(args[redirect_append + 1], O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd < 0) { perror("open failed"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[redirect_append] = NULL;
            } else if (redirect_out != -1) {
                int fd = open(args[redirect_out + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) { perror("open failed"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[redirect_out] = NULL;
            } else if (redirect_in != -1) {
                int fd = open(args[redirect_in + 1], O_RDONLY);
                if (fd < 0) { perror("open failed"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
                args[redirect_in] = NULL;
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
                    printf(RED "❌ Command not found: %s\n" RESET, args[0]);
                    suggest_command(args[0]);
                }
            } else {
                printf(CYAN "[+] Background job started (PID: %d)\n" RESET, pid);
            }
        }
    }
    return 0;
}
