#include "globus_i_gwtftp.h"
#include <sys/types.h>
#include <sys/wait.h>

#define GWTFTP_CHILD_CLIENT_FD  "GWTFTP_CHILD_CLIENT_FD"
#define GWTFTP_CHILD_SERVER_URL "GWTFTP_CHILD_SERVER_URL"

#define URL_SUBJECT_DELIM       "##"

/* 
 *  This file contains the main routine, the arugment parsing and 
 *  list code that listens for connections and accepts.  Once accepted
 *  it hands the new connection to connect.c for the FTP handlshake processing
 */

char *                                  gwtftp_i_data_interface = NULL;
globus_xio_driver_t                     gwtftp_l_gssapi_driver;
globus_xio_driver_t                     gwtftp_l_tcp_driver;
globus_xio_driver_t                     gwtftp_l_gsi_driver;
globus_xio_stack_t                      gwtftp_l_data_tcp_stack;
globus_xio_stack_t                      gwtftp_l_data_gsi_stack;

static globus_xio_stack_t               gwtftp_l_client_stack;
static globus_xio_stack_t               gwtftp_l_server_stack;
static globus_xio_stack_t               gwtftp_l_client_stack;
static globus_xio_driver_t              gwtftp_l_telnet_driver;
static globus_xio_server_t              gwtftp_l_server;

static globus_mutex_t                   gwtftp_l_mutex;
static globus_cond_t                    gwtftp_l_cond;
static globus_bool_t                    gwtftp_l_done = GLOBUS_FALSE;
static globus_bool_t                    gwtftp_l_daemon = GLOBUS_FALSE;
static globus_bool_t                    gwtftp_l_child = GLOBUS_FALSE;
static int                              gwtftp_l_log_level = 255;
static FILE *                           gwtftp_l_log_fptr;
static const char *                     gwtftp_l_pw_file = NULL;
static const char *                     gwtftp_l_log_filename = NULL;
static int                              gwtftp_l_listen_port = 0;
static globus_list_t *                  gwtftp_l_ip_list;

static globus_bool_t                    gwtftp_l_allow_all = GLOBUS_FALSE;
static char  *                          gwtftp_l_exec_program;

static globus_list_t *                  gwtftp_l_connection_list = NULL;

extern globus_options_entry_t           globus_i_gwtftp_opts_table[];

void
gwtftp_i_log(
    int                                 level,
    char *                              fmt,
    ...)
{
    va_list                             ap;

    if(level > gwtftp_l_log_level)
    {
        return;
    }

    va_start(ap, fmt);
    vfprintf(gwtftp_l_log_fptr, fmt, ap);
    va_end(ap);
}

void
gwtftp_i_log_result(
    int                                 level,
    globus_result_t                     result,
    char *                              fmt,
    ...)
{
    va_list                             ap;

    if(level > gwtftp_l_log_level)
    {
        return;
    }

    va_start(ap, fmt);
    vfprintf(gwtftp_l_log_fptr, fmt, ap);
    va_end(ap);
    if(result != GLOBUS_SUCCESS)
    {
        char *                          err_msg;

        err_msg = globus_error_print_friendly(globus_error_peek(result));
        fprintf(gwtftp_l_log_fptr, "Error: %s\n", err_msg);
        free(err_msg);
    }
}

