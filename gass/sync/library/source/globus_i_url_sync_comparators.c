/*
 * Copyright 1999-2006 University of Chicago
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

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_i_url_sync_comparators.c
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#include "globus_url_sync.h"
#include "globus_i_url_sync.h"
#include "globus_i_url_sync_handle.h"
#include "globus_i_url_sync_log.h"
#include "globus_ftp_client.h"
#include "globus_common.h"
#include "version.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

/* Types */

/** Monitor structured user locally to synchronize the asynch FTP calls. */
typedef struct
{
    globus_mutex_t                          mutex;
    globus_cond_t                           cond;
    globus_bool_t                           done;
} globus_l_url_sync_monitor_t;

/** Callback arg structure used in the chained comparison. */
typedef struct
{
	globus_list_t *							list;
	globus_url_sync_compare_func_cb_t		cb_func;
	void *									cb_arg;
}
globus_l_url_sync_chain_func_cb_arg_t;

/* Function declarations */

static
globus_result_t
globus_l_url_sync_ftpclient_mlst(
    globus_url_sync_endpoint_t *            endpoint);

static
void
globus_l_url_sync_ftpclient_complete_cb(
    void *                                  user_arg,
    globus_ftp_client_handle_t *            handle,
    globus_object_t *                       error);

static
globus_result_t
globus_l_url_sync_exists_func(
    void *                                      comparator_arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    globus_url_sync_compare_func_cb_t           callback_func,
    void *                                      callback_arg);

static
globus_result_t
globus_l_url_sync_size_func(
    void *                                      comparator_arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    globus_url_sync_compare_func_cb_t           callback_func,
    void *                                      callback_arg);

static
globus_result_t
globus_l_url_sync_modify_func(
    void *                                      comparator_arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    globus_url_sync_compare_func_cb_t           callback_func,
    void *                                      callback_arg);

static
globus_result_t
globus_l_url_sync_chain_func(
    void *                                      comparator_arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    globus_url_sync_compare_func_cb_t           callback_func,
    void *                                      callback_arg);

void
globus_url_sync_compare_chain_func_cb(
    void *                                      arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    int                                         result,
    globus_object_t *                           error);


/* Functions */

/**
 * Existence comparison function, including filetype checking.
 *
 * NOTE: This SHOULD be asynchronous but for now I made it synchronous to
 * simplify it.
 */
