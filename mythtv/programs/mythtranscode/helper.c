#include "config.h"
#include <inttypes.h>
#include <stdlib.h>	/* defines NULL */
#include <string.h>	/* memcmp */

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"

extern uint8_t mpeg2_scan_norm[64] ATTR_ALIGN(16);
void copy_quant_matrix(mpeg2dec_t *dec, uint16_t *dest)
{
  int i;
  uint8_t reverse[64];
  for(i=0; i< 64; i++)
    reverse[mpeg2_scan_norm[i]]=i;
  for(i=0; i< 64; i++)
    dest[reverse[i]]=dec->quantizer_matrix[0][i];
} 