static
void
gwtftp_l_accept_cb(
    globus_xio_server_t                 server,
    globus_xio_handle_t                 handle,
    globus_result_t                     result,
    void *                              user_arg)
{
   gwtftp_i_log(
       FTP2GRID_LOG_INFO, "A client has connected\n");

    globus_mutex_lock(&gwtftp_l_mutex);
    {
        if(result != GLOBUS_SUCCESS)
        {
            goto error_callback;
        }
        result = globus_xio_server_register_accept(
            gwtftp_l_server,
            gwtftp_l_accept_cb,
            NULL);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_register;
        }

        /* valid connection list is tracked here. Even if we are shutting
            down we add all valid handles to the list.  They will be
            closed in the cleanup section */

        globus_list_insert(&gwtftp_l_connection_list, handle);
        if(gwtftp_l_done)
        {
            goto error_over;
        }

        /* verify that we allow this IP  */
        result = gwtftp_i_ip_ok(handle);
        if(result != GLOBUS_SUCCESS)
        {
            globus_object_t *           obj;
            char *                      err_s;

            obj = globus_error_get(result);
            err_s = globus_error_print_friendly(obj);
            globus_object_free(obj);
            gwtftp_i_log(FTP2GRID_LOG_ERROR,
                "Rejecting a client based in IP: %s\n", err_s);
            globus_free(err_s);
        }
        else
        {
            /* only pass it on to the next level after we know we are not
                immediately about to kill it off */
            gwtftp_i_new_connection(handle, NULL);
        }
    }
    globus_mutex_unlock(&gwtftp_l_mutex);

    return;

error_register:
error_callback:
    gwtftp_l_done = GLOBUS_TRUE;
    globus_cond_signal(&gwtftp_l_cond);
error_over:
    globus_mutex_unlock(&gwtftp_l_mutex);
}

static
globus_result_t
gwtftp_l_opts_unknown(
    globus_options_handle_t             opts_handle,
    void *                              unknown_arg,
    int                                 argc,
    char **                             argv)
{
    return globus_error_put(globus_error_construct_error(
        NULL,
        NULL,
        2,
        __FILE__,
        "gwtftp_l_opts_unknown",
        __LINE__,
        "Unknown parameter: %s",
        unknown_arg));
}


static
globus_result_t
gwtftp_l_setup_xio_stack()
{
    globus_result_t                     result;

    globus_xio_stack_init(&gwtftp_l_server_stack, NULL);
    globus_xio_stack_init(&gwtftp_l_client_stack, NULL);
    globus_xio_stack_init(&gwtftp_l_data_tcp_stack, NULL);
    globus_xio_stack_init(&gwtftp_l_data_gsi_stack, NULL);

    result = globus_xio_driver_load("tcp", &gwtftp_l_tcp_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_tcp_load;
    }
    result = globus_xio_driver_load("telnet", &gwtftp_l_telnet_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_telnet_load;
    }
    result = globus_xio_driver_load("gssapi_ftp", &gwtftp_l_gssapi_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_gss_load;
    }
    result = globus_xio_driver_load("gsi", &gwtftp_l_gsi_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_gss_load;
    }

    result = globus_xio_stack_push_driver(
        gwtftp_l_client_stack, gwtftp_l_tcp_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_client_tcp_push;
    }
    result = globus_xio_stack_push_driver(
        gwtftp_l_client_stack, gwtftp_l_telnet_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_client_telnet_push;
    }

    result = globus_xio_stack_push_driver(
        gwtftp_l_server_stack, gwtftp_l_tcp_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_server_tcp_push;
    }
    result = globus_xio_stack_push_driver(
        gwtftp_l_server_stack, gwtftp_l_gssapi_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_server_gss_push;
    }

    result = globus_xio_stack_push_driver(
        gwtftp_l_data_tcp_stack, gwtftp_l_tcp_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_server_gss_push;
    }
    result = globus_xio_stack_push_driver(
        gwtftp_l_data_gsi_stack, gwtftp_l_tcp_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_server_gss_push;
    }
    result = globus_xio_stack_push_driver(
        gwtftp_l_data_gsi_stack, gwtftp_l_gsi_driver);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_server_gss_push;
    }

    return GLOBUS_SUCCESS;

error_server_gss_push:
error_server_tcp_push:
error_client_telnet_push:
error_client_tcp_push:
    globus_xio_driver_unload(gwtftp_l_gssapi_driver);
error_gss_load:
    globus_xio_driver_unload(gwtftp_l_telnet_driver);
error_telnet_load:
    globus_xio_driver_unload(gwtftp_l_tcp_driver);
error_tcp_load:
    globus_xio_stack_destroy(gwtftp_l_server_stack);
    globus_xio_stack_destroy(gwtftp_l_client_stack);

    return result;
}

