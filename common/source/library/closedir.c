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

#ifdef GLOBUS_IMPLEMENT_CLOSEDIR

extern
int
closedir(
    DIR *                               dir)
{
#ifdef TARGET_ARCH_NETOS
    int rc;
    int save_errno = 0;

    rc = tx_semaphore_get(&dir->sem, TX_WAIT_FOREVER);

    if (rc != TX_SUCCESS)
    {
        save_errno = EBADF;

        goto out;
    }
    free(dir->entries);
    tx_semaphore_delete(&dir->sem);
    free(dir);

out:
    errno = save_errno;
    return (save_errno == 0) ? 0 : -1;
#endif /* TARGET_ARCH_NETOS */
}
#endif /* IMPLEMENT_DIR_FUNCTIONS */
