#!/bin/bash
#
# Convert OpenSeaMap Logger $POSMSK Seatalk messages to NMEA
#
# Usage examples:
#   cp DATAtest.DAT DATA00.DAT
#   dos2unix -f DATA00.DAT
#   posmsk2nmea.sh DATA00.DAT > data.nmea
#
#   # convert all data in current directory
#   posmsk2nmea.sh DATA*.DAT > data.nmea
#   dos2unix data.nmea
#
#   # compress for upload to http://depth.openseamap.org
#   wd=`pwd`
#   newname=`basename $wd`.sk2nmea
#   mv data.nmea $newname
#   gzip $newname
# 
# Background:
#   - the two tools nmea_cksum2 and posmsk2nmea.sh are available from 
#     git clone osm.franken.de:/home/amerz/src/seatalk # or
#     git clone https://github.com/Github6am/logger-oseam-0183.git
#     the current version only extracts DBT messages
# 
#   - logger documentation
#     http://wkla.no-ip.biz/ArduinoWiki/doku.php?id=arduino:oseam
#
#   - upload instructions
#     http://depth.openseamap.org/#instructions
#
#   - awk documentation:
#     http://www.gnu.org/software/gawk/manual/gawk.html
#
#   - seatalk documentation:
#     http://www.thomasknauf.de/seatalk.htm
#
# Author:  Andreas Merz, 2015
# Licence: GPL

if [ "$1" = "-h" ] ; then
  head -n 39 $0 | cut -c 2-
  exit
fi

cmd=cat

$cmd $* | nmea_cksum2 | grep -v '#' \
  | awk -F ';' ' 
   BEGIN      {
                sbnr=0;   # seatalk byte nr
              }
              {
                result=0;
              }
   /\$POSMSK/ { a3 = $3;
                # split typical input: $POSMSK,0042603A00*47
                b1 = gensub(/(^.*POSMSK,)([^*]*)\*(..)/, "\\1", 1, a3);
                b2 = gensub(/(^.*POSMSK,)([^*]*)\*(..)/, "\\2", 1, a3);
                b3 = gensub(/(^.*POSMSK,)([^*]*)\*(..)/, "\\3", 1, a3);
                #print b1;
                #print b2;
                #print b3;
                #print b4;
                result = sprintf("%s;%s;%s", $1, $2,  seaconvert(b2));
              }
              {
                print $0
                if(result) print result
              }
              
              
function seaconvert( msg )
{
  feet2m=3.2808;      # feet/m   12 inch = 1 foot  
  fathom2m=feet2m/6;  # fathom/m  6 foot = 1 fathom 
  
  c1=gensub(/(..)([^*]*)/, "\\1", 1, msg);  # header
  c2=gensub(/(..)([^*]*)/, "\\2", 1, msg);  # body
  split(c2, d, "", seps);
  switch(c1) {
    case "00": #print "DBT";                      # example 5.8ft: 0042603A00
               hex = sprintf("0x%s%s%s%s", d[7],d[8],d[5],d[6]);
               val = strtonum( hex );             # unit: feet/10
               vd= val/(10.0*feet2m);             # value in meters
	           result=sprintf("$IIDBT,%3.1lf,f,%4.2lf,M,%4.2lf,F",
	             vd*feet2m, vd, vd*fathom2m);
               break;
               
    case "23": #print "MTW"
               result = sprintf("#%s %s", c1, c2);
               break;
               
    default: 
               result = sprintf("#%s %s", c1, c2);
  }
  
  return result;
}        
' | grep -v '#' | nmea_cksum2 -a




