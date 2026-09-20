/*
 * safety.c — Destructive command interception layer
 *
 * Maintains a table of (command, flag) pairs that are unconditionally
 * dangerous.  Also detects wildcard rm patterns.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safety.h"

#define RED    "\033[1;31m"
#define YELLOW "\033[1;33m"
#define RESET  "\033[0m"

/* ── Blocked single-command entries ─────────────────────────────── */
static const char *blocked_cmds[] = {
    "shutdown", "reboot", "halt", "poweroff", "init",
    "mkfs", "fdisk", "dd",   /* disk-level destructors    */
    NULL
};

/* ── Blocked (command, flag) pairs ──────────────────────────────── */
typedef struct { const char *cmd; const char *flag; } CmdFlag;

static const CmdFlag blocked_pairs[] = {
    { "rm",    "-rf"  },
    { "rm",    "-fr"  },
    { "rm",    "-rf/" },
    { "chmod", "777"  },   /* chmod 777 / is a classic footgun */
    { NULL,    NULL   }
};

/* ── Wildcard rm target patterns ────────────────────────────────── */
static const char *dangerous_rm_targets[] = {
    "/",  "/*",  "/.",  "~",  "~/*",
    NULL
};

int is_dangerous_command(char **args) {
    if (!args || !args[0]) return 0;

    /* Check blocked single commands */
    for (int i = 0; blocked_cmds[i]; i++) {
        if (strcmp(args[0], blocked_cmds[i]) == 0) {
            fprintf(stderr,
                RED "🚨 SAFETY LOCKDOWN" RESET ": "
                YELLOW "'%s'" RESET " is blocked — nova-shell will not execute it.\n",
                args[0]);
            return 1;
        }
    }

    /* Check blocked (cmd, flag) pairs */
    for (int i = 0; blocked_pairs[i].cmd; i++) {
        if (strcmp(args[0], blocked_pairs[i].cmd) == 0) {
            for (int j = 1; args[j]; j++) {
                if (strcmp(args[j], blocked_pairs[i].flag) == 0) {
                    fprintf(stderr,
                        RED "🚨 SAFETY LOCKDOWN" RESET ": "
                        YELLOW "'%s %s'" RESET " is blocked — "
                        "this flag combination is catastrophically destructive.\n",
                        args[0], args[j]);
                    return 1;
                }
            }
        }
    }

    /* Extra check: rm targeting dangerous paths */
    if (strcmp(args[0], "rm") == 0) {
        for (int j = 1; args[j]; j++) {
            for (int k = 0; dangerous_rm_targets[k]; k++) {
                if (strcmp(args[j], dangerous_rm_targets[k]) == 0) {
                    fprintf(stderr,
                        RED "🚨 SAFETY LOCKDOWN" RESET ": "
                        YELLOW "'rm %s'" RESET " targets a protected path.\n",
                        args[j]);
                    return 1;
                }
            }
        }
    }

    return 0;
}