static
void
gwtftp_l_interrupt_cb(
    void *                              user_arg)
{
    /* attempt a nice shutdown */

    globus_mutex_lock(&gwtftp_l_mutex);
    {
        gwtftp_l_done = GLOBUS_TRUE;
        globus_cond_signal(&gwtftp_l_cond);
    }
    globus_mutex_unlock(&gwtftp_l_mutex);
}

static
globus_result_t
gwtftp_l_child_start()
{
    char *                              tmp_env;
    int                                 fd;
    int                                 sc;
    globus_xio_attr_t                   xio_attr;
    globus_xio_handle_t                 handle;
    globus_xio_handle_t                 server_xio;
    globus_result_t                     result;
    char *                              cs;

    globus_xio_attr_init(&xio_attr);

    globus_mutex_lock(&gwtftp_l_mutex);
    {
        result = globus_xio_attr_cntl(xio_attr, gwtftp_l_telnet_driver,
            GLOBUS_XIO_TELNET_BUFFER, GLOBUS_TRUE);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_telnet;
        }

        cs = globus_libc_getenv(GWTFTP_CHILD_SERVER_URL);
        if(cs == NULL)
        {
            goto error_env;
        }

        /* get the FD for the client CC */
        tmp_env = globus_libc_getenv(GWTFTP_CHILD_CLIENT_FD);
        if(tmp_env == NULL)
        {
            goto error_env;
        }
        sc = sscanf(tmp_env, "%d", &fd);
        if(sc != 1)
        {
            goto error_env;
        }
        result = globus_xio_attr_cntl(
            xio_attr,
            gwtftp_l_tcp_driver,
            GLOBUS_XIO_TCP_SET_HANDLE,
            fd);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_tcp;
        }
        result = globus_xio_attr_cntl(
            xio_attr,
            gwtftp_l_telnet_driver,
            GLOBUS_XIO_TELNET_FORCE_SERVER,
            GLOBUS_TRUE);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_fs;
        }

        result = globus_xio_handle_create(&handle, gwtftp_l_client_stack);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_create;
        }
        result = globus_xio_open(handle, NULL, xio_attr);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_create;
        }

        globus_list_insert(&gwtftp_l_connection_list, handle);

        result = globus_xio_handle_create(
            &server_xio, gwtftp_l_server_stack);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_scheme;
        }


        result = gwtftp_i_server_conn_open(server_xio, cs, handle, NULL);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_open;
        }
        globus_list_insert(&gwtftp_l_connection_list, server_xio);

        while(!globus_list_empty(gwtftp_l_connection_list))
        {
            globus_cond_wait(&gwtftp_l_cond, &gwtftp_l_mutex);
        }
        globus_xio_attr_destroy(xio_attr);
    }
    globus_mutex_unlock(&gwtftp_l_mutex);

    return GLOBUS_SUCCESS;

error_open:
error_scheme:
error_create:
error_fs:
error_tcp:
error_env:
error_telnet:
    globus_mutex_unlock(&gwtftp_l_mutex);
    globus_xio_attr_destroy(xio_attr);

    return result;
}

