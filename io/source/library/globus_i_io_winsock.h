/**
 * @file globus_i_io_winsock.h Globus I/O toolset
 *
 *   Header file for globus_io_winsock.c
 *
 * $Source: 
 * $Date: 
 * $Revision: 
 * $State: 
 * $Author: Michael Lebman
 */

#ifndef GLOBUS_I_IO_WINSOCK_H
#define GLOBUS_I_IO_WINSOCK_H

//#include <winsock2.h>

int globus_i_io_winsock_socket_is_readable( SOCKET socket, int timeout );
int globus_i_io_winsock_socket_is_writable( SOCKET socket, int timeout );
int globus_i_io_winsock_will_io_succeed( SOCKET socket, 
	int readOperation, int timeout );
int globus_i_io_winsock_read( globus_io_handle_t * handle, char * buffer, 
 int numberOfBytes, int asynchronous );
int globus_i_io_winsock_write( globus_io_handle_t * handle, char * buffer, 
 int numberOfBytes, int asynchronous );
void globus_i_io_winsock_close( globus_io_handle_t * handle );
int globus_i_io_winsock_get_last_error( void );

#endif /* GLOBUS_I_IO_WINSOCK_H */
