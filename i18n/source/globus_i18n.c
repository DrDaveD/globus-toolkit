/*
 * Portions of this file Copyright 1999-2005 University of Chicago
 * Portions of this file Copyright 1999-2005 The University of Southern California.
 *
 * This file or a portion of this file is licensed under the
 * terms of the Globus Toolkit Public License, found at
 * http://www.globus.org/toolkit/download/license.html.
 * If you redistribute this file, with or without
 * modifications, you must include this notice in the file.
 */

#include "globus_common.h"
#include "globus_i18n.h"
#include "globus_error_string.h"
#include "globus_extension.h"
#ifdef WIN32
#include "globus_libtool_windows.h"
#else
#include "ltdl.h"
#endif
#include "version.h"
/* For _getcwd() */
#ifdef WIN32
#include <direct.h>
#endif

static globus_thread_key_t              globus_l_libtool_key;
static globus_rmutex_t                  globus_l_libtool_mutex;
static globus_hashtable_t               globus_l_resourcebundle_loaded;


static
int
globus_l_my_module_activate(void)
{

    char                                library[1024];

    globus_hashtable_init(
            &globus_l_resourcebundle_loaded,
            32,
            globus_hashtable_string_hash,
            globus_hashtable_string_keyeq);

    globus_extension_registry_add(
        I18N_REGISTRY, "get_string_by_key", NULL, globus_get_string_by_key);


    globus_rmutex_init(&globus_l_libtool_mutex, NULL);
    globus_thread_key_create(&globus_l_libtool_key, NULL);

    return GLOBUS_SUCCESS;
}

static
int
globus_l_my_module_deactivate(void)
{
    globus_extension_registry_remove(I18N_REGISTRY, "get_string_by_key");
    
    return GLOBUS_SUCCESS;
}

GlobusExtensionDefineModule(my_module) =
{
    "my_module",
    globus_l_my_module_activate,
    globus_l_my_module_deactivate,
    NULL,
    NULL,
    NULL
};
	
char * globus_get_string_by_key( char * locale,
	         	  char * resource_name,
			  char * key)
{
    UErrorCode	uerr	=U_ZERO_ERROR;
    char * resource_path;
    static char * currdir = NULL;
    char * utf8string;
    const UChar * string;
    int32_t	len;
    UResourceBundle * myResources;
    char * it;
    char * buf;
    char hashbuf[10];
    int hash;
    char * logfile;
    FILE * logptr;

    /*create a buffer big enough for key+hash*/
    buf = (char *)globus_malloc(sizeof(char)*(strlen(key)+10));

    strcpy(buf, key);
   
    myResources = (UResourceBundle *)globus_hashtable_lookup(
                &globus_l_resourcebundle_loaded, (void *) resource_name);
    
    if (myResources==NULL)
    {
        
        #ifdef WIN32
        currdir = _getcwd(NULL, 0);
        #else
        currdir = getcwd(NULL, 0);
        #endif
        resource_path = globus_common_create_string(
	            "%s/share/i18n/%s", 
		    globus_libc_getenv("GLOBUS_LOCATION"), 
		    resource_name);

        myResources = ures_open(resource_path, locale, &uerr);

        if (U_FAILURE(uerr)) 
        {

	    /*If we fail to open the resource, just fallback to returning
	     * the string itself*/

	    return key;
        }

        globus_free(resource_path);
        
        globus_hashtable_insert(
                &globus_l_resourcebundle_loaded,
                resource_name,
                myResources);
    }

/******************************************************************************
		  		i18n 
******************************************************************************/

extern globus_extension_registry_t i18n_registry;
#define I18N_REGISTRY &i18n_registry

char *
globus_common_i18n_get_string_by_key(
    const char *                        locale,
    const char *                        resource_name,
    const char *                        key);

char *
globus_common_i18n_get_string(
    globus_module_descriptor_t *        module,
    const char *                        key);

    /*convert non-invariant characters to "_" for key*/
    it=buf; 
    while (it[0]!=0)
    {
	switch (it[0])
	{
		case '#':
		case '!':
		case '@':
		case '[':
		case ']':
		case '^':
		case '`':
		case '{':
		case '|':
		case '}':
		case '~':
		case '\n':
		case ' ':

		it[0]= '_';
		
			break;
		default:
			/*we don't need to do anything*/
			break;
	}
	    it++;
    }
   

    /*Add hash of original string--in case 2 strings differ by non-invariant
     * characters*/
    hash=globus_hashtable_string_hash(key, 35535);
    sprintf((char *)&hashbuf, "_%d", hash);
    strcat(buf, (char *)&hashbuf);

    if( (logfile=globus_libc_getenv("GLOBUS_I18N_LOG")) != GLOBUS_NULL)
    {
        logptr = fopen(logfile, "a");
	fprintf(logptr,
		"\"%s\"     {\"%s\"}\n",
		buf, 
		key);
    }

    string = ures_getStringByKey(myResources, buf, &len, &uerr);

    if (U_FAILURE(uerr)) 
    {
/*	   
	fprintf(stderr, 
		"%s: ures_getStringByKey of key\n %s \nfailed with error \"%s\"\n", 
		resource_name, 
		buf,
		u_errorName(uerr)); */
	
	return key;
    }

    
    u_strToUTF8(NULL, 0, &len, string, -1, &uerr);
    /*  length returned from u_strToUTF8 doesn't include \0 
      but it will write a \0 to the end of the buffer if it is big enough*/
    utf8string=(char *)malloc(sizeof(char)*(len+1)); 
    uerr=U_ZERO_ERROR; 
    utf8string=u_strToUTF8(utf8string, len+1, NULL, string, -1, &uerr);

    return utf8string;
}


char * globus_get_error_def(char * resource_name,
		            char * key)
{
	return globus_get_string_by_key(GLOBUS_NULL, resource_name, key);
}


