#include "config.h"
#if defined(USE_GLOBUS_DATA_CODE)
#include  <globus_common.h>
#include "proto.h"
#include "../support/ftp.h"
#include <syslog.h>

extern globus_ftp_control_layout_t		g_layout;
extern globus_ftp_control_parallelism_t		g_parallelism;
extern globus_bool_t				g_send_restart_info;
extern int mode;
extern globus_size_t                            g_striped_file_size;

extern SIGNAL_TYPE         
lostconn(int sig);

extern globus_fifo_t				g_restarts;
extern globus_ftp_control_dcau_t                g_dcau;

static globus_bool_t                            g_send_perf_update;
static globus_bool_t                            g_send_range;

globus_bool_t g_eof_receive = GLOBUS_FALSE;

#if defined(STRIPED_SERVER_BACKEND)
#include "bmap_file.h"
#endif 

/*
#define G_DEBUG 1
*/

#if G_DEBUG
FILE * g_out;

#define DEBUG_OPEN() \
g_out = fopen("/disks/space1/wuftpd_out", "w")

#define DEBUG_CLOSE() \
fclose(g_out)

#else

#define DEBUG_OPEN()
#define DEBUG_CLOSE()

#endif

void
debug_printf(char * fmt, ...)
{
#if G_DEBUG
    va_list          ap;

    va_start(ap, fmt);
    vfprintf(g_out, fmt, ap);

    va_end(ap);
#endif
}


/*
 *  The enter and exit macros need to be around any code that will
 *  cause globus to poll, ex: globus_cond_wait(), globus_poll().
 *  Otherwise the SIGPIPE handler in wuftpd can cause errors in
 *  the globus_io callback mecanism.
 */
#define G_ENTER()                  \
{                                  \
    signal(SIGPIPE, SIG_IGN);      \
}

#define G_EXIT()                   \
{                                  \
    signal(SIGPIPE, lostconn);     \
}

typedef struct
{
    off_t			offset;
    off_t			length;
}
globus_l_wu_range_t;

typedef struct globus_i_wu_monitor_s
{
    globus_mutex_t             mutex;
    globus_cond_t              cond;
    globus_bool_t              done;

    globus_object_t *          error;
    globus_bool_t              timed_out;
    globus_bool_t              abort;
    int                        count;
    int                        fd;
    int                        code;

    off_t	               offset; 
    int                        callback_count;

    /* Range response messages */
    time_t		       last_range_update;
    globus_fifo_t	       ranges;

    /* Performance update messages */
    struct timeval	       last_perf_update;
    globus_off_t	       all_transferred;
    globus_off_t	       accum_bytes;
    globus_callback_handle_t   callback_handle;
    globus_ftp_control_handle_t *
			       handle;
#if defined (STRIPED_SERVER_BACKEND)
    char *                     name;
    bmap_file_t                *bmap_handle;    
#endif /* STRIPED_SERVER_BACKEND */
    globus_io_handle_t         io_handle;

} globus_i_wu_monitor_t;

/*
 *  global varials from ftpd.c
 */
extern int logged_in;
extern int transflag;
extern int retrieve_is_data;
extern int type;
extern unsigned int timeout_data;
extern unsigned int timeout_connect;
extern unsigned int timeout_accept;
extern int data_count_total;
extern int data_count_in;
extern int data_count_out;
extern int byte_count_total;
extern int byte_count_in;
extern int byte_count_out;
extern off_t file_size;
extern off_t byte_count;
extern int file_count_total;
extern int file_count_in;
extern int file_count_out;
extern int xfer_count_total;
extern int xfer_count_in;
extern int xfer_count_out;
extern struct sockaddr_in ctrl_addr;
extern struct sockaddr_in his_addr;
/**********************************************************n
 * local function prototypes
 ************************************************************/
int 
g_seek(
    FILE *                               fin,
    int                                  ndx);

void
g_force_close(
    int                                         cb_count);

void
connect_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               reuse,
    globus_object_t *                           error);

void
data_read_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof);

void
data_write_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof);

void 
data_close_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    globus_object_t *                           error);

static char *
globus_l_wu_create_range_string(globus_fifo_t * ranges);

static globus_bool_t
globus_l_wu_perf_update_callback(
    globus_abstime_t *			time_stop,
    void *				user_args);

static globus_bool_t
globus_l_wu_perf_update(
    globus_i_wu_monitor_t *              mon);

void
send_range(
    globus_i_wu_monitor_t *                      monitor);

/*************************************************************
 *   global vairables 
 ************************************************************/
globus_ftp_control_handle_t                     g_data_handle;

static globus_i_wu_monitor_t                     g_monitor;

/*
 *  define macros for file handling
 */
#if defined(STRIPED_SERVER_BACKEND)

#define G_File_Open(handle, fname, flags, fd)  \
    bmap_file_open(handle, fname, flags, fd)

#define G_File_Read(handle, fd, buf, length, offset, offset_out) \
    bmap_file_read(handle, buf, length, offset, offset_out) 

#define G_File_Write(handle, fd, buffer, length, offset) \
    bmap_file_write(handle, buffer, length, offset)

#define G_File_Close(handle, fd) \
    bmap_file_close(g_monitor.bmap_handle)

#else

#define G_File_Open(handle, fname, flags, fd)  \
    globus_open(handle,fd)

#define G_File_Read(handle, fd, buffer, length, offset, offset_out) \
    globus_read(handle, buffer, length, offset, offset_out)

#define G_File_Write(handle, fd, buffer, length, offset) \
    globus_write(handle, buffer, length, offset)

#define G_File_Close(handle, fd) \
    globus_close(handle)


int globus_open(
    globus_io_handle_t *   handle,
    int                    fd)
{
    globus_result_t        res;

    res = globus_io_file_posix_convert(dup(fd),
				       GLOBUS_NULL,
				       handle);

    return (res == GLOBUS_SUCCESS) ? 0 : -1;
}


int globus_close(
    globus_io_handle_t *   handle)
{
    globus_result_t        res;

    res = globus_io_close(handle);

    return (res == GLOBUS_SUCCESS) ? 0 : -1;
}


int
globus_read(    
    globus_io_handle_t *                handle,
    globus_byte_t *                     buffer,
    int                                 length,
    off_t                               offset,
    off_t *                             offs_out)
{
    globus_size_t                       bytes_read;
    globus_result_t                     res;

    *offs_out = offset;

    res = globus_io_read(handle, 
			 buffer, 
			 length, 
			 length, 
			 &bytes_read);

    return bytes_read;
}

