#!/usr/bin/perl

#randomly shuffle clips in a set

unless (defined $ARGV[0]) {
    print "Usage: set_shuffle.pl <setname>\n";
    exit 1;
}

my $tmpdir=`smogrify print_pref tempdir`;

my $file=$tmpdir."/$ARGV[0]/order";

open (FILE,"<$file") or die "Could not open set $ARGV[0]\n";

while (<FILE>) {
    chomp $_;
    push(@lines, $_);
}

close FILE;

for ($i = 0; $i<@lines; $i++) {
    $j = int rand (@lines);
    $tmp=$lines[$i];
    $lines[$i] = $lines[$j];
    $lines[$j]=$tmp;
}

unlink $file;

open (FILE,">$file");

foreach (@lines) {
    print FILE $_."\n";
}


close FILE;

exit 0;
