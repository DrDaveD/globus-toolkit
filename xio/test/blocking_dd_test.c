#include "globus_xio.h"
#include "globus_common.h"
#include "test_common.h"
#include "globus_xio_test_transport.h"

static globus_mutex_t                   globus_l_mutex;
static globus_cond_t                    globus_l_cond;
static globus_bool_t                    globus_l_closed = GLOBUS_FALSE;

#define USEC_THRESHHOLD  300000

int
blocking_dd_main(
    int                                     argc,
    char **                                 argv)
{
    int                                     rc;
    int                                     ctr;
    globus_xio_stack_t                      stack;
    globus_xio_handle_t                     handle;
    globus_xio_target_t                     target;
    globus_result_t                         res;
    globus_xio_attr_t                       attr;
    globus_byte_t *                         buffer;
    globus_size_t                           buffer_len;
    globus_size_t                           nbytes;
    globus_xio_data_descriptor_t            dd;
    int                                     opt_offset;

    globus_l_closed = GLOBUS_FALSE;

    rc = globus_module_activate(GLOBUS_XIO_MODULE);
    globus_assert(rc == 0);

    res = globus_xio_attr_init(&attr);
    test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);

    res = globus_xio_stack_init(&stack, NULL);
    test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);

    opt_offset = parse_parameters(argc, argv, stack, attr);
    if(opt_offset == argc)
    {
    }

    globus_mutex_init(&globus_l_mutex, NULL);
    globus_cond_init(&globus_l_cond, NULL);

    res = globus_xio_target_init(&target, NULL, "whatever", stack);
    test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);

    res = globus_xio_open(&handle, NULL, target);
    test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);

    buffer = globus_l_test_info.buffer;
    buffer_len = globus_l_test_info.buffer_length;

    res = globus_xio_data_descriptor_init(&dd, handle);
    test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);
    for(ctr = 0; ctr < 10; ctr++)
    {
        res = globus_xio_write(handle, buffer, buffer_len, buffer_len, &nbytes, dd);
        test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);

        res = globus_xio_read(
                handle, buffer, buffer_len, buffer_len, &nbytes, dd);
        test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);
    }
    res = globus_xio_data_descriptor_destroy(dd);
    test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);

    res = globus_xio_close(handle, NULL);
    test_res(GLOBUS_XIO_TEST_FAIL_NONE, res, __LINE__);

    globus_xio_attr_destroy(attr);
    globus_xio_stack_destroy(stack);
 
    test_common_end();

    rc = globus_module_deactivate(GLOBUS_XIO_MODULE);
    globus_assert(rc == 0);

    fprintf(stdout, "Success.\n");

    return 0;
}
