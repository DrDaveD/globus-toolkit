#! /usr/bin/perl
#
# Copyright 1999-2013 University of Chicago
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

# Scenario #1:
# Multiple File Servers and a Shared Identity Provider in a Cluster

BEGIN
{
    $ENV{PERL_LWP_SSL_VERIFY_HOSTNAME} = "0";
}

END {$?=0}

use strict;
use File::Path 'rmtree';
use File::Compare 'compare';
use IPC::Open3;
use POSIX;
use LWP;
use URI::Escape;
use Test::More;

use TempUser;

require "barrier.pl";
use GlobusTransferAPIClient;

my $api = GlobusTransferAPIClient->new();

my $config_file = "multi-node-cluster-scenario.conf";

sub diagsystem(@)
{
    my @cmd = @_;
    my ($pid, $in, $out, $err);
    my ($outdata, $errdata);
    $pid = open3($in, $out, $err, @cmd);
    close($in);
    local($/);
    $outdata = <$out>;
    $errdata = <$err>;
    diag("$cmd[0] stdout: $outdata") if ($outdata);
    diag("$cmd[0] stderr: $errdata") if ($errdata);
    waitpid($pid, 0);
    return $?;
}

sub cleanup()
{
    my @cmd;
    my $rc;

    @cmd = ("globus-connect-server-cleanup", "-c", $config_file, "-d");
    $rc = diagsystem(@cmd);

    # Just to make sure that doesn't fail
    foreach my $f (</etc/gridftp.d/globus-connect*>)
    {
        unlink($f);
    }
    foreach my $f (</etc/myproxy.d/globus-connect*>)
    {
        unlink($f);
    }
    rmtree("/var/lib/globus-connect-server");
    unlink("/var/lib/myproxy-oauth/myproxy-oauth.db");
    return $rc == 0;
}

sub gcmu_setup($;%)
{
    my $endpoint = shift;
    my %args = (
        command => 'globus-connect-server-setup',
        @_
    );
    my @cmd;
    my $rc;
    
    $ENV{ENDPOINT_NAME} = $endpoint;

    # Create $endpoint
    @cmd = ($args{command}, "-c", $config_file, "-v");

    return diagsystem(@cmd)==0;
}

sub activate_endpoint($$$)
{
    my ($endpoint, $user, $pass) = @_;
    my $json;

    $json = $api->activate($endpoint, $user, $pass);

    return $json->{code} =~ '^Activated\.*' ||
        $json->{code} =~ '^AutoActivated\.*' ||
        $json->{code} =~ '^AlreadyActivated\.*';
}

sub autoactivate_endpoint($)
{
    my ($endpoint) = @_;
    my $json;

    $json = $api->autoactivate($endpoint);

    return $json->{code} =~ '^Activated\.*' ||
        $json->{code} =~ '^AutoActivated\.*' ||
        $json->{code} =~ '^AlreadyActivated\.*';
}

sub deactivate_endpoint($)
{
    my $endpoint = shift;
    my $json;

    $json = $api->deactivate($endpoint);

    return $json->{code} =~ '^Deactivated';
}

sub transfer_between_endpoints($$$$)
{
    my $json = $api->transfer(@_);
    return $json->{status} eq 'SUCCEEDED';
}

# Prepare
my $test_mode;
my $hostname;

if ($ENV{PUBLIC_HOSTNAME}) {
    $hostname = $ENV{PUBLIC_HOSTNAME};
} else {
    $hostname = (POSIX::uname())[1];
}
plan tests => 22;

set_barrier_prefix("multi-node-cluster-scenario-2-");
set_barrier_print(\&diag);

my $res = barrier(1, hostname=>$hostname);
die "Barrier error" if $res eq 'ERROR';

# Determine our rank in the list of machines from the first barrier
my $rank = rank(@{$res});
my $size = scalar(@{$res});

$ENV{ID_NODE} = $res->[0]->{hostname};
$ENV{WEB_NODE} = $res->[1]->{hostname};
$ENV{IO_NODE} = $hostname;

my ($test_user, $test_pass);
if ($rank == 0)
{
    ($test_user, $test_pass) = TempUser::create_user();
}

