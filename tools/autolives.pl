#!/usr/bin/perl

# (c) Salsaman (salsaman+lives@gmail.com) 2004 - 2019

# released under the GPL 3 or later
# see file COPYING or www.gnu.org for details

#Communicate with LiVES and do auto VJing

# syntax is autolives.pl host cmd_port status_port [options]
# e.g. autolives.pl localhost 49999 49998
# or just autolives.pl to use defaults 

#options can be any combination of:
# -waitforplay
# -omc <notify_port>
# -time <secs> (ignored if -omc is passed)
# -mute (Actually set the audio source ot External)
# -loop (set continuous looping, ignored if -waitforplay is passed)
# -debug

$remote_host="localhost";
$remote_port=49999; #command port to app
$local_port=49998; #status port from app
$ctime = 1; # default change time
$mute = 0;
$loop = 0;
$DEBUG = 0;

$maxfx = 9; # e.g set to 7 if you want video gens and transitions to persist
$allow_fxchanges = 1;
$allow_clipswitch = 1;
$allow_fgbgswap = 1;
$allow_dirchange = 1;

$play_dir = 0; ## assume forwards

$timeout = 20;

if (defined($ARGV[0])) {
    $remote_host=$ARGV[0];
}

if (defined($ARGV[1])) {
    $remote_port=$ARGV[1];
}

if (defined($ARGV[2])) {
    $local_port=$ARGV[2];
}

if ($^O eq "MSWin32" || $^O eq "msys") {
    $is_mingw = 1;
    $sendOMC="MSYS2_ARG_CONV_EXCL=\"*\" sendOSC -h $remote_host $remote_port >/dev/null 2>&1";
}
else {
    $sendOMC="sendOSC -h $remote_host $remote_port";
}

while (($opt = shift) ne "") {
    if ($opt eq "-omc") {
	$noty = 1;
	$notify_port = shift;
    }
    if ($opt eq "-time") {
	$ctime = shift;
    }
    if ($opt eq "-mute") {
	$mute = 1;
    }
    if ($opt eq "-waitforplay") {
	$waitforplay = 1;
    }
    if ($opt eq "-loop") {
	$loop = 1;
    }
    if ($opt eq "-debug") {
	$DEBUG = 1;
    }
}

if ($is_mingw) {
    use IO::Socket::INET;
}
else {
    $SIG{'HUP'} = 'HUP_handler';

    if (&location("sendOSC") eq "") {
	if ($DEBUG) {print STDERR "You must have sendOSC installed to run this.\n";}
	exit 1;
    }

    use IO::Socket::UNIX;
}

if ($DEBUG) {print STDERR "autolives starting...\n";}

###################
# ready our listener
use IO::Socket;
use IO::Select;

my $s=new IO::Select;

chop($hostname = `hostname`);  
(undef,undef,undef,undef,$myaddr) = gethostbyname($hostname);

@my_ip = unpack("C4", $myaddr);
$my_ip_addr  = join(".", @my_ip);

if ($remote_host eq "localhost") {
    $my_ip_addr="localhost";
}

if ($DEBUG) {print STDERR "Opening status port UDP $local_port on $my_ip_addr...\n";}

if ($ip1=IO::Socket::INET->new(LocalPort => $local_port, Proto=>'udp',
			       LocalAddr => $my_ip_addr)) {
    $s->add($ip1);
    if ($DEBUG) {print STDERR "Status port ready.\n";}
}
else {
    die "error creating UDP status socket for $my_ip_addr  $@\n";
}

if ($noty) {
    if ($DEBUG) {print STDERR "Opening notify port UDP $local_port on $my_ip_addr...\n";}
    if ($ip2 = IO::Socket::INET->new(LocalPort => $notify_port, Proto=>'udp',
				     LocalAddr => $my_ip_addr)) {
	$s->add($ip2);
    }
    else {
	print STDERR "error creating notify socket for $my_ip_addr  $@\n";
    }
}

#################################################################
# start sending OMC commands

if ($DEBUG) {print STDERR "Beginning OMC handshake...\n";}

&send_command("/lives/open_status_socket,$my_ip_addr,$local_port");

if ($DEBUG) {print STDERR "Sent request to open status socket. Sending ping.\n";}

&send_command("/lives/ping");
my $retmsg=&get_newmsg;

if ($DEBUG) {print STDERR "got $retmsg\n";}

