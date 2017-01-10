#!/bin/bash

execname="Cfits"


tempfile=`tempfile 2>/dev/null` || tempfile=/tmp/test$$
trap "rm -f $tempfile" 0 1 2 5 15


LINES=$( tput lines )
COLUMNS=$( tput cols )
let " nbwlines = $LINES - 10 "
let " nbwcols = $COLUMNS - 10 "
echo "$COLUMNS -> $nbwcols"





LOOPNUMBER_file="LOOPNUMBER"
confnbfile="./conf/conf_CONFNUMBER.txt"


mkdir -p conf
mkdir -p status
mkdir -p tmp


LOOPNUMBER_default=2  # loop number

# LOOPNUMBER (loop number)
if [ ! -f $LOOPNUMBER_file ]
then
	echo "creating loop number"
	echo "$LOOPNUMBER_default" > $LOOPNUMBER_file
else
	LOOPNUMBER=$(cat $LOOPNUMBER_file)
	echo "LOOPNUMBER = $LOOPNUMBER"
fi




# ======================= LOGGING =================================
LOOPNAME=$( cat LOOPNAME )
echo "LOOPNAME = $LOOPNAME"
# internal log - logs EVERYTHING
function aoconflog {
echo "$@" >> aolconf.log
dolog "$LOOPNAME" "$@"
}

# external log, less verbose
function aoconflogext {
echo "$@" >> aolconf.log
dolog "$LOOPNAME" "$@"
dologext "$LOOPNAME $@"
}