static
globus_result_t
globus_l_url_sync_exists_func(
    void *                                      comparator_arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    globus_url_sync_compare_func_cb_t           callback_func,
    void *                                      callback_arg)
{
    int 		comparison_result = -1;
    globus_object_t *   error_object = GLOBUS_NULL;
    globus_result_t	result = GLOBUS_SUCCESS;

    GlobusFuncName(globus_l_url_sync_exists_func);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_ENTER(callback_func, "callback");
    
    /* Stat the source */
	if (source->stats.type == globus_url_sync_endpoint_type_unknown)
		result = globus_l_url_sync_ftpclient_mlst(source);
	
    if (result != GLOBUS_SUCCESS)
    {
        globus_object_t * err = globus_error_get(result);
	int response_code = globus_error_ftp_error_get_code(err);
	globus_i_url_sync_log_debug("response = %d, result = %d, %s\n", 
		    response_code, result, globus_error_print_chain(err));

	/* *** use the real return code(s) which are not currently known *** */
	switch (result)
	  {
	  case 18:
            /* gridftp authentication error */
	    error_object = GLOBUS_I_URL_SYNC_ERROR_REMOTE("authentication required");
	    break;
	  case 11:
	  case 12:
	    error_object = GLOBUS_I_URL_SYNC_ERROR_REMOTE("authentication expired");
	    break;
	  default:
	    error_object = GLOBUS_I_URL_SYNC_ERROR_NOTFOUND();
	  } 
    }
    else 
    {
        /* Report an error if source file is not found */
        if (!source->stats.exists) 
	{
	    error_object = GLOBUS_I_URL_SYNC_ERROR_NOTFOUND();
	} 
	else 
        {
	    int dir_ending = 
	      (destination->url[strlen(destination->url)-1] == '/')? GLOBUS_TRUE: GLOBUS_FALSE;

	    /* If source is directory, make sure URL ends with "/". */
	    if (source->stats.type == globus_url_sync_endpoint_type_dir)
	    {
	        if (source->url[strlen(source->url)-1] != '/')
		    strcat(source->url, "/");
	    }
	    
	    /* Stat the destination */
	    if (destination->stats.type == globus_url_sync_endpoint_type_unknown)
	        globus_l_url_sync_ftpclient_mlst(destination);
       
	    /* Compare existence */
	    comparison_result = source->stats.exists - destination->stats.exists;
       
	    if (destination->stats.exists)
	    {
	        if (source->stats.type != destination->stats.type)
		  error_object = GLOBUS_I_URL_SYNC_ERROR_FILETYPE();
		else 
		{
		    if (source->stats.type == globus_url_sync_endpoint_type_dir)
		    {
		        if (!dir_ending) 
			    strcat(destination->url, "/");
		    } else {
		        if (dir_ending) {
			    error_object = GLOBUS_I_URL_SYNC_ERROR_FILETYPE();
			}
		    }
		}
	    }
	    else
	    {
	        if (source->stats.type == globus_url_sync_endpoint_type_dir)
		{
		    if (!dir_ending)
		        strcat(destination->url, "/");
		}
		else 
		{
		    if (dir_ending)
			    error_object = GLOBUS_I_URL_SYNC_ERROR_FILETYPE();
		}
	    }
	}
    }
					
    /* Not handling the ftpclient_mlst() results because... the ftp client
     * documentation seems to indicate that if a file does not exist, the
     * mlst operation may return an error. So an error is not really an error
     * in some cases... Ideally this should be better handled or confirmed in
     * the docs. */
    callback_func(
		  callback_arg, source, destination, comparison_result, error_object);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_EXIT(callback_func, "callback");
    return GLOBUS_SUCCESS;
}
/* globus_l_url_sync_exists_func */

/**
 * Size comparison function.
 */
static
globus_result_t
globus_l_url_sync_size_func(
    void *                                      comparator_arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    globus_url_sync_compare_func_cb_t           callback_func,
    void *                                      callback_arg)
{
    int                                         comparison_result;
    GlobusFuncName(globus_l_url_sync_size_func);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_ENTER(callback_func, "callback");
	
	comparison_result = source->stats.size - destination->stats.size;
	
    callback_func(callback_arg, source, destination, comparison_result, GLOBUS_NULL);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_EXIT(callback_func, "callback");
    return GLOBUS_SUCCESS;
}
/* globus_l_url_sync_size_func */

/**
 * Modify comparison function.
 */
static
globus_result_t
globus_l_url_sync_modify_func(
    void *                                      comparator_arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    globus_url_sync_compare_func_cb_t           callback_func,
    void *                                      callback_arg)
{
    int                                         comparison_result;
    GlobusFuncName(globus_l_url_sync_modify_func);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_ENTER(callback_func, "callback");
	
	comparison_result = (int) difftime(
			mktime(&(source->stats.modify_tm)),
			mktime(&(destination->stats.modify_tm)));
	
    callback_func(callback_arg, source, destination, comparison_result, GLOBUS_NULL);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_EXIT(callback_func, "callback");
    return GLOBUS_SUCCESS;
}
/* globus_l_url_sync_modify_func */

/**
 * Chained comparison function. Sets up the callback argument then immediately
 * hands execution to its own callback which does the rest of the work.
 */