static
globus_result_t
gwtftp_l_master_start()
{
    globus_xio_attr_t                   xio_attr;
    char *                              cs;
    globus_result_t                     result;

    globus_xio_attr_init(&xio_attr);
    result = globus_xio_attr_cntl(xio_attr, gwtftp_l_telnet_driver,
            GLOBUS_XIO_TELNET_BUFFER, GLOBUS_TRUE);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_server_create;
    }

    if(gwtftp_l_listen_port != 0)
    {
        result = globus_xio_attr_cntl(
            xio_attr,
            gwtftp_l_tcp_driver,
            GLOBUS_XIO_TCP_SET_PORT,
            gwtftp_l_listen_port);
        if(result != GLOBUS_SUCCESS)
        {
            /* log message */
            gwtftp_i_log(FTP2GRID_LOG_MUST,
                "Requested listener port %d not set\n", gwtftp_l_listen_port);
        }
    }

    result = globus_xio_server_create(
        &gwtftp_l_server, xio_attr, gwtftp_l_client_stack);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_server_create;
    }

    result = globus_xio_server_cntl(
        gwtftp_l_server,
        gwtftp_l_tcp_driver,
        GLOBUS_XIO_TCP_GET_LOCAL_CONTACT,
        &cs);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_attr;
    }
    
   gwtftp_i_log(
       FTP2GRID_LOG_WARN,
       "server listening on %s\n", cs);
    free(cs);
    globus_xio_attr_destroy(xio_attr);

    globus_mutex_lock(&gwtftp_l_mutex);
    {
        result = globus_xio_server_register_accept(
            gwtftp_l_server,
            gwtftp_l_accept_cb,
            NULL);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_accept;
        }

        /* register signal handler */
        globus_callback_register_signal_handler(
            GLOBUS_SIGNAL_INTERRUPT,
            GLOBUS_TRUE,
            gwtftp_l_interrupt_cb,
            NULL);
        while(!gwtftp_l_done)
        {
            globus_cond_wait(&gwtftp_l_cond, &gwtftp_l_mutex);
        }
        /* close the server socket */
        globus_xio_server_close(gwtftp_l_server);
    
        /* walk through all created handles and close them all */
        while(!globus_list_empty(gwtftp_l_connection_list))
        {
            globus_xio_handle_t         close_handle;

            close_handle = (globus_xio_handle_t) globus_list_remove(
                &gwtftp_l_connection_list, gwtftp_l_connection_list);

            globus_xio_close(close_handle, NULL);
        }
    }    
    globus_mutex_unlock(&gwtftp_l_mutex);

    return GLOBUS_SUCCESS;

error_accept:
error_attr:
    /* close the server */
    globus_xio_server_close(gwtftp_l_server);
error_server_create:
    globus_xio_attr_destroy(xio_attr);

    return result;
}

void
gwtftp_i_close(
    globus_xio_handle_t                 handle,
    globus_xio_callback_t               close_cb,
    void *                              user_arg)
{
    globus_list_t *                     list;

    globus_mutex_lock(&gwtftp_l_mutex);
    {
        list = globus_list_search(gwtftp_l_connection_list, handle);
        if(list == NULL)
        {
            gwtftp_i_log(
                FTP2GRID_LOG_INFO, "handle not in list, not closing\n");
        }
        else
        {
            globus_list_remove(&gwtftp_l_connection_list, list);

            globus_xio_register_close(handle, NULL, close_cb, user_arg);

            globus_cond_signal(&gwtftp_l_cond);
        }
    }
    globus_mutex_unlock(&gwtftp_l_mutex);
}

static
globus_result_t
gwtftp_i_options(
    int                                 argc,
    char **                             argv)
{
    globus_i_gwtftp_cmd_opts_t          cmd_opts;
    globus_options_handle_t             opt_h;
    globus_result_t                     result;
    GlobusFTP2GridFuncName(gwtftp_i_options);

    gwtftp_l_log_fptr = stderr;

    memset(&cmd_opts, '\0', sizeof(globus_i_gwtftp_cmd_opts_t));
    cmd_opts.log_mask = FTP2GRID_LOG_WARN;
    globus_options_init(
        &opt_h, gwtftp_l_opts_unknown, &cmd_opts);
    globus_options_add_table(opt_h, globus_i_gwtftp_opts_table, &cmd_opts);
    result = globus_options_command_line_process(opt_h, argc, argv);
    if(result != GLOBUS_SUCCESS)
    {
        goto error;
    }
    gwtftp_l_log_level = cmd_opts.log_mask;
    if(cmd_opts.quiet)
    {
        gwtftp_l_log_level = 0;
    }
    if(cmd_opts.log_file != NULL)
    {
        gwtftp_l_log_fptr = fopen(cmd_opts.log_file, "a");
        if(gwtftp_l_log_fptr == NULL)
        {
            gwtftp_l_log_fptr = stderr;

            gwtftp_i_log(
                FTP2GRID_LOG_WARN,
                "logging file %s failed to open\n",
                cmd_opts.log_file);
        }
    }

    gwtftp_l_child = cmd_opts.child;
    gwtftp_l_pw_file = cmd_opts.pw_file;
    if(gwtftp_l_pw_file == NULL && !gwtftp_l_child)
    {
        /* WARN */
        gwtftp_i_log(FTP2GRID_LOG_MUST,
            "No password file specified.  *EVERYONE* can connect and use you credentials.\n  Please don't run it like this\n");
    }

    gwtftp_l_listen_port = cmd_opts.port;
    gwtftp_l_daemon = cmd_opts.daemon;

    gwtftp_l_ip_list = cmd_opts.ip_list;
    if(globus_list_empty(gwtftp_l_ip_list))
    {
        globus_list_insert(&gwtftp_l_ip_list, (void *)"127.0.0.1");
        globus_list_insert(&gwtftp_l_ip_list, (void *)"[::ffff:127.0.0.1]");
        globus_list_insert(&gwtftp_l_ip_list, (void *)"[::1]");
    }

    gwtftp_l_log_filename = cmd_opts.log_file;
    gwtftp_l_exec_program = argv[0];

    gwtftp_l_allow_all = cmd_opts.allow_all;

    globus_options_destroy(opt_h);

    return GLOBUS_SUCCESS;
error:
    globus_options_destroy(opt_h);
    return result;
}

