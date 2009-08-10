#!/usr/bin/perl
# (c) Salsaman 2004 - 2009

# released under the GPL 3 or later
# see file COPYING or www.gnu.org for details

#Communicate with LiVES and do auto VJing


# syntax is autolives.pl host cmd_port status_port
# e.g. autolives.pl localhost 9999 9998
# or just autolives.pl to use defaults 

if (&location("sendOSC") eq "") {
    print "You must have sendOSC installed to run this.\n";
    exit 1;
}


use IO::Socket::UNIX;


$remote_host="localhost";
$remote_port=9999; #command port to app
$local_port=9998; #status port from app

if (defined($ARGV[0])) {
    $remote_host=$ARGV[0];
}

if (defined($ARGV[1])) {
    $remote_port=$ARGV[1];
}

if (defined($ARGV[2])) {
    $local_port=$ARGV[2];
}

$sendOMC="sendOSC -h $remote_host $remote_port";


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


my $ip1=IO::Socket::INET->new(LocalPort => $local_port, Proto=>'udp',
        LocalAddr => $my_ip_addr)
    or die "error creating UDP listener for $my_ip_addr  $@\n";
$s->add($ip1);


$timeout=1;



#################################################################
# start sending OMC commands

`$sendOMC /lives/open_status_socket,$my_ip_addr,$local_port`;

`$sendOMC /lives/ping`;
my $retmsg=&get_newmsg;

unless ($retmsg eq "pong") {
    print "Could not connect to LiVES\n";
    exit 2;
}


# get number of realtime effect keys, print and exit
`$sendOMC /effect_key/count`;

my $numeffectkeys=&get_newmsg;


print "LiVES has $numeffectkeys realtime keys !\n";

print "getting effect key layout...\n";



`$sendOMC /effect_key/maxmode/get`;

$nummodes=&get_newmsg;


print "there are $nummodes modes per key\n";


&print_layout($numeffectkeys,$nummodes);


print "done !\n";

# get number of clips, print and exit
`$sendOMC /clip/count`;


$numclips=&get_newmsg;


print "LiVES has $numclips clips open !\n";

if (!$numclips) {
    print "Please open some clips first !\n";
    exit 3;
}


# TODO - check if app is playing

`$sendOMC /video/play`;


# switch clips randomly

while (1) {
    sleep 1;
    $action=int(rand(18))+1;
    
    if ($action<6) {
	# 1,2,3,4,5
	# random clip switch
	$nextclip=int(rand($numclips)+1);
	`$sendOMC /clip/select,$nextclip`;
    }
    elsif ($action<15) {
	# mess with effects
	$nexteffectkey=int(rand($numeffectkeys)+1);
	`$sendOMC /clip/select,$nextclip`;
	if ($action<11) {
	    # 6,7,8,9,10
	    if ($nexteffectkey!=$key_to_avoid) {
		`$sendOMC /effect_key/disable,$nexteffectkey`;
	    }
	}
	elsif ($action<13) {
	    #11,12
	    if ($nexteffectkey!=$key_to_avoid) {
		`$sendOMC /effect_key/enable,$nexteffectkey`;
	    }
	}
	else {
	    #13,14,15,16
	    if ($nexteffectkey!=$key_to_avoid) {
		`$sendOMC /effect_key/maxmode/get,$nexteffectkey`;
		$maxmode=&get_newmsg;
		$newmode=int(rand($maxmode))+1;
		`$sendOMC /effect_key/mode/set,$nexteffectkey,$newmode`;
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
	
}


exit 0;

#####################################################################





sub get_newmsg {
    my $newmsg;
    foreach $server($s->can_read($timeout)){
	$server->recv($newmsg,1024);
	my ($rport,$ripaddr) = sockaddr_in($server->peername);
	
	# TODO - check from address is our host
	#print "FROM : ".inet_ntoa($ripaddr)."($rport)  ";
	
	last;
    }
    chomp ($newmsg);
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
	    `$sendOMC /effect_key/name/get,$i,$j`;
	    $name=&get_newmsg;
	    unless ($name eq "") {
		print "key $i, mode $j: $name    ";
		if ($name eq "infinite"||$name eq "jess"||$name eq "oinksie") {
		    # avoid libvisual for now
		    $key_to_avoid=$i;
		}
	    }
	}
	print "\n";
    }
}
