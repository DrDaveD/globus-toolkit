#include "globus_common.h"
#include "globus_gram_protocol_constants.h"

static char *
globus_l_gram_protocol_error_strings[GLOBUS_GRAM_PROTOCOL_ERROR_LAST] =
{
  /* 
   * these error strings are phrased to fit grammatically into 
   * a sentence of the form "the job failed because <string>"
   */

/*   0 */     "Success",
/*   1 */     "one of the RSL parameters is not supported",
/*   2 */     "the RSL lentgh is greater than the maximum allowed",
/*   3 */     "of an unused NO_RESOURCES",  /* NEEDS EDITING */
/*   4 */     "jobmanager unable to set default to the directory requested",
/*   5 */     "the executable does not exist",
/*   6 */     "of an unused INSUFFICIENT_FUNDS",   /* NEEDS EDITING */
/*   7 */     "authentication with the remote server failed",
/*   8 */     "of an unused USER_CANCELLED",  /* NEEDS EDITING */
/*   9 */     "of an unused SYSTEM_CANCELLED",  /* NEEDS EDITING */
/*  10 */     "data transfer to the server failed",
/*  11 */     "the stdin file does not exist",
/*  12 */     "the connection to the server failed (check host and port)",
/*  13 */     "the provided RSL 'maxtime' value is not an integer",
/*  14 */     "the provided RSL 'count' value is not an integer",
/*  15 */     "the job manager received an invalid RSL",
/*  16 */     "the job manager failed in allowing others to make contact",
/*  17 */     "the job failed when the job manager attempted to run it",
/*  18 */     "an invalid paradyn was specified",  /* NEEDS EDITING */
/*  19 */     "the provided RSL 'jobtype' value is invalid",
/*  20 */     "the provided RSL 'myjob' value is invalid",
/*  21 */     "the job manager failed to locate an internal script argument file",
/*  22 */     "the job manager failed to create an internal script argument file",
/*  23 */     "the job manager detected an invalid job state",
/*  24 */     "the job manager detected an invalid script response",
/*  25 */     "the job manager detected an invalid job state",
/*  26 */     "the provided RSL 'jobtype' value is not supported by this job manager",
/*  27 */     "unused ERROR_UNIMPLEMENTED",  /* NEEDS EDITING */
/*  28 */     "the job manager failed to create an internal script submission file",
/*  29 */     "the job manager cannot find the user proxy",
/*  30 */     "the job manager failed to open the user proxy",
/*  31 */     "the job manager failed to cancel the job as requested",
/*  32 */     "system memory allocation failed",
/*  33 */     "the interprocess job communication initialization failed",
/*  34 */     "the interprocess job communication setup failed",  /* REDUNDANT? */
/*  35 */     "the provided RSL 'host count' value is invalid",  /* REDUNDANT? */
/*  36 */     "one of the provided RSL parameters is unsupported",
/*  37 */     "the provided RSL 'queue' parameter is invalid",
/*  38 */     "the provided RSL 'project' parameter is invalid",
/*  39 */     "the provided RSL string includes variables that could not be identified",
/*  40 */     "the provided RSL 'environment' parameter is invalid",
/*  41 */     "the provided RSL 'dryrun' parameter is invalid",
/*  42 */     "the provided RSL is invalid (an empty string)",
/*  43 */     "the job manager failed to stage the executable",
/*  44 */     "the job manager failed to stage the stdin file",
/*  45 */     "the requested job manager type is invalid",
/*  46 */     "the provided RSL 'arguments' parameter is invalid",
/*  47 */     "the gatekeeper failed to run the job manager",
/*  48 */     "the provided RSL could not be properly parsed",
/*  49 */     "there is a version mismatch between GRAM components",
/*  50 */     "the provided RSL 'arguments' parameter is invalid",
/*  51 */     "the provided RSL 'count' parameter is invalid",
/*  52 */     "the provided RSL 'directory' parameter is invalid",
/*  53 */     "the provided RSL 'dryrun' parameter is invalid",
/*  54 */     "the provided RSL 'environment' parameter is invalid",
/*  55 */     "the provided RSL 'executable' parameter is invalid",
/*  56 */     "the provided RSL 'host_count' parameter is invalid",
/*  57 */     "the provided RSL 'jobtype' parameter is invalid",
/*  58 */     "the provided RSL 'maxtime' parameter is invalid",
/*  59 */     "the provided RSL 'myjob' parameter is invalid",
/*  60 */     "the provided RSL 'paradyn' parameter is invalid",
/*  61 */     "the provided RSL 'project' parameter is invalid",
/*  62 */     "the provided RSL 'queue' parameter is invalid",
/*  63 */     "the provided RSL 'stderr' parameter is invalid",
/*  64 */     "the provided RSL 'stdin' parameter is invalid",
/*  65 */     "the provided RSL 'stdout' parameter is invalid",
/*  66 */     "the job manager failed to locate an internal script",
/*  67 */     "the job manager failed on the system call pipe()",
/*  68 */     "the job manager failed on the system call fcntl()",
/*  69 */     "the job manager failed to create the temporary stdout filename",
/*  70 */     "the job manager failed to create the temporary stderr filename",
/*  71 */     "the job manager failed on the system call fork()",
/*  72 */     "the executable file permissions do not allow execution",
/*  73 */     "the job manager failed to open stdout",
/*  74 */     "the job manager failed to open stderr",
/*  75 */     "the cache file could not be opened in order to relocate the user proxy",  /* NEEDS EDITING */
/*  76 */     "cannot access cache files in ~/.globus/.gass_cache, check permissions, quota, and disk space",
/*  77 */     "the job manager failed to insert the contact in the client contact list",
/*  78 */     "the contact was not found in the job manager's client contact list",
/*  79 */     "connecting to the job manager failed.  Possible reasons: job terminated, invalid job contact, network problems, ...",
/*  80 */     "the syntax of the job contact is invalid",
/*  81 */     "the executable parameter in the RSL is undefined",
/*  82 */     "the job manager service is misconfigured.  condor arch undefined",
/*  83 */     "the job manager service is misconfigured.  condor os undefined",
/*  84 */     "the provided RSL 'min_memory' parameter is invalid",
/*  85 */     "the provided RSL 'max_memory' parameter is invalid",
/*  86 */     "the RSL 'min_memory' value is not zero or greater",
/*  87 */     "the RSL 'max_memory' value is not zero or greater",
/*  88 */     "the creation of a HTTP message failed",
/*  89 */     "parsing incoming HTTP message failed",
/*  90 */     "the packing of information into a HTTP message failed",
/*  91 */     "an incoming HTTP message did not contain the expected information",
/*  92 */     "the job manager does not support the service that the client requested",
/*  93 */     "the gatekeeper failed to find the requested service",
/*  94 */     "the jobmanager does not accept any new requests (shutting down)",
/*  95 */     "the client failed to close the listener associated with the callback URL",
/*  96 */     "the gatekeeper contact cannot be parsed",
/*  97 */     "the job manager could not find the 'poe' command",
/*  98 */     "the job manager could not find the 'mpirun' command",
/*  99 */     "the provided RSL 'start_time' parameter is invalid",
/* 100 */     "the provided RSL 'reservation_handle' parameter is invalid",
/* 101 */     "the provided RSL 'max_wall_time' parameter is invalid",
/* 102 */     "the RSL 'max_wall_time' value is not zero or greater",
/* 103 */     "the provided RSL 'max_cpu_time' parameter is invalid",
/* 104 */     "the RSL 'max_cpu_time' value is not zero or greater",
/* 105 */     "the job manager is misconfigured, a scheduler script is missing",
/* 106 */     "the job manager is misconfigured, a scheduler script has invalid permissions",
/* 107 */     "the job manager failed to signal the job",
/* 108 */     "the job manager did not recognize/support the signal type",
/* 109 */     "the job manager failed to get the job id from the local scheduler",
/* 110 */     "the job manager is waiting for a commit signal",
/* 111 */     "the job manager timed out while waiting for a commit signal",
/* 112 */     "the provided RSL 'save_state' parameter is invalid",
/* 113 */     "the provided RSL 'restart' parameter is invalid",
/* 114 */     "the provided RSL 'two_phase' parameter is invalid",
/* 115 */     "the RSL 'two_phase' value is not zero or greater",
/* 116 */     "the provided RSL 'stdout_position' parameter is invalid",
/* 117 */     "the RSL 'stdout_position' value is not zero or greater",
/* 118 */     "the provided RSL 'stderr_position' parameter is invalid",
/* 119 */     "the RSL 'stderr_position' value is not zero or greater",
/* 120 */     "the job manager restart attempt failed",
/* 121 */     "the job state file doesn't exist",
/* 122 */     "could not read the job state file",
/* 123 */     "could not write the job state file",
/* 124 */     "old job manager is still alive",
/* 125 */     "job manager state file TTL expired",
/* 126 */     "it is unknown if the job was submitted",
/* 127 */     "the provided RSL 'remote_io_url' parameter is invalid",
/* 128 */     "could not write the remote io url file",
/* 129 */     "the standard output/error size is different",
/* 130 */     "the job manager was sent a stop signal (job is still running)",
/* 131 */     "the user proxy expired (job is still running)",
/* 132 */     "the job was not submitted by original jobmanager",
/* 133 */     "the job manager is not waiting for that commit signal",
/* 134 */     "the provided RSL 'scheduler_specific' parameter is invalid",
/* 135 */     "the job manager could not stage in a file",
/* 136 */     "the scratch directory could not be created",
/* 137 */     "the gass_cache attribute points to an invalid cache directory"
};

