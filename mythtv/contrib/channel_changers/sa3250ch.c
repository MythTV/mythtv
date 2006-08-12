/*
 * sa3250ch - an external channel changer for SA3250HD Tuner 
 * Based off 6200ch.c by Stacey D. Son
 * 
 * Copyright 2004,2005 by Stacey D. Son <mythdev@son.org> 
 * Copyright 2005 Matt Porter <mporter@kernel.crashing.org>
 * Portions Copyright 2006 Chris Ingrassia <chris@spicecoffee.org> (SA4200 and Single-digit command mode)
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
#include <string.h>

/* SA3250HD IDs */
#define SA3250HD_VENDOR_ID1     0x000011e6
#define SA3250HD_VENDOR_ID2     0x000014f8
#define SA3250HD_VENDOR_ID3     0x00001692
#define SA3250HD_MODEL_ID1      0x00000be0
#define SA4200HD_VENDOR_ID1     0x000014f8
#define SA4200HD_MODEL_ID1      0x00001072

#define AVC1394_SA3250_COMMAND_CHANNEL 0x000007c00   /* subunit command */
#define AVC1394_SA3250_OPERAND_KEY_PRESS 0xe7
#define AVC1394_SA3250_OPERAND_KEY_RELEASE 0x67 

#define CTL_CMD0 AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_PANEL | \
        AVC1394_SUBUNIT_ID_0 | AVC1394_SA3250_COMMAND_CHANNEL
#define CTL_CMD1 (0x04 << 24)
#define CTL_CMD2 0xff000000

#define STARTING_NODE 0

void usage()
{
   fprintf(stderr, "Usage: sa3250ch [-v] [-s] <channel_num>\n");
   fprintf(stderr, "  -v : Verbose Mode\n");
   fprintf(stderr, "  -s : Send command as single digit (for SA4200 and some SA3250s)\n");
   exit(1);
}

int main (int argc, char *argv[])
{
   rom1394_directory dir;
   int device = -1;
   int single = 0;
   int i;
   int verbose = 0;
   quadlet_t cmd[3];
   int dig[3];
   int chn = 708;

   if (argc < 2) 
      usage();

  for(i = 1; i < argc; ++i) {
      if ((argv[i][0] == '-') && (strlen(argv[i]) > 1)) {
        switch(argv[i][1]) {
            case 'v':
                verbose = 1;
                break;
            case 's':
                single = 1;
                break;
            default:
                fprintf(stderr, "WARNING: Unknown option \'%c\', ignoring", argv[i][1]);
        }
      }
      else {
          chn = atoi(argv[i]);
      }
  }

#ifdef RAW1394_V_0_8
   raw1394handle_t handle = raw1394_get_handle();
#else
   raw1394handle_t handle = raw1394_new_handle();
#endif

   if (!handle) {
      if (!errno) {
         fprintf(stderr, "Not Compatible!\n");
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
		
      if ( ((dir.vendor_id == SA4200HD_VENDOR_ID1) &&
	    (dir.model_id == SA4200HD_MODEL_ID1))  ||
          (((dir.vendor_id == SA3250HD_VENDOR_ID1) ||
            (dir.vendor_id == SA3250HD_VENDOR_ID2) ||
            (dir.vendor_id == SA3250HD_VENDOR_ID3)) &&
            (dir.model_id == SA3250HD_MODEL_ID1))) {
            device = i;
            break;
      }
   }
    
   if (device == -1) {
        fprintf(stderr, "Could not find SA3250HD on the 1394 bus.\n");
        raw1394_destroy_handle(handle);
        exit(1);
   }

   if (single) {
       /* Send channel as single number for SA4200 and some SA3250s */
       if (verbose)
        printf("Using single number channel change command method\n");
        
       cmd[0] = CTL_CMD0 | AVC1394_SA3250_OPERAND_KEY_PRESS;
       cmd[1] = CTL_CMD1 | (chn << 8);
       cmd[2] = 0x0;

       if (verbose)
            printf("AV/C Command: cmd0=0x%08x cmd1=0x%08x cmd2=0x%08x\n",
                   cmd[0], cmd[1], cmd[2]);
       avc1394_transaction_block(handle, 0, cmd, 3, 1);       
   } else {
       /* Default method sending three seperate digits */
       dig[2] = 0x30 | (chn % 10);
       dig[1] = 0x30 | ((chn % 100)  / 10);
       dig[0] = 0x30 | ((chn % 1000) / 100);

       cmd[0] = CTL_CMD0 | AVC1394_SA3250_OPERAND_KEY_PRESS;
       cmd[1] = CTL_CMD1 | (dig[2] << 16) | (dig[1] << 8) | dig[0];
       cmd[2] = CTL_CMD2;

       if (verbose)
          printf("AV/C Command: %d%d%d = cmd0=0x%08x cmd2=0x%08x cmd3=0x%08x\n", 
                dig[0] & 0xf, dig[1] & 0xf, dig[2] & 0xf, cmd[0], cmd[1], cmd[2]);

       avc1394_transaction_block(handle, 0, cmd, 3, 1);
       cmd[0] = CTL_CMD0 | AVC1394_SA3250_OPERAND_KEY_RELEASE;
       cmd[1] = CTL_CMD1 | (dig[0] << 16) | (dig[1] << 8) | dig[2];
       cmd[2] = CTL_CMD2;

       if (verbose)
          printf("AV/C Command: %d%d%d = cmd0=0x%08x cmd2=0x%08x cmd3=0x%08x\n", 
                dig[0] & 0xf, dig[1] & 0xf, dig[2] & 0xf, cmd[0], cmd[1], cmd[2]);

       avc1394_transaction_block(handle, 0, cmd, 3, 1);
   }

   raw1394_destroy_handle(handle);

   exit(0);
}
