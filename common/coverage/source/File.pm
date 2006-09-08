package Globus::Coverage::File;

use strict;
use Carp;
use Globus::Coverage;
use Globus::Coverage::Function;

@Globus::Coverage::File::ISA = qw(Globus::Coverage);

sub new
{
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self = $class->SUPER::new(@_);
    
    bless $self, $class;

    return $self;
}

sub statement_coverage
{
    my $self = shift;
    my $statements = shift;
    my $statements_reached = shift;
    my $coverage = [0, 0];

    if (defined($statements) && defined($statements_reached)) {
        $self->{STATEMENTS} = $statements;
        $self->{STATEMENTS_REACHED} = $statements_reached;
    }
    $coverage->[0] = $self->{STATEMENTS};
    $coverage->[1] = $self->{STATEMENTS_REACHED};
    $coverage->[2] = $self->percentage($coverage->[0], $coverage->[1]);

    return $coverage;
}

sub branch_coverage
{
    my $self = shift;
    my $branches = shift;
    my $branches_reached = shift;
    my $coverage = [0, 0];

    if (defined($branches) && defined($branches_reached)) {
        $self->{BRANCHES} = $branches;
        $self->{BRANCHES_REACHED} = $branches_reached;
    }
    $coverage->[0] = $self->{BRANCHES};
    $coverage->[1] = $self->{BRANCHES_REACHED};
    $coverage->[2] = $self->percentage($coverage->[0], $coverage->[1]);

    return $coverage;
}

sub source
{
    my $self = shift;
    my $source = shift;
    my $i = 0;

    if (defined($source)) {
        foreach my $line (split(/\n/, $source)) {
            $self->{SOURCE}->[$i++] = $line;
        }
    }

    return join(/\n/, @{$self->{SOURCE}});
}

sub lines
{
    my $self = shift;
    if (exists $self->{SOURCE}) {
        return scalar(@{$self->{SOURCE}});
    } else {
        return 0;
    }
}

sub line_coverage
{
    my $self = shift;
    my $line = shift;
    my $count = shift;

    if (defined($line) && defined($count)) {
        $self->{COUNT}->[$line-1] = $count;
    }

    return [$line, $self->{COUNT}->[$line-1], $self->{SOURCE}->[$line-1]];
}

sub function
{
    my $self = shift;
    my $name = shift;

    if (! exists($self->{FUNCTIONS}->{$name})) {
        $self->{FUNCTIONS}->{$name} = new Globus::Coverage::Function($name);
    }

    return $self->{FUNCTIONS}->{$name}
}

sub function_names
{
    my $self = shift;

    return keys %{$self->{FUNCTIONS}};
}

sub function_coverage
{
    my $self = shift;
    my $coverage = [0, 0];

    foreach my $fn ($self->function_names()) {
        my $funcinfo = $self->function($fn);

        $coverage->[0]++;
        if ($funcinfo->statement_coverage()->[1] != 0)
        {
            $coverage->[1]++;
        }
    }
    $coverage->[2] = $self->percentage($coverage->[0], $coverage->[1]);

    return $coverage;
}

1;
