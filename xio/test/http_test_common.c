#include "globus_common.h"
#include "http_test_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globus_xio.h"
#include "globus_xio_http.h"
#include "globus_xio_tcp_driver.h"

typedef struct
{
    char *                              uri;
    globus_xio_http_request_ready_callback_t
                                        callback;
    void *                              arg;
}
http_test_uri_handler_t;

static
void
http_l_test_server_accept_callback(
    globus_xio_server_t                 server,
    globus_xio_target_t                 target,
    globus_result_t                     result,
    void *                              user_arg);

static
void
http_l_test_server_open_callback(
    globus_xio_handle_t                 handle,
    globus_result_t                     result,
    void *                              user_arg);

static
void
http_l_test_server_close_callback(
    globus_xio_handle_t                 handle,
    globus_result_t                     result,
    void *                              user_arg);

static
void
http_l_test_server_request_callback(
    void *                              user_arg,
    globus_result_t                     result,
    const char *                        method,
    const char *                        uri,
    globus_xio_http_version_t           http_version,
    globus_hashtable_t                  headers);

int
http_test_initialize(
    globus_xio_driver_t *               tcp_driver,
    globus_xio_driver_t *               http_driver,
    globus_xio_stack_t *                stack)
{
    globus_result_t                     result;
    int                                 rc;

    rc = globus_module_activate(GLOBUS_COMMON_MODULE);
    if (rc != 0)
    {
        rc = 1;

        fprintf(stderr, "Error activation GLOBUS_COMMON\n");

        goto error_exit;
    }

    rc = globus_module_activate(GLOBUS_XIO_MODULE);
    if (rc != 0)
    {
        fprintf(stderr, "Error activating GLOBUS_XIO\n");
        rc = 2;

        goto deactivate_exit;
    }

    result = globus_xio_driver_load("tcp", tcp_driver);
    if (result != GLOBUS_SUCCESS)
    {
        fprintf(stderr,
                "Error loading tcp driver: %s\n",
                globus_object_printable_to_string(globus_error_peek(result)));

        rc = 10;

        goto deactivate_exit;
    }
    result = globus_xio_driver_load("http", http_driver);
    if (result != GLOBUS_SUCCESS)
    {
        fprintf(stderr,
                "Error loading http driver: %s\n",
                globus_object_printable_to_string(globus_error_peek(result)));

        rc = 11;

        goto unload_tcp_exit;
    }

    result = globus_xio_stack_init(stack, NULL);
    if (result != GLOBUS_SUCCESS)
    {
        fprintf(stderr,
                "Error initializing xio stack: %s\n",
                globus_object_printable_to_string(globus_error_peek(result)));
        rc = 12;

        goto unload_http_exit;
    }
    result = globus_xio_stack_push_driver(
            *stack,
            *tcp_driver);
    if (result != GLOBUS_SUCCESS)
    {
        fprintf(stderr,
                "Error pushing tcp onto stack: %s\n",
                globus_object_printable_to_string(globus_error_peek(result)));
        rc = 13;

        goto destroy_stack_exit;
    }

    result = globus_xio_stack_push_driver(
            *stack,
            *http_driver);
    if (result != GLOBUS_SUCCESS)
    {
        fprintf(stderr,
                "Error pushing http onto stack: %s\n",
                globus_object_printable_to_string(globus_error_peek(result)));
        rc = 14;

        goto destroy_stack_exit;
    }

    return 0;

destroy_stack_exit:
    globus_xio_stack_destroy(*stack);
unload_http_exit:
    globus_xio_driver_unload(*http_driver);
unload_tcp_exit:
    globus_xio_driver_unload(*tcp_driver);
deactivate_exit:
    globus_module_deactivate_all();
error_exit:
    return rc;
}
/* initialize() */


