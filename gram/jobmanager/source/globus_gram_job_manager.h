#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * Description:
 *   This header contains the exported interface of the Job Management.
 *
 * CVS Information:
 * $Source$
 * $Date$
 * $Revision$
 * $Author$
 */
#ifndef GLOBUS_GRAM_JOB_MANAGER_INCLUDE
#define GLOBUS_GRAM_JOB_MANAGER_INCLUDE

/* Includes */
#include "globus_common.h"
#include "globus_gram_protocol.h"
#include "globus_rsl.h"
#include "globus_gass_cache.h"

EXTERN_C_BEGIN

/* Type definitions */
typedef enum
{
    GLOBUS_GRAM_JOB_MANAGER_DONT_SAVE,
    GLOBUS_GRAM_JOB_MANAGER_SAVE_ALWAYS,
    GLOBUS_GRAM_JOB_MANAGER_SAVE_ON_ERROR
}
globus_gram_job_manager_logfile_flag_t;

typedef enum
{
    GLOBUS_GRAM_JOB_MANAGER_STATE_START,
    GLOBUS_GRAM_JOB_MANAGER_STATE_MAKE_SCRATCHDIR,
    GLOBUS_GRAM_JOB_MANAGER_STATE_OPEN_OUTPUT,
    GLOBUS_GRAM_JOB_MANAGER_STATE_TWO_PHASE,
    GLOBUS_GRAM_JOB_MANAGER_STATE_TWO_PHASE_COMMIT_EXTEND,
    GLOBUS_GRAM_JOB_MANAGER_STATE_TWO_PHASE_COMMITTED,
    GLOBUS_GRAM_JOB_MANAGER_STATE_STAGE_IN,
    GLOBUS_GRAM_JOB_MANAGER_STATE_SUBMIT,
    GLOBUS_GRAM_JOB_MANAGER_STATE_POLL1,
    GLOBUS_GRAM_JOB_MANAGER_STATE_POLL2,
    GLOBUS_GRAM_JOB_MANAGER_STATE_CLOSE_OUTPUT,
    GLOBUS_GRAM_JOB_MANAGER_STATE_STAGE_OUT,
    GLOBUS_GRAM_JOB_MANAGER_STATE_TWO_PHASE_END,
    GLOBUS_GRAM_JOB_MANAGER_STATE_TWO_PHASE_END_COMMIT_EXTEND,
    GLOBUS_GRAM_JOB_MANAGER_STATE_TWO_PHASE_END_COMMITTED,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FILE_CLEAN_UP,
    GLOBUS_GRAM_JOB_MANAGER_STATE_SCRATCH_CLEAN_UP,
    GLOBUS_GRAM_JOB_MANAGER_STATE_DONE,
    GLOBUS_GRAM_JOB_MANAGER_STATE_EARLY_FAILED,
    GLOBUS_GRAM_JOB_MANAGER_STATE_EARLY_FAILED_CLOSE_OUTPUT,
    GLOBUS_GRAM_JOB_MANAGER_STATE_EARLY_FAILED_PRE_FILE_CLEAN_UP,
    GLOBUS_GRAM_JOB_MANAGER_STATE_EARLY_FAILED_FILE_CLEAN_UP,
    GLOBUS_GRAM_JOB_MANAGER_STATE_EARLY_FAILED_SCRATCH_CLEAN_UP,
    GLOBUS_GRAM_JOB_MANAGER_STATE_EARLY_FAILED_RESPONSE,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FAILED,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FAILED_CLOSE_OUTPUT,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FAILED_TWO_PHASE,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FAILED_TWO_PHASE_COMMIT_EXTEND,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FAILED_TWO_PHASE_COMMITTED,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FAILED_FILE_CLEAN_UP,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FAILED_SCRATCH_CLEAN_UP,
    GLOBUS_GRAM_JOB_MANAGER_STATE_FAILED_DONE,
    GLOBUS_GRAM_JOB_MANAGER_STATE_STOP,
    GLOBUS_GRAM_JOB_MANAGER_STATE_STOP_CLOSE_OUTPUT,
    GLOBUS_GRAM_JOB_MANAGER_STATE_STOP_DONE,
    GLOBUS_GRAM_JOB_MANAGER_STATE_POLL_QUERY1,
    GLOBUS_GRAM_JOB_MANAGER_STATE_POLL_QUERY2
}
globus_gram_jobmanager_state_t;

typedef enum
{
    GLOBUS_GRAM_JOB_MANAGER_STAGE_IN,
    GLOBUS_GRAM_JOB_MANAGER_STAGE_IN_SHARED,
    GLOBUS_GRAM_JOB_MANAGER_STAGE_OUT
}
globus_gram_job_manager_staging_type_t;

