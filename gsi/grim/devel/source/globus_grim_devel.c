#include "globus_gss_assist.h"
#include "globus_common.h"
#include "globus_error_string.h"
#include "globus_error.h"
#include "globus_error_gssapi.h"
#include "globus_openssl.h"
#include "globus_error_openssl.h"
#include "globus_gsi_cert_utils.h"
#include "globus_gsi_system_config.h"
#include "globus_gsi_proxy.h"
#include "globus_gsi_credential.h"
#include "globus_grim_devel.h"
#include "globus_error.h"
#include "proxycertinfo.h"
#include <expat.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "version.h"

#define GRIM_ASSERTION_FORMAT_VERSION "1"
#define GRIM_OID                      "1.3.6.1.4.1.3536.1.1.1.7"
#define GRIM_SN                       "GRIMPOLICY"
#define GRIM_LN                       "GRIM Policy Language"

/*************************************************************************
 *              internal data types
 ************************************************************************/
struct globus_l_grim_conf_info_s
{
    int                                     max_time;
    int                                     default_time;
    int                                     key_bits;
    char *                                  log_filename;
    char *                                  ca_cert_dir;
    char *                                  cert_filename;
    char *                                  key_filename;
    char *                                  gridmap_filename;
    char *                                  port_type_filename;
}; 

struct globus_l_grim_assertion_s
{
    char *                                  version;
    char *                                  issuer;
    char *                                  username;
    char **                                 dna;
    char **                                 port_types;

    int                                     parse_state;
    globus_list_t *                         dna_list;
    globus_list_t *                         pt_list;

    globus_result_t                         res;
};

enum globus_l_grim_assertion_parse_state_e
{
    GLOBUS_L_GRIM_PARSE_NONE,
    GLOBUS_L_GRIM_PARSE_USERNAME,
    GLOBUS_L_GRIM_PARSE_GRID_ID,
    GLOBUS_L_GRIM_PARSE_VERSION,
    GLOBUS_L_GRIM_PARSE_PORT_TYPE,
    GLOBUS_L_GRIM_PARSE_CLIENT_ID
};

/*************************************************************************
 *              internal function signatures
 ************************************************************************/
globus_result_t
globus_l_grim_parse_conf_file(
    struct globus_l_grim_conf_info_s *      info,
    FILE *                                  fptr);
 
globus_result_t
globus_l_grim_parse_assertion(
    struct globus_l_grim_assertion_s *      info,
    char *                                  string);

globus_result_t
globus_l_grim_build_assertion(
    struct globus_l_grim_assertion_s *      info,
    char **                                 out_string);

globus_result_t
globus_l_grim_devel_parse_port_type_file(
    FILE *                                  fptr,
    char *                                  username,
    char **                                 groups,
    char ***                                port_types);

static int globus_l_grim_devel_activate(void);
static int globus_l_grim_devel_deactivate(void);

/*************************************************************************
 *              global variables
 ************************************************************************/
static globus_bool_t                            globus_l_grim_activated 
                                                            = GLOBUS_FALSE;
static int                                      globus_l_grim_NID;

/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t globus_i_grim_devel_module =
{
    "globus_grim_devel",
    globus_l_grim_devel_activate,
    globus_l_grim_devel_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

/*************************************************************************
 *              external api functions
 ************************************************************************/

#define GlobusLGrimSetGetAssertionEnter(assertion, info)                    \
{                                                                           \
    if(assertion == NULL)                                                   \
    {                                                                       \
        return globus_error_put(                                            \
                   globus_error_construct_error(                            \
                       GLOBUS_GRIM_DEVEL_MODULE,                            \
                       GLOBUS_NULL,                                         \
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,               \
                       "[globus_grim_devel]::assertion handle is null."));  \
    }                                                                       \
    info = (struct globus_l_grim_assertion_s *) assertion;                  \
}

globus_result_t
globus_grim_get_default_configuration_filename(
    char **                                 conf_filename)
{
    globus_result_t                         res;
    char *                                  home_dir = NULL;
    char *                                  tmp_s;
    globus_gsi_statcheck_t                  status;

    /* if i am root */
    if(geteuid() == 0)
    {
        *conf_filename = strdup(GLOBUS_GRIM_DEFAULT_CONF_FILENAME);
        return GLOBUS_SUCCESS;
    }

    res = GLOBUS_GSI_SYSCONFIG_GET_HOME_DIR(&home_dir, &status);
    if(home_dir == NULL || status != GLOBUS_FILE_DIR || res != GLOBUS_SUCCESS)
    {
        *conf_filename = strdup(GLOBUS_GRIM_DEFAULT_CONF_FILENAME);
        return GLOBUS_SUCCESS;
    }

    tmp_s = globus_gsi_cert_utils_create_string(
                "%s/.globus/%s",
                home_dir,
                "grim-conf.xml");
    if(tmp_s == NULL)
    {
        return globus_error_put(
                   globus_error_construct_string(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       "[globus_grim_devel]:: failed to create string."));
    }
                
    *conf_filename = tmp_s;

    return GLOBUS_SUCCESS;

}

/*
 *  Assertion parsing
 */

/*
 *
 */
globus_result_t
globus_grim_assertion_init(
    globus_grim_assertion_t *               assertion,
    char *                                  issuer,
    char *                                  username)
{
    struct globus_l_grim_assertion_s *      ass;

    ass = (struct globus_l_grim_assertion_s *)
                globus_malloc(sizeof(struct globus_l_grim_assertion_s));
    if(ass == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_ALLOC,
                       "[globus_grim_devel]:: malloc failed."));
    }
    ass->issuer = strdup(issuer);
    ass->username = strdup(username);
    ass->dna = NULL;
    ass->port_types = NULL;
    *assertion = ass;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_assertion_init_from_buffer(
    globus_grim_assertion_t *               assertion,
    char *                                  buffer)
{
    globus_result_t                         res;
    struct globus_l_grim_assertion_s *      ass;
    
    if(assertion == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: assertion parameter NULL."));
    }
    if(buffer == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: buffer parameter NULL."));
    }

    ass = (struct globus_l_grim_assertion_s *)
                globus_malloc(sizeof(struct globus_l_grim_assertion_s));
    if(ass == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_ALLOC,
                       "[globus_grim_devel]:: malloc failed."));
    }

    res = globus_l_grim_parse_assertion(ass, buffer);

    return res;
}
    
