#! /usr/bin/env perl

use strict;
use Test::Harness;
use Cwd;
use Getopt::Long;
require 5.005;
use vars qw(@tests);

my $globus_location = $ENV{GLOBUS_LOCATION};

#$Test::Harness::verbose = 1;

unlink("test_results.txt");

@tests = qw(
            http-header-test.pl
            http-post-test.pl
            http-put-test.pl
            http-get-test.pl
            );

my $runserver;
my $server_pid;

$ENV{'xio-test-output-dir'}="test_output/$$";

my $test_dir=$ENV{'xio-test-output-dir'};

system("rm -rf $test_dir");

push(@INC, $ENV{GLOBUS_LOCATION} . "/lib/perl");

eval runtests(@tests);

$@ && print "$@";

exit 0;
