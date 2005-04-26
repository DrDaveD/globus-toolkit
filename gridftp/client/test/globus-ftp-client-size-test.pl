#! /usr/bin/env perl 

#
# Portions of this file Copyright 1999-2005 University of Chicago
# Portions of this file Copyright 1999-2005 The University of Southern California.
#
# This file or a portion of this file is licensed under the
# terms of the Globus Toolkit Public License, found at
# http://www.globus.org/toolkit/download/license.html.
# If you redistribute this file, with or without
# modifications, you must include this notice in the file.
#


=head1 globus-ftp-client-size-test

Tests to exercise the size checking of the client library.

=cut

use strict;
use POSIX;
use Test;
use FtpTestLib;

my $test_exec = './globus-ftp-client-size-test';
my @tests;

my $gpath = $ENV{GLOBUS_LOCATION};

if (!defined($gpath))
{
    die "GLOBUS_LOCATION needs to be set before running this script"
}

@INC = (@INC, "$gpath/lib/perl");

my ($proto) = setup_proto();
my ($source_host, $source_file, $local_copy) = setup_remote_source();

sub check_size
{
    my ($errors,$rc) = ("",0);
    my ($old_proxy);
    my $src_url = shift;
    my $size = shift;
    my $checked_size;

    unlink('core');
    
    my $command = "$test_exec -s $src_url 2>/dev/null";
    $checked_size = `$command`;
    chomp($checked_size);
    $rc = $?;
    if($rc / 256 != 0 && $size >= 0)
    {
        $errors .= "\n# Test exited with " . $rc / 256;
    }
    if($size != -1 && $checked_size != $size)
    {
	$errors .= "\n# Size mismatch.";
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

if(source_is_remote())
{
    print "using remote source, skipping check_size()\n";
}
else
{
    my @file_sizes;
    if(!defined($ENV{'FTP_TEST_BACKEND'}))
    {
        push(@file_sizes, '/etc/group');
        push(@file_sizes, '/bin/sh');
    }
    else
    {
        if(defined($ENV{'FTP_TEST_SOURCE_BIGFILE'}))
        {
            push(@file_sizes, $ENV{'FTP_TEST_SOURCE_BIGFILE'});
        }
        if(defined($ENV{'FTP_TEST_SOURCE_FILE'}))
        {
            push(@file_sizes, $ENV{'FTP_TEST_SOURCE_FILE'});
        }
        if(defined($ENV{'FTP_TEST_LOCAL_BIGFILE'}))
        {
            push(@file_sizes, $ENV{'FTP_TEST_LOCAL_BIGFILE'});
        }
    }
    push(@file_sizes, '/adsfadsfa');
    
    foreach(@file_sizes)
    {
        my $size = (stat($_))[7];
        if(!defined($size))
        {
    	    $size = -1;
        }

        push(@tests, "check_size('$proto$source_host$_', $size);");
    }

if(defined($ENV{FTP_TEST_RANDOMIZE}))
{
    shuffle(\@tests);
}

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

}
