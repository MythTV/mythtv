#ifndef _COMMDETECTOR_H_
#define _COMMDETECTOR_H_

// This is used as a bitmask.
enum SkipTypes {
    COMM_DETECT_OFF         = 0x0000,
    COMM_DETECT_BLANKS      = 0x0001,
    COMM_DETECT_SCENE       = 0x0002,
    COMM_DETECT_BLANK_SCENE = 0x0003,
    COMM_DETECT_LOGO        = 0x0004,
    COMM_DETECT_ALL         = 0x00FF
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