int 
globus_write(
    globus_io_handle_t *                handle,
    globus_byte_t *                     buffer,
    int                                 length,
    off_t                               offset)
{
    globus_result_t                     res;
    globus_size_t                       bytes_written;

    res = globus_io_file_seek(handle,
			      offset,
			      GLOBUS_IO_SEEK_SET);
    if(res != GLOBUS_SUCCESS)
    {
        return -1;
    }
    
    res = globus_io_write(handle, 
			  buffer, 
			  length,
			  &bytes_written);

    return (res == GLOBUS_SUCCESS) ? bytes_written : -1;
}


int
std_read(    
    int                                 fd,
    globus_byte_t *                     buffer,
    int                                 length,
    off_t                               offset,
    off_t *                             offs_out)
{
    *offs_out = offset;

    return read(fd, buffer, length);
}

int 
std_write(
    int                                 fd,
    globus_byte_t *                     buffer,
    int                                 length,
    off_t                               offset)
{
    int                                 ret;

    ret = lseek(fd, offset, SEEK_SET);
    if(ret < 0)
    {
        return ret;
    }
    ret = write(fd, buffer, length);

    return ret;
}

#endif /* STRIPED_SERVER_BACKEND */

void
wu_monitor_reset(
    globus_i_wu_monitor_t *                      mon)
{
    mon->done = GLOBUS_FALSE;
    mon->timed_out = GLOBUS_FALSE;
    mon->abort = GLOBUS_FALSE;
    mon->count = 0;
    mon->offset = -1;
    mon->fd = -1;
    gettimeofday(&mon->last_perf_update, GLOBUS_NULL);
    mon->last_range_update = mon->last_perf_update.tv_sec;
    mon->accum_bytes = 0;
    mon->callback_handle = 0;
    globus_fifo_init(&mon->ranges);
}

void
wu_monitor_init(
    globus_i_wu_monitor_t *                      mon)
{
    globus_mutex_init(&mon->mutex, GLOBUS_NULL);
    globus_cond_init(&mon->cond, GLOBUS_NULL);

    wu_monitor_reset(mon);
}

void
wu_monitor_destroy(
    globus_i_wu_monitor_t *                      mon)
{
    globus_mutex_destroy(&mon->mutex);
    globus_cond_destroy(&mon->cond);

    globus_fifo_destroy(&mon->ranges);
}

static int
g_timeout_wakeup(
    globus_abstime_t *                           time_stop,
    void *                                       user_args)
{
    globus_mutex_lock(&g_monitor.mutex);
    {   
        globus_cond_signal(&g_monitor.cond);
    }
    globus_mutex_unlock(&g_monitor.mutex);

    return GLOBUS_TRUE;
}

void
g_start()
{
    char *                            a;
    globus_ftp_control_host_port_t    host_port;
    int			              rc;
    globus_reltime_t                  delay_time;
    globus_reltime_t                  period_time;
    globus_result_t		      res;

DEBUG_OPEN();
G_ENTER();
    rc = globus_module_activate(GLOBUS_FTP_CONTROL_MODULE);
    assert(rc == GLOBUS_SUCCESS);

    wu_monitor_init(&g_monitor);
    g_monitor.handle = &g_data_handle;

    g_dcau.mode = GLOBUS_FTP_CONTROL_DCAU_NONE;


    a = (char *)&his_addr;
    host_port.host[0] = (int)a[0];
    host_port.host[1] = (int)a[1];
    host_port.host[2] = (int)a[2];
    host_port.host[3] = (int)a[3];
    host_port.port = 21;

    globus_ftp_control_handle_init(&g_data_handle);
    res = globus_ftp_control_local_port(
              &g_data_handle,
              &host_port);
    assert(res == GLOBUS_SUCCESS);

#   if defined(STRIPED_SERVER_BACKEND)
    {
        res = globus_ftp_control_local_send_eof(
                  &g_data_handle,
                  GLOBUS_FALSE);
        assert(res == GLOBUS_SUCCESS);

        g_striped_file_size = -1;
    }
#   endif

    g_parallelism.mode = GLOBUS_FTP_CONTROL_PARALLELISM_NONE;
    g_parallelism.base.size = 1;
    g_layout.mode = GLOBUS_FTP_CONTROL_STRIPING_NONE;

    globus_ftp_control_local_parallelism(
              &g_data_handle,
              &g_parallelism);

    GlobusTimeReltimeSet(delay_time, timeout_connect / 2, 0);
    GlobusTimeReltimeSet(period_time, timeout_connect / 2, 0);

    globus_fifo_init(&g_restarts);

    debug_printf("registering wakeup at %d secs\n", timeout_connect / 2);
    rc = globus_callback_register_periodic(
             GLOBUS_NULL,
             &delay_time,
             &period_time,
             g_timeout_wakeup,
             GLOBUS_NULL,
             GLOBUS_NULL,
             GLOBUS_NULL);
    assert(res == GLOBUS_SUCCESS);

G_EXIT();
}

void
g_end()
{
    globus_i_wu_monitor_t                            monitor;
    globus_result_t                                 res;

    G_ENTER();

    wu_monitor_init(&monitor);
    /*
     *  force close the data connection
     */
    monitor.done = GLOBUS_FALSE;
    res = globus_ftp_control_data_force_close(
              &g_data_handle,
              data_close_callback,
              (void*)&monitor);
    if(res == GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&monitor.mutex);
        {   
            while(!monitor.done)
            {
                globus_cond_wait(&monitor.cond, &monitor.mutex);
            }
        }
        globus_mutex_unlock(&monitor.mutex);
    }

    wu_monitor_destroy(&monitor);

    globus_ftp_control_handle_destroy(&g_data_handle);
    globus_module_deactivate(GLOBUS_FTP_CONTROL_MODULE);

DEBUG_CLOSE();
    G_EXIT();
}

void
g_abort()
{
    globus_mutex_lock(&g_monitor.mutex);
    {   
        g_monitor.abort = GLOBUS_TRUE;
        globus_cond_signal(&g_monitor.cond);
    }
    globus_mutex_unlock(&g_monitor.mutex);
}

