/*
 * Portions of this file Copyright 1999-2005 University of Chicago
 * Portions of this file Copyright 1999-2005 The University of Southern California.
 *
 * This file or a portion of this file is licensed under the
 * terms of the Globus Toolkit Public License, found at
 * http://www.globus.org/toolkit/download/license.html.
 * If you redistribute this file, with or without
 * modifications, you must include this notice in the file.
 */

#include "globus_i_prews_gram_throughput_test.h"


struct stats_monitor_s
{
    globus_mutex_t                      mutex;
    globus_cond_t                       cond;
} stats_monitor;

/* so we can ignore jobs checking in after the deadline */
globus_bool_t stats_running = 0;

long stats_total_count      = 0;
long stats_succeeded_count  = 0;
long stats_failed_count     = 0;

struct timeval stats_time_started;
struct timeval stats_time_finished;


void
globus_i_stats_start()
{
    gettimeofday(&stats_time_started, NULL);

    stats_running = 1;
}


void
globus_i_stats_finish()
{
    stats_running = 0;
    
    gettimeofday(&stats_time_finished, NULL);
}


void
globus_i_stats_job_started()
{
    if (!stats_running)
    {
        return;
    }

    globus_mutex_lock(&stats_monitor.mutex);
    {
        stats_total_count++;
    }
    globus_mutex_unlock(&stats_monitor.mutex);
}


void
globus_i_stats_job_failed()
{
    if (!stats_running)
    {
        return;
    }

    globus_mutex_lock(&stats_monitor.mutex);
    {
        stats_failed_count++;
    }
    globus_mutex_unlock(&stats_monitor.mutex);
    globus_i_stats_brief_summary();

}


void
globus_i_stats_job_succeeded()
{
    if (!stats_running)
    {
        return;
    }

    globus_mutex_lock(&stats_monitor.mutex);
    {
        stats_succeeded_count++;
    }
    globus_mutex_unlock(&stats_monitor.mutex);
    globus_i_stats_brief_summary();
}


void
globus_i_stats_brief_summary()
{
    globus_mutex_lock(&stats_monitor.mutex);
    {
        printf("   Completed: %3.1ld   Running: %3.1ld   Succeeded: %3.1ld   "
               "Failed: %3.1ld\n",
               stats_succeeded_count + stats_failed_count,
               stats_total_count - stats_succeeded_count + stats_failed_count,
               stats_succeeded_count,
               stats_failed_count);
    }
    globus_mutex_unlock(&stats_monitor.mutex);
}


void
globus_i_stats_summary(int num_threads, int load)
{
    time_t secs  = 0;
    time_t usecs = 0;
    float run_time;

    usecs = stats_time_finished.tv_usec - stats_time_started.tv_usec;
    if (usecs < 0)
    {
        secs = -1;
        usecs += 1000000;
    }
    secs = secs + stats_time_finished.tv_sec - stats_time_started.tv_sec;

    /* now the time values are small enough to be combined */
    run_time = secs + ((float)usecs / 1000000);

    printf("\nSummary:\n\n");
    printf("    Duration:        %7.2lf seconds\n", run_time);
    printf("    Num Threads (M):    %4.1d\n", num_threads);
    printf("    Load (N):           %4.1d\n", load);

    globus_mutex_lock(&stats_monitor.mutex);
    {
        printf("    Completed:           %3.1ld\n",
               stats_succeeded_count + stats_failed_count);
        printf("       Succeeded:        %3.1ld\n",
               stats_succeeded_count);
        printf("       Failed:           %3.1ld\n",
               stats_failed_count);
        printf("    Jobs per minute: %7.1lf\n",
               (stats_succeeded_count + stats_failed_count) /
               (run_time / 60));
    }
    globus_mutex_unlock(&stats_monitor.mutex);

    printf("\n");
}