static
globus_result_t
globus_l_url_sync_chain_func(
    void *                                      comparator_arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    globus_url_sync_compare_func_cb_t           callback_func,
    void *                                      callback_arg)
{
    globus_l_url_sync_chain_func_cb_arg_t *		chain_cb_arg;
    GlobusFuncName(globus_l_url_sync_chain_func);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_ENTER(callback_func, "callback");

    /* Create create callback argument for the chain cb's */
    chain_cb_arg = (globus_l_url_sync_chain_func_cb_arg_t *)
		    globus_libc_malloc(sizeof (globus_l_url_sync_chain_func_cb_arg_t));
    chain_cb_arg->list      =  (globus_list_t *) comparator_arg;
    chain_cb_arg->cb_func   =  callback_func;
    chain_cb_arg->cb_arg    =  callback_arg;

    /* Hand off execution to the cb function. By passing a '0' as the result
     * parameter, it ensures that the callback will attempt to call the next
     * comparator in the chain, if there is at least one in the chain. */
    globus_url_sync_compare_chain_func_cb_t(
			chain_cb_arg,
			source,
			destination,
			0,
			GLOBUS_NULL);

    GLOBUS_I_URL_SYNC_LOG_DEBUG_EXIT(callback_func, "callback");
    return GLOBUS_SUCCESS;
}
/* globus_l_url_sync_chain_func */

/**
 * Chain function callback. This callback is passed to all of the comparison
 * functions in the chain of comparators. If the results returned by the last
 * compare func indicates that the files are out of synch or if there was an
 * error, it returns the last comparison result and error to the user callback.
 * Otherwise, it looks for the next comparator in the chain, and calls it.
 */
void
globus_url_sync_compare_chain_func_cb(
    void *                                      arg,
    globus_url_sync_endpoint_t *                source,
    globus_url_sync_endpoint_t *                destination,
    int                                         result,
    globus_object_t *                           error)
{
    globus_l_url_sync_chain_func_cb_arg_t *		chain_cb_arg;
    globus_url_sync_comparator_t *			next_comparator;
    GlobusFuncName(globus_url_sync_compare_chain_func_cb_t);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_ENTER(0, "");

    globus_assert(arg);

    chain_cb_arg = (globus_l_url_sync_chain_func_cb_arg_t *) arg;
    if (globus_list_empty(chain_cb_arg->list) || result!=0 || error)
    {
        /* Call the user callback function when the evaluations in this chain
	 * reach a conclussion:
	 *  a. either there are no more comparisons left to perform,
	 *  b. or a comparison indicated source/dest are out of synch,
	 *  c. or an error has occurred.
	 *
	 * Call the user callback, then free the temporary callback arg
	 * structure.
	 */
        chain_cb_arg->cb_func(
		 chain_cb_arg->cb_arg, source, destination, result, error);
	GLOBUS_I_URL_SYNC_LOG_DEBUG_EXIT(chain_cb_arg->cb_func, "callback");
	globus_libc_free(chain_cb_arg);
    }
    else
    {
        /* Initiate next comparison in the list. */
        next_comparator = (globus_url_sync_comparator_t *)
	  globus_list_first(chain_cb_arg->list);
	chain_cb_arg->list = globus_list_rest(chain_cb_arg->list);

	/* Call next compare func */
	next_comparator->compare_func(
				next_comparator->comparator_arg,
				source,
				destination,
				globus_url_sync_compare_chain_func_cb_t,
				chain_cb_arg);
	GLOBUS_I_URL_SYNC_LOG_DEBUG_EXIT(next_comparator->compare_func, "compare_func");
    }

} /* globus_url_sync_compare_chain_func_cb*/

/**
 * A helper function for simplifying the MSLT operations.
 */
