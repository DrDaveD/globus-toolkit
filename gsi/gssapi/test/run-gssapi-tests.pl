#!/usr/bin/env perl


require 5.005;

use strict;
use Cwd;
use Test::Harness;
use vars qw(@tests);

my $globus_location = $ENV{GLOBUS_LOCATION};

system('chmod go-rw testcred.pem');

@tests = qw(gssapi-anonymous-test.pl
            gssapi-delegation-test.pl
            gssapi-limited-delegation-test.pl
            gssapi-delegation-compat-test.pl
            gssapi-group-test.pl
           );

push(@INC, $ENV{GLOBUS_LOCATION} . "/lib/perl");

runtests(@tests);
