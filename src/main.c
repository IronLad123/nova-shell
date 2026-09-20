/*
 * main.c — nova-shell command loop
 *
 * New in this version:
 *  - Quoted string tokenisation  ("hello world" stays one token)
 *  - export VAR=VALUE / unset VAR builtins
 *  - alias NAME=CMD / unalias NAME / alias (list) builtins
 *  - !N  bang history recall
 *  - SIGCHLD handler to reap background zombie processes
 *  - history_save() called on exit to persist history
 *  - summary_error() called when command not found
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <ctype.h>

#include "pipe.h"
#include "monitor.h"
#include "safety.h"
#include "summary.h"
#include "suggest.h"
#include "history.h"

/* ── ANSI colours ─────────────────────────────────────────────── */
#define GREEN  "\033[1;32m"
#define CYAN   "\033[1;36m"
#define YELLOW "\033[1;33m"
#define RED    "\033[1;31m"
#define MAGENTA "\033[1;35m"
#define RESET  "\033[0m"

/* ── Shell limits ─────────────────────────────────────────────── */
#define MAX_CMD    2048
#define MAX_ARGS   128
#define MAX_ALIAS  64

/* ── Simple alias table ───────────────────────────────────────── */
typedef struct { char *name; char *value; } Alias;
static Alias aliases[MAX_ALIAS];
static int   alias_count = 0;

static const char *alias_lookup(const char *name) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, name) == 0)
            return aliases[i].value;
    return NULL;
}

static void alias_set(const char *name, const char *value) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            free(aliases[i].value);
            aliases[i].value = strdup(value);
            return;
        }
    }
    if (alias_count < MAX_ALIAS) {
        aliases[alias_count].name  = strdup(name);
        aliases[alias_count].value = strdup(value);
        alias_count++;
    }
}

static void alias_remove(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            free(aliases[i].name);
            free(aliases[i].value);
            aliases[i] = aliases[--alias_count];
            return;
        }
    }
}

/* ── SIGCHLD: reap background zombies automatically ───────────── */
static void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ── Prompt: "nova [~/path]> " ───────────────────────────────── */
static void print_prompt(void) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "?");
    } else {
        const char *home = getenv("HOME");
        if (home && strncmp(cwd, home, strlen(home)) == 0) {
            /* replace home prefix with ~ */
            char tmp[PATH_MAX];
            snprintf(tmp, sizeof(tmp), "~%s", cwd + strlen(home));
            strncpy(cwd, tmp, sizeof(cwd));
        }
    }
    printf(GREEN "nova " CYAN "[%s]" GREEN "> " RESET, cwd);
    fflush(stdout);
}