/*
 *
 */
globus_result_t
globus_grim_assertion_serialize(
    globus_grim_assertion_t                 assertion,
    char **                                 out_assertion_string)
{
    globus_result_t                         res;
    struct globus_l_grim_assertion_s *      info;

    GlobusLGrimSetGetAssertionEnter(assertion, info);

    if(out_assertion_string == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: out_assertion_string is null."));
    }

    res = globus_l_grim_build_assertion(info, out_assertion_string);

    return res;
}
 
/*
 *
 */
globus_result_t
globus_grim_assertion_destroy(
    globus_grim_assertion_t                 assertion)
{
    struct globus_l_grim_assertion_s *      info;

    GlobusLGrimSetGetAssertionEnter(assertion, info);

    free(info->issuer);
    free(info->username);
    if(info->dna != NULL)
    {
        GlobusGrimFreeNullArray(info->dna);
    }
    if(info->port_types)
    {
        GlobusGrimFreeNullArray(info->port_types);
    }
    globus_free(info);

    return GLOBUS_SUCCESS;
}
    
/*
 *
 */
globus_result_t
globus_grim_assertion_get_issuer(
    globus_grim_assertion_t                 assertion,
    char **                                 issuer)
{
    struct globus_l_grim_assertion_s *      info;

    GlobusLGrimSetGetAssertionEnter(assertion, info);
    if(issuer == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: issuer is null."));
    }

    *issuer = info->issuer;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_assertion_get_username(
    globus_grim_assertion_t                 assertion,
    char **                                 username)
{
    struct globus_l_grim_assertion_s *      info;

    GlobusLGrimSetGetAssertionEnter(assertion, info);
    if(username == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: username is null."));
    }

    *username = info->username;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_assertion_get_dn_array(
    globus_grim_assertion_t                 assertion,
    char ***                                dn_array)
{
    struct globus_l_grim_assertion_s *      info;

    GlobusLGrimSetGetAssertionEnter(assertion, info);

    if(dn_array == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: dn_array is null."));
    }

    *dn_array = info->dna;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_assertion_set_dn_array(
    globus_grim_assertion_t                 assertion,
    char **                                 dn_array)
{
    struct globus_l_grim_assertion_s *      info;
    int                                     ctr;
    char **                                 tmp_a = NULL;

    GlobusLGrimSetGetAssertionEnter(assertion, info);

    if(dn_array != NULL)
    {
        ctr = 0;
        while(dn_array[ctr] != NULL)
        {
            ctr++;
        }
        ctr++;

        tmp_a = globus_malloc(sizeof(char *) * ctr);
    
        for(ctr = 0; dn_array[ctr] != NULL; ctr++)
        {
            tmp_a[ctr] = strdup(dn_array[ctr]);
        }
        tmp_a[ctr] = NULL;
    }

    if(info->dna != NULL)
    {
        GlobusGrimFreeNullArray(info->dna);
    }

    info->dna = tmp_a;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_assertion_get_port_types_array(
    globus_grim_assertion_t                 assertion,
    char ***                                port_types_array)
{
    struct globus_l_grim_assertion_s *      info;

    GlobusLGrimSetGetAssertionEnter(assertion, info);

    if(port_types_array == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: port_types_array is null."));
    }

    *port_types_array = info->port_types;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_assertion_set_port_types_array(
    globus_grim_assertion_t                 assertion,
    char **                                 port_types)
{
    struct globus_l_grim_assertion_s *      info;
    int                                     ctr;
    char **                                 tmp_a = NULL;

    GlobusLGrimSetGetAssertionEnter(assertion, info);

    if(port_types != NULL)
    {
        ctr = 0;
        while(port_types[ctr] != NULL)
        {
            ctr++;
        }
        ctr++;

        tmp_a = globus_malloc(sizeof(char *) * ctr);

        for(ctr = 0; port_types[ctr] != NULL; ctr++)
        {
            tmp_a[ctr] = strdup(port_types[ctr]);
        }
        tmp_a[ctr] = NULL;
    }

    if(info->port_types != NULL)
    {
        GlobusGrimFreeNullArray(info->port_types);
    }

    info->port_types = tmp_a;

    return GLOBUS_SUCCESS;
}


/*
 *  Config file parsing functions
 */
#define GlobusLGrimSetGetConfigEnter(config, _info)                         \
{                                                                           \
    if(config == NULL)                                                      \
    {                                                                       \
        return globus_error_put(                                            \
                   globus_error_construct_error(                            \
                       GLOBUS_GRIM_DEVEL_MODULE,                            \
                       GLOBUS_NULL,                                         \
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,               \
                       "[globus_grim_devel]::config handle is null."));     \
    }                                                                       \
    _info = (struct globus_l_grim_conf_info_s *) config;                    \
}

/*
 *
 */
globus_result_t
globus_grim_config_init(
    globus_grim_config_t *                  config)
{
    struct globus_l_grim_conf_info_s *      info;
    globus_result_t                         res;
    char *                                  home_dir = NULL;
    globus_gsi_statcheck_t                  status;

    info = (struct globus_l_grim_conf_info_s *)
                globus_malloc(sizeof(struct globus_l_grim_conf_info_s));
    if(info == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_ALLOC,
                       "[globus_grim_devel]:: malloc failed."));
    }

    /* if not root look in home dir */
    if(geteuid() == 0)
    {
        info->port_type_filename = 
            strdup(GLOBUS_GRIM_DEFAULT_PORT_TYPE_FILENAME);
    }
    else
    {
        res = GLOBUS_GSI_SYSCONFIG_GET_HOME_DIR(&home_dir, &status);
        if(home_dir == NULL || status != GLOBUS_FILE_DIR || 
            res != GLOBUS_SUCCESS)
        {
            info->port_type_filename = 
                strdup(GLOBUS_GRIM_DEFAULT_PORT_TYPE_FILENAME);
        }
        else
        {
            char *                          tmp_s;

            tmp_s = globus_gsi_cert_utils_create_string(
                        "%s/.globus/%s",
                        home_dir,
                        "grim-port-type.xml");
            if(tmp_s == NULL)
            {
                return globus_error_put(
                            globus_error_construct_string(
                            GLOBUS_GRIM_DEVEL_MODULE,
                            GLOBUS_NULL,
                            "[globus_grim_devel]:: failed to create string."));
            }
            info->port_type_filename = tmp_s;
        }
    }

    info->max_time = GLOBUS_GRIM_DEFAULT_MAX_TIME;
    info->default_time = GLOBUS_GRIM_DEFAULT_TIME;
    info->key_bits = GLOBUS_GRIM_DEFAULT_KEY_BITS;
    info->log_filename = NULL;
    info->ca_cert_dir = strdup(GLOBUS_GRIM_DEFAULT_CA_CERT_DIR);
    info->cert_filename = strdup(GLOBUS_GRIM_DEFAULT_CERT_FILENAME);
    info->key_filename = strdup(GLOBUS_GRIM_DEFAULT_KEY_FILENAME);
    info->gridmap_filename = strdup(GLOBUS_GRIM_DEFAULT_GRIDMAP);

    *config = info;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_load_from_file(
    globus_grim_config_t                    config,
    FILE *                                  fptr)
{
    globus_result_t                         res;
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);

    if(fptr == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: file pointer is NULL."));
    }

    res = globus_l_grim_parse_conf_file(info, fptr);

    return res;
}

/*
 *
 */
globus_result_t
globus_grim_config_destroy(
    globus_grim_config_t                    config)
{
    struct globus_l_grim_conf_info_s *      info;

    info = (struct globus_l_grim_conf_info_s *) config;
    free(info->ca_cert_dir);
    free(info->cert_filename);
    free(info->key_filename);
    free(info->gridmap_filename);
    free(info->port_type_filename);
    free(info);

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_get_max_time(
    globus_grim_config_t                    config,
    int *                                   max_time)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    if(max_time == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: max_time is NULL."));
    }

    *max_time = info->max_time;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_set_max_time(
    globus_grim_config_t                    config,
    int                                     max_time)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    info->max_time = max_time;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_get_default_time(
    globus_grim_config_t                    config,
    int *                                   default_time)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    if(default_time == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: default_time is NULL."));
    }

    *default_time = info->default_time;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_set_default_time(
    globus_grim_config_t                    config,
    int                                     default_time)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    info->default_time = default_time;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_get_key_bits(
    globus_grim_config_t                    config,
    int *                                   key_bits)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    if(key_bits == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: key_bits is NULL."));
    }

    *key_bits = info->key_bits;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_set_key_bits(
    globus_grim_config_t                    config,
    int                                     key_bits)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    info->key_bits = key_bits;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_get_ca_cert_dir(
    globus_grim_config_t                    config,
    char **                                 ca_cert_dir)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    if(ca_cert_dir == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: ca_cert_dir is NULL."));
    }

    *ca_cert_dir = info->ca_cert_dir;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_set_ca_cert_dir(
    globus_grim_config_t                    config,
    char *                                  ca_cert_dir)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    free(info->ca_cert_dir);
    info->ca_cert_dir = strdup(ca_cert_dir);

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_get_cert_filename(
    globus_grim_config_t                    config,
    char **                                 cert_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    if(cert_filename == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: cert_filename is NULL."));
    }

    *cert_filename = info->cert_filename;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_set_cert_filename(
    globus_grim_config_t                    config,
    char *                                  cert_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    free(info->cert_filename);
    info->cert_filename = strdup(cert_filename);

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_get_key_filename(
    globus_grim_config_t                    config,
    char **                                 key_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    if(key_filename == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: key_filename is NULL."));
    }

    *key_filename = info->key_filename;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_set_key_filename(
    globus_grim_config_t                    config,
    char *                                  key_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    free(info->key_filename);
    info->key_filename = strdup(key_filename);

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_get_gridmap_filename(
    globus_grim_config_t                    config,
    char **                                 gridmap_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    *gridmap_filename = info->gridmap_filename;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_set_gridmap_filename(
    globus_grim_config_t                    config,
    char *                                  gridmap_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    free(info->gridmap_filename);
    info->gridmap_filename = strdup(gridmap_filename);

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_get_port_type_filename(
    globus_grim_config_t                    config,
    char **                                 port_type_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    *port_type_filename = info->port_type_filename;

    return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_result_t
globus_grim_config_set_port_type_filename(
    globus_grim_config_t                    config,
    char *                                  port_type_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);
    free(info->port_type_filename);
    info->port_type_filename = strdup(port_type_filename);

    return GLOBUS_SUCCESS;
}


/*
 *
 */ 
globus_result_t
globus_grim_config_get_log_filename(
    globus_grim_config_t                    config,
    char **                                 log_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);

    *log_filename = info->log_filename;
    
    return GLOBUS_SUCCESS;
}
 
/*
 *
 */ 
globus_result_t
globus_grim_config_set_log_filename(
    globus_grim_config_t                    config,
    char *                                  log_filename)
{
    struct globus_l_grim_conf_info_s *      info;
    
    GlobusLGrimSetGetConfigEnter(config, info);

    if(info->log_filename != NULL)
    {
        free(info->log_filename);
    }

    if(log_filename != NULL)
    {
        info->log_filename = strdup(log_filename);
    }
    else
    {
        info->log_filename = NULL;
    }

    return GLOBUS_SUCCESS;
}


/*
 *  
 */
globus_result_t
globus_grim_devel_get_NID(
    int *                                   nid)
{
    if(!globus_l_grim_activated)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_NOT_ACTIVATED,
                       "[globus_grim_devel]:: Module not activated."));
    }

    *nid = globus_l_grim_NID;
    
    return GLOBUS_SUCCESS;
}

/*
 *  
 */
globus_result_t
globus_grim_devel_port_type_file_parse(
    FILE *                                  fptr,
    char *                                  username,
    char **                                 groups,
    char ***                                port_types)
{
    globus_result_t                         res;

    if(fptr == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: File pointer is NULL."));
    }
    if(port_types == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: port_types parameter is NULL."));
    }

    res = globus_l_grim_devel_parse_port_type_file(
              fptr, 
              username,
              groups,
              port_types);

    return res;
}

/*
 *  
 */
globus_result_t
globus_grim_devel_get_all_port_types(
    FILE *                                  fptr,
    char ***                                port_types)
{
    globus_result_t                         res;

    if(fptr == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: File pointer is NULL."));
    }
    if(port_types == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: port_types parameter is NULL."));
    }

    res = globus_l_grim_devel_parse_port_type_file(
              fptr, 
              NULL, 
              NULL,
              port_types);

    return res;
}

/*
 *  
 */
globus_result_t
globus_grim_devel_port_type_file_parse_uid(
    FILE *                                  fptr,
    char ***                                port_types)
{
    char *                                  username;
    char **                                 groups;
    int                                     groups_max = 16;
    int                                     groups_ndx;
    struct group *                          grp_ent;
    int                                     ctr;
    globus_result_t                         res;
    struct passwd *                         pw_ent;

    if(fptr == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: File pointer is NULL."));
    }
    if(port_types == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_BAD_PARAMETER,
                       "[globus_grim_devel]:: port_types parameter is NULL."));
    }

    /*
     *  get group list
     */
    pw_ent = getpwuid(getuid());
    username = strdup(pw_ent->pw_name);

    groups_ndx = 0;
    groups = (char **) globus_malloc(sizeof(char *) * groups_max);
    while((grp_ent = getgrent()) != NULL)
    {
        for(ctr = 0; grp_ent->gr_mem[ctr] != NULL; ctr++)
        {
            if(strcmp(username, grp_ent->gr_mem[ctr]) == 0)
            {
                groups[groups_ndx] = strdup(grp_ent->gr_name);
                groups_ndx++;
                if(groups_ndx >= groups_max)
                {
                    groups_max *= 2;
                    groups = (char **)
                        globus_realloc(groups, sizeof(char *) * groups_max);
                }
            }
        }
    }
    groups[groups_ndx] = NULL;

    res = globus_l_grim_devel_parse_port_type_file(
             fptr,
             username,
             groups,
             port_types);
 
    /*
     *  clean up
     */
    free(username);
    GlobusGrimFreeNullArray(groups);

    return res;
}

/*************************************************************************
 *              internal functions
 ************************************************************************/

static int 
globus_l_grim_devel_activate()
{
    int                                     rc;

    rc = globus_module_activate(GLOBUS_COMMON_MODULE);
    if(rc != 0)
    {
        return rc;
    }
    rc = globus_module_activate(GLOBUS_GSI_PROXY_MODULE);
    if(rc != 0)
    {
        return rc;
    }
    rc = globus_module_activate(GLOBUS_GSI_CALLBACK_MODULE);
    if(rc != 0)
    {
        return rc;
    }
    
    rc = globus_module_activate(GLOBUS_OPENSSL_MODULE);
    if(rc != 0)
    {
        return rc;
    }
    
    rc = globus_module_activate(GLOBUS_GSI_OPENSSL_ERROR_MODULE);
    if(rc != 0)
    {
        return rc;
    }

    rc = globus_module_activate(GLOBUS_GSI_GSSAPI_MODULE);
    if(rc != 0)
    {
        return rc;
    }

    globus_l_grim_activated = GLOBUS_TRUE;
    globus_l_grim_NID = OBJ_create(GRIM_OID, GRIM_SN, GRIM_LN);

    return GLOBUS_SUCCESS;
}

static int 
globus_l_grim_devel_deactivate()
{
    globus_module_deactivate(GLOBUS_COMMON_MODULE);
    globus_module_deactivate(GLOBUS_GSI_PROXY_MODULE);
    globus_module_deactivate(GLOBUS_GSI_CALLBACK_MODULE);
    globus_module_deactivate(GLOBUS_GSI_OPENSSL_ERROR_MODULE);
    globus_module_deactivate(GLOBUS_OPENSSL_MODULE);
    globus_module_deactivate(GLOBUS_GSI_GSSAPI_MODULE);

    globus_l_grim_activated = GLOBUS_FALSE;

    return GLOBUS_SUCCESS;
}


/************************************************************************
 *                 xml parsing code for port file
 ***********************************************************************/
struct globus_l_grim_port_type_info_s
{
    int                                     found;
    int                                     remove;
    char *                                  username;
    char **                                 groups;
    char *                                  port_type;
    int                                     port_type_len;

    globus_list_t *                         list;
};  

static void
globus_l_grim_port_type_start(
    void *                                  data,
    const char *                            el, 
    const char **                           attr)
{   
    struct globus_l_grim_port_type_info_s * info;
    int                                     ctr;
    int                                     ctr2;

    info = (struct globus_l_grim_port_type_info_s *) data;

    info->remove = 0;
    info->found = 0;
    info->port_type = NULL;
    info->port_type_len = 0;
    if(strcmp(el, "port_type") == 0)
    {
        /*
         *  if there are no attrs it mean accept all users
         *  if user or group is null it means the user is looking
         *  for a list of all port types
         */
        if(attr[0] == NULL || info->groups == NULL || info->username == NULL)
        {
            info->found = 1;
            return;
        }

        for(ctr = 0; attr[ctr] != NULL; ctr++)
        {
            if(strcmp(attr[ctr], "access") == 0)
            {
                if(strcmp(attr[ctr + 1], "no") == 0)
                {
                    info->remove = 1;
                }
                ctr++;
            }
            else if(strcmp(attr[ctr], "username") == 0 &&
                    strcmp(info->username, attr[ctr + 1]) == 0)
            {
                info->found = 1;
                ctr++;
            }
            else if(strcmp(attr[ctr], "group") == 0)
            {
                for(ctr2 = 0;
                    info->groups[ctr2] != NULL;
                    ctr2++)
                {
                    if(strcmp(info->groups[ctr2], attr[ctr + 1]) == 0)
                    {
                        info->found = 1;
                    }
                }
                ctr++;
            }
        }
    }
}

static void
globus_l_grim_port_type_end(
    void *                                  data,
    const char *                            el)
{
    struct globus_l_grim_port_type_info_s * info;
    globus_list_t *                         list;
    char *                                  pt;
    char *                                  s;
    char *                                  tmp_s;
    int                                     len;

    
    info = (struct globus_l_grim_port_type_info_s *) data;

    if(info->found)
    {
        s = info->port_type;
        len = info->port_type_len;
        
        while((*s == ' ' || *s == '\n' || *s == '\r') && *s != '\0')
        {
            s++;
            len--;
        }
        while((s[len-1] == ' ' || s[len-1] == '\n' || s[len-1] == '\r') 
            && len != 0)
        {
            len--;
        }
        if(len <= 0)
        {
            /* if the port type is super small do nothing */
            info->found = 0;
            info->remove = 0;
            free(info->port_type);
            info->port_type = NULL;

            return;
        }

        tmp_s = malloc(sizeof(char) * (len + 1));
        strncpy(tmp_s, s, len);
        tmp_s[len] = '\0';
        free(info->port_type);
        info->port_type = tmp_s;

        if(info->remove)
        {
            for(list = info->list; 
                !globus_list_empty(list); 
                list = globus_list_rest(list))
            {
                pt = (char *) globus_list_first(list);
                if(strcmp(pt, info->port_type) == 0)
                {
                    globus_list_remove(&info->list, list);
                    free(info->port_type);
                    free(pt);
                }
            }
        }
        else
        {
            globus_list_insert(&info->list, info->port_type);
        }
        info->found = 0;
        info->remove = 0;
        info->port_type = NULL;
    }
}

static void
globus_l_grim_port_type_cdata(
    void *                                  data,
    const XML_Char *                        s,
    int                                     len)
{   
    struct globus_l_grim_port_type_info_s * info;
    
    info = (struct globus_l_grim_port_type_info_s *) data;

    if(info->found)
    {
        if(len <= 0)
        {
            return;
        }

        info->port_type = realloc(info->port_type, info->port_type_len+len+1);
        strncpy(&info->port_type[info->port_type_len], s, len);
        info->port_type_len += len;
        info->port_type[info->port_type_len] = '\0';
    }
}   


globus_result_t
globus_l_grim_devel_parse_port_type_file(
    FILE *                                  fptr,
    char *                                  username,
    char **                                 groups,
    char ***                                port_types)
{
    char                                    buffer[512];
    size_t                                  len;
    XML_Parser                              p;
    int                                     done;
    globus_result_t                         res = GLOBUS_SUCCESS;
    char *                                  port_type;
    char **                                 pt_rc;
    int                                     ctr;
    struct globus_l_grim_port_type_info_s   info;

    p = XML_ParserCreate(NULL);
    if(p == NULL)
    {
        fclose(fptr);

        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: parser didn't open."));
    }

    XML_SetElementHandler(p, globus_l_grim_port_type_start, 
        globus_l_grim_port_type_end);
    info.list = NULL;
    info.found = 0;
    info.username = username;
    info.groups = groups;

    XML_SetUserData(p, &info);
    XML_SetCharacterDataHandler(p, globus_l_grim_port_type_cdata);

    done = 0;
    while(!done)
    {
        len = fread(buffer, 1, sizeof(buffer), fptr);
        done = len < sizeof(buffer);
        if(XML_Parse(p, buffer, len, len < done) == XML_STATUS_ERROR)
        {
            res = globus_error_put(
                     globus_error_construct_error(
                         GLOBUS_GRIM_DEVEL_MODULE,
                         GLOBUS_NULL,
                         GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                         "[globus_grim_devel]:: xml parser failure."));
            goto exit;
        }
    }

    pt_rc = (char **)
              globus_malloc((globus_list_size(info.list) + 1) * sizeof(char *));
    ctr = 0;
    while(!globus_list_empty(info.list))
    {
        port_type = (char *) globus_list_remove(&info.list, info.list);
        pt_rc[ctr] = port_type;
        ctr++;
    }
    pt_rc[ctr] = NULL;

    if(port_types != NULL)
    {
        *port_types = pt_rc;
    }

  exit:
    fclose(fptr);
    XML_ParserFree(p);

    return res;
}

/************************************************************************
 *                 xml parsing code for conf file
 ***********************************************************************/
static void
grim_conf_end(
    void *                                  data,
    const char *                            el)
{   
}   

static void
grim_conf_start(
    void *                                  data,
    const char *                            el,
    const char **                           attr)
{   
    struct globus_l_grim_conf_info_s *      info;

    info = (struct globus_l_grim_conf_info_s *) data;

    if(strcmp(el, "conf") == 0)
    {
        if(strcmp(attr[0], "max_time") == 0)
        {
            info->max_time = atoi(attr[1]);
        } 
        else if(strcmp(attr[0], "default_time") == 0)
        {
            info->default_time = atoi(attr[1]);
        }
        else if(strcmp(attr[0], "key_bits") == 0)
        {
            info->key_bits = atoi(attr[1]);
        }
        else if(strcmp(attr[0], "cert_filename") == 0)
        {
            free(info->cert_filename);
            info->cert_filename = strdup(attr[1]);
        }
        else if(strcmp(attr[0], "key_filename") == 0)
        {
            free(info->key_filename);
            info->key_filename = strdup(attr[1]);
        }
        else if(strcmp(attr[0], "gridmap_filename") == 0)
        {
            free(info->gridmap_filename);
            info->gridmap_filename = strdup(attr[1]);
        }
        else if(strcmp(attr[0], "port_type_filename") == 0)
        {
            free(info->port_type_filename);
            info->port_type_filename = strdup(attr[1]);
        }
        else if(strcmp(attr[0], "log_filename") == 0)
        {
            info->log_filename = strdup(attr[1]);
        }
    }
}

globus_result_t
globus_l_grim_parse_conf_file(
    struct globus_l_grim_conf_info_s *      info,
    FILE *                                  fptr)
{
    globus_result_t                         res = GLOBUS_SUCCESS;
    int                                     done;
    char                                    buffer[512];
    size_t                                  len;
    XML_Parser                              p = NULL;

    p = XML_ParserCreate(NULL);
    if(p == NULL)
    {
        res = globus_error_put(
                  globus_error_construct_error(
                      GLOBUS_GRIM_DEVEL_MODULE,
                      GLOBUS_NULL,
                      GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                      "[globus_grim_devel]:: could not create parser."));
        goto exit;
    }

    XML_SetElementHandler(p, grim_conf_start, grim_conf_end);
    XML_SetUserData(p, info);

    done = 0;
    while(!done)
    {
        len = fread(buffer, 1, sizeof(buffer), fptr);
        done = len < sizeof(buffer); 
        if(XML_Parse(p, buffer, len, len < done) == XML_STATUS_ERROR)
        {
            res = globus_error_put(
                      globus_error_construct_error(
                        GLOBUS_GRIM_DEVEL_MODULE,
                        GLOBUS_NULL,
                        GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                        "[globus_grim_devel]:: xml parse failed."));
            goto exit;
        }
    }

  exit:
    
    if(p != NULL)
    {
        XML_ParserFree(p);
    }

    return res;
}


/************************************************************************
 *              code for dealing with assertions
 ***********************************************************************/
#define GrowString(b, len, new, offset)                 \
{                                                       \
    if(len <= strlen(new) + offset)                     \
    {                                                   \
        len *= 2;                                       \
        b = globus_realloc(b, len);                     \
    }                                                   \
    strcpy(&b[offset], new);                            \
    offset += strlen(new);                              \
}

/*
 *  creating an assertion string
 */
globus_result_t
globus_l_grim_build_assertion(
    struct globus_l_grim_assertion_s *      info,
    char **                                 out_string)
{
    char *                                  buffer;
    int                                     ctr;
    int                                     buffer_size = 1024;
    int                                     buffer_ndx = 0;
    char                                    hostname[MAXHOSTNAMELEN];
    char *                                  issuer;
    char *                                  username;
    char **                                 dna;
    char **                                 port_types;

    issuer = info->issuer;
    username = info->username;
    dna = info->dna;
    port_types = info->port_types;

    globus_libc_gethostname(hostname, MAXHOSTNAMELEN);
    buffer = globus_malloc(sizeof(char) * buffer_size);

    GrowString(buffer, buffer_size, 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", buffer_ndx);
    GrowString(buffer, buffer_size, "<GRIMAssertion>\n", buffer_ndx);
    GrowString(buffer, buffer_size, "    <Version>", buffer_ndx);
    GrowString(buffer, buffer_size, GRIM_ASSERTION_FORMAT_VERSION, buffer_ndx);
    GrowString(buffer, buffer_size, "</Version>\n", buffer_ndx);
    GrowString(buffer, buffer_size,
        "    <ServiceGridId Format=\"#X509SubjectName\">",
        buffer_ndx);
    GrowString(buffer, buffer_size, issuer, buffer_ndx);
    GrowString(buffer, buffer_size, "</ServiceGridId>\n", buffer_ndx);
    GrowString(buffer, buffer_size,
        "    <ServiceLocalId Format=\"#UnixAccountName\" \n",
        buffer_ndx);
    GrowString(buffer, buffer_size, "        NameQualifier=\"", buffer_ndx);
    GrowString(buffer, buffer_size, hostname, buffer_ndx);
    GrowString(buffer, buffer_size, "\">", buffer_ndx);
    GrowString(buffer, buffer_size, username, buffer_ndx);
    GrowString(buffer, buffer_size, "</ServiceLocalId>\n", buffer_ndx);

    /* add all mapped dns */
    for(ctr = 0; dna != NULL && dna[ctr] != NULL; ctr++)
    {
        GrowString(buffer, buffer_size,
            "    <AuthorizedClientId Format=\"#X509SubjectName\">",
            buffer_ndx);
        GrowString(buffer, buffer_size, dna[ctr], buffer_ndx);
        GrowString(buffer, buffer_size, "</AuthorizedClientId>\n", buffer_ndx);
    }

    /* add in port_types */
    for(ctr = 0; port_types != NULL && port_types[ctr] != NULL; ctr++)
    {
        GrowString(buffer, buffer_size, "    <AuthorizedPortType>", buffer_ndx);
        GrowString(buffer, buffer_size, port_types[ctr], buffer_ndx);
        GrowString(buffer, buffer_size, "</AuthorizedPortType>\n", buffer_ndx);
    }

    GrowString(buffer, buffer_size, "</GRIMAssertion>\n", buffer_ndx);

    *out_string = buffer;

    return GLOBUS_SUCCESS;
}

/*
 *  parsing an assertion string
 */
static void
globus_l_grim_assertion_end(
    void *                                  data,
    const char *                            el)
{
    struct globus_l_grim_assertion_s *      info;

    info = (struct globus_l_grim_assertion_s *) data;

    info->parse_state = GLOBUS_L_GRIM_PARSE_NONE;
}

static void
globus_l_grim_assertion_start(
    void *                                  data,
    const char *                            el,
    const char **                           attr)
{
    struct globus_l_grim_assertion_s *      info;

    info = (struct globus_l_grim_assertion_s *) data;

    info->parse_state = GLOBUS_L_GRIM_PARSE_NONE;
    if(strcmp(el, "ServiceGridId") == 0)
    {
        info->parse_state = GLOBUS_L_GRIM_PARSE_GRID_ID;
    }
    else if(strcmp(el, "ServiceLocalId") == 0)
    {
        /* for now hostname is not stored
        for(ctr = 0; attr[ctr] != NULL; ctr += 2)
        {
            if(strcmp(attr[ctr], "NameQualifier") == 0)
            {
            }
        }
        */
        info->parse_state = GLOBUS_L_GRIM_PARSE_USERNAME;
    }
    else if(strcmp(el, "Version") == 0)
    {
        info->parse_state = GLOBUS_L_GRIM_PARSE_VERSION;
    }
    else if(strcmp(el, "AuthorizedClientId") == 0)
    {
        info->parse_state = GLOBUS_L_GRIM_PARSE_CLIENT_ID;
    }
    else if(strcmp(el, "AuthorizedPortType") == 0)
    {
        info->parse_state = GLOBUS_L_GRIM_PARSE_PORT_TYPE;
    }
}

static void
globus_l_grim_assertion_cdata(
    void *                                  data,
    const XML_Char *                        s,
    int                                     len)
{
    char *                                  tmp_s;
    struct globus_l_grim_assertion_s *      info;

    info = (struct globus_l_grim_assertion_s *) data;

    switch(info->parse_state)
    {
        case GLOBUS_L_GRIM_PARSE_USERNAME:
            tmp_s = malloc(sizeof(char) * (len + 1));
            strncpy(tmp_s, s, len);
            info->username = tmp_s;

            break;
    
        case GLOBUS_L_GRIM_PARSE_GRID_ID:
            tmp_s = malloc(sizeof(char) * (len + 1));
            strncpy(tmp_s, s, len);
            info->issuer = tmp_s;

            break;

        case GLOBUS_L_GRIM_PARSE_VERSION:
            tmp_s = malloc(sizeof(char) * (len + 1));
            strncpy(tmp_s, s, len);
            info->version = tmp_s;
            /* if the versions are not the same */
            if(strcmp(s, GRIM_ASSERTION_FORMAT_VERSION) != 0)
            {
                info->res = globus_error_put(
                  globus_error_construct_error(
                      GLOBUS_GRIM_DEVEL_MODULE,
                      GLOBUS_NULL,
                      GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                      "[globus_grim_devel]:: could not create parser."));
            }

            break;

        case GLOBUS_L_GRIM_PARSE_CLIENT_ID:
            tmp_s = malloc(sizeof(char) * (len + 1));
            strncpy(tmp_s, s, len);
            globus_list_insert(&info->dna_list, tmp_s);

            break;

        case GLOBUS_L_GRIM_PARSE_PORT_TYPE:
            tmp_s = malloc(sizeof(char) * (len + 1));
            strncpy(tmp_s, s, len);
            globus_list_insert(&info->pt_list, tmp_s);

            break;
    }
}

globus_result_t
globus_l_grim_parse_assertion(
    struct globus_l_grim_assertion_s *      info,
    char *                                  assertion)
{
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_list_t *                         list;
    int                                     ctr;
    XML_Parser                              p = NULL;

    info->dna_list = NULL;
    info->pt_list = NULL;
    info->res = GLOBUS_SUCCESS;
    
    p = XML_ParserCreate(NULL);
    if(p == NULL)
    {
        res = globus_error_put(
                  globus_error_construct_error(
                      GLOBUS_GRIM_DEVEL_MODULE,
                      GLOBUS_NULL,
                      GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                      "[globus_grim_devel]:: could not create parser."));
        goto exit;
    }

    XML_SetElementHandler(p, globus_l_grim_assertion_start, 
        globus_l_grim_assertion_end);
    XML_SetCharacterDataHandler(p, globus_l_grim_assertion_cdata);
    XML_SetUserData(p, info);
    
    if(XML_Parse(p, assertion, strlen(assertion), 0) == XML_STATUS_ERROR)
    {
        res = globus_error_put( 
                  globus_error_construct_error(
                    GLOBUS_GRIM_DEVEL_MODULE,
                    GLOBUS_NULL,
                    GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                    "[globus_grim_devel]:: xml parse failed."));
        goto exit;
    }
    if(info->res != GLOBUS_SUCCESS)
    {
        res = info->res;

        goto exit;
    }

    info->dna = (char **) globus_malloc(
                            (globus_list_size(info->dna_list) + 1)
                                 * sizeof(char *));

    ctr = globus_list_size(info->dna_list);
    info->dna[ctr] = NULL;
    ctr--;
    for(list = info->dna_list; 
        !globus_list_empty(list);
        list = globus_list_rest(list))
    {
        info->dna[ctr] = globus_list_first(list);
        ctr--;
    }
    globus_list_free(info->dna_list);

    info->port_types = (char **) globus_malloc(
                                    (globus_list_size(info->pt_list) + 1)
                                        * sizeof(char *));
    ctr = globus_list_size(info->pt_list);
    info->port_types[ctr] = NULL;
    ctr--;
    for(list = info->pt_list; 
        !globus_list_empty(list);
        list = globus_list_rest(list))
    {
        info->port_types[ctr] = globus_list_first(list);
        ctr--;
    }
    globus_list_free(info->pt_list);

  exit:

    if(p != NULL)
    {
        XML_ParserFree(p);
    }

    return res;
}


globus_result_t
globus_grim_check_authorization(
    gss_ctx_id_t                        context,
    char **                             port_types,
    char *                              username)
{
    OM_uint32                           major_status;
    OM_uint32                           minor_status;
    gss_OID_desc                        pci_OID_desc =
        {10, (void *) "\x2b\x6\x1\x4\x1\x9b\x50\x1\x81\x5e"};
    gss_OID                             pci_OID = &pci_OID_desc;
    gss_buffer_set_t                    extension_data = NULL;
    gss_buffer_desc                     name_buffer_desc;
    gss_buffer_t                        name_buffer = &name_buffer_desc;
    gss_name_t                          local_dn = GSS_C_NO_NAME;
    PROXYCERTINFO *                     pci = NULL;
    PROXYPOLICY *                       policy;
    ASN1_OBJECT *                       policy_language;
    int                                 GRIM_nid;
    globus_result_t                     result;
    unsigned char *                     policy_string = NULL;
    globus_grim_assertion_t             grim_assertion = NULL;
    char *                              buffer = NULL;
    int                                 policy_length;
    char **                             dn_array;
    char **                             grim_port_types;
    char *                              grim_username;
    int                                 i;
    int                                 j;
    int                                 initiator;
    
    major_status = gss_inquire_sec_context_by_oid(&minor_status,
                                                  context,
                                                  pci_OID,
                                                  &extension_data);

    if(GSS_ERROR(major_status))
    {
        result = globus_error_put(
            globus_error_construct_gssapi_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                major_status,
                minor_status));
        goto exit;
    }
    
    if(extension_data->count != 1)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: found more than one ProxyCertInfo "
                "extension in certificate chain\n"));
        goto exit;
    }

    pci = d2i_PROXYCERTINFO(
        NULL,
        (unsigned char **) &(extension_data->elements[0].value),
        extension_data->elements[0].length);

    if(pci == NULL)
    {
        result = globus_error_put(
            globus_error_construct_openssl_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL));
        goto exit;
    }

    policy = PROXYCERTINFO_get_policy(pci);

    if(policy == NULL)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: Unable to obtain policy from "
                "ProxyCertInfo extension\n"));
        goto exit;
    }

    policy_language = PROXYPOLICY_get_policy_language(policy);

    if(policy_language == NULL)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: Unable to determine policy language "
                "from ProxyCertInfo extension\n"));
        goto exit;
    }

    result = globus_grim_devel_get_NID(&GRIM_nid);

    if(result != GLOBUS_SUCCESS)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                globus_error_get(result),
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: Unable to obtain NID for "
                "the GRIM policy language\n"));
        goto exit;
    }

    if(OBJ_obj2nid(policy_language) != GRIM_nid)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: Not a GRIM policy\n"));
        goto exit;
    }

    policy_string = PROXYPOLICY_get_policy(policy, &policy_length);

    if(policy_string == NULL || policy_length == 0)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: Unable to obtain policy data from "
                "ProxyCertInfo extension\n"));
        goto exit;
    }

    buffer = realloc(policy_string, policy_length + 1);

    if(buffer == NULL)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                GLOBUS_GRIM_DEVEL_ERROR_ALLOC,
                "[globus_grim_devel]:: Unable to allocate memory for "
                "policy data\n"));
        goto exit;
    }

    policy_string = NULL;

    buffer[policy_length] = '\0';

    result = globus_grim_assertion_init_from_buffer(
        &grim_assertion,
        buffer);

    if(result != GLOBUS_SUCCESS)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                globus_error_get(result),
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: Unable to parse GRIM policy\n"));
        goto exit;
    }

    result = globus_grim_assertion_get_dn_array(grim_assertion,
                                                &dn_array);

    if(result != GLOBUS_SUCCESS)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                globus_error_get(result),
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: Unable to get DN array from "
                "GRIM policy\n"));
        goto exit;
    }

    major_status = gss_inquire_context(&minor_status,
                                       context,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       &initiator,
                                       NULL);

    if(GSS_ERROR(major_status))
    {
        result = globus_error_put(
            globus_error_construct_gssapi_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                major_status,
                minor_status));
        goto exit;
    }

    major_status = gss_inquire_context(&minor_status,
                                       context,
                                       initiator ? local_dn : NULL,
                                       initiator ? NULL : local_dn,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

    if(GSS_ERROR(major_status))
    {
        result = globus_error_put(
            globus_error_construct_gssapi_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                major_status,
                minor_status));
        goto exit;
    }

    major_status = gss_display_name(&minor_status,
                                    local_dn,
                                    name_buffer,
                                    NULL);
    
    if(GSS_ERROR(major_status))
    {
        result = globus_error_put(
            globus_error_construct_gssapi_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                major_status,
                minor_status));
        goto exit;
    }
    
    for(i=0; dn_array[i] != NULL && strncmp(dn_array[i],
                                            name_buffer->value,
                                            name_buffer->length); i++);

    major_status = gss_release_buffer(&minor_status,
                                      name_buffer);

    if(GSS_ERROR(major_status))
    {
        result = globus_error_put(
            globus_error_construct_gssapi_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                major_status,
                minor_status));
        goto exit;
    }

    if(dn_array[i] == NULL)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                GLOBUS_NULL,
                GLOBUS_GRIM_DEVEL_ERROR_AUTHORIZING_SUBJECT,
                "[globus_grim_devel]:: Could not find local DN in GRIM "
                "policy\n"));
        goto exit;
    }

    result = globus_grim_assertion_get_port_types_array(grim_assertion,
                                                        &grim_port_types);

    if(result != GLOBUS_SUCCESS)
    {
        result = globus_error_put( 
            globus_error_construct_error(
                GLOBUS_GRIM_DEVEL_MODULE,
                globus_error_get(result),
                GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                "[globus_grim_devel]:: Unable to get port type array from "
                "GRIM policy\n"));
        goto exit;
    }

    for(i=0; port_types[i] != NULL; i++)
    {
        for(j=0; grim_port_types[j] != NULL &&
                strcmp(port_types[i],
                       grim_port_types[j]); j++);

        if(grim_port_types[j] == NULL)
        {
            result = globus_error_put( 
                globus_error_construct_error(
                    GLOBUS_GRIM_DEVEL_MODULE,
                    GLOBUS_NULL,
                    GLOBUS_GRIM_DEVEL_ERROR_AUTHORIZING_PORT_TYPE,
                    "[globus_grim_devel]:: Could not find port type in GRIM "
                    "policy\n"));
            goto exit;
        }
    }
    
    if(username != NULL)
    {
        result = globus_grim_assertion_get_username(grim_assertion,
                                                    &grim_username);
        
        if(result != GLOBUS_SUCCESS)
        {
            result = globus_error_put( 
                globus_error_construct_error(
                    GLOBUS_GRIM_DEVEL_MODULE,
                    globus_error_get(result),
                    GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                    "[globus_grim_devel]:: Unable to get user name from "
                    "GRIM policy\n"));
            goto exit;
        }

        if(grim_username == NULL)
        {
            result = globus_error_put( 
                globus_error_construct_error(
                    GLOBUS_GRIM_DEVEL_MODULE,
                    GLOBUS_NULL,
                    GLOBUS_GRIM_DEVEL_ERROR_AUTHORIZING_USER_NAME,
                    "[globus_grim_devel]:: Could not find user name in GRIM "
                    "policy\n"));
            goto exit;
        }

        if(strcmp(grim_username, username))
        {
            result = globus_error_put( 
                globus_error_construct_error(
                    GLOBUS_GRIM_DEVEL_MODULE,
                    GLOBUS_NULL,
                    GLOBUS_GRIM_DEVEL_ERROR_AUTHORIZING_USER_NAME,
                    "[globus_grim_devel]:: Could not find user name in GRIM "
                    "policy\n"));
            goto exit;
        }
    }

 exit:

    if(extension_data)
    {
        gss_release_buffer_set(&minor_status, &extension_data);
    }

    if(pci)
    {
        PROXYCERTINFO_free(pci);
    }

    if(policy_string)
    {
        free(policy_string);
    }

    if(buffer)
    {
        free(buffer);
    }

    if(grim_assertion)
    {
        globus_grim_assertion_destroy(grim_assertion);
    }

    if(local_dn != GSS_C_NO_NAME)
    {
        gss_release_name(&minor_status, local_dn);
    }
    
    return result;
}
