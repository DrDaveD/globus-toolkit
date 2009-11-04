use Getopt::Long;
use IO::File;

my $gpath = $ENV{GPT_LOCATION};

if (!defined($gpath))
{
    $gpath = $ENV{GLOBUS_LOCATION};
}

if (!defined($gpath))
{
    die "GPT_LOCATION or GLOBUS_LOCATION needs to be set before running this script";
}

@INC = (@INC, "$gpath/lib/perl");

require Grid::GPT::Setup;

my $metadata =
    new Grid::GPT::Setup(package_name => "globus_gram_job_manager_setup");

my $globusdir	= $ENV{GLOBUS_LOCATION};
my $setupdir	= "$globusdir/setup/globus";
my $sysconfdir	= "$globusdir/etc";
my $libexecdir	= "$globusdir/libexec";
my $bindir	= "$globusdir/bin";
my $sbindir	= "$globusdir/sbin";
my $state_dir   = "$globusdir/tmp/gram_job_state";
my $help	= 0;
my $auditing_dir = '';   

GetOptions('state-dir|s=s' => \$state_dir,
           'auditing-dir|a=s' => \$auditing_dir,
           'help|h' => \$help);

&usage if($help);

&setup_state_dir();
if ($auditing_dir ne '')
{
    &setup_audit_dir();
}
&setup_job_manager_conf();
print "Done\n";

$metadata->finish();