function stringcenter {
line=$1
    let " col1 = $nbwcols-35"
    columns="$col1"
    string=$(printf "%*s%*s\n" $(( (${#line} + columns) / 2)) "$line" $(( (columns - ${#line}) / 2)) " ")
}






# DM DC level [um] for each Vmax setting

dmDCum025="0.0219"
dmDCum050="0.0875"
dmDCum075="0.1969"
dmDCum100="0.3500"
dmDCum125="0.5469"
dmDCum150="0.7857"




# Set DM Vmax 
function Set_dmVmax {
file="./status/status_dmVmax.txt"
currentfw=$(echo "$(cat $file)")
if [ ! "${current_dmVmax}" == "$1" ]
then
echo "CHANGING dmVmax to $1"# &> ${outmesg}

else
echo "WHEEL ALREADY SET TO"# > ${outmesg}
fi
sleep 0.1
currentfw=$1
echo "${currentfw}" > $file
sleep 0.1
}









state="menuhardwarecontrol"


while true; do

stateok=0

mkdir -p status


if [ $state = "menuhardwarecontrol" ]; then
stateok=1
menuname="HARDWARE CONTROL - LOOP ${LOOPNAME} ($LOOPNUMBER})\n"



file="./conf/conf_dmVmax.txt"
if [ -f $file ]; then
dmVmax=$(cat $file)
else
dmVmax="125"
echo "$dmVmax" > $file
fi

file="./conf/conf_dmDCum.txt"
if [ -f $file ]; then
dmDCum=$(cat $file)
else
dmDCum="125"
echo "$dmDCum" > $file
fi



stringcenter "DM control  [ current: Vmax = ${dmVmax} V  DC = ${dmDCum} um ]"
menuitems=( "1 ->" "\Zb\Zr$string\Zn" )
menuitems+=( "" "" )

menuitems+=( "" "" )
menuitems+=( "dmrs" "Re-start all DM processes" )
menuitems+=( "dmk" "Kill all DM processes - do not restart" )
menuitems+=( "dmcommrs" "Re-start scexao2 -> scexao DM communication processes" )
menuitems+=( "dmcommk" "Kill scexao2 -> scexao DM communication processes" )


menuitems+=( "" "" )
if [ "$dmVmax" = " 25" ]; then
menuitems+=( "dmVmax025" "\Zr\Z2 dmVmax =  25 V  (DC level = ${dmDCum025} um)\Zn" )
else
menuitems+=( "dmVmax025" " dmVmax =  25 V  (DC level = ${dmDCum025} um)" )
fi

if [ "$dmVmax" = " 50" ]; then
menuitems+=( "dmVmax050" "\Zr\Z2 dmVmax =  50 V  (DC level = ${dmDCum050} um)\Zn" )
else
menuitems+=( "dmVmax050" " dmVmax =  50 V  (DC level = ${dmDCum050} um)" )
fi

if [ "$dmVmax" = " 75" ]; then
menuitems+=( "dmVmax075" "\Zr\Z2 dmVmax =  75 V  (DC level = ${dmDCum075} um)\Zn" )
else
menuitems+=( "dmVmax075" " dmVmax =  75 V  (DC level = ${dmDCum075} um)" )
fi

if [ "$dmVmax" = "100" ]; then
menuitems+=( "dmVmax100" "\Zr\Z2 dmVmax = 100 V  (DC level = ${dmDCum100} um)\Zn" )
else
menuitems+=( "dmVmax100" " dmVmax = 100 V  (DC level = ${dmDCum100} um)" )
fi

if [ "$dmVmax" = "125" ]; then
menuitems+=( "dmVmax125" "\Zr\Z2 dmVmax = 125 V  (DC level = ${dmDCum125} um)\Zn" )
else
menuitems+=( "dmVmax125" " dmVmax = 125 V  (DC level = ${dmDCum125} um)" )
fi

if [ "$dmVmax" = "150" ]; then
menuitems+=( "dmVmax150" "\Zr\Z2 dmVmax = 150 V  (DC level = ${dmDCum150} um)\Zn" )
else
menuitems+=( "dmVmax150" " dmVmax = 150 V  (DC level = ${dmDCum150} um)" )
fi




menuitems+=( "" "" )




stringcenter "scexao2 streams"
menuitems+=( "2 ->" "\Zb\Zr$string\Zn" )
menuitems+=( " " " " )

menuitems+=( "ir1cs" "\Zr ircam1 \Zn : (re-)start scexao2->scexao TCP transfer" )
menuitems+=( "ir1ck" "\Zr ircam1 \Zn : kill scexao2->scexao TCP transfer" )
menuitems+=( " " " " )
menuitems+=( "ir1dcs" "\Zr ircam1_dark \Zn: (re-)start scexao2->scexao TCP transfer" )
menuitems+=( "ir1dck" "\Zr ircam1_dark \Zn: kill scexao2->scexao TCP transfer" )
menuitems+=( " " " " )
menuitems+=( " " " " )
menuitems+=( "ir2cs" "\Zr ircam2crop \Zn : (re-)start scexao2->scexao TCP transfer" )
menuitems+=( "ir2ck" "\Zr ircam2crop \Zn : kill scexao2->scexao TCP transfer" )
menuitems+=( " " " " )
menuitems+=( " " " " )
menuitems+=( "lj1cs" "\Zr labjack1 \Zn: (re-)start scexao2->scexao TCP transfer" )
menuitems+=( "lj1ck" "\Zr labjack1 \Zn: kill scexao2->scexao TCP transfer" )
menuitems+=( " " " " )
menuitems+=( "lj2cs" "\Zr labjack2 \Zn: (re-)start scexao2->scexao TCP transfer" )
menuitems+=( "lj2ck" "\Zr labjack2 \Zn: kill scexao2->scexao TCP transfer" )
menuitems+=( " " " " )
menuitems+=( "ljcs" "\Zr labjack \Zn: (re-)start scexao2->scexao TCP transfer" )
menuitems+=( "ljck" "\Zr labjack \Zn: kill scexao2->scexao TCP transfer" )



dialog --colors --title "Hardware Control" \
--ok-label "Select" \
--cancel-label "Top" \
--help-button --help-label "Exit" \
--default-item "${menuhardwarecontrol_default}" \
 --menu "$menuname" \
  $nbwlines $nbwcols 100 "${menuitems[@]}"  2> $tempfile


retval=$?
choiceval=$(cat $tempfile)


menualign_default="$choiceval"
state="menuhardwarecontrol"




case $retval in
   0) # button
menuhardwarecontrol_default="$choiceval"
	case $choiceval in


	dmrs)
/home/scexao/bin/dmrestart
;;
	dmk)
/home/scexao/bin/dmrestart -k
;;

	dmcommrs)
/home/scexao/bin/dmrestart -C
;;
	dmcommk)
