TEMPLATE = subdirs

# Directories
SUBDIRS += mythepg mythprogfind mythtv mythfrontend mythcommflag mythlcd 
SUBDIRS += mythtvosd

!win32 {
    SUBDIRS += mythbackend mythfilldatabase mythtranscode
}

