#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_io_socket.c Globus I/O toolset (socket layer)
 *
 * $Source$
 * $Date$
 * $Revision$
 * $State$
 * $Author$
 */
#endif

/**
 * RCS Identification for this file
 */
static char *rcsid = "$Header$";


/*
 * Include header files
 */
#include "globus_l_io.h"
#include <string.h>

/*
 * Module Specific Variables
 */

/*
 * Function:    globus_i_io_setup_socket()
 *
 * Description:
 *
 * Parameters:
 *
 * Returns:
 */
globus_result_t
globus_i_io_setup_socket(
    globus_io_handle_t *		handle)
{
    globus_object_t *			err = GLOBUS_NULL;
    int					one = 1;
    int					save_errno;
    
    if(handle->socket_attr.reuseaddr != GLOBUS_FALSE)
    {
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_REUSEADDR,
		      (char *) &one,
		      sizeof(one)) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}
    }

    if(handle->socket_attr.keepalive != GLOBUS_FALSE)
    {
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_KEEPALIVE,
		      (char *) &one,
		      sizeof(one)) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}
    }
    if(handle->socket_attr.linger != GLOBUS_FALSE)
    {
	struct linger			linger;
	linger.l_onoff = 1;
	linger.l_linger = handle->socket_attr.linger_time;
	
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_KEEPALIVE,
		      (char *) &linger,
		      sizeof(struct linger)) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}
    }
    if(handle->socket_attr.oobinline)
    {
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_OOBINLINE,
		      (char *) &one,
		      sizeof(one)) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}	
    }
    if(handle->socket_attr.sndbuf)
    {
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_SNDBUF,
		      (char *) &handle->socket_attr.sndbuf,
		      sizeof(one)) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}
    }
    else
    {
	/* Turn request for default into a concrete default value */
	GLOBUS_SOCK_SIZE_T		len;

	len = sizeof(int);

	if(getsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_SNDBUF,
		      (char *) &handle->socket_attr.sndbuf,
		      &len) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}
    }
    if(handle->socket_attr.rcvbuf)
    {
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_RCVBUF,
		      (char *) &handle->socket_attr.rcvbuf,
		      sizeof(one)) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}
    }
    else
    {
        GLOBUS_SOCK_SIZE_T	len;

	len = sizeof(int);

	/* Turn request for default into a concrete default value */
	if(getsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_RCVBUF,
		      (char *) &handle->socket_attr.rcvbuf,
		      &len) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}
    }
        
    return GLOBUS_SUCCESS;
    
  sockopt_failure:
    err = globus_io_error_construct_system_failure(
	GLOBUS_IO_MODULE,
	GLOBUS_NULL,
	handle,
	save_errno);
    
    return globus_error_put(err);
}
/* globus_i_io_setup_socket() */

/*
 * Function:	globus_i_io_socket_get_attr()
 *
 * Description:	
 *		
 * Parameters:	
 *
 * Returns:	
 */
globus_result_t
globus_i_io_socket_get_attr(
    globus_io_handle_t *		handle,
    globus_io_attr_t *			attr)
{
    globus_i_io_socketattr_instance_t *	instance;
    
    instance = (globus_i_io_socketattr_instance_t *)
	globus_object_get_local_instance_data(
	    globus_object_upcast(attr->attr,
				 GLOBUS_IO_OBJECT_TYPE_SOCKETATTR));
    

    globus_i_io_socket_copy_attr(
	   instance,
	   &handle->socket_attr);
    
    return GLOBUS_SUCCESS;
}
/* globus_io_socket_get_attr() */


/*
 * Function:    globus_i_io_socket_set_attr()
 *
 * Description:
 *
 * Parameters:
 *
 * Returns:
 */
