#ifndef GLOBUS_SCHEDULER_EVENT_GENERATOR_H
#define GLOBUS_SCHEDULER_EVENT_GENERATOR_H

#include "globus_common.h"

/** @ingroup seg_api
 * Error types used by the SEG.
 */
typedef enum
{
    /** NULL Parameter */
    GLOBUS_SEG_ERROR_TYPE_NULL = 1024,
    /** Already called a one-time function */
    GLOBUS_SEG_ERROR_TYPE_ALREADY_SET,
    /** Shared module missing descriptor */
    GLOBUS_SEG_ERROR_TYPE_INVALID_MODULE,
    /** Invalid printf format for SEG protocol message */
    GLOBUS_SEG_ERROR_TYPE_INVALID_FORMAT,
    /** Out of memory */
    GLOBUS_SEG_ERROR_TYPE_OUT_OF_MEMORY,
    /** Unable to load scheduler module */
    GLOBUS_SEG_ERROR_TYPE_LOADING_MODULE
}
globus_scheduler_event_generator_error_t;

#if __STDC_VERSION__ == 199901L
#    define MYNAME __func__
#elif defined(__GNUC__)
#    define MYNAME __FUNCTION__
#else
#    define MYNAME ""
#endif

typedef void (*globus_scheduler_event_generator_fault_t)(
    void *                              user_arg,
    globus_result_t                     fault);

#define GLOBUS_SEG_ERROR_NULL_OBJECT() \
    globus_error_construct_error(GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE, \
            NULL, \
            GLOBUS_SEG_ERROR_TYPE_NULL, \
            __FILE__, \
            __func__, \
            __LINE__, \
            "Null parameter")

#define GLOBUS_SEG_ERROR_ALREADY_SET_OBJECT() \
    globus_error_construct_error(GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE, \
            NULL, \
            GLOBUS_SEG_ERROR_TYPE_ALREADY_SET, \
            __FILE__, \
            __func__, \
            __LINE__, \
            "Value already set")
    
#define GLOBUS_SEG_ERROR_INVALID_MODULE_OBJECT(module, errmsg) \
    globus_error_construct_error(GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE, \
            NULL, \
            GLOBUS_SEG_ERROR_TYPE_INVALID_MODULE, \
            __FILE__, \
            __func__, \
            __LINE__, \
            "Invalid module %s: %s", \
            module, \
            errmsg)

#define GLOBUS_SEG_ERROR_INVALID_FORMAT_OBJECT(fmt) \
    globus_error_construct_error(GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE, \
            NULL, \
            GLOBUS_SEG_ERROR_TYPE_INVALID_FORMAT, \
            __FILE__, \
            __func__, \
            __LINE__, \
            "Invalid format %s", \
            fmt)

#define GLOBUS_SEG_ERROR_OUT_OF_MEMORY_OBJECT() \
    globus_error_construct_error(GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE, \
            NULL, \
            GLOBUS_SEG_ERROR_TYPE_OUT_OF_MEMORY, \
            __FILE__, \
            __func__, \
            __LINE__, \
            "Out of memory")

#define GLOBUS_SEG_ERROR_LOADING_MODULE_OBJECT(module, dlerr_msg) \
    globus_error_construct_error(GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE, \
            NULL, \
            GLOBUS_SEG_ERROR_TYPE_LOADING_MODULE, \
            __FILE__, \
            __func__, \
            __LINE__, \
            "Unable to dlopen module \"%s\": %s", \
            module_name, \
            dlerr_msg)

#define GLOBUS_SEG_ERROR_NULL \
    globus_error_put(GLOBUS_SEG_ERROR_NULL_OBJECT())

#define GLOBUS_SEG_ERROR_ALREADY_SET \
    globus_error_put(GLOBUS_SEG_ERROR_ALREADY_SET_OBJECT())

#define GLOBUS_SEG_ERROR_INVALID_MODULE(module, errmsg) \
    globus_error_put(GLOBUS_SEG_ERROR_INVALID_MODULE_OBJECT(module, errmsg))

#define GLOBUS_SEG_ERROR_INVALID_FORMAT(fmt) \
    globus_error_put(GLOBUS_SEG_ERROR_INVALID_FORMAT_OBJECT(fmt))

#define GLOBUS_SEG_ERROR_OUT_OF_MEMORY \
    globus_error_put(GLOBUS_SEG_ERROR_OUT_OF_MEMORY_OBJECT())

#define GLOBUS_SEG_ERROR_LOADING_MODULE(module, dlerr_msg) \
    globus_error_put(GLOBUS_SEG_ERROR_LOADING_MODULE_OBJECT(module, \
            dlerr_msg))

extern globus_module_descriptor_t globus_i_scheduler_event_generator_module;
#define GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE \
    (&globus_i_scheduler_event_generator_module)

/**
 * @defgroup seg_api Scheduler Implementation API
 */
globus_result_t
globus_scheduler_event(
    const char * format,
    ...);

globus_result_t
globus_scheduler_event_pending(
    time_t                              timestamp,
    const char *                        jobid);

globus_result_t
globus_scheduler_event_active(
    time_t                              timestamp,
    const char *                        jobid);

globus_result_t
globus_scheduler_event_failed(
    time_t                              timestamp,
    const char *                        jobid,
    int                                 failure_code);

globus_result_t
globus_scheduler_event_done(
    time_t                              timestamp,
    const char *                        jobid,
    int                                 exit_code);

/* API used by executable which drives the SEG */
globus_result_t
globus_scheduler_event_generator_set_timestamp(
    time_t                              timestamp);

globus_result_t
globus_scheduler_event_generator_load_module(
    const char *                        module_name);
typedef void (*globus_scheduler_event_generator_fault_handler_t)(
    void *                              user_arg,
    globus_result_t                     result);

globus_result_t
globus_scheduler_event_generator_set_fault_handler(
    globus_scheduler_event_generator_fault_handler_t
                                        fault_handler,
    void *                              user_arg);

#endif /* GLOBUS_SCHEDULER_EVENT_GENERATOR_H */