void
g_passive(globus_bool_t spas)
{
    globus_result_t                             res;
    globus_ftp_control_host_port_t              host_port;
    int                                         hi;
    int                                         low;
    unsigned char *                             a;

    if (!logged_in)   
    {
        reply(530, "Login with USER first.");
        return;
    }

    host_port.port = 0;
    res = globus_ftp_control_local_pasv(
              &g_data_handle,
              &host_port);
    if(res != GLOBUS_SUCCESS)
    {
        perror_reply(425, 
                 globus_object_printable_to_string(globus_error_get(res)));

        return;
    }

    a = (unsigned char *)&ctrl_addr.sin_addr;
    host_port.host[0] = (int) a[0];
    host_port.host[1] = (int) a[1];
    host_port.host[2] = (int) a[2];
    host_port.host[3] = (int) a[3];

    hi = host_port.port / 256;
    low = host_port.port % 256;

    if(spas)
    {
	lreply(229, "Entering Striped Passive Mode");
	lreply(0, " %d,%d,%d,%d,%d,%d",
	       host_port.host[0],
	       host_port.host[1],
	       host_port.host[2],
	       host_port.host[3],
	       hi,
	       low);
	reply(229, "End");

    }
    else
    {
	reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", 
	      host_port.host[0],
	      host_port.host[1],
	      host_port.host[2],
	      host_port.host[3],
	      hi,
	      low);
    }
}


/*
 *  what to do if it times out
 *
 *  send error message on control connection
 */
static SIGNAL_TYPE 
g_alarm_signal(
    int                                             sig)
{
    globus_mutex_lock(&g_monitor.mutex);
    {
        g_monitor.timed_out = GLOBUS_TRUE;
        g_monitor.done = GLOBUS_TRUE;
        globus_cond_signal(&g_monitor.cond);
    }
    globus_mutex_unlock(&g_monitor.mutex);
}

/*
 *  if the restart marker is bad return 0 - -1
 */
int
invert_restart(
    off_t *                                        offset_a, 
    off_t *                                        length_a) 
{
    int                                            start = 0;
    globus_l_wu_range_t *			   tmp;

    if(globus_fifo_size(&g_restarts) == 0)
    {
        offset_a[0] = 0;
        length_a[0] = -1;
  
        return 1;
    }
    tmp = (globus_l_wu_range_t *) globus_fifo_peek(&g_restarts);

    if(tmp->offset != 0)
    {
        offset_a[0] = 0;
        length_a[0] = tmp->offset;
        start++;
    }

    while(globus_fifo_size(&g_restarts) != 1)
    {
	tmp = (globus_l_wu_range_t *) globus_fifo_dequeue(&g_restarts);

	offset_a[start] = tmp->offset + tmp->length;
	globus_libc_free(tmp);

	tmp = (globus_l_wu_range_t *) globus_fifo_peek(&g_restarts);
        length_a[start] = tmp->offset - offset_a[start];

        start++;
    }
    tmp = (globus_l_wu_range_t *) globus_fifo_dequeue(&g_restarts);

    offset_a[start] = tmp->offset + tmp->length;
    length_a[start] = -1;

    globus_libc_free(tmp);
    start++;
    return start;
}

/*
 *  globusified send data routine
 */
int
#ifdef THROUGHPUT
g_send_data(
    char *                                          name,
    FILE *                                          instr,
    globus_ftp_control_handle_t *                   handle,
    off_t                                           offset,
    off_t                                           blksize,
    off_t                                           length,
    off_t					    size)
#else
g_send_data(
    FILE *                                          instr,
    globus_ftp_control_handle_t *                   handle,
    off_t                                           offset,
    off_t                                           blksize,
    off_t                                           length,
    off_t					    size)
