#include "globus_common.h"
#include "globus_scheduler_event_generator.h"
#include "version.h"

#include <string.h>

#define SEG_LSF_DEBUG(level, message) \
    GlobusDebugPrintf(SEG_LSF, level, message)

enum {
    JOB_STAT_NULL = 0x00,
    JOB_STAT_PEND = 0x01,
    JOB_STAT_PSUSP = 0x02,
    JOB_STAT_RUN = 0x04,
    JOB_STAT_SSUSP = 0x08,
    JOB_STAT_USUSP = 0x10,
    JOB_STAT_EXIT = 0x20,
    JOB_STAT_DONE = 0x40,
    JOB_STAT_PDONE = (0x80),
    JOB_STAT_PERR = (0x100),
    JOB_STAT_WAIT = (0x200),
    JOB_STAT_UNKWN = 0x10000
};

/**
 * Debug levels:
 * If the environment variable SEG_LSF_DEBUG is set to a bitwise or
 * of these values, then a corresponding log message will be generated.
 */
typedef enum
{
    /**
     * Information of function calls and exits
     */
    SEG_LSF_DEBUG_INFO = (1<<0),
    /**
     * Warnings of things which may be bad.
     */
    SEG_LSF_DEBUG_WARN = (1<<1),
    /**
     * Fatal errors.
     */
    SEG_LSF_DEBUG_ERROR = (1<<2),
    /**
     * Details of function executions.
     */
    SEG_LSF_DEBUG_TRACE = (1<<3)
}
globus_l_seg_lsf_debug_level_t;

enum
{
    SEG_LSF_ERROR_UNKNOWN = 1,
    SEG_LSF_ERROR_OUT_OF_MEMORY,
    SEG_LSF_ERROR_BAD_PATH,
    SEG_LSF_ERROR_LOG_PERMISSIONS,
    SEG_LSF_ERROR_LOG_NOT_PRESENT
};

/**
 * State of the LSF log file parser.
 */
typedef struct 
{
    /**
     * Path to the LSF logdir
     */
    char *                              log_dir;

    /**
     * Last known status for the lsb.events.index file. This will only
     * change when a log file is being rotated. When this happens, we
     * may need to be very careful about what is going on with our latest
     * read.
     */
    struct stat                         event_idx_stat;

    /**
     * Path to lsb.events.index file
     */
    char *                              event_idx_path;

    /**
     * Current historical log file we are looking at if we are replaying
     * older events. Seems to be in the range [1..?] on the ISI system.
     */
    unsigned int                        event_idx;


    /**
     * If non-zero, the earliest event timestamp we are interested in reading
     * about from the currently opened logfile
     */
    time_t                              start_timestamp;

    /**
     * If non-zero the newest event in the current log file we are reading
     * if it is an historical one. This may be larger than the final timestamp
     * in the file for some reason.
     */
    time_t                              end_of_file_timestamp;

    /**
     * Path of the currently opened log file.
     */
    char *                              path;
    /**
     * True if the current log file is the lsb.events file and not
     * one of the rotated files.
     */
    globus_bool_t                       is_current_file;
    /**
     * Stdio file handle of the log file we are currently reading
     */
    FILE *                              fp;

    /** Buffer of log file data */
    char *                              buffer;
    /** Length of the buffer */
    size_t                              buffer_length;
    /** Starting offset of valid data in the buffer. */
    size_t                              buffer_point;
    /** Amount of valid data in the buffer */
    size_t                              buffer_valid;
    /** Callback for periodic file polling */
    globus_callback_handle_t            callback;
} globus_l_lsf_logfile_state_t;

static globus_mutex_t                   globus_l_lsf_mutex;
static globus_cond_t                    globus_l_lsf_cond;
static globus_bool_t                    shutdown_called;
static int                              callback_count;


GlobusDebugDefine(SEG_LSF);

static
int
globus_l_lsf_module_activate(void);

static
int
globus_l_lsf_module_deactivate(void);

static
void
globus_l_lsf_read_callback(
    void *                              user_arg);

static
int
globus_l_lsf_parse_events(
    globus_l_lsf_logfile_state_t *      state);

static
int
globus_l_lsf_clean_buffer(
    globus_l_lsf_logfile_state_t *      state);

static
int
globus_l_lsf_increase_buffer(
    globus_l_lsf_logfile_state_t *      state);