globus_result_t
http_test_server_init(
    http_test_server_t *                server,
    globus_xio_driver_t                 tcp_driver,
    globus_xio_driver_t                 http_driver,
    globus_xio_stack_t                  stack)
{
    int                                 rc;
    globus_result_t                     result;
    globus_xio_attr_t                   attr;
    GlobusXIOName(http_test_server_init);

    memset(server, '\0', sizeof(http_test_server_t));

    server->http_driver = http_driver;
    server->tcp_driver = tcp_driver;

    globus_hashtable_init(
            &server->uri_handlers,
            16,
            globus_hashtable_string_hash,
            globus_hashtable_string_keyeq);

    rc = globus_mutex_init(&server->mutex, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorMemory("mutex");
        goto error_exit;
    }
    rc = globus_cond_init(&server->cond, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorMemory("cond");
        goto free_mutex_error;
    }
    result = globus_xio_attr_init(&attr);

    if (result != GLOBUS_SUCCESS)
    {
        goto free_cond_error;
    }

    /*
    result = globus_xio_attr_cntl(
        attr,
        tcp_driver,
        GLOBUS_XIO_TCP_SET_LINGER,
        GLOBUS_TRUE,
        1200);
    if (result != GLOBUS_SUCCESS)
    {
        goto destroy_attr_error;
    }
    */

    result = globus_xio_server_create(
        &server->server, 
        attr,
        stack);

    if (result != GLOBUS_SUCCESS)
    {
        goto destroy_attr_error;
    }
    result = globus_xio_server_cntl(
            server->server,
            tcp_driver,
            GLOBUS_XIO_TCP_GET_LOCAL_CONTACT,
            &server->contact);
    if (result != GLOBUS_SUCCESS)
    {
        goto destroy_server_error;
    }

    globus_xio_attr_destroy(attr);

    return result;

destroy_server_error:
    globus_xio_server_close(server->server);
destroy_attr_error:
    globus_xio_attr_destroy(attr);
free_cond_error:
    globus_cond_destroy(&server->cond);
free_mutex_error:
    globus_mutex_destroy(&server->mutex);
error_exit:
    return result;
}
/* http_test_server_init() */

globus_result_t
http_test_server_register_handler(
    http_test_server_t *                server,
    const char *                        uri,
    globus_xio_http_request_ready_callback_t
                                        ready_callback,
    void *                              arg)
{
    globus_result_t                     result;
    http_test_uri_handler_t *           handler;
    GlobusXIOName(http_test_server_register_handler);

    globus_mutex_lock(&server->mutex);
    handler = globus_hashtable_remove(&server->uri_handlers, (void*) uri);

    if (handler == NULL)
    {
        handler = globus_libc_malloc(sizeof(http_test_uri_handler_t));
        if (handler == NULL)
        {
            result = GlobusXIOErrorMemory("handler");

            goto error_exit;
        }
        handler->uri = globus_libc_strdup(uri);
        if (handler->uri == NULL)
        {
            result = GlobusXIOErrorMemory("uri");
            goto free_handler_error;
        }
    }

    handler->callback = ready_callback;
    handler->arg = arg;

    globus_hashtable_insert(&server->uri_handlers, handler->uri, handler);
    globus_mutex_unlock(&server->mutex);

    return GLOBUS_SUCCESS;

free_handler_error:
    globus_libc_free(handler);
error_exit:
    globus_mutex_unlock(&server->mutex);

    return result;
}
/* http_test_server_register_handler() */

globus_result_t
http_test_server_run(
    http_test_server_t *                server)
{
    globus_result_t                     result;

    globus_mutex_lock(&server->mutex);
    while (! server->shutdown)
    {
        while ((!server->shutdown) && server->outstanding_operation == 1)
        {
            globus_cond_wait(&server->cond, &server->mutex);
        }

        if ((!server->shutdown) && (server->outstanding_operation == 0))
        {
            result = globus_xio_server_register_accept(
                    server->server,
                    NULL,
                    http_l_test_server_accept_callback,
                    server);

            if (result == GLOBUS_SUCCESS)
            {
                server->outstanding_operation++;
            }
            else
            {
                fprintf(stderr,
                        "Error registering accept: %s\n",
                        globus_object_printable_to_string(
                            globus_error_peek(result)));
            }
        }
    }

    while (server->shutdown && server->outstanding_operation > 0)
    {
        globus_cond_wait(&server->cond, &server->mutex);
    }
    server->shutdown_done = GLOBUS_TRUE;
    globus_mutex_unlock(&server->mutex);

    return GLOBUS_SUCCESS;
}
/* http_test_server_run() */

globus_result_t
http_test_server_shutdown(
    http_test_server_t *                server)
{
    globus_mutex_lock(&server->mutex);
    server->shutdown = GLOBUS_TRUE;
    globus_cond_broadcast(&server->cond);
    globus_mutex_unlock(&server->mutex);

    return GLOBUS_SUCCESS;
}
/* http_test_server_shutdown() */

