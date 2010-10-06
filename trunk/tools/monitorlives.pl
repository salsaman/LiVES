#!/usr/bin/perl
# (c) Salsaman 2008

# released under the GPL 3 or later
# see file COPYING or www.gnu.org for details

# open notify port to LiVES and display notifications


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
$local_port=9997; #notify port from app

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


$timeout=0;



#################################################################
# start sending OMC commands

`$sendOMC /lives/open_notify_socket,$my_ip_addr,$local_port`;

while (1) {
    $retmsg=&get_newmsg;
    if ($retmsg!="") {
	print "got msg $retmsg\n";
    }
}





##################################################




sub get_newmsg {
    my $newmsg;
    foreach $server($s->can_read($timeout)){
	$server->recv($newmsg,1024);
	my ($rport,$ripaddr) = sockaddr_in($server->peername);
	
	# TODO - check from address is our host
	#print "FROM : ".inet_ntoa($ripaddr)."($rport)  ";
	
	last;
    }
    $newmsg=substr($newmsg,0,length($newmsg)-1);
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