/* ── Tokenise respecting single-quoted and double-quoted strings ─ */
static int tokenise(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p   = line;

    while (*p && argc < max_args - 1) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        if (*p == '"' || *p == '\'') {
            /* Quoted token */
            char q = *p++;
            argv[argc++] = p;
            while (*p && *p != q) p++;
            if (*p == q) *p++ = '\0';
        } else {
            /* Unquoted token */
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    argv[argc] = NULL;
    return argc;
}

/* ── Expand $VAR tokens in-place (modifies argv values) ──────── */
static void expand_env(char **argv) {
    for (int i = 0; argv[i]; i++) {
        if (argv[i][0] == '$' && argv[i][1] != '\0') {
            char *val = getenv(argv[i] + 1);
            if (val) argv[i] = val;
        }
    }
}

/* ── Expand alias for argv[0] ─────────────────────────────────── */
static void expand_alias(char **argv, char *buf, size_t bufsz) {
    if (!argv[0]) return;
    const char *aval = alias_lookup(argv[0]);
    if (!aval) return;

    /* Re-tokenise alias value into the front of argv */
    strncpy(buf, aval, bufsz - 1);
    buf[bufsz - 1] = '\0';

    char *tmp[MAX_ARGS];
    int   n = tokenise(buf, tmp, MAX_ARGS);

    /* Shift existing args to make room */
    int orig_argc = 0;
    while (argv[orig_argc]) orig_argc++;

    for (int i = orig_argc; i >= 1; i--)
        argv[i + n - 1] = argv[i];

    for (int i = 0; i < n; i++)
        argv[i] = tmp[i];
}

/* ── previous dir for cd - ────────────────────────────────────── */
static char prev_dir[PATH_MAX] = "";

/* ── Builtin: cd ─────────────────────────────────────────────── */
static int builtin_cd(char **argv) {
    if (strcmp(argv[0], "cd") != 0) return 0;

    char current[PATH_MAX];
    getcwd(current, sizeof(current));

    const char *target = argv[1];
    if (!target || strcmp(target, "~") == 0) {
        target = getenv("HOME");
        if (!target) target = "/";
    } else if (strcmp(target, "-") == 0) {
        if (prev_dir[0] == '\0') {
            fprintf(stderr, RED "cd: OLDPWD not set\n" RESET);
            return 1;
        }
        printf("%s\n", prev_dir);
        target = prev_dir;
    }

    if (chdir(target) != 0) {
        fprintf(stderr, RED "cd: %s: No such file or directory\n" RESET, target);
    } else {
        strncpy(prev_dir, current, sizeof(prev_dir) - 1);
        setenv("OLDPWD", current, 1);
        char newdir[PATH_MAX];
        if (getcwd(newdir, sizeof(newdir))) setenv("PWD", newdir, 1);
    }
    return 1;
}

/* ── Builtin: export ─────────────────────────────────────────── */
static int builtin_export(char **argv) {
    if (strcmp(argv[0], "export") != 0) return 0;
    if (!argv[1]) {
        /* Print all exported vars */
        extern char **environ;
        for (char **ep = environ; *ep; ep++) printf("export %s\n", *ep);
        return 1;
    }
    char *eq = strchr(argv[1], '=');
    if (!eq) {
        /* export VARNAME (mark existing var as exported — no-op in simple shell) */
        return 1;
    }
    *eq = '\0';
    setenv(argv[1], eq + 1, 1);
    *eq = '=';   /* restore for safety */
    return 1;
}

/* ── Builtin: unset ──────────────────────────────────────────── */
static int builtin_unset(char **argv) {
    if (strcmp(argv[0], "unset") != 0) return 0;
    if (argv[1]) unsetenv(argv[1]);
    return 1;
}

/* ── Builtin: alias ──────────────────────────────────────────── */
static int builtin_alias(char **argv) {
    if (strcmp(argv[0], "alias") != 0) return 0;
    if (!argv[1]) {
        /* List all aliases */
        for (int i = 0; i < alias_count; i++)
            printf("alias %s='%s'\n", aliases[i].name, aliases[i].value);
        return 1;
    }
    char *eq = strchr(argv[1], '=');
    if (!eq) {
        /* Print single alias */
        const char *v = alias_lookup(argv[1]);
        if (v) printf("alias %s='%s'\n", argv[1], v);
        return 1;
    }
    *eq = '\0';
    /* Strip surrounding quotes from value */
    char *val = eq + 1;
    size_t vlen = strlen(val);
    if (vlen >= 2 && ((val[0] == '\'' && val[vlen-1] == '\'') ||
                      (val[0] == '"'  && val[vlen-1] == '"'))) {
        val[vlen-1] = '\0';
        val++;
    }
    alias_set(argv[1], val);
    return 1;
}

/* ── Builtin: unalias ────────────────────────────────────────── */
static int builtin_unalias(char **argv) {
    if (strcmp(argv[0], "unalias") != 0) return 0;
    if (argv[1]) alias_remove(argv[1]);
    return 1;
}

/* ── Builtin: pwd ────────────────────────────────────────────── */
static int builtin_pwd(char **argv) {
    if (strcmp(argv[0], "pwd") != 0) return 0;
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
    else perror("pwd");
    return 1;
}

/* ── Builtin: help ───────────────────────────────────────────── */
static int builtin_help(char **argv) {
    if (strcmp(argv[0], "help") != 0) return 0;

    printf("\n" CYAN "  ╔═══════════════════════════════════════════╗\n");
    printf(       "  ║        nova-shell  built-in  commands       ║\n");
    printf(       "  ╚═══════════════════════════════════════════╝\n" RESET);
    printf(GREEN  "  cd [dir|-|~]" RESET    "      Change directory\n");
    printf(GREEN  "  pwd" RESET             "                Print working directory\n");
    printf(GREEN  "  export VAR=VAL" RESET  "    Set environment variable\n");
    printf(GREEN  "  unset VAR" RESET       "         Remove environment variable\n");
    printf(GREEN  "  alias N=CMD" RESET     "       Define command alias\n");
    printf(GREEN  "  unalias N" RESET       "         Remove alias\n");
    printf(GREEN  "  history" RESET         "           Show command history\n");
    printf(GREEN  "  !N" RESET              "                Recall history entry N\n");
    printf(GREEN  "  exit" RESET            "              Save history & exit\n\n");
    printf(YELLOW "  Syntax:\n" RESET);
    printf("    Pipe      : cmd1 | cmd2 | cmd3  (unlimited depth)\n");
    printf("    Redirect  : cmd > file  cmd >> file  cmd < file\n");
    printf("    Background: cmd &\n");
    printf("    Variables : $VAR expansion, \"quoted $VAR\" supported\n\n");
    return 1;
}

/* ── Redirect helpers ────────────────────────────────────────── */
typedef struct {
    int out_fd;      /* -1 = none, otherwise open fd */
    int in_fd;
    int out_append;  /* 1 = O_APPEND */
} Redirects;

static Redirects parse_redirects(char **argv) {
    Redirects r = { -1, -1, 0 };
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], ">>") == 0 && argv[i+1]) {
            r.out_fd = open(argv[i+1], O_CREAT|O_WRONLY|O_APPEND, 0644);
            r.out_append = 1;
            argv[i] = NULL; /* truncate argv */
            break;
        }
        if (strcmp(argv[i], ">") == 0 && argv[i+1]) {
            r.out_fd = open(argv[i+1], O_CREAT|O_WRONLY|O_TRUNC, 0644);
            argv[i] = NULL;
            break;
        }
        if (strcmp(argv[i], "<") == 0 && argv[i+1]) {
            r.in_fd = open(argv[i+1], O_RDONLY);
            if (r.in_fd < 0) perror("nova: open");
            argv[i] = NULL;
            break;
        }
    }
    return r;
}

