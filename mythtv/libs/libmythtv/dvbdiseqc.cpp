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

#include "dvbtypes.h"
#include "dvbdiseqc.h"

#define TO_RADS (M_PI / 180.0)
#define TO_DEC (180.0 / M_PI)

DVBDiSEqC::DVBDiSEqC(int _cardnum, int _fd_frontend):
    cardnum(_cardnum), fd_frontend(_fd_frontend)
{

    // Number of repeats for DiSEqC 1.1 devices
    repeat = 1;
}

DVBDiSEqC::~DVBDiSEqC()
{
}

bool DVBDiSEqC::Set(dvb_tuning_t& tuning, bool reset, bool& havetuned)
{

    switch(tuning.diseqc_type)
    {
        case 1:
            if (!ToneSwitch(tuning, reset, havetuned))
                return false;
            // fall through
        case 0:
            if (!ToneVoltageLnb(tuning, reset, havetuned))
                return false;
            break;
        case 2: // v1.0 2 Way
        case 3: // v1.1 2 Way
        case 4: // v1.0 4 Way
        case 5: // v1.1 4 Way
            if (!Diseqc1xSwitch(tuning, reset, havetuned))
                return false;
            break;
        case 6: // 1.2 Positioner (HH Motor)
            if (!PositionerGoto(tuning,reset,havetuned))
		return false;
            break;
        case 7: // 1.3 Positioner (HH Motor with USALS)
            if (!PositionerGotoAngular(tuning,reset,havetuned))
		return false;
            break;
        case 8: // v1.1 10 Way
            if (!Diseqc1xSwitch_10way(tuning, reset, havetuned))
                return false;
            break;
 
        default:
            ERRNO("Unsupported DiSEqC type.");
    }
    
    return true;
}

/*****************************************************************************
                        Backward Compatible Methods
 ****************************************************************************/

bool DVBDiSEqC::ToneVoltageLnb(dvb_tuning_t& tuning, bool reset, bool& havetuned)
{
    CHANNEL(QString("Setting LNB: %1 %2")
            .arg(tuning.tone==SEC_TONE_ON?"Tone ON":"Tone OFF")
            .arg(tuning.voltage==SEC_VOLTAGE_13?"13V":"18V"));

    if (prev_tuning.tone != tuning.tone || reset)
    {
        if (ioctl(fd_frontend, FE_SET_TONE, tuning.tone) < 0)
        {
            ERRNO("Setting Tone mode failed.");
            return false;
        }

        prev_tuning.tone = tuning.tone;
        havetuned = true;
    }

    usleep(DISEQC_SHORT_WAIT);

    if (prev_tuning.voltage != tuning.voltage || reset)
    {
        if (ioctl(fd_frontend, FE_SET_VOLTAGE, tuning.voltage) < 0)
        {
            ERRNO("Setting Polarization failed.");
            return false;
        }

        prev_tuning.voltage = tuning.voltage;
        havetuned = true;
    }

    return true;
}

