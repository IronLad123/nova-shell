#include <string.h>
#include "safety.h"

int is_dangerous_command(char **args) {
    if (!args || !args[0])
        return 0;

    if (strcmp(args[0], "rm") == 0 && args[1] && strcmp(args[1], "-rf") == 0)
        return 1;

    if (strcmp(args[0], "shutdown") == 0)
        return 1;

    if (strcmp(args[0], "reboot") == 0)
        return 1;

    return 0;
}
