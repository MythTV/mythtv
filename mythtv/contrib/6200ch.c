/*
 * 6200ch - an external channel changer for Motorola DCT-6200 Tuner 
 * 
 * Copyright 2004 by Stacey D. Son <mythtv@son.org> 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <libavc1394/rom1394.h>
#include <libavc1394/avc1394.h>
#include <libraw1394/raw1394.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

// Motorola DCT-6200 IDs
#define DCT6200_VENDOR_ID  0x00000ce5
#define DCT6200_SPEC_ID    0x00005068
#define DCT6200_SW_VERSION 0x00010101
#define DCT6200_MODEL_ID   0x0000620a

#define AVC1394_SUBUNIT_TYPE_6200 (9 << 19)  /* uses a reserved subunit type */ 

#define AVC1394_6200_COMMAND_CHANNEL 0x000007C00   /* 6200 subunit command */
#define AVC1394_6200_OPERAND_SET 0x67      /* 6200 subunit command operand */

#define CTL_CMD0 AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_6200 | \
        AVC1394_SUBUNIT_ID_0 | AVC1394_6200_COMMAND_CHANNEL | \
        AVC1394_6200_OPERAND_SET

#define OPERAND0 0x040000FF
#define OPERAND1 0xFF000000

#define STARTING_NODE 2  /* skip 1394 nodes to avoid error msgs */

void usage()
{
   fprintf(stderr, "Usage: 6200ch [-v] <channel_num>\n");
   exit(1);
}

int main (int argc, char *argv[])
{
   rom1394_directory dir;
   int device = -1;
   int i;
   int verbose = 0;
   quadlet_t cmd[3];
   int chn = 550;

   if (argc < 2) 
      usage();

   if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 'v') {
      verbose = 1;
      chn = atoi(argv[2]);
   } else {
      chn = atoi(argv[1]);
   }

#ifdef RAW1394_V_0_8
   raw1394handle_t handle = raw1394_get_handle();
#else
   raw1394handle_t handle = raw1394_new_handle();
#endif

   if (!handle) {
      if (!errno) {
         fprintf(stderr, "Not Compatable!\n");
      } else {
         perror("Couldn't get 1394 handle");
         fprintf(stderr, "Is ieee1394, driver, and raw1394 loaded?\n");
      }
      exit(1);
   } 

   if (raw1394_set_port(handle, 0) < 0) {
      perror("couldn't set port");
      raw1394_destroy_handle(handle);
      exit(1);
   }

   int nc = raw1394_get_nodecount(handle);
   for (i=STARTING_NODE; i < nc; ++i) {
      if (rom1394_get_directory(handle, i, &dir) < 0) {
         fprintf(stderr,"error reading config rom directory for node %d\n", i);
         raw1394_destroy_handle(handle);
    	 exit(1);
      }

      if (verbose) 
         printf("node %d: vendor_id = 0x%08x model_id = 0x%08x\n", 
                 i, dir.vendor_id, dir.model_id); 
		
      if ( (dir.vendor_id == DCT6200_VENDOR_ID) &&
	   (dir.model_id == DCT6200_MODEL_ID)) {
            if (dir.unit_spec_id != DCT6200_SPEC_ID)
               fprintf(stderr, "Warning: Unit Spec ID different.\n");
            if (dir.unit_sw_version != DCT6200_SW_VERSION)
               fprintf(stderr, "Warning: Unit Software Version different.\n");
            device = i;
            break;
      }
   }
    
   if (device == -1) {
        fprintf(stderr, "Could not find Motorola DCT-6200 on the 1394 bus.\n");
        raw1394_destroy_handle(handle);
        exit(1);
   }


   cmd[0] = CTL_CMD0;
   cmd[1] = OPERAND0 | (chn << 8);
   cmd[2] = OPERAND1;

   if (verbose)
      printf("AV/C Command: Opcode=0x%08X Operand0=0x%08X Operand1=0x%08X\n", 
              cmd[0], cmd[1], cmd[2]);

   if (avc1394_send_command_block(handle, device, cmd, 3) != 0) {
      fprintf(stderr, "Command not accepted\n");
      raw1394_destroy_handle(handle);	
      exit(1);
   }

    raw1394_destroy_handle(handle);
    exit(0);
}