static
void
gwtftp_l_sigchld(
    void *                              user_arg)
{
    int                                 child_pid;
    int                                 child_rc;
    int                                 child_status;

    while((child_pid = waitpid(-1, &child_status, WNOHANG)) > 0)
    {
        if(WIFEXITED(child_status))
        {
            /* normal exit */
            child_rc = WEXITSTATUS(child_status);
        }
        if(child_rc != 0)
        {
            gwtftp_i_log(
                FTP2GRID_LOG_WARN, "Child pid %d finished with rc %d\n",
                child_pid, child_rc);
        }
    }
#   ifdef BUILD_LITE
    {
        globus_callback_register_signal_handler(
            SIGCHLD,
            GLOBUS_FALSE,
            gwtftp_l_sigchld,
            user_arg);
    }
#   else
    {
        globus_reltime_t            delay;

        GlobusTimeReltimeSet(delay, 5, 0);

        globus_callback_register_oneshot(
            NULL,
            &gfork_l_sigchild_fake,
            gwtftp_l_sigchld,
            user_arg);
    }
#   endif
}

int
main(
    int                                 argc,
    char **                             argv)
{
    globus_result_t                     result;
    
    globus_module_activate(GLOBUS_XIO_MODULE);
    globus_module_activate(GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE);

    globus_mutex_init(&gwtftp_l_mutex, NULL);
    globus_cond_init(&gwtftp_l_cond, NULL);

    result = gwtftp_i_options(argc, argv);
    if(result != GLOBUS_SUCCESS)
    {
        goto error;
    }
    result = gwtftp_l_setup_xio_stack();
    if(result != GLOBUS_SUCCESS)
    {
        goto error;
    }

    gwtftp_i_server_init();
    if(gwtftp_l_child)
    {
        /* convert FDs to a handle */
        result = gwtftp_l_child_start();
        if(result != GLOBUS_SUCCESS)
        {
            goto error;
        }
    }
    else
    {
        if(gwtftp_l_daemon)
        {
#           ifdef BUILD_LITE
            {
                result = globus_callback_register_signal_handler(
                    SIGCHLD,
                    GLOBUS_FALSE,
                    gwtftp_l_sigchld,
                    NULL);
            }
#           else
            {
                result = globus_callback_register_oneshot(
                    NULL,
                    &gfork_l_sigchild_fake,
                    gwtftp_l_sigchld,
                    NULL);
            }
#       endif
        }

        result = gwtftp_l_master_start();
        if(result != GLOBUS_SUCCESS)
        {
            goto error;
        }
    }

    globus_module_deactivate(GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE);
    globus_module_deactivate(GLOBUS_XIO_MODULE);
    return 0;

error:
    globus_module_deactivate(GLOBUS_GRIDFTP_SERVER_CONTROL_MODULE);
    globus_module_deactivate(GLOBUS_XIO_MODULE);
    return 1;
}


