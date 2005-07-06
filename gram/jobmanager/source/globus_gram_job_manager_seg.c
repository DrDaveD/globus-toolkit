#include "globus_common.h"
#include "globus_gram_job_manager.h"
#include "globus_scheduler_event_generator.h"
#include "globus_scheduler_event_generator_app.h"

#include <sys/types.h>
#include <utime.h>


globus_result_t
globus_l_gram_seg_event_callback(
    void *                              user_arg,
    const globus_scheduler_event_t *    event);

globus_result_t
globus_gram_job_manager_init_seg(
    globus_gram_jobmanager_request_t *  request)
{
    globus_module_activate(GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE);

    globus_scheduler_event_generator_set_event_handler(
            globus_l_gram_seg_event_callback,
            request);
    globus_scheduler_event_generator_set_timestamp(
            request->submission_timestamp);
    globus_scheduler_event_generator_load_module(
            request->seg_module);

    request->seg_started = GLOBUS_TRUE;

    return GLOBUS_SUCCESS;
}
/* globus_gram_job_manager_init_seg() */

globus_result_t
globus_gram_job_manager_shutdown_seg(
    globus_gram_jobmanager_request_t *  request)
{
    globus_module_deactivate(GLOBUS_SCHEDULER_EVENT_GENERATOR_MODULE);

    return GLOBUS_SUCCESS;
}
/* globus_gram_job_manager_init_seg() */

globus_result_t
globus_l_gram_seg_event_callback(
    void *                              user_arg,
    const globus_scheduler_event_t *    event)
{
    globus_gram_jobmanager_request_t *  request = user_arg;
    globus_bool_t                       event_registered;
    globus_result_t                     result;
    globus_scheduler_event_t *          new_event;

    if (event->event_type == GLOBUS_SCHEDULER_EVENT_RAW)
    {
        /* Deal with this later */
        return GLOBUS_SUCCESS;
    }

    globus_mutex_lock(&request->mutex);

    /* Keep the state file's timestamp up to date so that
     * anything scrubbing the state files of old and dead
     * processes leaves it alone */
    if(request->job_state_file)
    {
        utime(request->job_state_file, NULL);
    }

    if (strcmp(request->job_id, event->job_id) == 0)
    {
        result = globus_scheduler_event_copy(&new_event, event);

        if (result == GLOBUS_SUCCESS)
        {
            globus_fifo_enqueue(&request->seg_event_queue, new_event);
        }
        else
        {
            goto out;
        }

        if (request->jobmanager_state == GLOBUS_GRAM_JOB_MANAGER_STATE_POLL2)
        {
            do
            {
                event_registered =
                    globus_gram_job_manager_state_machine(request);
            }
            while (!event_registered);
        }
    }

    globus_mutex_unlock(&request->mutex);

out:
    return result;
}
/* globus_l_gram_seg_event_callback() */

void
globus_gram_job_manager_seg_handle_event(
    globus_gram_jobmanager_request_t *  request)
{
    globus_scheduler_event_t *          event;

    event = globus_fifo_dequeue(&request->seg_event_queue);

    if (globus_i_gram_job_manager_script_valid_state_change(
        request, event->event_type))
    {
        globus_gram_job_manager_request_set_status(
                request,
                event->event_type);
        request->unsent_status_change = GLOBUS_TRUE;
    }

    globus_scheduler_event_destroy(event);
}
/* globus_gram_job_manager_seg_handle_event() */
