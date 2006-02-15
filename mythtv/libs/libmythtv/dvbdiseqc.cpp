/*
 *  Class DVBDiSEqC
 *
 *  Copyright (C) Kenneth Aafloy 2003
 *
 *  Description:
 *
 *  Author(s):
 *      Taylor Jacob (rtjacob at earthlink.net)
 *	    - Finished Implimenting Petri's DiSEqC 1.0 - 1.1 code
 *          - DiSEqC 1.2 Positioner control 
 *      Petri Nykanen
 *          - DiSEqC 1.0 - 1.1.
 *      Kenneth Aafloy (ke-aa at frisurf.no)
 *          - Initial framework.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>
#include <cmath>

#include "pthread.h"
#include "qsqldatabase.h"

#include "mythcontext.h"
#include "tv_rec.h"
#include "cardutil.h"

#include "dvbtypes.h"
#include "dvbdiseqc.h"

#define TO_RADS (M_PI / 180.0)
#define TO_DEC (180.0 / M_PI)

#define LOC QString("DiSEqC(%1): ").arg(cardnum)
#define LOC_ERR QString("DiSEqC(%1) Error: ").arg(cardnum)

DVBDiSEqC::DVBDiSEqC(int _cardnum, int _fd_frontend):
    cardnum(_cardnum), fd_frontend(_fd_frontend)
{

    // Number of repeats for DiSEqC 1.1 devices
    repeat = 1;
}

DVBDiSEqC::~DVBDiSEqC()
{
}

bool DVBDiSEqC::Set(DVBTuning& tuning, bool reset, bool& havetuned)
{
    switch (tuning.diseqc_type)
    {
        case DISEQC_MINI_2:
            if (!ToneSwitch(tuning, reset, havetuned))
                return false;
            // fall through
        case DISEQC_SINGLE:
            if (!ToneVoltageLnb(tuning, reset, havetuned))
                return false;
            break;
        case DISEQC_SWITCH_2_1_0: // 2 Way v1.0
        case DISEQC_SWITCH_2_1_1: // 2 Way v1.1
            if (!Diseqc1xSwitch(tuning, reset, havetuned, 2))
                return false;
            break;
        case DISEQC_SWITCH_4_1_0: // 4 Way v1.0
        case DISEQC_SWITCH_4_1_1: // 4 Way v1.1
            if (!Diseqc1xSwitch(tuning, reset, havetuned, 4))
                return false;
            break;
        case DISEQC_POSITIONER_1_2: // 1.2 Positioner (HH Motor)
            if (!PositionerGoto(tuning,reset,havetuned))
		return false;
            break;
        case DISEQC_POSITIONER_X: // 1.3 Positioner (HH Motor with USALS)
            if (!PositionerGotoAngular(tuning,reset,havetuned))
		return false;
            break;
        case DISEQC_POSITIONER_1_2_SWITCH_2: // 10 Way v1.1 or v2.1
            if (!Diseqc1xSwitch(tuning, reset, havetuned, 10))
                return false;
            break;
        case DISEQC_SW21: // Dish Network legacy switch SW21
        case DISEQC_SW64: // Dish Network legacy switch SW64
            if (!LegacyDishSwitch(tuning, reset, havetuned))
                return false;
            break;
 
        default:
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Unsupported DiSEqC type("
                    <<tuning.diseqc_type<<")");
    }
    
    return true;
}

bool DVBDiSEqC::LegacyDishSwitch(DVBTuning &tuning, bool reset,
                                 bool &havetuned)
{
    VERBOSE(VB_CHANNEL, LOC + "Legacy Dish Switch: " +
            QString("Port %1").arg(tuning.diseqc_port));

    if (reset ||
        (prev_tuning.diseqc_port != tuning.diseqc_port ||
         prev_tuning.tone        != tuning.tone        ||
         prev_tuning.voltage     != tuning.voltage))
    {
        uint8_t cmd = 0x00;

        if (DISEQC_SW21 == tuning.diseqc_type)
            cmd = (tuning.diseqc_port) ? 0x66 : 0x34;
        else if (DISEQC_SW64 == tuning.diseqc_type)
        {
            if (tuning.diseqc_port == 0)
                cmd = (tuning.voltage == SEC_VOLTAGE_13) ? 0x39 : 0x1A;
            else if (tuning.diseqc_port == 1)
                cmd = (tuning.voltage == SEC_VOLTAGE_13) ? 0x4B : 0x5C;
            else
                cmd = (tuning.voltage == SEC_VOLTAGE_13) ? 0x0D : 0x2E;
        }

        if (tuning.voltage == SEC_VOLTAGE_18)
            cmd |= 0x80;

#ifdef FE_DISHNETWORK_SEND_LEGACY_CMD
        if (ioctl(fd_frontend, FE_DISHNETWORK_SEND_LEGACY_CMD, cmd) <0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Legacy Dish Switch: "
                    "Sending init command failed." + ENO);
            return false;
        }
#else
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Legacy Dish Switch: " +
                "Linux kernel does not support this switch.");
#endif

        usleep(DISEQC_SHORT_WAIT);

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.tone        = tuning.tone;
        prev_tuning.voltage     = tuning.voltage;
        havetuned = true;
    }
    return true;
}

/*****************************************************************************
                        Backward Compatible Methods
 ****************************************************************************/

