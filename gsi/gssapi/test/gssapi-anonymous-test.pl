#!/usr/bin/env perl

=pod

=head1 Tests for the globus common error object code

Tests to exercise the error object functionality of the globus
common library.

=cut

use strict;
use POSIX;
use Test;
use Cwd;

my $test_prog = 'gssapi-anonymous-test';

my $diff = 'diff';
my @tests;
my @todo;

sub basic_func
{
   my ($errors,$rc) = ("",0);
   
   $ENV{X509_CERT_DIR} = cwd();
   $ENV{X509_USER_PROXY} = "testcred.pem";

   $rc = system("$test_prog 1>$test_prog.log.stdout 2>$test_prog.log.stderr") / 256;

   if($rc != 0)
   {
      $errors .= "Test exited with $rc. ";
   }

   if(-r 'core')
   {
      $errors .= "\n# Core file generated.";
   }
   
   $rc = system("$diff $test_prog.log.stdout $test_prog.stdout") / 256;
   
   if($rc != 0)
   {
      $errors .= "Test produced unexpected output, see $test_prog.log.stdout";
   }


   $rc = system("$diff $test_prog.log.stderr $test_prog.stderr") / 256;
   
   if($rc != 0)
   {
      $errors .= "Test produced unexpected output, see $test_prog.log.stderr";
   }
   
   if($errors eq "")
   {
      ok('success', 'success');
      
      if( -e "$test_prog.log.stdout" )
      {
	 unlink("$test_prog.log.stdout");
      }
      
      if( -e "$test_prog.log.stderr" )
      {
	 unlink("$test_prog.log.stderr");
      } 
   }
   else
   {
      ok($errors, 'success');
   }

}

sub sig_handler
{
   if( -e "$test_prog.log.stdout" )
   {
      unlink("$test_prog.log.stdout");
   }

   if( -e "$test_prog.log.stderr" )
   {
      unlink("$test_prog.log.stderr");
   }
}

$SIG{'INT'}  = 'sig_handler';
$SIG{'QUIT'} = 'sig_handler';
$SIG{'KILL'} = 'sig_handler';


push(@tests, "basic_func();");

# Now that the tests are defined, set up the Test to deal with them.
plan tests => scalar(@tests), todo => \@todo;

# And run them all.
foreach (@tests)
{
   eval "&$_";
}