#endif
{
    int                                             jb_count;
    int                                             jb_len;
    register int                                    c;
    register int                                    cnt = 0;
    globus_byte_t *                                 buf = GLOBUS_NULL;
    int                                             filefd;
    globus_bool_t                                   eof = GLOBUS_FALSE;
    globus_bool_t                                   aborted;
    int                                             cb_count = 0;
    globus_result_t                                 res;
    int                                             buffer_ndx;
    int                                             file_ndx;
    int                                             connection_count = 4;
    globus_bool_t                                   l_timed_out = GLOBUS_FALSE;
    globus_ftp_control_parallelism_t                parallelism;
    off_t *                                         offset_a;
    off_t *                                         length_a;
    int                                             count_a;
    int                                             ctr;
    off_t                                           skipped_offset;
    char                                            error_buf[1024];
#if defined(STRIPED_SERVER_BACKEND)
    bmap_offs_t                                     offs_out = -1;
#else
    off_t                                          offs_out = -1;
#endif

#ifdef THROUGHPUT
    int                                             bps;
    double                                          bpsmult;
    time_t                                          t1;
    time_t                                          t2;
#endif

    G_ENTER();

    error_buf[0] = '\0';
#ifdef THROUGHPUT
    throughput_calc(name, &bps, &bpsmult);
#endif

    wu_monitor_reset(&g_monitor);

#if defined (STRIPED_SERVER_BACKEND)
    if(G_File_Open(
	&(g_monitor.bmap_handle),
	name,O_RDONLY,
	fileno(instr)) != 0)
#else
    if(G_File_Open(
	&(g_monitor.io_handle),
	NULL,
	NULL,
	fileno(instr)) != 0)
#endif
     {
         sprintf(error_buf, "file_open failed");
         goto data_err;
     }
           
    if(offset == -1)
    {
        offset = 0;
    }
    /*
     *  perhaps a time out should be added here
     */
    (void) signal(SIGALRM, g_alarm_signal);
    alarm(timeout_connect);

    if(mode == MODE_E)
    {
	if(g_layout.mode == GLOBUS_FTP_CONTROL_STRIPING_PARTITIONED)
	{
	    if(retrieve_is_data)
	    {
#if defined (STRIPED_SERVER_BACKEND)
                if(g_striped_file_size > 0)
                {
                     g_layout.partitioned.size = g_striped_file_size;
                }
                else
#endif
		g_layout.partitioned.size = size;

		globus_ftp_control_local_layout(handle, &g_layout, 0);
	    }
	}
	else
	{
	    globus_ftp_control_local_layout(handle, &g_layout, 0);
	}
	globus_ftp_control_local_parallelism(handle,
					     &g_parallelism);

    }
    wu_monitor_reset(&g_monitor);
    res = globus_ftp_control_data_connect_write(
              handle,
              connect_callback,
              (void *)&g_monitor);
    if(res != GLOBUS_SUCCESS)
    {
        sprintf(error_buf, "Connect_write() failed: %s.", globus_object_printable_to_string(globus_error_get(res)));
        goto data_err;
    }

    globus_mutex_lock(&g_monitor.mutex);
    {
        while(!g_monitor.done)
        {
            globus_cond_wait(&g_monitor.cond, &g_monitor.mutex);
        }
        l_timed_out = g_monitor.timed_out;
    }
    globus_mutex_unlock(&g_monitor.mutex);

    if(l_timed_out)
    {
        goto connect_err;
    }
    if(g_monitor.error != GLOBUS_NULL)
    {
        sprintf(error_buf, "data_connect_failed() failed: %s", 
                globus_object_printable_to_string(g_monitor.error));
        globus_object_free(g_monitor.error);

        goto connect_err;
    }

    G_EXIT();
    if(g_monitor.code == 150)
    {
       reply(150, "Opening %s mode data connection.",
              type == TYPE_A ? "ASCII" : "BINARY");
    }
    else
    {
       reply(125, "Reusing %s mode data connection.",
              type == TYPE_A ? "ASCII" : "BINARY");
    }
    G_ENTER();

    transflag++;
    switch (type) 
    {

    case TYPE_A:
    case TYPE_I:
    case TYPE_L:

#       ifdef THROUGHPUT
        {
            if (bps != -1)
            {
                blksize = bps;
            }
        }
#       endif

        filefd = fileno(instr);

        /*
         *  set timeout
         */
        (void) signal(SIGALRM, g_alarm_signal);
        alarm(timeout_data);

#       ifdef THROUGHPUT
        {
            if (bps != -1)
            {
                t1 = time(NULL);
            }
       }
#      endif

        if(!globus_fifo_empty(&g_restarts))
        {
            offset_a = (off_t
			*)globus_malloc(sizeof(off_t)*
					(globus_fifo_size(&g_restarts) + 1));
            length_a = (off_t *)globus_malloc(sizeof(off_t)*(
		                               globus_fifo_size(&g_restarts) + 1));
            count_a = invert_restart(offset_a, length_a);
        }
        else
        {
            offset_a = (off_t *)globus_malloc(sizeof(off_t));
            length_a = (off_t *)globus_malloc(sizeof(off_t));
            offset_a[0] = offset;
            length_a[0] = length;
            count_a = 1;
        }

        wu_monitor_reset(&g_monitor);
        for(ctr = 0; ctr < count_a && !eof; ctr++)
        {
            g_seek(instr, offset_a[ctr]);

            offset = offset_a[ctr];
            length = length_a[ctr];
            jb_count = 0;
            while ((jb_count < length || length == -1) && !eof)
            {
                /*
                 *  allocate a buffer for each send
                 */
                if ((buf = (char *) globus_malloc(blksize)) == NULL)  
                {
                    transflag = 0;
 
                    G_EXIT();
                    perror_reply(451, "Local resource failure: malloc");
                    retrieve_is_data = 1;
		    goto bail0;
                }

                if(length == -1 || length - jb_count >= blksize)
                {
                    jb_len = blksize;
                }
                else
                {
                    jb_len = length - jb_count;
                }
                offs_out = -1;
#            if defined(STRIPED_SERVER_BACKEND)
                cnt = G_File_Read(
                          g_monitor.bmap_handle,
                          filefd,
                          buf,
                          jb_len,
                          offset,
                          &offs_out);
#            else
                cnt = G_File_Read(
		          &g_monitor.io_handle,
                          filefd,
                          buf,
                          jb_len,
                          offset,
                          &offs_out);

#            endif
            
               if(offs_out > 0 && cnt > 0)
               {
                   offset = offs_out;
               }  

               if (cnt <= 0 )
               {
                  offset = 0;
                  cnt = 0;
               }

                /*
                 *  if file eof, or we have read the portion we want  to 
                 *  send in a partial file transfer set eof to true
                 */
                if(cnt <= 0 || 
                   (jb_count + cnt == length && length != -1 && ((ctr+1) == count_a)))
		{
                    eof = GLOBUS_TRUE;
                }

                res = globus_ftp_control_data_write(
                          handle,
                          buf,
                          cnt,
                          offset,
                          eof,
                          data_write_callback,
                          &g_monitor);
                if(res != GLOBUS_SUCCESS)
                {
                    sprintf(error_buf, "data_write() failed: %s", 
                        globus_object_printable_to_string(
                            globus_error_get(res)));
                    goto data_err;
                }
		buf = GLOBUS_NULL; /* So that time outs or or other goto
				      data errs don't free the buffer. */
                cb_count++;
                offset += cnt;
                jb_count += cnt;

		res = globus_ftp_control_get_parallelism(
			  handle,
			  &parallelism);
                assert(res == GLOBUS_SUCCESS);

		connection_count = 2*parallelism.base.size;

                globus_mutex_lock(&g_monitor.mutex);
                {   
                    g_monitor.count++;
                    while(g_monitor.count == connection_count && 
                          !g_monitor.abort &&
                          !g_monitor.timed_out)
                    {
                        globus_cond_wait(&g_monitor.cond, &g_monitor.mutex);
                    }
                }
                globus_mutex_unlock(&g_monitor.mutex);
            }
            globus_mutex_lock(&g_monitor.mutex);
            {   
                while(g_monitor.count != 0)
                {
                    globus_cond_wait(&g_monitor.cond, &g_monitor.mutex);
                }
            }
            globus_mutex_unlock(&g_monitor.mutex);

            byte_count += cnt;
#           ifdef TRANSFER_COUNT
            {
                if (retrieve_is_data)
                {
#                   ifdef RATIO
                    {
                        if(freefile)
                        {
                            total_free_dl += cnt;
                        }
                    }
#                   endif /* RATIO */

                    data_count_total += cnt;
                    data_count_out += cnt;
                }
                byte_count_total += cnt;
                byte_count_out += cnt;

            }    
#           endif

#           ifdef THROUGHPUT
            {
                if (bps != -1)
                {
                    t2 = time(NULL);
                    if (t2 == t1)
                    {
                        sleep(1);
                    }
                    t1 = time(NULL);
                }
            }
#           endif
        } /* end while */

#       ifdef THROUGHPUT
        {
            if (bps != -1)
            {
                throughput_adjust(name);
            }
        }
#       endif


        /*
         *  wait until the eof callback is received
         */
        globus_mutex_lock(&g_monitor.mutex);
        {   
            while(g_monitor.count > 0)
            {
                globus_cond_wait(&g_monitor.cond, &g_monitor.mutex);
            }
            l_timed_out = g_monitor.timed_out;
            aborted = g_monitor.abort;
        }
        globus_mutex_unlock(&g_monitor.mutex);

        transflag = 0;
        if(aborted)
        {
            alarm(0);
            transflag = 0;
            retrieve_is_data = 1;
    
            g_force_close(cb_count);

            G_EXIT();
	    goto bail0;
        }

        if(l_timed_out)
        {
            sprintf(error_buf, "TImed out");
            goto data_err;
        }

        /* 
         *  unset alarm 
         */
        alarm(0);

        G_EXIT();

#if defined(STRIPED_SERVER_BACKEND)
        /*
         *  send 126 message
         */
        if(mode == MODE_E)
        {
            int                           data_connections;
            int                           stripe_count;
            int                           ctr_126;
            int                           i_126 = 0;
            char *                        buf_126;

            res = globus_ftp_control_get_stripe_count(
                      handle,
                      &stripe_count);

            if(res != GLOBUS_SUCCESS || stripe_count == 0)
            {
                sprintf(error_buf, "Couldn't get count.");
                goto data_err;
            }

            buf_126 = (char *)malloc(15 * stripe_count);
            for(ctr_126 = 0; ctr_126 < stripe_count; ctr_126++)
            {
                res = globus_ftp_control_data_get_total_data_channels(
                          handle,
                          &data_connections,
                          ctr_126);
                if(res != GLOBUS_SUCCESS)
                {
            sprintf(error_buf, "Couldn't get data channdle count.");
                    goto data_err;
                }
                sprintf(&buf_126[i_126], "%d ", data_connections);
                i_126 = strlen(buf_126);
            }
            buf_126[i_126] = '\0';

            reply(126, "%s", buf_126);
            free(buf_126);
        }
#       endif /* STRIPED_SERVER_BACKEND */

        reply(226, "Transfer complete.");
        G_ENTER();

#       ifdef TRANSFER_COUNT
        {
            if (retrieve_is_data) 
            {
                file_count_total++;
                file_count_out++;
            }
            xfer_count_total++;
            xfer_count_out++;
        }
#       endif
 
        retrieve_is_data = 1;

        /*
         *  EXIT POINT 
         */
        goto clean_exit;


    default:
        transflag = 0;

        G_EXIT();
        reply(550, "Unimplemented TYPE %d in send_data", type);
        retrieve_is_data = 1;

	goto bail1;
    }
 
  /* 
   *  DATA_ERR
   */
  data_err:
    alarm(0);
    transflag = 0;

    g_force_close(cb_count);

    if(buf != GLOBUS_NULL)
    {
        globus_free(buf);
    }

    G_EXIT();
    reply(426, "Data connection. %s", error_buf);
 
    retrieve_is_data = 1;

    goto bail0;

  connect_err:
    alarm(0);
    transflag = 0;

    G_EXIT();
    reply(425, "Can't open data connection. %s.", error_buf);

    goto bail0;

  /*
   *  FILE_ERR
   */
  file_err:
    alarm(0);
    transflag = 0;
    G_EXIT();
    perror_reply(551, "Error on input file");
    retrieve_is_data = 1;

    goto bail0;

  bail0:
#if defined(STRIPED_SERVER_BACKEND)
    G_File_Close(g_monitor.bmap_handle, filefd);
#else
    G_File_Close(&g_monitor.io_handle, 0);
#endif 

    globus_i_wu_free_ranges(&g_restarts);
    return (0);
  bail1:
#if defined(STRIPED_SERVER_BACKEND)
    G_File_Close(g_monitor.bmap_handle, filefd);
#else
    G_File_Close(&g_monitor.io_handle, 0);
#endif 
    globus_i_wu_free_ranges(&g_restarts);
    return (1);

  clean_exit:
#if defined(STRIPED_SERVER_BACKEND)
    G_File_Close(g_monitor.bmap_handle, filefd);
#else
    G_File_Close(&g_monitor.io_handle, 0);
#endif 

    G_EXIT();
    return (1);
}

