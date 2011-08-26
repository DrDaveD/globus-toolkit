/*
 * Copyright 1999-2009 University of Chicago
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "globus_gram_job_manager.h"

#include <string.h>


/**
 * Compute the name of the state file to use for this job request.
 *
 * Sets the value of the @a job_state_file member of the request structure.
 *
 * @param request
 *     The request to create the state file for.
 * @param state_file
 *     Pointer to set to the state file string. The caller is responsible for
 *     freeing this.
 * @param state_lock_file
 *     Pointer to set to the state file lockfile string. The caller is
 *     responsible for freeing this.
 *
 * @retval GLOBUS_SUCCESS
 *     Success.
 * @retval GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED
 *     Malloc failed.
 */
int
globus_gram_job_manager_state_file_set(
    globus_gram_jobmanager_request_t *  request,
    char **                             state_file,
    char **                             state_lock_file)
{
    int                                 rc = GLOBUS_SUCCESS;

    if(request->config->job_state_file_dir == GLOBUS_NULL)
    {
        *state_file = globus_common_create_string(
                "%s/tmp/gram_job_state/%s.%s.%s",
                request->config->globus_location ?
                request->config->globus_location : "",
                request->config->logname,
                request->config->hostname,
                request->uniq_id);
    }
    else
    {
        *state_file = globus_common_create_string(
                "%s/job.%s.%s",
                request->config->job_state_file_dir,
                request->config->hostname,
                request->uniq_id);
    }

    if (*state_file == NULL)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;

        goto create_state_file_failed;
    }

    *state_lock_file = globus_common_create_string(
            "%s.lock",
            *state_file);

    if (*state_lock_file == NULL)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;

        goto create_state_lock_file_failed;
    }

    if (rc != GLOBUS_SUCCESS)
    {
create_state_lock_file_failed:
        free(*state_file);
        *state_file = NULL;
    }
create_state_file_failed:
    return rc;
}
/* globus_gram_job_manager_state_file_set() */

int
globus_l_gram_state_file_create_lock(
    globus_gram_jobmanager_request_t *  request)
{
    int                                 rc = GLOBUS_SUCCESS;

    globus_gram_job_manager_request_log(
            request,
            GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
            "event=gram.state_file.create_lock.start "
            "level=TRACE "
            "gramid=%s "
            "path=\"%s\" "
            "\n",
            request->job_contact_path,
            request->job_state_lock_file);

    if (request->manager->lock_fd == -1)
    {
        /* We are not in single job manager mode */
        request->job_state_lock_fd = open( request->job_state_lock_file,
                                           O_RDWR | O_CREAT,
                                           S_IRUSR | S_IWUSR );
        if ( request->job_state_lock_fd < 0 )
        {
            rc = GLOBUS_GRAM_PROTOCOL_ERROR_LOCKING_STATE_LOCK_FILE;

            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                    "event=gram.state_file.create_lock.end "
                    "level=ERROR "
                    "gramid=%s "
                    "path=\"%s\" "
                    "status=%d "
                    "msg=\"%s\" "
                    "errno=%d "
                    "reason=\"%s\" "
                    "\n",
                    request->job_contact_path,
                    request->job_state_lock_file,
                    -rc,
                    "Error opening lock file",
                    errno,
                    strerror(errno));

            goto open_lock_file_failed;
        }

        rc = globus_gram_job_manager_file_lock(request->job_state_lock_fd);
        if ( rc != GLOBUS_SUCCESS )
        {
            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                    "event=gram.state_file.create_lock.end "
                    "level=ERROR "
                    "gramid=%s "
                    "path=\"%s\" "
                    "status=%d "
                    "msg=\"%s\" "
                    "errno=%d "
                    "reason=\"%s\" "
                    "\n",
                    request->job_contact_path,
                    request->job_state_lock_file,
                    -rc,
                    "Error locking state file",
                    errno,
                    strerror(errno));
            close(request->job_state_lock_fd);
            remove(request->job_state_lock_file);
            goto lock_file_failed;
        }
    }
    else
    {
        (void) unlink(request->job_state_lock_file);
        rc = symlink(request->manager->lock_path, request->job_state_lock_file);
        if (rc != GLOBUS_SUCCESS)
        {
            rc = GLOBUS_GRAM_PROTOCOL_ERROR_LOCKING_STATE_LOCK_FILE;

            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                    "event=gram.state_file.create_lock.end "
                    "level=ERROR "
                    "gramid=%s "
                    "path=\"%s\" "
                    "manager_lock_path=%s "
                    "status=%d "
                    "msg=\"%s\" "
                    "errno=%d "
                    "reason=\"%s\" "
                    "\n",
                    request->job_contact_path,
                    request->job_state_lock_file,
                    request->manager->lock_path,
                    -rc,
                    "Error symlinking lock file",
                    errno,
                    strerror(errno));
            goto link_failed;
        }
    }

