#include <stdio.h>
#include "summary.h"

static int commands = 0;
static int background_jobs = 0;
static int pipes = 0;

void summary_init() {
    commands = background_jobs = pipes = 0;
}

void summary_command() {
    commands++;
}

void summary_background() {
    background_jobs++;
}

void summary_pipe() {
    pipes++;
}

void summary_print() {
    printf("\nSession Summary:\n");
    printf("================\n");
    printf("Commands run   : %d\n", commands);
    printf("Background jobs: %d\n", background_jobs);
    printf("Pipes used     : %d\n", pipes);
}