/*
 *  force close the data connection
 *  wait for all of the callbacks to return and for the 
 *  close callback
 */
void
g_force_close(
    int                                     cb_count)
{
    globus_result_t                         res;

    G_ENTER();

    g_monitor.done = GLOBUS_FALSE;
    res = globus_ftp_control_data_force_close(
              &g_data_handle,
              data_close_callback,
              (void*)&g_monitor);
    if(res == GLOBUS_SUCCESS)
    {
        /*
         *  wait for close all of the calbacks and for the
         *  close callback.
         */
        globus_mutex_lock(&g_monitor.mutex);
        {   
            while(!g_monitor.done || g_monitor.count < cb_count)
            {
                globus_cond_wait(&g_monitor.cond, &g_monitor.mutex);
            }
        }
        globus_mutex_unlock(&g_monitor.mutex);
    }

    G_EXIT();
}

void 
data_close_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    globus_object_t *                           error)
{
    globus_i_wu_monitor_t *                      monitor;

    monitor = (globus_i_wu_monitor_t *)callback_arg;

    globus_mutex_lock(&monitor->mutex);
    {
        monitor->done = GLOBUS_TRUE;
        globus_cond_signal(&monitor->cond);
    }
    globus_mutex_unlock(&monitor->mutex);
}


void
data_write_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof)
{
    globus_i_wu_monitor_t *                         monitor;

    monitor = (globus_i_wu_monitor_t *)callback_arg;

    globus_mutex_lock(&monitor->mutex);
    {
        monitor->count--;
        globus_cond_signal(&monitor->cond);
    }
    globus_mutex_unlock(&monitor->mutex);

    globus_free(buffer);
}

/*
 *  globus hacked receive data
 *  --------------------------
 */