/home/scexao/bin/dmrestart -c
;;


	dmVmax025)
dmVmax=" 25"
echo "${dmVmax}" > ./conf/conf_dmVmax.txt
Cfits << EOF
aolsetdmvoltmax 00 ${dmVmax}
aolsetdmDC 00 ${dmDCum025}
exit
EOF
;;

	dmVmax050)
dmVmax=" 50"
echo "${dmVmax}" > ./conf/conf_dmVmax.txt
Cfits << EOF
aolsetdmvoltmax 00 ${dmVmax}
aolsetdmDC 00 ${dmDCum050}
exit
EOF
;;

	dmVmax075)
dmVmax=" 75"
echo "${dmVmax}" > ./conf/conf_dmVmax.txt
Cfits << EOF
aolsetdmvoltmax 00 ${dmVmax}
aolsetdmDC 00 ${dmDCum075}
exit
EOF
;;

	dmVmax100)
dmVmax="100"
echo "${dmVmax}" > ./conf/conf_dmVmax.txt
Cfits << EOF
aolsetdmvoltmax 00 ${dmVmax}
aolsetdmDC 00 ${dmDCum100}
exit
EOF
;;

	dmVmax125)
dmVmax="125"
echo "${dmVmax}" > ./conf/conf_dmVmax.txt
Cfits << EOF
aolsetdmvoltmax 00 ${dmVmax}
aolsetdmDC 00 ${dmDCum125}
exit
EOF
;;

	dmVmax150)
dmVmax="150"
echo "${dmVmax}" > ./conf/conf_dmVmax.txt
Cfits << EOF
aolsetdmvoltmax 00 ${dmVmax}
aolsetdmDC 00 ${dmDCum150}
exit
EOF
;;


	ir1cs)
/home/scexao/bin/getTCPscexao2im -c ircam1 30102
;;

	ir1ck)
/home/scexao/bin/getTCPscexao2im -k ircam1 30102
;;

	ir1dcs)
/home/scexao/bin/getTCPscexao2im -c ircam1_dark 30103
;;

	ir1dck)
/home/scexao/bin/getTCPscexao2im -k ircam1_dark 30103
;;


	ir2cs)
/home/scexao/bin/getTCPscexao2im -c ircam2crop 30101
;;

	ir2ck)
/home/scexao/bin/getTCPscexao2im -k ircam2crop 30101
;;



	lj1cs)
/home/scexao/bin/getTCPscexao2im -c labjack1 30105
;;

	lj1ck)
/home/scexao/bin/getTCPscexao2im -k labjack1 30105
;;


	lj2cs)
/home/scexao/bin/getTCPscexao2im -c labjack2 30106
;;

	lj2ck)
/home/scexao/bin/getTCPscexao2im -k labjack2 30106
;;


	ljcs)
/home/scexao/bin/getTCPscexao2im -c labjack 30107
;;

	ljck)
/home/scexao/bin/getTCPscexao2im -k labjack 30107
;;



	esac;;
   1) state="menutop";;   
   2) state="menuexit";;
   255) state="menuexit";;
esac
fi











if [ $state = "menuexit" ]; then
stateok=1
echo "menuexit -> exit"
exit
fi



if [ $stateok = 0 ]; then
echo "state \"$state\" not recognized ... exit"
exit
fi




done
