BEGIN
{
    push(@INC, "$ENV{GLOBUS_LOCATION}" . '/lib/perl');
}

#use warnings;
use strict;
use Globus::Core::Paths;

# Prototypes
sub GassCacheList ( );
sub GassCacheCleanupUrl ( $ );


# ******************************************************
# Command line options
# ******************************************************
my %Options =
    (
     "[-ping]"		=> "Enable ping test",
     "[-]"		=> "Read roster from STDIN",
     "[-reserved]"	=> "Check the reserved list",
     "[-cluster]"	=> "Report \"Cluster\" nodes that are down",
     "[-h]"		=> "Dump help",
    );
my $Program = $0;
my $ProgramC = $Globus::Core::Paths::libexecdir . '/globus-gass-cache-util';
my $verbose = 0;


# Option flags
my $OptionLong = 0;

# C Program flags & args
my @CprogFlags = (
		  "-a", "-add",
		  "-d", "-delete",
		  "-dirs",
		  "-m", "-mangle",
		  "-q", "-query",
		  "-l", "-list",
		  "-cleanup-tag",
		  "-cleanup-url" );
my $CprogFlagsRE =
    "(" . join( ")|(", @CprogFlags ) . ")";

# C Program flags & args
my @CprogArgs = (
		 "-h", "-mdshost",
		 "-p", "-mdsport",
		 "-b", "-mdsbasedn",
		 "-T", "-mdstimeout",
		 "-r", "-resource",
		 "-n", "-newname",
		 "-t", "-tag" );
my $CprogArgsRE =
    "(" . join( ")|(", @CprogArgs ) . ")";

# The manglings we know about..
my @Manglings = ( "html", "md5" );

# Make stdout sane
$|=1;

