#!/usr/bin/perl

# 
# Copyright 1999-2006 University of Chicago
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


use strict;
use TAP::Harness::JUnit;
use Globus::Core::Paths;

require 5.005;
use vars qw(@tests);

my @tests = qw(
    globus-gram-protocol-allow-attach-test.pl
    globus-gram-protocol-error-test.pl
    globus-gram-protocol-io-test.pl
    globus-gram-protocol-pack-test.pl
    pack-with-extensions-test.pl
    create-extensions-test.pl
    unpack-message-test.pl
    unpack-with-extensions-test.pl
    unpack-job-request-reply-with-extensions-test.pl
    unpack-status-reply-with-extensions-test.pl
);

my $harness = TAP::Harness::JUnit->new({
                        xmlfile => 'globus-gram-protocol-test.xml',
                        merge => 1 });

if(0 != system("$Globus::Core::Paths::bindir/grid-proxy-info -exists -hours 2 2>/dev/null") / 255)
{
    print STDERR "Unable to run tests: No proxy\n";
    exit 1
}

$harness->runtests(@tests);
