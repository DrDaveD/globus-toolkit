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

END {$?=0}

use strict;
use File::Path;
use IPC::Open3;
use Test::More;

use GlobusTransferAPIClient;

my $api = GlobusTransferAPIClient->new();

# Prepare
my $random = int(1000000*rand());
my $endpoint = "RESET$random";
my $server = "RESET$random";
my $config_file = "reset-endpoint.conf";

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

sub count_servers($)
{
    my $endpoint = shift;
    my $json = $api->get_endpoint($endpoint);

    return scalar(map($_->{hostname}, @{$json->{DATA}}));
}

sub cleanup
{
    my @cmd = ("globus-connect-server-cleanup", "-c", $config_file, "-d",
            "-v");
    my $rc = diagsystem(@cmd);

    # Just to make sure that doesn't fail
    foreach my $f (</etc/gridftp.d/globus-connect*>)
    {
        unlink($f);
    }
    foreach my $f (</etc/myproxy.d/globus-connect*>)
    {
        unlink($f);
    }
    File::Path::rmtree("/var/lib/globus-connect-server");
    unlink("/var/lib/myproxy-oauth/myproxy-oauth.db");
    return $rc;
}

sub gcmu_setup($$;@)
{
    my $endpoint = shift;
    my $server = shift;
    my @cmd = ("globus-connect-server-setup", "-c", $config_file, "-v", @_);
    
    $ENV{RANDOM_ENDPOINT} = $endpoint;
    $ENV{RANDOM_SERVER} = $server;

    my $rc = diagsystem(@cmd);

    return $rc;
}

plan tests => 7;

# Test Step #1:
# Create endpoint
ok(gcmu_setup($endpoint, $server) == 0, "create_endpoint");

# Test Step #2:
# Get number of servers on endpoint, assert == 1
ok(count_servers($endpoint) == 1, "count_servers1");

# Test Step #3:
# Update endpoint with new server
$server = "$server.2";
ok(gcmu_setup($endpoint, $server) == 0, "update_endpoint1");

# Test Step #4:
# Get number of servers on endpoint, assert == 2
ok(count_servers($endpoint) == 2, "count_servers2");

# Test Step #5:
# Update endpoint with -s option
ok(gcmu_setup($endpoint, $server, '-s') == 0, "update_endpoint2");

# Test Step #6:
# Get number of servers on endpoint, assert == 1
ok(count_servers($endpoint) == 1, "count_servers3");

# Test Step #7:
# Clean up gcmu
ok(cleanup() == 0, "cleanup");
# vim: filetype=perl:
