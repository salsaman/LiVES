#!/usr/bin/perl

#script to test all realtime effect plugins and parameters

# syntax is lives-rte-test.pl host cmd_port status_port
# e.g. lives-rte-test.pl localhost 49999 49998
# or just lives-rte-test.pl to use defaults 

if (&location("sendOSC") eq "") {
    print "You must have sendOSC installed to run this.\n";
    exit 1;
}


use IO::Socket::UNIX;


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


$timeout=10;



#################################################################
# start sending OMC commands

`$sendOMC /lives/open_status_socket,$my_ip_addr,$local_port`;

`$sendOMC /lives/ping`;
my $retmsg=&get_newmsg;

unless ($retmsg eq "pong") {
    print "Could not connect to LiVES; try starting it with: lives -oscstart $remote_port\n";
    exit 2;
}

#print "got $retmsg\n";

# reset and clear fx map
`$sendOMC /effect_key/reset`;
`$sendOMC /effect_key/map/clear`;


# play in loop cont mode
`$sendOMC /lives/constant/value/get,"LIVES_LOOP_CONT"`;

$loopcont=&get_newmsg;

`$sendOMC /video/loop/set,$loopcont`;
`$sendOMC /clip/select,1`;
`$sendOMC /video/play`;

$ready=0;

for ($j=0;;$j++) {
    # get next fx name
    `$sendOMC /effects/realtime/name/get,$j`;

    $retmsg=&get_newmsg;
    if ($retmsg eq "") {
	last;
    }
    else {
#	next if ($retmsg eq "compositorcompositor");

#	$ready=0;
#	if ($retmsg eq "colorkeycolour key") {
	    $ready=1;
#	}

	next if (!$ready);

	if (1||$retmsg=~ /^frei0rFrei0r:/) {
	    print "testing $retmsg\n";

	    # map to key 1 and enable it
	    `$sendOMC /effect_key/map/clear`;
	    `$sendOMC /effect_key/map,1,"$retmsg"`;
	    `$sendOMC /effect_key/enable,1`;
	    
	    #if trans, set bg
	    `$sendOMC /effect_key/inchannels/active/get,1`;
	    $nchans=&get_newmsg;
	    if ($nchans==2) {
		`$sendOMC /clip/background/select,2`;
	    }

	    #print("number of active in channels is $nchans\n");


	    #test each parameter in turn - get value, set to min, max, default
	    `$sendOMC /effect_key/parameter/count,1`;
	    $nparms=&get_newmsg;

	    #print "Effect has $nparms params\n";

	    if ($nparms>20) {
		$nparms=20;
	    }

#	    &sleep200;

	    for ($i=0;$i<$nparms;$i++) {
		`$sendOMC /effect_key/parameter/name/get,1,$i`;
		$pname=&get_newmsg;
		print ("Testing param $i, $pname\n");

		`$sendOMC /effect_key/parameter/type/get,1,$i`;
		$ptype=&get_newmsg;
		if ($ptype==1) {
		    $ptname="int";
		}
		if ($ptype==2) {
		    $ptname="float";
		}
		if ($ptype==3) {
		    $ptname="bool";
		}
		if ($ptype==4) {
		    $ptname="string";
		}
		if ($ptype==5) {
		    $ptname="colour";
		}

		print("param type is $ptname\n");

		`$sendOMC /effect_key/parameter/default/get,1,$i`;
		$pdef=&get_newmsg;

		print("default value is $pdef\n");

		# set to min, max, def; bool on/off; text "LiVES test"

		if ($ptype != 3 && $ptype != 4) {
		    `$sendOMC /effect_key/parameter/min/get,1,$i`;
		    $pmin=&get_newmsg;
		    
		    print("min value is $pmin\n");

		    `$sendOMC /effect_key/parameter/max/get,1,$i`;
		    $pmax=&get_newmsg;
		    
		    print("max value is $pmax\n");

		    if ($pdef<$pmin || $pdef > $pmax) {
			#print ("DEFAULT OUT OF RANGE ($pdef) $retmsg: $pname\n");
		    }


		    #set to min



		}

		#check nvalues in param
		`$sendOMC /effect_key/parameter/value/count,1,$i`;
		$pnvals=&get_newmsg;
		print("nvalues is $pnvals\n");

		if ($pnvals!=1) {
		    print "not testing ARRAY\n";
		    exit 0;
		    next;

		}


		if ($ptype==5) {
		    #colour
		    `$sendOMC /effect_key/parameter/colorspace/get,1,$i`;
		    $csp=&get_newmsg;
		    print("CSP is $csp\n");

		    $test=(split/,/,$pmin)[1];

		    if ($test eq "") {
			print "min needs expand\n";

			if ($csp==1) {
			    $pmin="$pmin,$pmin,$pmin";
			}
			else {
			    $pmin="$pmin,$pmin,$pmin,$pmin";
			}

		    }

		    $test=(split/,/,$pmax)[1];

		    if ($test eq "") {
			print "max needs expand\n";

			if ($csp==1) {
			    $pmax="$pmax,$pmax,$pmax";
			}
			else {
			    $pmax="$pmax,$pmax,$pmax,$pmax";
			}

		    }

		    $test=(split/,/,$pdef)[1];

		    if ($test eq "") {
			print "def needs expand\n";

			if ($csp==1) {
			    $pdef="$pdef,$pdef,$pdef";
			}
			else {
			    $pdef="$pdef,$pdef,$pdef,$pdef";
			}

		    }

		}


		# set to first value
		if ($ptype==3) {
		    #bool
		    $pmin=!$pdef;
		}

		if ($ptype==4) {
		    #string
		    $pmin="\"hello \\\"world\\\"!\"";
		    if ($pdef eq "") {
			$pdef="\" \"";
		    }
		}


		`$sendOMC /effect_key/parameter/value/set,1,$i,$pmin`;

		`$sendOMC /effect_key/parameter/value/get,1,$i`;
		$pval=&get_newmsg;
		    
		print("set to first value: $pval\n");


		# set to second value

		if ($ptype==1||$ptype==2||$ptype==5) {
		    `$sendOMC /effect_key/parameter/value/set,1,$i,$pmax`;
		    

		    `$sendOMC /effect_key/parameter/value/get,1,$i`;
		    $pval=&get_newmsg;
		    
		    print("set to second value: $pval\n");
		}




		# reset to def. value

		`$sendOMC /effect_key/parameter/value/set,1,$i,$pdef`;

		`$sendOMC /effect_key/parameter/value/get,1,$i`;
		$pval=&get_newmsg;
		    
		print("reset to def value: $pval\n");


		# sleep 0.2 seconds
	    }


	    #deactivate fx
	    `$sendOMC /effect_key/disable,1`;

	}
    }
}


#clear mapping
`$sendOMC /effect_key/map/clear`;

`$sendOMC /video/stop`;



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
    # remove terminating NULL
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


sub sleep200 {
# sleep for 200 ms
    select(undef, undef, undef, 2.);
}