# Walk through the command line
my $URL = "";
if ( $#ARGV < 0 )
{
    exec ( "$ProgramC", @ARGV );
}
my @JobList;
my $ArgNo;
for($ArgNo = 0; $ArgNo < @ARGV; $ArgNo++)
{
    my $Arg = $ARGV[$ArgNo];

    if ( ( $Arg eq "-l" ) || ( $Arg eq "-list" ) )
    {
	push @JobList, "list";
    }
    elsif ( ( $Arg eq "-cu" ) || ( $Arg eq "-cleanup-url" ) )
    {
	push @JobList, "cleanup-url";
    }
    elsif ( $Arg eq "-long" )
    {
	$OptionLong = 1;
    }
    elsif ( $Arg eq "-v" )
    {
	$verbose++;
    }
    elsif ( $Arg =~ /^($CprogFlagsRE)$/ )
    {
	# Invoke the C program
	exec ( "$ProgramC", @ARGV );
    }
    elsif ( $Arg =~ /^($CprogArgsRE)$/ )
    {
	# Skip next arg...
        $ArgNo++;
    }
    elsif ( $Arg =~ /^-/ )
    {
	# Not sure what the hell it is, punt it off to the C prog...
	exec ( "$ProgramC", @ARGV );
    }
    # Must be the URL
    else
    {
	$URL = $Arg;
    }
}

# Did we do anything?  If not, punt to the C program...
if ( $#JobList < 0 )
{
    exec ( "$ProgramC", @ARGV );
}
else
{
    $#ARGV = -1;
    foreach my $Job ( @JobList )
    {
	if ( $Job eq "list" )
	{
	    GassCacheList( );
	}
	elsif ( $Job eq "cleanup-url" )
	{
	    GassCacheCleanupUrl( $URL );
	}
    }
}


# ******************************************************
# List the contents of the Cache
# ******************************************************
sub GassCacheList ( )
{
    my %RootDirs;

    # Read the directories from the program...
    my $Cmd = "$ProgramC -dirs";
    open( DIRS, "$Cmd|" ) || die "Can't run '$Cmd'";
    while( <DIRS> )
    {
	if ( /^(\w+)_ROOT: '(.*)'/ )
	{
	    $RootDirs{$1} = $2;
	}
    }
    close( DIRS );
    die "No GLOBAL root" if ( ! defined $RootDirs{GLOBAL} );
    die "No LOCAL root" if ( ! defined $RootDirs{LOCAL} );

    # Now, let's do the real fun...
    print "Scanning the global entries in $RootDirs{GLOBAL}\n" if($verbose);
    my %Global;
    $Cmd = "find $RootDirs{GLOBAL} -name 'data*' -print";
    foreach my $FullPath (`$Cmd` )
    {
	chomp $FullPath;
	my @Stat = stat( $FullPath );
	my $Inode = $Stat[1];
	my $Size = $Stat[7];

	# And, let's get it's directory
	my @Dirs = split( /\//, $FullPath );
	$#Dirs--;
	my $Dir = join( "/", @Dirs );

	# Read the URL from the "url" file
	my $Url = "";
	if ( open( URL, "$Dir/url" ) )
	{
	    $Url = <URL>; chomp $Url;
	    close( URL );
	}

	# Pull out it's mangled dir
	my $Mangled = $Dir;
	$Mangled =~ s/$RootDirs{GLOBAL}\///;

	# Store it all in a (perl) hash
	my $r = ();
	$r->{Inode} = $Inode;
	$r->{Size} = $Size;
	$r->{Url} = $Url;
	$r->{Mangled} = $Mangled;
	$r->{Dir} = $Dir;
	$Global{$Inode} = $r;
	$r = ();
    }
    close( FIND );

    # Scan through the local directory, now..
    print "Scanning the local entries in $RootDirs{LOCAL}\n" if($verbose);
    my @Local;
    $Cmd = "find $RootDirs{LOCAL} -name 'data*' -print";
    foreach my $FullPath (`$Cmd` )
    {
	chomp $FullPath;
	my @Stat = stat( $FullPath );
	my $Inode = $Stat[1];
	my $Size = $Stat[7];

	# Get the directory portion of the path
	my @Dirs = split( /\//, $FullPath );
	$#Dirs--;
	my $Dir = join( "/", @Dirs );

	# Read the tag from the tag file..
	my $Tag = "";
	if ( open( TAG, "$Dir/tag" ) )
	{
	    $Tag = <TAG>;
	    close( TAG );
	}

	# Strip the local dir portion off the hash
	my $Mangled = $Dir;
	$Mangled =~ s/$RootDirs{LOCAL}\///;

	# There *should* be a matching global... Let's look
	my $MangledOk = 0;
	if ( defined( $Global{$Inode} ) )
	{
	    my $GlobalMangled = $Global{$Inode}->{Mangled};
	    $Mangled =~ s/\/$GlobalMangled//;
	    $MangledOk = 1;
	}
	# Otherwise, we should be able to get the hash from the tag..
	elsif ( $Tag ne "" )
	{
	    my $Cmd = "$ProgramC -t $Tag -m";
	    if ( open ( MANGLE, "$Cmd|" ) )
	    {
		while( <MANGLE> )
		{
		    if ( /^TAG: '(.*)'/ )
		    {
			$Mangled = $1;
			$MangledOk = 1;
		    }
		    }
		close( MANGLE );
	    }
	}

	# Otherwise, let's make some guesses..
	if ( ! $MangledOk )
	{
	    # Should currently be something like: md5/local/md5/global
	    foreach my $GlobalStr ( @Manglings )
	    {
		$GlobalStr = "/" . $GlobalStr . "/";
		my $GlobalStart = rindex( $Mangled, $GlobalStr );
		if ( $GlobalStart > 0 )
		{
		    $Mangled = substr( $Mangled, 0, $GlobalStart );
		    last;
		}
	    }
	}

	# Store it all in a (perl) hash
	my $r = ();
	$r->{Inode} = $Inode;
	$r->{Size} = $Size;
	$r->{Tag} = $Tag;
	$r->{Mangled} = $Mangled;
	$r->{Dir} = $Dir;
	push( @Local, $r );
	$r = ();
    }

    # Dump it all out..
    foreach my $Inode ( keys %Global )
    {
	print "URL: $Global{$Inode}->{Url}\n";
	if ( $OptionLong )
	{
	    print "\tSize: $Global{$Inode}->{Size}\n";
	    print "\tMangled: $Global{$Inode}->{Mangled}\n";
	}
	foreach my $Local ( @Local )
	{
	    if ( $Local->{Inode} == $Inode )
	    {
		print "\tTag:" . $Local->{Tag} . "\n";
	    }
	}
    }
}


# ******************************************************
# Cleanup a URL in the GASS Cache
# ******************************************************
sub GassCacheCleanupUrl ( $ )
{
    my $Url = shift;
    my %RootDirs;

    # Sanity check...
    if ( !defined( $Url ) || ( $Url eq "" )  )
    {
	print STDERR "CleanupUrl requires a URL to cleanup\n";
	system "$ProgramC -help";
	exit 1;
    }

    # Mangle the URL
    my $Mangled = "";
    {
	my $Cmd = "$ProgramC -mangle $Url";
	open( MANGLE, "$Cmd|" ) || die "Can't run '$Cmd'\n";
	while ( <MANGLE> )
	{
	    chomp;
	    if ( /URL:\s+\'(.*)\'/ )
	    {
		$Mangled = $1;
	    }
	}
	close( MANGLE );
	if ( $Mangled eq "" )
	{
	    print STDERR "Failed to mangle URL!\n";
	    exit 1;
	}
    }

    # Read the directories from the program...
    my $Cmd = "$ProgramC -dirs";
    open( DIRS, "$Cmd|" ) || die "Can't run '$Cmd'";
    while( <DIRS> )
    {
	if ( /^(\w+)_ROOT: '(.*)'/ )
	{
	    $RootDirs{$1} = $2;
	}
    }
    close( DIRS );

    # Let's learn all about our data file...
    my $FullGlobalDir = "$RootDirs{GLOBAL}/$Mangled";
    if ( ! -d $FullGlobalDir )
    {
	print STDERR "Could not clean up file because the URL was not found in the GASS cache.\n";
	exit 1;
    }
    my $FullGlobalData = "$FullGlobalDir/data";
    if ( ! -f $FullGlobalData )
    {
	print STDERR "Could not clean up file because the URL was not found in the GASS cache.\n";
	exit 1;
    }
    my $FullGlobalDataInode = -1;
    {
	my @Stat = stat( $FullGlobalData );
	if ( $#Stat < 0 )
	{
	    print STDERR "Could not stat data file for URL '$Url'\n";
	    print STDERR "Should be '$FullGlobalData'\n";
	    exit 1;
	}
	$FullGlobalDataInode = $Stat[1];
    }

    # Tell the user...
    print "Found global data file @ $FullGlobalData\n" if($verbose);

    # Scan through the local directory, now..
    my %Local;
    my $LocalCount = 0;
    $Cmd =
	"find $RootDirs{LOCAL} -inum $FullGlobalDataInode -print";
    open( FIND, "$Cmd|" ) || die "Can't run '$Cmd'";
    print "Scanning the local entries in $RootDirs{LOCAL}\n" if($verbose);
    while( <FIND> )
    {
	chomp;
	my $FullDataPath = $_;

	my @Dirs = split( /\//, $FullDataPath );
	my $DataFile = pop @Dirs;
	my $Dir = join( "/", @Dirs );
	die "Oops" if ( "$Dir/$DataFile" ne $FullDataPath );
	my $TagFile = "$Dir/tag";

	# Check it out..
	if ( ! -f $FullDataPath )
	{
	    print STDERR "$FullDataPath is not a file!\n";
	    next;
	}
	if ( ! ( $FullDataPath =~ /\/data/ ) )
	{
	    print STDERR "$FullDataPath is not a data file!!\n";
	    next;
	}
	if ( ! -d $Dir )
	{
	    print STDERR "$Dir is not a directory!!\n";
	    next;
	}
	my $Tag;
	if ( ! -f $TagFile )
	{
	    print STDERR "$TagFile <tag file> not found\n";
	    my $Tag = "";
	}
	else
	{
	    $Tag = `cat $TagFile`;
	}

	# Store it...
	my $r = {};
	$r->{Tag} = $Tag;
	$r->{File} = $DataFile;
	push @{$Local{$Dir}}, $r;
	$r = {};
	$LocalCount++;
    }
    close( FIND );

    # Print results to the user...
    if($verbose)
    {
	if ( $LocalCount <= 0 )
	{
	    print "No local links found\n";
	}
	else
	{
	    print "Found $LocalCount local links:\n";
	    foreach my $Dir ( keys %Local )
	    {
		foreach my $FileRec ( @{$Local{$Dir}} )
		{
		    my $File = $FileRec->{File};
		    my $Tag = $FileRec->{Tag};
		    if ( $Tag eq "" )
		    {
			print "\t$Dir/$File\n";
		    }
		    else
		    {
			print "\t$Tag\n";
		    }
		}
	    }
	}
    }

    # Clean it all up...
    foreach my $Dir ( keys %Local )
    {
	foreach my $FileRec ( @{$Local{$Dir}} )
	{
	    print "$FileRec->{Tag}.. ";
	    unlink "$Dir/$FileRec->{File}";
	    unlink "$Dir/url";

	    my @Dirs = split( /\//, $Dir );
	    while( 1 )
	    {
		my $Dir = join ('/', @Dirs );
		last if ( ! rmdir( $Dir ) );
		print "\n\t$Dir ";
		pop @Dirs;
	    }
	    print "\n";
	}
    }

    # And, remove the global dir
    {
	my $Dir = $FullGlobalDir;
	print "Global.. " if($verbose);
	unlink "$Dir/data";
	unlink "$Dir/url";

	my @Dirs = split( /\//, $Dir );
	while( 1 )
	{
	    my $Dir = join ('/', @Dirs );
	    last if ( ! rmdir( $Dir ) );
	    print "\n\t$Dir " if($verbose);
	    pop @Dirs;
	}
	print "\n" if($verbose);
    }

}

# ******************************************************
# Dump out usage
# ******************************************************
sub Usage ( $ )
{
    my $Unknown = shift;

    print "$Program: unknown option '$Unknown'\n" if ( $Unknown ne "" );
    printf "usage: $Program %s\n", join (" ", sort keys %Options);
    print "use '-h' for more help\n";
    exit 1;

} # usage ()
# ******************************************************

# ******************************************************
# Dump out help
# ******************************************************
sub Help ( )
{
    my ($opt, $text);

    printf "usage: $Program %s\n", join (" ", sort keys %Options);
    foreach $opt (sort {lc($a) cmp lc($b) } keys %Options)
    {
	printf ("  %15s : %-40s\n", $opt, $Options{$opt} );
    }
    exit 0;

} # help ()
# ******************************************************
