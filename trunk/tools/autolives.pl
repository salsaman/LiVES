#!/usr/bin/perl

# (c) Salsaman (salsaman+lives@gmail.com) 2004 - 2018

# released under the GPL 3 or later
# see file COPYING or www.gnu.org for details

#Communicate with LiVES and do auto VJing


# syntax is autolives.pl host cmd_port status_port [options]
# e.g. autolives.pl localhost 49999 49998
# or just autolives.pl to use defaults 

#options can be any combination of:
# -waitforplay
# -omc notify_port
# -debug


$remote_host="localhost";
$remote_port=49999; #command port to app
$local_port=49998; #status port from app

if (defined($ARGV[0])) {
    $remote_host=$ARGV[0];
}

if (defined($ARGV[1])) {
    $remote_port=$ARGV[1];
}

if (defined($ARGV[2])) {
    $local_port=$ARGV[2];
}

if ($^O eq "MSWin32") {
    $sendOMC="perl sendOSC.pl $remote_host $remote_port";
}
else {
    $sendOMC="sendOSC -h $remote_host $remote_port";
}

while (($opt = shift) ne "") {
    if ($opt eq "-omc") {
	$noty = 1;
	$notify_port = shift;
    }
    if ($opt eq "-waitforplay") {
	$waitforplay = 1;
    }
    if ($opt eq "-debug") {
	$DEBUG = 1;
    }
}


if ($^O eq "MSWin32") {
    use IO::Socket::INET;
}
else {
    $SIG{'HUP'} = 'HUP_handler';

    if (&location("sendOSC") eq "") {
	print STDERR "You must have sendOSC installed to run this.\n";
	exit 1;
    }

    use IO::Socket::UNIX;
}


print STDERR "autolives starting...\n";

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

#$noty = 1;

print STDERR "Opening status port UDP $local_port on $my_ip_addr...\n";


my $ip1=IO::Socket::INET->new(LocalPort => $local_port, Proto=>'udp',
        LocalAddr => $my_ip_addr)
    or die "error creating UDP listener for $my_ip_addr  $@\n";
$s->add($ip1);

print STDERR "Status port ready.\n";


if ($noty) {
    print STDERR "Opening notify port UDP $local_port on $my_ip_addr...\n";
    my $ip2=IO::Socket::INET->new(LocalPort => $notify_port, Proto=>'udp',
				  LocalAddr => $my_ip_addr)
	or die "error creating UDP notification listener for $my_ip_addr  $@\n";
    $s->add($ip2);
}


$timeout=1;



#################################################################
# start sending OMC commands

print STDERR "Beginning OMC handshake...\n";

if ($^O eq "MSWin32") {
    `$sendOMC /lives/open_status_socket s $my_ip_addr i $local_port`;
}
else {
    `$sendOMC /lives/open_status_socket,$my_ip_addr,$local_port`;
}


print STDERR "Sent request to open status socket. Sending ping.\n";

`$sendOMC /lives/ping`;
my $retmsg=&get_newmsg;

print STDERR "got $retmsg\n";

unless ($retmsg eq "pong") {
    print STDERR "Could not connect to LiVES\n";
    exit 2;
}


# get number of realtime effect keys, print STDERR and exit
`$sendOMC /effect_key/count`;

my $numeffectkeys=&get_newmsg;


print STDERR "LiVES has $numeffectkeys realtime keys !\n";

print STDERR "getting effect key layout...\n";



`$sendOMC /effect_key/maxmode/get`;

$nummodes=&get_newmsg;


print STDERR "there are $nummodes modes per key\n";


&print_layout($numeffectkeys,$nummodes);


print STDERR "done !\n";

# get number of clips, print STDERR and exit
`$sendOMC /clip/count`;


$numclips=&get_newmsg;


print STDERR "LiVES has $numclips clips open !\n";

if (!$numclips) {
    print STDERR "Please open some clips first !\n";
    exit 3;
}

if ($noty) {
    if ($^O eq "MSWin32") {
	`$sendOMC /lives/open_notify_socket s $my_ip_addr i $notify_port`;
    }
    else {
	`$sendOMC /lives/open_notify_socket,$my_ip_addr,$notify_port`;
    }
}

if ($^O eq "MSWin32") {
    `$sendOMC /lives/constant/value/get LIVES_STATUS_PLAYING`;
}
else {
    `$sendOMC /lives/constant/value/get,LIVES_STATUS_PLAYING`;
}


$playstat=&get_newmsg;

if (!$waitforplay) {
    `$sendOMC /video/play`;
    
    for ($i=0;$i<5;$i++) {
	`$sendOMC /lives/status/get`;
	$status=&get_newmsg;
	if ($status != $playstat) {
	    sleep 1;
	}
	else {
	    last;
	}
    }

    if ($status != $playstat) {
	print STDERR "Playback not started, status was $status, wanted $playstat (playing)\n";
	exit 4;
    }

    print STDERR "Status is $status (playing)\n";
}

