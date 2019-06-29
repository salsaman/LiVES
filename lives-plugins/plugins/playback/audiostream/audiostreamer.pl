#!/usr/bin/perl

# perl program to take audio input from $infifo, convert it to the 
# specified format, and output it to $outfifo

# (C) G. Finch 2011 - released under GPL v3 or higher

use POSIX;     # Needed for setlocale()
setlocale(LC_NUMERIC, "C");

if (!defined($ARGV[0])) {exit 1;}

my $command=$ARGV[0];

if ($command eq "get_formats") {
    # list of formats we can handle, separated by |
    # see lives/src/plugins.h for values

    # unlike encoders, this is sent as a string e.g. "1|3|8"

    print "1|3";
    exit 0;
}

if ($command eq "check") {
    my $chkform=$ARGV[1];
    if ($chkform == 3) {
	#vorbis

	if (&location("oggenc") eq "" && &location("sox") eq "") {
	    print "\nFor audio encoding to the 'vorbis' format you need to have either\n'oggenc' or correctly configured 'sox' installed.\nPlease install either of these programs and try again.\n";
	    exit 1;
	}
	exit 0;
    }
    if ($chkform == 1) {
	if (&location("sox") eq "") {
	    print "\nFor audio encoding to the 'wav' format you need to have correctly configured 'sox' installed.\nPlease check your installation of sox and try again.\n";
	    exit 1;
	}
	exit 0;
    }

    print "\nUnknown audio format for streaming.\n";
    exit 1;
}

if ($command eq "play") {
    my $result;
    
    my $format=$ARGV[1];
    my $infifo=$ARGV[2];
    my $outfifo=$ARGV[3];
    my $arate=$ARGV[4];

    # TODO - channels, samps, signed, endian

    #create outfifo; host should have already created infifo
    system("mkfifo $outfifo");

    # do conversion
    # audio formats taken from lives/src/plugins.h
    if ($format==3) {
	#vorbis
	unless (`which oggenc 2>/dev/null` eq "") {
	    system("oggenc -r --ignorelength -R $arate -B 16 -C 2 -m 32 -M 256 -o $outfifo $infifo");
	}
	else {
	    system("sox -t raw -r $arate -s -L -b 16 -c 2 $infifo -t vorbis $outfifo");
	}
    }
    elsif ($format==1) {
	#pcm
	system("sox -t raw -r $arate -s -L -b 16 -c 2 $infifo -t wav $outfifo 2>/dev/null");
    }
    else {
	exit 2;
    }
}

if ($command eq "cleanup") {
    # do any cleanup
    my $format=$ARGV[1];
    my $outfifo=$ARGV[2];

    unlink $outfifo;
}

########################################33333333

sub location {
    # return the location of an executable
    my ($command)=shift;
    my ($location)=`which $command 2>/dev/null`;
    chomp($location);
    $location;
}