static
int
globus_l_lsf_find_logfile(
    globus_l_lsf_logfile_state_t *      state);

globus_module_descriptor_t
globus_scheduler_event_module_ptr =
{
    "globus_scheduler_event_generator_lsf",
    globus_l_lsf_module_activate,
    globus_l_lsf_module_deactivate,
    NULL,
    NULL,
    &local_version,
    NULL
};

static
int
globus_l_lsf_module_activate(void)
{
    globus_l_lsf_logfile_state_t *      logfile_state;
    int                                 rc;
    globus_reltime_t                    delay;
    globus_result_t                     result;

    rc = globus_module_activate(GLOBUS_COMMON_MODULE);
    if (rc != GLOBUS_SUCCESS)
    {
        goto error;
    }
    rc = globus_mutex_init(&globus_l_lsf_mutex, NULL);

    if (rc != GLOBUS_SUCCESS)
    {
        goto deactivate_common_error;
    }
    rc = globus_cond_init(&globus_l_lsf_cond, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        goto destroy_mutex_error;
    }
    shutdown_called = GLOBUS_FALSE;
    callback_count = 0;

    GlobusDebugInit(
        SEG_LSF,
        SEG_LSF_DEBUG_INFO
        SEG_LSF_DEBUG_WARN
        SEG_LSF_DEBUG_ERROR
        SEG_LSF_DEBUG_TRACE);

    logfile_state = globus_libc_calloc(
            1,
            sizeof(globus_l_lsf_logfile_state_t));

    if (logfile_state == NULL)
    {
        goto destroy_cond_error;
        return 1;
    }

    rc = globus_l_lsf_increase_buffer(logfile_state);
    if (rc != GLOBUS_SUCCESS)
    {
        goto free_logfile_state_error;
    }

    /* Configuration info */
    result = globus_scheduler_event_generator_get_timestamp(
            &logfile_state->start_timestamp);

    if (result != GLOBUS_SUCCESS)
    {
        goto free_logfile_state_buffer_error;
    }

    if (logfile_state->start_timestamp == 0)
    {
        logfile_state->start_timestamp = time(NULL);
    }

    result = globus_common_get_attribute_from_config_file(
            NULL,
            "etc/globus-lsf.conf",
            "log_path",
            &logfile_state->log_dir);
    if (result != GLOBUS_SUCCESS)
    {
        SEG_LSF_DEBUG(SEG_LSF_DEBUG_WARN,
                ("unable to find log file in configuration\n"));
        goto free_logfile_state_buffer_error;
    }
    /* Convert timestamp to filename */
    rc = globus_l_lsf_find_logfile(logfile_state);

    if (rc == GLOBUS_SUCCESS)
    {
        logfile_state->fp = fopen(logfile_state->path, "r");

        if (logfile_state->fp == NULL)
        {
            rc = SEG_LSF_ERROR_OUT_OF_MEMORY;

            goto free_logfile_state_path_error;
        }
        GlobusTimeReltimeSet(delay, 0, 0);
    }
    else if(rc == SEG_LSF_ERROR_LOG_NOT_PRESENT)
    {
        GlobusTimeReltimeSet(delay, 1, 0);
    }
    else
    {
        goto free_logfile_state_path_error;
    }

    result = globus_callback_register_oneshot(
            &logfile_state->callback,
            &delay,
            globus_l_lsf_read_callback,
            logfile_state);

    if (result != GLOBUS_SUCCESS)
    {
        goto free_logfile_state_path_error;
    }
    callback_count++;

    return 0;

free_logfile_state_path_error:
    if (logfile_state->path)
    {
        globus_libc_free(logfile_state->path);
    }
    if (logfile_state->log_dir)
    {
        globus_libc_free(logfile_state->log_dir);
    }
free_logfile_state_buffer_error:
    globus_libc_free(logfile_state->buffer);
free_logfile_state_error:
    globus_libc_free(logfile_state);
destroy_cond_error:
    globus_cond_destroy(&globus_l_lsf_cond);
destroy_mutex_error:
    globus_mutex_destroy(&globus_l_lsf_mutex);
deactivate_common_error:
    globus_module_deactivate(GLOBUS_COMMON_MODULE);
error:
    return 1;
}
/* globus_l_lsf_module_activate() */

