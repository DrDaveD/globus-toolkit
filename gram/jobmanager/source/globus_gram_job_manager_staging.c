#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gram_job_manager_staging.c GRAM Job Manager Staging Tracking
 *
 * CVS Information:
 *
 * $Source$
 * $Date$
 * $Revision$
 * $Author$
 */

#include "globus_gram_job_manager.h"
#include "globus_rsl_assist.h"

#include <string.h>

#endif

static
int
globus_l_gram_job_manager_staging_add_pair(
    globus_gram_jobmanager_request_t *	request,
    globus_rsl_value_t *		from,
    globus_rsl_value_t *		to,
    const char *			type);

static
globus_bool_t
globus_l_gram_job_manager_staging_match(
    void *				datum,
    void *				arg);

static
void
globus_l_gram_job_manager_staging_free_all(
    globus_gram_jobmanager_request_t *	request);

int
globus_gram_job_manager_staging_create_list(
    globus_gram_jobmanager_request_t *	request)
{
    int					i;
    int					rc;
    globus_rsl_value_t *		from;
    globus_rsl_value_t *		to;
    globus_rsl_t *			tmp_rsl;
    globus_list_t *			list;
    globus_list_t *			pairs;
    char *				can_stage_list[] =
    {
	GLOBUS_GRAM_PROTOCOL_FILE_STAGE_IN_PARAM,
	GLOBUS_GRAM_PROTOCOL_FILE_STAGE_IN_SHARED_PARAM,
	GLOBUS_GRAM_PROTOCOL_FILE_STAGE_OUT_PARAM,
	NULL
    };

    if(request->jm_restart)
    {
	return GLOBUS_SUCCESS;
    }
    tmp_rsl = globus_rsl_parse(request->rsl_spec);
    globus_rsl_assist_attributes_canonicalize(tmp_rsl);

    for(i = 0; can_stage_list[i] != NULL; i++)
    {
	list = globus_rsl_param_get_values(tmp_rsl, can_stage_list[i]);

	if(!list)
	{
	    continue;
	}

	while(!globus_list_empty(list))
	{
	    pairs = globus_rsl_value_sequence_get_value_list(
		    globus_list_first(list));
	    list = globus_list_rest(list);

	    from = globus_list_first(pairs);
	    to = globus_list_first(globus_list_rest(pairs));

	    rc = globus_l_gram_job_manager_staging_add_pair(
		    request,
		    from,
		    to,
		    can_stage_list[i]);
	    if(rc != GLOBUS_SUCCESS)
	    {
		goto failed_adding_exit;
		
	    }
	}
    }
    globus_rsl_free_recursive(tmp_rsl);

    return GLOBUS_SUCCESS;
failed_adding_exit:
    globus_rsl_free_recursive(tmp_rsl);
    globus_l_gram_job_manager_staging_free_all(request);
    return rc;
}
/* globus_gram_job_manager_staging_create_list() */

int
globus_gram_job_manager_staging_remove(
    globus_gram_jobmanager_request_t *	request,
    globus_gram_job_manager_staging_type_t
    					type,
    char *				from,
    char *				to)
{
    globus_gram_job_manager_staging_info_t 
					query;
    globus_gram_job_manager_staging_info_t *
					item;
    globus_list_t **			list;
    globus_list_t *			node;

    globus_gram_job_manager_request_log(
	    request,
	    "JM: Finished staging (%s = (%s %s))\n",
	    type,
	    from,
	    to);

    query.evaled_from = from;
    query.evaled_to = to;
    query.type = type;

    switch(type)
    {
      case GLOBUS_GRAM_JOB_MANAGER_STAGE_IN:
	list = &request->stage_in_todo;
	break;
      case GLOBUS_GRAM_JOB_MANAGER_STAGE_IN_SHARED:
	list = &request->stage_in_shared_todo;
	break;
      case GLOBUS_GRAM_JOB_MANAGER_STAGE_OUT:
	list = &request->stage_out_todo;
	break;
    }

    node = globus_list_search_pred(
	    *list,
	    globus_l_gram_job_manager_staging_match,
	    &query);

    if(node)
    {
	item = globus_list_remove(list, node);

	globus_libc_free(item->from);
	globus_libc_free(item->to);
	globus_libc_free(item);

	globus_gram_job_manager_request_log(
	    request,
	    "JM: successfully removed (%s = (%s %s)) from todo list\n",
	    type,
	    from,
	    to);
    }
    else
    {
	globus_gram_job_manager_request_log(
	    request,
	    "JM: strange... (%s = (%s %s)) wasn't in the todo list\n",
	    type,
	    from,
	    to);
    }
    return GLOBUS_SUCCESS;
}
/* globus_gram_job_manager_staging_remove() */

