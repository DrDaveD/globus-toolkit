#! /usr/bin/env perl 

=head1 Simple Transfer Tests

Tests to exercise the 3rd party transfer functionality of the Globus
FTP client library.

=cut

use strict;
use POSIX;
use Test;
use FtpTestLib;

my $test_exec = './globus-ftp-client-transfer-test';
my @tests;
my @todo;

my $gpath = $ENV{GLOBUS_LOCATION};

if (!defined($gpath))
{
    die "GLOBUS_LOCATION needs to be set before running this script"
}

@INC = (@INC, "$gpath/lib/perl");

=head2 I<basic_func> (Test 1-2)

Do a transfer of /etc/group to/from localhost (with and without a valid proxy).

=over 4

=item Test 1

Transfer file without a valid proxy. Success if test program returns 1,
and no core dump is generated.

=item Test 2

Transfer file with a valid proxy. Success if test program returns 0 and files
compare.

=back

=cut

my ($source_host, $source_file, $local_copy) = setup_remote_source();
my ($dest_host, $dest_file) = setup_remote_dest();

sub basic_func
{
    my ($use_proxy) = (shift);
    my ($errors,$rc) = ("",0);
    my ($old_proxy);

    unlink('core');

    $old_proxy=$ENV{'X509_USER_PROXY'}; 
    if($use_proxy == 0)
    {
        $ENV{'X509_USER_PROXY'} = "/dev/null";
    }
    
    my $command = "$test_exec -s gsiftp://$source_host$source_file -d gsiftp://$dest_host$dest_file >/dev/null 2>&1";
    $rc = system($command) / 256;
    if(($use_proxy && $rc != 0) || (!$use_proxy && $rc == 0))
    {
        $errors .= "\n# Test exited with $rc. ";
    }
    if(-r 'core')
    {
        $errors .= "\n# Core file generated.";
    }
    if($use_proxy && $errors eq "")
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
    if((!$use_proxy) && defined($old_proxy))
    {
	$ENV{'X509_USER_PROXY'} = $old_proxy;
    }
    elsif((!$use_proxy))
    {
        delete $ENV{'X509_USER_PROXY'};
    }
    
    clean_remote_file($dest_host, $dest_file);
}
push(@tests, "basic_func" . "(0);"); #Use invalid proxy
push(@tests, "basic_func" . "(1);"); #Use proxy

=head2 I<bad_url_src> (3-4)

Do a simple transfer of a non-existent source file.

=over 4

=item Test 3

Attempt to transfer a non-existent file. Success if program returns 1
and no core file is generated.

=item Test 4

Attempt to transfer a file from an invalid source port.

=back

=cut
sub bad_url_src
{
    my ($errors,$rc) = ("",0);
    my $src = shift;

    unlink('core');

    my $command = "$test_exec -s '$src' -d gsiftp://$dest_host$dest_file >/dev/null 2>&1";
    $rc = system($command) / 256;
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
    
    clean_remote_file($dest_host, $dest_file);
}
push(@tests, "bad_url_src('gsiftp://$source_host/no-such-file-here');");
push(@tests, "bad_url_src('gsiftp://$source_host:4/no-such-file-here');");

=head2

Do a simple transfer of a non-existent source file.

=over 4

=item Test 5

Attempt to transfer a file to an unwritable location. Success if program
returns 1 and no core file is generated.

=back

=cut
sub bad_url_dest
{
    my ($errors,$rc) = ("",0);

    unlink('core');

    my $command = "$test_exec -s gsiftp://$source_host$source_file -d gsiftp://$dest_host/no-such-file-here >/dev/null 2>&1";
    $rc = system($command) / 256;
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
push(@tests, "bad_url_dest();");

=head2 I<abort_test> (Test 6-47)

Do a simple get of $test_url, aborting at each possible state abort
machine. Note that not all aborts will be reached for the "get"
operation.

Success if no core file is generated for all abort points. (we could
use a stronger measure of success here)

=cut
sub abort_test
{
    my ($errors,$rc) = ("", 0);
    my ($abort_point) = shift;

    unlink('core');

    my $command = "$test_exec -a $abort_point -s gsiftp://$source_host$source_file -d gsiftp://$dest_host$dest_file >/dev/null 2>&1";
    $rc = system($command) / 256;
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
for(my $i = 1; $i <= 42; $i++)
{
    push(@tests, "abort_test($i);");
}

=head2 I<restart_test> (Test 48-88)

Do a simple transfer of /etc/group to/from localhost, restarting at each
plugin-possible point. Compare the resulting file with the original file.
Success if program returns 0, files compare, and no core file is generated.

=cut
sub restart_test
{
    my ($errors,$rc) = ("",0);
    my ($restart_point) = shift;

    unlink('core');

    my $command = "$test_exec -r $restart_point -s gsiftp://$source_host$source_file -d gsiftp://$dest_host$dest_file >/dev/null 2>&1";
    $rc = system($command) / 256;
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
}

=head2 I<dcau_test> (Test 89-92)

Do a simple get of /etc/group to/from localhost, using each of the possible
DCAU modes, including subject authorization with a bad subject name.

=over 4

=item Test 89

DCAU with no authorization.

=item Test 90

DCAU with "self" authorization.

=item Test 91

DCAU with subject authorization for our subject name.

=item Test 92

DCAU with subject authorization with an invalid subject.

=back

=cut
sub dcau_test
{
    my ($errors,$rc) = ("",0);
    my ($dcau, $desired_rc) = @_;

    unlink('core');

    my $command = "$test_exec -c $dcau -s gsiftp://$source_host$source_file -d gsiftp://$dest_host$dest_file >/dev/null 2>&1";
    $rc = system($command) / 256;
    if($rc != $desired_rc)
    {
        $errors .= "\n# Test exited with $rc. ";
    }
    if(-r 'core')
    {
        $errors .= "\n# Core file generated.";
    }
    if($rc == 0)
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

chomp(my $subject = `grid-cert-info -subject`);
    
$subject =~ s/^ *//;

push(@tests, "dcau_test('none', 0);");
push(@tests, "dcau_test('self', 0);");
push(@tests, "dcau_test(\"'$subject'\", 0);");
push(@tests, "dcau_test(\"'/O=Grid/O=Globus/CN=bogus'\", 1);");

=head2 I<prot_test> (Test 93-95)

Do a simple transfer of /etc/group to/from localhost, with clear, safe, and
private data channel protection.

=over 4

=item Test 93

PROT with clear protection.

=item Test 94

PROT with safe protection.

=item Test 95

PROT with private protection.

=back

=cut
sub prot_test
{
    my ($errors,$rc) = ("",0);
    my ($prot, $desired_rc) = @_;

    unlink('core');

    my $command = "$test_exec -c self -t $prot -s gsiftp://$source_host$source_file -d gsiftp://$dest_host$dest_file >/dev/null 2>&1";
    $rc = system($command) / 256;
    if($rc != $desired_rc)
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

push(@tests, "prot_test('none', 0);");
push(@tests, "prot_test('safe', 0);");
push(@tests, "prot_test('private', 0);");

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
