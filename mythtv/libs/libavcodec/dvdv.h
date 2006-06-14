/*
 * This header is adapted from Accellent by John Dagliesh:
 *   http://www.defyne.org/dvb/accellent.html
 */

#ifndef __FF_DVDV__
#define __FF_DVDV__

/*
Header file containing structures for writing data to Apple's
private hardware MPEG2 decoding API, that I'm calling DVDVideo
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DVDV_MBInfo
{
    int16_t  frwd_mv[2];
    int16_t  back_mv[2];
    int16_t  frwd_mv_field[2];
    int16_t  back_mv_field[2];

    uint8_t  field_select[4];    // 0/1 in order of mvs above

    uint8_t  mb_type;            // |1 = frwd, |2 = back, |4 = use fields
    uint8_t  ildct;              // 0/1

    uint8_t  num_coded_elts[6];  // for each block incl. dc
} DVDV_MBInfo;

typedef struct DVDV_DCTElt
{
    uint8_t   run_sub_one;
    uint8_t   unused;
    uint16_t  elt;          // adjusted for first coded elt in block:
} DVDV_DCTElt;              // intra sub 0x0400, else add 0x0400

typedef struct DVDV_Frame
{
    int  intra_vlc_format;
    int  alternate_scan;
} DVDV_Frame;

struct DVDV_CurPtrs
{
    DVDV_MBInfo  * mb;
    DVDV_DCTElt  * dct;
    uint8_t      * cbp;
    DVDV_Frame   * frame;
};

extern struct DVDV_CurPtrs gDVDVState;
extern int gDVDVEnabled;

#ifdef __cplusplus
}
#endif

#endif // __FF_DVDV__
