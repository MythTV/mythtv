/* dvbtune - tune.c

   Copyright (C) Dave Chapman 2001,2002
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/
   
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>

#ifdef NEWSTRUCT
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#else
#include <ost/dmx.h>
#include <ost/sec.h>
#include <ost/frontend.h>
#endif

#include "tune.h"

#ifndef NEWSTRUCT
int OSTSelftest(int fd)
{
    int ans;

    if ( (ans = ioctl(fd,FE_SELFTEST,0) < 0)){
        perror("FE SELF TEST: ");
        return -1;
    }

    return 0;
}

int OSTSetPowerState(int fd, uint32_t state)
{
    int ans;

    if ( (ans = ioctl(fd,FE_SET_POWER_STATE,state) < 0)){
        perror("OST SET POWER STATE: ");
        return -1;
    }

    return 0;
}

int OSTGetPowerState(int fd, uint32_t *state)
{
    int ans;

    if ( (ans = ioctl(fd,FE_GET_POWER_STATE,state) < 0)){
        perror("OST GET POWER STATE: ");
        return -1;
    }

    switch(*state){
    case FE_POWER_ON:
        fprintf(stderr,"POWER ON (%d)\n",*state);
        break;
    case FE_POWER_STANDBY:
        fprintf(stderr,"POWER STANDBY (%d)\n",*state);
        break;
    case FE_POWER_SUSPEND:
        fprintf(stderr,"POWER SUSPEND (%d)\n",*state);
        break;
    case FE_POWER_OFF:
        fprintf(stderr,"POWER OFF (%d)\n",*state);
        break;
    default:
        fprintf(stderr,"unknown (%d)\n",*state);
        break;
    }

    return 0;
}

int SecGetStatus (int fd, struct secStatus *state)
{
    int ans;

    if ( (ans = ioctl(fd,SEC_GET_STATUS, state) < 0)){
        perror("SEC GET STATUS: ");
        return -1;
    }

    switch (state->busMode){
    case SEC_BUS_IDLE:
        fprintf(stderr,"SEC BUS MODE:  IDLE (%d)\n",state->busMode);
        break;
    case SEC_BUS_BUSY:
        fprintf(stderr,"SEC BUS MODE:  BUSY (%d)\n",state->busMode);
        break;
    case SEC_BUS_OFF:
        fprintf(stderr,"SEC BUS MODE:  OFF  (%d)\n",state->busMode);
        break;
    case SEC_BUS_OVERLOAD:
        fprintf(stderr,"SEC BUS MODE:  OVERLOAD (%d)\n",state->busMode);
        break;
    default:
        fprintf(stderr,"SEC BUS MODE:  unknown  (%d)\n",state->busMode);
        break;
    }

    switch (state->selVolt){
    case SEC_VOLTAGE_OFF:
        fprintf(stderr,"SEC VOLTAGE:  OFF (%d)\n",state->selVolt);
        break;
    case SEC_VOLTAGE_LT:
        fprintf(stderr,"SEC VOLTAGE:  LT  (%d)\n",state->selVolt);
        break;
    case SEC_VOLTAGE_13:
        fprintf(stderr,"SEC VOLTAGE:  13  (%d)\n",state->selVolt);
        break;
    case SEC_VOLTAGE_13_5:
        fprintf(stderr,"SEC VOLTAGE:  13.5 (%d)\n",state->selVolt);
        break;
    case SEC_VOLTAGE_18:
        fprintf(stderr,"SEC VOLTAGE:  18 (%d)\n",state->selVolt);
        break;
    case SEC_VOLTAGE_18_5:
        fprintf(stderr,"SEC VOLTAGE:  18.5 (%d)\n",state->selVolt);
        break;
    default:
        fprintf(stderr,"SEC VOLTAGE:  unknown (%d)\n",state->selVolt);
        break;
    }

    fprintf(stderr,"SEC CONT TONE: %s\n", (state->contTone == SEC_TONE_ON ? "ON" : "OFF"));
    return 0;
}
#endif

void print_status(FILE* fd,fe_status_t festatus) {
  fprintf(fd,"FE_STATUS:");
  if (festatus & FE_HAS_SIGNAL) fprintf(fd," FE_HAS_SIGNAL");
#ifdef NEWSTRUCT
  if (festatus & FE_TIMEDOUT) fprintf(fd," FE_TIMEDOUT");
#else
  if (festatus & FE_HAS_POWER) fprintf(fd," FE_HAS_POWER");
  if (festatus & FE_SPECTRUM_INV) fprintf(fd," FE_SPECTRUM_INV");
  if (festatus & FE_TUNER_HAS_LOCK) fprintf(fd," FE_TUNER_HAS_LOCK");
#endif
  if (festatus & FE_HAS_LOCK) fprintf(fd," FE_HAS_LOCK");
  if (festatus & FE_HAS_CARRIER) fprintf(fd," FE_HAS_CARRIER");
  if (festatus & FE_HAS_VITERBI) fprintf(fd," FE_HAS_VITERBI");
  if (festatus & FE_HAS_SYNC) fprintf(fd," FE_HAS_SYNC");
  fprintf(fd,"\n");
}

#ifdef NEWSTRUCT
int check_status(int fd_frontend,struct dvb_frontend_parameters* feparams,int tone) {
  int i,res;
  int32_t strength;
  fe_status_t festatus;
  struct dvb_frontend_event event;
  struct dvb_frontend_info fe_info;
  struct pollfd pfd[1];
  int status;

  if (ioctl(fd_frontend,FE_SET_FRONTEND,feparams) < 0) {
    perror("ERROR tuning channel\n");
    return -1;
  }

  pfd[0].fd = fd_frontend;
  pfd[0].events = POLLIN;

  event.status=0;
  while (((event.status & FE_TIMEDOUT)==0) && ((event.status & FE_HAS_LOCK)==0)) {
    fprintf(stderr,"polling....\n");
    if (poll(pfd,1,10000)){
      if (pfd[0].revents & POLLIN){
        fprintf(stderr,"Getting frontend event\n");
        if (status = ioctl(fd_frontend, FE_GET_EVENT, &event) < 0){
	  if (status != -EOVERFLOW) {
	    perror("FE_GET_EVENT");
	    return -1;
	  }
        }
      }
      print_status(stderr,event.status);
    }
  }

  if (event.status & FE_HAS_LOCK) {
      switch(fe_info.type) {
         case FE_OFDM:
           fprintf(stderr,"Event:  Frequency: %d\n",event.parameters.frequency);
           break;
         case FE_QPSK:
           fprintf(stderr,"Event:  Frequency: %d\n",(unsigned int)((event.parameters.frequency)+(tone==SEC_TONE_OFF ? LOF1 : LOF2)));
           fprintf(stderr,"        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
           fprintf(stderr,"        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
           fprintf(stderr,"\n");
           break;
         case FE_QAM:
           fprintf(stderr,"Event:  Frequency: %d\n",event.parameters.frequency);
           fprintf(stderr,"        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
           fprintf(stderr,"        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
           break;
         default:
           break;
      }

      strength=0;
      ioctl(fd_frontend,FE_READ_BER,&strength);
      fprintf(stderr,"Bit error rate: %d\n",strength);

      strength=0;
      ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength);
      fprintf(stderr,"Signal strength: %d\n",strength);

      strength=0;
      ioctl(fd_frontend,FE_READ_SNR,&strength);
      fprintf(stderr,"SNR: %d\n",strength);

      festatus=0;
      ioctl(fd_frontend,FE_READ_STATUS,&festatus);
      print_status(stderr,festatus);
    } else {
    fprintf(stderr,"Not able to lock to the signal on the given frequency\n");
    return -1;
    }
  return 0;
}
#else
int check_status(int fd_frontend,FrontendParameters* feparams,int tone) {
  int i,res;
  int32_t strength;
  fe_status_t festatus;
  FrontendEvent event;
  FrontendInfo fe_info;
  struct pollfd pfd[1];

  i = 0; res = -1;
  while ((i < 3) && (res < 0)) {
    if (ioctl(fd_frontend,FE_SET_FRONTEND,feparams) < 0) {
      perror("ERROR tuning channel\n");
      return -1;
    }

    pfd[0].fd = fd_frontend;
    pfd[0].events = POLLIN;

    if (poll(pfd,1,10000)){
      if (pfd[0].revents & POLLIN){
        fprintf(stderr,"Getting frontend event\n");
        if ( ioctl(fd_frontend, FE_GET_EVENT, &event) < 0) {
          perror("FE_GET_EVENT");
          return -1;
        }
        fprintf(stderr,"Received ");
        switch(event.type){
          case FE_UNEXPECTED_EV:
            fprintf(stderr,"unexpected event\n");
            res = -1;
            break;
          case FE_FAILURE_EV:
            fprintf(stderr,"failure event\n");
            res = -1;
            break;
          case FE_COMPLETION_EV:
            fprintf(stderr,"completion event\n");
            res = 0;
          break;
        }
      }
      i++;
    }
  }

  if (res > 0)
    switch (event.type) {
       case FE_UNEXPECTED_EV: fprintf(stderr,"FE_UNEXPECTED_EV\n");
                                break;
       case FE_COMPLETION_EV: fprintf(stderr,"FE_COMPLETION_EV\n");
                                break;
       case FE_FAILURE_EV: fprintf(stderr,"FE_FAILURE_EV\n");
                                break;
    }

    if (event.type == FE_COMPLETION_EV) {
      switch(fe_info.type) {
         case FE_OFDM:
           fprintf(stderr,"Event:  Frequency: %d\n",event.u.completionEvent.Frequency);
           break;
         case FE_QPSK:
           fprintf(stderr,"Event:  Frequency: %d\n",(unsigned int)((event.u.completionEvent.Frequency)+(tone==SEC_TONE_OFF ? LOF1 : LOF2)));
           fprintf(stderr,"        SymbolRate: %d\n",event.u.completionEvent.u.qpsk.SymbolRate);
           fprintf(stderr,"        FEC_inner:  %d\n",event.u.completionEvent.u.qpsk.FEC_inner);
           fprintf(stderr,"\n");
           break;
         case FE_QAM:
           fprintf(stderr,"Event:  Frequency: %d\n",event.u.completionEvent.Frequency);
           fprintf(stderr,"        SymbolRate: %d\n",event.u.completionEvent.u.qpsk.SymbolRate);
           fprintf(stderr,"        FEC_inner:  %d\n",event.u.completionEvent.u.qpsk.FEC_inner);
           break;
         default:
           break;
      }

      strength=0;
      ioctl(fd_frontend,FE_READ_BER,&strength);
      fprintf(stderr,"Bit error rate: %d\n",strength);

      strength=0;
      ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength);
      fprintf(stderr,"Signal strength: %d\n",strength);

      strength=0;
      ioctl(fd_frontend,FE_READ_SNR,&strength);
      fprintf(stderr,"SNR: %d\n",strength);

      festatus=0;
      ioctl(fd_frontend,FE_READ_STATUS,&festatus);

      fprintf(stderr,"FE_STATUS:");
      if (festatus & FE_HAS_POWER) fprintf(stderr," FE_HAS_POWER");
      if (festatus & FE_HAS_SIGNAL) fprintf(stderr," FE_HAS_SIGNAL");
      if (festatus & FE_SPECTRUM_INV) fprintf(stderr," FE_SPECTRUM_INV");
      if (festatus & FE_HAS_LOCK) fprintf(stderr," FE_HAS_LOCK");
      if (festatus & FE_HAS_CARRIER) fprintf(stderr," FE_HAS_CARRIER");
      if (festatus & FE_HAS_VITERBI) fprintf(stderr," FE_HAS_VITERBI");
      if (festatus & FE_HAS_SYNC) fprintf(stderr," FE_HAS_SYNC");
      if (festatus & FE_TUNER_HAS_LOCK) fprintf(stderr," FE_TUNER_HAS_LOCK");
      fprintf(stderr,"\n");
    } else {
    fprintf(stderr,"Not able to lock to the signal on the given frequency\n");
    return -1;
  }
  return 0;
}
#endif

int tune_it(int fd_frontend, int fd_sec, unsigned int freq, unsigned int srate, char pol, int tone, fe_spectral_inversion_t specInv, unsigned int diseqc,fe_modulation_t modulation,fe_code_rate_t HP_CodeRate,fe_code_rate_t LP_CodeRate,fe_transmit_mode_t TransmissionMode,fe_guard_interval_t guardInterval, fe_bandwidth_t bandwidth, fe_hierarchy_t hierarchy) {
  int res;
#ifdef NEWSTRUCT
  struct dvb_frontend_parameters feparams;
  struct dvb_frontend_info fe_info;
  fe_sec_voltage_t voltage;
#else
  FrontendParameters feparams;
  FrontendInfo fe_info;
  secVoltage voltage;
  struct secStatus sec_state;
#endif

  /* discard stale frontend events */
  /*
  pfd[0].fd = fd_frontend;
  pfd[0].events = POLLIN;

  if (poll(pfd,1,500)){
    if (pfd[0].revents & POLLIN){
      while (1) {
        if (ioctl (fd_frontend, FE_GET_EVENT, &event) == -1) { break; }
      }
    }
  }
  */
  if ( (res = ioctl(fd_frontend,FE_GET_INFO, &fe_info) < 0)){
     perror("FE_GET_INFO: ");
     return -1;
  }
  