int 
g_receive_data(
    globus_ftp_control_handle_t *            handle,
    FILE *                                   outstr,
    off_t                                    offset,
    char *                                   fname)
{
    register int                             c;
    int                                      cnt = 0;
    int                                      bare_lfs = 0;
    globus_byte_t *                          buf;
    int                                      netfd;
    int                                      filefd;
    globus_bool_t                            eof = GLOBUS_FALSE;
    globus_bool_t                            l_timed_out;
    globus_bool_t                            l_error;
    globus_bool_t                            aborted;
    globus_result_t                          res;
    int                                      ctr;
    int                                      cb_count = 0;
    int                                      data_connection_count = 1;
    globus_reltime_t			     five_seconds;
    char                                     error_buf[1024];
#ifdef BUFFER_SIZE
    size_t                                   buffer_size = BUFFER_SIZE;
#else
    size_t                                   buffer_size = BUFSIZ;
#endif

    G_ENTER();
    error_buf[0] = '\0';
    wu_monitor_reset(&g_monitor);
    g_monitor.offset = offset;

    (void) signal(SIGALRM, g_alarm_signal);
    alarm(timeout_accept);

#if defined(STRIPED_SERVER_BACKEND)
    if(G_File_Open( 
	&(g_monitor.bmap_handle),
	fname,
	O_WRONLY,
	fileno(outstr)) != 0 )
#else
    if(G_File_Open(
	&(g_monitor.io_handle),
	GLOBUS_NULL,
	GLOBUS_NULL,
	fileno(outstr)) != 0)
#endif  /* STRIPED_SERVER_BACKEND */
    {
	sprintf(error_buf, "file_open() failed");
	goto data_err;
    }


    res = globus_ftp_control_data_connect_read(
              handle,
              connect_callback,
              (void *)&g_monitor);
    if(res != GLOBUS_SUCCESS)
    {
             sprintf(error_buf, "connect_read() failed");
        goto data_err;
    }

    globus_mutex_lock(&g_monitor.mutex);
    {
        while(!g_monitor.done)
        {
            globus_cond_wait(&g_monitor.cond, &g_monitor.mutex);
        }
        l_timed_out = g_monitor.timed_out;
    }
    globus_mutex_unlock(&g_monitor.mutex);

    if(l_timed_out)
    {
        sprintf(error_buf, "timed out() failed");
        goto connect_err;
    }
    if(g_monitor.error != GLOBUS_NULL)
    {
        sprintf(error_buf, "data_connect_failed() failed: %s", 
                globus_object_printable_to_string(g_monitor.error));
        globus_object_free(g_monitor.error);

        goto connect_err;
    }

    G_EXIT();
    if(g_monitor.code == 150)
    {
       reply(150, "Opening %s mode data connection.",
              type == TYPE_A ? "ASCII" : "BINARY");
    }
    else
    {
       reply(125, "Reusing %s mode data connection.",
              type == TYPE_A ? "ASCII" : "BINARY");
    }
    G_ENTER();
    
    transflag++;
    switch (type) 
    {

    case TYPE_I:
    case TYPE_L:
    case TYPE_A:

    /* the globus code should handle ascii mode */

        filefd = fileno(outstr);

        (void) signal(SIGALRM, g_alarm_signal);
        alarm(timeout_data);

        g_monitor.count = 0;
        g_monitor.done = GLOBUS_FALSE;
        g_monitor.fd = filefd;
        cb_count = 0;
	res = globus_ftp_control_data_query_channels(
			  handle,
                          &data_connection_count,
                          0);
	assert(res == GLOBUS_SUCCESS);

	debug_printf("receive data parallelism: %d", 
		     data_connection_count);

        if(res == GLOBUS_SUCCESS)
        {
            data_connection_count *= 2;
        }
        else
        {
            data_connection_count = 2;
        }

	GlobusTimeReltimeSet(five_seconds, 5, 0);

        g_send_range = GLOBUS_FALSE;
        g_send_perf_update = GLOBUS_FALSE;
	globus_callback_register_periodic(
	    &g_monitor.callback_handle,
	    &five_seconds,
	    &five_seconds,
	    globus_l_wu_perf_update_callback,
	    &g_monitor,
	    GLOBUS_NULL,
	    GLOBUS_NULL);
					 
        g_monitor.callback_count = 0; 
        for(ctr = 0; ctr < data_connection_count; ctr++)
        {
            if ((buf = (globus_byte_t *) globus_malloc(buffer_size)) == NULL)
            {
                transflag = 0;
		globus_callback_unregister(g_monitor.callback_handle);

                G_EXIT();
                perror_reply(451, "Local resource failure: malloc");
                goto bail;
            }

            res = globus_ftp_control_data_read(
                      handle,
                      buf,
                      buffer_size,
                      data_read_callback,
                      (void *)&g_monitor);
            if(res != GLOBUS_SUCCESS)
            {
                globus_free(buf);
             sprintf(error_buf, "data_read() failed");
                goto data_err;
            }
            g_monitor.callback_count++;
            cb_count++;
        }

        globus_mutex_lock(&g_monitor.mutex);
        {
            while(!g_monitor.done && 
                  g_monitor.count < g_monitor.callback_count &&
                  !g_monitor.abort &&
                  !g_monitor.timed_out)
            {
                globus_cond_wait(&g_monitor.cond, &g_monitor.mutex);
		if(g_send_perf_update)
		{
		    g_send_perf_update = GLOBUS_FALSE;
		    globus_l_wu_perf_update(&g_monitor);
		}
		    if(g_send_range)
		{
		    g_send_range = GLOBUS_FALSE;
		    send_range(&g_monitor);
		}
            }
            l_timed_out = g_monitor.timed_out;
            l_error = g_monitor.done;
            aborted = g_monitor.abort;
        }
        globus_mutex_unlock(&g_monitor.mutex);

        transflag = 0;
        if(aborted)
        {
            alarm(0);
            g_force_close(cb_count);
	    globus_callback_unregister(g_monitor.callback_handle);

            G_EXIT();
	    goto bail;
        }

        if(l_timed_out || l_error)
        {
             sprintf(error_buf, "timed out failed");
            goto data_err;
        }

        alarm(0);

#       ifdef TRANSFER_COUNT
        {
            file_count_total++;
            file_count_in++;
            xfer_count_total++;
            xfer_count_in++;
        }
#       endif

        goto clean_exit;

    case TYPE_E:

        G_EXIT();
        reply(553, "TYPE E not implemented.");
        transflag = 0;
        goto bail;

    default:

        G_EXIT();
        reply(550, "Unimplemented TYPE %d in receive_data", type);
        transflag = 0;
	goto  bail;
    }

  data_err:

    g_force_close(cb_count);

    globus_callback_unregister(g_monitor.callback_handle);

    alarm(0);
    transflag = 0;
   
    G_EXIT();
    reply(426, "Data Connection. %s", error_buf);
    goto bail;

  connect_err:
    alarm(0);
    transflag = 0;

    G_EXIT();
    reply(425, "Can't open data connection. %s.", error_buf);

    goto bail;


  file_err:
    globus_callback_unregister(g_monitor.callback_handle);

    alarm(0);
    transflag = 0;

    G_EXIT();
    perror_reply(452, "Error writing file");

    bail:
#if defined(STRIPED_SERVER_BACKEND)
    G_File_Close(g_monitor.bmap_handle, filefd);
#else
    G_File_Close(&g_monitor.io_handle, 0);
#endif 
    globus_i_wu_free_ranges(&g_restarts);
    return (-1);

  clean_exit:
#if defined(STRIPED_SERVER_BACKEND)
    G_File_Close(g_monitor.bmap_handle, filefd);
#else
    G_File_Close(&g_monitor.io_handle, 0);
#endif 
    globus_callback_unregister(g_monitor.callback_handle);

    G_EXIT();
    return (0);
}