/* 
 *  this function is called to form the connection from this prgram 
 *  to the end server.  it is implemented here but called else where so
 *  that this file can fully manage the handle list
 */
globus_result_t
gwtftp_i_server_connect(
    const char *                        cs,
    const char *                        subject,
    globus_xio_callback_t               open_cb, 
    void *                              user_arg)
{
    globus_xio_handle_t                 xio_handle;
    globus_result_t                     result;

    globus_mutex_lock(&gwtftp_l_mutex);
    {
        result = globus_xio_handle_create(
            &xio_handle, gwtftp_l_server_stack);
        if(result != GLOBUS_SUCCESS)
        {
            goto error;
        }

        result = globus_xio_register_open(
            xio_handle,
            cs,
            NULL,
            open_cb,
            user_arg);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_open;
        }
        globus_list_insert(&gwtftp_l_connection_list, xio_handle);
    }
    globus_mutex_unlock(&gwtftp_l_mutex);

    return GLOBUS_SUCCESS;
error_open:
error:
    return result;
}


globus_result_t
gwtftp_i_ip_ok(
    globus_xio_handle_t                 handle)
{
    globus_result_t                     result;
    char *                              remote_contact;
    int                                 remote_host_ints[16];
    int                                 remote_host_count;
    char *                              ok_mask;
    int                                 ok_mask_ints[16];
    int                                 ok_mask_count;
    int                                 i;
    int                                 count;
    globus_bool_t                       ok = GLOBUS_FALSE;
    globus_list_t *                     list;
    GlobusFTP2GridFuncName(gwtftp_i_ip_ok);

    if(gwtftp_l_allow_all)
    {
        return GLOBUS_SUCCESS;
    }

    result = globus_xio_handle_cntl(
        handle,
        gwtftp_l_tcp_driver,
        GLOBUS_XIO_TCP_GET_REMOTE_NUMERIC_CONTACT,
        &remote_contact);
    if(result != GLOBUS_SUCCESS)
    {
        goto error;
    }

    result = globus_libc_contact_string_to_ints(
        remote_contact,
        remote_host_ints,
        &remote_host_count,
        NULL);
    free(remote_contact);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_convert;
    }

    for(list = gwtftp_l_ip_list;
        !globus_list_empty(list) && !ok;
        list = globus_list_rest(list))
    {
        ok_mask = (char *) globus_list_first(list);

        result = globus_libc_contact_string_to_ints(
            ok_mask,
            ok_mask_ints,
            &ok_mask_count,
            NULL);
        if(result != GLOBUS_SUCCESS)
        {
            continue;
        }
        if(remote_host_count != ok_mask_count)
        {
            continue;
        }

        count = -1;
        /* find out how many matter */
        for(i = ok_mask_count - 1; count == -1 && i >= 0; i--)
        {
            if(ok_mask_ints[i] != 0)
            {
                count = i;
            }
        } 

        /* compare all that matter */
        ok = GLOBUS_TRUE;
        for(i = 0; i <= count; i++)
        {
            if(remote_host_ints[i] != ok_mask_ints[i])
            {
                ok = GLOBUS_FALSE;
            }
        }
    }
    if(!ok)
    {
        goto error_not_ok;
    }

    return GLOBUS_SUCCESS;
error_not_ok:
    result = GlobusFTP2GridError("Connection from unathorized IP",
        GLOBUS_FTP2GRID_ERROR_IP);
error_convert:
error:
    return result;
}

