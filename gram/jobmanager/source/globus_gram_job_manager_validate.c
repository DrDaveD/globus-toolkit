#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gram_job_manager_validate.c
 *
 * RSL Validation Support for the GRAM Job Manager.
 *
 * CVS Information:
 * $Source$
 * $Date$
 * $Revision$
 * $Author$
 */

#define GLOBUS_GRAM_VALIDATE_JOB_SUBMIT_STRING \
        "GLOBUS_GRAM_JOB_SUBMIT"
#define GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART_STRING \
	"GLOBUS_GRAM_JOB_MANAGER_RESTART"
#define GLOBUS_GRAM_VALIDATE_STDIO_UPDATE_STRING \
	"GLOBUS_GRAM_JOB_MANAGER_STDIO_UPDATE"

#include "globus_common.h"
#include "globus_gram_job_manager.h"
#include "globus_gram_job_manager_validation.h"
#include "globus_rsl.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/**
 * Verbose debugging.
 *
 * Set this to GLOBUS_TRUE to get information about the status of the file
 * parsing and rsl validation in the job manager log file.
 */
static globus_bool_t globus_l_gram_job_manager_verbose_debugging = GLOBUS_FALSE;

static
int
globus_l_gram_job_manager_read_validation_file(
    globus_gram_jobmanager_request_t *	request,
    const char *			validation_filename);

static
int
globus_l_gram_job_manager_attribute_match(
    void *				datum,
    void *				args);

static
globus_bool_t
globus_l_gram_job_manager_validation_string_match(
    const char *			str1,
    const char *			str2);

static
int
globus_l_gram_job_manager_check_rsl_attributes(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_validation_when_t
    					when);

static
globus_bool_t
globus_l_gram_job_manager_attribute_exists(
    globus_list_t *			attributes,
    char *				attribute_name);

static
int
globus_l_gram_job_manager_insert_default_rsl(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_validation_when_t
    					when);

static
void
globus_l_gram_job_manager_free_validation_record(
    globus_gram_job_manager_validation_record_t *
    					record);

static
void
globus_l_gram_job_manager_validate_log(
    globus_gram_jobmanager_request_t *	request,
    const char *			fmt,
    ...);

static
int
globus_l_gram_job_manager_validation_rsl_error(
    const char *			attribute);

static
int
globus_l_gram_job_manager_validation_value_error(
    const char *			attribute);

static
int
globus_l_gram_job_manager_missing_value_error(
    const char *			attribute);

/**
 * @param request
 *        A job request. The validation field of this job request will be
 *        updated with a list of validation records constructed from the
 *        rsl validation files associated with the job manager.
 */
extern
int
globus_gram_job_manager_validation_init(
    globus_gram_jobmanager_request_t *	request)
{
    char *				validation_dir;
    char *				validation_filename;
    char *				scheduler_validation_filename;
    char *				tmp;
    int					rc = GLOBUS_SUCCESS;
    struct stat				s;
    globus_list_t *			tmp_list;
    globus_gram_job_manager_validation_record_t *
					record;
    globus_result_t 			result;

    request->validation_records = GLOBUS_NULL;

    result = globus_location(&validation_dir);
    if(result != GLOBUS_SUCCESS)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;
	goto globus_location_failed;
    }

    tmp = globus_libc_realloc(
	    validation_dir,
	    strlen(validation_dir) +
	    strlen("/share/globus-gram-job-manager-rsl-validation/") + 1);
    if(tmp == NULL)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;
	goto realloc_validation_dir_failed;
    }
    validation_dir = tmp;
    strcat(validation_dir, "/share/globus-gram-job-manager-rsl-validation/"); 

    validation_filename = globus_libc_malloc(
	    strlen(validation_dir) +
	    strlen("globus-gram-job-manager.rvf") + 1);
    if(validation_filename == NULL)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;
	goto malloc_validation_filename_failed;
    }
    scheduler_validation_filename = globus_libc_malloc(
	    strlen(validation_dir) +
	    strlen(request->jobmanager_type) +
	    strlen(".rvf") + 1);
    if(scheduler_validation_filename == NULL)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;
	goto malloc_scheduler_validation_filename_failed;
    }

    sprintf(validation_filename,
	    "%s%s",
	    validation_dir,
	    "globus-gram-job-manager.rvf");

    sprintf(scheduler_validation_filename,
	    "%s%s.rvf",
	    validation_dir,
	    request->jobmanager_type);

    /* Read in validation files. Do the generic job manager one first,
     * as the scheduler-specific one overrides it.
     */
    rc = globus_l_gram_job_manager_read_validation_file(
	    request,
	    validation_filename);

    if(rc != GLOBUS_SUCCESS)
    {
	goto read_validation_failed;
    }

    if(stat(scheduler_validation_filename, &s) == 0)
    {
	rc = globus_l_gram_job_manager_read_validation_file(
		request,
		scheduler_validation_filename);

	if(rc != GLOBUS_SUCCESS)
	{
	    goto read_scheduler_validation_failed;
	}
    }
