#!/usr/bin/perl

# perl program to take audio output from sox audio, convert it to the 
# specified format, and output it to stdout

# (C) G. Finch 2011 - released under GPL v3 or higher


if (!defined($ARGV[0])) {exit 1;}

my $command=$ARGV[0];


if ($command eq "get_formats") {
    # list of formats separated by |
    # see lives/src/plugins.h for values
    print "";
    exit 0;
}