bool DVBDiSEqC::ToneVoltageLnb(DVBTuning& tuning, bool reset, bool& havetuned)
{
    VERBOSE(VB_CHANNEL, LOC + QString("Setting LNB: %1 %2")
            .arg(tuning.tone==SEC_TONE_ON?"Tone ON":"Tone OFF")
            .arg(tuning.voltage==SEC_VOLTAGE_13?"13V":"18V"));

    if (prev_tuning.tone != tuning.tone || reset)
    {
        if (ioctl(fd_frontend, FE_SET_TONE, tuning.tone) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Setting Tone mode failed." + ENO);
            return false;
        }

        prev_tuning.tone = tuning.tone;
    }

    usleep(DISEQC_SHORT_WAIT);

    if (prev_tuning.voltage != tuning.voltage || reset)
    {
        if (ioctl(fd_frontend, FE_SET_VOLTAGE, tuning.voltage) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Setting Polarization failed." + ENO);
            return false;
        }

        prev_tuning.voltage = tuning.voltage;
    }

    havetuned |= ((prev_tuning.voltage == tuning.voltage) &&
                  (prev_tuning.tone == tuning.tone));

    return true;
}

bool DVBDiSEqC::ToneSwitch(DVBTuning& tuning, bool reset, bool& havetuned)
{
    VERBOSE(VB_CHANNEL, LOC + QString("Tone Switch - Port %1/2")
            .arg(tuning.diseqc_port));

    if (prev_tuning.diseqc_port != tuning.diseqc_port || reset)
    {
        if (tuning.diseqc_port > 2)
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Tone Switches only support two ports.");

        if (ioctl(fd_frontend, FE_DISEQC_SEND_BURST,
                  (tuning.diseqc_port == 1 ? SEC_MINI_A : SEC_MINI_B )) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Setting Tone Switch failed." + ENO);
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
    }

    havetuned |= (prev_tuning.diseqc_port == tuning.diseqc_port);

    return true;
}

/*****************************************************************************
                    Diseqc 1.x Compatible Methods
 ****************************************************************************/

bool DVBDiSEqC::SendDiSEqCMessage(DVBTuning& tuning, dvb_diseqc_master_cmd &cmd)
{
    // Turn off tone burst
    if (ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "FE_SET_TONE failed" + ENO);
        return false;
    }

/* 
   Old version of the code set the voltage to 13V everytime.
   After looking at the EutelSat specs I saw no reason that
   this was done. I have tested this with my DiSEqC switch
   and all is fine. 
*/

    if (ioctl(fd_frontend, FE_SET_VOLTAGE, tuning.voltage) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "FE_SET_VOLTAGE failed" + ENO);
        return false;
    }   

    usleep(DISEQC_SHORT_WAIT);

    VERBOSE(VB_CHANNEL, LOC + QString("Sending 1.0 Command: %1 %2 %3 %4")
            .arg(cmd.msg[0], 2, 16)
            .arg(cmd.msg[1], 2, 16)
            .arg(cmd.msg[2], 2, 16)
            .arg(cmd.msg[3], 2, 16));

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "FE_DISEQC_SEND_MASTER_CMD failed" + ENO);
        return false;
    }

    usleep(DISEQC_SHORT_WAIT);

    // Check to see if its a 1.1 or 1.2 device. If so repeat the message repeats times.
    if ((tuning.diseqc_type == 3) || (tuning.diseqc_type == 5) || 
        (tuning.diseqc_type == 6) || (tuning.diseqc_type == 7)) 
    {

        int repeats = repeat;
        while (repeats--) 
        {

            if (tuning.diseqc_type == 7)
            {
                VERBOSE(VB_CHANNEL, LOC +
                        QString("Sending 1.3 Repeat Command: %1 %2 %3 %4 %5")
                        .arg(cmd.msg[0],2,16)
                        .arg(cmd.msg[1],2,16)
                        .arg(cmd.msg[2],2,16)
                        .arg(cmd.msg[3],2,16)
                        .arg(cmd.msg[4],2,16));
            }
            else
            {
                VERBOSE(VB_CHANNEL, LOC +
                        QString("Sending 1.1/1.2 Repeat Command: %1 %2 %3 %4")
                        .arg(cmd.msg[0],2,16)
                        .arg(cmd.msg[1],2,16)
                        .arg(cmd.msg[2],2,16)
                        .arg(cmd.msg[3],2,16));
            }

            cmd.msg[0] = CMD_REPEAT;      
            if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "FE_DISEQC_SEND_MASTER_CMD failed" + ENO);
                return false;
            }
            usleep(DISEQC_SHORT_WAIT);
     
            cmd.msg[0] = CMD_FIRST;      
            if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "FE_DISEQC_SEND_MASTER_CMD failed" + ENO);
                return false;
            }
            usleep(DISEQC_SHORT_WAIT);
        }
    }

    if (ioctl(fd_frontend, FE_DISEQC_SEND_BURST, SEC_MINI_A ) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "FE_DISEQC_SEND_BURST failed" + ENO);
        return false;
    }

    usleep(DISEQC_SHORT_WAIT);

    if (ioctl(fd_frontend, FE_SET_TONE, tuning.tone) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "FE_SET_TONE failed" + ENO);
        return false;
    }

    return true;
}


