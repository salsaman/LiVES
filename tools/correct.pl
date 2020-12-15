#!/usr/bin/perl

## code to generate patch files from codespell output
## (c) G. Finch (salsaman@gmail.com) 2020

## Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

## 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

## 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

## 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;
use warnings;

my $filenew = "";

$SIG{INT}  = \&signal_handler;
$SIG{TERM} = \&signal_handler;

my $auto = 0; ## set non-zero to avoid (Y/n) prompts

my $fuzz = 0; ## fuzziness for line number matching (+- $fuzz)
my $no_patch = 0; ## set to non-zero to not create output patch file

my $ppat = "part.patch"; ## temp patch file
my $pat = "codespell.patch"; ## output patch file (in pwd)

#################################################

if (!defined($ARGV[0])) {
    print "Usage: correct.pl textfile path_to_toplevel\n";
    exit 0;
}

my $done = 0;
my $missed = 0;
my $ignored = 0;
my @notfound;

if (!$no_patch) {
    unlink $pat;
    open OUT2, ">", $pat;
}

open IN, "<", $ARGV[0];

while (<IN>) {
    my $line = $_;
    chomp($line);
    my @parts = (split /\//, $line);
    shift(@parts);
    my @bits = (split /\:/, join("/", @parts));
    my $file = "$ARGV[1]/$bits[0]";
    my $tline = 0 + $bits[1];
    my @corn = (split / ==> /, $bits[2]);
    my @calts = (split /, /, $corn[1]); 
    $corn[0] = substr($corn[0], 1, length($corn[0]));

    $filenew = "$file.new";
    while (1) {
	last if !-e $filenew;
	$filenew .= ".new";
    }
    print "\n\nFile to patch is $file\n";
    print "At line $tline: $corn[0] should be $corn[1]\n";

    open IN2, "<", $file;
    open OUT, ">", $filenew;

    my $count = 0;
    my $found = 0;
    my $repstr;
    my $npos = scalar(@calts);
    my $ignore = 0;

    while (<IN2>) {
	$count++;
	my $line = $_;
	if (!$found && $count >= $tline - $fuzz && $count <= $tline + $fuzz) {
	    if ($line =~ /(.*)$corn[0](.*)/) {
		if ($count != $tline) {
		    print "FOUND with offset " . ($count - $tline) . "\n\n";
		}
		print "Original:\n$line";
		if ($npos > 1) {
		    print "\nAlternatives found; please select from the following options:\n";
		    print "0: -leave unchanged-\n";
		    for (my $i = 1; $i <= $npos; $i++) {
			print "$i: $1$calts[$i - 1]$2\n";
		    }
		    while (1) {
			my $reply = <STDIN>;
			chomp $reply;
			if ((!$reply && $reply ne "0") || $reply > $npos) {
			    print "Invalid choice, please try again\n";
			    next;
			}
			if ($reply eq "0") {
			    $ignore = 1;
			    print "Skipped\n";
			}
			else {
			    $repstr = "$1$calts[$reply - 1]$2";
			}
			last;
		    }
		}
		else {
		    $repstr = "$1$corn[1]$2";
		    print "Proposed:\n$repstr\n";
		    if (!$auto) {
			print "\nShall I append this to the patch ? (Y/n)\n";
			while (1) {
			    my $reply = <STDIN>;
			    chomp $reply;
			    if ($reply eq "y") {
				$reply = "";
			    }
			    if ($reply eq "n") {
				$ignore = 1;
				print "Skipped\n";
			    }
			    last if $reply eq "" || $reply eq "n";
			    print "Please answer 'y' or 'n'\n";
			}
		    }
		}
		last if $ignore;
		$found = 1;
		$done++;
		print OUT "$repstr\n";
		next;
	    }
	}
	print OUT $line;
    }
    close IN2;
    close OUT;
    if ($ignore) {
	$ignored++;
    }
    elsif (!$found) {
	print "NOT FOUND\n\n";
	$missed++;
	push (@notfound, "$file:$tline:$corn[0]");
    }
    elsif (!$no_patch) {
	#create a patch, then adjust first 2 lines
	if (-e $ppat) {
	    unlink $ppat;
	}
	print "appending to patch\n";
	my $com = "diff -u $file $filenew > $ppat";
	system($com);
	print OUT2 "--- a/$bits[0]\n";
	print OUT2 "+++ b/$bits[0]\n";
	open IN2, "<", $ppat;
	my $skip = 2;
	while (<IN2>) {
	    next if $skip-- > 0;
	    print OUT2 $_;
	}
	close IN2;
	unlink $ppat;
	print OUT2 "\n";
    }
    unlink $filenew;
    $filenew = "";
}

if (!$no_patch) {
    close OUT2;
}
close IN;

print "Corrected: $done, not found: $missed, ignored $ignored\n";

foreach (@notfound) {
    print "Failed to find $_\n";
}

if (!$no_patch) {
    print "\nPatch file written as $pat\n";
}

sub signal_handler {
    if ($filenew && -e $filenew) {
	unlink $filenew;
    }
    die "\n";
}