unless ($retmsg eq "pong") {
    if ($DEBUG) {print STDERR "Could not connect to LiVES\n";}
    exit 2;
}

if ($allow_fxchanges) {
    # get number of realtime effect keys
    &send_command("/effect_key/count");

    my $numeffectkeys=&get_newmsg;

    if ($DEBUG) {print STDERR "LiVES has $numeffectkeys realtime keys !\n";}

    if ($numeffectkeys > $maxfx) {
	$numeffectkeys = $maxfx;
	if ($DEBUG) {print STDERR "only messing with $numeffectkeys\n";}
    }
    
    if ($DEBUG) {print STDERR "getting effect key layout...\n";}

    # get number of keymodes

    &send_command("/effect_key/maxmode/get");

    $nummodes=&get_newmsg;

    if ($DEBUG) {
	print STDERR "there are $nummodes modes per key\n";
	&print_layout($numeffectkeys,$nummodes);
	print STDERR "done !\n";
    }
}

# get number of clips
&send_command("/clip/count");

$numclips=&get_newmsg;

if ($DEBUG) {print STDERR "LiVES has $numclips clips open !\n";}

if (!$numclips) {
    print STDERR "Please open some clips first !\n";
    exit 3;
}

if ($noty) {
    &send_command("/lives/open_notify_socket,$my_ip_addr,$notify_port");
}

# get some constants we need
&send_command("/lives/constant/value/get,LIVES_STATUS_PLAYING");

$playstat=&get_newmsg;

if ($loop) {
    &send_command("/lives/constant/value/get,LIVES_LOOP_MODE_CONTINUOUS");

    $loopconst=&get_newmsg;

    # get current loop mode
    &send_command("/video/loop/get");
    $currloop=&get_newmsg;
}

if ($mute) {
    #mute the sound if requested
    &send_command("/audio/source/get");
    $mute = 1 - &get_newmsg;
    if ($mute) {
	&send_command("/audio/source/set,1");
    }
}

if (!$waitforplay) {
    #trigger playback
    if ($loop) {
	&send_command("/video/loop/set,$loopconst");
    }

    &send_command("/video/play");

    #wait $timeout seconds for playback to start, else bail
    for ($i=0; $i<$timeout; $i++) {
	&send_command("/lives/status/get");
	$status=&get_newmsg;
	if ($status != $playstat) {
	    sleep 1;
	}
	else {
	    last;
	}
    }

    if ($status != $playstat) {
	if ($DEBUG) {print STDERR "Playback not started, status was $status, wanted $playstat (playing)\n";}
	&finish;
	exit 4;
    }

    if ($DEBUG) {print STDERR "Status is $status (playing)\n";}
}

if ($DEBUG) {print STDERR "Performing magic...\n";}

my $reselect = 0;

while (1) {
    if ($noty) {
	for ($ii=0;$ii<4;$ii++) {
	    &get_notify;
	}
    }
    else {
	if (!$reselect) {
	    select(undef, undef, undef, $ctime);
	}
    }

    $reselect = 0;

    if ($waitforplay) {
	&send_command("/lives/status/get");
	$status=&get_newmsg;
	if ($status != $playstat) {
	    sleep 1;
	    next;
	}
    }

    $action=int(rand(20))+1;
    if ($DEBUG) {print STDERR "Action is $action $action\n"};
    if ($action<6) {
	# 1,2,3,4,5
	# random clip switch
	if ($allow_clipswitch) {
	    $nextclip=int(rand($numclips)+1);
	    &send_command("/clip/select,$nextclip");
	}
	else {
	    $reselect = 1;
	}
    }
    elsif ($action<15) {
	# mess with effects
	if ($allow_fxchanges) {
	    $nexteffectkey=int(rand($numeffectkeys)+1);
	    if ($action<10) {
		# 6,7,8,9
		if ($nexteffectkey!=$key_to_avoid) {
		    &send_command("/effect_key/disable,$nexteffectkey");
		}
	    }
	    elsif ($action<13) {
		#10,11,12
		if ($nexteffectkey!=$key_to_avoid) {
		    &send_command("/effect_key/enable,$nexteffectkey");
		}
	    }
	    else {
		#13,14,15,16
		if ($nexteffectkey!=$key_to_avoid) {
		    send_command("/effect_key/maxmode/get,$nexteffectkey");
		    
		    $maxmode=&get_newmsg;
		    $newmode=int(rand($maxmode))+1;

		    &send_command("/effect_key/mode/set,$nexteffectkey,$newmode");
		}
	    }
	}
	else {
	    $reselect = 1;
	}
    }
    elsif ($action==17) {
	if ($allow_fgbgswap) {
	    &send_command("/clip/foreground/background/swap");
	}
	else {
	    $reselect = 1;
	}
    }
    else {
	#18, #19, #20
	if ($allow_dirchange) {
	    if ($play_reversed || $action == 18) {
		#make flipping from backwards to forwards more likely
		# so that we actually make some progress in the clip
		$play_reversed = !$play_reversed;
		&send_command("/video/play/reverse/soft");
	    }
	}
	else {
	    $reselect = 1;
	}
    }

    if (!$reselect && !$waitforplay) {
	send_command("/lives/status/get");
	$status=&get_newmsg;
	if ($status != $playstat) {
	    &finish;
	    #playback stopped
	    if ($DEBUG) {print STDERR "playback stopped, exiting\n";}
	    last;
	}
    }
}