typedef enum
{
    GLOBUS_GRAM_JOB_MANAGER_SIGNAL,
    GLOBUS_GRAM_JOB_MANAGER_CANCEL
}
globus_gram_job_manager_query_type_t;

typedef struct
{
    globus_gram_job_manager_staging_type_t
					type;
    globus_rsl_value_t *		from;
    char *				evaled_from;
    globus_rsl_value_t *		to;
    char *				evaled_to;
}
globus_gram_job_manager_staging_info_t;

typedef struct
{
    /**
     * Query type
     */
    globus_gram_job_manager_query_type_t
					type;
    /**
     * Connection handle
     *
     * Handle to send the script response to.
     */
    globus_gram_protocol_handle_t	handle;
    /**
     * Signal
     *
     * Type of signal to process.
     */
    globus_gram_protocol_job_signal_t	signal;

    /**
     * Signal-specific data
     *
     * If a priority change maybe something like high, medium, low. see
     * the documentation on signals in the globus_gram_protocol library.
     */
    char *				signal_arg;

    int					failure_code;
}
globus_gram_job_manager_query_t;
/**
 * Job Manager Request
 */
typedef struct
{
    /**
     * Job State
     *
     * The state of the job. This corresponds to the job state machine
     * described in the GRAM documentation.
     *
     * @todo add link 
     */ 
    globus_gram_protocol_job_state_t	status;

    /**
     * Job Failure Reason
     *
     * If the state is GLOBUS_GRAM_STATE_FAILED, then this
     * is an integer code that defines the failure. It is one of
     * GLOBUS_GRAM_PROTOCOL_ERROR_*.
     */
    int					failure_code;
    
    /**
     * Job identifier
     *
     * Underlying queueing system job id for this job.
     * This value is filled in when the request is submitted.
     */
    char *				job_id;

    /**
     * Unique job identifier
     *
     * Unique id for this job that will be consistent
     * across jobmanager restarts/recoveries.
     */
    char *				uniq_id;

    /**
     * Poll Frequency
     *
     * How often should a check of the job status and output files be done.
     */
    unsigned int			poll_frequency;


    /**
     * Job Manager Type
     *
     * Identifies the scheduler which will be used to process this job
     * request. Possible values are fork, loadleveler, lsf, easymcs, pbs,
     * and others.
     */ 
    char *				jobmanager_type;
    char *				tcp_port_range;
    char *				globus_location;

    /**
     * Log File Name
     *
     * A path to a file to append logging information to.
     */
    char *				jobmanager_logfile;

    /**
     * Log File Pointer
     *
     * A stdio FILE pointer used for logging. NULL if no logging is requested.
     */
    FILE *				jobmanager_log_fp;

    /**
     * Flag denoting the disposition of the log file once the job manager
     * completes monitoring this job.
     */
    globus_gram_job_manager_logfile_flag_t
					logfile_flag;

     /**
      * Standard Output File Name
      *
      * Absolute path to a file to be used as standard output for the
      * executable.
      */
    char *				local_stdout;

    /**
     * Standard Error File Name
     *
     * Absolute path to a file to be used as standard error for the
     * executable.
     */
    char *				local_stderr;

    /**
     * Condor Architecture
     *
     * Used only when type=condor.  Must match one of the archetecture values
     * as defined by condor
     */
    char *				condor_arch;

    /**
     * Condor Operating System
     *
     * Used only when type=condor.  Must match one of the opsys values as
     * defined by condor
     */ 
    char *				condor_os;

    /**
     * Relative distinguished name
     *
     * Nickname of the job manager in the MDS.
     */
    char *				rdn;

    /**
     * Dry Run
     *
     * If this is GLOBUS_TRUE, do not actually submit the job to the scheduler,
     * just verify the job parameters.
     */
    globus_bool_t			dry_run;


    /**
     *
     * Two-phase commit.
     *
     * Non-zero if request should be confirmed via another signal.
     *
     * The value is how many seconds to wait before timing out.
     */
    int					two_phase_commit;
    int					commit_extend;

    /**
     * Save Job Manager State
     *
     * Generate a state file for possibly restarting the job manager
     * at a later time after a failure or signal.
     */
    globus_bool_t			save_state;

    /**
     * Previous Job Manager Contact 
     *
     * If we're restarting from a terminated Job Manager, this will specify
     * the old job contact so we can locate the Job Manager state file.
     */
    char *				jm_restart;

    /**
     * Scratch directory root.
     *
     * If the client requests a scratch directory with a relative path,
     * this bsae directory is prepended to it. It defaults to $(HOME),
     * but can be overridden on the job manager command line or configuration
     * file.
     */
    char *				scratch_dir_base;

    /**
     * Job scratch directory.
     *
     * Scratch subdirectory created for this job. It will be removed
     * when the job completes. This is a subdirectory of scratch_dir_base.
     */
    char *				scratchdir;

    /**
     * Information about the destinations for the job's stdout and
     * stderr. Opaque to all modules except globus_gram_job_manager_output.c
     */
    struct globus_l_gram_job_manager_output_info_t *
					output;
    globus_gass_cache_t			cache_handle;
    char *				cache_tag;

    globus_symboltable_t		symbol_table;
    char *				rsl_spec;
    globus_rsl_t *			rsl;
    int					ttl_limit;

    char *				remote_io_url;

    globus_bool_t			kerberos;
    char *				x509_user_proxy;

    char *				home;
    char *				logname;
    char *				globus_id;

    char *				job_state_file_dir;
    char *				job_state_file;

    globus_mutex_t			mutex;
    globus_cond_t			cond;
    globus_bool_t			in_handler;
    globus_list_t *			client_contacts;
    globus_list_t *			validation_records;
    globus_list_t *			stage_in_todo;
    globus_list_t *			stage_in_shared_todo;
    globus_list_t *			stage_out_todo;
    globus_gram_jobmanager_state_t	jobmanager_state;
    globus_gram_jobmanager_state_t	restart_state;
    globus_bool_t			unsent_status_change;
    globus_callback_handle_t		two_phase_commit_timer;
    globus_callback_handle_t		poll_timer;
    globus_callback_handle_t		proxy_expiration_timer;
    char *				url_base;
    char *				job_contact;
    gss_ctx_id_t			response_context;
    globus_fifo_t			pending_queries;
}
globus_gram_jobmanager_request_t;

