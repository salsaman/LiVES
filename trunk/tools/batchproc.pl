#!/usr/bin/perl
# (c) Salsaman 2009

# released under the GPL 3 or later
# see file COPYING or www.gnu.org for details

#batch process each clip


# syntax is batchproc.pl cmd

# e.g. batchproc "/clip/encode_as,/home/user/file\$clip.mpg"


# $clip is the clip number being processed (1 - n)

use IO::Socket::UNIX;


$remote_host="localhost";
$remote_port=9999; #command port to app
$local_port=9998; #status port from app
$monitor_port=9997; #monitor port from app

if (defined($ARGV[0])) {
    $cmd=$ARGV[0];
}

$sendOMC="sendOSC -h $remote_host $remote_port";


###################
# ready our listener
use IO::Socket;
use IO::Select;

my $s1=new IO::Select;
my $s2=new IO::Select;

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
$s1->add($ip1);


my $ip2=IO::Socket::INET->new(LocalPort => $monitor_port, Proto=>'udp',
        LocalAddr => $my_ip_addr)
    or die "error creating UDP listener for $my_ip_addr  $@\n";
$s2->add($ip2);


$timeout=1;



#################################################################
# start sending OMC commands

`$sendOMC /lives/open_status_socket,$my_ip_addr,$local_port`;

my $retmsg;

do {
    $retmsg=&get_newmsg;
} while (!($retmsg eq ""));


`$sendOMC /lives/ping`;

$retmsg=&get_newmsg;

unless ($retmsg eq "pong") {
    print "Could not connect to LiVES\n";
    #exit 2;
}

`$sendOMC /lives/open_notify_socket,$my_ip_addr,$monitor_port`;


do {
    $retmsg=&get_newmsg;
} while (!($retmsg eq ""));


# get number of clips
`$sendOMC /clip/count`;

$numclips=&get_newmsg;

print "LiVES has $numclips clips open !\n";

for ($clip=1;$clip<=$numclips;$clip++) {
    print "switch to clip $clip\n";

    `$sendOMC /clip/select,$clip`;

    $cmd2=eval join $cmd, 'qq(', ')';

    print "command is: $cmd2\n";

    `$sendOMC $cmd2`;

    do {
	$retmsg=(&get_newmon)+0;
    } while ($retmsg!=512&&!$retmsg!=1024&&$retmsg!=2048&&!$retmsg!=64);

    if ($retmsg==64||$retmsg==2048) {
	exit 1;
    }
}




exit 0;

#####################################################################





sub get_newmsg {
    my $newmsg;
    foreach $server($s1->can_read($timeout)){
	$server->recv($newmsg,1024);
	my ($rport,$ripaddr) = sockaddr_in($server->peername);
	
	# TODO - check from address is our host
	#print "FROM : ".inet_ntoa($ripaddr)."($rport)  ";
	
	last;
    }
    chomp ($newmsg);
    return $newmsg;
}



sub get_newmon {
    my $newmsg;
    foreach $server($s2->can_read($timeout)){
	$server->recv($newmsg,1024);
	my ($rport,$ripaddr) = sockaddr_in($server->peername);
	
	# TODO - check from address is our host
	#print "FROM : ".inet_ntoa($ripaddr)."($rport)  ";
	
	last;
    }
    chomp ($newmsg);
    return $newmsg;
}