static
globus_result_t
globus_l_url_sync_ftpclient_mlst(
    globus_url_sync_endpoint_t *            endpoint)
{
    globus_result_t                         result;
    globus_l_url_sync_monitor_t             monitor;
    globus_byte_t *                         buffer;
    globus_size_t                           buffer_length;
    globus_ftp_client_operationattr_t       dummy;
    char			            name[GLOBUS_I_URL_SYNC_FILENAME_BUFLEN];
    GlobusFuncName(globus_l_url_sync_ftp_mlst);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_ENTER(endpoint->ftp_handle, endpoint->url);

    /* Initialize monitor */
    globus_mutex_init(&monitor.mutex, GLOBUS_NULL);
    globus_cond_init(&monitor.cond, GLOBUS_NULL);
    monitor.done = GLOBUS_FALSE;

    /* Initialize buffer */
    buffer = GLOBUS_NULL;
    buffer_length = 0;

    /* Create a dummy op attr to workaround gridftp bug in older clients */
    globus_ftp_client_operationattr_init(&dummy);

    /* MSLT */
    result = globus_ftp_client_mlst(
            endpoint->ftp_handle,
            endpoint->url,
            &dummy, /* operation attribute optional */
            &endpoint->mlst_buffer,
            &endpoint->mlst_buffer_length,
            globus_l_url_sync_ftpclient_complete_cb,
            &monitor);

    if (result != GLOBUS_SUCCESS)
        goto cleanexit;

    /* Wait for completion */
    globus_mutex_lock(&monitor.mutex);
    {
        while(!monitor.done)
        {
            globus_cond_wait(&monitor.cond, &monitor.mutex);
        }
    }
    globus_mutex_unlock(&monitor.mutex);

    /* Parse MSLT buffer */
    if (endpoint->mlst_buffer_length)
    {
        globus_url_sync_l_parse_mlst_buffer(endpoint, endpoint->mlst_buffer, name);
    }
    
  cleanexit:
    globus_cond_destroy(&monitor.cond);
    globus_mutex_destroy(&monitor.mutex);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_EXIT(endpoint->ftp_handle, endpoint->url);
    return result;
}
/* globus_l_url_sync_ftpclient_mlst */

/*
 * The operation complete callback for the ftp client operation.
 */
static
void
globus_l_url_sync_ftpclient_complete_cb(
    void *                                  user_arg,
    globus_ftp_client_handle_t *            handle,
    globus_object_t *                       error)
{
    globus_l_url_sync_monitor_t *           monitor;
    GlobusFuncName(globus_l_url_sync_ftpclient_complete_cb);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_ENTER(0, "");

    monitor = (globus_l_url_sync_monitor_t *) user_arg;

    if(error)
    {
        globus_i_url_sync_log_error(error);
    }

    /* Signal monitor */
    globus_mutex_lock(&monitor->mutex);
    {
        monitor->done = GLOBUS_TRUE;
        globus_cond_signal(&monitor->cond);
    }
    globus_mutex_unlock(&monitor->mutex);
    GLOBUS_I_URL_SYNC_LOG_DEBUG_EXIT(0, "");
}
/* globus_l_url_sync_ftpclient_complete_cb */

/* Chained comparator functions */
void
globus_url_sync_chained_comparator_init(
    globus_url_sync_comparator_t *					chain)
{
	chain->comparator_arg  =  GLOBUS_NULL;
	chain->compare_func    =  globus_l_url_sync_chain_func;
}

void
globus_url_sync_chained_comparator_destroy(
    globus_url_sync_comparator_t *					chain)
{
	globus_assert(chain);

	if (chain->comparator_arg)
		globus_list_free(chain->comparator_arg);
}

void
globus_url_sync_chained_comparator_add(
    globus_url_sync_comparator_t *					chain,
    globus_url_sync_comparator_t *					next)
{
	globus_assert(chain);
	chain->comparator_arg = globus_list_cons(
			next, (globus_list_t *) chain->comparator_arg);
}


/* Variables */

globus_url_sync_comparator_t    globus_url_sync_comparator_exists =
{
    GLOBUS_NULL,
    globus_l_url_sync_exists_func
};

globus_url_sync_comparator_t    globus_url_sync_comparator_size =
{
    GLOBUS_NULL,
    globus_l_url_sync_size_func
};

globus_url_sync_comparator_t    globus_url_sync_comparator_modify =
{
    GLOBUS_NULL,
    globus_l_url_sync_modify_func
};

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */

