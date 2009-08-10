#!/usr/bin/perl

# released under the GPL 3 or later
# see file COPYING or www.gnu.org for details

$builtin_dir.="$ARGV[0]/$ARGV[1]/share/lives/plugins/effects/RFXscripts/";
print "    adding script files from $builtin_dir\n";

open OUT,"> POTFILES_PLUGINS";
opendir DIR,$builtin_dir;
while ($file=readdir(DIR)) {
    unless ($file =~ /^\./) {
	print OUT "$builtin_dir$file\n";
    }
}
closedir DIR;
close OUT;

print "done.\n";