globus_result_t
globus_i_io_socket_set_attr(
    globus_io_handle_t *		handle,
    globus_io_attr_t *			attr)
{
    globus_object_t *			err = GLOBUS_NULL;
    globus_object_t *			socket_attr;
    globus_i_io_socketattr_instance_t *	instance;
    int					save_errno;
    static char *			myname="globus_i_io_socket_set_attr";
    int					rcvbuf=0;

    socket_attr = globus_object_upcast(attr->attr,
				       GLOBUS_IO_OBJECT_TYPE_SOCKETATTR);
    globus_assert(socket_attr != GLOBUS_NULL);
    
    instance = (globus_i_io_socketattr_instance_t *)
	globus_object_get_local_instance_data(socket_attr);

    globus_assert(instance);
    
    /* set local socket options */
    if(instance->reuseaddr != handle->socket_attr.reuseaddr)
    {
	err = globus_io_error_construct_immutable_attribute(
	    GLOBUS_IO_MODULE,
	    GLOBUS_NULL,
	    GLOBUS_NULL,
	    0,
	    myname,
	    "socket reuseaddr");

	goto error_exit;
    }
    if(instance->keepalive != handle->socket_attr.keepalive)
    {
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_KEEPALIVE,
		      (char *) &instance->keepalive,
		      sizeof(instance->keepalive)) < 0)
	{
	    save_errno = errno;
	    
	    goto sockopt_failure;
	}
    }
    if(instance->linger != handle->socket_attr.linger ||
       (instance->linger && 
	   instance->linger_time != handle->socket_attr.linger_time))
    {
	struct linger			linger;

	linger.l_onoff = instance->linger;
	linger.l_linger = instance->linger_time;
	
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_LINGER,
		      (char *) &linger,
		      sizeof(struct linger)) < 0)
	{
	    save_errno = errno;
	    
	    goto undo_keepalive;
	}
    }
    if(instance->oobinline != handle->socket_attr.oobinline)
    {
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_OOBINLINE,
		      (char *) &instance->oobinline,
		      sizeof(instance->oobinline)) < 0)
	{
	    save_errno = errno;
	    
	    goto undo_linger;
	}
    }
    
    if(instance->sndbuf != handle->socket_attr.sndbuf &&
       instance->sndbuf != 0 &&
       handle->socket_attr.sndbuf != 0)
    {
	if(setsockopt(handle->fd,
		      SOL_SOCKET,
		      SO_SNDBUF,
		      (char *) &instance->sndbuf,
		      sizeof(instance->sndbuf)) < 0)
	{
	    save_errno = errno;
	    
	    goto undo_oobinline;
	}
    }
    
    if(instance->rcvbuf != handle->socket_attr.rcvbuf)
    {
	/* The rcvbuf can only be modified before a connection is
	 * established on some systems.
	 */
	switch(handle->type)
	{
	  case GLOBUS_IO_HANDLE_TYPE_TCP_LISTENER:
	  case GLOBUS_IO_HANDLE_TYPE_UDSS_LISTENER:
	    {
		if(instance->rcvbuf == 0)
		{
		    int			fd;
		    GLOBUS_SOCK_SIZE_T  len;

		    len = sizeof(int);

		    fd = socket(PF_INET, SOCK_STREAM, 0);

		    if(fd >= 0)
		    {
			len = sizeof(int);

			if(getsockopt(fd,
				      SOL_SOCKET,
				      SO_RCVBUF,
				      (char *) &rcvbuf,
				      &len) < 0)
			{
			    save_errno = errno;
			    close(fd);
			    goto undo_sndbuf;
			}
			close(fd);
		    }
		    else
		    {
			break;
		    }
		}
		else
		{
		    rcvbuf = instance->rcvbuf;
		}
		if(rcvbuf != 0)
		{
		    if(setsockopt(handle->fd,
				  SOL_SOCKET,
				  SO_RCVBUF,
				  (char *) &rcvbuf,
				  sizeof(rcvbuf)) < 0)
		    { 
			save_errno = errno;
			goto undo_sndbuf;
		    }
		}
	    }
	    break;
          default:
	    err = globus_io_error_construct_immutable_attribute(
		GLOBUS_IO_MODULE,
		GLOBUS_NULL,
		GLOBUS_NULL,
		0,
		myname,
		"socket rcvbuf");

	    goto undo_sndbuf;
	}
    }
    /* commit any changes to the handle structure */
    if(instance->reuseaddr != handle->socket_attr.reuseaddr)
    {
	handle->socket_attr.reuseaddr = instance->reuseaddr;
    }
    
    if(instance->keepalive != handle->socket_attr.keepalive)
    {
	handle->socket_attr.keepalive = instance->keepalive;
    }    

    if(instance->linger != handle->socket_attr.linger ||
       instance->linger_time != handle->socket_attr.linger_time)
    {
	handle->socket_attr.linger = instance->linger;
	handle->socket_attr.linger_time = instance->linger_time;
    }

    if(instance->oobinline != handle->socket_attr.oobinline)
    {
	handle->socket_attr.oobinline = instance->oobinline;
    }

    if(instance->sndbuf != handle->socket_attr.sndbuf)
    {
	handle->socket_attr.sndbuf = instance->sndbuf;
    }
    if(instance->rcvbuf != handle->socket_attr.rcvbuf)
    {
	handle->socket_attr.rcvbuf = rcvbuf;
    }
    
    return GLOBUS_SUCCESS;
    
    /* undo any changes, then return an error */
  undo_sndbuf:
    if(instance->sndbuf != handle->socket_attr.sndbuf)
    {
	setsockopt(handle->fd,
		   SOL_SOCKET,
		   SO_SNDBUF,
		   (char *) &handle->socket_attr.sndbuf,
		   sizeof(handle->socket_attr.sndbuf));
	/* don't care about return value here, because an
	 * error already occurred
	 */
    }

  undo_oobinline:
    if(instance->oobinline != handle->socket_attr.oobinline)
    {
	setsockopt(handle->fd,
		   SOL_SOCKET,
		   SO_SNDBUF,
		   (char *) &handle->socket_attr.oobinline,
		   sizeof(handle->socket_attr.oobinline));
	/* don't care about return value here, because an
	 * error already occurred
	 */
    }
  undo_linger:
    if(instance->linger != handle->socket_attr.linger ||
       (instance->linger && 
	   instance->linger_time != handle->socket_attr.linger_time))
    {
	struct linger			linger;

	linger.l_onoff = handle->socket_attr.linger;
	linger.l_linger = handle->socket_attr.linger_time;
	
	setsockopt(handle->fd,
		   SOL_SOCKET,
		   SO_LINGER,
		   (char *) &linger,
		   sizeof(struct linger));
	/* don't care about return value here, because an
	 * error already occurred
	 */	
    }

  undo_keepalive:
    if(instance->keepalive != handle->socket_attr.keepalive)
    {
	setsockopt(handle->fd,
		   SOL_SOCKET,
		   SO_SNDBUF,
		   (char *) &handle->socket_attr.keepalive,
		   sizeof(handle->socket_attr.keepalive));
	/* don't care about return value here, because an
	 * error already occurred
	 */
    }
    
  sockopt_failure:
    if(err == GLOBUS_NULL)
    {
	err = globus_io_error_construct_system_failure(
	    GLOBUS_IO_MODULE,
	    GLOBUS_NULL,
	    handle,
	    save_errno);
    }
    
  error_exit:
    return globus_error_put(err);
}
/* globus_i_io_socket_set_attr() */