void
http_test_server_destroy(
    http_test_server_t *                server)
{
    http_test_server_shutdown(server);

    globus_mutex_lock(&server->mutex);
    while (!server->shutdown_done)
    {
        globus_cond_wait(&server->cond, &server->mutex);
    }
    globus_mutex_unlock(&server->mutex);
    globus_xio_server_close(server->server);
    globus_mutex_destroy(&server->mutex);
    globus_cond_destroy(&server->cond);

    memset(server, '\0', sizeof(http_test_server_t));

    return ;
}
/* http_test_server_destroy() */

globus_result_t
http_test_server_respond(
    http_test_server_t *                test_server,
    int                                 status_code,
    char *                              reason_phrase,
    globus_xio_http_header_t *          header_array,
    size_t                              header_array_len)
{
    int                                 i;
    globus_result_t                     result;

    globus_mutex_lock(&test_server->mutex);

    if (status_code != 0)
    {
        result = globus_xio_handle_cntl(
                test_server->handle,
                test_server->http_driver,
                GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                status_code);

        if (result != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
    }

    if (reason_phrase != NULL)
    {
        result = globus_xio_handle_cntl(
                test_server->handle,
                test_server->http_driver,
                GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_REASON_PHRASE,
                reason_phrase);

        if (result != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
    }

    for (i = 0 ; i < header_array_len; i++)
    {
        result = globus_xio_handle_cntl(
                test_server->handle,
                test_server->http_driver,
                GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                header_array[i].name,
                header_array[i].value);

        if (result != GLOBUS_SUCCESS)
        {
            goto error_exit;
        }
    }
    globus_mutex_unlock(&test_server->mutex);

    return GLOBUS_SUCCESS;

error_exit:
    globus_mutex_unlock(&test_server->mutex);
    return result;
}
/* http_test_server_respond() */

globus_result_t
http_test_server_close_handle(
    http_test_server_t *                test_server)
{
    globus_result_t                     result;

    result = globus_xio_register_close(
            test_server->handle,
            NULL,
            http_l_test_server_close_callback,
            test_server);

    return result;
}

static
void
http_l_test_server_accept_callback(
    globus_xio_server_t                 server,
    globus_xio_target_t                 target,
    globus_result_t                     result,
    void *                              user_arg)
{
    http_test_server_t *                test_server = user_arg;
    globus_xio_attr_t                   attr;

    globus_mutex_lock(&test_server->mutex);
    if (result != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    result = globus_xio_attr_init(&attr);
    if (result != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    result = globus_xio_attr_cntl(
            attr,
            test_server->http_driver,
            GLOBUS_XIO_HTTP_ATTR_SET_REQUEST_CALLBACK,
            http_l_test_server_request_callback,
            test_server);
    if (result != GLOBUS_SUCCESS)
    {
        goto destroy_attr_exit;
    }

    /* 
    result = globus_xio_attr_cntl(
            attr,
            test_server->tcp_driver,
            GLOBUS_XIO_TCP_SET_LINGER,
            GLOBUS_TRUE,
            1200);
    if (result != GLOBUS_SUCCESS)
    {
        goto destroy_attr_exit;
    }
    */

    result = globus_xio_register_open(
            &test_server->handle,
            attr,
            target,
            http_l_test_server_open_callback,
            test_server);

destroy_attr_exit:
    globus_xio_attr_destroy(attr);
error_exit:
    if (result != GLOBUS_SUCCESS)
    {
        test_server->outstanding_operation--;
        globus_cond_signal(&test_server->cond);
    }
    globus_mutex_unlock(&test_server->mutex);
    return;
}
/* http_l_test_server_accept_callback() */

static
void
http_l_test_server_open_callback(
    globus_xio_handle_t                 handle,
    globus_result_t                     result,
    void *                              user_arg)
{
    /* Processing is done in the request callback */
}
/* http_l_test_server_open_callback() */

static
void
http_l_test_server_request_callback(
    void *                              user_arg,
    globus_result_t                     result,
    const char *                        method,
    const char *                        uri,
    globus_xio_http_version_t           http_version,
    globus_hashtable_t                  headers)
{
    http_test_server_t *                test_server = user_arg;
    http_test_uri_handler_t *           uri_handler;
    
    globus_mutex_lock(&test_server->mutex);

    if (result == GLOBUS_SUCCESS)
    {
        uri_handler = globus_hashtable_lookup(
                &test_server->uri_handlers,
                (void*) uri);

        if (uri_handler != NULL)
        {
            globus_mutex_unlock(&test_server->mutex);

            (*uri_handler->callback)(
                uri_handler->arg,
                result,
                method,
                uri,
                http_version,
                headers);

            return;
        }
    }
    globus_xio_register_close(
            test_server->handle,
            NULL,
            http_l_test_server_close_callback,
            test_server);

    globus_mutex_unlock(&test_server->mutex);
}
/* http_l_test_server_request_callback() */

static
void
http_l_test_server_close_callback(
    globus_xio_handle_t                 handle,
    globus_result_t                     result,
    void *                              user_arg)
{
    http_test_server_t *                test_server = user_arg;
    
    globus_mutex_lock(&test_server->mutex);
    test_server->outstanding_operation--;
    globus_cond_signal(&test_server->cond);
    globus_mutex_unlock(&test_server->mutex);
}
/* http_l_test_server_close_callback() */


globus_result_t
http_test_client_request(
    globus_xio_handle_t *               new_handle,
    globus_xio_driver_t                 tcp_driver,
    globus_xio_driver_t                 http_driver,
    globus_xio_stack_t                  stack,
    const char *                        contact,
    const char *                        uri,
    const char *                        method,
    globus_xio_http_version_t           http_version,
    globus_xio_http_header_t *          header_array,
    size_t                              header_array_length,
    globus_xio_http_response_ready_callback_t
                                        callback,
    void *                              callback_arg)
{
    char *                              url;
    char *                              fmt = "http://%s/%s";
    globus_xio_target_t                 target;
    globus_xio_attr_t                   attr;
    int                                 i;
    globus_result_t                     result = GLOBUS_SUCCESS;
    GlobusXIOName(http_test_client_request);

    url = globus_libc_malloc(strlen(fmt) + strlen(contact) + strlen(uri) + 1);
    if (url == GLOBUS_NULL)
    {
        result = GlobusXIOErrorMemory("url");

        goto error_exit;
    }

    sprintf(url, fmt, contact, uri);

    result = globus_xio_target_init(&target, NULL, url, stack);

    if (result != GLOBUS_SUCCESS)
    {
        goto free_url_exit;
    }

    globus_xio_attr_init(&attr);

    if (method != NULL)
    {
        result = globus_xio_attr_cntl(
                attr,
                http_driver,
                GLOBUS_XIO_HTTP_ATTR_SET_REQUEST_METHOD,
                method);

        if (result != GLOBUS_SUCCESS)
        {
            goto destroy_attr_exit;
        }
    }

    if (http_version != GLOBUS_XIO_HTTP_VERSION_UNSET)
    {
        result = globus_xio_attr_cntl(
                attr,
                http_driver,
                GLOBUS_XIO_HTTP_ATTR_SET_REQUEST_HTTP_VERSION,
                http_version);

        if (result != GLOBUS_SUCCESS)
        {
            goto destroy_attr_exit;
        }
    }

    result = globus_xio_attr_cntl(
            attr,
            http_driver,
            GLOBUS_XIO_HTTP_ATTR_SET_RESPONSE_CALLBACK,
            callback,
            callback_arg);

    if (result != GLOBUS_SUCCESS)
    {
        goto destroy_attr_exit;
    }

    /*
    result = globus_xio_attr_cntl(
            attr,
            tcp_driver,
            GLOBUS_XIO_TCP_SET_LINGER,
            GLOBUS_TRUE,
            1200);
    if (result != GLOBUS_SUCCESS)
    {
        goto destroy_attr_exit;
    }
    */

    for (i = 0; i < header_array_length; i++)
    {
        result = globus_xio_attr_cntl(
                attr,
                http_driver,
                GLOBUS_XIO_HTTP_ATTR_SET_REQUEST_HEADER,
                header_array[i].name,
                header_array[i].value);

        if (result != GLOBUS_SUCCESS)
        {
            goto destroy_attr_exit;
        }
    }

    result = globus_xio_open(
            new_handle,
            attr,
            target);

    if (result == GLOBUS_SUCCESS)
    {
        target = NULL;
    }

destroy_attr_exit:
    globus_xio_attr_destroy(attr);
    if (target)
    {
        globus_xio_target_destroy(target);
    }
free_url_exit:
    globus_libc_free(url);
error_exit:
    return result;
}
/* http_test_client_request() */
