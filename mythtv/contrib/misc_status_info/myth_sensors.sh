#!/bin/bash
#
# sensors.sh
# Retrieves sensors output and formats it for inclusion in the MythTV backend
# status page.

# The following line outputs information to include the CPU temperature
# information in the backend status page.  Though the sensors output includes
# the °C unit marker, it is stripped out of the data before being placed in the
# output page, so the display value includes a proper HTML entity.

#sensors | awk '/CPU Temp/ { printf "Current CPU Temperature: %s &#8451;.[]:[]temperature-CPU[]:[]%s\n", $3, $3 };'

# The following lines output CPU and Motherboard temperature and CPU and Case
# fan speed information.  Modify the patterns (i.e. /CPU Temp/) to match the
# sensors output lines containing the data you want to grab (i.e. /Core0
# Temp/).  Continue adding lines to the awk program as desired to incude
# additional information.

sensors | awk '
/CPU Temp/ {
  printf "Current CPU Temperature: %s &#8451;.[]:[]temperature-CPU[]:[]%s\n", \
         $3, $3
};
/M\/B Temp/ {
  printf \
  "Current Motherboard Temperature: %s &#8451;.[]:[]temperature-MB[]:[]%s\n", \
    $3, $3
};
/CPU Fan/ {
  printf "Current CPU Fan Speed: %s.[]:[]fan-CPU[]:[]%s\n", $3, $3
};
/Case Fan/ {
  printf "Current Case Fan Speed: %s.[]:[]fan-case[]:[]%s\n", $3, $3
};
' | sort

# With appropriate authentication in place (i.e. using SSH Certificate
# Authentication), you may also retrieve sensors data from remote system (i.e.
# slave backends, remote frontends, etc.).  Do so with commands of the format
# below (provide an appropriate hostname in place of "slave" (3 places) and
# replace the awk script with one providing the desired information as shown
# above):

#ssh -q slave sensors | awk '/CPU Temp/ { printf "Current CPU Temperature (slave): %s &#8451;.[]:[]temperature-CPU-slave[]:[]%s\n", $3, $3 };'

# Alternatively, this information could be retrieved from an appropriate
# monitoring program (monit, mrtg) or by using SNMP, depending on the system
# and network configuration.