bool DVBDiSEqC::ToneSwitch(dvb_tuning_t& tuning, bool reset, bool& havetuned)
{
    CHANNEL(QString("DiSEqC Tone Switch - Port %1/2").arg(tuning.diseqc_port));

    if (prev_tuning.diseqc_port != tuning.diseqc_port || reset)
    {
        if (tuning.diseqc_port > 2)
            ERRNO("Tone Switches only supports two ports.");

        if (ioctl(fd_frontend, FE_DISEQC_SEND_BURST,
            (tuning.diseqc_port == 1 ? SEC_MINI_A : SEC_MINI_B )) < 0)
        {
            ERRNO("Setting Tone Switch failed.");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        havetuned = true;
    }

    return true;
}

/*****************************************************************************
                    Diseqc 1.x Compatible Methods
 ****************************************************************************/

bool DVBDiSEqC::SendDiSEqCMessage(dvb_tuning_t& tuning, dvb_diseqc_master_cmd &cmd)
{
    // Turn off tone burst
    if (ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF) == -1) 
    {
        ERRNO("FE_SET_TONE failed");
        return false;
    }

/* 
   Old version of the code set the voltage to 13V everytime.  After looking at the EutelSat
   specs I saw no reason that this was done. I have tested this with my DiSEqC switch and all
   is fine. 
*/

    if (ioctl(fd_frontend, FE_SET_VOLTAGE, tuning.voltage) == -1) 
    {
        ERRNO("FE_SET_VOLTAGE failed");
        return false;
    }   

    usleep(DISEQC_SHORT_WAIT);

    GENERAL(QString("DiSEqC Sending 1.0 Command: %1 %2 %3 %4")
                   .arg(cmd.msg[0], 2, 16)
                   .arg(cmd.msg[1], 2, 16)
                   .arg(cmd.msg[2], 2, 16)
                   .arg(cmd.msg[3], 2, 16));

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
    {
        ERRNO("FE_DISEQC_SEND_MASTER_CMD failed");
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
                GENERAL(QString("DiSEqC Sending 1.3 Repeat Command: %1 %2 %3 %4 %5")
                               .arg(cmd.msg[0],2,16)
                               .arg(cmd.msg[1],2,16)
                               .arg(cmd.msg[2],2,16)
                               .arg(cmd.msg[3],2,16)
                               .arg(cmd.msg[4],2,16));
            }
            else
            {
                GENERAL(QString("DiSEqC Sending 1.1/1.2 Repeat Command: %1 %2 %3 %4")
                           .arg(cmd.msg[0],2,16)
                           .arg(cmd.msg[1],2,16)
                           .arg(cmd.msg[2],2,16)
                           .arg(cmd.msg[3],2,16));
            }

            cmd.msg[0] = CMD_REPEAT;      
            if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
            {
                ERRNO("FE_DISEQC_SEND_MASTER_CMD failed");
                return false;
            }
            usleep(DISEQC_SHORT_WAIT);
     
            cmd.msg[0] = CMD_FIRST;      
            if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
            {
                ERRNO("FE_DISEQC_SEND_MASTER_CMD failed");
                return false;
            }
            usleep(DISEQC_SHORT_WAIT);
        }
    }

    if (ioctl(fd_frontend, FE_DISEQC_SEND_BURST, SEC_MINI_A ) == -1) 
    {
        ERRNO("FE_DISEQC_SEND_BURST failed");
        return false;
    }

    usleep(DISEQC_SHORT_WAIT);

    if (ioctl(fd_frontend, FE_SET_TONE, tuning.tone) == -1) 
    {
        ERRNO("FE_SET_TONE failed");
        return false;
    }

    return true;
}


bool DVBDiSEqC::SendDiSEqCMessage(dvb_diseqc_master_cmd &cmd)
{
    // Turn off tone burst
    if (ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF) == -1) 
    {
        ERRNO("FE_SET_TONE failed");
        return false;
    }

    usleep(DISEQC_SHORT_WAIT);

    GENERAL(QString("DiSEqC Sending 1.0 Command: %1 %2 %3 %4")
                   .arg(cmd.msg[0], 2, 16)
                   .arg(cmd.msg[1], 2, 16)
                   .arg(cmd.msg[2], 2, 16)
                   .arg(cmd.msg[3], 2, 16));

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
    {
        ERRNO("FE_DISEQC_SEND_MASTER_CMD failed");
        return false;
    }

    usleep(DISEQC_SHORT_WAIT);
  
    int repeats = repeat;
    while (repeats--) 
    {
        GENERAL(QString("DiSEqC Sending 1.1/1.2/1.3 Repeat Command: %1 %2 %3 %4")
                       .arg(cmd.msg[0], 2, 16)
                       .arg(cmd.msg[1], 2, 16)
                       .arg(cmd.msg[2], 2, 16)
                       .arg(cmd.msg[3], 2, 16));

        cmd.msg[0] = CMD_REPEAT;      
        if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
        {
            ERRNO("FE_DISEQC_SEND_MASTER_CMD failed");
            return false;
        }
        usleep(DISEQC_SHORT_WAIT);
    
        cmd.msg[0] = CMD_FIRST;      
        if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1) 
        {
            ERRNO("FE_DISEQC_SEND_MASTER_CMD failed");
            return false;
        }
        usleep(DISEQC_SHORT_WAIT); 
    }

    if (ioctl(fd_frontend, FE_DISEQC_SEND_BURST, SEC_MINI_A ) == -1) 
    {
        ERRNO("FE_DISEQC_SEND_BURST failed");
        return false;
    }

    return true;
}

