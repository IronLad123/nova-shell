#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "history.h"

#define MAX_HISTORY 100

static char* history[MAX_HISTORY];
static int history_count = 0;

void history_init() {
    for (int i = 0; i < MAX_HISTORY; i++)
        history[i] = NULL;
    history_count = 0;
}

void history_add(const char* cmd) {
    if (!cmd || strlen(cmd) == 0)
        return;

    if (history_count == MAX_HISTORY) {
        free(history[0]);
        for (int i = 1; i < MAX_HISTORY; i++)
            history[i - 1] = history[i];
        history_count--;
    }

    history[history_count++] = strdup(cmd);
}

void history_print() {
    printf("\nCommand History:\n");
    printf("================\n");
    for (int i = 0; i < history_count; i++)
        printf("%2d: %s\n", i + 1, history[i]);
}
