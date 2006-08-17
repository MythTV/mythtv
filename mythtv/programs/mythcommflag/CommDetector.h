#ifndef _COMMDETECTOR_H_
#define _COMMDETECTOR_H_

// This is used as a bitmask.
enum SkipTypes {
    COMM_DETECT_UNINIT      = -1,
    COMM_DETECT_OFF         = 0x00000000,
    COMM_DETECT_BLANKS      = 0x00000001,
    COMM_DETECT_SCENE       = 0x00000002,
    COMM_DETECT_BLANK_SCENE = 0x00000003,
    COMM_DETECT_LOGO        = 0x00000004,
    COMM_DETECT_ALL         = 0x000000FF,
    COMM_DETECT_2           = 0x00000100,
    COMM_DETECT_2_LOGO      = 0x00000101,
    COMM_DETECT_2_BLANK     = 0x00000102,
    COMM_DETECT_2_ALL       = 0x000001FF,
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