//  OSTSelftest(fd_frontend);
//  OSTSetPowerState(fd_frontend, FE_POWER_ON);
//  OSTGetPowerState(fd_frontend, &festatus);

#ifdef NEWSTRUCT
  fprintf(stderr,"Using DVB card \"%s\"\n",fe_info.name);
#endif

  switch(fe_info.type) {
    case FE_OFDM:
#ifdef NEWSTRUCT
      if (freq < 1000000) freq*=1000UL;
      feparams.frequency=freq;
      feparams.inversion=INVERSION_OFF;
      feparams.u.ofdm.bandwidth=bandwidth;
      feparams.u.ofdm.code_rate_HP=HP_CodeRate;
      feparams.u.ofdm.code_rate_LP=LP_CodeRate;
      feparams.u.ofdm.constellation=modulation;
      feparams.u.ofdm.transmission_mode=TransmissionMode;
      feparams.u.ofdm.guard_interval=guardInterval;
      feparams.u.ofdm.hierarchy_information=hierarchy;
#else
      if (freq < 1000000) freq*=1000UL;
      feparams.Frequency=freq;
      feparams.Inversion=INVERSION_OFF;
      feparams.u.ofdm.bandWidth=bandwidth;
      feparams.u.ofdm.HP_CodeRate=HP_CodeRate;
      feparams.u.ofdm.LP_CodeRate=LP_CodeRate;
      feparams.u.ofdm.Constellation=modulation;
      feparams.u.ofdm.TransmissionMode=TransmissionMode;
      feparams.u.ofdm.guardInterval=guardInterval;
      feparams.u.ofdm.HierarchyInformation=hierarchy;
#endif
      fprintf(stderr,"tuning DVB-T (%s) to %d Hz\n",DVB_T_LOCATION,freq);
      break;
    case FE_QPSK:
#ifdef NEWSTRUCT
      fprintf(stderr,"tuning DVB-S to L-Band:%d, Pol:%c Srate=%d, 22kHz=%s\n",feparams.frequency,pol,srate,tone == SEC_TONE_ON ? "on" : "off");
#else
      fprintf(stderr,"tuning DVB-S to L-Band:%d, Pol:%c Srate=%d, 22kHz=%s\n",feparams.Frequency,pol,srate,tone == SEC_TONE_ON ? "on" : "off");
#endif
      if ((pol=='h') || (pol=='H')) {
        voltage = SEC_VOLTAGE_18;
      } else {
        voltage = SEC_VOLTAGE_13;
      }
#ifdef NEWSTRUCT
      if (ioctl(fd_frontend,FE_SET_VOLTAGE,voltage) < 0) {
#else
      if (ioctl(fd_sec,SEC_SET_VOLTAGE,voltage) < 0) {
#endif
         perror("ERROR setting voltage\n");
      }

      if (freq > 2200000) {
        // this must be an absolute frequency
        if (freq < SLOF) {
#ifdef NEWSTRUCT
          feparams.frequency=(freq-LOF1);
#else
          feparams.Frequency=(freq-LOF1);
#endif
          if (tone < 0) tone = SEC_TONE_OFF;
        } else {
#ifdef NEWSTRUCT
          feparams.frequency=(freq-LOF2);
#else
          feparams.Frequency=(freq-LOF2);
#endif
          if (tone < 0) tone = SEC_TONE_ON;
        }
      } else {
        // this is an L-Band frequency
#ifdef NEWSTRUCT
       feparams.frequency=freq;
#else
       feparams.Frequency=freq;
#endif
      }

#ifdef NEWSTRUCT
      feparams.inversion=specInv;
      feparams.u.qpsk.symbol_rate=srate;
      feparams.u.qpsk.fec_inner=FEC_AUTO;
#else
      feparams.Inversion=specInv;
      feparams.u.qpsk.SymbolRate=srate;
      feparams.u.qpsk.FEC_inner=FEC_AUTO;
#endif

#ifdef NEWSTRUCT
      if (ioctl(fd_frontend,FE_SET_TONE,tone) < 0) {
         perror("ERROR setting tone\n");
      }
#else
      if (ioctl(fd_sec,SEC_SET_TONE,tone) < 0) {
         perror("ERROR setting tone\n");
      }
#endif

#ifdef NEWSTRUCT
  #warning DISEQC is unimplemented for NEWSTRUCT
#else
      if (diseqc > 0) {
        struct secCommand scmd;
        struct secCmdSequence scmds;

        scmds.continuousTone = tone;
        scmds.voltage = voltage;
        /*
        scmds.miniCommand = toneBurst ? SEC_MINI_B : SEC_MINI_A;
        */
        scmds.miniCommand = SEC_MINI_NONE;

        scmd.type = 0;
        scmds.numCommands = 1;
        scmds.commands = &scmd;

        scmd.u.diseqc.addr = 0x10;
        scmd.u.diseqc.cmd = 0x38;
        scmd.u.diseqc.numParams = 1;
        scmd.u.diseqc.params[0] = 0xf0 | 
                                  (((diseqc - 1) << 2) & 0x0c) |
                                  (voltage==SEC_VOLTAGE_18 ? 0x02 : 0) |
                                  (tone==SEC_TONE_ON ? 0x01 : 0);

        if (ioctl(fd_sec,SEC_SEND_SEQUENCE,&scmds) < 0) {
          perror("Error sending DisEqC");
          return -1;
        }
      }
#endif
      break;
    case FE_QAM:
      fprintf(stderr,"tuning DVB-C to %d, srate=%d\n",freq,srate);
#ifdef NEWSTRUCT
      feparams.frequency=freq;
      feparams.inversion=INVERSION_OFF;
      feparams.u.qam.symbol_rate = srate;
      feparams.u.qam.fec_inner = FEC_AUTO;
      feparams.u.qam.modulation = QAM_64;
#else
      feparams.Frequency=freq;
      feparams.Inversion=INVERSION_OFF;
      feparams.u.qam.SymbolRate = srate;
      feparams.u.qam.FEC_inner = FEC_AUTO;
      feparams.u.qam.QAM = QAM_64;
#endif
      break;
    default:
      fprintf(stderr,"Unknown FE type. Aborting\n");
      exit(-1);
  }
  usleep(100000);

#ifndef NEWSTRUCT   
  if (fd_sec) SecGetStatus(fd_sec, &sec_state);
#endif

  return(check_status(fd_frontend,&feparams,tone));
}
