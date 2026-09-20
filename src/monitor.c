/*
 * monitor.c — Real-time resource telemetry
 *
 * Tracks wall-clock time (gettimeofday), child CPU usage (getrusage),
 * and peak RSS.  Displays a coloured ASCII progress bar and a
 * performance badge for every command.
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "monitor.h"

#define GREEN  "\033[1;32m"
#define YELLOW "\033[1;33m"
#define RED    "\033[1;31m"
#define CYAN   "\033[1;36m"
#define DIM    "\033[2m"
#define RESET  "\033[0m"

static struct timeval t_start;

/* ── Draw a 20-cell coloured bar ────────────────────────────────── */
static void draw_bar(double pct, const char *color) {
    int filled = (int)(pct / 5.0);   /* 20 cells → 5% each */
    if (filled > 20) filled = 20;
    printf("%s[", color);
    for (int i = 0; i < 20; i++)
        printf(i < filled ? "█" : "░");
    printf("] %5.1f%%\033[0m", pct);
}

void monitor_start(void) {
    gettimeofday(&t_start, NULL);
}

void monitor_end(void) {
    struct timeval t_end;
    struct rusage  ru;

    gettimeofday(&t_end, NULL);
    getrusage(RUSAGE_CHILDREN, &ru);

    double elapsed =
        (t_end.tv_sec  - t_start.tv_sec) +
        (t_end.tv_usec - t_start.tv_usec) / 1e6;

    double cpu_sec =
        ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 +
        ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;

    double cpu_pct = (elapsed > 0.0) ? (cpu_sec / elapsed) * 100.0 : 0.0;
    if (cpu_pct > 100.0) cpu_pct = 100.0;

    /* ru_maxrss: bytes on Linux, kilobytes on macOS */
#ifdef __APPLE__
    double mem_mb = ru.ru_maxrss / (1024.0 * 1024.0);
#else
    double mem_mb = ru.ru_maxrss / 1024.0;
#endif
    double mem_pct = (mem_mb / 512.0) * 100.0;  /* scale: assume 512 MB max */
    if (mem_pct > 100.0) mem_pct = 100.0;

    /* ── Performance badge ────────────────────────────────── */
    const char *badge_color;
    const char *badge;
    if (elapsed < 0.1) {
        badge_color = GREEN;  badge = "⚡ FAST  ";
    } else if (elapsed < 1.0) {
        badge_color = YELLOW; badge = "🟡 NORMAL";
    } else {
        badge_color = RED;    badge = "🐢 SLOW  ";
    }

    printf(DIM "  ┌─ " RESET CYAN "telemetry\n" RESET);
    printf(DIM "  │ " RESET "time   %s%.3fs%s\n", CYAN, elapsed, RESET);
    printf(DIM "  │ " RESET "cpu    "); draw_bar(cpu_pct,  GREEN);  printf("\n");
    printf(DIM "  │ " RESET "mem    "); draw_bar(mem_pct,  CYAN);   printf("  (%.1f MB)\n", mem_mb);
    printf(DIM "  └─ " RESET "%s%s\033[0m\n", badge_color, badge);

    if (cpu_pct > 85.0 || mem_mb > 400.0)
        printf(RED "  ⚠  Resource hog detected — consider background execution (&)\n" RESET);
}