static
void
gwtftp_i_exec__unix(
    uid_t                               uid,
    globus_xio_handle_t                 client_xio,
    globus_url_t *                      url)
{
    int                                 i;
    globus_xio_system_socket_t          socket_handle;
    pid_t                               pid;
    int                                 rc;
    char                                env_str[64];
    char *                              exec_program[32];
    char *                              cs;
    globus_result_t                     result;

    gwtftp_i_log(FTP2GRID_LOG_INFO, "Forking a child.\n");
    pid = fork();
    if(pid == 0)
    {
        cs = globus_common_create_string("%s:%d", url->host, url->port);
        globus_libc_setenv(GWTFTP_CHILD_SERVER_URL, cs, 1);

        gwtftp_i_log(FTP2GRID_LOG_INFO, "Child processing seting uid to %d.\n",
            uid);
        /* extract the FD */
        result = globus_xio_handle_cntl(
            client_xio,
            gwtftp_l_tcp_driver,
            GLOBUS_XIO_TCP_GET_HANDLE,
            &socket_handle);
        if(result != GLOBUS_SUCCESS)
        {
            gwtftp_i_log(FTP2GRID_LOG_WARN, "Failed to extract socket.\n");
            exit(1);
        }
        /* all xio sockets will close on exec */
        sprintf(env_str, "%d", socket_handle);
        globus_libc_setenv(GWTFTP_CHILD_CLIENT_FD, env_str, 1);

        exec_program[0] = strdup(gwtftp_l_exec_program);
        exec_program[1] = strdup("-CH");
        i = 2;
        if(gwtftp_l_log_filename != NULL)
        {
            exec_program[i] = strdup("-l");
            i++;
            exec_program[i] = globus_common_create_string("%s.%d", 
                gwtftp_l_log_filename, getpid());
            i++;
        }
        exec_program[i] = strdup("-lm");
        i++;
        exec_program[i] = globus_common_create_string("%d", gwtftp_l_log_level);
        i++;
        exec_program[i] = NULL;

        gwtftp_i_log(FTP2GRID_LOG_INFO, "Execing child process: ");
        for(i = 0; exec_program[i] != NULL; i++)
        {
            gwtftp_i_log(FTP2GRID_LOG_INFO, "%s ", exec_program[i]);
        }
        gwtftp_i_log(FTP2GRID_LOG_INFO, "\n");
        rc = setuid(uid);
        if(rc != 0)
        {
            gwtftp_i_log(FTP2GRID_LOG_WARN, "Child setuid failed.\n");
            exit(rc);
        }
        fflush(gwtftp_l_log_fptr);

        rc = execvp(exec_program[0], exec_program);
        for(i = 0; exec_program[i] != NULL; i++)
        {
            free(exec_program[i]);
        }
        gwtftp_i_log(FTP2GRID_LOG_ERROR, "Exec failed\n");
        exit(rc);
    }
    else if(pid < 0)
    {
        gwtftp_i_log(FTP2GRID_LOG_ERROR, "Failed to fork\n");
    }
}

int
gwtftp_i_pass_ok__unix(
    const char *                        username,
    const char *                        pw)
{
    char *                              pw_hash;
    struct passwd *                     pw_ent;
    FILE *                              pw_fptr;
    uid_t                               uid = -1;
    globus_bool_t                       done = GLOBUS_FALSE;
    struct passwd                       l_pw;
    struct passwd *                     l_pw2;
    char                                pw_buf[256];
    int                                 rc;

    /* WHAT TO DO ABOUT NULL USERNAME */
    if(username == NULL)
    {
        rc = globus_libc_getpwuid_r(
            getuid(),
            &l_pw,
            pw_buf,
            256,
            &l_pw2);
        if(rc != 0)
        {
            return -1;
        }
        username = l_pw.pw_name;
    }
    if(gwtftp_l_pw_file == NULL)
    {
        uid = getuid();
        return (int)uid;
    }
    pw_fptr = fopen(gwtftp_l_pw_file, "r");
    if(pw_fptr == NULL)
    {
        return -1;
    }

    while(!done)
    {
        pw_ent = fgetpwent(pw_fptr);
        if(pw_ent == NULL)
        {
            done = GLOBUS_TRUE;
        }
        else
        {
            if(strcmp(pw_ent->pw_name, username) == 0)
            {
                pw_hash = DES_crypt(pw, pw_ent->pw_passwd);
                if(strcmp(pw_hash, pw_ent->pw_passwd) == 0)
                {
                    uid = pw_ent->pw_uid;
                }
                done = GLOBUS_TRUE;
            }
        }
    }
    fclose(pw_fptr);

    return (int)uid;
}

