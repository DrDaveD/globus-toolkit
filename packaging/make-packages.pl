#!/usr/bin/env perl

# GT3.x build tool

# Basic strategy:
#  Iterate over etc/{cvstree}/package-list and /bundles
#  to get a list of packages and bundles defined by that tree.
#  Then, package up all the sources corresponding to the packages.
#  After that, make source bundles out of the packages.
#  Finally, install the resulting bundles.

use strict;
use Getopt::Long;
use Config;
use Cwd;
use Pod::Usage;

# Where do things go?
my $top_dir = cwd();
my $cvs_prefix = $top_dir . "/source-trees/";
my $log_dir = $top_dir . "/log-output";
my $pkglog = $log_dir . "/package-logs";
my $bundlelog = $log_dir . "/bundle-logs";
my $source_output = $top_dir . "/source-output";
my $package_output = $top_dir . "/package-output";
my $bin_output = $top_dir . "/bin-pkg-output";
my $bundle_output = $top_dir . "/bundle-output";
my $bin_bundle_output = $top_dir . "/bin-bundle-output";

# What do I need to clean up from old buids?
my @cleanup_dirs = ('log-output', '$bundle_ouput/BUILD');

# tree_name => [ cvs directory, module, checkout-dir tag ]
# TODO: Make prereq builds separate?
my %prereq_archives = (
     'autotools' => [ "/home/globdev/CVS/globus-packages", "side_tools", "autotools", "HEAD" ],
		       );

# tree_name => [ cvs directory, module, checkout-dir tag ]
# TODO: Add explicit CVSROOT
# TODO: Allow per-package module specification
my %cvs_archives = (
     'gt2' => [ "/home/globdev/CVS/globus-packages", "gp", $cvs_prefix . "gt2-cvs", "HEAD" ],
     'gt3' => [ "/home/globdev/CVS/globus-packages", "gs", $cvs_prefix . "ogsa-cvs", "HEAD" ],
     'gt4' => [ "/home/globdev/CVS/globus-packages", "ws", $cvs_prefix . "wsrf-cvs", "HEAD" ],
     'cbindings' => [ "/home/globdev/CVS/globus-packages", "ogsa-c", $cvs_prefix . "cbindings", "HEAD" ],
     'autotools' => [ "/home/globdev/CVS/globus-packages", "side_tools", $cvs_prefix . "autotools", "HEAD" ]
      );

# package_name => [ tree, subdir, custom_build, (patch-n-build file, if exists) ]
my %package_list;

# bundle_name => [ flavor, flags, @package_array ]
my %bundle_list;

# Which of the bundles defined should I build?
my @bundle_build_list;
my %package_build_hash;

# Which of the CVS trees should I operate on?
my @cvs_build_list;
my %cvs_build_hash;

# What flavor shall things be built as?
my $flavor = "gcc32dbg";
my $thread = "pthr";

my ($install, $installer, $anonymous, $force,
    $noupdates, $help, $man, $verbose, $skippackage,
    $skipbundle, $faster, $paranoia, $version, $uncool,
    $binary, $inplace, $gt2dir, $gt3dir, $doxygen,
    $autotools, $deps, $graph, $listpack, $listbun,
    $cvsuser ) =
   (0, 0, 0, 0,
    0, 0, 0, 0, 0, 
    0, 0, 1, "1.0", 0, 
    0, 0, "", "", 0,
    1, 0, 0, 0, 0,
    "");

my @user_bundles;
my @user_packages;

GetOptions( 'i|install=s' => \$install,
	    'installer=s' => \$installer,
	    'a|anonymous!' => \$anonymous,
	    'force' => \$force,
	    'n|no-updates!' => \$noupdates,
	    'faster!' => \$faster,
	    'flavor=s' => \$flavor,
	    'd2|gt2-dir=s' => \$gt2dir,
	    't2|gt2-tag=s' => \$cvs_archives{gt2}[3],
	    'd3|gt3-dir=s' => \$gt3dir,
	    't3|gt3-tag=s' => \$cvs_archives{gt3}[3],
	    'v|verbose!' => \$verbose,
	    'skippackage!' => \$skippackage,
	    'skipbundle!' => \$skipbundle,
	    'binary!' => \$binary,
	    'bundles=s' => \@user_bundles,
	    'p|packages=s' => \@user_packages,
	    'trees=s' => \@cvs_build_list,
	    'paranoia!' => \$paranoia,
	    'version=s' => \$version,
	    'uncool!' => \$uncool,
	    'inplace!' => \$inplace,
	    'doxygen!' => \$doxygen,
	    'autotools!' => \$autotools,
	    'd|deps!' => \$deps,
	    'graph!' => \$graph,
	    'lp|list-packages!' => \$listpack,
	    'lb|list-bundles!' => \$listbun,
	    'cvs-user=s' => \$cvsuser,
	    'help|?' => \$help,
	    'man' => \$man,
) or pod2usage(2);

if ( $help or $man ) {
    pod2usage(2) if $help;
    pod2usage(1) if $man;
}

# Allow comma separated packages or multiple instances.
@user_packages = split(/,/,join(',',@user_packages));
@user_bundles = split(/,/,join(',',@user_bundles));
@cvs_build_list = split(/,/,join(',',@cvs_build_list));

if ( $gt2dir )
{
    $cvs_archives{gt2}[2] = $gt2dir;
    $cvs_archives{autotools}[2] = $gt2dir;
}

if ( $gt3dir )
{
    $cvs_archives{gt3}[2] = $gt3dir;
    $cvs_archives{cbindings}[2] = $gt3dir;
}


# main ()

cleanup();
mkdir $log_dir;
setup_environment();
generate_dependency_tree();

exit if ( $listpack or $listbun );

if ( not $noupdates )
{
    # Need autotools for gt2 or gt3
    if ($cvs_build_hash{'gt2'} eq 1 or
	$cvs_build_hash{'gt3'} eq 1 or
	$cvs_build_hash{'gt4'} eq 1 or
	$cvs_build_hash{'cbindings'} eq 1)
    {
	if ( $cvs_build_hash{'autotools'} ne 1)
        {
	    $cvs_build_hash{'autotools'} = 1;
	    push @cvs_build_list, 'autotools';
	}
    }
    if ( $deps && $cvs_build_hash{'autotools'} )
    {
        cvs_checkout_generic("autotools");
    } else {
	get_sources();
    }
} else {
    print "Not checking out sources with -no-updates set.\n";
    print "INFO: This means CVS Tags are not being checked either.\n";
}

build_prerequisites();

if ( not $skippackage )
{
    package_sources();
} else {
    print "Not packaging sources with -skippackage set.\n";
}

if ( $inplace )
{
    print "Exiting after installation for inplace builds.\n";
    exit;
}

if ( not $skipbundle )
{
    bundle_sources();
} else {
    print "Not bundling sources with -skipbundle set.\n";
}

if ( $install )
{
    install_bundles();
    install_packages();
} else {
    print "Not installing bundle because -install= not set.\n";
}

if ( $installer )
{
    create_installer();
} else {
    print "Not creating installer because --installer= not set.\n";
}

if ( $binary )
{
    generate_bin_packages();
} else {
    print "Not generating binary packages because -binary not set.\n";
}

exit 0;

# --------------------------------------------------------------------
sub generate_dependency_tree()
# --------------------------------------------------------------------
{
    print "Generating package build list ...\n";
    $cvs_archives{'autotools'}[3] = $cvs_archives{'gt2'}[3];
    $cvs_archives{'cbindings'}[3] = $cvs_archives{'gt3'}[3];

    if ( not defined(@cvs_build_list) )
    {
	@cvs_build_list = ("autotools", "gt2", "gt3", "gt4", "cbindings");
    }

    foreach my $tree (@cvs_build_list)
    {
	$cvs_build_hash{$tree} = 1;
    }

    # Figure out what bundles and packages exist.
    for my $tree (@cvs_build_list)
    {
	populate_bundle_list($tree);
	populate_package_list($tree);
    }

    # Out of what exists, what shall we build?
    populate_bundle_build_list();
    populate_package_build_hash();

    # Do we need to pull in more packages?
    if ( $deps )
    {
	install_gpt();
	if ( $graph )
	{
	    open(GRAPH, ">$top_dir/dotty.out");
	    print GRAPH "digraph G {\n";
        }
	import_package_dependencies(%package_build_hash);
	if ( $graph )
	{
	    print GRAPH "}";
	    close GRAPH;
        }
	
	# To interact well with installs, need to make
	# a new bundle that contains everything that was
	# pulled in via --deps, so that GPT may sort them
	# for us.  Otherwise we install in the wrong order.
        push @{$bundle_list{"custom-deps"}}, $flavor;
        push @{$bundle_list{"custom-deps"}}, "";  # No flags
        for my $pk (keys %package_build_hash)
        {
           push @{$bundle_list{"custom-deps"}}, $pk;
        }

	@bundle_build_list = ( "custom-deps" );
    }
}

# --------------------------------------------------------------------
sub import_package_dependencies
# --------------------------------------------------------------------
{
    my (%package_hash) = @_;
    my %new_hash;

    for my $pack ( keys %package_hash )
    {
	cvs_checkout_package($pack) unless $noupdates;

	# For a patch-n-build, also need to get patch tarball
	if ( $package_list{$pack}[2] eq "pnb" )
	{
	    print "PNB detected for $pack.\n";
	    my $cvs_dir = $package_list{$pack}[0];
	    my $tarfile = $package_list{$pack}[3];
	    if ( ! -e "$cvs_dir/tarfiles/$tarfile" )
	    {
		print "checking out $cvs_dir/tarfiles/$tarfile\n";
		cvs_checkout_subdir( $cvs_dir, "tarfiles/$tarfile") unless $noupdates;
	    }
	}

	my $metadatafile = package_subdir($pack) . "/pkgdata/pkg_data_src.gpt.in";
	if ( ! -e $metadatafile )
	{
	    $metadatafile = package_subdir($pack) . "/pkg_data_src.gpt";
	}
	if ( ! -e $metadatafile )
	{
	    $metadatafile = package_subdir($pack) . "/pkgdata/pkg_data_src.gpt";
	}

	require Grid::GPT::V1::Package;
	my $pkg = new Grid::GPT::V1::Package;
	
	print "Reading in metadata for $pack.\n";
	$pkg->read_metadata_file("$metadatafile");
	for my $dep (keys %{$pkg->{'Source_Dependencies'}->{'pkgname-list'}})
	{
 	    print GRAPH "$pack -> $dep;\n" if $graph;
	    next if $graph and ($dep =~ /setup/ or $dep =~ /rips/);

	    # if we don't have $dep in our hash, add it and iterate
	    if ( ($package_build_hash{$dep} ne 1) and 
		 ( $dep ne "trusted_ca_setup") and
		 ( $dep ne "globus_gram_job_manager_service_setup") and
		 ( $dep ne "mmjfs_service_setup") and
		 ( $dep ne "mjs_service_setup") )
	    {
		$package_build_hash{$dep} = 1;
		$new_hash{$dep} = 1;
		print "Pulling in dependency $dep\n";
	    }
	}
    }

    # This checks whether new_hash is empty
    if ( %new_hash )
    {
	import_package_dependencies(%new_hash);
    }
}

# --------------------------------------------------------------------
sub setup_environment()
# --------------------------------------------------------------------
{
    print "Setting up build environment.\n";

    #TODO: figure out package list first, then set
    # cvs_build_hash appropriately.q
    if ( $cvs_build_hash{'gt3'} eq 1  )
    {
	check_java_env();
    }
    
    if ( $install )
    {
	$ENV{GLOBUS_LOCATION} = $install;
    } else {
	$ENV{GLOBUS_LOCATION} = "$source_output/tmp_core";
    }

    if ( $doxygen )
    {
	$doxygen = "CONFIGOPT_GPTMACRO=--enable-doxygen";
    } else {
	$doxygen = "";
    }

    if ( $verbose )
    {
	$verbose = "-verbose";
    } else {
	$verbose = "";
    }

    if ( $force )
    {
	$force = "-force";
    } else {
	$force = "";
    }


    report_environment();
}

# --------------------------------------------------------------------
sub check_java_env()
# --------------------------------------------------------------------
{
    if ( $ENV{'JAVA_HOME'} eq "" ) 
    {
	if ( -e "/usr/java/j2sdk1.4.1_02/" ) 
	{
	    $ENV{'JAVA_HOME'} = "/usr/java/j2sdk1.4.1_02/";
	} elsif ( -e "/home/dsl/javapkgs/java-env-setup.sh" )
	{
	    system("source /home/dsl/javapkgs/java-env-setup.sh");
	} elsif ( -e "/usr/java/jdk1.3.1_07" )
	{
	    $ENV{'JAVA_HOME'} = "/usr/java/jdk1.3.1_07";
	} else {
	    print "INFO: Could not find JAVA_HOME for your system.\n";
	}
    }
    system("ant -h > /dev/null");
    if ( $? != 0 )
    {
	print "INFO: ant -h returned an error.  Make sure ant is on your path.\n";
    }
}

