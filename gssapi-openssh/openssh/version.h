/* $OpenBSD: version.h,v 1.64 2012/02/09 20:00:18 markus Exp $ */

#ifdef GSI
#define GSI_VERSION	" GSI"
#else
#define GSI_VERSION	""
#endif

#ifdef KRB5
#define KRB5_VERSION	" KRB5"
#else
#define KRB5_VERSION	""
#endif

#ifdef MECHGLUE
#define MGLUE_VERSION	" MECHGLUE"
#else
#define MGLUE_VERSION	""
#endif

#define NCSA_VERSION	" GSI_GSSAPI_20120527"

#define SSH_VERSION	"OpenSSH_6.0"

#define SSH_PORTABLE	"p1"
#define SSH_HPN         "-hpn13v11"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE SSH_HPN \
            NCSA_VERSION GSI_VERSION KRB5_VERSION MGLUE_VERSION
