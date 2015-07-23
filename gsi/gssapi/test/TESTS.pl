#!/usr/bin/perl
#
# Copyright 1999-2010 University of Chicago
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


my ($prefix, $exec_prefix, $libdir, $perlmoduledir);
my ($sbindir, $bindir, $includedir, $datarootdir,
    $datadir, $libexecdir, $sysconfdir, $sharedstatedir,
    $localstatedir, $aclocaldir);
BEGIN
{
    if (exists $ENV{GLOBUS_LOCATION})
    {
        $prefix = $ENV{GLOBUS_LOCATION};
    }
    else
    {
        $prefix = "/media/psf/Home/development/globus-toolkit.gt52/INSTALL";
    }

    $exec_prefix = "${prefix}";
    $libdir = "${exec_prefix}/lib";
    $sbindir = "${exec_prefix}/sbin";
    $bindir = "${exec_prefix}/bin";
    $includedir = "${prefix}/include/globus";
    $datarootdir = "${prefix}/share";
    $datadir = "${prefix}/share";
    $perlmoduledir = "${prefix}/lib/perl";
    $libexecdir = "${datadir}/globus";
    $sysconfdir = "${prefix}/etc";
    $sharedstatedir = "${prefix}/com";
    $localstatedir = "${prefix}/var";
    $aclocaldir = "${datadir}/globus/aclocal";

    if (exists $ENV{GPT_LOCATION})
    {
        unshift(@INC, "$ENV{GPT_LOCATION}/lib/perl");
    }

    unshift(@INC, "${perlmoduledir}");
}

require 5.005;

use warnings;
use strict;
use Test::Harness;
use vars qw(@tests);
use Globus::Testing::Utilities;

Globus::Testing::Utilities::testcred_setup
    || die "Unable to set up test credentials\n";

@tests = qw(gssapi-anonymous-test.pl
            compare-name-test.pl
            compare-name-test-rfc2818.pl
            compare-name-test-gt2.pl
            duplicate-name-test.pl
            indicate-mechs-test.pl
            inquire-names-for-mech-test.pl
            gssapi-delegation-test.pl
            gssapi-limited-delegation-test.pl
            gssapi-delegation-compat-test.pl
            gssapi-acquire-test.pl
            gssapi-context-test.pl gssapi-expimp-test.pl gssapi-inquire-sec-ctx-by-oid-test.pl
            gssapi-import-name.pl
            nonterminated-export-cred-test.pl
            release-name-test.pl
           );

runtests(@tests);
