#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file import_name.c
 * @author Sam Lang, Sam Meder
 * 
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

static char *rcsid = "$Id$";

#include "gssapi_openssl.h"
#include "globus_i_gsi_gss_utils.h"
#include "globus_gsi_gss_constants.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/**
 * @name Import Name
 * @ingroup globus_gsi_gssapi
 */
/* @{ */
/**
 * Accept a name as any one of four formats:
 * (0) If the OID is GSS_C_NT_HOSTBASED_SERVICE
 * Then it is assumed the name is  service@FQDN
 * We will make up a name with only /CN=service/FQDN
 * This is done to match the Kerberos service names.          
 * For example the service name of host is used for logins etc. 
 * (1) /x=y/x=y... i.e. x500 type name
 *
 * @param minor_status
 * @param input_name_buffer
 * @param input_name_type
 * @param output_name_P
 *
 * @return
 */
OM_uint32 
GSS_CALLCONV gss_import_name(
    OM_uint32 *                         minor_status,
    const gss_buffer_t                  input_name_buffer,
    const gss_OID                       input_name_type,
    gss_name_t *                        output_name_P)
{
    OM_uint32                           major_status = GSS_S_COMPLETE;
    globus_result_t                     local_result;
    gss_name_desc *                     output_name = NULL;
    X509_NAME *                         x509n = NULL;
    X509_NAME_ENTRY *                   x509_name_entry = NULL;
    int                                 length;
    char *                              name_buffer = NULL;
    char *                              index;

    static char *                       _function_name_ =
        "gss_import_name";
    GLOBUS_I_GSI_GSSAPI_DEBUG_ENTER;

    *minor_status = (OM_uint32) GLOBUS_SUCCESS;

    output_name = (gss_name_t) malloc(sizeof(gss_name_desc));
    
    if (output_name == NULL)
    {
        GLOBUS_GSI_GSSAPI_MALLOC_ERROR(minor_status);
        major_status = GSS_S_FAILURE;
        goto exit;
    } 
    
    if(g_OID_equal(input_name_type, GSS_C_NT_ANONYMOUS))
    {
        output_name->name_oid = input_name_type;
        output_name->x509n = NULL;
        *output_name_P = output_name;
        goto free_output_name;
    }
    
    x509n = X509_NAME_new();
    
    if (x509n == NULL)
    {
        GLOBUS_GSI_GSSAPI_MALLOC_ERROR(minor_status);
        major_status = GSS_S_FAILURE;
        goto free_output_name; 
    }
   
    /*
     * copy input, so it has trailing zero, and can be written over
     * during parse
     */
    length = input_name_buffer->length;

    name_buffer = (char *) malloc(length + 1);
    if (name_buffer == NULL)
    {
        GLOBUS_GSI_GSSAPI_MALLOC_ERROR(minor_status);
        major_status = GSS_S_FAILURE;
        goto free_x509_name;
    }

    memcpy(name_buffer, input_name_buffer->value, length);
    
    name_buffer[length] = '\0';

    /* 
     * take the same form Kerberos does, i.e. service@FQDN
     * and get the FQDN as a CN
     * DEE need to convert to FQDN-host for globus conventions
     * but leave off for now, as this may change.
     */
    if (g_OID_equal(GSS_C_NT_HOSTBASED_SERVICE, input_name_type))
    {
        index = strchr(name_buffer, '@');
        if (index)
        {
            /* replace with a / */
            *index = '/';   
        }

        if(!isdigit((int) *(index + 1)) &&
           !globus_libc_getenv("GSI_NO_NAME_LOOKUP"))
        {
            struct hostent              hostinfo;
            char *                      buffer;
            char *                      new_name;
            int                         name_offset;
            int                         local_errno = 0;

            buffer = malloc(8192);

            if(buffer == NULL)
            {
                GLOBUS_GSI_GSSAPI_MALLOC_ERROR(minor_status);
                major_status = GSS_S_FAILURE;
                goto free_x509_name;
            }
            
            if(globus_libc_gethostbyname_r(index + 1,
                                           &hostinfo,
                                           buffer,
                                           8192,
                                           &local_errno))
            {
                name_offset = (index - name_buffer)/sizeof(char) + 1;
                new_name = realloc(name_buffer,
                                   name_offset + 1 + strlen(hostinfo.h_name));
                if(new_name == NULL)
                {
                    GLOBUS_GSI_GSSAPI_MALLOC_ERROR(minor_status);
                    major_status = GSS_S_FAILURE;
                    goto free_x509_name;
                }

                name_buffer = new_name;

                strcpy(&name_buffer[name_offset],hostinfo.h_name);
                free(buffer);
            }
            else
            {
                free(buffer);
                errno = local_errno;
                GLOBUS_GSI_GSSAPI_ERRNO_ERROR_RESULT(
                    minor_status,
                    GLOBUS_GSI_GSSAPI_ERROR_CANONICALIZING_HOST,
                    ("Failed lookup of host %s",index + 1));
                major_status = GSS_S_FAILURE;
                goto free_x509_name;                
            }
        }
        
        x509_name_entry = X509_NAME_ENTRY_create_by_NID(
            &x509_name_entry,
            NID_commonName,
            V_ASN1_APP_CHOOSE,
            (unsigned char *) name_buffer,
            -1);
        X509_NAME_add_entry(x509n, x509_name_entry, 0, 0);
    }
    else
    {
        local_result = globus_gsi_cert_utils_get_x509_name(
            input_name_buffer->value,
            input_name_buffer->length,
            x509n);
        if(local_result != GLOBUS_SUCCESS)
        {
            GLOBUS_GSI_GSSAPI_ERROR_CHAIN_RESULT(
                minor_status, local_result,
                GLOBUS_GSI_GSSAPI_ERROR_BAD_NAME);
            major_status = GSS_S_BAD_NAME;
            goto free_x509_name;
        }
    }

    output_name->name_oid = input_name_type;
    output_name->x509n = x509n;
    *output_name_P = output_name;

    goto exit;

 free_x509_name:

    if(x509n)
    {
        X509_NAME_free(x509n);
    }
  
 free_output_name:

    if (output_name)
    {
        free(output_name);
    }

 exit:

    if (x509_name_entry)
    {
        X509_NAME_ENTRY_free(x509_name_entry);
    }
    
    if (name_buffer)
    {
        free(name_buffer);
    }

    GLOBUS_I_GSI_GSSAPI_DEBUG_EXIT;
    return major_status;
    
} 
/* gss_import_name */
/* @} */
