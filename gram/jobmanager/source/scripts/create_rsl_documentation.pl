#! /usr/bin/perl

%values = &read_input();
&print_html(%values);

sub print_html
{
    print <<EOF;
/**
\@page globus_job_manager_rsl RSL Attributes

This page contains a list of all RSL attributes which are supported
by the GRAM Job Manager.

EOF

    foreach(keys %values)
    {
	next if($values{$_}{Publish} eq "false");
	print <<EOF;

\@anchor globus_gram_rsl_attribute_$_
\@par $_
$values{$_}{Description}
EOF
    }
    print <<EOF;
*/
EOF
}

sub read_input
{
    my %result;
    my $record = "";

    while(<>)
    {
	s/#.*//;

	if($_ ne "\n")
	{
	    $record .= $_;
	}
	else
	{
	    &insert_record(\%result, $record);
	}
    }
    &insert_record(\%result, $record);

    return %result;
}

sub insert_record
{
    my $hash = shift;
    my $data = shift;
    my %result;
    my $attribute;
    my $value;
    my $in_multiline = 0;

    foreach (split(/\n/, $data))
    {
	if($in_multiline)
	{
	    $value .= $_;
	    if($value =~ m/[^\\]"/)
	    {
		$value =~ s/\s+/ /g;
		$in_multiline = 0;
		$value =~ s/\\"/"/g;
		$value =~ s/^"//;
		$value =~ s/"$//;
	    }
	    else
	    {
		next;
	    }
	}
	else
	{
	    ($attribute, $value) = split(/:/, $_, 2);

	    if($value =~ m/^\s*"/)
	    {
		# multiline value
		$in_multiline = 1;
	    }
	    $value =~ s/^\s*//;
	}
	$result{$attribute} = $value;
    }

    $attribute = $result{Attribute};

    foreach (keys %result)
    {
	$hash->{$attribute}{$_} = $result{$_};
    }
}