/* globus_gram_job_manager_request.c */
int
globus_gram_job_manager_request_init(
    globus_gram_jobmanager_request_t **	request);

int 
globus_gram_job_manager_request_destroy(
    globus_gram_jobmanager_request_t *	request);

void
globus_gram_job_manager_request_open_logfile(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_logfile_flag_t
    					logfile_flag);
int
globus_gram_job_manager_request_log(
    globus_gram_jobmanager_request_t *	request,
    const char *			format,
    ...);

/* globus_gram_job_manager_validate.c */

/**
 * Select when an RSL parameter is valid or required.
 * @ingroup globus_gram_job_manager_rsl_validation 
 */
typedef enum
{
    GLOBUS_GRAM_VALIDATE_JOB_SUBMIT = 1,
    GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART = 2
}
globus_gram_job_manager_validation_when_t;

extern
int
globus_gram_job_manager_validation_init(
    globus_gram_jobmanager_request_t *  request);

extern
int
globus_gram_job_manager_validate_rsl(
    globus_gram_jobmanager_request_t *  request,
    globus_gram_job_manager_validation_when_t
    					when);

/* globus_gram_job_manager_contact.c */
int
globus_gram_job_manager_contact_add(
    globus_gram_jobmanager_request_t *	request,
    const char *			contact,
    int					job_state_mask);

int
globus_gram_job_manager_contact_remove(
    globus_gram_jobmanager_request_t *	request,
    const char *			contact);
int
globus_gram_job_manager_contact_list_free(
    globus_gram_jobmanager_request_t *	request);

void
globus_gram_job_manager_contact_state_callback(
    globus_gram_jobmanager_request_t *	request);