# --------------------------------------------------------------------
sub report_environment()
# --------------------------------------------------------------------
{
    if ( -e "$top_dir/CVS/Tag" )
    {
	open(TAG, "$top_dir/CVS/Tag");
	my $mptag = <TAG>;
	chomp $mptag;
	print "You are using $0 from: " . substr($mptag,1) . "\n";
    } else {
	print "You are using $0 from HEAD.\n";
    }

    print "Using JAVA_HOME = ", $ENV{'JAVA_HOME'}, "\n";
    system("type ant");

    if ( $install )
    {
	print "Installing to $install\n";
    }

    print "\n";
}
    
	    
# --------------------------------------------------------------------
sub cleanup()
# --------------------------------------------------------------------
{
    # Need to make this depend on which steps you're going to do
    # So that if you want to leave things in package-output, you can.

    if ( not $skippackage and not $faster )
    {
	push @cleanup_dirs, "package-output";
    }

    for my $f ( @cleanup_dirs )
    {
	if ( -d "$f" )
	{
	    print "Cleaning up old build by moving $f to ${f}.old\n";
	    if ( -d "${f}.old" )
	    {
		system("rm -fr ${f}.old");
	    }
	    system("mv ${f} ${f}.old");
	}
    }

    print "\n";
}


# --------------------------------------------------------------------
sub populate_package_list
# --------------------------------------------------------------------
{
    my ($tree) = @_;
    my $build_default;

    if (-d "$top_dir/etc/$tree")
    {
	chdir "$top_dir/etc/$tree";
    } else {
	print "INFO: No packages defined for tree $tree.\n";
	return;
    }

    open(DEFAULT, "build-default");
    $build_default = <DEFAULT>;
    chomp $build_default;

    open(PKG, "package-list");

    while ( <PKG> )
    {
	my ($pkg, $subdir, $custom, $pnb) = split(' ', $_);
	chomp $pnb;

	next if substr($pkg, 0, 1) eq "#";

	if ( $custom eq "" )
	{
	    $custom = $build_default;
	}

	$package_list{$pkg} = [ $tree, $subdir, $custom, $pnb ];
    }
}

# TODO: Use the GT2 bundle.def files for this instead.
# TODO: Add the NMI GT3 bundle.xml files for this also.
# --------------------------------------------------------------------
sub populate_bundle_list
# --------------------------------------------------------------------
{
    my ($tree) = @_;
    my $bundle;

    chdir "$top_dir/etc/$tree";
    open(BUN, "bundles");

    while ( <BUN> )
    {
	my ($pkg, $bun, $threaded, $flags) = split(' ', $_);
	next if ( $pkg eq "" or $pkg eq "#" );
    
	chomp $flags;

	if ( $pkg eq "BUNDLE" )
	{
	    $bundle = $bun;

	    # Process threading and gpt-build flags (like -static)
	    if ( $threaded eq "THREADED" )
	    {
		push @{$bundle_list{$bundle}}, $flavor . $thread;
	    } else {
		push @{$bundle_list{$bundle}}, $flavor;
	    }

	    if ( defined $flags )
	    {
		push @{$bundle_list{$bundle}}, $flags;
	    } else {
		push @{$bundle_list{$bundle}}, "";
	    }
	} else {
	    if ( $bundle eq undef )
	    {
		print "Ignoring $pkg, no bundle set yet.\n";
	    } else {
		push @{$bundle_list{$bundle}}, $pkg;
	    }
	}
    }
}

# The goal is to let the user specify both bundles and packages.
# User defined packages will be collected into a bundle called
# "user_def".  If only packages are specified, then only those
# should be built.  If both packages and bundles are specified,
# then both sets of things should be built.  If only bundles are
# specified, only they should be built.
# If nothing is specified, build everything.
# --------------------------------------------------------------------
sub populate_bundle_build_list()
# --------------------------------------------------------------------
{
    if ( defined(@user_packages) ) 
    {
	my $bundle = "user_def";

	#TODO: How do I know what flavor for the user_def bundle?
	# Also, how do I know what flags?  For now, empty string.
	push @{$bundle_list{$bundle}}, $flavor;
	push @{$bundle_list{$bundle}}, "";
	push @{$bundle_list{$bundle}}, @user_packages;
	push @bundle_build_list, $bundle;
    } 

    if ( defined(@user_bundles) or defined(@user_packages))
    {
	foreach my $user_bundle (@user_bundles)
	{
	    if (exists $bundle_list{$user_bundle} )
	    {
		print "Bundle $user_bundle\n";
		push @bundle_build_list, $user_bundle;
		print "\n";
	    } else {
                die "Unknown bundle requested: $user_bundle\n";
	    }
	}
    } else {
	# build all bundles.
	for my $f ( keys %bundle_list )
	{
	    push @bundle_build_list, $f;
	    print "Bundle $f\n" if $listbun;
	}
    }
}