static
int
globus_l_lsf_module_deactivate(void)
{
    globus_mutex_lock(&globus_l_lsf_mutex);
    shutdown_called = GLOBUS_TRUE;

    while (callback_count > 0)
    {
        globus_cond_wait(&globus_l_lsf_cond, &globus_l_lsf_mutex);
    }
    globus_mutex_unlock(&globus_l_lsf_mutex);

    GlobusDebugDestroy(SEG_LSF);

    globus_module_deactivate(GLOBUS_COMMON_MODULE);

    return 0;
}

/**
 * read_cb:
 *  check to see if lsb.events.index file changed---if so, we must relocate
 *  our position in the appropriate log file.
 *  
 *  if we're ok---parse events buffer
 *
 *  if (!eof)
 *      register read (read_cb)
 *  else
 *      if (it's an old logfile)
 *          register_close(old_close_cb)
 *      else
 *          register poll (wakeup_cb)
 */
static
void
globus_l_lsf_read_callback(
    void *                              user_arg)
{
    int                                 rc;
    globus_l_lsf_logfile_state_t *      state = user_arg;
    size_t                              max_to_read;
    globus_bool_t                       eof_hit = GLOBUS_FALSE;
    globus_reltime_t                    delay;
    globus_result_t                     result;
    struct stat                         s;

    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO, ("globus_l_lsf_read_callback()\n"));

    globus_mutex_lock(&globus_l_lsf_mutex);
    if (shutdown_called)
    {
        SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO, ("polling while deactivating"));

        globus_mutex_unlock(&globus_l_lsf_mutex);
        goto error;
    }
    globus_mutex_unlock(&globus_l_lsf_mutex);

    rc = stat(state->event_idx_path, &s);

    if ((rc == 0 && state->fp != NULL &&
            state->event_idx_stat.st_mtime != s.st_mtime)
        || (rc != 0 && errno != ENOENT))
    {
        /* Log was rotated since we started our read, so we need to
         * figure out what we need to read
         */
        fclose(state->fp);
        state->is_current_file = GLOBUS_FALSE;

        rc = globus_l_lsf_find_logfile(state);

        if (rc == GLOBUS_SUCCESS)
        {
            state->fp = fopen(state->path, "r");

            GlobusTimeReltimeSet(delay, 0, 0);

            result = globus_callback_register_oneshot(
                    &state->callback,
                    &delay,
                    globus_l_lsf_read_callback,
                    state);
        }
        /* ERROR? */
        return;
    }

    if (state->fp != NULL)
    {
        /* Read data */
        max_to_read = state->buffer_length - state->buffer_valid
                - state->buffer_point;

        SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                ("reading a maximum of %u bytes\n", max_to_read));

        rc = fread(state->buffer + state->buffer_point + state->buffer_valid,
                1, max_to_read, state->fp);
        
        SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                ("read %d bytes\n", rc));

        if (rc < max_to_read)
        {
            if (feof(state->fp))
            {
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE, ("hit eof\n"));
                eof_hit = GLOBUS_TRUE;
                clearerr(state->fp);
            }
            else
            {
                /* XXX: Read error */
            }
        }

        state->buffer_valid += rc;

        /* Parse data */
        SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE, ("parsing events\n"));
        rc = globus_l_lsf_parse_events(state);

        SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE, ("cleaning buffer\n"));
        rc = globus_l_lsf_clean_buffer(state);
    }

    /* If end of log and it's not the current log, find a new logfile
     */
    if (eof_hit && !state->is_current_file)
    {
        fclose(state->fp);
        if (state->start_timestamp > 0 &&
                state->start_timestamp <= state->end_of_file_timestamp)
        {
            state->start_timestamp = state->end_of_file_timestamp;
        }
        rc = globus_l_lsf_find_logfile(state);

        if (rc == GLOBUS_SUCCESS)
        {
            state->fp = fopen(state->path, "r");

            GlobusTimeReltimeSet(delay, 0, 0);
        }
    }
    else if (eof_hit)
    {
        GlobusTimeReltimeSet(delay, 2, 0);
    }
    else
    {
        GlobusTimeReltimeSet(delay, 0, 0);
    }

    result = globus_callback_register_oneshot(
            &state->callback,
            &delay,
            globus_l_lsf_read_callback,
            state);
    if (result != GLOBUS_SUCCESS)
    {
        goto error;
    }
    return;