bool DVBDiSEqC::SendDiSEqCMessage(dvb_diseqc_master_cmd &cmd)
{
    // Turn off tone burst
    if (ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "FE_SET_TONE failed" + ENO);
        return false;
    }

    usleep(DISEQC_SHORT_WAIT);

    VERBOSE(VB_CHANNEL, LOC + QString("Sending 1.0 Command: %1 %2 %3 %4")
            .arg(cmd.msg[0], 2, 16)
            .arg(cmd.msg[1], 2, 16)
            .arg(cmd.msg[2], 2, 16)
            .arg(cmd.msg[3], 2, 16));

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "FE_DISEQC_SEND_MASTER_CMD failed" + ENO);
        return false;
    }

    usleep(DISEQC_SHORT_WAIT);
  
    int repeats = repeat;
    while (repeats--) 
    {
        VERBOSE(VB_CHANNEL, LOC +
                QString("Sending 1.1/1.2/1.3 Repeat Command: %1 %2 %3 %4")
                .arg(cmd.msg[0], 2, 16)
                .arg(cmd.msg[1], 2, 16)
                .arg(cmd.msg[2], 2, 16)
                .arg(cmd.msg[3], 2, 16));

        cmd.msg[0] = CMD_REPEAT;      
        if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "FE_DISEQC_SEND_MASTER_CMD failed" + ENO);
            return false;
        }
        usleep(DISEQC_SHORT_WAIT);
    
        cmd.msg[0] = CMD_FIRST;      
        if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "FE_DISEQC_SEND_MASTER_CMD failed" + ENO);
            return false;
        }
        usleep(DISEQC_SHORT_WAIT); 
    }

    if (ioctl(fd_frontend, FE_DISEQC_SEND_BURST, SEC_MINI_A ) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "FE_DISEQC_SEND_BURST failed" + ENO);
        return false;
    }

    return true;
}

bool DVBDiSEqC::Diseqc1xSwitch(DVBTuning& tuning, bool reset, 
                               bool& havetuned, uint ports)
{
    if (reset) 
    {
      	if (!DiseqcReset()) 
        {
      	    VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
      	    return false;
      	}
    }

    VERBOSE(VB_CHANNEL, LOC +
            QString("1.1 Switch (%1 ports) - Port %2 - %3 %4")
            .arg(ports)
            .arg(tuning.diseqc_port)
            .arg(tuning.tone==SEC_TONE_ON?"Tone ON":"Tone OFF")
            .arg(tuning.voltage==SEC_VOLTAGE_13?"13V":"18V"));

    if ((prev_tuning.diseqc_port != tuning.diseqc_port  ||
         prev_tuning.tone != tuning.tone                ||
         prev_tuning.voltage != tuning.voltage        ) || reset)
    {
        dvb_diseqc_master_cmd cmd =
            {{CMD_FIRST, MASTER_TO_LSS, WRITE_N1, 0xf0, 0x00, 0x00}, 4};

        if (tuning.diseqc_port >= ports )
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Unsupported switch");
            return false;
        }

        switch (ports)
        {
            case 10:
                cmd.msg[COMMAND] = WRITE_N1;
                cmd.msg[DATA_1] = 0xF0 | (tuning.diseqc_port & 0x0F);
                break;
            case 4:
            case 2: 
                cmd.msg[COMMAND] = WRITE_N0;
                cmd.msg[DATA_1] =
                    0xF0 |
                    (((tuning.diseqc_port) * 4) & 0x0F)          |
                    ((tuning.voltage == SEC_VOLTAGE_18) ? 2 : 0) |
                    ((tuning.tone == SEC_TONE_ON) ? 1 : 0);
                break;
            default:
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Unsupported number of ports for DiSEqC 1.1 Switch");
        }

        if (!SendDiSEqCMessage(tuning,cmd)) 
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.tone = tuning.tone;
        prev_tuning.voltage = tuning.voltage;
        havetuned = true;

    }

    havetuned |=
        (prev_tuning.diseqc_port == tuning.diseqc_port) &&
        (prev_tuning.voltage     == tuning.voltage)     &&
        (prev_tuning.tone        == tuning.tone);

    return true;
}

