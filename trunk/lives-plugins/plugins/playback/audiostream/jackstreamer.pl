#!/usr/bin/perl

# perl program to take audio output from jack audio, convert it to the 
# specified format, and output it to $outfifo

# (C) G. Finch 2011 - released under GPL v3 or higher


if (!defined($ARGV[0])) {exit 1;}

my $command=$ARGV[0];


if ($command eq "get_formats") {
    # list of formats separated by |
    # see lives/src/plugins.h for values
    print "3";
    exit 0;
}


if ($command eq "check") {
    my ($location)=`which jack-stdout 2>/dev/null`;
    if ($location eq "") {
	print "Audio streaming with jack requires jack-stdout\nSee: http://gareus.org/oss/jackstdio/start\n";
	exit 1;
    }
    exit 0;
}



if ($command eq "play") {
    my $result;
    
    my $format=$ARGV[1];
    my $outfifo=$ARGV[2];
    my $arate=$ARGV[3];

    if ($format!=3) {
	exit 2; # unsupported format
    }

# audio formats taken from lives/src/plugins.h
    if ($format==3) {
	#vorbis
	system("jack-stdout LiVES_audio_out:out_0 LiVES_audio_out:out_1 | sox -t raw -r $arate -s -L -b 16 -c 2 - -t vorbis $outfifo");
    }
}

if ($command eq "cleanup") {
    # do any cleanup

}
