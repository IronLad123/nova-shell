/*
 * summary.c — Session analytics with elapsed-time tracking
 */

#include <stdio.h>
#include <time.h>
#include "summary.h"

#define CYAN   "\033[1;36m"
#define GREEN  "\033[1;32m"
#define YELLOW "\033[1;33m"
#define RESET  "\033[0m"
#define DIM    "\033[2m"

static int    cmd_count  = 0;
static int    bg_count   = 0;
static int    pipe_count = 0;
static int    err_count  = 0;
static time_t start_time = 0;

void summary_init(void) {
    cmd_count = bg_count = pipe_count = err_count = 0;
    start_time = time(NULL);
}

void summary_command(void)    { cmd_count++;  }
void summary_background(void) { bg_count++;   }
void summary_pipe(void)       { pipe_count++; }
void summary_error(void)      { err_count++;  }

void summary_print(void) {
    time_t end_time = time(NULL);
    double elapsed  = difftime(end_time, start_time);
    int    hrs      = (int)(elapsed / 3600);
    int    mins     = (int)((elapsed - hrs * 3600) / 60);
    int    secs     = (int)(elapsed) % 60;

    printf("\n");
    printf(CYAN "  ╔══════════════════════════════════════╗\n");
    printf(      "  ║     nova-shell  session  summary     ║\n");
    printf(      "  ╠══════════════════════════════════════╣\n" RESET);
    printf(GREEN "  ║" RESET "  Commands executed   " GREEN "%4d" RESET "             " GREEN "║\n" RESET, cmd_count);
    printf(GREEN "  ║" RESET "  Pipelines run       " GREEN "%4d" RESET "             " GREEN "║\n" RESET, pipe_count);
    printf(GREEN "  ║" RESET "  Background jobs     " GREEN "%4d" RESET "             " GREEN "║\n" RESET, bg_count);

    if (err_count > 0)
        printf(YELLOW "  ║" RESET "  Not-found errors    " YELLOW "%4d" RESET "             " YELLOW "║\n" RESET, err_count);
    else
        printf(GREEN  "  ║" RESET "  Not-found errors    " GREEN  "%4d" RESET "             " GREEN  "║\n" RESET, err_count);

    printf(GREEN "  ║" RESET "  Session duration    %02d:%02d:%02d          " GREEN "║\n" RESET,
           hrs, mins, secs);
    printf(CYAN "  ╚══════════════════════════════════════╝\n" RESET);
    printf(DIM   "  Thanks for using nova-shell. See you next time!\n\n" RESET);
}