# --------------------------------------------------------------------
sub populate_package_build_hash()
# --------------------------------------------------------------------
{
    my @temp_build_list;

    # make the decision of whether to build all source packages or no.
    # bundle_build_list = array of bundle names.
    # $bundle_list{'bundle name'} = flavor, array of packages.
    # So, for each bundle to build, run through the array of packages
    # and add it to the list of packages to be built.
    if ( defined(@bundle_build_list) ) 
    {
	for my $iter (@bundle_build_list)
	{
	    my @array = $bundle_list{$iter};

	    foreach my $pkg_array (@array)
	    {
		# TODO: There must be a better way to skip flavors.
		# I don't like the magic number "2" below.  It comes
		# from having "flavor, flags" in the array ahead of
		# @package_list.  However, if I change it, this is
		# kludgy.
		my @tmp_array = @{$pkg_array};
		foreach my $pkg (@tmp_array[2..$#tmp_array]) {
		    push @temp_build_list, $pkg;
		}
	    }
	}
    } else {
	@temp_build_list = keys %package_list;
    }

    # Eliminate duplicates in the package_build_list
    # A "Perl Idiom".
    %package_build_hash = map { $_ => 1 } @temp_build_list;
    if ( $listpack )
    {
       foreach my $pack ( keys(%package_build_hash) )
       {
            print "$pack\n";
       }
    }
}

# --------------------------------------------------------------------
sub build_prerequisites()
# --------------------------------------------------------------------
{
    install_gpt();

    if ( $cvs_build_hash{'autotools'} eq 1 or
	 $cvs_build_hash{'gt2'} eq 1 or
	 $cvs_build_hash{'gt3'} eq 1 or
	 $cvs_build_hash{'gt4'} eq 1 or
	 $cvs_build_hash{'cbindings'})
    {
	install_gt2_autotools() if $autotools;
    }

    if ( $cvs_build_hash{'gt2'} eq 1 or 
	 $cvs_build_hash{'gt3'} eq 1 or
	 $cvs_build_hash{'gt4'} eq 1 or
	 $cvs_build_hash{'cbindings'} eq 1)
    {
	install_globus_core();
    }
}

# --------------------------------------------------------------------
sub paranoia
# --------------------------------------------------------------------
{
    my ($errno, $errmsg) = ($?, $!);
    my ($death_knell) = @_;

    if ($? ne 0 and $paranoia)
    {
	die "ERROR: $death_knell";
    }
}

# --------------------------------------------------------------------
sub log_system
# --------------------------------------------------------------------
{
    my ($command, $log) = @_;

    my $output;
    my $res;

    if ( $verbose )
    {
	# This contruction is like piping through tee
	# except that I can get the return code too.
	open LOG, ">>$log";
	open FOO, "$command 2>&1 |";

	while (<FOO>)
	{
	    my $line = $_;
	    print $line;
	    print LOG "$line";

	}

	close FOO;
	close LOG;
	$res = $?;
    }
    else
    {
	$output =  ">> $log 2>&1";
	system("$command $output");
	$res = $?
    }

    return $res;
}

# --------------------------------------------------------------------
sub install_gpt()
# --------------------------------------------------------------------
{
    my $gpt_ver = "gpt-3.0.1";
    my $gpt_dir = $top_dir . "/$gpt_ver";

    $ENV{'GPT_LOCATION'}=$gpt_dir;
    
    if ( -e "${gpt_dir}/sbin/gpt-build" )
    {
	print "GPT is already built, skipping.\n";
	print "\tDelete $gpt_dir to force rebuild.\n";
    } else {
	print "Installing $gpt_ver\n";
	print "Logging to ${log_dir}/$gpt_ver.log\n";
	chdir $top_dir;
	system("tar xzf fait_accompli/${gpt_ver}-src.tar.gz");
	paranoia("Trouble untarring fait_accompli/${gpt_ver}-src.tar.gz");

	chdir $gpt_dir;

	# gpt 3.0.1 has trouble if LANG is set, as on RH9
	# Newer GPTs will unset LANG automatically in build_gpt.
	my $OLANG = $ENV{'LANG'};
	$ENV{'LANG'} = "";
	system("./build_gpt > $log_dir/$gpt_ver.log 2>&1");
	$ENV{'LANG'} = $OLANG;

	paranoia("Trouble with ./build_gpt.  See $log_dir/$gpt_ver.log");
    }

    @INC = (@INC, "$gpt_dir/lib/perl", "$gpt_dir/lib/perl/$Config{'archname'}");
    print "\n";
}

#
# --------------------------------------------------------------------
sub gpt_get_version
# --------------------------------------------------------------------
{
    my ($metadatafile) = @_;

    require Grid::GPT::V1::Package;
    my $pkg = new Grid::GPT::V1::Package;
	
    $pkg->read_metadata_file("$metadatafile");
    my $version = $pkg->{'Version'}->label();
    return $version;
}

#TODO: Let them specify a path to autotools
# --------------------------------------------------------------------
sub install_gt2_autotools()
# --------------------------------------------------------------------
{
    my $res;
    chdir cvs_subdir('autotools');

    if ( -e 'autotools/bin/automake' )
    {
	print "Using existing GT2 autotools installation.\n";
    } else {
	print "Building GT2 autotools.\n";
	print "Logging to ${log_dir}/gt2-autotools.log\n";

	if ( -e "side_tools/build-autotools" )
	{	    
	    $res = log_system("./side_tools/build-autotools",
		    "${log_dir}/gt2-autotools.log");
	} else {
	    die "ERROR: side_tools/build-autotools doesn't exist.  Check cvs logs.";
	}

	if ( $? ne 0 )
	{
	    print "\tAutotools dies the first time through sometimes due to\n";
	    print "\temacs .texi issues.  I am trying again.\n";

	    log_system("./side_tools/build-autotools", 
		       "${log_dir}/gt2-autotools.log");
	    if ( $? ne 0 )
	    {
		die "ERROR: Error building autotools.  Check log.\n";
	    } else {
		print "\tWorked second time through.\n";
	    }
	}
    }

    $ENV{'PATH'} = cwd() . "/autotools/bin:$ENV{'PATH'}";

    print "\n";
}

# Some packages require globus core to be installed to build.
# TODO:  This should always go local, because packages install links
#  to the automake headers.  These links don't get cleaned by
#  make distclean.  So we need to have a stable install of globus_core
#  so that users can delete old install directories.
# --------------------------------------------------------------------
sub install_globus_core()
# --------------------------------------------------------------------
{
    system("$ENV{GPT_LOCATION}/sbin/gpt-build -nosrc $flavor");

    if ( $? ne 0 )
    {
	die "ERROR: Error building gpt_core from $ENV{GPT_LOCATION}/sbin/gpt-build -nosrc $flavor.\n";
    }
}


# Double-check that the existing checkout has the right tag for the user.
# Assumes you are somewhere where a CVS/Tag exists.
# --------------------------------------------------------------------
sub cvs_check_tag
# --------------------------------------------------------------------
{
    my ( $tree ) = @_;

    # TODO: Need to find a way to do this even if -noupdates is set.
    if ( -e "CVS/Tag" )
    {
	open(TAG, "CVS/Tag");
	my $cvstag = <TAG>;
	chomp $cvstag;
	$cvstag = substr($cvstag, 1);
	if ( $cvstag ne cvs_tag($tree) )
	{
	    die "ERROR: Want to build tag " . cvs_tag($tree) . ", CVS checkout is from $cvstag.\n";
	}
    }
}

# --------------------------------------------------------------------
sub cvs_subdir
# --------------------------------------------------------------------
{
    my ( $tree ) = @_;

    return $cvs_archives{$tree}[2];
}

# --------------------------------------------------------------------
sub cvs_tag
# --------------------------------------------------------------------
{
    my ( $tree ) = @_;

    return $cvs_archives{$tree}[3];
}

# --------------------------------------------------------------------
sub package_tree
# --------------------------------------------------------------------
{
    my ( $package ) = @_;

    return $package_list{$package}[0];
}

# --------------------------------------------------------------------
sub package_subdir
# --------------------------------------------------------------------
{
    my ( $package ) = @_;

    return cvs_subdir( package_tree($package) ) . "/" . $package_list{$package}[1];
}


# --------------------------------------------------------------------
sub get_sources()
# --------------------------------------------------------------------
{
    foreach my $tree ( @cvs_build_list )
    {
	print "Checking out cvs tree $tree.\n";
	cvs_checkout_generic( $tree );
    }
}

# --------------------------------------------------------------------
sub set_cvsroot
# --------------------------------------------------------------------
{
    my ($cvsroot) = @_;

    if ( defined $ENV{'CVSROOT'} )
    {
	$cvsroot = $ENV{'CVSROOT'};
    } elsif ( $anonymous )
    {
	$cvsroot = ":pserver:anonymous\@cvs.globus.org:" . $cvsroot;
    } else {
	if ( not -d "$cvsroot" )
	{
	    $cvsroot = "cvs.globus.org:$cvsroot";
	    $cvsroot = $cvsuser . "@" . $cvsroot if ( $cvsuser );
	    $ENV{CVS_RSH} = "ssh";
	}
	# else cvsroot is fine as-is.
    }

    return $cvsroot
}

# --------------------------------------------------------------------
sub cvs_checkout_subdir
# --------------------------------------------------------------------
{
    my ( $tree, $dir ) = @_;
    my $cvs_logs = $log_dir . "/cvs-logs";
    my ($cvsroot, $module, $cvs_dir, $tag) = @{$cvs_archives{$tree}};
    my $cvsopts = "-r $tag";
    my $locallog;

    mkdir $cvs_logs if not -d $cvs_logs;
    mkdir $cvs_prefix if not -d $cvs_prefix;
    mkdir $cvs_dir if not -d $cvs_dir;
    chdir $cvs_dir;

    $cvsroot = set_cvsroot($cvsroot);

    if ( $tag eq "HEAD" )
    {
	$cvsopts = "";
    }

    $locallog = $dir;
    $locallog =~ tr|/|_|;

    if ( ! -d "$dir" ) 
    { 
	log_system("cvs -d $cvsroot co $cvsopts $dir",
		   "$cvs_logs/" . $locallog . ".log");
    } else { 
	log_system("cvs -d $cvsroot update $dir", 
		   "$cvs_logs/" . $locallog . ".log");
    }
}

# --------------------------------------------------------------------
sub cvs_checkout_package
# --------------------------------------------------------------------
{
    my ( $package ) = @_;
    my $tree = package_tree($package);
    my $subdir = $package_list{$package}[1];

    if (! defined($tree)) {
        die "ERROR: There was a dependency on package $package which I know nothing about.\n";
    }

    print "Checking out $subdir from $tree.\n";
    cvs_checkout_subdir($tree, $subdir);
}

# --------------------------------------------------------------------
sub cvs_checkout_generic ()
# --------------------------------------------------------------------
{
    my ( $tree ) = @_;
    my ($cvsroot, $module, $dir, $tag) = @{$cvs_archives{$tree}};
    my $cvs_logs = $log_dir . "/cvs-logs";
    my $cvsopts = "-r $tag";

    system("mkdir -p $cvs_logs") unless -d $cvs_logs;

    chdir $cvs_prefix;
    $cvsroot = set_cvsroot($cvsroot);

    if ( not -e $dir )
    {
	print "Making fresh CVS checkout of \n";
	print "$cvsroot, module $module, tag $tag\n";
	print "Logging to $cvs_logs/" . $tree . ".log\n";
	system("mkdir -p $dir");
	paranoia("Can't make $dir: $!.\n");
	chdir $dir || die "Can't cd to $dir: $!\n";

	#CVS doesn't think of HEAD as a branch tag, so
	#don't use -r if you're checking out HEAD.
	if ( $tag eq "HEAD" )
	{
	    $cvsopts = "";
	}

	log_system("cvs -d $cvsroot co -P $cvsopts $module",
		   "$cvs_logs/" . $tree . ".log");

	if ( $? ne 0 )
	{
	    chdir "..";
	    rmdir $dir;
	    die "ERROR: There was an error checking out $cvsroot with module $module, tag $tag.\n";
	}
    }
    else {
	if ( $noupdates )
	{
	    print "Skipping CVS update of $cvsroot\n";
	    print "INFO: This means that I'm not checking the CVS tag for you, either.\n";
	}
	else {
	    my @update_list;
	    print "Updating CVS checkout of $cvsroot\n";
	    chdir $dir;

	    for my $f ( <*> ) 
	    {
		chdir $f;
		cvs_check_tag($tree);
                if ( -d "CVS" )
                {
                    print "Queueing $f on update command.\n";
                    push @update_list, $f;
                }
                chdir '..';
	    }
	    print "Logging to ${cvs_logs}/" . $tree . ".log\n";

            log_system("cvs -d $cvsroot -z3 up -dP @update_list", "${cvs_logs}/" . $tree . ".log");
            paranoia "Trouble with cvs up on tree $tree.";
	}
    }

    print "\n";
}



# --------------------------------------------------------------------
sub package_sources()
# --------------------------------------------------------------------
{
    my $build_default;

    mkdir $pkglog;
    mkdir $source_output;
    mkdir $package_output;

    for my $package ( keys %package_build_hash )
    {
	my ($tree, $subdir, $custom) = ($package_list{$package}[0],
					$package_list{$package}[1], 
					$package_list{$package}[2]);
	chdir cvs_subdir($tree);

	if ( $faster )
	{
	    if ( -e <$package_output/${package}-.*> )
	    {
		print "-faster set.  ${package} exists, skipping.\n";
		next;
	    }
	}

	if ( $inplace )
	{
	    print "Not generating GPT packages.  Building $package inside CVS.\n";
	    inplace_build($package, $subdir, $tree);
	    next;
	}

	if ( $custom eq "gpt" ){
	    package_source_gpt($package, $subdir, $tree);
	} elsif ( $custom eq "pnb" ){
	    package_source_pnb($package, $subdir, $tree);
	} elsif ( $custom eq "tar" ) { 
	    package_source_tar($package, $subdir);
	} else {
	    print "ERROR: Unknown custom packaging type '$custom' for $package.\n";
	}
    }
    
    print "\n";
}

# --------------------------------------------------------------------
sub inplace_build()
# --------------------------------------------------------------------
{
    my ($package, $subdir, $tree) = @_;

    chdir $subdir;
    log_system("$ENV{'GPT_LOCATION'}/sbin/gpt-build --srcdir=. $flavor", "$pkglog/$package");
    paranoia("Inplace build of $package failed!");

}

# --------------------------------------------------------------------
sub package_source_gpt()
# --------------------------------------------------------------------
{
    my ($package, $subdir, $tree) = @_;
    
    if ( ! -d $subdir )
    {
	die "$subdir does not exist, for package $package in tree $tree\n";
    } else {
	#This causes GPT not to worry about whether dependencies
	#have been installed while doing configure/make dist.
	#Any non-zero value will do.  I chose "and how" for fun.
	$ENV{'GPT_IGNORE_DEPS'}="and how";

	chdir $subdir;

	print "Following GPT packaging for $package.\n";

	if ( -e 'Makefile' )
	{
	   log_system("make distclean", "$pkglog/$package");
	   paranoia("make distclean failed for $package");
	}

	log_system("./bootstrap", "$pkglog/$package");
	paranoia("$package bootstrap failed.");

	#TODO: make function out of "NB" part of PNB, call it here.
	if ( $package eq "globus_gridftp_server" or $package eq "gsincftp") 
	{
	    print "\tSpecial love for gridftp_server and gsincftp\n";
	    my $version = gpt_get_version("pkg_data_src.gpt");

	    my $tarfile = "$package-$version";

	    #Strip leading dirs off of $subdir
	    my ($otherdirs, $tardir) = $subdir =~ m!(.+/)([^/]+)$!;

	    if ( -e Makefile )
	    {
		log_system("make distcean", "$pkglog/$package");
		paranoia "make distclean failed for $package";
	    }

	    chdir "..";
	    
	    # The dir we are tarring is probably called "source" in CVS.
	    # mv it to package name.
	    log_system("mv $tardir $package-$version",
		       "$pkglog/$package");
	    paranoia "system() call failed.  See $pkglog/$package.";
	    log_system("tar cf $package_output/$tarfile.tar $package-$version",
		       "$pkglog/$package");
	    paranoia "system() call failed.  See $pkglog/$package.";
	    log_system("gzip -f $package_output/$tarfile.tar",
		       "$pkglog/$package");
	    paranoia "system() call failed.  See $pkglog/$package.";

	    # Move it back so future builds find it.
	    log_system("mv $package-$version $tardir",
		       "$pkglog/$package");
	    paranoia "system() call failed.  See $pkglog/$package.";
	} else {	
	    log_system("./configure --with-flavor=$flavor",
		       "$pkglog/$package");
	    paranoia "configure failed.  See $pkglog/$package.";
	    log_system("make dist", "$pkglog/$package");
	    paranoia "make dist failed.  See $pkglog/$package.";
	    my $version = gpt_get_version("pkgdata/pkg_data_src.gpt");
	    log_system("cp ${package}-${version}.tar.gz $package_output", "$pkglog/$package");
	    paranoia "cp of ${package}-*.tar.gz failed: $!  See $pkglog/$package.";
	    $ENV{'GPT_IGNORE_DEPS'}="";
	}
    }
}

# --------------------------------------------------------------------
sub package_source_pnb()
# --------------------------------------------------------------------
{
    my ($package, $subdir, $tree) = @_;
    my $tarname = $package_list{$package}[3];
    my $tarfile = cvs_subdir($tree) . "/tarfiles/" . $tarname;
    my $tarbase = $tarname;
    $tarbase =~ s!\.tar\.gz!!;
    my $patchfile = "${tarbase}-patch";

    print "Following PNB packaging for $package.\n";
    print "\tUsing tarfile: $tarfile.\n";

    chdir $subdir;

    my $version = gpt_get_version("pkg_data_src.gpt");

    # Some patches will fail to apply a second time
    # So clean up the old patched tar directory if
    # it exists from a previous build.
    if ( -d "$tarbase" )
    {
	log_system("rm -fr $tarbase", "$pkglog/$package");
	paranoia("$tarbase exists, but could not be deleted.\n");
    }

    log_system("gzip -dc $tarfile | tar xf -",
	       "$pkglog/$package");
    paranoia "Untarring $package failed.  See $pkglog/$package.";
    chdir $tarbase;
    log_system("patch -N -s -p1 -i ../patches/$patchfile",
	       "$pkglog/$package");
    paranoia "patch failed.  See $pkglog/$package.";

    # Strip off leading directory component
    my ($otherdirs, $tardir) = $subdir =~ m!(.+/)([^/]+)$!;

    chdir "../..";

    # The dir we are tarring is probably called "source" in CVS.
    # mv it to package name so tarball looks correct.
    log_system("mv $tardir $package-$version",
	       "$pkglog/$package");
    paranoia "a system() failed.  See $pkglog/$package.";
    log_system("tar cf $package_output/${package}-${version}.tar $package-$version",
	       "$pkglog/$package");
    paranoia "a system() failed.  See $pkglog/$package.";
    log_system("gzip -f $package_output/${package}-${version}.tar",
	       "$pkglog/$package");
    paranoia "a system() failed.  See $pkglog/$package.";

    # Move it back so future builds find it.
    log_system("mv $package-$version $tardir",
	       "$pkglog/$package");
    paranoia "a system() failed.  See $pkglog/$package.";
}

# --------------------------------------------------------------------
sub package_source_tar()
# --------------------------------------------------------------------
{
    my ($package, $subdir) = @_;

    my $package_name="${package}-src";
    my $destdir = "$source_output/$package_name";
    
    if ( ! -d $subdir )
    {
	print "$subdir does not exist for package $package.\n";
    } else {
	print "Creating source directory for $package\n";
	log_system("rm -fr $destdir", "$pkglog/$package");

	mkdir $destdir;
	log_system("cp -Rp $subdir/* $destdir", "$pkglog/$package");
	paranoia "Failed to copy $subdir to $destdir for $package.";
	log_system("touch $destdir/INSTALL", "$pkglog/$package");
	paranoia "touch $destdir/INSTALL failed";

	if ( -e "$destdir/pkgdata/pkg_data_src.gpt" and not $uncool)
	{
	    log_system("cp $destdir/pkgdata/pkg_data_src.gpt $destdir/pkgdata/pkg_data_src.gpt.in",
		       "$pkglog/$package");
	    paranoia "Metadata copy failed for $package.";
	    if (!( -e "$destdir/filelist" ))
	    {
	      if ( -e "$destdir/pkgdata/filelist" )
	      {
	  	  log_system("cp $destdir/pkgdata/filelist $destdir", "$pkglog/$package");
		  paranoia "Filelist copy failed for $package.";
              } else {
		  print "\tPartially cool.  Still got filelist from package-list.\n";
		  log_system("cp $top_dir/package-list/$package/filelist 	$destdir/", "$pkglog/$package");
		  paranoia "Filelist copy from package-list failed for $package.";
	      }
            }
	} else {
	    log_system("mkdir -p $destdir/pkgdata/", "$pkglog/$package");
	    paranoia "mkdir failed during $package.";
	    log_system("cp $top_dir/package-list/$package/pkg_data_src.gpt  $destdir/pkgdata/pkg_data_src.gpt.in",
		   "$pkglog/$package");
	    paranoia "Metadata copy failed for $package.";
	    log_system("cp $top_dir/package-list/$package/filelist  $destdir/",
		   "$pkglog/$package");
	    paranoia "Filelist copy failed for $package.";
	    print "\tUsed pkgdata from package-list, not cool.\n";
	}
    
	#Introspect metadata to find version number.
	my $version = gpt_get_version("$destdir/pkgdata/pkg_data_src.gpt.in");

	my $tarfile = "$package-$version";
	
	chdir $source_output;
	log_system("tar cvzf $package_output/$tarfile.tar.gz $package_name",
		   "$pkglog/$package");
	paranoia "tar failed for $package.";
    }
}

#TODO: Add bundle logging.
# --------------------------------------------------------------------
sub bundle_sources()
# --------------------------------------------------------------------
{
    my $bundlename;

    mkdir $bundle_output;
    mkdir $bundlelog;
    chdir $bundle_output;

    for my $bundle ( @bundle_build_list )
    {
	next if $bundle eq "";
	next if $bundle eq "user_def";

	print "Trying to make bundle $bundle\n";
	mkdir $bundle;

	open(PKG, ">$bundle/packaging_list") or die "Can't open packaging_list: $!\n";

	my @tmp_array = @{$bundle_list{$bundle}};
	for my $package ( @tmp_array[2..$#tmp_array])
	{
#	    next if $package eq $flavor or $package eq $flavor . $thread;
	    system("cp $package_output/${package}-* $bundle");
	    paranoia("cp of $package_output/${package}-* failed.");
	    print PKG "$package\n";
	}
	#TODO: Let user choose deps/nodeps
	#TODO: backticks make me nervous about using log_system
	system("($ENV{'GPT_LOCATION'}/sbin/gpt-bundle -nodeps -bn=$bundle -bv=$version -srcdir=$bundle `cat $bundle/packaging_list`) > $bundlelog/$bundle 2>&1");
	paranoia("Bundling of $bundle failed.  See $bundlelog/$bundle.");
    }
}

#TODO: Need better way of keeping GPT version in sync with
# the GPT version in use by mp
# --------------------------------------------------------------------
sub create_installer()
# --------------------------------------------------------------------
{
    open(INS, ">$bundle_output/$installer") or die "Can't open $bundle_output/$installer: $!\n";

    print INS << "EOF";
#!/bin/sh

if [ x\$1 = "x" ]; then
        echo \$0: Usage: \$0 install-directory
        exit;
fi

mkdir -p \$1
if [ \$? -ne 0 ]; then
        echo Unable to make directory \$1.  Exiting.
        exit
fi
EOF

print INS "echo Build environment:\n";

if ($cvs_build_hash{'gt3'} eq 1)
{
    print INS << "EOF";
type ant
if [ \$? -ne 0 ]; then
   echo You need a working version of ant.
   exit
fi
type java
if [ \$? -ne 0 ]; then
   echo You need a working version of java.
   exit
fi
EOF
}

 if ($cvs_build_hash{'gt2'} eq 1)
{
print INS "type gcc\n"
}

print INS "echo\n\n";

print INS << "EOF";
MYDIR=`pwd`

cd \$1

INSDIR=`pwd`
GPT_LOCATION="\$INSDIR"
GLOBUS_LOCATION="\$INSDIR"
GT3_LOCATION="\$INSDIR"
GPT_BUILD="\$GPT_LOCATION"/sbin/gpt-build
GPT_INSTALL="\$GPT_LOCATION"/sbin/gpt-install

cd \$MYDIR

export GPT_LOCATION GLOBUS_LOCATION GT3_LOCATION

case "`uname`" in
   HP-UX)
        FLAVOR=vendorcc32dbg
        ;;
   OSF1)
        FLAVOR=vendorcc64dbg
        ;;
   *)
        case "`uname -m`" in
            alpha|ia64)
                FLAVOR=gcc64dbg
                ;;
            *)
                FLAVOR=gcc32dbg
                ;;
        esac
        ;;