read_scheduler_validation_failed:
    if(globus_l_gram_job_manager_verbose_debugging)
    {
        tmp_list = request->validation_records;

	while(!globus_list_empty(tmp_list))
	{
	    record = globus_list_first(tmp_list);
	    tmp_list = globus_list_rest(tmp_list);

	    globus_l_gram_job_manager_validate_log(
		    request,
		    "JMI: Validation Record:\n"
		    "attribute = '%s'\n"
		    "description = '%s'\n"
		    "required_when = '%d'\n"
		    "default_when = '%d'\n"
		    "default_values = '%s'\n"
		    "enumerated_values = '%s'\n\n",
		    record->attribute,
		    record->description ? record->description : "",
		    record->required_when,
		    record->default_when,
		    record->default_value ? record->default_value : "",
		    record->enumerated_values ? record->enumerated_values : "");
	}
    }
    if(rc != GLOBUS_SUCCESS)
    {
	while(!globus_list_empty(request->validation_records))
	{
	    globus_l_gram_job_manager_free_validation_record(
		    globus_list_remove(&request->validation_records,
				       request->validation_records));
	}
    }

read_validation_failed:
    globus_libc_free(scheduler_validation_filename);
malloc_scheduler_validation_filename_failed:
    globus_libc_free(validation_filename);
malloc_validation_filename_failed:
realloc_validation_dir_failed:
    globus_libc_free(validation_dir);
globus_location_failed:
    return rc;
}
/* globus_gram_job_manager_validation_init() */

/**
 * Validate a request's RSL.
 * @ingroup globus_gram_job_manager_rsl_validation
 *
 * Validate the RSL tree defining a job request, using validation files.
 * An RSL is valid if all required RSL parameters and defined in the RSL,
 * and if all RSL parameters in the RSL tree are supported by the job
 * manager and/or scheduler.
 *
 * As a side effect, the RSL will be modified to include any missing RSL
 * paramaters which have default values defined in one of the validation
 * files.
 *
 * @param request
 *        A job request. The rsl field of this job request is validated
 *        according to the rsl validation data stored in the two files
 *        passed in to this function.
 *
 * @return Returns GLOBUS_SUCCESS if the RSL is valid, and GLOBUS_FAILURE
 *         if it is not.
 * @see globus_gram_job_manager_rsl_validation_file
 */
int
globus_gram_job_manager_validate_rsl(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_validation_when_t
    					when)
{
    int					rc;

    /* First validation: RSL is a boolean "&" */
    if(!globus_rsl_is_boolean_and(request->rsl))
    {
	return GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL;
    }

    /*
     * Make sure all of the attributes match defined RSL validation records.
     */
    rc = globus_l_gram_job_manager_check_rsl_attributes(
	    request,
	    when);

    if(rc != GLOBUS_SUCCESS)
    {
	goto rsl_check_failed;
    }
    /*
     * Insert default RSL values where appropriate, make sure everything
     * which is required is defined.
     */
    rc = globus_l_gram_job_manager_insert_default_rsl(
	    request,
	    when);

rsl_check_failed:
    return rc;
}
/* globus_gram_job_manager_validate_rsl() */

/**
 * Parse a validation file.
 *
 * Parse the contents of a validation file, storing validation records
 * into the list pointed to by the request's @a validation_records datum.
 * Each record consists of a set of attribute-value pairs. All except
 * "attribute" are optional. Unrecognized attributes will be ignored.
 *
 * If an attribute appears twice in a validation file, or if it appears
 * in both the job manager and the scheduler-specific validation file,
 * then the second definition will be used, completely overriding the
 * initial definition.
 *
 * @param request
 *        The request structure containing information about the
 *        scheduler we will be using, and into which the validation
 *        records will be read.
 * @param validation_filename
 *        The name of the validation file to parse.
 *
 * @note This parser isn't very strict in what it accepts. The
 * parser ignores unrecognized attibutes, so a misplaced quote could
 * it to ignore large portions of the file.
 *
 * @retval GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_VALIDATION_FILE
 *         The validation file could not be opened.
 * @retval GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE
 *         The validation file could not be read or parsed.
 */
