awk '$1=="1" {print $2 "\t" $3}' logudp.dat > flow1.dat;
awk '$1=="2" {print $2 "\t" $3}' logudp.dat > flow2.dat;
awk '$1=="3" {print $2 "\t" $3}' logudp.dat > flow3.dat;
awk '$1=="1" {print $2 "\t" $3}' logtcp.dat > tlow1.dat;
awk '$1=="2" {print $2 "\t" $3}' logtcp.dat > tlow2.dat;
awk '$1=="3" {print $2 "\t" $3}' logtcp.dat > tlow3.dat;
