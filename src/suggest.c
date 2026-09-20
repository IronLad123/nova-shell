/*
 * suggest.c — Levenshtein-distance typo correction engine
 *
 * Uses true dynamic-programming edit-distance to find the closest
 * known command when the user mistypes. Scans $PATH for real binaries
 * so suggestions are always up-to-date.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include "suggest.h"

#define MAX_CANDIDATES 512
#define MAX_CMD_LEN    64
#define SUGGEST_THRESHOLD 3   /* max edit distance to bother suggesting */

/* ── Levenshtein distance (full DP, O(m·n)) ─────────────────────── */
static int levenshtein(const char *a, const char *b) {
    int la = (int)strlen(a);
    int lb = (int)strlen(b);

    /* Use a single-row rolling array — O(min(m,n)) space */
    int *row = malloc((lb + 1) * sizeof(int));
    if (!row) return INT_MAX;

    for (int j = 0; j <= lb; j++) row[j] = j;

    for (int i = 1; i <= la; i++) {
        int prev = i;
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int val = row[j-1] + cost;           /* substitute */
            if (row[j]   + 1 < val) val = row[j]   + 1; /* delete  */
            if (prev      + 1 < val) val = prev      + 1; /* insert  */
            row[j-1] = prev;
            prev = val;
        }
        row[lb] = prev;
    }
    int dist = row[lb];
    free(row);
    return dist;
}

/* ── Collect candidate commands from $PATH ───────────────────────── */
static int collect_path_cmds(char candidates[][MAX_CMD_LEN], int max) {
    const char *path_env = getenv("PATH");
    if (!path_env) return 0;

    char path_copy[PATH_MAX];
    strncpy(path_copy, path_env, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    int count = 0;
    char *dir = strtok(path_copy, ":");
    while (dir && count < max) {
        DIR *d = opendir(dir);
        if (!d) { dir = strtok(NULL, ":"); continue; }

        struct dirent *ent;
        while ((ent = readdir(d)) && count < max) {
            if (ent->d_name[0] == '.') continue;
            if (strlen(ent->d_name) >= MAX_CMD_LEN) continue;
            /* Deduplicate naively (good enough for shell suggest) */
            int dup = 0;
            for (int i = 0; i < count; i++) {
                if (strcmp(candidates[i], ent->d_name) == 0) { dup = 1; break; }
            }
            if (!dup) strncpy(candidates[count++], ent->d_name, MAX_CMD_LEN - 1);
        }
        closedir(d);
        dir = strtok(NULL, ":");
    }
    return count;
}

/* ── Public API ──────────────────────────────────────────────────── */
void suggest_command(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;

    static char candidates[MAX_CANDIDATES][MAX_CMD_LEN];
    int n = collect_path_cmds(candidates, MAX_CANDIDATES);

    const char *best      = NULL;
    int         best_dist = INT_MAX;

    for (int i = 0; i < n; i++) {
        int d = levenshtein(cmd, candidates[i]);
        if (d < best_dist) {
            best_dist = d;
            best      = candidates[i];
        }
    }

    if (best && best_dist <= SUGGEST_THRESHOLD) {
        printf("\033[1;33m💡 Did you mean: \033[1;36m%s\033[1;33m?"
               "  (edit distance: %d)\033[0m\n", best, best_dist);
    }
}