static
int
globus_l_gram_job_manager_read_validation_file(
    globus_gram_jobmanager_request_t *	request,
    const char *			validation_filename)
{
    FILE *				fp;
    int					length;
    globus_gram_job_manager_validation_record_t *
					tmp = NULL;
    char *				token_start;
    char *				token_end;
    char *				attribute;
    char *				value;
    globus_list_t *			node;
    char *				data = NULL;
    int					i;
    int					j;
    int					rc = GLOBUS_SUCCESS;

    fp = fopen(validation_filename, "r");

    if(fp == NULL)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_VALIDATION_FILE;

	goto error_exit;
    }

    if(fseek(fp, 0, SEEK_END) == -1)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

	goto close_exit;
    }
    if((length = ftell(fp)) == -1)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

	goto close_exit;
    }
    if(fseek(fp, 0, SEEK_SET) == -1)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

	goto close_exit;
    }

    token_start = data = globus_libc_malloc((size_t) length + 1);

    if(token_start == NULL)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

	goto close_exit;
    }

    if(fread(token_start, 1, (size_t) length, fp) != length)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

	goto close_exit;
    }
    token_start[(size_t) length] = '\0';

    while(*token_start)
    {
	while(*token_start)
	{
	    if(isspace(*token_start))
	    {
		token_start++;
	    }
	    else if(*token_start == '#')
	    {
		token_start = strchr(token_start, '\n');
	    }
	    else
	    {
		break;
	    }
	}
	token_end = strchr(token_start, ':');

	if(!token_end)
	{
	    break;
	}
	attribute = globus_libc_malloc(token_end - token_start + 1);
	if(attribute == NULL)
	{
	    rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;
	    goto error_exit;
	}
	memcpy(attribute, token_start, token_end - token_start);
	attribute[token_end-token_start] = '\0';
	token_start = token_end + 1; /* skip : */

	while(*token_start && isspace(*token_start))
	{
	    token_start++;
	}
	if(*token_start == '"')
	{
	    token_start++;

	    token_end = token_start;

	    do
	    {
		token_end++;
		token_end = strchr(token_end, '"');
	    }
	    while((*token_end) && *(token_end-1) == '\\');

	    value = globus_libc_malloc(token_end - token_start + 1);
	    if(value == NULL)
	    {
		rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

		goto error_exit;
	    }
	    for(i = 0, j = 0; token_start + i < token_end; i++)
	    {
		if(token_start[i] == '\\' && token_start[i+1] == '"')
		{
		    value[j++] = token_start[++i];
		}
		else if(!(isspace(token_start[i]) && isspace(token_start[i+1])))
		{
		    value[j++] = token_start[i];
		}
	    }
	    value[j] = '\0';
	    token_end++;

	    while(*token_end && *token_end != '\n')
	    {
		token_end++;
	    }
	    if(*token_end == '\n')
	    {
		token_end++;
	    }
	}
	else
	{
	    token_end = strchr(token_start, '\n');
	    if(token_end != NULL)
	    {
		value = globus_libc_malloc(token_end - token_start + 1);
		if(value == NULL)
		{
		    rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

		    goto error_exit;
		}
		memcpy(value, token_start, token_end - token_start);
		value[token_end - token_start] = '\0';
		token_end++;
	    }
	    else
	    {
		value = globus_libc_strdup(token_start);
		token_end = token_start + strlen(token_start);
	    }
	}
	if(tmp == GLOBUS_NULL)
	{
	    tmp = globus_libc_calloc(1, 
		    sizeof(globus_gram_job_manager_validation_record_t));
	    if(tmp == NULL)
	    {
		rc = GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE;

		goto error_exit;
	    }
	    /* Default to publishable */
	    tmp->publishable = GLOBUS_TRUE;
	}
	/* Compare token names against known attributes */
	if(strcasecmp(attribute, "attribute") == 0)
	{
	    tmp->attribute = value;
	}
	else if(strcasecmp(attribute, "description") == 0)
	{
	    tmp->description = value;
	}
	else if(strcasecmp(attribute, "requiredwhen") == 0)
	{
	    tmp->required_when = 0;

	    if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_SUBMIT_STRING))
	    {
		tmp->required_when |= GLOBUS_GRAM_VALIDATE_JOB_SUBMIT;
	    }
	    if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART_STRING))
	    {
		tmp->required_when |= GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART;
	    }
	    if(strstr(value, GLOBUS_GRAM_VALIDATE_STDIO_UPDATE_STRING))
	    {
		tmp->required_when |= GLOBUS_GRAM_VALIDATE_STDIO_UPDATE;
	    }
	    globus_libc_free(value);
	    value = GLOBUS_NULL;
	}
	else if(strcasecmp(attribute, "defaultwhen") == 0)
	{
	    tmp->default_when = 0;

	    if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_SUBMIT_STRING))
	    {
		tmp->default_when |= GLOBUS_GRAM_VALIDATE_JOB_SUBMIT;
	    }
	    if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART_STRING))
	    {
		tmp->default_when |= GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART;
	    }
	    if(strstr(value, GLOBUS_GRAM_VALIDATE_STDIO_UPDATE_STRING))
	    {
		tmp->default_when |= GLOBUS_GRAM_VALIDATE_STDIO_UPDATE;
	    }
	    globus_libc_free(value);
	    value = GLOBUS_NULL;
	}
	else if(strcasecmp(attribute, "validwhen") == 0)
	{
	    tmp->valid_when = 0;

	    if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_SUBMIT_STRING))
	    {
		tmp->valid_when |= GLOBUS_GRAM_VALIDATE_JOB_SUBMIT;
	    }
	    if(strstr(value, GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART_STRING))
	    {
		tmp->valid_when |= GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART;
	    }
	    if(strstr(value, GLOBUS_GRAM_VALIDATE_STDIO_UPDATE_STRING))
	    {
		tmp->valid_when |= GLOBUS_GRAM_VALIDATE_STDIO_UPDATE;
	    }
	    globus_libc_free(value);
	    value = GLOBUS_NULL;
	}
	else if(strcasecmp(attribute, "default") == 0)
	{
	    tmp->default_value = value;
	}
	else if(strcasecmp(attribute, "values") == 0)
	{
	    tmp->enumerated_values = value;
	}
	else if(strcasecmp(attribute, "publish") == 0)
	{
	    if(strcasecmp(value, "true") == 0)
	    {
		tmp->publishable = GLOBUS_TRUE;
	    }
	    else
	    {
		tmp->publishable = GLOBUS_FALSE;
	    }
	    globus_libc_free(value);
	    value = GLOBUS_NULL;
	}
	else
	{
	    globus_l_gram_job_manager_validate_log(
		    request,
		    "Ignoring Unknown attribute in %s: '%s: %s'\n",
		    validation_filename,
		    attribute,
		    value);

	    /* unknown attribute.... ignore */
	    globus_libc_free(value);
	    value = GLOBUS_NULL;
	}
	globus_libc_free(attribute);
	attribute = GLOBUS_NULL;

	token_start = token_end;

	/* Eat whitespace on end of record entry */
	while(*token_start && isspace(*token_start))
	{
	    if(*token_start == '\n')
	    {
		break;
	    }
	    else
	    {
		token_start++;
	    }
	}
	/* If record entry is followed by blank line or eof, then
	 * store entry in list
	 */
	if(*token_start == '\0' || *token_start == '\n')
	{
	    node = globus_list_search_pred(
		    request->validation_records, 
		    globus_l_gram_job_manager_attribute_match,
		    tmp->attribute);
	    if(node)
	    {
		/*
		 * Validation record already exists, replace it with new
		 * values
		 */
		globus_l_gram_job_manager_free_validation_record(
			globus_list_remove(&request->validation_records, node));
	    }

	    /* Insert into validation record list */
	    globus_list_insert(&request->validation_records, tmp);
	    tmp = GLOBUS_NULL;
	}
    }