bool DVBDiSEqC::DiseqcReset()
{
    struct dvb_diseqc_master_cmd reset_cmd =
        {{CMD_FIRST, MASTER_TO_LSS, RESET, 0x00, 0x00}, 3};
 
    struct dvb_diseqc_master_cmd init_cmd =
        {{CMD_FIRST, MASTER_TO_LSS, POWERON, 0x00, 0x00}, 3};

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &init_cmd) <0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Setup: Sending init command failed." + ENO);
        return false;
    }
    usleep(DISEQC_LONG_WAIT);

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &reset_cmd) <0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Setup: Sending reset command failed." + ENO);
        return false;
    }
    usleep(DISEQC_LONG_WAIT);

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &init_cmd) <0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Setup: Sending init command failed." + ENO);
        return false;
    }
    usleep(DISEQC_LONG_WAIT);

    return true;
}

/*****************************************************************************
                            Positioner Control
 *****************************************************************************/

bool DVBDiSEqC::PositionerDriveEast(int timestep)
{
    if (!DiseqcReset()) 
    {
      	VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, DRIVE_E, timestep ,0x00,0x00}, 4};

    if (!SendDiSEqCMessage(cmd)) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
        return false;
    }
    
    return true;
}

bool DVBDiSEqC::PositionerDriveWest(int timestep)
{
    if (!DiseqcReset()) 
    {
      	VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, DRIVE_W, timestep ,0x00,0x00}, 4};

    if (!SendDiSEqCMessage(cmd)) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
        return false;
    }

    return true;
}

bool DVBDiSEqC::PositionerGoto(DVBTuning& tuning, bool reset, bool& havetuned)
{
    // A reset seems to be required for my positioner to work consistently
    VERBOSE(VB_CHANNEL, LOC + QString("1.2 Motor - Goto Stored Position %1")
            .arg(tuning.diseqc_port));

    if ((prev_tuning.diseqc_port != tuning.diseqc_port ||
         prev_tuning.tone != tuning.tone ||
         prev_tuning.voltage != tuning.voltage) || reset)
    {
        if (!DiseqcReset()) 
        {
      	    VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	    return false;
        }
        
        dvb_diseqc_master_cmd cmd = 
            {{CMD_FIRST, MASTER_TO_POSITIONER, GOTO, tuning.diseqc_port, 
              0x00, 0x00}, 4};

        if (!SendDiSEqCMessage(tuning,cmd)) 
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.tone = tuning.tone;
        prev_tuning.voltage = tuning.voltage;
    }

    havetuned |=
        (prev_tuning.diseqc_port == tuning.diseqc_port) &&
        (prev_tuning.voltage     == tuning.voltage)     &&
        (prev_tuning.tone        == tuning.tone);

    return true;
}

bool DVBDiSEqC::PositionerStore(DVBTuning& tuning)
{
    if (!DiseqcReset()) 
    {
      	VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	return false;
    }

    VERBOSE(VB_CHANNEL, LOC + QString("1.2 Motor - Store Stored Position %1")
            .arg(tuning.diseqc_port));

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, STORE, tuning.diseqc_port , 0x00, 
          0x00}, 4};

    if (!SendDiSEqCMessage(cmd)) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
        return false;
    }

    return true;
}

bool DVBDiSEqC::PositionerStoreEastLimit()
{
    if (!DiseqcReset()) 
    {
      	VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, LIMIT_E, 0x00,0x00,0x00}, 3};

    if (!SendDiSEqCMessage(cmd)) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
        return false;
    }

    return true;
}

