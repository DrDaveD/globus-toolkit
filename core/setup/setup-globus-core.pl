my $gpath = $ENV{GPT_LOCATION};
if (!defined($gpath))
{
  $gpath = $ENV{GLOBUS_LOCATION};

}
if (!defined($gpath))
{
   die "GPT_LOCATION or GLOBUS_LOCATION needs to be set before running this script"
}

@INC = (@INC, "$gpath/lib/perl");

require Grid::GPT::Setup;

my $metadata = new Grid::GPT::Setup(package_name => "globus_core_setup");

my $globusdir = $ENV{GLOBUS_LOCATION};
my $setupdir = "$globusdir/setup/globus/";
my $result = `$setupdir/findshelltools`;

print "creating globus-script-initializer\n";
print "creating globus-sh-tools.sh\n";

for my $setupfile ('globus-script-initializer', 'globus-sh-tools.sh')
{
    $result = system("cp $setupdir/$setupfile $globusdir/libexec");
    $result = system("chmod 0755 $globusdir/libexec/$setupfile");

}
print "creating globus-makefile-header\n";

$result = system("cp $setupdir/globus-makefile-header $globusdir/sbin");
$result = system("chmod 0755 $globusdir/sbin/globus-makefile-header");

$result = system("cp $setupdir/globus-makefile-header.gpt1 $globusdir/sbin");
$result = system("chmod 0755 $globusdir/sbin/globus-makefile-header.gpt1");

print "Done\n";

$metadata->finish();
