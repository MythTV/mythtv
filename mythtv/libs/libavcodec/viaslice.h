#ifndef VIASLICE_H_
#define VIASLICE_H_

typedef struct {
    int image_number;

    uint8_t *slice_data;
    int slice_datalen;

    int code;
    int maxcode;
    int slicecount;
    int lastcode;

    int progressive_sequence;
    int top_field_first;
} via_slice_state_t;

#endif