sub setup_state_dir
{
    my $last_built_path = '';
    my $built_path = '';
    my @components;

    print "Creating state file directory.\n";

    if( $state_dir !~ m|^/|)
    {
	print STDERR "Invalid directory for state files \"$state_dir\"\n";
	exit(1);
    }

    @components = split(/\//, $state_dir);

    foreach(@components)
    {
	next if $_ eq '';

	$last_built_path = $built_path;
	$built_path .= "/$_";

	if(-e $built_path && ! -d $built_path)
	{
	    print STDERR "Invalid path for state files: " .
	                 "$built_path is not a directory\n";
	    exit(1);
	}
	elsif(! -e $built_path)
	{
	    my $fs_type;

	    mkdir($built_path, 0755) ||
                die "Unable to create directory $built_path\n";
	}
    }
    
    if((stat($state_dir))[2] != 01777)
    {
        chmod(01777, $state_dir) || die "Can't set permissions on $state_dir\n";
    }
    print "Done.\n";

    print "Checking if state directory supports POSIX file locking... ";
    $rc = system("$ENV{GLOBUS_LOCATION}/libexec/globus-job-manager-lock-test $state_dir/lock_test");
    if ($rc)
    {
        die "no\n";
    }
    print "yes\n";

}

sub setup_audit_dir
{
    my $last_built_path = '';
    my $built_path = '';
    my @components;

    print "Creating auditing file directory.\n";

    if( $auditing_dir !~ m|^/|)
    {
	print STDERR "Invalid directory for audit files \"$auditing_dir\"\n";
	exit(1);
    }

    @components = split(/\//, $auditing_dir);

    foreach(@components)
    {
	next if $_ eq '';

	$last_built_path = $built_path;
	$built_path .= "/$_";

	if(-e $built_path && ! -d $built_path)
	{
	    print STDERR "Invalid path for state files: " .
	                 "$built_path is not a directory\n";
	    exit(1);
	}
	elsif(! -e $built_path)
	{
	    mkdir($built_path, 0777) ||
                die "Unable to create directory $built_path\n";
	    chmod(0755, $built_path) ||
	        die "Can't set permissions on $built_path\n";
	}
    }
    
    # Desired permissions: drwx-wx-wt 
    if((stat($auditing_dir))[2] != 05733)
    {
        chmod(05733, $auditing_dir) || die "Can't set permissions on $auditing_dir\n";
    }

    print "Done.\n";
}

sub setup_job_manager_conf
{
    my ($gatekeeper_port, $gatekeeper_subject);
    my ($hostname, $cpu, $manufacturer, $os_name, $os_version);
    my ($toolkit_version);
    my $jm_conf	= "${sysconfdir}/globus-job-manager.conf";
    my $conf_file;
    my $toolkit_version = `${globusdir}/bin/globus-version` || "unknown";

    chomp($toolkit_version);


    ($gatekeeper_subject, $gatekeeper_port) =
	&get_gatekeeper_info("${sysconfdir}/globus-gatekeeper.conf");

    ($hostname, $cpu, $manufacturer, $os_name, $os_version) = &get_system_info();

    print "Creating job manager configuration file...\n";
    $conf_file = new IO::File(">$jm_conf") || die "open failed for $jm_conf";

    print $conf_file <<EOF;
	-home \"$globusdir\"
	-globus-gatekeeper-host $hostname
	-globus-gatekeeper-port $gatekeeper_port
	-globus-gatekeeper-subject \"$gatekeeper_subject\"
	-globus-host-cputype $cpu
	-globus-host-manufacturer $manufacturer
	-globus-host-osname $os_name
	-globus-host-osversion $os_version
        -globus-toolkit-version $toolkit_version
	-stdio-log \"\$(HOME)\"
        -log-levels 'FATAL|ERROR'
	-state-file-dir $state_dir
	-machine-type unknown
EOF
    if ($auditing_dir ne '')
    {
        print $conf_file "-audit-directory $auditing_dir\n";
    }
    $conf_file->close();
}

sub get_gatekeeper_info
{
    my ($gatekeeper_conf_filename) = $_[0];
    my ($host_cert_line, $host_cert_file, $subject, $port) = ();

    print "Reading gatekeeper configuration file...\n";
    if ( ! -f "$gatekeeper_conf_filename" )
    {
       die "File \"$gatekeeper_conf_filename\" not found.\n";
    }

    chomp($host_cert_line = `grep x509_user_cert $gatekeeper_conf_filename`);
    $host_cert_file = (split(/x509_user_cert/, $host_cert_line))[1];
    $host_cert_file =~ s/^\s+//; #strip leading whitespace

    if ( ! -r "$host_cert_file" )
    {
	print STDERR <<EOF;
Warning: Host cert file: $host_cert_file not found.  Re-run
         setup-globus-gram-job-manager after installing host cert file.
EOF
       $subject="unavailable at time of install";
    }
    else
    {
       chomp($subject =
             `${bindir}/grid-cert-info -subject -file $host_cert_file`);
       if ( $? != 0 )
       {
	  die "Failed getting subject from host certificate: $host_cert_file.";
       }
       else
       {
	  $subject =~ s/^\s+//; #strip leading whitespace
       }
    }

    my $port = 0;
    if ( open(GK_CONF, $gatekeeper_conf_filename) )
    {
      $port =(m/^(\s*)-port\s+([0-9]+)/)[1] while(! $port && ($_=<GK_CONF>));
      close GK_CONF;
    }

    return ($subject, $port);
}

sub get_system_info
{
    my ($hostname, $cpu, $manufacturer, $os_name, $os_version);

    print "Determining system information...\n";
    chomp($hostname = `${bindir}/globus-hostname`);
    ($cpu, $manufacturer) = (split(/-/, `${sbindir}/config.guess`))[0,1];
    $uname_cmd = &lookup_shell_command("GLOBUS_SH_UNAME");

    chomp($os_name=`$uname_cmd -s`);
    $os_version="";

    if($os_name eq "AIX")
    {
       chomp($os_version = `$uname_cmd -v`);
       $os_version .= ".";
    }

    chomp($os_version .= `$uname_cmd -r`);

    return ($hostname, $cpu, $manufacturer, $os_name, $os_version);
}

sub lookup_shell_command
{
    my ($cmdvar, $cmd);

    $cmdvar = $_[0];

    chomp($cmd = `$bindir/globus-sh-exec -e echo \\\$$cmdvar`);

    return $cmd;
}

sub usage
{
    print "Usage: $0 [options]\n".
    "Options:  [--state-dir|-s DIR]\n".
    "          [--auditing-dir|-a DIR]\n".
    "          [--help|-h]\n";
    exit 1;
}