open_lock_file_failed:
lock_file_failed:
link_failed:
    if (rc == GLOBUS_SUCCESS)
    {
        globus_gram_job_manager_request_log(
                request,
                GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
                "event=gram.state_file.create_lock.end "
                "level=TRACE "
                "gramid=%s "
                "path=\"%s\" "
                "status=%d "
                "\n",
                request->job_contact_path,
                request->job_state_lock_file,
                rc);
    }
    return rc;
}
/* globus_gram_job_manager_state_file_create_lock() */

int
globus_gram_job_manager_state_file_write(
    globus_gram_jobmanager_request_t *  request)
{
    int                                 rc = GLOBUS_SUCCESS;
    FILE *                              fp = NULL;
    char                                tmp_file[1024] = { 0 };

    globus_gram_job_manager_request_log(
            request,
            GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
            "event=gram.write_state_file.start "
            "level=TRACE "
            "gramid=%s "
            "path=\"%s\" "
            "\n",
            request->job_contact_path,
            request->job_state_file);

    rc = globus_l_gram_state_file_create_lock(request);
    if (rc != GLOBUS_SUCCESS)
    {
        globus_gram_job_manager_request_log(
                request,
                GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                "event=gram.write_state_file.end "
                "level=ERROR "
                "gramid=%s "
                "path=\"%s\" "
                "msg=\"%s\" "
                "\n",
                request->job_contact_path,
                request->job_state_file,
                "Error creating lock file");
        goto error_exit;
    }

    /*
     * We want the file update to be atomic, so create a new temp file,
     * write the new information, close the new file, then rename the new
     * file on top of the old one. The rename is the atomic update action.
     */
    strcpy( tmp_file, request->job_state_file );
    strcat( tmp_file, ".tmp" );

    fp = fopen( tmp_file, "w" );
    if ( fp == NULL )
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_WRITING_STATE_FILE;

        globus_gram_job_manager_request_log(
                request,
                GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                "event=gram.write_state_file.end "
                "level=ERROR "
                "gramid=%s "
                "path=\"%s\" "
                "status=%d "
                "msg=\"%s\" "
                "errno=%d "
                "reason=\"%s\"\n",
                request->job_contact_path,
                tmp_file,
                -rc,
                "Error opening state file",
                errno,
                strerror(errno));

        return rc;
    }

    rc = fprintf(fp, "%s\n", request->job_contact ? request->job_contact : " ");
    if (rc < 0)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_WRITING_STATE_FILE;

        goto error_exit;
    }
    rc = fprintf(fp, "%4d\n",
            (request->jobmanager_state == GLOBUS_GRAM_JOB_MANAGER_STATE_STOP)
                    ? (int) request->restart_state
                    : (int) request->jobmanager_state);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%4d\n", (int) request->status);
    if (rc < 0)
    {
        goto error_exit;
    }

    rc = fprintf(fp, "%4d\n", request->failure_code);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%s\n", request->job_id_string ? request->job_id_string : " ");
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%s\n", request->rsl_spec);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%s\n", request->cache_tag);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%s\n", request->config->jobmanager_type);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%d\n", request->two_phase_commit);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%s\n", request->scratchdir ? request->scratchdir : " ");
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%lu\n",
                 (unsigned long) request->seg_last_timestamp);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%lu\n", (unsigned long) request->creation_time);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%lu\n", (unsigned long) request->queued_time);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = globus_gram_job_manager_staging_write_state(request, fp);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = globus_gram_job_manager_write_callback_contacts(request, fp);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%s\n", request->gateway_user ? request->gateway_user : " ");
    if (rc < 0)
    {
        goto error_exit;
    }

    rc = fprintf(fp, "%d\n", request->exit_code);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp,
            "%ld.%09ld %ld.%09ld %ld.%09ld "
            "%ld.%09ld %ld.%09ld %ld.%09ld %ld.%09ld "
            "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
            request->job_stats.unsubmitted_timestamp.tv_sec,
            request->job_stats.unsubmitted_timestamp.tv_nsec,
            request->job_stats.file_stage_in_timestamp.tv_sec,
            request->job_stats.file_stage_in_timestamp.tv_nsec,
            request->job_stats.pending_timestamp.tv_sec,
            request->job_stats.pending_timestamp.tv_nsec,
            request->job_stats.active_timestamp.tv_sec,
            request->job_stats.active_timestamp.tv_nsec,
            request->job_stats.failed_timestamp.tv_sec,
            request->job_stats.failed_timestamp.tv_nsec,
            request->job_stats.file_stage_out_timestamp.tv_sec,
            request->job_stats.file_stage_out_timestamp.tv_nsec,
            request->job_stats.done_timestamp.tv_sec,
            request->job_stats.done_timestamp.tv_nsec,
            request->job_stats.restart_count,
            request->job_stats.callback_count,
            request->job_stats.status_count,
            request->job_stats.register_count,
            request->job_stats.unregister_count,
            request->job_stats.signal_count,
            request->job_stats.refresh_count,
            request->job_stats.file_clean_up_count,
            request->job_stats.file_stage_in_http_count,
            request->job_stats.file_stage_in_https_count,
            request->job_stats.file_stage_in_ftp_count,
            request->job_stats.file_stage_in_gsiftp_count,
            request->job_stats.file_stage_in_shared_http_count,
            request->job_stats.file_stage_in_shared_https_count,
            request->job_stats.file_stage_in_shared_ftp_count,
            request->job_stats.file_stage_in_shared_gsiftp_count,
            request->job_stats.file_stage_out_http_count,
            request->job_stats.file_stage_out_https_count,
            request->job_stats.file_stage_out_ftp_count,
            request->job_stats.file_stage_out_gsiftp_count);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp,
            "%s\n%s\n",
            request->job_stats.client_address
                ?  request->job_stats.client_address
                : " ",
            request->job_stats.user_dn
                ? request->job_stats.user_dn
                : " ");
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%4d\n", (int) request->expected_terminal_state);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%s\n", request->config->service_tag);
    if (rc < 0)
    {
        goto error_exit;
    }
    rc = fprintf(fp, "%s\n",
                request->original_job_id_string
                ? request->original_job_id_string
                : "");
    if (rc < 0)
    {
        goto error_exit;
    }

    rc = fprintf(fp, "%d\n",
                request->job_log_level);
    if (rc < 0)
    {
        goto error_exit;
    }



    /*
     * On some filsystems, write + rename is *not* atomic, so we explicitly
     * flush to disk here. fdatasync might be better, but only on systems with
     * POSIX realtime extensions
     */
    fflush(fp);
    fsync(fileno(fp));
    fclose( fp );
    fp = NULL;

    rc = rename( tmp_file, request->job_state_file );
    if (rc != 0)
    {
        rc = GLOBUS_FAILURE;

        globus_gram_job_manager_request_log(
                request,
                GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                "event=gram.write_state_file.end "
                "level=ERROR "
                "gramid=%s "
                "path=%s "
                "status=-1 "
                "msg=\"%s\" "
                "errno=%d "
                "reason=\"%s\" "
                "\n",
                request->job_contact_path,
                request->job_state_file,
                "Error renaming temporary state file",
                errno,
                strerror(errno));
        goto rename_failed;
    }

    globus_gram_job_manager_request_log(
            request,
            GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
            "event=gram.write_state_file.end "
            "level=TRACE "
            "gramid=%s "
            "path=%s "
            "status=0 "
            "\n",
            request->job_contact_path,
            request->job_state_file);

    return GLOBUS_SUCCESS;

