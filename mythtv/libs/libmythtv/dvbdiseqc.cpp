/*
 *  Class DVBDiSEqC
 *
 *  Copyright (C) Kenneth Aafloy 2003
 *
 *  Description:
 *
 *  Author(s):
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
#include "pthread.h"
#include "qsqldatabase.h"

#include "mythcontext.h"
#include "tv_rec.h"

#include "dvbtypes.h"
#include "dvbdiseqc.h"

DVBDiSEqC::DVBDiSEqC(int _cardnum, int _fd_frontend):
    cardnum(_cardnum), fd_frontend(_fd_frontend)
{
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
        case 2: // v1.0
        case 3: // v1.1
            if (!OneWayProtocol(tuning, reset, havetuned))
                return false;
            break;
        default:
            ERROR("Unsupported DiSEqC type.");
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
    CHANNEL(QString("Tone Switch - Port %1/2").arg(tuning.diseqc_port));

    if (prev_tuning.diseqc_port != tuning.diseqc_port || reset)
    {
        if (tuning.diseqc_port > 2)
            ERROR("Tone Switches only supports two ports.");

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

bool DVBDiSEqC::OneWayProtocol(dvb_tuning_t& tuning, bool reset, bool& havetuned)
{
    if (tuning.diseqc_port != 0 &&
        ((prev_tuning.diseqc_port != tuning.diseqc_port ||
          prev_tuning.tone != tuning.tone ||
          prev_tuning.voltage != tuning.voltage) || reset))
    {
        if (tuning.diseqc_port > 4)
        {
            ERROR("Supports only up to 4-way switches.");
            return false;
        }

        if (!WritePortGroup(tuning))
        {
            ERROR("Setting DiSEqC failed.\n");
            return false;
        }

        prev_tuning.diseqc_port = tuning.diseqc_port;
        prev_tuning.tone = tuning.tone;
        prev_tuning.voltage = tuning.voltage;
        havetuned = true;
    }

    if (!ToneVoltageLnb(tuning, reset, havetuned))
        return false;

    return true;
}

bool DVBDiSEqC::WritePortGroup(dvb_tuning_t& tuning)
{
    struct dvb_diseqc_master_cmd cmd =
        {{CMD_FIRST, MASTER_TO_LSS, WRITE_N0, 0xf0, 0x00, 0x00}, 4};

    // param: high nibble: reset bits, low nibble set bits,
    // bits are: option, position, polarization, band.
    cmd.msg[DATA_1] = 0xf0 |
        ((((tuning.diseqc_port-1) * 4) & 0x0f) |
        ((tuning.voltage == SEC_VOLTAGE_13) ? 1 : 0) |
        ((tuning.tone == SEC_TONE_ON) ? 0 : 2));
   
    if (!PreDiseqcCmd())
        return false;

    /*
      See Eutelsat DiSEqC Update and recommendations for Implementation V2.1
      Sections 4.1.1, 4.2.1, 4.2.3. Note that the PreDiseqcCmd() may need to
      be set already in the start the correct voltage level, now it is set at
      the end as some Eutelsat instructions indicated.
    */

    if(ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) < 0)
    {
        ERRNO("OneWayProtocol: Sending command failed.");
        return false;
    }

    usleep(DISEQC_LONG_WAIT);

    int repeats = repeat;
    while(repeats--)
    {
        if(tuning.diseqc_type == 3)
        {
            cmd.msg[COMMAND] = WRITE_N1; // uncommitted
            if(ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) <0)
            {
                ERRNO("OneWayProtocol: Sending uncommitted command failed.");
                return false;
            }
            usleep(DISEQC_LONG_WAIT);
            cmd.msg[COMMAND] = WRITE_N0; // change back to committed
        }

        if(ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) < 0)
        {
            ERRNO("OneWayProtocol: Sending committed command failed.");
            return false;
        }
        usleep(DISEQC_LONG_WAIT);
    } 

    usleep(DISEQC_SHORT_WAIT);
    if(ioctl(fd_frontend, FE_DISEQC_SEND_BURST,
        ((tuning.diseqc_port-1) / 4) % 2 ? SEC_MINI_B : SEC_MINI_A) < 0)
    {
        ERRNO("OneWayProtocol: Sending burst failed.");
        return false;
    }
    usleep(DISEQC_SHORT_WAIT);
    
    return true;
}

bool DVBDiSEqC::PreDiseqcCmd()
{
    usleep(DISEQC_SHORT_WAIT);
    if (ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF) < 0)
    {
        ERRNO("OneWayProtocol: Setting tone off failed.");
        return false;
    }
    usleep(DISEQC_SHORT_WAIT);

    if (ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_13) < 0)
    {
        ERRNO("OneWayProtocol: Setting voltage failed.");
        return false;
    }
    usleep(DISEQC_SHORT_WAIT);

    return true;
}

bool DVBDiSEqC::DiseqcReset()
{
    struct dvb_diseqc_master_cmd reset_cmd =
       {{CMD_FIRST, MASTER_TO_LSS, RESET, 0x00, 0x00, 0x00}, 3};
 
    struct dvb_diseqc_master_cmd init_cmd =
       {{CMD_FIRST, MASTER_TO_LSS, POWERON, 0x00, 0x00, 0x00}, 3};

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

bool DVBDiSEqC::Positioner_DriveEast(int timestep)
{
    (void)timestep;
    return false;
}

bool DVBDiSEqC::Positioner_DriveWest(int timestep)
{
    (void)timestep;
    return false;
}

bool DVBDiSEqC::Positioner_Goto(int satpos)
{
    (void)satpos;
    return false;
}

bool DVBDiSEqC::Positioner_Store(int satpos)
{
    (void)satpos;
    return false;
}

bool DVBDiSEqC::Positioner_StoreEastLimit()
{
    return false;
}

bool DVBDiSEqC::Positioner_StoreWestLimit()
{
    return false;
}

bool DVBDiSEqC::Positioner_Halt()
{
    return false;
}

bool DVBDiSEqC::Positioner_Status()
{
    return false;
}

bool DVBDiSEqC::Positioner_DisableLimits()
{
    return false;
}

/*****************************************************************************
                                Diseqc v2
 ****************************************************************************/

