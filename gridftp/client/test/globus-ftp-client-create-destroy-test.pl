#! /usr/bin/env perl
#
# Extremely basic data structure creation/destroy test. No transfers
# are done here. Sanity check activation/deactivation code, and make
# sure handle con/destructors work.

use strict;
use POSIX;
use Test;
use FtpTestLib;

my $test_exec = './globus-ftp-client-create-destroy-test';
my @tests;

my $gpath = $ENV{GLOBUS_LOCATION};

if (!defined($gpath))
{
    die "GLOBUS_LOCATION needs to be set before running this script"
}

@INC = (@INC, "$gpath/lib/perl");

sub create_destroy
{
    my ($errors,$rc) = ("",0);

    unlink('core');
    
    my $command = "$test_exec >/dev/null 2>&1";
    $rc = run_command($command) / 256;
    if($rc != 0)
    {
        $errors .= "\n# Test exited with $rc. ";
    }
    if(-r 'core')
    {
        $errors .= "\n# Core file generated.";
    }

    if($errors eq "")
    {
        ok('success', 'success');
    }
    else
    {
        $errors = "\n# Test failed\n# $command\n# " . $errors;
        ok($errors, 'success');
    }
}
push(@tests, "create_destroy");

if(@ARGV)
{
    plan tests => scalar(@ARGV);

    foreach (@ARGV)
    {
        eval "&$tests[$_-1]";
    }
}
else
{
    plan tests => scalar(@tests);

    foreach (@tests)
    {
        eval "&$_";
    }
}