static char *
globus_l_gram_protocol_error_7_hack_message = GLOBUS_NULL;

/**
 * Error code translation.
 * @ingroup globus_gram_protocol_error_messages
 *
 * This function takes the error code value and returns the associated error
 * code string. The string is statically allocated by the Globus GRAM Protocol
 * library and should not be modified or freed.
 *
 * @param error_code
 *        The error code to look up.
 *
 * @return An error string containing the reason for the error. The error
 *         string is written to be used in the context
 *         "[operation] failed because [error_string]".
 *
 */
const char *
globus_gram_protocol_error_string(int error_code)
{
    if (error_code<0 || error_code>=GLOBUS_GRAM_PROTOCOL_ERROR_LAST)
        return("Invalid error code");

    if ((error_code == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION)
	&& globus_l_gram_protocol_error_7_hack_message)
	return globus_l_gram_protocol_error_7_hack_message;

    return(globus_l_gram_protocol_error_strings[error_code]);
} /* globus_gram_protocol_error_string() */


/**
 * GSI specific error code hack.
 * @ingroup globus_gram_protocol_error_messages
 *
 * This function creates a custom version of the error message for the error
 * GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION.  This function should really
 * only used by the GRAM client library.
 *
 * @param message
 *        The new message to be associated with error code 7.
 *
 * @return void
 */
void
globus_gram_protocol_error_7_hack_replace_message(const char * message)
{
    if (globus_l_gram_protocol_error_7_hack_message)
	globus_libc_free(globus_l_gram_protocol_error_7_hack_message);

    globus_l_gram_protocol_error_7_hack_message = GLOBUS_NULL;

    if (message)
	globus_l_gram_protocol_error_7_hack_message = 
	    globus_libc_strdup(message);
}