error:
    globus_mutex_lock(&globus_l_lsf_mutex);
    if (shutdown_called)
    {
        callback_count--;

        if (callback_count == 0)
        {
            globus_cond_signal(&globus_l_lsf_cond);
        }
    }
    globus_mutex_unlock(&globus_l_lsf_mutex);

    SEG_LSF_DEBUG(SEG_LSF_DEBUG_WARN,
            ("globus_l_lsf_read_callback() exited with error\n"));
    return;

}
/* globus_l_lsf_read_callback() */

/**
 * Determine the next available LSF log file name from the 
 * timestamp stored in the logfile state structure.
 * 
 * @param state
 *     LSF log state structure. The path field of the structure may be
 *     modified by this function.
 *
 * @retval GLOBUS_SUCCESS
 *     Name of an log file name has been found and the file exists.
 * @retval 1
 *     Something bad occurred.
 */
static
int
globus_l_lsf_find_logfile(
    globus_l_lsf_logfile_state_t *      state)
{
    struct stat                         s;
    int                                 rc;
    const char                          lsf_log_prefix[] = "lsb.events.";
    const char                          lsf_idx_name[] = "lsb.events.index";
    FILE *                              idx_file;
    int                                 num_idx_files;
    time_t                              main_events_start;
    time_t                              most_recent_event;

    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO, ("globus_l_lsf_find_logfile()\n"));

    if (state->path == NULL)
    {
        SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE, ("allocating path\n"));
        state->path = malloc(strlen(state->log_dir) + sizeof(lsf_log_prefix)
                + 10);

        if (state->path == NULL)
        {
            rc = SEG_LSF_ERROR_OUT_OF_MEMORY;
            goto error;
        }
    }
    if (state->event_idx_path == NULL)
    {
        state->event_idx_path = malloc(strlen(state->log_dir)
                + sizeof(lsf_idx_name) + 1);
        sprintf(state->event_idx_path, "%s/%s", state->log_dir, lsf_idx_name);
    }

    if (state->start_timestamp == 0)
    {
        sprintf(state->path, "%s/lsb.events", state->log_dir);
        state->is_current_file = GLOBUS_TRUE;
        stat(state->event_idx_path, &state->event_idx_stat);
    }
    else
    {
        do
        {
            stat(state->event_idx_path, &state->event_idx_stat);
            idx_file = fopen(state->event_idx_path, "r");
            if (idx_file == NULL)
            {
                sprintf(state->path, "%s/lsb.events", state->log_dir);
                state->is_current_file = GLOBUS_TRUE;
                rc = 0;

                break;
            }
            fscanf(idx_file, 
                    "#LSF_JOBID_INDEX_FILE %*d.%*d %d %ld",
                    &num_idx_files,
                    &main_events_start);
            fclose(idx_file);
            if (main_events_start < state->start_timestamp)
            {
                /* The main lsb.events file starts before our start event,
                 * so we'll use that instead of an historic file
                 */
                sprintf(state->path, "%s/lsb.events", state->log_dir);
                state->is_current_file = GLOBUS_TRUE;
            }
            else
            {
                int i;

                for (i = 0; i < num_idx_files; i++)
                {
                    sprintf(state->path, "%s/%s%d",
                            state->log_dir,
                            lsf_log_prefix,
                            num_idx_files - i);
                    idx_file = fopen(state->path, "r");
                    fscanf(idx_file, "#%ld",
                            &most_recent_event);
                    fclose(idx_file);
                    if (most_recent_event > state->start_timestamp)
                    {
                        state->end_of_file_timestamp = most_recent_event;
                        break;
                    }
                }
                if (i == num_idx_files)
                {
                    sprintf(state->path, "%s/lsb.events", state->log_dir);
                    state->is_current_file = GLOBUS_TRUE;
                }
            }
            stat(state->event_idx_path, &s);
        }
        while (state->event_idx_stat.st_mtime != s.st_mtime);
    }

    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO,
            ("globus_l_lsf_find_logfile() exits w/out error\n"));
    return 0;

error:
    SEG_LSF_DEBUG(SEG_LSF_DEBUG_WARN,
            ("globus_l_lsf_find_logfile() exits w/error\n"));
    return rc;
}
/* globus_l_lsf_find_logfile() */