/* ═══════════════════════════════════════════════════════════════
 * main()
 * ═══════════════════════════════════════════════════════════════ */
int main(void) {
    summary_init();
    history_init();

    /* Ignore Ctrl-C in the shell itself; restore in child */
    signal(SIGINT,  SIG_IGN);
    /* Auto-reap background children */
    signal(SIGCHLD, sigchld_handler);

    char input[MAX_CMD];

    printf(CYAN "\n  ███╗   ██╗ ██████╗ ██╗   ██╗ █████╗\n");
    printf(     "  ████╗  ██║██╔═══██╗██║   ██║██╔══██╗\n");
    printf(     "  ██╔██╗ ██║██║   ██║██║   ██║███████║\n");
    printf(     "  ██║╚██╗██║██║   ██║╚██╗ ██╔╝██╔══██║\n");
    printf(     "  ██║ ╚████║╚██████╔╝ ╚████╔╝ ██║  ██║\n");
    printf(     "  ╚═╝  ╚═══╝ ╚═════╝   ╚═══╝  ╚═╝  ╚═╝\n");
    printf(RESET GREEN "         shell  •  type 'help' to begin\n\n" RESET);

    while (1) {
        print_prompt();

        if (!fgets(input, sizeof(input), stdin)) {
            printf("\n");
            break;
        }

        /* Strip trailing newline */
        input[strcspn(input, "\n")] = '\0';
        if (input[0] == '\0') continue;

        /* ── !N bang recall ─────────────────────────────────── */
        if (input[0] == '!') {
            int n = atoi(input + 1);
            const char *recalled = history_get(n);
            if (!recalled) {
                fprintf(stderr, RED "nova: !%d: event not found\n" RESET, n);
                continue;
            }
            printf("%s\n", recalled);
            strncpy(input, recalled, sizeof(input) - 1);
        }

        history_add(input);

        /* ── exit ───────────────────────────────────────────── */
        if (strcmp(input, "exit") == 0) {
            history_save();
            summary_print();
            break;
        }

        /* ── Tokenise ────────────────────────────────────────── */
        char line_copy[MAX_CMD];
        strncpy(line_copy, input, sizeof(line_copy) - 1);

        char *argv[MAX_ARGS];
        int   argc = tokenise(line_copy, argv, MAX_ARGS);
        if (argc == 0) continue;

        /* ── Alias expansion ─────────────────────────────────── */
        char alias_buf[MAX_CMD];
        expand_alias(argv, alias_buf, sizeof(alias_buf));

        /* ── Env expansion ───────────────────────────────────── */
        expand_env(argv);

        /* ── Built-ins ───────────────────────────────────────── */
        if (builtin_cd(argv))      continue;
        if (builtin_pwd(argv))     continue;
        if (builtin_help(argv))    continue;
        if (builtin_export(argv))  continue;
        if (builtin_unset(argv))   continue;
        if (builtin_alias(argv))   continue;
        if (builtin_unalias(argv)) continue;

        if (strcmp(argv[0], "history") == 0) {
            history_print();
            continue;
        }

        /* ── Safety gate ─────────────────────────────────────── */
        if (is_dangerous_command(argv)) continue;

        /* ── Background flag ─────────────────────────────────── */
        int background = 0;
        int last = argc - 1;
        if (last >= 0 && strcmp(argv[last], "&") == 0) {
            background = 1;
            argv[last] = NULL;
            summary_background();
        }

        /* ── Pipeline? ───────────────────────────────────────── */
        if (handle_pipe(argv)) {
            summary_pipe();
            continue;
        }

        /* ── Redirections ────────────────────────────────────── */
        Redirects redir = parse_redirects(argv);
        if ((redir.out_fd < 0 && redir.out_append) ||
            (redir.in_fd  < 0 && /* had < token but open failed */ 0)) {
            perror("nova: open");
            continue;
        }

        /* ── Fork + exec ─────────────────────────────────────── */
        summary_command();
        monitor_start();

        pid_t pid = fork();
        if (pid < 0) {
            perror("nova: fork");
            continue;
        }

        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            if (redir.out_fd >= 0) { dup2(redir.out_fd, STDOUT_FILENO); close(redir.out_fd); }
            if (redir.in_fd  >= 0) { dup2(redir.in_fd,  STDIN_FILENO);  close(redir.in_fd);  }

            execvp(argv[0], argv);
            fprintf(stderr, RED "nova: %s: command not found\n" RESET, argv[0]);
            exit(127);
        }

        /* Parent */
        if (redir.out_fd >= 0) close(redir.out_fd);
        if (redir.in_fd  >= 0) close(redir.in_fd);

        if (!background) {
            int status;
            waitpid(pid, &status, 0);
            monitor_end();

            if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                suggest_command(argv[0]);
                summary_error();
            }
        } else {
            printf(CYAN "[+] Background PID %d\n" RESET, pid);
        }
    }

    return 0;
}
