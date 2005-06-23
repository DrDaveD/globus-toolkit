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

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gram_job_manager_request.c Globus Job Management Request
 *
 * CVS Information:
 * $Source$
 * $Date$
 * $Revision$
 * $Author$
 */
#endif

/*
 * Include header files
 */
#include "globus_common.h"
#include "globus_gram_protocol.h"
#include "globus_gram_job_manager.h"

#include <string.h>

/**
 * Allocate and initialize a request.
 *
 * This function allocates a new request structure and clears all of the
 * values in the structure. It also creates a script argument file which
 * will be used when the job request is submitted.
 *
 * @param request
 *        A pointer to a globus_gram_jobmanager_request_t pointer. This
 *        will be modified to point to a freshly allocated request structure.
 *
 * @return GLOBUS_SUCCESS on successfully initialization, or GLOBUS_FAILURE.
 */
int 
globus_gram_job_manager_request_init(
    globus_gram_jobmanager_request_t **	request)
{
    globus_gram_jobmanager_request_t * r;

    /*** creating request structure ***/
    *request = (globus_gram_jobmanager_request_t * ) globus_libc_calloc
                   (1, sizeof(globus_gram_jobmanager_request_t));

    r = *request;

    r->failure_code = 0;
    r->job_id = NULL;
    r->poll_frequency = 30;
    r->jobmanager_type = NULL;
    r->jobmanager_logfile = NULL;
    r->jobmanager_log_fp = NULL;
    r->local_stdout = NULL;
    r->local_stderr = NULL;
    r->condor_os = NULL;
    r->condor_arch = NULL;
    r->status = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
	r->status_update_time = 0;
    r->url_base = GLOBUS_NULL;
    r->job_contact = GLOBUS_NULL;
    r->job_contact_path = GLOBUS_NULL;
    r->old_job_contact = GLOBUS_NULL;
    r->two_phase_commit = GLOBUS_FALSE;
    r->save_state = GLOBUS_FALSE;
    r->jm_restart = NULL;
    r->scratchdir = GLOBUS_NULL;
    r->scratch_dir_base = GLOBUS_NULL;
    r->in_handler = GLOBUS_FALSE;
    r->validation_records = NULL;
    r->relocated_proxy = GLOBUS_FALSE;
    r->proxy_timeout = 60;
    r->cache_tag = GLOBUS_NULL;
    r->job_state_file_dir = GLOBUS_NULL;
    r->job_state_file = GLOBUS_NULL;
    r->job_state_lock_file = GLOBUS_NULL;
    r->job_state_lock_fd = -1;
    r->stdout_position_hack = GLOBUS_NULL;
    r->stderr_position_hack = GLOBUS_NULL;
    globus_fifo_init(&r->pending_queries);
    globus_gram_job_manager_output_init(r);
    globus_mutex_init(&r->mutex, GLOBUS_NULL);
    globus_cond_init(&r->cond, GLOBUS_NULL);
    r->extra_envvars = GLOBUS_NULL;
    r->response_context = GSS_C_NO_CONTEXT;

    r->seg_module = NULL;
    r->seg_started = GLOBUS_FALSE;
    r->seg_last_timestamp = 0;
    globus_fifo_init(&r->seg_event_queue);
    
    return(GLOBUS_SUCCESS);

}
/* globus_gram_job_manager_request_init() */

/**
 * Deallocate memory related to a request.
 *
 * This function frees the data within the request, and then frees the request.
 * The caller must not access the request after this function has returned.
 *
 * @param request
 *        Job request to destroy.
 *
 * @return GLOBUS_SUCCESS
 */
int 
globus_gram_job_manager_request_destroy(
    globus_gram_jobmanager_request_t *	request)
{
    OM_uint32                           minor_status;
    if (!request)
        return(GLOBUS_FAILURE);

    globus_mutex_destroy(&request->mutex);
    globus_cond_destroy(&request->cond);

    if (request->job_id)
        globus_libc_free(request->job_id);
    if (request->jobmanager_type)
        globus_libc_free(request->jobmanager_type);
    if (request->jobmanager_logfile)
        globus_libc_free(request->jobmanager_logfile);
    if (request->local_stdout)
        globus_libc_free(request->local_stdout);
    if (request->local_stderr)
        globus_libc_free(request->local_stderr);
    if (request->cache_tag)
	globus_libc_free(request->cache_tag);
    if (request->url_base)
	globus_libc_free(request->url_base);
    if (request->job_contact)
	globus_libc_free(request->job_contact);
    if (request->job_contact_path)
	globus_libc_free(request->job_contact_path);
    if (request->old_job_contact)
	globus_libc_free(request->old_job_contact);
    if (request->job_state_file_dir)
	globus_libc_free(request->job_state_file_dir);
    if (request->job_state_file)
	globus_libc_free(request->job_state_file);
    if (request->job_state_lock_file)
	globus_libc_free(request->job_state_lock_file);
    if (request->extra_envvars)
        globus_libc_free(request->extra_envvars);
    if (request->response_context == GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&minor_status,
                               &request->response_context,
                               NULL);

    globus_libc_free(request);

    return(GLOBUS_SUCCESS);

}
/* globus_gram_job_manager_request_destroy() */

