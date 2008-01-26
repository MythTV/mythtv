#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Usage: $0 </path/to/spumux.xml>"
	exit 0
fi

BADPNGS="n"

DIR=`dirname $1`
rm -f "${DIR}/spumux.xml.bad"

grep png $1 | awk -F'image="' '{print $2}' | awk -F'"' '{print $1}' | while read
do
	pngtopnm <"${REPLY}" >/dev/null
	RC=$?
	if [ ${RC} -eq 1 ]; then
		echo "Bad PNG: ${REPLY}"
		if [ "${BADPNGS}" = "n" ]; then
			BADPNGS="y"
			/bin/echo -n >"${DIR}/spumux.xml.bad" "${REPLY}"
		else
			/bin/echo -n >>"${DIR}/spumux.xml.bad" "\\|${REPLY}"
		fi
	fi
done

cat "${DIR}/spumux.xml.bad"

if [ -e "${DIR}/spumux.xml.bad" ]; then
	BADPNGS=`cat "${DIR}/spumux.xml.bad"`
	grep -v "${BADPNGS}" "$1" >"${DIR}/spumux.xml.new"
#	mv "${DIR}/spumux.xml" "${DIR}/spumux.xml.old"
	mv "${DIR}/spumux.xml.new" "${DIR}/spumux.xml"
	rm "${DIR}/spumux.xml.bad"	
fi
