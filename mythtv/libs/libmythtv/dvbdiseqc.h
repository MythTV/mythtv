/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbdiseqc.cpp of the MythTV project.
 */

#ifndef DVBDISEQC_H
#define DVBDISEQC_H

#include "pthread.h"
#include "qsqldatabase.h"
#include "tv_rec.h"
#include "dvbtypes.h"
#include <linux/dvb/frontend.h>

#define DISEQC_SHORT_WAIT 15*1000
#define DISEQC_LONG_WAIT 100*1000

class DVBDiSEqC
{
public:
    DVBDiSEqC(int cardnum, int fd_frontend);
    ~DVBDiSEqC();

    bool Set(dvb_tuning_t& tuning, bool reset, bool& havetuned);
    bool DiseqcReset();

private:
    int cardnum;
    int fd_frontend;
    dvb_tuning_t prev_tuning;
    int repeat;

    
    bool SendDiSEqCMessage(dvb_tuning_t& tuning, dvb_diseqc_master_cmd &cmd);
    bool SendDiSEqCMessage(dvb_diseqc_master_cmd &cmd);
    
    bool ToneVoltageLnb(dvb_tuning_t& tuning, bool reset, bool& havetuned);
    bool ToneSwitch(dvb_tuning_t& tuning, bool reset, bool& havetuned);
    bool Diseqc1xSwitch(dvb_tuning_t& tuning, bool reset, bool& havetuned);
    bool Diseqc1xSwitch_10way(dvb_tuning_t& tuning, bool reset, bool& havetuned);
    bool PositionerGoto(dvb_tuning_t& tuning, bool reset, bool& havetuned);
    bool PositionerStore(dvb_tuning_t& tuning);
    bool PositionerStopMovement();
    bool PositionerStoreEastLimit();
    bool PositionerStoreWestLimit();
    bool PositionerDisableLimits();   
    bool PositionerDriveEast(int timestep);
    bool PositionerDriveWest(int timestep);
    bool PositionerGotoAngular(dvb_tuning_t& tuning, bool reset, 
                               bool& havetuned);

    // Still need to be written
    bool Positioner_Status();

    enum diseqc_cmd_bytes {
        FRAME               = 0x0,
        ADDRESS             = 0x1,
        COMMAND             = 0x2,
        DATA_1              = 0x3,
        DATA_2              = 0x4,
        DATA_3              = 0x5
    };

    enum diseqc_frame_byte {
        CMD_FIRST           = 0xe0,
        CMD_REPEAT          = 0xe1,
        CMD_REPLY_FIRST     = 0xe2,
        CMD_REPLY_REPEAT    = 0xe3,
        REPLY_OK            = 0xe4,
        REPLY_NOSUPPORT     = 0xe5,
        REPLY_CRCERR_RPT    = 0xe6,
        REPLY_CMDERR_RPT    = 0xe7
    };
        
    enum diseqc_address {
        MASTER_TO_ALL        = 0x00,
        MASTER_TO_LSS        = 0x10,
        MASTER_TO_LNB        = 0x11,
        MASTER_TO_SWITCH     = 0x14,
        MASTER_TO_POSITIONER = 0x31
    };


    enum diseqc_commands {
        RESET               = 0x00,
        CLR_RESET           = 0x01,
        STANDBY             = 0x02,
        POWERON             = 0x03,
        SET_LO              = 0x20,
        SET_VR              = 0x21,
        SET_POS_A           = 0x22,
        SET_SO_A            = 0x23,
        SET_HI              = 0x24,
        SET_HL              = 0x25,
        SET_POS_B           = 0x26,
        SET_SO_B            = 0x27,
        WRITE_N0            = 0x38,
        WRITE_N1            = 0x39,
        WRITE_N2            = 0x3A,
        WRITE_N3            = 0x3B,
        READ_LNB_LOF_LO     = 0x52,
        READ_LNB_LOF_HI     = 0x53,

        HALT                = 0x60,
        LIMITS_OFF          = 0x63,
        POS_STAT            = 0x64,
        LIMIT_E             = 0x66,
        LIMIT_W             = 0x67,
        DRIVE_E             = 0x68,
        DRIVE_W             = 0x69,
        STORE               = 0x6A,
        GOTO                = 0x6B,
        GOTO_ANGULAR        = 0x6E
    };

};

#endif // DVBDISEQC_H
