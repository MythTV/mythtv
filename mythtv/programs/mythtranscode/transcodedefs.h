#ifndef TRANSCODEDEFS_H_
#define TRANSCODEDEFS_H_

enum REENCODE_STATUS : std::int8_t {
    REENCODE_MPEG2TRANS      =  2,
    REENCODE_CUTLIST_CHANGE  =  1,
    REENCODE_OK              =  0,
    REENCODE_ERROR           = -1,
    REENCODE_STOPPED         = -2,
};

#endif