error_exit:
    globus_gram_job_manager_request_log(
            request,
            GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
            "event=gram.write_state_file.end "
            "level=ERROR "
            "gramid=%s "
            "path=\"%s\" "
            "status=%d "
            "msg=\"%s\"\n",
            request->job_contact_path,
            tmp_file,
            rc,
            "Error writing to state file");

    if (fp)
    {
        fclose(fp);
    }
rename_failed:
    if (tmp_file[0] != 0)
    {
        remove(tmp_file);
    }

    return GLOBUS_FAILURE;
}
/* globus_gram_job_manager_state_file_write() */

int
globus_gram_job_manager_state_file_read(
    globus_gram_jobmanager_request_t *  request)
{
    FILE *                              fp;
    char *                              buffer = NULL;
    size_t                              file_len;
    struct stat                         statbuf;
    int                                 rc;
    int                                 i;
    unsigned long                       tmp_timestamp;
    struct stat                         single_lock_stat, job_lock_stat;

    request->old_job_contact = NULL;

    globus_gram_job_manager_request_log(
            request,
            GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
            "event=gram.state_file_read.start "
            "level=TRACE "
            "gramid=%s "
            "path=%s "
            "\n",
            request->job_contact_path,
            request->job_state_file);

    if (stat(request->job_state_file, &statbuf) != 0)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE;

        globus_gram_job_manager_request_log(
                request,
                GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                "event=gram.state_file_read.end "
                "level=ERROR "
                "gramid=%s "
                "path=%s "
                "msg=\"%s\" "
                "status=%d "
                "errno=%d "
                "reason=\"%s\" "
                "\n",
                request->job_contact_path,
                request->job_state_file,
                "Error checking file status",
                -rc,
                errno,
                strerror(errno));

        return rc;
    }
    if (statbuf.st_uid != getuid())
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE;

        globus_gram_job_manager_request_log(
                request,
                GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                "event=gram.state_file_read.end "
                "level=ERROR "
                "gramid=%s "
                "path=%s "
                "msg=\"%s\" "
                "status=%d "
                "errno=%d "
                "reason=\"%s\" "
                "\n",
                request->job_contact_path,
                request->job_state_file,
                "State file not owned by me",
                -rc,
                errno,
                strerror(errno));

        return rc;
    }
    file_len = (size_t) statbuf.st_size;
    buffer = malloc(file_len+1);
    if (buffer == NULL)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;

        globus_gram_job_manager_request_log(
                request,
                GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                "event=gram.state_file_read.end "
                "level=ERROR "
                "gramid=%s "
                "path=%s "
                "msg=\"%s\" "
                "status=%d "
                "errno=%d "
                "reason=\"%s\" "
                "\n",
                request->job_contact_path,
                request->job_state_file,
                "Malloc failed",
                -rc,
                errno,
                strerror(errno));
        goto exit;
    }

    /* Try to obtain a lock on the state lock file */
    if ( request->job_state_lock_file != NULL &&
         request->job_state_lock_fd < 0)
    {
        rc = fstat(request->manager->lock_fd, &single_lock_stat);
        if (rc < 0)
        {
            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_DEBUG,
                    "event=gram.state_file_read.info "
                    "level=DEBUG "
                    "gramid=%s "
                    "path=%s "
                    "fd=%d"
                    "msg=\"%s\" "
                    "errno=%d "
                    "reason=\"%s\" "
                    "\n",
                    request->job_contact_path,
                    request->job_state_file,
                    request->manager->lock_fd,
                    "Unable to check status of single lock file",
                    errno,
                    strerror(errno));
            goto skip_single_check;
        }
        rc = stat(request->job_state_lock_file, &job_lock_stat);
        if (rc < 0)
        {
            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_DEBUG,
                    "event=gram.state_file_read.info "
                    "level=DEBUG "
                    "gramid=%s "
                    "path=%s "
                    "msg=\"%s\" "
                    "errno=%d "
                    "reason=\"%s\" "
                    "\n",
                    request->job_contact_path,
                    request->job_state_file,
                    "Unable to check status of job lock file",
                    errno,
                    strerror(errno));
            goto skip_single_check;
        }

        if (single_lock_stat.st_dev == job_lock_stat.st_dev &&
            single_lock_stat.st_ino == job_lock_stat.st_ino)
        {
            request->job_state_lock_fd = request->manager->lock_fd;

            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_DEBUG,
                    "event=gram.state_file_read.info "
                    "level=DEBUG "
                    "gramid=%s "
                    "path=%s "
                    "msg=\"%s\" "
                    "\n",
                    request->job_contact_path,
                    request->job_state_file,
                    "Using global job manager lock as job lock file");
        }
    }