/* globus_gram_job_manager_output.c */
extern
int
globus_gram_job_manager_output_init(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_output_set_urls(
    globus_gram_jobmanager_request_t *	request,
    const char *			type,
    globus_list_t *			url_list,
    globus_list_t *			position_list);

char *
globus_gram_job_manager_output_get_cache_name(
    globus_gram_jobmanager_request_t *	request,
    const char *			type);
char *
globus_gram_job_manager_output_local_name(
    globus_gram_jobmanager_request_t *	request,
    const char *			type);

int
globus_gram_job_manager_output_open(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_output_close(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_output_write_state(
    globus_gram_jobmanager_request_t *	request,
    FILE *				fp);

int
globus_gram_job_manager_output_read_state(
    globus_gram_jobmanager_request_t *	request,
    FILE *				fp);

globus_bool_t
globus_gram_job_manager_output_check_size(
    globus_gram_jobmanager_request_t *	request,
    const char *			type,
    globus_off_t			size);

/* globus_gram_job_manager_state.c */
globus_bool_t
globus_gram_job_manager_state_machine_callback(
    globus_abstime_t *			time_left,
    void *				arg);

globus_bool_t
globus_gram_job_manager_state_machine(
    globus_gram_jobmanager_request_t *	request);

/* globus_gram_job_manager_gsi.c */
int
globus_gram_job_manager_import_sec_context(
    globus_gram_jobmanager_request_t *	request);

globus_bool_t
globus_gram_job_manager_gsi_used(
    globus_gram_jobmanager_request_t *	request);

char *
globus_gram_job_manager_gsi_proxy_relocate(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_register_proxy_timeout(
    globus_gram_jobmanager_request_t *	request);

/* globus_gram_job_manager_query.c */
void
globus_gram_job_manager_query_callback(
    void *				arg,
    globus_gram_protocol_handle_t	handle,
    globus_byte_t *			buf,
    globus_size_t			nbytes,
    int					errorcode);

void
globus_gram_job_manager_query_reply(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_query_t *	query,
    int					failure_code);

/* globus_gram_job_manager_staging.c */
int
globus_gram_job_manager_staging_create_list(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_staging_remove(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_staging_type_t
    					type,
    char *				from,
    char *				to);

int
globus_gram_job_manager_staging_write_state(
    globus_gram_jobmanager_request_t *	request,
    FILE *				fp);
int
globus_gram_job_manager_staging_read_state(
    globus_gram_jobmanager_request_t *	request,
    FILE *				fp);

/* globus_gram_job_manager_rsl.c */
globus_rsl_t *
globus_gram_job_manager_rsl_merge(
    globus_rsl_t *			base_rsl,
    globus_rsl_t *			override_rsl);

globus_bool_t
globus_gram_job_manager_rsl_need_stage_in(
    globus_gram_jobmanager_request_t *	request);

globus_bool_t
globus_gram_job_manager_rsl_need_stage_out(
    globus_gram_jobmanager_request_t *	request);

globus_bool_t
globus_gram_job_manager_rsl_need_file_cleanup(
    globus_gram_jobmanager_request_t *	request);

globus_bool_t
globus_gram_job_manager_rsl_need_scratchdir(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_rsl_env_add(
    globus_rsl_t *			ast_node,
    char *				var,
    char *				value);

int
globus_gram_job_manager_rsl_request_fill(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_rsl_eval_one_attribute(
    globus_gram_jobmanager_request_t *	request,
    char *                              attribute,
    char **                             value);

int
globus_gram_job_manager_rsl_remove_attribute(
    globus_gram_jobmanager_request_t *	request,
    char *				attribute);

int
globus_gram_job_manager_rsl_add_substitutions_to_symbol_table(
    globus_gram_jobmanager_request_t *	request);

/* globus_gram_job_manager_state_file.c */
void
globus_gram_job_manager_state_file_set(
    globus_gram_jobmanager_request_t *	request);
int
globus_gram_job_manager_state_file_read(
    globus_gram_jobmanager_request_t *	request);
int
globus_gram_job_manager_state_file_write(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_state_file_update(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_state_file_register_update(
    globus_gram_jobmanager_request_t *	request);

/* globus_gram_job_manager_script.c */
int 
globus_gram_job_manager_script_make_scratchdir(
    globus_gram_jobmanager_request_t *	request);
int 
globus_gram_job_manager_script_stage_in(
    globus_gram_jobmanager_request_t *	request);
int 
globus_gram_job_manager_script_stage_out(
    globus_gram_jobmanager_request_t *	request);
int 
globus_gram_job_manager_script_submit(
    globus_gram_jobmanager_request_t *	request);
int 
globus_gram_job_manager_script_poll(
    globus_gram_jobmanager_request_t *	request);
int 
globus_gram_job_manager_script_file_cleanup(
    globus_gram_jobmanager_request_t *	request);
int 
globus_gram_job_manager_script_rm_scratchdir(
    globus_gram_jobmanager_request_t *	request);
int
globus_gram_job_manager_script_signal(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_query_t *	query);
int
globus_gram_job_manager_script_cancel(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_query_t *	query);

/* globus_gram_job_manager_cleanup.c */
int
globus_gram_job_manager_clean_cache(
    globus_gram_jobmanager_request_t *	request);

EXTERN_C_END

#endif /* GLOBUS_GRAM_JOB_MANAGER_INCLUDE */
#endif /* ! GLOBUS_DONT_DOCUMENT_INTERNAL */
