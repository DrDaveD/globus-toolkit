#! /usr/bin/env perl

use strict;
use Test::Harness;
use Cwd;
use Getopt::Long;
use FtpTestLib;
require 5.005;
use vars qw(@tests);

my $globus_location = $ENV{GLOBUS_LOCATION};

#$Test::Harness::verbose = 1;


@tests = qw(
            globus-ftp-client-bad-buffer-test.pl
            globus-ftp-client-caching-get-test.pl
            globus-ftp-client-caching-transfer-test.pl
            globus-ftp-client-create-destroy-test.pl
            globus-ftp-client-exist-test.pl 
            globus-ftp-client-extended-get-test.pl
            globus-ftp-client-extended-put-test.pl
            globus-ftp-client-extended-transfer-test.pl
            globus-ftp-client-get-test.pl
            globus-ftp-client-lingering-get-test.pl
            globus-ftp-client-multiple-block-get-test.pl
            globus-ftp-client-partial-get-test.pl
            globus-ftp-client-partial-put-test.pl
            globus-ftp-client-partial-transfer-test.pl
            globus-ftp-client-plugin-test.pl
            globus-ftp-client-put-test.pl
            globus-ftp-client-size-test.pl 
            globus-ftp-client-transfer-test.pl
            globus-ftp-client-user-auth-test.pl
            );

my $runserver;
my $server_pid;

GetOptions( 'runserver' => \$runserver);

if(defined($runserver))
{
    $server_pid = setup_server();
}

if(0 != system("grid-proxy-info -exists -hours 2") / 256)
{
    print "Security proxy required to run the tests.\n";
    exit 1;
}

print "Running sanity check\n";
my ($source_host, $source_file, $local_copy1) = setup_remote_source();
my ($local_copy2) = setup_local_source();
my ($dest_host, $dest_file) = setup_remote_dest();

if(0 != system("./globus-ftp-client-get-test -s gsiftp://$source_host$source_file  >/dev/null 2>&1") / 256)
{
    print "Sanity check of source (gsiftp://$source_host$source_file) failed.\n";
    kill(-9,getpgrp($server_pid));
    exit 1;
}
if(0 != system("./globus-ftp-client-put-test -d gsiftp://$dest_host$dest_file < $local_copy2 ") / 256)
{
    print "Sanity check of local source ($local_copy2) to dest (gsiftp://$dest_host$dest_file) failed.\n";
    clean_remote_file($dest_host, $dest_file);
    kill(-9,getpgrp($server_pid));
    exit 1;
}
clean_remote_file($dest_host, $dest_file);
print "Server appears sane, running tests\n";

push(@INC, $ENV{GLOBUS_LOCATION} . "/lib/perl");

eval runtests(@tests);

$@ && print "$@";

if($server_pid)
{
    kill(9,$server_pid);
    $server_pid=0;
}

exit 0;

sub setup_server()
{
    my $server_pid;
    my $server_prog = "$globus_location/sbin/in.ftpd";
    my $server_host = "localhost";
    my $server_port = 3131;
    my $server_args = "-a -s -p $server_port";
    my $subject;
    
    $ENV{X509_CERT_DIR} = cwd();
    $ENV{X509_USER_PROXY} = "testcred.pem";
    
    system('chmod go-rw testcred.pem');
     
    $subject = `grid-proxy-info -subject`;
    chomp($subject);
    
    $ENV{GRIDMAP}="gridmap";
    
    if( 0 != system("grid-mapfile-add-entry -dn \"$subject\" -ln `whoami` -f $ENV{GRIDMAP} >/dev/null 2>&1") / 256)
    {
        print "Unable to create gridmap file\n";
        exit 1;
    }
    
    $server_pid = open(SERVER, "$server_prog $server_args |");
    
    if($server_pid == -1)
    {
        print "Unable to start server\n";
        exit 1;
    }
    
    $ENV{GLOBUS_FTP_CLIENT_TEST_SUBJECT} = $subject;
    $ENV{FTP_TEST_SOURCE_HOST} = "$server_host:$server_port";
    $ENV{FTP_TEST_DEST_HOST} = "$server_host:$server_port";   
    
    return $server_pid;
}

