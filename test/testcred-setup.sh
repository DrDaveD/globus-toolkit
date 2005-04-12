#!/bin/sh

. $GLOBUS_LOCATION/test/globus_test/testcred-env.sh

chmod 0600 ${X509_USER_PROXY}

SUBJECT=`grid-proxy-info -identity`;

if [ $? -ne 0 ]; then
   echo Unable to determine identity from proxy file ${X509_USER_PROXY}
   exit 1
fi

rm -f $GRIDMAP  >/dev/null 2>&1;
grid-mapfile-add-entry -dn "${SUBJECT}" -ln `whoami` -f ${GRIDMAP} \
       >/dev/null 2>&1
if [ $? -ne 0 ]; then
   echo Unable to add identity \"${SUBJECT}\" to gridmap ${GRIDMAP}
   exit 2
fi