esac

THREAD=pthr

if [ ! -f gpt-3.0.1/sbin/gpt-build ]; then
    echo Building GPT ...
    gzip -dc gpt-3.0.1-src.tar.gz | tar xf -
    cd gpt-3.0.1

    LANG="" ./build_gpt
    if [ \$? -ne 0 ]; then
        echo Error building GPT
        exit;
    fi

    cd ..
fi

if [ -d BUILD ]; then
        rm -fr BUILD
fi


EOF


my $flavorstring;
for my $bundle ( @bundle_build_list )
{
    next if $bundle eq "" or $bundle eq "user_def";
	
    my ($flava, $flags, @packages) = @{$bundle_list{$bundle}};

    if ( $flava eq $flavor . $thread )
    {
	$flavorstring = "\$FLAVOR\$THREAD";
    } else {
	$flavorstring = "\$FLAVOR";
    }

   if ( $bundle eq "globus-rls-server" )
   {
      print INS "if [ x\$GLOBUS_IODBC_PATH = \"x\" ]; then\n";
      print INS "\techo GLOBUS_IODBC_PATH not set, skipping RLS build.\n";
      print INS "else\n";
   }

   print INS "\$GPT_BUILD $flags bundles/${bundle}-*-src_bundle.tar.gz $flavorstring\n";

   print INS "if [ \$? -ne 0 ]; then\n";
   print INS "    echo Error building $bundle\n";
   print INS "    exit 1;\n";
   print INS "fi\n" if ( $bundle eq "globus-rls-server" );
   print INS "fi\n\n";
}

