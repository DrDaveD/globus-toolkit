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

#include "gaa.h"
#include "gaa_simple.h"
#include <string.h>

/** gaa_simple_assert_cred_pull()
 *
 * @ingroup gaa_simple
 *
 * Pulls "assertion" credentials (sets only the principal).  This function
 * is meant to be used as a cred_pull callback in GAA.
 *
 * @param gaa 
 *        input gaa pointer
 * @param sc
 *        input/output security context -- credentials in the sc are looked
 *        up in the credential map; if mapped entries are found, they're added
 *        to the sc.
 * @param which
 *        this argument is ignored (GAA_IDENTITY is assumed)
 * @param params
 *        input/output -- should be a (char **) pointer to a comma-separated
 *        list of users whose identity should be asserted.  This list is
 *	  nulled out once the credentials are asserted.
 *
 * @retval GAA_S_SUCCESS
 *        success
 *
 * @note
 * Assertion credentials are probably useful only for testing.
 */
gaa_status
gaa_simple_assert_cred_pull(gaa_ptr	gaa,
			   gaa_sc_ptr	sc,
			   gaa_cred_type which,
			   void *	params)
{
    char *				s;
    gaa_status				status = GAA_S_SUCCESS;
    gaa_cred *				c = 0;
    gaa_status				estat;
    char **				users = params;
    char *				s1;

    if (users == 0)
	return(GAA_S_SUCCESS);
    s = *users;
    while (s)
    {
	if (s1 = strchr(s, ','))
	    *s1++ = '\0';
	if ((status = gaa_new_cred(gaa, sc, &c, "assertion", s, GAA_IDENTITY,
				   1, &estat)) != GAA_S_SUCCESS)
	    return(status);
	if ((status = gaa_add_cred(gaa, sc, c)) != GAA_S_SUCCESS)
	    return(status);
	s = s1;
    }
    *users = '\0';
    return(status);
}

/** gaa_simple_assert_cred_eval()
 *
 * @ingroup gaa_simple
 *
 * Evaluate an assertion credential (take the raw user name and convert
 * it to a principal name with the "assertion" authority).
 * This function is meant to be used as a cred_eval
 * callback in gaa.
 *
 * @param gaa 
 *        input gaa pointer
 * @param sc
 *        this argument is ignored.
 * @param cred
 *        input/output credential (an unevaluated credential is input
 *        and will be filled in).
 * @param raw
 *        input "raw" credential -- a (char *) user name.
 * @param cred_type
 *        this argument is ignored
 * @param params
 *        this argument is ignored.
 *
 * @retval GAA_S_SUCCESS
 *         success
 */
gaa_status
gaa_simple_assert_cred_eval(gaa_ptr	gaa,
			   gaa_sc_ptr	sc,
			   gaa_cred *	cred,
			   void *	raw,
			   gaa_cred_type cred_type,
			   void *	params)
{
    gaa_status				status = GAA_S_SUCCESS;
    char *				user = raw;

    if (cred == 0)
	return(GAA_S_INVALID_ARG);
    if ((status = gaa_new_sec_attrb(&(cred->principal), GAA_IDENTITY,
				    "assertion", user)) != GAA_S_SUCCESS)
	return(status);
    if ((status =
	 gaa_new_identity_info(gaa, &(cred->info.id_info))) != GAA_S_SUCCESS)
	goto end;

 end:
    if (status == GAA_S_SUCCESS)
	cred->type = cred_type;
    else
    {
	gaa_free_sec_attrb(cred->principal);
	cred->principal = 0;
    }
    return(status);
}

/** gaa_simple_assert_cred_verify()
 *
 * @ingroup gaa_simple
 *
 * Verify an assertion credential.  Since assertion credentials are always
 * assumed to be valid, this function always returns GAA_S_SUCCESS.
 *
 * @param cred
 *        This argument is ignored.
 * @param params
 *        This argument is ignored.
 *
 * @retval GAA_S_SUCCESS
 *         success
 */
gaa_status
gaa_simple_assert_cred_verify(gaa_cred *cred, void *params)
{
    return(GAA_S_SUCCESS);
}