close_exit:
    fclose(fp);
error_exit:
    if(data)
    {
	globus_libc_free(data);
    }
    if(tmp)
    {
	globus_l_gram_job_manager_free_validation_record(tmp);
    }
    return rc;
}
/* globus_l_gram_job_manager_read_validation_file() */

/**
 * Attribute name matching search predicate.
 *
 * Compares a validation record against the desired attribute name. Used
 * as a predicate in globus_list_search_pred().
 *
 * @param datum
 *        A void * cast of a validation record.
 * @param args
 *        A void * cast of the desired attribute name.
 */
static
int
globus_l_gram_job_manager_attribute_match(
    void *				datum,
    void *				args)
{
    globus_gram_job_manager_validation_record_t *
					tmp = datum;

    return globus_l_gram_job_manager_validation_string_match(
		tmp->attribute,
		args);
}
/* globus_l_gram_job_manager_attribute_match() */

static
globus_bool_t
globus_l_gram_job_manager_validation_string_match(
    const char *			str1,
    const char *			str2)
{
    while(str1 && *str1 && str2 && *str2)
    {
	if(*str1 == '_')
	{
	    str1++;
	}
	else if(*str2 == '_')
	{
	    str2++;
	}
	else if(tolower(*str1) == tolower(*str2))
	{
	    str1++;
	    str2++;
	}
	else
	{
	    return GLOBUS_FALSE;
	}
    }
    if(str1 && str2 && (*str1 || *str2))
    {
	return GLOBUS_FALSE;
    }

    return GLOBUS_TRUE;
}
/* globus_l_gram_job_manager_validation_string_match() */

