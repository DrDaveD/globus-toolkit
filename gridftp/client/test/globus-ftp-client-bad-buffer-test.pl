#! /usr/bin/env perl
#
# Try reading an url by passing in a bad data buffer

use strict;
use POSIX;
use Test;
use FtpTestLib;

my $test_exec = './globus-ftp-client-bad-buffer-test';

my $gpath = $ENV{GLOBUS_LOCATION};

if (!defined($gpath))
{
    die "GLOBUS_LOCATION needs to be set before running this script"
}

@INC = (@INC, "$gpath/lib/perl");

my @tests;
my @todo;

my ($source_host, $source_file, $local_copy) = setup_remote_source();

# Test 1: Bad buffer test
# Success if the transfer fails with exit code 2
sub bad_buffer
{
    my ($errors,$rc) = ("",0);
    my ($output);

    unlink('core');
    
    my $command = "$test_exec -s gsiftp://$source_host$source_file >/dev/null 2>&1";
    $rc = run_command($command) / 256;
    if($rc != 2)
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
push(@tests, "bad_buffer");

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
    plan tests => scalar(@tests), todo => \@todo;

    foreach (@tests)
    {
        eval "&$_";
    }
}
