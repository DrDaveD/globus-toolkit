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

#ifndef GLOBUS_I_PREWS_GRAM_THROUGHPUT_TEST_H_
#define GLOBUS_I_PREWS_GRAM_THROUGHPUT_TEST_H_

#include "globus_common.h"
#include "globus_gram_protocol.h"
#include "globus_gram_client.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>



#define GlobusLErrorWrapResult(doing__, result__)                           \
    (globus_error_put(                                                      \
        globus_error_construct_error(                                       \
            NULL,                                                           \
            globus_error_get(result__),                                     \
            0, __FILE__, _globus_func_name, __LINE__,                       \
            "Error %s", (doing__))))



typedef struct
{
    /* parsed options */
    globus_bool_t     help;

    char *            resource_manager;

    int               job_duration;
    int               load;
    int               num_threads;
    int               test_duration;

} globus_i_info_t;


typedef struct
{
    globus_mutex_t                      mutex;
    globus_cond_t                       cond;
    globus_list_t *                     job_list;
} globus_i_client_thread_t;


struct test_monitor_s
{
    globus_mutex_t                      mutex;
    globus_cond_t                       cond;
    globus_bool_t                       done;
    int                                 active_threads;
};


static void
globus_l_interrupt_cb(
    void *   user_arg);

globus_result_t
globus_l_submit_job(
    const char *                        callback_contact,
    const char *                        resource_manager,
    int                                 job_duration,
    char **                             job_contact);

void
globus_i_parse_arguments(
    int                                 argc,
    char **                             argv,
    globus_i_info_t *                   info);

void
globus_l_test_duration_timeout(
    void *                              user_arg);

void
globus_l_client_thread(
    void *                              user_arg);

void
globus_i_print_error(
    globus_result_t                     result);

void
globus_i_print_warning(
    globus_result_t                     result);

void
globus_i_stats_start();

void
globus_i_stats_finish();

void
globus_i_stats_job_started();

void
globus_i_stats_job_failed();

void
globus_i_stats_job_succeeded();

void
globus_i_stats_brief_summary();

void
globus_i_stats_summary(int num_threads, int load);

#endif

