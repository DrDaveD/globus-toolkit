#! /usr/bin/env perl
#
# Test to exercise the "put" functionality of the Globus FTP client library
# in extended block mode

use strict;
use POSIX;
use Test;
use FtpTestLib;

my $test_exec = './globus-ftp-client-extended-put-test';
my $gpath = $ENV{GLOBUS_LOCATION};

if (!defined($gpath))
{
    die "GLOBUS_LOCATION needs to be set before running this script"
}

@INC = (@INC, "$gpath/lib/perl");

my @tests;
my @todo;

my ($local_copy) = setup_local_source(1);
my ($dest_host, $dest_file) = setup_remote_dest();

# Test #1-11. Basic functionality: Do a put of $test_file to
# a new unique file name on localhost, varying parallelism level.
# Compare the resulting file with the real file
# Success if program returns 0, files compare,
# and no core file is generated.
sub basic_func
{
    my ($parallelism) = (shift);
    my ($errors,$rc) = ("",0);

    unlink('core');

    my $command = "$test_exec -P $parallelism -d gsiftp://$dest_host$dest_file < $local_copy >/dev/null 2>&1";
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
        my ($output) = get_remote_file($dest_host, $dest_file);
        $errors = compare_local_files($local_copy, $output);
        unlink($output);
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
    
    clean_remote_file($dest_host, $dest_file);
}
for(my $par = 0; $par <= 10; $par++)
{
    push(@tests, "basic_func($par);");
}

# Test #12: Bad URL: Do a simple put to a bad location on the ftp server.
# Success if program returns 1 and no core file is generated.
sub bad_url
{
    my ($errors,$rc) = ("",0);

    unlink('core');

    my $command = "$test_exec -d gsiftp://$dest_host/no/such/file/here < $local_copy >/dev/null 2>&1";
    $rc = run_command($command) / 256;
    if($rc != 1)
    {
        $errors .= "\n# Test exited with $rc.";
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
push(@tests, "bad_url");

# Test #13-53: Do a simple put of $test_file to localhost, aborting
# at each possible position. Note that not all aborts may be reached.
# Success if no core file is generated for all abort points. (we could use
# a stronger measure of success here)
sub abort_test
{
    my ($errors,$rc) = ("", 0);
    my ($abort_point) = shift;

    unlink('core');

    my $command = "$test_exec -a $abort_point -d gsiftp://$dest_host$dest_file < $local_copy >/dev/null 2>&1";
    $rc = run_command($command) / 256;
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
    
    clean_remote_file($dest_host, $dest_file);
}
for(my $i = 1; $i <= 41; $i++)
{
    push(@tests, "abort_test($i);");
}

# Test #54-94. Restart functionality: Do a simple put of $test_file to
# localhost, restarting at each plugin-possible point.
# Compare the resulting file with the real file
# Success if program returns 0, files compare,
# and no core file is generated.
sub restart_test
{
    my ($errors,$rc) = ("",0);
    my ($restart_point) = shift;

    unlink('core');

    my $command = "$test_exec -r $restart_point -d gsiftp://$dest_host$dest_file < $local_copy >/dev/null 2>&1";
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
        my ($output) = get_remote_file($dest_host, $dest_file);
        $errors = compare_local_files($local_copy, $output);
        unlink($output);
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
    
    clean_remote_file($dest_host, $dest_file);
}
for(my $i = 1; $i <= 41; $i++)
{
    push(@tests, "restart_test($i);");

    if($i == 38)
    {
	push(@todo, 54 + $i);
    }
}


=head2 I<perf_test> (Test 95)

Do an extended put of $testfile, enabling perf_plugin

=back

=cut
sub perf_test
{
    my ($errors,$rc) = ("",0);

    unlink('core');

    my $command = "$test_exec -d gsiftp://$dest_host$dest_file -M < $local_copy >/dev/null 2>&1";
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
    
    clean_remote_file($dest_host, $dest_file);
}

push(@tests, "perf_test();");

=head2 I<throughput_test> (Test 96)

Do an extended put of $testfile, enabling throughput_plugin

=back

=cut
sub throughput_test
{
    my ($errors,$rc) = ("",0);

    unlink('core');

    my $command = "$test_exec -d gsiftp://$dest_host$dest_file -T < $local_copy >/dev/null 2>&1";
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
    
    clean_remote_file($dest_host, $dest_file);
}

push(@tests, "throughput_test();");


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