/**
 * Change the status associated with a job request
 *
 * Changes the status associated with a job request.
 * There is now additional tracking data associated with the
 * status that must be updated when the status is.  This function
 * handles managing it.  It is NOT recommended that you directly
 * change the status.
 *
 * @param request
 *        Job request to change status of.
 * @param status
 *        Status to set the job request to.
 *
 * @return GLOBUS_SUCCESS assuming valid input.
 *         If the request is null, returns GLOBUS_FAILURE.
 */
int
globus_gram_job_manager_request_set_status(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_protocol_job_state_t	status)
{
    return globus_gram_job_manager_request_set_status_time(request, status,
		time(0));
}
/* globus_gram_job_manager_request_set_status() */


/**
 * Change the status associated with a job request
 *
 * Changes the status associated with a job request.
 * There is now additional tracking data associated with the
 * status that must be updated when the status is.  This function
 * handles managing it.  It is NOT recommended that you directly
 * change the status.
 *
 * @param request
 *        Job request to change status of.
 * @param status
 *        Status to set the job request to.
 * @param valid_time
 *        The status is known good as of this time (seconds since epoch)
 *
 * @return GLOBUS_SUCCESS assuming valid input.
 *         If the request is null, returns GLOBUS_FAILURE.
 */
int
globus_gram_job_manager_request_set_status_time(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_protocol_job_state_t	status,
    time_t valid_time)
{
    if( ! request )
        return GLOBUS_FAILURE;
    request->status = status;
    request->status_update_time = valid_time;
    return GLOBUS_SUCCESS;
}
/* globus_gram_job_manager_request_set_status() */

extern
void
globus_gram_job_manager_request_open_logfile(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_logfile_flag_t
    					logfile_flag)
{
    if (logfile_flag == GLOBUS_GRAM_JOB_MANAGER_DONT_SAVE)
    {
        /* don't write a log file */
        request->jobmanager_logfile = globus_libc_strdup("/dev/null");
    }
    else
    {
        /*
         * Open the gram logfile just for testing!
         */
	request->jobmanager_logfile =
	    globus_libc_malloc(strlen("%s/gram_job_mgr_%lu.log") +
		                      strlen(request->home) +
				      16);

        sprintf(request->jobmanager_logfile, "%s/gram_job_mgr_%lu.log",
                request->home,
                (unsigned long) getpid());

        request->jobmanager_log_fp = fopen(request->jobmanager_logfile, "a");
	
	if(request->jobmanager_log_fp == NULL)
        {
            sprintf(request->jobmanager_logfile,
		    "/tmp/gram_job_mgr_%lu.log",
                    (unsigned long) getpid());

            request->jobmanager_log_fp =
		fopen(request->jobmanager_logfile, "a");

	    if(request->jobmanager_log_fp == NULL)
            {
                sprintf(request->jobmanager_logfile, "/dev/null");
            }
        }
    }

    if (!request->jobmanager_log_fp)
    {
	request->jobmanager_log_fp = fopen(request->jobmanager_logfile, "w");
    }

    if(request->jobmanager_log_fp)
    {
        int fd;

	setbuf(request->jobmanager_log_fp, NULL);

        fd = fileno(request->jobmanager_log_fp);

        while(fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
        {
            if(errno != EINTR)
            {
                break;
            }
        }
    }
}
/* globus_gram_job_manager_request_open_logfile() */

/**
 * Write data to the job manager log file
 *
 * This function writes data to the passed file, using a printf format
 * string. Data is prefixed with a timestamp when written.
 *
 * @param log_fp
 *        Log file to write to.
 * @param format
 *        Printf-style format string to be written.
 * @param ...
 *        Parameters substituted into the format string, if needed.
 *
 * @return This function returns the value returned by vfprintf.
 */
int
globus_gram_job_manager_request_log(
    globus_gram_jobmanager_request_t *	request,
    const char *			format,
    ... )
{
    struct tm *curr_tm;
    time_t curr_time;
    va_list ap;
    int rc;

    if ( request->jobmanager_log_fp == GLOBUS_NULL ) {
	return -1;
    }

    time( &curr_time );
    curr_tm = localtime( &curr_time );

    globus_libc_lock();

    fprintf( request->jobmanager_log_fp,
	     "%d/%d %02d:%02d:%02d ",
	     curr_tm->tm_mon + 1, curr_tm->tm_mday,
	     curr_tm->tm_hour, curr_tm->tm_min,
	     curr_tm->tm_sec );

    va_start(ap, format);

    rc = vfprintf( request->jobmanager_log_fp,
	           format,
		   ap);

    globus_libc_unlock();

    return rc;
}
/* globus_gram_job_manager_request_log() */
