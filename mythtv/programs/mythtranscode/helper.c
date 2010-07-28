#include "config.h"
#include <inttypes.h>

#include "mpeg2_internal.h"

extern uint16_t inv_zigzag_direct16[64];

void copy_quant_matrix(mpeg2dec_t *dec, uint16_t *dest)
{
  int i;
  for(i=0; i< 64; i++)
    dest[inv_zigzag_direct16[i]-1] = dec->quantizer_matrix[0][i];
}

