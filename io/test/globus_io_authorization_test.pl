#!/usr/bin/env perl

=head1 Tests for the globus IO authorization modes

=cut

use strict;
use POSIX;
use Test;
use Cwd;

my @tests;
my @todo;

my $test_prog = 'globus_io_authorization_test';

my $diff = 'diff';

sub basic_func
{
   my ($errors,$rc) = ("",0);
   my $args = shift;
   my $result;
   my $expect_failure = shift;
   
   unlink('core');
   chomp($result = `$test_prog $args`);

   if($rc != 0 && !$expect_failure)
   {
      $errors .= "Test exited with $rc. ";
   }

   if(-r 'core')
   {
      ok("Core file generated.", 'ok');
   }
   else
   {
       if(!$expect_failure)
       {
           ok($result, 'ok');
       }
       else
       {
           ok($result, 'Could not accept connection');
       }
   }
}

$ENV{X509_CERT_DIR} = cwd();
$ENV{X509_USER_PROXY} = "testcred.pem";

my $identity = `grid-proxy-info -subject`;
chomp($identity);

push(@tests, "basic_func('self',0);");
push(@tests, "basic_func('identity \"$identity\"',0)");
push(@tests, "basic_func('identity \"/CN=bad DN\"',1)");
push(@tests, "basic_func('callback',0);");
push(@tests, "basic_func('-callback',0);");

# Now that the tests are defined, set up the Test to deal with them.
plan tests => scalar(@tests), todo => \@todo;

# And run them all.
foreach (@tests)
{
   eval "&$_";
}
