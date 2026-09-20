/*
 * history.c — Persistent command history with session recall
 *
 * - Stores up to MAX_HISTORY entries in a ring buffer in memory.
 * - On init, loads previous entries from ~/.nova_history.
 * - On shell exit (history_save), appends new session entries.
 * - Supports !N bang-style recall (checked in main.c).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "history.h"

#define MAX_HISTORY  500
#define HISTORY_FILE ".nova_history"

static char *hist[MAX_HISTORY];
static int   hist_count = 0;
static int   session_start = 0;   /* index of first entry from THIS session */

/* ── Helper: resolve ~/.nova_history path ───────────────────────── */
static void history_path(char *buf, size_t sz) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s", home, HISTORY_FILE);
}

/* ── Load history from disk ─────────────────────────────────────── */
void history_init(void) {
    for (int i = 0; i < MAX_HISTORY; i++) hist[i] = NULL;
    hist_count = 0;

    char path[PATH_MAX];
    history_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) { session_start = 0; return; }

    char line[1024];
    while (fgets(line, sizeof(line), f) && hist_count < MAX_HISTORY) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;
        hist[hist_count++] = strdup(line);
    }
    fclose(f);
    session_start = hist_count;
}

/* ── Add a command to in-memory history ─────────────────────────── */
void history_add(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;

    /* Don't add duplicates of the immediately previous command */
    if (hist_count > 0 && strcmp(hist[hist_count - 1], cmd) == 0) return;

    if (hist_count == MAX_HISTORY) {
        free(hist[0]);
        memmove(&hist[0], &hist[1], (MAX_HISTORY - 1) * sizeof(char *));
        hist_count--;
        if (session_start > 0) session_start--;
    }
    hist[hist_count++] = strdup(cmd);
}

/* ── Print history (all entries with index) ─────────────────────── */
void history_print(void) {
    printf("\n\033[1;36m  #   Command\033[0m\n");
    printf("  ─────────────────────────────────────\n");
    for (int i = 0; i < hist_count; i++) {
        /* Highlight current-session entries */
        if (i >= session_start)
            printf("\033[1;32m%3d\033[0m  %s\n", i + 1, hist[i]);
        else
            printf("\033[0;90m%3d\033[0m  %s\n", i + 1, hist[i]);
    }
    printf("\n");
}

/* ── Recall a specific entry by 1-based index ───────────────────── */
const char *history_get(int n) {
    if (n < 1 || n > hist_count) return NULL;
    return hist[n - 1];
}

/* ── Return total count ─────────────────────────────────────────── */
int history_count_get(void) {
    return hist_count;
}

/* ── Persist new session entries to disk ────────────────────────── */
void history_save(void) {
    if (hist_count == session_start) return;  /* nothing new this session */

    char path[PATH_MAX];
    history_path(path, sizeof(path));

    FILE *f = fopen(path, "a");
    if (!f) return;

    for (int i = session_start; i < hist_count; i++) {
        fprintf(f, "%s\n", hist[i]);
    }
    fclose(f);
}
