#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "monitor.h"

// store start time
static struct timeval start_time;

// helper: draw ascii bar
void draw_bar(double percent) {
    int bars = (int)(percent / 10.0); // 10 blocks
    printf("[");
    for (int i = 0; i < 10; i++) {
        if (i < bars)
            printf("█");
        else
            printf("░");
    }
    printf("] %.0f%%\n", percent);
}

void monitor_start() {
    gettimeofday(&start_time, NULL);
}

void monitor_end() {
    struct timeval end;
    struct rusage usage;

    gettimeofday(&end, NULL);
    getrusage(RUSAGE_CHILDREN, &usage);

    // time
    double elapsed =
        (end.tv_sec - start_time.tv_sec) +
        (end.tv_usec - start_time.tv_usec) / 1000000.0;

    // cpu
    double cpu_time =
        usage.ru_utime.tv_sec +
        usage.ru_utime.tv_usec / 1000000.0 +
        usage.ru_stime.tv_sec +
        usage.ru_stime.tv_usec / 1000000.0;

    double cpu_percent = (elapsed > 0)
        ? (cpu_time / elapsed) * 100.0
        : 0.0;

    if (cpu_percent > 100.0)
        cpu_percent = 100.0;

    // memory (approximate)
    double mem_kb = usage.ru_maxrss;
    double mem_percent = (mem_kb / 102400.0) * 100.0; // assume 100MB scale
    if (mem_percent > 100.0)
        mem_percent = 100.0;

    // ---- OUTPUT ----
    printf("⏱ Time: %.2f s\n", elapsed);

    printf("CPU : ");
    draw_bar(cpu_percent);

    printf("MEM : ");
    draw_bar(mem_percent);

    // performance badge
    if (elapsed < 0.5)
        printf("🏅 Performance: FAST\n");
    else if (elapsed < 2.0)
        printf("🏅 Performance: NORMAL\n");
    else
        printf("🏅 Performance: SLOW\n");

    // resource hog warning
    if (cpu_percent > 80.0 || mem_percent > 70.0)
        printf("🔥 Resource Hog Detected!\n");
}