bool DVBDiSEqC::Diseqc1xSwitch_10way(dvb_tuning_t& tuning, bool reset, 
                               bool& havetuned)
{
    if (reset) 
    {
      	if (!DiseqcReset()) 
        {
      	    ERRNO("DiseqcReset() failed");
      	    return false;
      	}
    }

    GENERAL(QString("DiSEqC 1.1 Switch - Port %1").arg(tuning.diseqc_port));

    if ((prev_tuning.diseqc_port != tuning.diseqc_port ||
        prev_tuning.tone != tuning.tone ||
        prev_tuning.voltage != tuning.voltage) || reset)
    {
        if (tuning.diseqc_port > 9)
        {
            ERRNO("Supports only up to 10-way switches.");
            return false;
        }

        dvb_diseqc_master_cmd cmd = 
            {{CMD_FIRST, MASTER_TO_LSS, WRITE_N1, 0xf0, 0x00, 0x00}, 4};

        cmd.msg[DATA_1] = 0xF0 
		| (tuning.diseqc_port & 0x0F); 

        if (!SendDiSEqCMessage(tuning,cmd)) 
        {
            ERRNO("Setting DiSEqC failed.\n");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.tone = tuning.tone;
        prev_tuning.voltage = tuning.voltage;
        havetuned = true;

    }

    return true;
}

bool DVBDiSEqC::Diseqc1xSwitch(dvb_tuning_t& tuning, bool reset, 
                               bool& havetuned)
{
    if (reset) 
    {
      	if (!DiseqcReset()) 
        {
      	    ERRNO("DiseqcReset() failed");
      	    return false;
      	}
    }

    GENERAL(QString("DiSEqC 1.0 Switch - Port %1").arg(tuning.diseqc_port));

    if ((prev_tuning.diseqc_port != tuning.diseqc_port ||
        prev_tuning.tone != tuning.tone ||
        prev_tuning.voltage != tuning.voltage) || reset)
    {
        if (tuning.diseqc_port > 3)
        {
            ERRNO("Supports only up to 4-way switches.");
            return false;
        }

        dvb_diseqc_master_cmd cmd = 
            {{CMD_FIRST, MASTER_TO_LSS, WRITE_N0, 0xf0, 0x00, 0x00}, 4};

        cmd.msg[DATA_1] = 0xF0 
                          | (((tuning.diseqc_port) * 4) & 0x0F) 
                          | ((tuning.voltage == SEC_VOLTAGE_18) ? 2 : 0) 
                          | ((tuning.tone == SEC_TONE_ON) ? 1 : 0);

        if (!SendDiSEqCMessage(tuning,cmd)) 
        {
            ERRNO("Setting DiSEqC failed.\n");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.tone = tuning.tone;
        prev_tuning.voltage = tuning.voltage;
        havetuned = true;

    }

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
        ERRNO("Setup: Sending init command failed.");
        return false;
    }
    usleep(DISEQC_LONG_WAIT);

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &reset_cmd) <0)
    {
        ERRNO("Setup: Sending reset command failed.");
        return false;
    }
    usleep(DISEQC_LONG_WAIT);

    if (ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &init_cmd) <0)
    {
        ERRNO("Setup: Sending init command failed.");
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
      	ERRNO("DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, DRIVE_E, timestep ,0x00,0x00}, 4};

    if (!SendDiSEqCMessage(cmd)) 
    {
        ERRNO("Setting DiSEqC failed.\n");
        return false;
    }
    
    return true;
}

bool DVBDiSEqC::PositionerDriveWest(int timestep)
{
    if (!DiseqcReset()) 
    {
      	ERRNO("DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, DRIVE_W, timestep ,0x00,0x00}, 4};

    if (!SendDiSEqCMessage(cmd)) 
    {
        ERRNO("Setting DiSEqC failed.\n");
        return false;
    }

    return true;
}

bool DVBDiSEqC::PositionerGoto(dvb_tuning_t& tuning, bool reset, bool& havetuned)
{
    // A reset seems to be required for my positioner to work consistently
    GENERAL(QString("DiSEqC 1.2 Motor - Goto Stored Position %1")
                    .arg(tuning.diseqc_port));

    if ((prev_tuning.diseqc_port != tuning.diseqc_port ||
         prev_tuning.tone != tuning.tone ||
         prev_tuning.voltage != tuning.voltage) || reset)
    {
        if (!DiseqcReset()) 
        {
      	    ERRNO("DiseqcReset() failed");
    	    return false;
        }
        
        dvb_diseqc_master_cmd cmd = 
            {{CMD_FIRST, MASTER_TO_POSITIONER, GOTO, tuning.diseqc_port, 
              0x00, 0x00}, 4};

        if (!SendDiSEqCMessage(tuning,cmd)) 
        {
            ERRNO("Setting DiSEqC failed.\n");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.tone = tuning.tone;
        prev_tuning.voltage = tuning.voltage;
        havetuned = true;
    }

    return true;
}

bool DVBDiSEqC::PositionerStore(dvb_tuning_t& tuning)
{
    if (!DiseqcReset()) 
    {
      	ERRNO("DiseqcReset() failed");
    	return false;
    }

    GENERAL(QString("DiSEqC 1.2 Motor - Store Stored Position %1")
        .arg(tuning.diseqc_port));

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, STORE, tuning.diseqc_port , 0x00, 
          0x00}, 4};

    if (!SendDiSEqCMessage(cmd)) 
    {
        ERRNO("Setting DiSEqC failed.\n");
        return false;
    }

    return true;
}

bool DVBDiSEqC::PositionerStoreEastLimit()
{
    if (!DiseqcReset()) 
    {
      	ERRNO("DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, LIMIT_E, 0x00,0x00,0x00}, 3};

    if (!SendDiSEqCMessage(cmd)) 
    {
        ERRNO("Setting DiSEqC failed.\n");
        return false;
    }

    return true;
}

bool DVBDiSEqC::PositionerStoreWestLimit()
{
    if (!DiseqcReset()) 
    {
      	ERRNO("DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, LIMIT_W, 0x00,0x00,0x00}, 3};

    if (!SendDiSEqCMessage(cmd)) 
    {
        ERRNO("Setting DiSEqC failed.\n");
        return false;
    }

    return true;
}

bool DVBDiSEqC::PositionerStopMovement()
{
    if (!DiseqcReset()) 
    {
      	ERRNO("DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, HALT, 0x00,0x00,0x00}, 3};

    if (!SendDiSEqCMessage(cmd)) 
    {
        ERRNO("Setting DiSEqC failed.\n");
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
      	ERRNO("DiseqcReset() failed");
    	return false;
    }

    dvb_diseqc_master_cmd cmd = 
        {{CMD_FIRST, MASTER_TO_POSITIONER, LIMITS_OFF, 0x00,0x00,0x00}, 3};

    if (!SendDiSEqCMessage(cmd)) 
    {
        ERRNO("Setting DiSEqC failed.\n");
        return false;
    }

    return true;
}

/*****************************************************************************
                                Diseqc v1.3 (Goto X)
 ****************************************************************************/

bool DVBDiSEqC::PositionerGotoAngular(dvb_tuning_t& tuning, bool reset, 
                                      bool& havetuned) 
{
    // TODO: Send information here to FE saying motor is moving and
    //       to expect a longer than average tuning delay
    if (prev_tuning.diseqc_pos != tuning.diseqc_pos)
        GENERAL("DiSEqC Motor Moving");

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
      	ERRNO("DiseqcReset() failed");
    	return false;
    }

    // required db changes - get lat and lon for ground station location in db 
    // and added to tuning
    // sat_pos be passed into tuning, and be a float not an int./

    GENERAL(QString("DiSEqC 1.3 Motor - Goto Angular Position %1").arg(tuning.diseqc_pos));

    if ((prev_tuning.diseqc_port != tuning.diseqc_port ||
          prev_tuning.tone != tuning.tone ||
          prev_tuning.diseqc_pos != tuning.diseqc_pos ||
          prev_tuning.voltage != tuning.voltage) || reset)
    {

        dvb_diseqc_master_cmd cmd = 
            {{CMD_FIRST, MASTER_TO_POSITIONER, GOTO_ANGULAR, CMD1 , CMD2 , 
              0x00}, 5};

        if (!SendDiSEqCMessage(tuning,cmd)) 
        {
            ERRNO("Setting DiSEqC failed.\n");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.diseqc_pos = tuning.diseqc_pos;
        prev_tuning.tone = tuning.tone;
        prev_tuning.voltage = tuning.voltage;
        havetuned = true;
    }

    return true;
}
