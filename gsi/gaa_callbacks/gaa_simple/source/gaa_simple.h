#ifndef _GAA_SIMPLE_H_
#define _GAA_SIMPLE_H_

struct gaa_simple_eacl_args {
    char *dirname;
    FILE *errfile;
};

typedef struct gaa_simple_eacl_args gaa_simple_eacl_args;

typedef struct {
    char *                              service_type;
    char *                              urlbase;
    char *                              restrictions;
    char *				start_time;
    char *				end_time;
    char **                             actions;
} gaa_simple_callback_arg_t;

extern gaa_status
gaa_simple_read_eacl(
    gaa_ptr                             gaa,
    gaa_policy **                       policy,
    gaa_string_data                     object,
    void *                              params);

            
extern gaa_status
gaa_simple_parse_restrictions(
    gaa_ptr                             gaa,
    gaa_policy **                       policy,
    gaa_string_data                     object,
    void *                              params); 

extern gaa_status
gaa_simple_assert_cred_pull(
    gaa_ptr                             gaa,
    gaa_sc_ptr                          sc,
    gaa_cred_type                       which,
    void *                              params);

extern gaa_status
gaa_simple_assert_cred_eval(
    gaa_ptr                             gaa,
    gaa_sc_ptr                          sc,
    gaa_cred *                          cred,
    void *                              raw,
    gaa_cred_type                       cred_type,
    void *                              params);

extern gaa_status
gaa_simple_check_id_cond(
    gaa_ptr                             gaa,
    gaa_sc_ptr                          sc,
    gaa_condition *                     cond,
    gaa_time_period *                   valid_time,
    gaa_list_ptr                        req_options,
    gaa_status *                        output_flags,
    void *                              params);

extern gaa_status
gaa_simple_assert_cred_verify(
    gaa_cred *                          cred,
    void *                              params);

extern gaa_status
gaa_simple_check_group_cond(
    gaa_ptr                             gaa,
    gaa_sc_ptr                          sc,
    gaa_condition *                     cond,
    gaa_time_period *                   valid_time,
    gaa_list_ptr                        req_options,
    gaa_status *                        output_flags,
    void *                              params);

extern gaa_status
gaa_simple_check_id_cond_nocase(
    gaa_ptr                             gaa,
    gaa_sc_ptr                          sc,
    gaa_condition *                     cond,
    gaa_time_period *                   valid_time,
    gaa_list_ptr                        req_options,
    gaa_status *                        output_flags,
    void *                              params);


extern gaa_status
gaa_simple_check_notbefore(gaa_ptr		gaa,
			   gaa_sc_ptr		sc,
			   gaa_condition *	cond,
			   gaa_time_period *	valid_time,
			   gaa_list_ptr		req_options,
			   gaa_status *    	output_flags,
			   void *		params);


extern gaa_status
gaa_simple_check_notafter(gaa_ptr		gaa,
			  gaa_sc_ptr		sc,
			  gaa_condition *	cond,
			  gaa_time_period *	valid_time,
			  gaa_list_ptr		req_options,
			  gaa_status *    	output_flags,
			  void *		params);

extern void gaa_simple_free_pval(
    void *                              pval);

#endif /* _GAA_SIMPLE_H_ */