void
connect_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               reuse,
    globus_object_t *                           error)
{
    globus_i_wu_monitor_t *                      monitor;

    monitor = (globus_i_wu_monitor_t *)callback_arg;

    globus_mutex_lock(&monitor->mutex);
    {
         if(error != GLOBUS_NULL)
         {
             monitor->error = globus_object_copy(error);
         }
         else
         {
             monitor->error = GLOBUS_NULL;
         }

         monitor->done = GLOBUS_TRUE; 
         if(reuse) 
         {
             monitor->code = 125;
         }
         else
         {
             monitor->code = 150;
         }
         globus_cond_signal(&monitor->cond);
    }
    globus_mutex_unlock(&monitor->mutex);
}

void
send_range(
    globus_i_wu_monitor_t *                      monitor)
{
    char *		                        range_str;

    G_EXIT();

    range_str =
        globus_l_wu_create_range_string(&monitor->ranges);
		    
    reply(111, "Range Marker %s", range_str);
    globus_libc_free(range_str);

    G_ENTER();
}

void
data_read_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof)
{
    globus_i_wu_monitor_t *                      monitor;
    globus_result_t                             res;
    int                                         ret;
#ifdef BUFFER_SIZE
    size_t                                      buffer_size = BUFFER_SIZE;
#else
    size_t                                      buffer_size = BUFSIZ;
#endif

    monitor = (globus_i_wu_monitor_t *)callback_arg;

    globus_mutex_lock(&monitor->mutex);
    {
        if(error != GLOBUS_NULL)
        {
            monitor->count++;
            globus_cond_signal(&monitor->cond);
            globus_mutex_unlock(&monitor->mutex);

            return;
        }

        if(length > 0)
        {
            if(monitor->offset > 0)
            {
                offset = offset + monitor->offset;
            }
#        if defined(STRIPED_SERVER_BACKEND)
            ret = G_File_Write(
                      monitor->bmap_handle,
                      monitor->fd,
                      buffer,
                      length,
                      offset);
#        else
            ret = G_File_Write(
                      &monitor->io_handle,
                      monitor->fd,
                      buffer,
                      length,
                      offset);
#        endif
            if(ret <= 0)
            {
                /* How to signal error ? */
                assert(0);
                return;
            }

            byte_count += length;
#           ifdef TRANSFER_COUNT
            {
                data_count_total += length;
                data_count_in += length;
                byte_count_total += length;
                byte_count_in += length;
            }
#           endif
        }

	if(g_send_restart_info)
	{
	    time_t				t;

	    t = time(NULL);

	    globus_i_wu_insert_range(&monitor->ranges, offset, length);

	    if(t - monitor->last_range_update > 10)
	    {
                g_send_range = GLOBUS_TRUE;
                monitor->last_range_update = t;
                globus_cond_signal(&monitor->cond);
	    }
	}
	monitor->accum_bytes += length;
	monitor->all_transferred += length;

        if(eof)
        {
g_eof_receive = GLOBUS_TRUE;
            monitor->count++;
            globus_cond_signal(&monitor->cond);
            globus_free(buffer);
        }
        else
        {
            int                          data_connection_count;
            int                          new_callbacks;
            int                          ctr;

	    res = globus_ftp_control_data_query_channels(
	  		  handle,
                          &data_connection_count,
                          0);
	    assert(res == GLOBUS_SUCCESS);

            new_callbacks = 1;

            /*
             * this is done in case new connections have come in
             */
            if((data_connection_count * 2) > monitor->callback_count) 
            {
                new_callbacks = (data_connection_count * 2) -  monitor->callback_count + 1;
            }

                monitor->callback_count += (new_callbacks - 1);
            for(ctr = 0; ctr < new_callbacks; ctr++)
            {
                res = globus_ftp_control_data_read(
                          handle,
                          buffer,
                          buffer_size,
                          data_read_callback,
                          (void *)monitor);
                if(res != GLOBUS_SUCCESS)
                {
                    monitor->count++;
                    globus_free(buffer);
                    monitor->done = GLOBUS_TRUE;
                    globus_cond_signal(&monitor->cond);
            
                    globus_mutex_unlock(&monitor->mutex);

                    return;
                }
		if(ctr < new_callbacks-1)
		{
		    buffer = (globus_byte_t *) globus_malloc(buffer_size);
		}
            }
        }

        (void) signal(SIGALRM, g_alarm_signal);
        alarm(timeout_data);
    }
    globus_mutex_unlock(&monitor->mutex);
}

void
globus_i_wu_insert_range(
    globus_fifo_t *				ranges,
    globus_off_t				offset,
    globus_off_t				length)
{
    globus_fifo_t				tmp;
    globus_l_wu_range_t *			range;
    globus_l_wu_range_t *			newrange;

    globus_fifo_move(&tmp, ranges);

    while(!globus_fifo_empty(&tmp))
    {
	range = globus_fifo_dequeue(&tmp);
	if(offset <= range->offset)
	{
	    if(offset + length < range->offset)
	    {
		newrange = globus_malloc(sizeof(globus_l_wu_range_t));
		newrange->offset = offset;
		newrange->length = length;

		globus_fifo_enqueue(ranges, newrange);
		globus_fifo_enqueue(ranges, range);
		goto copy_rest;
	    }
	    else if(offset+length == range->offset)
	    {
		length += range->length;
		globus_libc_free(range);
	    }
	    else
	    {
		int newlength;

		/* weird.... overlapping data */
		newlength = range->offset + range->length - offset;
		if(newlength < length)
		{
		    newlength = length;
		}
		length = newlength;
		globus_libc_free(range);
	    }
	}
	else
	{
	    if(range->offset + range->length < offset)
	    {
		globus_fifo_enqueue(ranges, range);
	    }
	    else if(range->offset + range->length == offset)
	    {
		offset = range->offset;
		length += range->length;
		globus_libc_free(range);
	    }
	    else
	    {
		globus_fifo_enqueue(ranges, range);
	    }
	}
    }

    newrange = globus_malloc(sizeof(globus_l_wu_range_t));
    newrange->offset = offset;
    newrange->length = length;
    globus_fifo_enqueue(ranges, newrange);
copy_rest:
    while(! globus_fifo_empty(&tmp))
    {
	globus_fifo_enqueue(ranges, globus_fifo_dequeue(&tmp));
    }
}

