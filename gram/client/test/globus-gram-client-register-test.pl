#! /usr/bin/env perl
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
#
# Ping a valid and invalid gatekeeper contact.

use strict;
use Test::More;

my $test_exec = './globus-gram-client-register-test';
my $lrm = $ENV{CONTACT_LRM} if exists($ENV{CONTACT_LRM});

my $gpath = $ENV{GLOBUS_LOCATION};
my $x509_certdir_string;

if (!defined($gpath))
{
    die "GLOBUS_LOCATION needs to be set before running this script"
}
if ($ENV{CONTACT_STRING} eq "")
{
    die "CONTACT_STRING not set";
}
if (exists($ENV{X509_CERT_DIR}))
{
    $x509_certdir_string = "(X509_CERT_DIR $ENV{X509_CERT_DIR})";
}
else
{
    $x509_certdir_string = "";
}

@INC = (@INC, "$gpath/lib/perl");

my @tests;
my @todo;
my $testno = 1;

sub register_test
{
    my ($errors,$rc) = ("",0);
    my ($output);
    my ($contact, $rsl, $result, $fullarg, @lrm_skip_list) = @_;
    my $valgrind = "";
    my @args = ();
    my $oldstdout;
    my $testname = "globus_gram_client_register_test_" . $testno++;

    if (exists $ENV{VALGRIND})
    {
        push(@args, "valgrind", "--log-file=VALGRIND-$testname.log");

        if (exists $ENV{VALGRIND_OPTIONS})
        {
            push(@args, split(/\s+/, $ENV{VALGRIND_OPTIONS}));
        }
    }

    push(@args, $test_exec, $contact, $rsl);
    if (defined($fullarg) && $fullarg ne '')
    {
        push(@args, $fullarg);
    }

    SKIP: {
        skip "Skipping test for $lrm", 1
            if (defined($lrm) && grep(/$lrm/, @lrm_skip_list));

        open($oldstdout, ">&STDOUT");
        open(STDOUT, ">/dev/null");

        system(@args);
        $rc = $?>> 8;

        open(STDOUT, ">", $oldstdout);

        ok($rc == $result, $testname)
    }
}
push(@tests, "register_test('$ENV{CONTACT_STRING}', '&(executable=/bin/sleep)(arguments=1)', 0);");
push(@tests, "register_test('$ENV{CONTACT_STRING}X', '&(executable=/bin/sleep)(arguments=1)', 7);");
push(@tests, "register_test('$ENV{CONTACT_STRING}', '&(executable=/no-such-bin/sleep)(arguments=1)', 5, '', 'condor');");
# Explanation for these test cases:
# Both attempt to run the command
# grid-proxy-info -type | grep limited && globusrun -k $GLOBUS_GRAM_JOB_CONTACT
# In the 1st case, the credential is a limited proxy, so the job is canceled,
# causing the client to receive a FAILED notification.
# In the 2nd case, the credential is a full proxy, so the job is not canceled
# and the job terminates normally
push(@tests, "register_test('$ENV{CONTACT_STRING}', '&(executable=/bin/sh)(arguments = -c \"eval \"\"\$GLOBUS_LOCATION/bin/grid-proxy-info -type | grep limited && \$GLOBUS_LOCATION/bin/globusrun -k \$GLOBUS_GRAM_JOB_CONTACT; sleep 30 \"\"\")(environment = (GLOBUS_LOCATION \$(GLOBUS_LOCATION)) (PATH \"/bin:/usr/bin\")$x509_certdir_string) (library_path = \$(GLOBUS_LOCATION)/lib)', 8);");
push(@tests, "register_test('$ENV{CONTACT_STRING}', '&(executable=/bin/sh)(arguments = -c \"eval \"\"\$GLOBUS_LOCATION/bin/grid-proxy-info -type | grep limited && \$GLOBUS_LOCATION/bin/globusrun -k \$GLOBUS_GRAM_JOB_CONTACT; sleep 30\"\"\")(environment = (GLOBUS_LOCATION \$(GLOBUS_LOCATION))(PATH \"/bin:/usr/bin\") $x509_certdir_string) (library_path = \$(GLOBUS_LOCATION)/lib)', 0, '-f');");

# Now that the tests are defined, set up the Test to deal with them.
plan tests => scalar(@tests), todo => \@todo;

foreach (@tests)
{
    eval "&$_";
}
