#! /usr/bin/env perl
#
# Ping a valid and invalid gatekeeper contact.

use strict;
use POSIX;
use Test;

my $test_exec = 'globus-gram-client-ping-test';

my $gpath = $ENV{GLOBUS_LOCATION};

if (!defined($gpath))
{
    die "GLOBUS_LOCATION needs to be set before running this script"
}
if ($ENV{CONTACT_STRING} eq "")
{
    die "CONTACT_STRING not set";
}

@INC = (@INC, "$gpath/lib/perl");

my @tests;
my @todo;

sub ping_test
{
    my ($errors,$rc) = ("",0);
    my ($output);
    my ($contact, $result) = @_;

    unlink('core');

    system("$test_exec '$contact' >/dev/null 2>/dev/null");
    $rc = $?>> 8;
    if($rc != $result)
    {
        $errors .= "Test exited with $rc. ";
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
        ok($errors, 'success');
    }
}
push(@tests, "ping_test('$ENV{CONTACT_STRING}', 0);");
push(@tests, "ping_test('$ENV{CONTACT_STRING}X', 7);");

# Now that the tests are defined, set up the Test to deal with them.
plan tests => scalar(@tests), todo => \@todo;

foreach (@tests)
{
    eval "&$_";
}