int
globus_gram_job_manager_staging_write_state(
    globus_gram_jobmanager_request_t *	request,
    FILE *				fp)
{
    globus_list_t *			tmp_list;
    globus_gram_job_manager_staging_info_t *
					info;
    char *				tmp_str;

    fprintf(fp, "%d\n", globus_list_size(request->stage_in_todo));

    tmp_list = request->stage_in_todo;
    while(!globus_list_empty(tmp_list))
    {
	info = globus_list_first(tmp_list);
	tmp_list = globus_list_rest(tmp_list);

	tmp_str = globus_rsl_value_unparse(info->from);
	fprintf(fp, "%s\n", tmp_str);
	globus_libc_free(tmp_str);

	tmp_str = globus_rsl_value_unparse(info->to);
	fprintf(fp, "%s\n", tmp_str);
	globus_libc_free(tmp_str);
    }
    fprintf(fp, "%d\n", globus_list_size(request->stage_in_shared_todo));
    tmp_list = request->stage_in_shared_todo;
    while(!globus_list_empty(tmp_list))
    {
	info = globus_list_first(tmp_list);
	tmp_list = globus_list_rest(tmp_list);

	tmp_str = globus_rsl_value_unparse(info->from);
	fprintf(fp, "%s\n", tmp_str);
	globus_libc_free(tmp_str);

	tmp_str = globus_rsl_value_unparse(info->to);
	fprintf(fp, "%s\n", tmp_str);
	globus_libc_free(tmp_str);
    }
    tmp_list = request->stage_out_todo;
    while(!globus_list_empty(tmp_list))
    {
	info = globus_list_first(tmp_list);
	tmp_list = globus_list_rest(tmp_list);

	tmp_str = globus_rsl_value_unparse(info->from);
	fprintf(fp, "%s\n", tmp_str);
	globus_libc_free(tmp_str);

	tmp_str = globus_rsl_value_unparse(info->to);
	fprintf(fp, "%s\n", tmp_str);
	globus_libc_free(tmp_str);
    }
    return GLOBUS_SUCCESS;
}
/* globus_gram_job_manager_staging_write_state() */

int
globus_gram_job_manager_staging_read_state(
    globus_gram_jobmanager_request_t *	request,
    FILE *				fp)
{
    char				buffer[8192];
    int					i;
    int					tmp_list_size;
    globus_gram_job_manager_staging_info_t *
					info;

    fscanf(fp, "%[^\n]%*c", buffer);
    tmp_list_size = atoi(buffer);

    for(i = 0; i < tmp_list_size; i++)
    {
	info = globus_libc_calloc(
		1,
		sizeof(globus_gram_job_manager_staging_info_t));

        info->type = GLOBUS_GRAM_JOB_MANAGER_STAGE_IN;

	fscanf(fp, "%[^\n]%*c", buffer);
	globus_gram_job_manager_rsl_parse_value(request, buffer, &info->from);

	fscanf(fp, "%[^\n]%*c", buffer);
	globus_gram_job_manager_rsl_parse_value(request, buffer, &info->to);

	globus_gram_job_manager_rsl_evaluate_value(
		request,
		info->from,
		&info->evaled_from);
	globus_gram_job_manager_rsl_evaluate_value(
		request,
		info->to,
		&info->evaled_to);

        globus_list_insert(&request->stage_in_todo, info);
    }
    fscanf(fp, "%[^\n]%*c", buffer);
    tmp_list_size = atoi(buffer);

    for(i = 0; i < tmp_list_size; i++)
    {
	info = globus_libc_calloc(
		1,
		sizeof(globus_gram_job_manager_staging_info_t));
        info->type = GLOBUS_GRAM_JOB_MANAGER_STAGE_IN_SHARED;

	fscanf(fp, "%[^\n]%*c", buffer);
	globus_gram_job_manager_rsl_parse_value(request, buffer, &info->from);

	fscanf(fp, "%[^\n]%*c", buffer);
	globus_gram_job_manager_rsl_parse_value(request, buffer, &info->to);

	globus_gram_job_manager_rsl_evaluate_value(
		request,
		info->from,
		&info->evaled_from);
	globus_gram_job_manager_rsl_evaluate_value(
		request,
		info->to,
		&info->evaled_to);

        globus_list_insert(&request->stage_in_shared_todo, info);
    }
    fscanf(fp, "%[^\n]%*c", buffer);
    tmp_list_size = atoi(buffer);

    for(i = 0; i < tmp_list_size; i++)
    {
	info = globus_libc_calloc(
		1,
		sizeof(globus_gram_job_manager_staging_info_t));
	info->type = GLOBUS_GRAM_JOB_MANAGER_STAGE_OUT;

	fscanf(fp, "%[^\n]%*c", buffer);
	globus_gram_job_manager_rsl_parse_value(request, buffer, &info->from);

	fscanf(fp, "%[^\n]%*c", buffer);
	globus_gram_job_manager_rsl_parse_value(request, buffer, &info->to);

	globus_gram_job_manager_rsl_evaluate_value(
		request,
		info->from,
		&info->evaled_from);
	globus_gram_job_manager_rsl_evaluate_value(
		request,
		info->to,
		&info->evaled_to);

        globus_list_insert(&request->stage_out_todo, info);
    }
    return GLOBUS_SUCCESS;
}
/* globus_gram_job_manager_staging_read_state() */

