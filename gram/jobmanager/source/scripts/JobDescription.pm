#
# Globus::GRAM::JobDescription
#
# CVS Information
#     $Source$
#     $Date$
#     $Revision$
#     $Author$

use IO::File;
use Globus::GRAM::Error;

=head1 NAME

Globus::GRAM::JobDescription - GRAM Job Description

=head1 SYNOPSIS

    use Globus::GRAM::JobDescription;

    $description = new Globus::GRAM::JobDescription($filename);
    $executable = $description->executable();
    $description->add($new_attribute, $new_value);
    $description->save();
    $description->save($filename);
    $description->print_recursive($file_handle);

=head1 DESCRIPTION

This object contains the parameters of a job request in a simple
object wrapper. The object may be queried to determine the value of
any RSL parameter, may be updated with new parameters, and may be saved
in the filesystem for later use.

=head2 Methods

=over 4

=cut

package Globus::GRAM::JobDescription;

=item new Globus::GRAM::JobDescription(I<$filename>)

A JobDescription is constructed from a 
file consisting of a Perl hash of parameter => array mappings. Every
value in the Job Description is stored internally as an array, even single
literals, similar to the way an RSL tree is parsed in C. An example of such
a file is

    $description =
    {
	executable  => [ '/bin/echo' ], 
	arguments   => [ 'hello', 'world' ],
	environment => [
	                   [
			       'GLOBUS_GRAM_JOB_CONTACT',
			       'https://globus.org:1234/2345/4332'
			   ]
		       ]
    };

which corresponds to the rsl fragment

    &(executable  = /bin/echo)
     (arguments   = hello world)
     (environment =
         (GLOBUS_GRAM_JOB_CONTACT 'https://globus.org:1234/2345/4332')
     )

=cut

sub new
{
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $desc_fn = shift;
    my $desc_values;
    my $self;

    $self = require "$desc_fn";
    $self->{_description_file} = $desc_fn;

    bless $self, $class;

    return $self;
}

=item $description->I<add>('name', I<$value>);

Add a parameter to a job description. The parameter will be normalized
internally so that the access methods described below will work with
this new parameter. As an example,

    @description->add('new_attribute', $new_value)

will create a new attribute in the JobDescription, which can be accessed
by calling the I<$description->new_attribute>() method.

=cut

sub add
{
    my $self = shift;
    my $attr = shift;
    my $value = shift;

    $attr =~ s/_//g;
    $attr = lc($attr);

    $self->{$attr} = [$value];
}

=item $description->I<save>([$filename])

Save the JobDescription, including any added parameters, to the file
named by $filename if present, or replacing the file used in constructing
the object.

=cut

sub save
{
    my $self = shift;
    my $filename = shift or "$self->{_description_file}.new";
    my $file = new IO::File(">$filename");

    $file->print("\$description = {\n");

    foreach (keys %{$self})
    {
	$file->print("    '$_' => ");
	
	$self->print_recursive($file, $self->{$_});
	$file->print(",\n");
    }
    $file->print("};\n");
    $file->close();

    if($filename eq "$self->{_description_file}.new")
    {
	rename("$self->{_description_file}.new", $self->{_description_file});
    }

    return 0;
}

=item $description->I<print_recursive>($file_handle)

Write the value of the job description object to the file handle
specified in the argument list.

=cut

sub print_recursive
{
    my $self = shift;
    my $file = shift;
    my $value = shift;
    my $first = 1;

    if(ref($value) eq "SCALAR")
    {
	$file->print($value);
    }
    elsif(ref($value) eq "ARRAY")
    {
	$file->print("[ ");
	foreach (@{$value})
	{
	    $file->print(", ") if (!$first);
	    $first = 0;
	    $self->print_recursive($file, $_);
	}
	$file->print(" ]");
    }
    elsif(ref($value) eq "HASH")
    {
	$file->print("(");

	foreach (keys %{$value})
	{
	    $file->print(", ") if (!$first);
	    $first = 0;
	    $file->print("'$_' => ");
	    $self->print_recursive($file, $value->{$_});
	}
    }
    elsif(!ref($value))
    {
	$file->print("'$value'");
    }
    return;
}

=item $description->I<parameter>()

For any parameter defined in the JobDescription can be accessed by calling
the method named by the parameter. The method names are automatically created
when the JobDescription is created, and may be invoked with arbitrary
SillyCaps or underscores. That is, the parameter gram_myjob may be accessed
by the GramMyJob, grammyjob, or gram_my_job method names (and others).

If the attributes does not in this object, then undef will be returned.

In a list context, this returns the list of values associated
with an attribute.

In a scalar context, if the attribute's value consist of a single literal,
then that literal will be returned, otherwise undef will be returned.

For example, from a JobDescription called $d constructed from a
description file containing

    {
	executable => [ '/bin/echo' ],
	arguments  => [ 'hello', 'world' ]
    }

The following will hold:

    $executable = $d->executable()    # '/bin/echo'
    $arguments = $d->arguments()      # undef
    @executable = $d->executable()    # ('/bin/echo')
    @arguments = $d->arguments()      # ('hello', 'world')
    $not_present = $d->not_present()  # undef
    @not_present = $d->not_present()  # ()

To test for existence of a value:

    @not_present = $d->not_present()
    print "Not defined\n" if(!defined($not_present[0]));

=cut

sub AUTOLOAD
{
    use vars qw($AUTOLOAD);
    my $self = shift;
    my $name = $AUTOLOAD;
    $name =~ s/.*://;

    $name =~ s/_//g;
    $name = lc($name);


    if((! ref($self)) ||(! exists($self->{$name})))
    {
	if(wantarray)
	{
	    return ();
	}
	else
	{
	    return undef;
	}
    }
    if(wantarray)
    {
	# Return a list containing the contents of the value array for
	# this attribute.
	# This makes things like $description->environment() act as expected.
	return @{$self->{$name}};
    }
    elsif(scalar(@{$self->{$name}}) == 1 && !ref($self->{$name}[0]))
    {
	# If there is only a single value in the value array for this
	# attribute, return that value
	# This makes things like $description->directory() act as expected.
	return @{$self->{$name}}[0];
    }
    else
    {
	return undef;
    }
}
1;

__END__

=back

=cut
