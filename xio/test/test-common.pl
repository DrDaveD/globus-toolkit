#! /usr/bin/env perl

use strict;
use POSIX;
use Test;

# set default
my $output_dir="test_output";

sub run_test
{
    my $cmd=(shift);
    my $test_str=(shift);
    my ($errors,$rc) = ("",0);

    unlink("core");
    unlink("$output_dir/$test_str.out");
    unlink("$output_dir/$test_str.err");
    unlink("$output_dir/$test_str.dbg");
    unlink("$output_dir/$test_str.fail");

    $ENV{"INSURE_REPORT_FILE_NAME"} = "$output_dir/$test_str.insure";
    $ENV{"GLOBUS_XIO_DEBUG"} = "127,$output_dir/$test_str.dbg,1";
    $ENV{"GLOBUS_XIO_BOUNCE_DEBUG"} = "127,$output_dir/$test_str.dbg,1";
    $ENV{"GLOBUS_CALLBACK_POLLING_THREADS"} = "1";

    my $command = "$cmd > $output_dir/$test_str.out 2> $output_dir/$test_str.err";
    $rc = system($command);
    if($rc != 0)
    {
        $errors .= "\n # Tests :$command: exited with  $rc.";
    }
    if(-r 'core')
    {
        my $core_str = "$output_dir/$test_str.core";
        system("mv core $core_str");
        $errors .= "\n# Core file generated." . $errors;
        unlink("core");
    }

    if($errors eq "")
    {
        ok('success', 'success');
#        unlink("$output_dir/$test_str.out");
#        unlink("$output_dir/$test_str.err");
    }
    else
    {
        my $filename="$output_dir/$test_str.fail";
        open(FAIL, ">$filename");
        print FAIL "\n";
        print FAIL "Test :$test_str: failed with :$rc:\n";
        print FAIL "stdout :$output_dir/$test_str.out:\n";
        print FAIL "stderr :$output_dir/$test_str.err:\n";
        if(-r "$output_dir/$test_str.core")
        {
            print FAIL "core: $output_dir/$test_str.core\n";
        }
        print FAIL "cmd :$cmd\n";
        close(FAIL);

        $errors .= "\n# Test failed\n# $cmd\n# " . $errors;
        ok($errors, 'success');
    }
}