/**
 * Validate RSL attributes
 *
 * Checks that all of the RSL attributes in the request's RSL match
 * a validation record. If an RSL has an enumerated list of values,
 * then the value of the RSL is compared against that list.
 *
 * @param request
 *        The job request containing the RSL to validate.
 * @param when
 *        Which RSL validation time scope we will use to decide
 *        whether to use the default values or not.
 */
static
int
globus_l_gram_job_manager_check_rsl_attributes(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_validation_when_t
    					when)
{
    globus_list_t *			operands;
    globus_list_t *			node;
    globus_rsl_t *			rsl = request->rsl;
    globus_rsl_t *			relation;
    char *				attribute;
    char *				value_str;
    globus_gram_job_manager_validation_record_t *
					record;
    globus_rsl_value_t *		value;

    operands = globus_rsl_boolean_get_operand_list(rsl);

    /* Check to make sure that every attribute is recognized by this
     * job manager.
     */
    while(!globus_list_empty(operands))
    {
	relation = globus_list_first(operands);
	operands = globus_list_rest(operands);

	if(!globus_rsl_is_relation_eq(relation))
	{
	    globus_l_gram_job_manager_validate_log(
		request,
		"JMI: RSL contains something besides an \"=\" relation\n");

	    return GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL;
	}
	attribute = globus_rsl_relation_get_attribute(relation);

	node = globus_list_search_pred(
		request->validation_records,
		globus_l_gram_job_manager_attribute_match,
		attribute);

	if(!node)
	{
	    globus_l_gram_job_manager_validate_log(
		request,
		"RSL attribute '%s' is not in the validation file!\n",
		attribute);
	    return GLOBUS_GRAM_PROTOCOL_ERROR_PARAMETER_NOT_SUPPORTED;
	}

	record = globus_list_first(node);

	/* Check valid_when */
	if((record->valid_when & when) == 0)
	{
	    globus_l_gram_job_manager_validate_log(
		request,
		"RSL attribute '%s' is not valid when %d\n",
		when);

	    switch(when)
	    {
	      case GLOBUS_GRAM_VALIDATE_JOB_SUBMIT:
	        return GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SUBMIT_ATTRIBUTE;
	      case GLOBUS_GRAM_VALIDATE_JOB_MANAGER_RESTART:
	        return GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_RESTART_ATTRIBUTE;
	      case GLOBUS_GRAM_VALIDATE_STDIO_UPDATE:
	        return GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDIO_UPDATE_ATTRIBUTE;
	    }
	}
	/* Check enumerated values if applicable */
	if(record->enumerated_values)
	{
	    value = globus_rsl_relation_get_single_value(relation);

	    if(!value)
	    {
		return
		    globus_l_gram_job_manager_validation_rsl_error(attribute);
	    }
	    value_str = globus_rsl_value_literal_get_string(value);
	    if(!value_str)
	    {
		return globus_l_gram_job_manager_validation_rsl_error(
			attribute);
	    }
	    if(strstr(record->enumerated_values, value_str) == GLOBUS_NULL)
	    {
		globus_l_gram_job_manager_validate_log(
		    request,
		    "RSL attribute %s's value is not in the enumerated set\n",
		    attribute);

		return globus_l_gram_job_manager_validation_value_error(
			    attribute);
	    }
	}
    }

    return GLOBUS_SUCCESS;
}
/* globus_l_gram_job_manager_check_rsl_attributes() */