exit 0;


#####################################################################
sub send_command {
    if ($DEBUG) {print STDERR "Autolives sending: @_\n";}
    my ($command)="$sendOMC @_";
    if (!$is_mingw) {
	`$command`;
    }
    else {
	system("bash.exe", "-l", "-c", $command);
    }
}


sub finish {
    if ($loop) {
	#reset loop mode
	&send_command("/video/loop/set,$currloop");
    }

    # reset mute status
    if ($mute) {
	&send_command("/audio/source/set,0");
	$mute = 0;
    }
}


sub get_newmsg {
    my $newmsg;
    while (1) {
	$lport = -1;
	foreach $server($s->can_read($timeout)){
	    $server->recv($newmsg,1024);
	    ($rport,$ripaddr) = sockaddr_in($server->peername);
	    ($lport,$lipaddr) = sockaddr_in($server->sockname);
	    if ($DEBUG) {print STDERR "check $lport $local_port\n";}
	    last if ($lport == $local_port);
	    # TODO - check from address is our host
	    if ($DEBUG) {print STDERR "FROM : ".inet_ntoa($ripaddr)."($rport)  "};
	}
	#if ($DEBUG) {print STDERR "OK $lport $local_port : $newmsg\n";}
	last if ($lport == $local_port || $lport == -1);
    }
    # remove terminating NULL
    $newmsg=substr($newmsg,0,length($newmsg)-1);
    chomp ($newmsg);
    return $newmsg;
}


sub get_notify {
    my $newmsg;
    while (1) {
	foreach $server($s->can_read()){
	    $server->recv($newmsg,1024);
	    ($rport,$ripaddr) = sockaddr_in($server->peername);
	    ($lport,$lipaddr) = sockaddr_in($server->sockname);
	    #if ($DEBUG) {print STDERR "check $lport $notify_port $newmsg\n";}
	    last if ($lport == $notify_port);
	}
	last if ($lport == $notify_port && $newmsg =~ /^32768\|(.*)/);
    }
    # remove terminating NULL
    $newmsg=substr($newmsg,0,length($newmsg)-1);
    chomp ($newmsg);
    #if ($DEBUG) {print STDERR "notify message = $newmsg\n";}
    return $newmsg;
}


sub location {
    # return the location of an executable
    my ($command)=@_;
    my ($location)=`which $command 2>/dev/null`;
    chomp($location);
    $location;
}


sub print_layout {
    my ($keys,$modes)=@_;
    my ($i,$j);
    for ($i=1;$i<=$keys;$i++) {
	for ($j=1;$j<=$modes;$j++) {
	    &send_command("/effect_key/name/get,$i,$j");
	    $name=&get_newmsg;
	    unless ($name eq "") {
		if ($DEBUG) {print STDERR "key $i, mode $j: $name    ";}
		if ($name eq "infinite"||$name eq "jess"||$name eq "oinksie") {
		    # avoid libvisual for now
		    $key_to_avoid=$i;
		}
	    }
	}
	if ($DEBUG) {print STDERR "\n";}
    }
}


sub HUP_handler {
    &finish;

    # send error message to log file.
    if ($DEBUG) {print STDERR "autolives.pl exiting now\n";}

    exit(0);
}