skip_single_check:
    if (request->job_state_lock_file != NULL && request->job_state_lock_fd < 0)
    {
        request->job_state_lock_fd = open( request->job_state_lock_file,
                                           O_RDWR );
        if ( request->job_state_lock_fd < 0 )
        {
            rc = GLOBUS_GRAM_PROTOCOL_ERROR_LOCKING_STATE_LOCK_FILE;

            globus_gram_job_manager_request_log(
                    request,
                    GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                    "event=gram.state_file_read.end "
                    "level=ERROR "
                    "gramid=%s "
                    "status=%d "
                    "path=%s "
                    "msg=\"%s\" "
                    "errno=%d "
                    "reason=\"%s\" "
                    "\n",
                    request->job_contact_path,
                    -rc,
                    request->job_state_file,
                    "Error opening job lock file",
                    errno,
                    strerror(errno));

            goto free_buffer_exit;
        }

        rc = globus_gram_job_manager_file_lock(request->job_state_lock_fd);
        if ( rc != GLOBUS_SUCCESS )
        {
            if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE )
            {
                fp = fopen( request->job_state_file, "r" );
                if(fp)
                {
                    fgets( buffer, file_len, fp );
                    buffer[strlen(buffer)-1] = '\0';
                    request->old_job_contact = strdup(buffer);
                    fclose(fp);
                }

                globus_gram_job_manager_request_log(
                        request,
                        GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
                        "event=gram.state_file_read.end "
                        "level=TRACE "
                        "gramid=%s "
                        "status=%d "
                        "path=%s "
                        "contact=%s "
                        "\n",
                        request->job_contact_path,
                        -rc,
                        request->job_state_file,
                        request->old_job_contact
                                ? request->old_job_contact
                                : "");
            }
            else
            {
                globus_gram_job_manager_request_log(
                        request,
                        GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                        "event=gram.state_file.lock.end "
                        "level=ERROR "
                        "path=%s "
                        "status=%d "
                        "msg=\"%s\" "
                        "reason=\"%s\" "
                        "\n",
                        request->job_state_lock_file,
                        -rc,
                        "Error locking state lock file",
                        globus_gram_protocol_error_string(rc));
            }

            /* unlink here? */
            close( request->job_state_lock_fd );

            free(buffer);

            return rc;
        }
    }

    fp = fopen( request->job_state_file, "r" );
    if(!fp)
    {
        rc = GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE;

        globus_gram_job_manager_request_log(
                request,
                GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
                "event=gram.state_file.lock.end "
                "level=ERROR "
                "path=%s "
                "status=%d "
                "msg=\"%s\" "
                "errno=%d "
                "reason=\"%s\" "
                "\n",
                request->job_state_lock_file,
                -rc,
                "Error opening state file",
                errno,
                strerror(errno));
        return rc;
    }

    if(fgets( buffer, file_len, fp )  == NULL)
    {
        goto error_exit;
    }
    /* job contact string */
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto error_exit;
    }
    request->restart_state = atoi( buffer );
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto error_exit;
    }
    globus_gram_job_manager_request_set_status_time(request,
                atoi( buffer ), statbuf.st_mtime);

    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto error_exit;
    }
    request->failure_code = atoi( buffer );

    if(fgets( buffer, file_len, fp ) == NULL)
    {
        goto error_exit;
    }
    buffer[strlen(buffer)-1] = '\0';
    if(strcmp(buffer, " ") != 0)
    {
        request->job_id_string = strdup( buffer );
    }

    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_job_id_string;
    }
    buffer[strlen(buffer)-1] = '\0';
    request->rsl_spec = strdup( buffer );
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_rsl_spec;
    }
    buffer[strlen(buffer)-1] = '\0';
    request->cache_tag = strdup( buffer );
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_cache_tag;
    }
    buffer[strlen(buffer)-1] = '\0';
    if (request->config && 
        strcmp(buffer, request->config->jobmanager_type) != 0)
    {
        /* Job should be handled by another job manager */
        remove(request->job_state_lock_file);
        goto free_cache_tag;
    }
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_cache_tag;
    }
    buffer[strlen(buffer)-1] = '\0';
    if((sscanf(buffer,"%d",&i)) < 1)
    {
        /* The last line we grabbed was the jobmanager_type. Now we
         * need to grab the two_phase_commit number. Older jobmanagers
         * don't print the jobmanager_type to the state file, hence
         * the check above.
         */
        if(fgets( buffer, file_len, fp ) == NULL)
        {
            goto free_cache_tag;
        }
        buffer[strlen(buffer)-1] = '\0';
    }
    request->two_phase_commit = atoi(buffer);
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_cache_tag;
    }
    buffer[strlen(buffer)-1] = '\0';
    if(strcmp(buffer, " ") != 0)
    {
        request->scratchdir = strdup(buffer);
    }
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_scratchdir;
    }
    buffer[strlen(buffer)-1] = '\0';
    sscanf(buffer, "%lu", &tmp_timestamp);
    request->seg_last_timestamp = (time_t) tmp_timestamp;

    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_scratchdir;
    }
    buffer[strlen(buffer)-1] = '\0';
    sscanf(buffer, "%lu", &tmp_timestamp);
    request->creation_time = (time_t) tmp_timestamp;
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_scratchdir;
    }
    buffer[strlen(buffer)-1] = '\0';
    sscanf(buffer, "%lu", &tmp_timestamp);
    request->queued_time = (time_t) tmp_timestamp;

    request->stage_in_todo = NULL;
    request->stage_in_shared_todo = NULL;
    request->stage_out_todo = NULL;
    request->stage_stream_todo = NULL;

    rc = globus_gram_job_manager_staging_read_state(request,fp);
    if(rc != GLOBUS_SUCCESS)
    {
        goto free_scratchdir;
    }
    rc = globus_gram_job_manager_read_callback_contacts(request, fp);
    if(rc != GLOBUS_SUCCESS)
    {
        goto free_scratchdir;
    }

    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_scratchdir;
    }
    buffer[strlen(buffer)-1] = '\0';
    if(strcmp(buffer, " ") != 0)
    {
        request->gateway_user = strdup(buffer);
    }
    else
    {
        request->gateway_user = NULL;
    }
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_gateway_user;
    }
    buffer[strlen(buffer)-1] = 0;
    sscanf(buffer, "%d", &request->exit_code);

    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_gateway_user;
    }
    buffer[strlen(buffer)-1] = 0;
    sscanf(buffer,
            "%ld.%09ld %ld.%09ld %ld.%09ld "
            "%ld.%09ld %ld.%09ld %ld.%09ld %ld.%09ld "
            "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
            &request->job_stats.unsubmitted_timestamp.tv_sec,
            &request->job_stats.unsubmitted_timestamp.tv_nsec,
            &request->job_stats.file_stage_in_timestamp.tv_sec,
            &request->job_stats.file_stage_in_timestamp.tv_nsec,
            &request->job_stats.pending_timestamp.tv_sec,
            &request->job_stats.pending_timestamp.tv_nsec,
            &request->job_stats.active_timestamp.tv_sec,
            &request->job_stats.active_timestamp.tv_nsec,
            &request->job_stats.failed_timestamp.tv_sec,
            &request->job_stats.failed_timestamp.tv_nsec,
            &request->job_stats.file_stage_out_timestamp.tv_sec,
            &request->job_stats.file_stage_out_timestamp.tv_nsec,
            &request->job_stats.done_timestamp.tv_sec,
            &request->job_stats.done_timestamp.tv_nsec,
            &request->job_stats.restart_count,
            &request->job_stats.callback_count,
            &request->job_stats.status_count,
            &request->job_stats.register_count,
            &request->job_stats.unregister_count,
            &request->job_stats.signal_count,
            &request->job_stats.refresh_count,
            &request->job_stats.file_clean_up_count,
            &request->job_stats.file_stage_in_http_count,
            &request->job_stats.file_stage_in_https_count,
            &request->job_stats.file_stage_in_ftp_count,
            &request->job_stats.file_stage_in_gsiftp_count,
            &request->job_stats.file_stage_in_shared_http_count,
            &request->job_stats.file_stage_in_shared_https_count,
            &request->job_stats.file_stage_in_shared_ftp_count,
            &request->job_stats.file_stage_in_shared_gsiftp_count,
            &request->job_stats.file_stage_out_http_count,
            &request->job_stats.file_stage_out_https_count,
            &request->job_stats.file_stage_out_ftp_count,
            &request->job_stats.file_stage_out_gsiftp_count);
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_gateway_user;
    }
    buffer[strlen(buffer)-1] = '\0';
    if(strcmp(buffer, " ") != 0)
    {
        request->job_stats.client_address = strdup(buffer);
    }
    else
    {
        request->job_stats.client_address = NULL;
    }

    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_client_address;
    }
    buffer[strlen(buffer)-1] = 0;
    if(strcmp(buffer, " ") != 0)
    {
        request->job_stats.user_dn = strdup(buffer);
    }
    else
    {
        request->job_stats.user_dn = NULL;
    }
    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_client_address;
    }
    request->expected_terminal_state = atoi( buffer );

    if (fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_client_address;
    }
    buffer[strlen(buffer)-1] = '\0';
    if (request->config && strcmp(buffer, request->config->service_tag) != 0)
    {
        /* Job should be handled by another job manager */
        remove(request->job_state_lock_file);
        goto free_client_address;
    }
    if(fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_client_address;
    }
    buffer[strlen(buffer)-1] = '\0';
    if(strcmp(buffer, " ") != 0)
    {
        request->original_job_id_string = strdup(buffer);
    }

    if(fgets( buffer, file_len, fp ) == NULL)
    {
        goto free_original_job_id_string;
    }
    buffer[strlen(buffer)-1] = '\0';
    if(strcmp(buffer, " ") != 0)
    {
        request->job_log_level = atoi(buffer);
    }

    fclose(fp);

    free(buffer);

    globus_gram_job_manager_request_log(
            request,
            GLOBUS_GRAM_JOB_MANAGER_LOG_TRACE,
            "event=gram.state_file.lock.end "
            "level=TRACE "
            "path=%s "
            "status=%d "
            "\n",
            request->job_state_lock_file,
            0);

    return GLOBUS_SUCCESS;

