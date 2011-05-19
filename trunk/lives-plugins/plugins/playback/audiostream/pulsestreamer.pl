#!/usr/bin/perl

# perl program to take audio output from pulseaudio, convert it to the 
# specified format, and output it to $outfifo

# (C) G. Finch 2011 - released under GPL v3 or higher


if (!defined($ARGV[0])) {exit 1;}

my $command=$ARGV[0];


if ($command eq "get_formats") {
    # list of formats separated by |
    # see lives/src/plugins.h for values
    print "3"; # vorbis
    exit 0;
}

if ($command eq "check") {
    my ($location)=`which pacat 2>/dev/null`;
    if ($location eq "") {
	print "Audio streaming with pulseaudio requires pacat\n";
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

    system("pactl list | grep \"Monitor Source:\" > /tmp/livesquery");
    if (defined(open IN,"</tmp/livesquery")) {
	read IN,$result,256;
    }
    else {
	exit 3; # general error
    }
    
    close IN;
    unlink("/tmp/livesquery");

    my $device=(split(/: /,$result))[1];
    chomp($device);

# audio formats taken from lives/src/plugins.h
    if ($format==3) {
	#vorbis

	$com="pacat --latency-msec=10 --process-time-msec=10 -r -d $device | sox -t raw -r $arate -s -L -b 16 -c 2 - -t vorbis $outfifo";
	system($com);

    }
}


if ($command eq "cleanup") {
    # do any cleanup
    unlink("/tmp/livesquery");


}