print INS "\$GPT_LOCATION/sbin/gpt-postinstall\n";

print "Wrote installer $bundle_output/$installer.\n";
}


#TODO: Allow users to specify pre-existing binary bundles too.
# --------------------------------------------------------------------
sub install_bundles
# --------------------------------------------------------------------
{
    chdir $bundle_output;

    for my $bundle ( @bundle_build_list )
    {
	next if $bundle eq "" or $bundle eq "user_def";
	
	my ($flava, $flags, @packages) = @{$bundle_list{$bundle}};
	
	print "Installing $bundle to $install using flavor $flava, flags $flags.\n";
	system("$ENV{'GPT_LOCATION'}/sbin/gpt-build $doxygen $force $verbose $flags ${bundle}-*.tar.gz $flava");
	paranoia("Building of $bundle failed.\n");
    }

}

# --------------------------------------------------------------------
sub install_packages
# --------------------------------------------------------------------
{
    chdir $package_output;

    if ( $deps )
    {
	print "Installing all dependencies in flavor $flavor pulled in.\n";
	system("$ENV{'GPT_LOCATION'}/sbin/gpt-bundle -srcdir=. -bn=deps -all");
	system("$ENV{'GPT_LOCATION'}/sbin/gpt-build $force $verbose deps-*.tar.gz $flavor");
    } else 
    {
	for my $pkg ( @user_packages )
	{
	    print "Installing user requested package $pkg to $install using flavor $flavor.\n";
	    system("$ENV{'GPT_LOCATION'}/sbin/gpt-build $force $verbose ${pkg}-*.tar.gz $flavor");
	    paranoia("Building of $pkg failed.\n");
	}
    }
}