/**
 * Move any data in the state buffer to the beginning, to enable reusing 
 * buffer space which has already been parsed.
 */
static
int
globus_l_lsf_clean_buffer(
    globus_l_lsf_logfile_state_t *      state)
{
    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO,
            ("globus_l_lsf_clean_buffer() called\n"));

    /* move data to head of buffer */
    if (state->buffer != NULL)
    {
        if(state->buffer_point > 0)
        {
            if (state->buffer_valid > 0)
            {
                memmove(state->buffer,
                        state->buffer+state->buffer_point,
                        state->buffer_valid);
            }
            state->buffer_point = 0;
        }
    }
    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO,
            ("globus_l_lsf_clean_buffer() exits\n"));
    return 0;
}
/* globus_l_lsf_clean_buffer() */

/**
 * Reduce unused space in the log buffer, increasing the size of the buffer
 * if it is full.
 *
 * @param state
 *     LSF log state structure. The buffer-related fields of the structure
 *     may be modified by this function.
 */
static
int
globus_l_lsf_increase_buffer(
    globus_l_lsf_logfile_state_t *      state)
{
    char *                              save = state->buffer;
    const size_t                        GLOBUS_LSF_READ_BUFFER_SIZE = 4096;
    int                                 rc;

    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO,
            ("globus_l_lsf_increase_buffer() called\n"));
    /* If the buffer is full, resize */
    if (state->buffer_valid == state->buffer_length)
    {
        state->buffer = globus_libc_realloc(state->buffer,
                    state->buffer_length + GLOBUS_LSF_READ_BUFFER_SIZE);
        if (state->buffer == NULL)
        {
            SEG_LSF_DEBUG(SEG_LSF_DEBUG_ERROR, ("realloc() failed\n"));

            rc = SEG_LSF_ERROR_OUT_OF_MEMORY;
            goto error;
        }
    }

    state->buffer_length += GLOBUS_LSF_READ_BUFFER_SIZE;

    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO,
            ("globus_l_lsf_increase_buffer() exits w/success\n"));
    return 0;

error:
    SEG_LSF_DEBUG(SEG_LSF_DEBUG_WARN,
            ("globus_l_lsf_increase_buffer() exits w/failure\n"));
    state->buffer = save;
    return rc;
}
/* globus_l_lsf_increase_buffer() */

