#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>


#include <dvdread/nav_types.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include "dvdnav/dvdnav.h"
#include "remap.h"
#include "decoder.h"
#include "vm.h"
#include "vm_serialize.h"

/*
"navstate",<version>,<sprm x 24>,<gprm x 16>,
<domain>,<vtsn>,<pgcn>,<pgn>,<celln>,<cell_restart>,<blockn>,
<rsm_vts>,<rsm_blockn>,<rsm_pgcn>,<rsm_celln>,<rsm_sprm x 5>,"end"
*/
/* The serialized string starts with "navstate,1,", so that needs 11 chars.
 * There are 10 integer fields (domain, vtsN, pgcN, pgN, cellN, cell_restart,
 * blockN, rsm_vtsN, rsm_blockN, rsm_pgcN and rsm_cellN).
 * None of the values can realistically be more than 32 bits (and most will
 * be far less).  In worst case, each field would need 11 characters
 * (-2147483648) plus a trailing comma, so we need 10 * (11+1) = 120 chars
 * for those 10 fields.
 * Each SPRM is 16 bits, so thats 6 chars + 1 per register ("0xffff,") * 24
 * registers = 168 chars.
 * Each GPRM is 16 bits plus a mode character plus two 32 bit counter values.
 * In the format "[0xffff;0;0xffffffff;0xffffffff],", each register needs 33
 * chars.  With 16 registers, that's 33 * 16 = 528
 * There are five resume SPRM values, so 5 * (6 + !) = 42
 * Finally, the string is terminated with "end" and a NULL char, so another 4 chars.
 * In total then, 11 + 120 + 168 + 528 + 42 + 4 = 873 chars.  Round that up to 1024
 * and there should never be any issues.  Just to be safe, we'll still check as
 * we go along.
 */

#define BUFFER_SIZE    1024
#define FORMAT_VERSION    1

static void vm_serialize_int(int *stored, char **buf, size_t *remaining, int value)
{
  if(stored && buf && remaining && *stored > 0)
  {
    *stored = snprintf(*buf, *remaining, "%d,", value);
    if(*stored > 0)
    {
      *remaining -= (size_t)(*stored);
      *buf += (*stored);
    }
  }
}

static void vm_deserialize_int(int *consumed, char **buf, int* value)
{
  if(consumed && buf && *consumed > 0)
  {
    sscanf( *buf, "%d,%n", value, consumed);

    if(*consumed > 0)
      *buf += *consumed;
  }
}

char *vm_serialize_dvd_state(const dvd_state_t *state)
{
  char   *str_state = 0;
  char   *buf;
  int    stored;
  int    tmp;
  int    idx;
  size_t remaining = BUFFER_SIZE;

  if(state)
  {
    str_state = malloc(BUFFER_SIZE);
    buf = str_state;

    stored = snprintf(buf, remaining, "navstat,%d,", FORMAT_VERSION);

    if(stored > 0)
    {
      remaining -= (size_t)stored;
      buf += stored;
    }

    // SPRM
    for(idx = 0; idx < 24 && stored > 0; idx++)
    {
      stored = snprintf(buf, remaining, "0x%hx,", state->registers.SPRM[idx]);
      if(stored > 0)
      {
        remaining -= (size_t)stored;
        buf += stored;
      }
    }

    // GPRM
    for(idx = 0; idx < 16 && stored > 0; idx++)
    {
      stored = snprintf(buf, remaining,
                        "[0x%hx;%d;0x%x;0x%x],", state->registers.GPRM[idx],
                                                 state->registers.GPRM_mode[idx],
                                                 state->registers.GPRM_time[idx].tv_sec,
                                                 state->registers.GPRM_time[idx].tv_usec);
      if(stored > 0)
      {
        remaining -= (size_t)stored;
        buf += stored;
      }
    }

    vm_serialize_int(&stored, &buf, &remaining, state->domain);
    vm_serialize_int(&stored, &buf, &remaining, state->vtsN);
    vm_serialize_int(&stored, &buf, &remaining, state->pgcN);
    vm_serialize_int(&stored, &buf, &remaining, state->pgN);
    vm_serialize_int(&stored, &buf, &remaining, state->cellN);
    vm_serialize_int(&stored, &buf, &remaining, state->cell_restart);
    vm_serialize_int(&stored, &buf, &remaining, state->blockN);
    vm_serialize_int(&stored, &buf, &remaining, state->rsm_vtsN);
    vm_serialize_int(&stored, &buf, &remaining, state->rsm_blockN);
    vm_serialize_int(&stored, &buf, &remaining, state->rsm_pgcN);
    vm_serialize_int(&stored, &buf, &remaining, state->rsm_cellN);

    // Resume SPRM
    for(idx = 0; idx < 5 && stored > 0; idx++)
    {
      stored = snprintf(buf, remaining, "0x%hx,", state->rsm_regs[idx]);
      if(stored > 0)
      {
        remaining -= (size_t)stored;
        buf += stored;
      }
    }

    if(stored > 0 && remaining >= 4)
    {
      // Done.  Terminating the string.
      strcpy(buf, "end");
    }
    else
    {
      // Error
      free(str_state);
      str_state = 0;
    }
  }
  return str_state;
}

int vm_deserialize_dvd_state(const char* serialized, dvd_state_t *state)
{
  char        *buf = serialized;
  int         consumed;
  int         version;
  int         tmp;
  int         idx;
  int         ret = 0; /* assume an error */
  dvd_state_t new_state;

  sscanf( buf, "navstat,%d,%n", &version, &consumed);
  if(version == 1)
  {
    buf += consumed;

    // SPRM
    for(idx = 0; idx < 24 && consumed > 0; idx++)
    {
      sscanf(buf, "0x%hx,%n", &new_state.registers.SPRM[idx], &consumed);
      buf += consumed;
    }

    // GPRM
    for(idx = 0; idx < 16 && consumed > 0; idx++)
    {
      sscanf(buf, "[0x%hx;%d;0x%x;0x%x],%n", &new_state.registers.GPRM[idx],
                                             &new_state.registers.GPRM_mode[idx],
                                             &new_state.registers.GPRM_time[idx].tv_sec,
                                             &new_state.registers.GPRM_time[idx].tv_usec,
                                             &consumed);
      buf += consumed;
    }

    vm_deserialize_int(&consumed, &buf, &new_state.domain);
    vm_deserialize_int(&consumed, &buf, &new_state.vtsN);
    vm_deserialize_int(&consumed, &buf, &new_state.pgcN);
    vm_deserialize_int(&consumed, &buf, &new_state.pgN);
    vm_deserialize_int(&consumed, &buf, &new_state.cellN);
    vm_deserialize_int(&consumed, &buf, &tmp);
    new_state.cell_restart = tmp;
    vm_deserialize_int(&consumed, &buf, &new_state.blockN);
    vm_deserialize_int(&consumed, &buf, &new_state.rsm_vtsN);
    vm_deserialize_int(&consumed, &buf, &new_state.rsm_blockN);
    vm_deserialize_int(&consumed, &buf, &new_state.rsm_pgcN);
    vm_deserialize_int(&consumed, &buf, &new_state.rsm_cellN);

    // Resume SPRM
    for(idx = 0; idx < 5 && consumed > 0; idx++)
    {
      sscanf(buf, "0x%hx,%n", &new_state.registers.SPRM[idx], &consumed);
      buf += consumed;
    }

    if(strcmp(buf,"end") == 0)
    {
      /* Success! */
      *state = new_state;
      state->pgc = NULL;
      ret = 1;
    }
  }

  return ret;
}