# --------------------------------------------------------------------
sub generate_bin_packages
# --------------------------------------------------------------------
{
    mkdir $bin_bundle_output;
    mkdir $bundlelog;
    chdir $bin_bundle_output;
    my $arch=`uname -m`;
    chomp $arch;

    log_system("$ENV{'GPT_LOCATION'}/sbin/gpt-pkg -all -pkgdir=$bin_output $verbose", "$log_dir/binary_packaging");

    paranoia("Failure to package binaries.  See $log_dir/binary_packaging");

    for my $bundle ( @bundle_build_list )
    {
	next if $bundle eq "" or $bundle eq "user_def";
	print "$ENV{'GPT_LOCATION'}/sbin/gpt-bundle -bn='${bundle}-${arch}' -bv=$version -nodeps -bindir=$bin_output `cat $bundle_output/$bundle/packaging_list`\n";
	log_system("$ENV{'GPT_LOCATION'}/sbin/gpt-bundle -bn='${bundle}-${arch}' -bv=$version -nodeps -bindir=$bin_output `cat $bundle_output/$bundle/packaging_list`", "$bundlelog/binary_$bundle");

	paranoia("Failed to create binary bundle for $bundle.");
    }
}

END{}
1;

=head1 NAME

make-packages.pl - GT3 packaging tool