static
int
globus_l_gram_job_manager_staging_add_pair(
    globus_gram_jobmanager_request_t *	request,
    globus_rsl_value_t *		from,
    globus_rsl_value_t *		to,
    const char *			type)
{
    int					rc;
    globus_gram_job_manager_staging_info_t *
					info;

    info = globus_libc_calloc(
	    1,
	    sizeof(globus_gram_job_manager_staging_info_t));
    if(!info)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED;
	goto info_calloc_failed;
    }

    globus_gram_job_manager_rsl_evaluate_value(
	    request,
	    info->from,
	    &info->evaled_from);

    if(!info->evaled_from)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EVALUATION_FAILED;
	goto eval_from_failed;
    }
    globus_gram_job_manager_rsl_evaluate_value(
	    request,
	    info->to,
	    &info->evaled_to);

    if(!info->evaled_to)
    {
	rc = GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EVALUATION_FAILED;
	goto eval_to_failed;
    }

    if(strcmp(type, GLOBUS_GRAM_PROTOCOL_FILE_STAGE_IN_PARAM) == 0)
    {
	info->type = GLOBUS_GRAM_JOB_MANAGER_STAGE_IN;
	info->from = globus_rsl_value_copy_recursive(from);
	info->to = globus_rsl_value_copy_recursive(to);
	globus_list_insert(&request->stage_in_todo, info);
    }
    else if(strcmp(type, GLOBUS_GRAM_PROTOCOL_FILE_STAGE_IN_SHARED_PARAM)== 0)
    {
	info->type = GLOBUS_GRAM_JOB_MANAGER_STAGE_IN_SHARED;
	info->from = globus_rsl_value_copy_recursive(from);
	info->to = globus_rsl_value_copy_recursive(to);
	globus_list_insert(&request->stage_in_shared_todo, info);

    }
    else if(strcmp(type, GLOBUS_GRAM_PROTOCOL_FILE_STAGE_OUT_PARAM) == 0)
    {
	info->type = GLOBUS_GRAM_JOB_MANAGER_STAGE_OUT;
	info->from = globus_rsl_value_copy_recursive(from);
	info->to = globus_rsl_value_copy_recursive(to);
	globus_list_insert(&request->stage_out_todo, info);
    }
    return GLOBUS_SUCCESS;

eval_to_failed:
    globus_libc_free(info->evaled_from);
eval_from_failed:
    globus_libc_free(info);
info_calloc_failed:
    return rc;
}
/* globus_l_gram_job_manager_staging_add_url() */

static
globus_bool_t
globus_l_gram_job_manager_staging_match(
    void *				datum,
    void *				arg)
{
    globus_gram_job_manager_staging_info_t *
					item;
    globus_gram_job_manager_staging_info_t *
					query;

    item = datum;
    query = arg;

    globus_assert(item->type == query->type);

    if((strcmp(item->evaled_from, query->evaled_from) == 0) &&
       (strcmp(item->evaled_to, query->evaled_to) == 0))
    {
	return GLOBUS_TRUE;
    }
    else
    {
	return GLOBUS_FALSE;
    }
}
/* globus_l_gram_job_manager_staging_match() */

static
void
globus_l_gram_job_manager_staging_free_all(
    globus_gram_jobmanager_request_t *	request)
{
    globus_gram_job_manager_staging_info_t *
					info;

    while(!globus_list_empty(request->stage_in_todo))
    {
	info = globus_list_remove(&request->stage_in_todo,
		                  request->stage_in_todo);

	globus_rsl_value_free_recursive(info->from);
	globus_rsl_value_free_recursive(info->to);
	globus_libc_free(info->evaled_from);
	globus_libc_free(info->evaled_to);
	globus_libc_free(info);
    }
    while(!globus_list_empty(request->stage_in_shared_todo))
    {
	info = globus_list_remove(&request->stage_in_shared_todo,
		                  request->stage_in_shared_todo);

	globus_rsl_value_free_recursive(info->from);
	globus_rsl_value_free_recursive(info->to);
	globus_libc_free(info->evaled_from);
	globus_libc_free(info->evaled_to);
	globus_libc_free(info);
    }
    while(!globus_list_empty(request->stage_out_todo))
    {
	info = globus_list_remove(&request->stage_out_todo,
		                  request->stage_out_todo);

	globus_rsl_value_free_recursive(info->from);
	globus_rsl_value_free_recursive(info->to);
	globus_libc_free(info->evaled_from);
	globus_libc_free(info->evaled_to);
	globus_libc_free(info);
    }
}
/* globus_l_gram_job_manager_staging_free_all() */
