#
# Join two cppcheck.xml files together.
#
# INPUT1: The cppcheck.xml file for mythtv
# INPUT2: The cppcheck.xml file for mythplugins
# OUTPUT: The file name for the combined file.
#
# The file format looks like this:
# ~~~
# <?xml version="1.0" encoding="UTF-8"?>
# <results version="2">
#     <cppcheck version="2.12.1"/>
#     <errors>
#         <error id="blah" severity="blah" msg="blah">
#             <location file="blah" line="xx" column="yy"/>
#             <symbol>blah</symbol>
#         </error>
#         ...
#     </errors>
# </results>
# ~~~

# Read both files
file(READ "${DIR1}/cppcheck.xml" text1)
if(NOT DIR2 STREQUAL "")
  file(READ "${DIR2}/cppcheck.xml" text2)
endif()

# Cmake string handling is messed up. A cmake list is just a string with
# semicolons separating the various items in the list.  We have to change the
# semicolons in things like "&amp;" to prevent cmake from mangling the text.
string(REPLACE ";" "|:,|" text1 "${text1}")
string(REPLACE ";" "|:,|" text2 "${text2}")

# Prune the files and join them
string(REGEX REPLACE "</errors>.*" "" text1 ${text1})
string(REGEX REPLACE ".*<errors>." "" text2 ${text2})
string(CONCAT full ${text1} ${text2})
string(REGEX REPLACE "${TOPDIR}/" "" full ${full})

# Restore the semicolions.
string(REPLACE "|:,|" ";" full "${full}")

# write it out
file(WRITE "${TOPDIR}/cppcheck.xml" "${full}")