/**
 * Add default values to RSL and verify required parameters
 *
 * Inserts default values to RSL when an RSL parameter is not defined
 * in it. After this is complete, it checks that all RSL parameters
 * with the "required_when" flag set are present in the RSL tree.
 *
 * @param request
 *        Request which contains the RSL tree to validate.
 * @param when
 *        Which RSL validation time scope we will use to decide
 *        whether to use the default values or not.
 */
static
int
globus_l_gram_job_manager_insert_default_rsl(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_validation_when_t
    					when)
{
    globus_gram_job_manager_validation_record_t *
					record;
    globus_list_t **			attributes;
    globus_rsl_t *			new_relation;
    char *				new_relation_str;
    globus_rsl_t *			rsl = request->rsl;
    globus_list_t *			validation_records;

    attributes = globus_rsl_boolean_get_operand_list_ref(rsl);

    validation_records = request->validation_records;

    while(!globus_list_empty(validation_records))
    {
	record = globus_list_first(validation_records);
	validation_records = globus_list_rest(validation_records);

	if(record->default_value && (record->default_when&when))
	{
	    if(!globus_l_gram_job_manager_attribute_exists(
			*attributes,
			record->attribute))
	    {
		new_relation_str = globus_libc_malloc(
			strlen(record->attribute) +
			strlen(record->default_value) + 
			strlen("%s = %s"));

		sprintf(new_relation_str,
			"%s = %s",
			record->attribute,
			record->default_value);

		globus_l_gram_job_manager_validate_log(
		    request,
		    "Nope, adding default RSL of %s\n",
		    new_relation_str);

		new_relation = globus_rsl_parse(new_relation_str);

		globus_list_insert(attributes, new_relation);

		globus_libc_free(new_relation_str);
	    }
	    else
	    {
		globus_l_gram_job_manager_validate_log(
		    request,
		    "Yes\n");
	    }
	}
	if(record->required_when & when)
	{
	    globus_l_gram_job_manager_validate_log(
		request,
		"Checking whether required attribute %s is in "
		"user RSL spec...",
		record->attribute);

	    if(!globus_l_gram_job_manager_attribute_exists(
			*attributes,
			record->attribute))
	    {
		globus_l_gram_job_manager_validate_log(
			request,
			"No, invalid RSL\n");

		return globus_l_gram_job_manager_missing_value_error(
			    record->attribute);
	    }
	    else
	    {
		globus_l_gram_job_manager_validate_log(
			request,
			"Yes, valid RSL\n");
	    }
	}
    }
    return GLOBUS_SUCCESS;
}
/* globus_l_gram_job_manager_insert_default_rsl() */

/**
 * Check that a relation for a required RSL attribute is present.
 *
 * @param attributes
 *        List of relations which are part of the job request's
 *        RSL.
 * @param attribute_name
 *        The name of the attribute to search for.
 */
static
globus_bool_t
globus_l_gram_job_manager_attribute_exists(
    globus_list_t *			attributes,
    char *				attribute_name)
{
    char *				tmp;
    globus_rsl_t *			relation;

    while(!globus_list_empty(attributes))
    {
	relation = globus_list_first(attributes);
	attributes = globus_list_rest(attributes);
	tmp = globus_rsl_relation_get_attribute(relation);

	if(globus_l_gram_job_manager_validation_string_match(
		    tmp,
		    attribute_name))
	{
	    return GLOBUS_TRUE;
	}
    }
    return GLOBUS_FALSE;
}
/* globus_l_gram_job_manager_attribute_exists() */

/**
 * Free a validation record
 *
 * Frees all strings referenced by the validation record, and 
 * then frees the record itself.
 *
 * @param record
 *        The record to free.
 */