bool DVBDiSEqC::PositionerStoreWestLimit()
{
    if (!DiseqcReset()) 
    {
      	VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, LIMIT_W, 0x00,0x00,0x00}, 3};

    if (!SendDiSEqCMessage(cmd)) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
        return false;
    }

    return true;
}

bool DVBDiSEqC::PositionerStopMovement()
{
    if (!DiseqcReset()) 
    {
      	VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, HALT, 0x00,0x00,0x00}, 3};

    if (!SendDiSEqCMessage(cmd)) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
        return false;
    }
    return true;

}

bool DVBDiSEqC::Positioner_Status()
{
    // Not sure if this function is possible without being DiSEqC >= v2
    // Someone with a DVB card that supports v2 cards will need to help me out 
    // here
    return false;
}

bool DVBDiSEqC::PositionerDisableLimits()
{
    if (!DiseqcReset()) 
    {
      	VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, LIMITS_OFF, 0x00,0x00,0x00}, 3};

    if (!SendDiSEqCMessage(cmd)) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
        return false;
    }

    return true;
}

/*****************************************************************************
                                Diseqc v1.3 (Goto X)
 ****************************************************************************/

bool DVBDiSEqC::PositionerGotoAngular(DVBTuning& tuning, bool reset, 
                                      bool& havetuned) 
{
    // TODO: Send information here to FE saying motor is moving and
    //       to expect a longer than average tuning delay
    if (prev_tuning.diseqc_pos != tuning.diseqc_pos)
        VERBOSE(VB_CHANNEL, LOC + "DiSEqC Motor Moving");

    int CMD1=0x00 , CMD2=0x00;        // Bytes sent to motor
    double USALS=0.0;
    int DecimalLookup[10] =
        { 0x00, 0x02, 0x03, 0x05, 0x06, 0x08, 0x0A, 0x0B, 0x0D, 0x0E };

    // Equation lifted from VDR rotor plugin by
    // Thomas Bergwinkl <Thomas.Bergwinkl@t-online.de>

    double P = gContext->GetSetting("Latitude", "").toFloat() * TO_RADS;           // Earth Station Latitude
    double Ue = gContext->GetSetting("Longitude", "").toFloat() * TO_RADS;           // Earth Station Longitude
    double Us = tuning.diseqc_pos * TO_RADS;          // Satellite Longitude

    double az = M_PI + atan( tan (Us-Ue) / sin(P) );
    double x = acos( cos(Us-Ue) * cos(P) );
    double el = atan( (cos(x) - 0.1513 ) /sin(x) );
    double Azimuth = atan((-cos(el)*sin(az))/(sin(el)*cos(P)-cos(el)*sin(P)*cos(az)))* TO_DEC;

//    printf("Offset = %f\n",Azimuth);

    if (Azimuth > 0.0)
        CMD1=0xE0;    // East
    else 
        CMD1=0xD0;      // West

    USALS = fabs(Azimuth);
 
    while (USALS > 16) 
    {
        CMD1++;
   	USALS -=16;
    }

    while (USALS >= 1.0) 
    {
   	CMD2+=0x10;
        USALS--;
    }

    CMD2 += DecimalLookup[(int)round(USALS*10)];
  
    if (!DiseqcReset()) 
    {
      	VERBOSE(VB_IMPORTANT, LOC_ERR + "DiseqcReset() failed");
    	return false;
    }

    // required db changes - get lat and lon for ground station location in db 
    // and added to tuning
    // sat_pos be passed into tuning, and be a float not an int./

    VERBOSE(VB_CHANNEL, LOC + QString("1.3 Motor - Goto Angular Position %1")
            .arg(tuning.diseqc_pos));

    if ((prev_tuning.diseqc_port != tuning.diseqc_port  ||
         prev_tuning.tone        != tuning.tone         ||
         prev_tuning.diseqc_pos  != tuning.diseqc_pos   ||
         prev_tuning.voltage     != tuning.voltage    ) || reset)
    {

        dvb_diseqc_master_cmd cmd = 
            {{CMD_FIRST, MASTER_TO_POSITIONER, GOTO_ANGULAR, CMD1 , CMD2 , 
              0x00}, 5};

        if (!SendDiSEqCMessage(tuning,cmd)) 
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting DiSEqC failed.");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.diseqc_pos = tuning.diseqc_pos;
        prev_tuning.tone = tuning.tone;
        prev_tuning.voltage = tuning.voltage;
    }

    havetuned |=
        (prev_tuning.diseqc_port == tuning.diseqc_port) &&
        (prev_tuning.diseqc_pos  == tuning.diseqc_pos)  &&
        (prev_tuning.voltage     == tuning.voltage)     &&
        (prev_tuning.tone        == tuning.tone);

    return true;
}