void
gwtftp_i_authorized_user(
    globus_xio_handle_t                 client_xio,
    const char *                        full_username,
    const char *                        pass)
{
    int                                 rc;
    uid_t                               uid;
    globus_list_t *                     list;
    globus_xio_handle_t                 server_xio;
    globus_result_t                     result;
    globus_url_t                        g_url;
    char *                              url_retry = NULL;
    char *                              tmp_ptr;
    char *                              subject = NULL;

    gwtftp_i_log(FTP2GRID_LOG_INFO,
        "Authorizing: %s\n", full_username);
    globus_mutex_lock(&gwtftp_l_mutex);
    {
        list = globus_list_search(gwtftp_l_connection_list, client_xio);
        globus_assert(list != NULL);

        /* pull out local user name and  subject if they are there */

        tmp_ptr = strstr(full_username, URL_SUBJECT_DELIM);
        if(tmp_ptr != NULL)
        {
            subject = strdup(full_username);
            full_username = tmp_ptr + strlen(URL_SUBJECT_DELIM);
            tmp_ptr = strstr(subject, URL_SUBJECT_DELIM);
            globus_assert(tmp_ptr != NULL);
            *tmp_ptr = '\0';
        }

        rc = globus_url_parse(full_username, &g_url);
        if(rc == GLOBUS_URL_ERROR_BAD_SCHEME)
        {
            /* try to recover */
            url_retry = globus_common_create_string(
                "gsiftp://%s", full_username);
            tmp_ptr = strchr(url_retry, '!');
            if(tmp_ptr != NULL)
            {
                *tmp_ptr = ':';
            }
            tmp_ptr = strchr(url_retry, '^');
            if(tmp_ptr != NULL)
            {
                *tmp_ptr = '@';
            }
            rc = globus_url_parse(url_retry, &g_url);
        }
        if(rc != 0)
        {
            goto error;
        }
        if(g_url.scheme_type != GLOBUS_URL_SCHEME_GSIFTP &&
            g_url.scheme_type != GLOBUS_URL_SCHEME_FTP)
        {
            goto error_scheme;
        }

        if(g_url.port == 0)
        {
            g_url.port = 2811;
        }

        rc = gwtftp_i_pass_ok__unix(g_url.user, pass);
        if(rc < 0)
        {
            /* if it is bad. 
                XXX: we can send a nice little message, but just close for now
             */
            goto error;
        }
        else
        {
            uid = (uid_t)rc;
            if(gwtftp_l_daemon)
            {
                gwtftp_i_exec__unix(uid, client_xio, &g_url);
                globus_list_remove(&gwtftp_l_connection_list, list);
                globus_xio_register_close(client_xio, NULL, NULL, NULL);
            }
            else
            {
                char *                  cs;

                result = globus_xio_handle_create(
                    &server_xio, gwtftp_l_server_stack);
                if(result != GLOBUS_SUCCESS)
                {
                    goto error_scheme;
                }

                cs = globus_common_create_string(
                    "%s:%d", g_url.host, g_url.port);

                result = gwtftp_i_server_conn_open(
                    server_xio, cs, client_xio, subject);
                globus_free(cs);
                if(result != GLOBUS_SUCCESS)
                {
                    goto error_open;
                }
                globus_list_insert(&gwtftp_l_connection_list, server_xio);
            }
        }
    }
    globus_mutex_unlock(&gwtftp_l_mutex);
    globus_url_destroy(&g_url);
    if(url_retry != NULL)
    {
        globus_free(url_retry);
    }

    return;

error_open:
    globus_xio_register_close(server_xio, NULL, NULL, NULL);
error_scheme:
    globus_url_destroy(&g_url);
error:

    if(url_retry != NULL)
    {
        globus_free(url_retry);
    }
    globus_list_remove(&gwtftp_l_connection_list, list);
    globus_xio_register_close(client_xio, NULL, NULL, NULL);

    globus_mutex_unlock(&gwtftp_l_mutex);
    return;
}
