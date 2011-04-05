#!/usr/bin/perl

# perl program to take audio output from pulseaudio, convert it to the 
# specified format, and output it to stdout

# (C) G. Finch 2011 - released under GPL v3 or higher

# first get the name of the audio device


if (!defined($ARGV[0])) {exit 1;}

my $command=$ARGV[0];



if ($command eq "play") {
    my $result;
    
    my $outfifo=$ARGV[1];
    my $oformat=$ARGV[2];

    my $arate=44100;

    system("pactl -l | grep \"Monitor Source:\" > /tmp/livesquery");
    if (defined(open IN,"</tmp/livesquery")) {
	read IN,$result,256;
    }
    else {
	exit 2;
    }
    
    close IN;
    unlink("/tmp/livesquery");

    my $device=(split(/: /,$result))[1];
    chomp($device);


# audio formats taken from lives/src/plugins.h
    if ($format==3) {
	#vorbis

	$com="pacat -r -d $device | sox -t raw -r $arate -s -L -b 16 -c 2 - -t vorbis $outfifo";

	system("mkfifo $outfifo");
	system($com);

    }
}