void
globus_i_wu_free_ranges(
    globus_fifo_t *				ranges)
{
    while(!globus_fifo_empty(ranges))
    {
	globus_l_wu_range_t *			range;
	range = (globus_l_wu_range_t *) globus_fifo_dequeue(ranges);
	globus_libc_free(range);
    }
}

static int
globus_l_wu_count_digits(int num)
{
    int digits = 1;

    if(num < 0)
    {
	digits++;
	num = -num;
    }
    while(0 < (num = (num / 10))) digits++;

    return digits;
}

static char *
globus_l_wu_create_range_string(
    globus_fifo_t *                     ranges)
{
    int					length = 0, mylen;
    char *				buf = GLOBUS_NULL;
    globus_l_wu_range_t *		range;

    while((! globus_fifo_empty(ranges)) && (length < 4*1024))
    {
	range = globus_fifo_dequeue(ranges);

	mylen = globus_l_wu_count_digits(range->offset);
	mylen++;
	mylen += globus_l_wu_count_digits(range->offset+range->length);
	mylen++;

	buf = realloc(buf, length + mylen + 1);
	length += globus_libc_sprint_off_t(buf + length,
					   range->offset);
	length += sprintf(buf+length, "-");

	length += globus_libc_sprint_off_t(buf + length,
					   range->offset +
					   range->length);
	length += sprintf(buf + length, ",");
	globus_libc_free(range);
    }
    buf[strlen(buf)-1] = '\0';

    return buf;
}

static globus_bool_t
globus_l_wu_perf_update_callback(
    globus_abstime_t *			time_stop,
    void *				user_args)
{
    globus_i_wu_monitor_t *		monitor;

    monitor = (globus_i_wu_monitor_t *) user_args;

    globus_mutex_lock(&monitor->mutex);
    {
        g_send_perf_update = GLOBUS_TRUE;
        globus_cond_signal(&monitor->cond);
    }
    globus_mutex_lock(&monitor->mutex);

    return GLOBUS_TRUE;
}

static globus_bool_t 
globus_l_wu_perf_update(
    globus_i_wu_monitor_t *		monitor)
{
    struct timeval tv;
    unsigned int 			num_channels;
    double 				elapsed;
    double 				throughput;
    globus_ftp_control_handle_t *	handle;
    time_t				time;
    struct tm *				tm;

    if(!g_send_restart_info)
    {
	return GLOBUS_TRUE;
    }
    handle = monitor->handle;

    gettimeofday(&tv, NULL);
    time = tv.tv_sec;
    tm = gmtime(&time);
    globus_ftp_control_data_query_channels(handle, &num_channels, 0);
	
    elapsed = (double)(tv.tv_sec - monitor->last_perf_update.tv_sec);
	
    elapsed += (double)((tv.tv_usec - monitor->last_perf_update.tv_usec))
	               / (double) 1000000.0;
    throughput = ((double)monitor->accum_bytes) / elapsed;

    G_EXIT();	
    lreply(112,
	   "Perf Marker %04d%02d%02d%02d%02d%02d.%06d",
	   tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
	   tm->tm_hour, tm->tm_min, tm->tm_sec,
	   tv.tv_usec);
    lreply(0, " AllTransferred: %ld", (long) monitor->all_transferred);
    lreply(0, " AllConnections: %u", num_channels);
    lreply(0, " AllThroughput: %f", throughput);
    
    lreply(0, " StripeTransferred: 0 %ld", (long) monitor->all_transferred);
    lreply(0, " StripeConnections: 0 %u", num_channels);
    lreply(0, " StripeThroughput: 0 %f", throughput);
    reply(112, "End");

    monitor->accum_bytes = 0;
    monitor->last_perf_update = tv;

    return GLOBUS_TRUE;
}

int 
g_seek(
    FILE *                               fin,
    int                                  ndx)
{
    register int i;
    register int n;
    register int c;

    if (type == TYPE_A) 
    {
        n = ndx;
        i = 0;
        while (i++ < n) 
        {
            if ((c = getc(fin)) == EOF) 
            {
                return -1;
            }
            if (c == '\n')
            {
                i++;
            }
        }
    }
    else if (lseek(fileno(fin), ndx, SEEK_SET) < 0) 
    {
        return -1;
    }

    return 0;
}
    
void
g_set_tcp_buffer(int size)
{
    globus_ftp_control_tcpbuffer_t tcpbuffer;

    if(size != 0)
    {
	tcpbuffer.mode = GLOBUS_FTP_CONTROL_TCPBUFFER_FIXED;
	tcpbuffer.fixed.size = size;

        globus_ftp_control_local_tcp_buffer(&g_data_handle,
	                                    &tcpbuffer);
    }
    else
    {
	tcpbuffer.mode = GLOBUS_FTP_CONTROL_TCPBUFFER_DEFAULT;
        globus_ftp_control_local_tcp_buffer(&g_data_handle,
	                                    &tcpbuffer);
    }
}

#if defined(STRIPED_SERVER_BACKEND)

void
stripd_server_size(
    char *                          filename)
{
    int                             ret;
    bmap_file_t *                   bp;
    bmap_offs_t                     size;

    ret = bmap_file_open(&bp, filename, O_RDONLY, -1);
    if(ret != 0)
    {
        reply(550, "File %s could not be opened.", filename);
        return;
    }
    size = bmap_file_size(bp);
    bmap_file_close(bp);

    switch (type) 
    {
        case TYPE_L:
        case TYPE_I:
            reply(213, "%"L_FORMAT, size);
            break;

        case TYPE_A:
        default:
          reply(504, "SIZE not implemented for Type %c.", "?AEIL"[type]);
    }
}
#endif /* (STRIPED_SERVER_BACKEND */

#endif /* USE_GLOBUS_DATA_CODE */
