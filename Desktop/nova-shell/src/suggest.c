#include <stdio.h>
#include <string.h>
#include "suggest.h"

static const char *known_cmds[] = {
    "ls", "cd", "pwd", "cat", "echo",
    "gcc", "make", "ps", "kill",
    "clear", "exit", "history", NULL
};

void suggest_command(char *cmd) {
    if (!cmd) return;

    for (int i = 0; known_cmds[i]; i++) {
        if (known_cmds[i][0] == cmd[0]) {
            printf("💡 Did you mean: %s ?\n", known_cmds[i]);
            return;
        }
    }
}
