/*
 * Calibration program for TSC filter benchmarking
 * See mythtv/filters/README for more information
 * compile with gcc -o tsc-calibrate tsc-calibrate.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#define TEST_DELAY 16

int main (int argc, char **argv) {
    unsigned long long t1, t2;
    unsigned int t1l, t2l, t1h, t2h;
    struct timeval to = { 0, 0 };
    int delay;

    if ( argc<2 || (delay = atoi(argv[1])) < 1 )
    {
        fprintf (stderr, 
                "\nUsage:\n\n"
                "%s <delay>\n\n"
                "Specify a delay in seconds to calibrate.  A longer delay will provide a more\n"
                "accurate result.  8-32 seconds should be sufficient for most purposes.  Define\n"
                "TIME_FILTER, define TF_TYPE as TSC, and define TF_TSC_TICKS as the reported\n"
                "value when building filters in order to use TSC benchmarks reported in frames\n"
                "per second instead of ticks per frame.\n\n",
                argv[0]);
        exit (1);
    }

    to.tv_sec = delay;
    asm ("rdtsc" :"=a" (t1l), "=d" (t1h));
    select (0,0,0,0, &to);
    asm ("rdtsc" :"=a" (t2l), "=d" (t2h));
    t1 = t1l + ((unsigned long long)t1h<<32);
    t2 = t2l + ((unsigned long long)t2h<<32);
    printf ("TF_TSC_TICKS for this system: %llu\n", (t2-t1+delay/2)/delay);
}