static
void
globus_l_gram_job_manager_free_validation_record(
    globus_gram_job_manager_validation_record_t *
    					record)
{
    if(!record)
    {
	return;
    }
    if(record->attribute)
    {
	globus_libc_free(record->attribute);
    }
    if(record->description)
    {
	globus_libc_free(record->description);
    }
    if(record->default_value)
    {
	globus_libc_free(record->default_value);
    }
    if(record->enumerated_values)
    {
	globus_libc_free(record->enumerated_values);
    }
    globus_libc_free(record);
}
/* globus_l_gram_job_manager_free_validation_record() */

static
void
globus_l_gram_job_manager_validate_log(
    globus_gram_jobmanager_request_t *	request,
    const char *			fmt,
    ...)
{
#ifdef BUILD_DEBUG
    va_list				ap;

    va_start(ap, fmt);
    if(globus_l_gram_job_manager_verbose_debugging)
    {
	globus_libc_vfprintf(request->jobmanager_log_fp, fmt, ap);
    }
    va_end(ap);
#endif
}
/* globus_l_gram_job_manager_validate_log() */

#define HANDLE_RSL_ERROR(param,error) \
    if(globus_l_gram_job_manager_validation_string_match( \
		attribute, param)) \
    { \
        return error; \
    }

/**
 * Decide what type of RSL error to return when the value of @a attribute.
 * is not of the appropriate type.
 *
 * @param attribute
 *        Attribute to check.
 *
 * @note This should go away when we have better error reporting in the
 *       GRAM protocol.
 */
static
int
globus_l_gram_job_manager_validation_rsl_error(
    const char *			attribute)
{
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_ARGUMENTS_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ARGUMENTS)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_COUNT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_COUNT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_DIR_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DIRECTORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_DRY_RUN_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DRYRUN)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_ENVIRONMENT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ENVIRONMENT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_EXECUTABLE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EXECUTABLE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_FILE_CLEANUP_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_CLEANUP)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_FILE_STAGE_IN_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_IN)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_FILE_STAGE_IN_SHARED_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_IN_SHARED)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_FILE_STAGE_OUT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_OUT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_GASS_CACHE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_GASS_CACHE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MYJOB_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MYJOB)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_HOST_COUNT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_HOST_COUNT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_JOB_TYPE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_JOBTYPE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_CPU_TIME_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_CPU_TIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_MEMORY_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_MEMORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_TIME_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAXTIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_WALL_TIME_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_WALL_TIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MIN_MEMORY_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MIN_MEMORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_PROJECT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_PROJECT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_QUEUE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_QUEUE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_REMOTE_IO_URL_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_REMOTE_IO_URL)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_RESTART_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_RESTART)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_SAVE_STATE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SAVE_STATE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_SCRATCHDIR_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCRATCH)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDERR_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDERR)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDERR_POSITION_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDERR_POSITION)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDIN_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDIN)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDOUT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDOUT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDOUT_POSITION_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDOUT_POSITION)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_TWO_PHASE_COMMIT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_RSL_TWO_PHASE_COMMIT)

    return GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCHEDULER_SPECIFIC;
}
/* globus_l_gram_job_manager_validation_rsl_error() */

static
int
globus_l_gram_job_manager_validation_value_error(
    const char *			attribute)
{
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_COUNT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_COUNT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MYJOB_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_GRAM_MYJOB)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_HOST_COUNT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_HOST_COUNT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_JOB_TYPE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOBTYPE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_CPU_TIME_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_CPU_TIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_MEMORY_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_MEMORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_TIME_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAXTIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MAX_WALL_TIME_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_WALL_TIME)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_MIN_MEMORY_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MIN_MEMORY)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_PROJECT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_PROJECT)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_QUEUE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_QUEUE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_SAVE_STATE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SAVE_STATE)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_SCRATCHDIR_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SCRATCH)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDERR_POSITION_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDERR_POSITION)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_STDOUT_POSITION_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDOUT_POSITION)
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_TWO_PHASE_COMMIT_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_TWO_PHASE_COMMIT)

    return GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCHEDULER_SPECIFIC;
}
/* globus_l_gram_job_manager_validation_value_error() */

static
int
globus_l_gram_job_manager_missing_value_error(
    const char *			attribute)
{
    HANDLE_RSL_ERROR(GLOBUS_GRAM_PROTOCOL_EXECUTABLE_PARAM,
	             GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE)

    return GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_ATTRIBUTE;
}
/* globus_l_gram_job_manager_missing_value_error() */

#endif /* !GLOBUS_DONT_DOCUMENT_INTERNAL */
