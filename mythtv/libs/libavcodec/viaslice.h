#ifndef VIASLICE_H_
#define VIASLICE_H_

typedef struct {
    int image_number;

    uint8_t *slice_data;
    int slice_datalen;

    int code;
    int maxcode;
    int slicecount;
} via_slice_state_t;

#endif