static
int
globus_l_lsf_parse_events(
    globus_l_lsf_logfile_state_t *      state)
{
    char *                              eol;
    time_t                              event_timestamp;
    char                                event_type_buffer[64];
    char                                job_id_buffer[32];
    int                                 rc;
    int                                 job_status;
    int                                 exit_status;
    long                                offset;
    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO,
            ("globus_l_lsf_parse_events() called\n"));

    while ((eol = memchr(state->buffer + state->buffer_point,
                '\n',
                state->buffer_valid)) != NULL)
    {
        *eol = '\0';

        SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                ("parsing line %s\n", state->buffer + state->buffer_point));

        if (state->buffer[state->buffer_point] == '#')
        {
            /* Parse first line of log file */
            if (state->is_current_file)
            {
                /* If this is lsb.events, then the first line contains
                 * the offset to where events not in an old log file are
                 * to be found in this file
                 */
                sscanf(state->buffer + state->buffer_point + 1, "%ld", &offset);
                fseek(state->fp, offset, SEEK_SET);

                /* don't bother parsing the rest of our read, it's junk */
                state->buffer_point = 0;
                state->buffer_valid = 0;
                break;
            }
            /* If this is one of the lsb.events.N files, then the first
             * line is the last timestamp covered by this file... we don't
             * care about this info at this point.
             */
            goto next_line;
        }
        sscanf(state->buffer + state->buffer_point,
                "\"%[^\"]\" \"%*[^\"]\" %ld %s",
                event_type_buffer,
                &event_timestamp,
                job_id_buffer);

        if (!strcmp(event_type_buffer, "JOB_NEW"))
        {
            if (event_timestamp >= state->start_timestamp)
            {
                rc = globus_scheduler_event_pending(event_timestamp,
                        job_id_buffer);
                state->start_timestamp = event_timestamp;
            }
        }
        else if (!strcmp(event_type_buffer, "JOB_START"))
        {
            if (event_timestamp >= state->start_timestamp)
            {
                rc = globus_scheduler_event_active(event_timestamp,
                        job_id_buffer);

                state->start_timestamp = event_timestamp;
            }
        }
        else if (!strcmp(event_type_buffer, "JOB_STATUS"))
        {
            sscanf(state->buffer + state->buffer_point,
                    "\"JOB_STATUS\" \"%*[^\"]\" %*d %*s %d",
                    &job_status);

            switch (job_status)
            {
            case JOB_STAT_PEND: /* IGNORE */
                /*
                 * The job is pending, that is, it has not yet been started.
                 */

                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in PEND state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            case JOB_STAT_PSUSP:
                /*
                 * The  job  has  been  suspended,  either  by its owner or
                 * the LSF administrator, while pending.
                 */
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in PSUSP state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            case JOB_STAT_RUN:
                /*
                 * the job is currently running.
                 */
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in RUN state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;

            case JOB_STAT_SSUSP:
                /*
                 * The job has been suspended by LSF. The job has been
                 * suspended by LSF due to either of the following two
                 * causes:
                 * - The load conditions on the execution host or hosts have
                 *   exceeded  a threshold  according  to the loadStop vector
                 *   defined for the host or queue.
                 * - The run window  of  the  job's  queue  is  closed.  See
                 *   bqueues(1),
                 */
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in SSUSP state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            case JOB_STAT_USUSP:
                /*
                 * The job has been suspended, either  by  its  owner  or
                 * the  LSF administrator, while running.
                 */
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in SSUSP state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            case JOB_STAT_EXIT:
                /*
                 * The  job  has terminated with a non-zero status - it may
                 * have been aborted due to an error in its execution, or
                 * killed  by its owner or the LSF administrator.
                 */

                if (event_timestamp >= state->start_timestamp)
                {
                    char * tmp = eol-1;
                    while (isdigit(*tmp))
                    {
                        tmp--;
                    }
                    sscanf(tmp, " %d", &exit_status);
                    tmp--;
                    while (isdigit(*tmp))
                    {
                        tmp--;
                    }
                    tmp--;
                    while (isdigit(*tmp))
                    {
                        tmp--;
                    }

                    if (exit_status == 0)
                    {
                        sscanf(tmp, " %d", &exit_status);
                        exit_status = (exit_status & 0xff00) >> 8;
                        rc = globus_scheduler_event_done(event_timestamp,
                                job_id_buffer,
                                exit_status);
                    }
                    else
                    {
                        rc = globus_scheduler_event_failed(event_timestamp,
                                job_id_buffer,
                                exit_status);
                    }
                    state->start_timestamp = event_timestamp;
                }
                break;
            case JOB_STAT_DONE:
                /*
                 * The job has terminated with status of 0.
                 */
                if (event_timestamp >= state->start_timestamp)
                {
                    rc = globus_scheduler_event_done(event_timestamp,
                            job_id_buffer,
                            0);
                    state->start_timestamp = event_timestamp;
                }
                break;
            case JOB_STAT_PDONE:
                /*
                 * Post job process done successfully
                 */
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in PDONE state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            case JOB_STAT_PERR:
                /*
                 * Post job process has error
                 */
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in PERR state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            case JOB_STAT_WAIT:
                /*
                 * For  jobs  submitted to a chunk job queue, members of a
                 * chunk job that are waiting to run.
                 */
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in WAIT state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            case JOB_STAT_UNKWN:
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in UNKNWN state "
                                "(%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            case JOB_STAT_NULL:
                SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                        ("ignoring JOB_STATUS: job %s in NULL state (%ld)\n",
                        job_id_buffer, event_timestamp));
                break;
            }
        }
        else
        {
            SEG_LSF_DEBUG(SEG_LSF_DEBUG_TRACE,
                    ("ignoring line: %s",
                    state->buffer + state->buffer_point));
        }
next_line:
        state->buffer_valid -= eol + 1 - state->buffer - state->buffer_point;
        state->buffer_point = eol + 1 - state->buffer;
        if (event_timestamp >= state->start_timestamp)
        {
            state->start_timestamp = event_timestamp;
        }
    }

    SEG_LSF_DEBUG(SEG_LSF_DEBUG_INFO,
            ("globus_l_lsf_parse_events() exits\n"));
    return 0;
}
/* globus_l_lsf_parse_events() */