foreach my $method ("OAuth", "MyProxy")
{
    # To match failures with test step numbers, add 9 for MyProxy pass through
    # the tests
    my $random = int(1000000*rand());
    my $short_hostname;
    my $endpoint;

    ($short_hostname = $hostname) =~ s/\..*//;
    $endpoint = "MULTI2-$short_hostname-$random";

    $ENV{SECURITY_IDENTITY_METHOD} = $method;
    set_barrier_prefix("multi-node-cluster-scenario-2-$method-");

    # Test step #1:
    # Create ID server on node 0
    SKIP: {
        skip "ID node operations only", 1 unless ($rank == 0);

        ok(gcmu_setup($endpoint, command=>'globus-connect-server-id-setup'),
                "setup_id_$method");
    }

    # barrier to wait for id node to configure
    if ($rank == 0)
    {
        $res = barrier(2, rank=>$rank, user=>$test_user);
        die "Barrier error" if $res eq 'ERROR';
    }
    else
    {
        $res = barrier(2, rank=>$rank);
        die "Barrier error" if $res eq 'ERROR';
        if (!$test_pass)
        {
            $test_user = (map { $_->{user} } grep {$_->{rank} == 0} @{$res})[0];
            ($test_user, $test_pass) = TempUser::create_user($test_user);
        }
    }
    # Test step #2:
    # Create Web server on node 1
    SKIP: {
        skip "Web node operations only", 1 unless ($rank == 1);

        ok(gcmu_setup($endpoint, command => 'globus-connect-server-web-setup'),
            "setup_web_$method");
    }
    $res = barrier(3, rank=>$rank);
    die "Barrier error" if $res eq 'ERROR';

    # Test Step #3:
    # Set up I/O nodes everywhere
    ok(gcmu_setup($endpoint, command => "globus-connect-server-io-setup"),
            "setup_io_$method");

    $res = barrier(4, rank=>$rank);
    die "Barrier error" if $res eq 'ERROR';

    # Test Step #4:
    # Activate ID node's endpoint and then auto-activate the rest
    SKIP: {
        skip "ID node operation only", 1 unless ($rank == 0);
        ok(activate_endpoint($endpoint, $test_user, $test_pass),
                "activate_id_endpoint")
    }
    $res = barrier(5, rank=>$rank);
    die "Barrier error" if $res eq 'ERROR';

    # Test Step #5:
    # Autoactivate all other nodes
    SKIP: {
        skip "Non-ID node operation only", 1 unless $rank != 0;
        ok(autoactivate_endpoint($endpoint), "autoactivate_endpoints");
    }

    # barrier to wait for I/O nodes to configure and activate
    $res = barrier(6, rank=>$rank, endpoint=>$endpoint);
    die "Barrier error" if $res eq 'ERROR';

    my $source_endpoint = $endpoint;
    my $dest_endpoint = $res->[($rank+1) % $size]->{endpoint};

    # Test Step #6-8:
    # Transfer file between local and remote endpoints and vice versa, compare
    SKIP: {
        skip "Not enough nodes for transfer", 3 unless $size >= 2;
        my $fh;
        my ($uid, $gid, $homedir) = ((getpwnam($test_user))[2,3,7]);
        my ($infile, $outfile, $fh);
        my $random_data = '';

        $infile = "$source_endpoint.in";
        $outfile = "$source_endpoint.out";

        open($fh, ">$homedir/$infile");
        $random_data .= chr rand 255 for 1..100;
        print $fh $random_data;
        $fh->close();

        chown $uid, $gid, "$homedir/$infile";
        diag("Transferring $infile from $source_endpoint to $dest_endpoint");
        ok(transfer_between_endpoints($source_endpoint, $infile,
                $dest_endpoint, $infile),
                "transfer_between_endpoints_$method");
        diag("Transferring $infile from $dest_endpoint to $source_endpoint");
        ok(transfer_between_endpoints($dest_endpoint, $infile,
                $source_endpoint, $outfile),
                "transfer_between_endpoints_$method");
        diag("Comparing $homedir/$infile and $homedir/$outfile");
        ok(compare("$homedir/$infile", "$homedir/$outfile") == 0,
                "compare_$method");

        unlink("$homedir/$infile", "$homedir/$outfile");
    }

    # barrier to wait for transfer tests to complete before cleaning up
    $res = barrier(7, rank=>$rank);
    die "Barrier error" if $res eq 'ERROR';

    # Test Step #9:
    # Deactivate endpoints
    ok(deactivate_endpoint($endpoint), "deactivate_endpoint_$method");

    $res = barrier(8, rank=>$rank);
    die "Barrier error" if $res eq 'ERROR';

    SKIP: {
        skip "Non-ID node only", 1 if $rank == 0;
        # Test Step #10:
        # Clean up gcmu
        ok(cleanup(), "cleanup_$method");
    }

    $res = barrier(9, rank=>$rank);
    die "Barrier error" if $res eq 'ERROR';

    SKIP: {
        skip "ID node only", 1 if $rank != 0;
        # Test Step #11:
        # Clean up gcmu
        ok(cleanup(), "cleanup_$method");
    }
}

# vim: filetype=perl :