free_original_job_id_string:
    if (request->original_job_id_string != NULL)
    {
        free(request->original_job_id_string);
        request->original_job_id_string = NULL;
    }
free_client_address:
    if (request->job_stats.client_address != NULL)
    {
        free(request->job_stats.client_address);
        request->job_stats.client_address = NULL;
    }
free_gateway_user:
    if (request->gateway_user != NULL)
    {
        free(request->gateway_user);
        request->gateway_user = NULL;
    }
free_scratchdir:
    if (request->scratchdir != NULL)
    {
        free(request->scratchdir);
        request->scratchdir = NULL;
    }
free_cache_tag:
    if (request->cache_tag != NULL)
    {
        free(request->cache_tag);
        request->cache_tag = NULL;
    }
free_rsl_spec:
    if (request->rsl_spec != NULL)
    {
        free(request->rsl_spec);
        request->rsl_spec = NULL;
    }
free_job_id_string:
    if (request->job_id_string != NULL)
    {
        free(request->job_id_string);
        request->job_id_string = NULL;
    }
error_exit:
    rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_STATE_FILE;

    globus_gram_job_manager_request_log(
            request,
            GLOBUS_GRAM_JOB_MANAGER_LOG_ERROR,
            "event=gram.state_file.lock.end "
            "level=ERROR "
            "path=%s "
            "status=%d "
            "msg=\"%s\" "
            "reason=\"%s\" "
            "\n",
            request->job_state_lock_file,
            -rc,
            "Error reading state file",
            globus_gram_protocol_error_string(rc));

    fclose(fp);
free_buffer_exit:
    if (buffer != NULL)
    {
        free(buffer);
    }
exit:
    return GLOBUS_GRAM_PROTOCOL_ERROR_READING_STATE_FILE;
}

/**
 * Try to set an advisory write lock on a file descriptor
 *
 * @param fd
 *     Open file descriptor to lock.
 * 
 * @retval GLOBUS_SUCCESS
 *     Success
 * @retval GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE
 *     Another process has the file locked.
 * @retval GLOBUS_GRAM_PROTOCOL_ERROR_LOCKING_STATE_LOCK_FILE
 *     System error locking the file.
 */
int
globus_gram_job_manager_file_lock(
    int                                 fd)
{
    int                                 rc;
    struct flock                        fl;

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    while( (rc = fcntl( fd, F_SETLK, &fl )) < 0 )
    {
        if ( errno == EACCES || errno == EAGAIN )
        {
            rc = GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE;
            break;
        }
        else if ( errno != EINTR )
        {
            rc = GLOBUS_GRAM_PROTOCOL_ERROR_LOCKING_STATE_LOCK_FILE;
            break;
        }
    }
    return rc;
}
/* globus_gram_job_manager_file_lock() */
