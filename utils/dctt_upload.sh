#!/bin/bash

usage() { 

	echo ""	
	echo "Usage: $0 [-P <path> -C <config-file>] [-p <serial port>] -f <hex file>" 
	echo -e "  -P\t The avrdude binary. If not specified, script will look in /usr/bin and /usr/local/bin."
	echo -e "  -C\t The avrdude config file. If not specified, script will look in /usr/etc and /usr/local/etc" 
	echo -e "  -p\t The serial port of the Arduino Mega 2560. Default: /dev/ttyACM0"
	echo -e "  -f\t Full path to the binary hex file. (Required)"

	1>&2; 
	exit 1; 
}

(( $# )) || usage


while getopts ":P:C:p:f:" o; do
    case "${o}" in
        P)
            avrdude_binary=${OPTARG}
            ;;
        C)
            avrdude_config=${OPTARG}
            ;;
        p)
            port=${OPTARG}
            ;;
        f)
            file=${OPTARG}
            ;;
        *)	
            usage
            ;;
    esac
done


#
# Setup path to avrdude binary and config file.
#
if ([ "${avrdude_binary}" ] && [ "${avrdude_config}" ]); then
	if [ ! -f "${avrdude_binary}" ]; then
    	echo "ERROR: Could not find avrdude here: $avrdude_binary. Install avrdude, e.g. sudo apt-get install avrdude"    
    	exit 1
    fi
	if [ ! -f "${avrdude_config}" ]; then
		echo "ERROR: Could not find avrdude config file here: $avrdude_config."    
		exit 1			
	fi	  
else
	# Check that -P and -C are specified together.
	if ( ([ -z "${avrdude_binary}" ] && [ "${avrdude_config}" ]) || ([ -z "${avrdude_config}" ] && [ "${avrdude_binary}" ])); then
		echo "ERROR: -P and -C parameters need to be used in conjunction."	
		exit 1
	fi
fi

# If avrdude is not pointed out explicitly, use defaults.
if [ -z "${avrdude_binary}" ]; then
	
	if [ -f "/usr/local/bin/avrdude" ]; then
		avrdude_binary="/usr/local/bin/avrdude"
		if [ -f "/usr/local/etc/avrdude.conf" ]; then
			avrdude_config="/usr/local/etc/avrdude.conf"
		else
	    	echo "ERROR: Could not find avrdude config file here: $avrdude_config."    
	    	exit 1						
		fi
	else
		if [ -f "/usr/bin/avrdude" ]; then
			avrdude_binary="/usr/bin/avrdude"
			if [ -f "/etc/avrdude.conf" ]; then
				avrdude_config="/etc/avrdude.conf"
			else
		    	echo "ERROR: Could not find avrdude config file here: $avrdude_config."    
		    	exit 1							
			fi
		else
	    	echo "ERROR: Could not find avrdude in /usr/bin or /usr/bin/local. Install avrdude, e.g. sudo apt-get install avrdude"    
	    	exit 1			
		fi

	fi
fi		

#
# Arduino serial port
# 

# If no port is defined, use default.
if [ -z "${port}" ]; then
	port="/dev/ttyACM0"
fi

# Use stty and to see if the port "exists".
stty -F ${port} &> /dev/null;
if [ "$?" != "0"  ]; then
    echo "ERROR: The serial port ${port} does not exist."
    exit 1
fi

#
# HEX file
# 

# Check if the file exists.
if [ -z "${file}" ]; then
	exit 1
fi
if [ ! -f "${file}" ]; then
    echo "ERROR: The file ${file} does not exist."
    exit 1
fi

cmd="${avrdude_binary} -C${avrdude_config} -patmega2560 -cstk500v2 -P${port} -b115200 -D -u -V -Uflash:w:${file}:i"

echo "Using following settings:"
echo ""
echo "Avrdude binary: ${avrdude_binary}"
echo "Avrdude config: ${avrdude_config}"
echo "Arduino Mega 2560 serial port: ${port}"
echo "Door Controller Test Tool binary file: ${file}"
echo ""
echo "-------------------------------"
echo "Starting avrdude write process!"
echo "-------------------------------"

echo "${cmd}"
$cmd