=head1 SYNOPSIS

make-packages.pl [options] [file ...]

Options:

    --skippackage          Don't create source packages
    --skipbundle           Don't create source bundles
    --install=<dir>        Install into <dir>
    --anonymous            Use anonymous cvs checkouts
    --no-updates           Don't update CVS checkouts
    --force                Force
    --faster               Don't repackage if packages exist already
    --flavor               Set flavor base.  Default gcc32dbg
    --gt2-tag (-t2)        Set GT2 and autotools tags.  Default HEAD
    --gt3-tag (-t3)        Set GT3 and cbindings tags.  Default HEAD
    --gt2-dir (-d2)        Set GT2 and autotools CVS directory.
    --gt3-dir (-d3)        Set GT3 and cbindings CVS directory.
    --verbose              Be verbose.  Also sends logs to screen.
    --bundles="b1,b2,..."  Create bundles b1,b2,...
    --packages="p1,p2,..." Create packages p1,p2,...
    --trees="t1,t2,..."    Work on trees t1,t2,... Default "gt2,gt3,cbindings"
    --noparanoia           Don't exit at first error.
    --help                 Print usage message
    --man                  Print verbose usage page

=head1 OPTIONS

=over 8

=item B<--skippackage>

Don't create source packages.  In this case, you should have source
    packages already created in source-packages/

=item B<--skipbundle>

Don't create source bundles.

=item B<--install=dir>

Attempt to install packages and bundles into dir.  Short
version is "-i=".

=item B<--anonymous>

Use anonymous cvs checkouts.  Otherwise it defaults to using
CVS_RSH=ssh.  Short version is "-a"

=item B<--no-updates>

Don't update CVS checkouts.  This is useful if you have local
modifications.  Note, however, that make-packages won't
check that your CVS tags match the requested CVS tags.
Short version is "-n"
 
=item B<--faster>

Faster doesn't work correctly.  It is supposed to not try 
re-creating a package that has already been packaged.

=item B<--flavor=>

Set flavor base.  Default gcc32dbg.  You might want to
switch it to vendorcc.  Threading type is currently always
"pthr" if necessary.

=item B<--gt2-tag=TAG3, --gt3-tag=TAG3>

Set GT2 or GT3 tag.  Default HEAD.  Short version is "-t2="
or "-t3=".

=item B<--verbose>

Echoes all log output to screen.  Otherwise logs are just
stored under log-output.  Good for getting headaches.

=item B<--bundles="b1,b2,...">

Create bundles b1,b2,....  Bundles are defined under
etc/*/bundles

=item B<--packages="p1,p2,...">

Create packages p1,p2,....  Packages are defined under
etc/*/package-list

=item B<--paranoia>

Exit at first error.  This can save you a lot of time
and debugging effort.  Disable with --noparanoia.

=back

=head1 DESCRIPTION

B<make-packages.pl> goes from checking out CVS to
creating GPT packges, then bundles, then installing.

You can affect the flow of control by not updating
CVS with "-n"

=cut
