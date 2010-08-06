#! /usr/bin/perl

use strict;
use Test;
use File::Temp;
use Globus::Testing::Utilities;
use Globus::Core::Paths;

Globus::Testing::Utilities::testcred_setup(1) || die("Unable to set up creds");

my ($proxy_fh, $proxy_file) = mkstemp( "/tmp/proxytest.XXXXXXXX" );

$ENV{X509_USER_PROXY}=$proxy_file;

sub test_proxy
{
    my $case = shift;
    my $proxy_format = $case->[0];
    my $proxy_type = $case->[1];
    my $expect = $case->[2];
    my $result;

    system("$Globus::Core::Paths::bindir/grid-proxy-init $proxy_format $proxy_type > /dev/null 2>/dev/null");

    if ($? != 0)
    {
        print STDERR "# Error creating proxy with grid-proxy-init $proxy_format $proxy_type\n";

        ok($?, 0);
        return;
    }
    $result = `$Globus::Core::Paths::bindir/grid-proxy-info -type`;

    if ($? != 0)
    {
        print STDERR "# Error getting proxy type\n";

        ok($?, 0);
    }
    chomp($result);

    ok($result, $expect);
    truncate($proxy_fh, 0);
}

my @tests = (
    [ "", "", "RFC 3820 compliant impersonation proxy" ],
    [ "-draft", "", "Proxy draft (pre-RFC) compliant impersonation proxy" ],
    [ "-rfc", "", "RFC 3820 compliant impersonation proxy" ],
    [ "-old", "", "full legacy globus proxy" ],

    [ "", "-limited", "RFC 3820 compliant limited proxy" ],
    [ "-draft", "-limited", "Proxy draft (pre-RFC) compliant limited proxy" ],
    [ "-rfc", "-limited", "RFC 3820 compliant limited proxy" ],
    [ "-old", "-limited", "limited legacy globus proxy" ],

    [ "", "-independent", "RFC 3820 compliant independent proxy" ],
    [ "-draft", "-independent", "Proxy draft (pre-RFC) compliant independent proxy" ],
    [ "-rfc", "-independent", "RFC 3820 compliant independent proxy" ]
);

plan tests => scalar(@tests);

foreach (@tests)
{
    eval test_proxy($_);
}

END {
    unlink($proxy_file);
}