$waittime = 1.;

print STDERR "Performing magic...\n";

while (1) {
    if ($noty) {
	for ($ii=0;$ii<4;$ii++) {
	    get_notify();
	}
    }
    else {
	select(undef, undef, undef, $waittime);
    }
    
    $action=int(rand(18))+1;
    
    if ($action<6) {
	# 1,2,3,4,5
	# random clip switch
	$nextclip=int(rand($numclips)+1);
	if ($^O eq "MSWin32") {
	    `$sendOMC /clip/select i $nextclip`;
	} else {
	    `$sendOMC /clip/select,$nextclip`;
	}
    }
    elsif ($action<15) {
	# mess with effects
	$nexteffectkey=int(rand($numeffectkeys)+1);
	`$sendOMC /clip/select,$nextclip`;
	if ($action<11) {
	    # 6,7,8,9,10
	    if ($nexteffectkey!=$key_to_avoid) {
		`$sendOMC /effect_key/disable,$nexteffectkey`;
		if ($^O eq "MSWin32") {
		    `$sendOMC /effect_key/disable i $nexteffectkey`;
		} else {
		    `$sendOMC /effect_key/disable,$nexteffectkey`;
		}
	    }
	}
	elsif ($action<13) {
	    #11,12
	    if ($nexteffectkey!=$key_to_avoid) {
		if ($^O eq "MSWin32") {
		    `$sendOMC /effect_key/enable i $nexteffectkey`;
		} else {
		    `$sendOMC /effect_key/enable,$nexteffectkey`;
		}
	    }
	}
	else {
	    #13,14,15,16
	    if ($nexteffectkey!=$key_to_avoid) {
		if ($^O eq "MSWin32") {
		    `$sendOMC /effect_key/maxmode/get i $nexteffectkey`;
		} else {
		    `$sendOMC /effect_key/maxmode/get,$nexteffectkey`;
		}

		$maxmode=&get_newmsg;
		$newmode=int(rand($maxmode))+1;

		if ($^O eq "MSWin32") {
		    `$sendOMC /effect_key/mode/set i $nexteffectkey i $newmode`;
		} else {
		    `$sendOMC /effect_key/mode/set,$nexteffectkey,$newmode`;
		}
	    }
	}
    }
    elsif ($action==17) {
	`$sendOMC /clip/foreground/background/swap`;
    }

    else {
	#18
	    `$sendOMC /video/play/reverse`;
    }

    if (!$waitforplay) {
	`$sendOMC /lives/status/get`;
	$status=&get_newmsg;
	if ($status != $playstat) {
	    print STDERR "playback stopped, exiting\n";
	    last;
	}
    }
    else {
	while (1) {
	    `$sendOMC /lives/status/get`;
	    $status=&get_newmsg;
	    if ($status != $playstat) {
		sleep 1;
	    }
	    else {
		last;
	    }
	}
    }
	
}

exit 0;

#####################################################################


sub get_newmsg {
    my $newmsg;
    while (1) {
	$lport = -1;
	foreach $server($s->can_read($timeout)){
	    $server->recv($newmsg,1024);
	    ($rport,$ripaddr) = sockaddr_in($server->peername);
	    ($lport,$lipaddr) = sockaddr_in($server->sockname);
	    #print STDERR "check $lport $local_port\n";

	    next if ($lport != $local_port);
	
	    # TODO - check from address is our host
	    #print STDERR "FROM : ".inet_ntoa($ripaddr)."($rport)  ";
	
	    last;
	}
	#print STDERR "OK $lport $local_port : $newmsg\n";
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
	    #print STDERR "check $lport $notify_port $newmsg\n";

	    next if ($lport != $notify_port);
	
	    # TODO - check from address is our host
	    #print STDERR "FROM : ".inet_ntoa($ripaddr)."($rport)  ";
	
	    last;
	}
	last if ($lport == $notify_port && $newmsg =~ /^32768\|(.*)/);
    }
    # remove terminating NULL
    $newmsg=substr($newmsg,0,length($newmsg)-1);
    chomp ($newmsg);
    #print STDERR "notify message = $newmsg\n";
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
	    if ($^O eq "MSWin32") {
		`$sendOMC /effect_key/name/get i $i i $j`;
	    } else {
		`$sendOMC /effect_key/name/get,$i,$j`;
	    }
	    $name=&get_newmsg;
	    unless ($name eq "") {
		print STDERR "key $i, mode $j: $name    ";
		if ($name eq "infinite"||$name eq "jess"||$name eq "oinksie") {
		    # avoid libvisual for now
		    $key_to_avoid=$i;
		}
	    }
	}
	print STDERR "\n";
    }
}


sub HUP_handler {
    # close all files.

    # send error message to log file.
    print STDERR "autolives.pl exiting now\n";

    exit(0);
}
