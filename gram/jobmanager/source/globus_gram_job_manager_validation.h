#ifndef GLOBUS_GRAM_JOB_MANAGER_VALIDATION_H
#define GLOBUS_GRAM_JOB_MANAGER_VALIDATION_H
/**
 * @defgroup globus_gram_job_manager_rsl_validation RSL Validation
 * RSL Validation
 *
 * Validates that a request's RSL contains only valid parameters, and that
 * all required parameters are defined.
 *
 * RSL Validation operates on an RSL, and one or more validation files.
 * The format of the validation files is defined in the
 * @ref globus_gram_job_manager_rsl_validation_file
 * section of the manual.
 */


/**
 * RSL Validation Record
 * @ingroup globus_gram_job_manager_rsl_validation
 *
 * Contains Information parsed from the validation file about a single
 * RSL parameter.
 */
typedef struct
{
    /** The name of the RSL attribute this record refers to. */
    char *				attribute;
    /**
     * A textual description of the RSL parameter. This is not
     * used other than for debugging the parser.
     */
    char *				description;
    /**
     * Default value of the parameter to be inserted in the RSL
     * if the parameter is not present.
     */
    char *				default_value;
    /**
     * String containing an enumeration of legal values for the
     * RSL parameter. For example, for the grammyjob parameter, this
     * would be "collective independent".
     */
    char *				enumerated_values;

    /**
     * Bitwise or of values of the
     * globus_i_gram_job_manager_validation_when_t values, indicated
     * when, if ever, this RSL parameter is required.
     */
    int					required_when;

    /**
     * Bitwise or of values of the
     * globus_i_gram_job_manager_validation_when_t values, indicated
     * when, if ever, this RSL parameter's default value should be
     * inserted into the RSL.
     */
    int					default_when;
    /**
     * Bitwise or of values of the
     * globus_i_gram_job_manager_validation_when_t values, indicated
     * when, if ever, this RSL parameter is valid.
     */
    int					valid_when;
}
globus_gram_job_manager_validation_record_t;
#endif /* GLOBUS_GRAM_JOB_MANAGER_VALIDATION_H */
