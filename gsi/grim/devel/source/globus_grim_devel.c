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
#include "libxml/tree.h"
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
    endgrent();
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
globus_result_t
globus_l_grim_devel_parse_port_type_file(
    FILE *                                  fptr,
    char *                                  username,
    char **                                 groups,
    char ***                                port_types)
{
    char **                                 pt_rc;
    int                                     ctr;
    globus_byte_t *                         buffer = NULL;
    xmlChar *                               pt;
    xmlDocPtr                               doc;
    xmlNodePtr                              cur;
    struct stat                             stat_info;
    globus_bool_t                           found = GLOBUS_FALSE;
    char *                                  tmp_pt;
    char *                                  tmp_str;
    globus_result_t                         res = GLOBUS_SUCCESS;
    ssize_t                                 nbytes;
    globus_bool_t                           remove;
    globus_list_t *                         list;
    globus_list_t *                         pt_list = NULL;

    if(fstat(fileno(fptr), &stat_info) != 0)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Could't stat the file."));
    }

    /* allocate buffer big enough for the entire file */
    buffer = globus_malloc(stat_info.st_size);
    nbytes = fread(buffer, 1, stat_info.st_size, fptr);
    if(nbytes <= 0)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Failure reading from FILE *."));
    }

    doc = xmlParseMemory(buffer, nbytes);
    if(doc == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: document not successfully parsed."));
    }

    cur = xmlDocGetRootElement(doc);
    if(cur == NULL)
    {
        res =  globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Empty document."));

        goto exit;
    }

    if(xmlStrcmp(cur->name, (const xmlChar *) "authorized_port_types"))
    {
        res =  globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Wrong document type."));

        goto exit;
    }

    cur = cur->xmlChildrenNode;
    while(cur != NULL) 
    {
        
        if(cur->type == XML_ELEMENT_NODE)
        {
            if(xmlStrcmp(cur->name, (const xmlChar *)"port_type") != 0)
            {
                res = globus_error_put(
                       globus_error_construct_error(
                            GLOBUS_GRIM_DEVEL_MODULE,
                            GLOBUS_NULL,
                            GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                            "[globus_grim_devel]:: Invalid element."));

                goto exit;
            }

            pt = xmlNodeGetContent(cur);
            if(pt == NULL)
            {
                res = globus_error_put(
                       globus_error_construct_error(
                            GLOBUS_GRIM_DEVEL_MODULE,
                            GLOBUS_NULL,
                            GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                            "[globus_grim_devel]:: No port type in node."));

                goto exit;
            }

            /* find out if we want to remove the porttype */
            found = GLOBUS_TRUE;
            remove = GLOBUS_FALSE;
            tmp_str = xmlGetProp(cur, (const xmlChar *)"access");
            if(tmp_str != NULL)
            {
                if(strcmp(tmp_str, "no") == 0)
                {
                    remove = GLOBUS_TRUE;
                }
                free(tmp_str);
            }

            /* user and group are mutally exclusive to a given node */
            /* find user */
            tmp_str = xmlGetProp(cur, (const xmlChar *)"username");
            if(tmp_str != NULL)
            {
                if(strcmp(tmp_str, username) != 0)
                {
                    /* if use tag exists but is not me, not found */
                    found = GLOBUS_FALSE;
                    /* if user attr exists but not equal we do not remove */
                    remove = GLOBUS_FALSE;
                }
                free(tmp_str);
            }
            /* if use not found check group */
            else
            { 
                tmp_str = xmlGetProp(cur, (const xmlChar *)"group");
                if(tmp_str != NULL)
                {
                    found = GLOBUS_FALSE;
                    for(ctr = 0; groups[ctr] != NULL && !found; ctr++)
                    {
                        if(strcmp(groups[ctr], tmp_str) == 0)
                        {
                            found = GLOBUS_TRUE;
                        }
                    }
                    if(!found)
                    {
                        remove = GLOBUS_FALSE;
                    }
                }
            }

            /* remove will only be true if found is true */
            if(remove)
            {
                for(list = pt_list;
                    !globus_list_empty(list);
                    list = globus_list_rest(list))
                {
                    tmp_pt = (char *) globus_list_first(list);
                    if(strcmp(pt, tmp_pt) == 0)
                    {
                        globus_list_remove(&pt_list, list);
                        free(tmp_pt);
                    }
                }
            }
            /* if not remove but found add it */
            else if(found)
            {
                globus_list_insert(&pt_list, globus_libc_strdup(pt));
            }
 
            free(pt);
        }

        cur = cur->next;
    }
      
    pt_rc = (char **)
              globus_malloc((globus_list_size(pt_list) + 1) * sizeof(char *));
    ctr = 0;
    while(!globus_list_empty(pt_list))
    {
        pt = (char *) globus_list_remove(&pt_list, pt_list);
        pt_rc[ctr] = pt;
        ctr++;
    }
    pt_rc[ctr] = NULL;

    if(port_types != NULL)
    {
        *port_types = pt_rc;
    }

  exit:

    if(buffer != NULL)
    {
        globus_free(buffer);
    }

    return res;
}

/************************************************************************
 *                 xml parsing code for conf file
 ***********************************************************************/
globus_result_t
globus_l_grim_parse_conf_file(
    struct globus_l_grim_conf_info_s *      info,
    FILE *                                  fptr)
{
    globus_byte_t *                         buffer = NULL;
    xmlDocPtr                               doc;
    xmlNodePtr                              cur;
    char *                                  tmp_str;
    globus_result_t                         res = GLOBUS_SUCCESS;
    struct stat                             stat_info;
    ssize_t                                 nbytes;

    if(fstat(fileno(fptr), &stat_info) != 0)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Could't stat the file."));
    }

    /* allocate buffer big enough for the entire file */
    buffer = globus_malloc(stat_info.st_size);
    nbytes = fread(buffer, 1, stat_info.st_size, fptr);
    if(nbytes <= 0)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Failure reading from FILE *."));
    }

    doc = xmlParseMemory(buffer, nbytes);
    if(doc == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: document not successfully parsed."));
    }

    cur = xmlDocGetRootElement(doc);
    if(cur == NULL)
    {
        res =  globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Empty document."));

        goto err;
    }

    if(xmlStrcmp(cur->name, (const xmlChar *) "grim_conf"))
    {
        res =  globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Wrong document type."));

        goto err;
    }

    /* loop through all nodes and see if we want to override default values */
    cur = cur->xmlChildrenNode;
    while(cur != NULL) 
    {
        if(cur->type == XML_ELEMENT_NODE)
        {
            if(xmlStrcmp(cur->name, (const xmlChar *)"conf") != 0)
            {
                res =  globus_error_put(
                            globus_error_construct_error(
                            GLOBUS_GRIM_DEVEL_MODULE,
                            GLOBUS_NULL,
                            GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                            "[globus_grim_devel]:: Invalid element."));

                goto err;
            }

            tmp_str = xmlGetProp(cur, (const xmlChar *)"max_time");
            if(tmp_str != NULL)
            {
                info->max_time = atoi(tmp_str);
                free(tmp_str);
            }

            tmp_str = xmlGetProp(cur, (const xmlChar *)"default_time");
            if(tmp_str != NULL)
            {
                info->default_time = atoi(tmp_str);
                free(tmp_str);
            }

            tmp_str = xmlGetProp(cur, (const xmlChar *)"key_bits");
            if(tmp_str != NULL)
            {
                info->key_bits = atoi(tmp_str);
                free(tmp_str);
            }

            tmp_str = xmlGetProp(cur, (const xmlChar *)"cert_filename");
            if(tmp_str != NULL)
            {
                free(info->cert_filename);
                info->cert_filename = strdup(tmp_str);
                free(tmp_str);
            }

            tmp_str = xmlGetProp(cur, (const xmlChar *)"key_filename");
            if(tmp_str != NULL)
            {
                free(info->key_filename);
                info->key_filename = strdup(tmp_str);
                free(tmp_str);
            }

            tmp_str = xmlGetProp(cur, (const xmlChar *)"gridmap_filename");
            if(tmp_str != NULL)
            {
                free(info->gridmap_filename);
                info->gridmap_filename = strdup(tmp_str);
                free(tmp_str);
            }

            tmp_str = xmlGetProp(cur, (const xmlChar *)"port_type_filename");
            if(tmp_str != NULL)
            {
                free(info->port_type_filename);
                info->port_type_filename = strdup(tmp_str);
                free(tmp_str);
            }

            tmp_str = xmlGetProp(cur, (const xmlChar *)"log_filename");
            if(tmp_str != NULL)
            {
                info->log_filename = strdup(tmp_str);
                free(tmp_str);
            }
        }
        cur = cur->next;
    }

  err:

    if(buffer != NULL)
    {
        globus_free(buffer);
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

globus_result_t
globus_l_grim_parse_assertion(
    struct globus_l_grim_assertion_s *      info,
    char *                                  assertion)
{
    globus_result_t                         res = GLOBUS_SUCCESS;
    globus_list_t *                         list;
    int                                     ctr;
    xmlDocPtr                               doc;
    xmlNodePtr                              cur;
    char *                                  tmp_s;

    info->dna_list = NULL;
    info->pt_list = NULL;
    info->res = GLOBUS_SUCCESS;

    doc = xmlParseMemory(assertion, strlen(assertion));
    if(doc == NULL)
    {
        return globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: document not successfully parsed."));
    }

    cur = xmlDocGetRootElement(doc);
    if(cur == NULL)
    {
        res =  globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Empty document."));

        goto exit;
    }

    if(xmlStrcmp(cur->name, (const xmlChar *) "GRIMAssertion") != 0)
    {
        res =  globus_error_put(
                   globus_error_construct_error(
                       GLOBUS_GRIM_DEVEL_MODULE,
                       GLOBUS_NULL,
                       GLOBUS_GRIM_DEVEL_ERROR_EXPAT_FAILURE,
                       "[globus_grim_devel]:: Wrong document type."));

        goto exit;
    }


    cur = cur->xmlChildrenNode;
    while(cur != NULL)
    {
        if(xmlStrcmp(cur->name, (const xmlChar *)"ServiceGridId") == 0)
        {
            info->issuer = xmlNodeGetContent(cur);
        }
        else if(xmlStrcmp(cur->name, (const xmlChar *)"ServiceLocalId") == 0)
        {
            info->username = xmlNodeGetContent(cur);
        }
        else if(xmlStrcmp(cur->name, (const xmlChar *)"Version") == 0)
        {
            info->version = xmlNodeGetContent(cur);
            /* if the versions are not the same */
            if(strcmp(info->version, GRIM_ASSERTION_FORMAT_VERSION) != 0)
            {
                info->res = globus_error_put(
                  globus_error_construct_error(
                      GLOBUS_GRIM_DEVEL_MODULE,
                      GLOBUS_NULL,
                      GLOBUS_GRIM_DEVEL_ERROR_POLICY,
                      "[globus_grim_devel]:: could not create parser."));
            }

        }
        else if(xmlStrcmp(cur->name, 
                    (const xmlChar *)"AuthorizedClientId") == 0)
        {
            tmp_s = xmlNodeGetContent(cur);
            globus_list_insert(&info->dna_list, tmp_s);
        }
        else if(xmlStrcmp(cur->name, 
                    (const xmlChar *)"AuthorizedPortType") == 0)
        {
            tmp_s = xmlNodeGetContent(cur);
            globus_list_insert(&info->pt_list, tmp_s);
        }

        cur = cur->next;
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
