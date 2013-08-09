/* Programs.cpp

   Copyright (C)  David C. J. Matthews 2004  dm at prolingua.co.uk

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

#include "Programs.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"
#include "Logging.h"
#include "freemheg.h"

#include <QStringList>
#if HAVE_GETTIMEOFDAY == 0
#include <sys/timeb.h>
#endif
#ifdef __FreeBSD__
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "config.h"
#include "compat.h"

/*
 * Resident programs are subroutines to provide various string and date functions
 * but also interface to surrounding tuner.  They are defined in the UK MHEG profile.
 * As with many of the more abstruse aspects of MHEG they are not all used in any
 * applications I've seen so I haven't implemented them.
 */


MHProgram::MHProgram()
{
    m_fInitiallyAvailable = true; // Default true
}

void MHProgram::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHIngredient::Initialise(p, engine);
    MHParseNode *pCmdNode = p->GetNamedArg(C_NAME);

    if (pCmdNode)
    {
        pCmdNode->GetArgN(0)->GetStringValue(m_Name);    // Program name
    }

    MHParseNode *pAvail = p->GetNamedArg(C_INITIALLY_AVAILABLE);

    if (pAvail)
    {
        m_fInitiallyAvailable = pAvail->GetArgN(0)->GetBoolValue();
    }

    // The MHEG Standard says that InitiallyAvailable is mandatory and should be false.
    // That doesn't seem to be the case in some MHEG programs so we force it here.
    m_fInitiallyActive = false;
}

void MHProgram::PrintMe(FILE *fd, int nTabs) const
{
    MHIngredient::PrintMe(fd, nTabs);
    PrintTabs(fd, nTabs);
    fprintf(fd, ":Name ");
    m_Name.PrintMe(fd, 0);
    fprintf(fd, "\n");

    if (! m_fInitiallyAvailable)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":InitiallyAvailable false");
        fprintf(fd, "\n");
    }
}

// Set "running" and generate an IsRunning event.
void MHProgram::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHIngredient::Activation(engine);
    m_fRunning = true;
    engine->EventTriggered(this, EventIsRunning);
}

// This can be called if we change scene while a forked program is running
void MHProgram::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    // TODO: Stop the forked program.
    MHIngredient::Deactivation(engine);
}

void MHResidentProgram::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:ResidentPrg ");
    MHProgram::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

static void SetSuccessFlag(const MHObjectRef &success, bool result, MHEngine *engine)
{
    engine->FindObject(success)->SetVariableValue(result);
}

// Return an integer value.  May throw an exception if it isn't the correct type.
static int GetInt(MHParameter *parm, MHEngine *engine)
{
    MHUnion un;
    un.GetValueFrom(*parm, engine);
    un.CheckType(MHUnion::U_Int);
    return un.m_nIntVal;
}

// Return a bool value.  May throw an exception if it isn't the correct type.
static bool GetBool(MHParameter *parm, MHEngine *engine)
{
    MHUnion un;
    un.GetValueFrom(*parm, engine);
    un.CheckType(MHUnion::U_Bool);
    return un.m_fBoolVal;
}

// Extract a string value.
static void GetString(MHParameter *parm, MHOctetString &str, MHEngine *engine)
{
    MHUnion un;
    un.GetValueFrom(*parm, engine);
    un.CheckType(MHUnion::U_String);
    str.Copy(un.m_StrVal);
}

void MHResidentProgram::CallProgram(bool fIsFork, const MHObjectRef &success, const MHSequence<MHParameter *> &args, MHEngine *engine)
{
    if (! m_fAvailable)
    {
        Preparation(engine);
    }

    //  if (m_fRunning) return; // Strictly speaking there should be only one instance of a program running at a time.
    Activation(engine);
    MHLOG(MHLogDetail, QString("Calling program %1").arg(m_Name.Printable()));

    try
    {
        // Run the code.
        if (m_Name.Equal("GCD"))   // GetCurrentDate - returns local time.
        {
            if (args.Size() == 2)
            {
                time_t epochSeconds = 0;
                short int timeZone = 0;
#if HAVE_GETTIMEOFDAY
                struct timeval   time;
                struct timezone  zone;

                if (gettimeofday(&time, &zone) == -1)
                {
                    MHLOG(MHLogDetail, QString("gettimeofday() failed"));
                }

                epochSeconds     = time.tv_sec;
                timeZone = zone.tz_minuteswest;
#elif HAVE_FTIME
                struct timeb timebuffer;
                if (ftime(&timebuffer) == -1)
                {
                    MHLOG(MHLogDetail, QString("ftime() failed"));
                }
                epochSeconds = timebuffer.time;
                timeZone = timebuffer.timezone;
#else
#error Configuration error? No ftime() or gettimeofday()?
#endif
                // Adjust the time to local.  TODO: Check this.
                epochSeconds -= timeZone * 60;
                // Time as seconds since midnight.
                int nTimeAsSecs = epochSeconds % (24 * 60 * 60);
                // Modified Julian date as number of days since 17th November 1858.
                // 1st Jan 1970 was date 40587.
                int nModJulianDate = 40587 + epochSeconds / (24 * 60 * 60);

                engine->FindObject(*(args.GetAt(0)->GetReference()))->SetVariableValue(nModJulianDate);
                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(nTimeAsSecs);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("FDa"))   // FormatDate
        {
            if (args.Size() == 4)
            {
                // This is a bit like strftime but not quite.
                MHOctetString format;
                GetString(args.GetAt(0), format, engine);
                int date = GetInt(args.GetAt(1), engine); // As produced in GCD
                int time = GetInt(args.GetAt(2), engine);
                // Convert to a Unix date (secs since 1st Jan 1970) but adjusted for time zone.
                time_t timet = (date - 40587) * (24 * 60 * 60) + time;
                struct tm *timeStr = gmtime(&timet);
                MHOctetString result;

                for (int i = 0; i < format.Size(); i++)
                {
                    unsigned char ch = format.GetAt(i);
                    char buffer[5]; // Largest text is 4 chars for a year + null terminator

                    if (ch == '%')
                    {
                        i++;

                        if (i == format.Size())
                        {
                            break;
                        }

                        ch = format.GetAt(i);
                        buffer[0] = 0;

                        switch (ch)
                        {
                            case 'Y':
                                sprintf(buffer, "%04d", timeStr->tm_year + 1900);
                                break;
                            case 'y':
                                sprintf(buffer, "%02d", timeStr->tm_year % 100);
                                break;
                            case 'X':
                                sprintf(buffer, "%02d", timeStr->tm_mon + 1);
                                break;
                            case 'x':
                                sprintf(buffer, "%1d", timeStr->tm_mon + 1);
                                break;
                            case 'D':
                                sprintf(buffer, "%02d", timeStr->tm_mday);
                                break;
                            case 'd':
                                sprintf(buffer, "%1d", timeStr->tm_mday);
                                break;
                            case 'H':
                                sprintf(buffer, "%02d", timeStr->tm_hour);
                                break;
                            case 'h':
                                sprintf(buffer, "%1d", timeStr->tm_hour);
                                break;
                            case 'I':

                                if (timeStr->tm_hour == 12 || timeStr->tm_hour == 0)
                                {
                                    strcpy(buffer, "12");
                                }
                                else
                                {
                                    sprintf(buffer, "%02d", timeStr->tm_hour % 12);
                                }

                                break;
                            case 'i':

                                if (timeStr->tm_hour == 12 || timeStr->tm_hour == 0)
                                {
                                    strcpy(buffer, "12");
                                }
                                else
                                {
                                    sprintf(buffer, "%1d", timeStr->tm_hour % 12);
                                }

                                break;
                            case 'M':
                                sprintf(buffer, "%02d", timeStr->tm_min);
                                break;
                            case 'm':
                                sprintf(buffer, "%1d", timeStr->tm_min);
                                break;
                            case 'S':
                                sprintf(buffer, "%02d", timeStr->tm_sec);
                                break;
                            case 's':
                                sprintf(buffer, "%1d", timeStr->tm_sec);
                                break;
                                // TODO: These really should be localised.
                            case 'A':

                                if (timeStr->tm_hour < 12)
                                {
                                    strcpy(buffer, "AM");
                                }
                                else
                                {
                                    strcpy(buffer, "PM");
                                }

                                break;
                            case 'a':

                                if (timeStr->tm_hour < 12)
                                {
                                    strcpy(buffer, "am");
                                }
                                else
                                {
                                    strcpy(buffer, "pm");
                                }

                                break;
                            default:
                                buffer[0] = ch;
                                buffer[1] = 0;
                        }

                        result.Append(buffer);
                    }
                    else
                    {
                        result.Append(MHOctetString(&ch, 1));
                    }
                }

                MHParameter *pResString = args.GetAt(3);
                engine->FindObject(*(pResString->GetReference()))->SetVariableValue(result);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("GDW"))   // GetDayOfWeek - returns the day of week that the date occurred on.
        {
            if (args.Size() == 2)
            {
                int date = GetInt(args.GetAt(0), engine); // Date as produced in GCD
                // Convert to a Unix date (secs since 1st Jan 1970) but adjusted for time zone.
                time_t timet = (date - 40587) * (24 * 60 * 60);
                struct tm *timeStr = gmtime(&timet);
                // 0 => Sunday, 1 => Monday etc.
                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(timeStr->tm_wday);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("Rnd"))   // Random
        {
            if (args.Size() == 2)
            {
                int nLimit = GetInt(args.GetAt(0), engine);
                MHParameter *pResInt = args.GetAt(1);
                int r = random() % (nLimit + 1);
                engine->FindObject(
                    *(pResInt->GetReference()))->SetVariableValue(r);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("CTC"))   // CastToContentRef
        {
            // Converts a string to a ContentRef.
            if (args.Size() == 2)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                MHContentRef result;
                result.m_ContentRef.Copy(string);
                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(result);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("CTO"))   // CastToObjectRef
        {
            // Converts a string and an integer to an ObjectRef.
            if (args.Size() == 3)
            {
                MHObjectRef result;
                GetString(args.GetAt(0), result.m_GroupId, engine);
                result.m_nObjectNo = GetInt(args.GetAt(1), engine);
                engine->FindObject(*(args.GetAt(2)->GetReference()))->SetVariableValue(result);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("GSL"))   // GetStringLength
        {
            if (args.Size() == 2)
            {
                // Find a substring within a string and return an index to the position.
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                MHParameter *pResInt = args.GetAt(1);
                SetSuccessFlag(success, true, engine);
                engine->FindObject(*(pResInt->GetReference()))->SetVariableValue(string.Size());
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("GSS"))   // GetSubString
        {
            if (args.Size() == 4)   // Extract a sub-string from a string.
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                int nBeginExtract = GetInt(args.GetAt(1), engine);
                int nEndExtract = GetInt(args.GetAt(2), engine);

                if (nBeginExtract < 1)
                {
                    nBeginExtract = 1;
                }

                if (nBeginExtract > string.Size())
                {
                    nBeginExtract = string.Size();
                }

                if (nEndExtract < 1)
                {
                    nEndExtract = 1;
                }

                if (nEndExtract > string.Size())
                {
                    nEndExtract = string.Size();
                }

                MHParameter *pResString = args.GetAt(3);
                // Returns beginExtract to endExtract inclusive.
                engine->FindObject(*(pResString->GetReference()))->SetVariableValue(
                    MHOctetString(string, nBeginExtract - 1, nEndExtract - nBeginExtract + 1));
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("SSS"))   // SearchSubString
        {
            if (args.Size() == 4)
            {
                // Find a substring within a string and return an index to the position.
                MHOctetString string, searchString;
                GetString(args.GetAt(0), string, engine);
                int nStart = GetInt(args.GetAt(1), engine);

                if (nStart < 1)
                {
                    nStart = 1;
                }

                GetString(args.GetAt(2), searchString, engine);
                // Strings are indexed from one.
                int nPos;

                for (nPos = nStart - 1; nPos <= string.Size() - searchString.Size(); nPos++)
                {
                    int i;

                    for (i = 0; i < searchString.Size(); i++)
                    {
                        if (searchString.GetAt(i) != string.GetAt(i + nPos))
                        {
                            break;
                        }
                    }

                    if (i == searchString.Size())
                    {
                        break;    // Found a match.
                    }
                }

                // Set the result.
                MHParameter *pResInt = args.GetAt(3);
                SetSuccessFlag(success, true, engine); // Set this first.

                if (nPos <= string.Size() - searchString.Size())   // Found
                {
                    // Set the index to the position of the string, counting from 1.
                    engine->FindObject(*(pResInt->GetReference()))->SetVariableValue(nPos + 1);
                }
                else   // Not found.  Set the result index to -1
                {
                    engine->FindObject(*(pResInt->GetReference()))->SetVariableValue(-1);
                }
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("SES"))   // SearchAndExtractSubString
        {
            if (args.Size() == 5)
            {
                // Find a substring within a string and return an index to the position
                // and the prefix to the substring.
                MHOctetString string, searchString;
                GetString(args.GetAt(0), string, engine);
                int nStart = GetInt(args.GetAt(1), engine);

                if (nStart < 1)
                {
                    nStart = 1;
                }

                GetString(args.GetAt(2), searchString, engine);
                // Strings are indexed from one.
                int nPos;

                for (nPos = nStart - 1; nPos <= string.Size() - searchString.Size(); nPos++)
                {
                    int i;

                    for (i = 0; i < searchString.Size(); i++)
                    {
                        if (searchString.GetAt(i) != string.GetAt(i + nPos))
                        {
                            break;    // Doesn't match
                        }
                    }

                    if (i == searchString.Size())
                    {
                        break;    // Found a match.
                    }
                }

                // Set the results.
                MHParameter *pResString = args.GetAt(3);
                MHParameter *pResInt = args.GetAt(4);
                SetSuccessFlag(success, true, engine); // Set this first.

                if (nPos <= string.Size() - searchString.Size())   // Found
                {
                    // Set the index to the position AFTER the string, counting from 1.
                    engine->FindObject(*(pResInt->GetReference()))->SetVariableValue(nPos + 1 + searchString.Size());
                    // Return the sequence from nStart-1 of length nPos-nStart+1
                    MHOctetString resultString(string, nStart - 1, nPos - nStart + 1);
                    engine->FindObject(*(pResString->GetReference()))->SetVariableValue(resultString);
                }
                else   // Not found.  Set the result string to empty and the result index to -1
                {
                    engine->FindObject(*(pResInt->GetReference()))->SetVariableValue(-1);
                    engine->FindObject(*(pResString->GetReference()))->SetVariableValue(MHOctetString(""));
                }
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("GSI"))   // SI_GetServiceIndex
        {
            // Returns an index indicating the service
            if (args.Size() == 2)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                MHParameter *pResInt = args.GetAt(1);
                // The format of the service is dvb://netID.[transPortID].serviceID
                // where the IDs are in hex.
                // or rec://svc/lcn/N where N is the "logical channel number" i.e. the Freeview channel.
                QString str = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                int nResult = engine->GetContext()->GetChannelIndex(str);
                engine->FindObject(*(pResInt->GetReference()))->SetVariableValue(nResult);
                SetSuccessFlag(success, nResult >= 0, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("TIn"))   // SI_TuneIndex - Fork not allowed
        {
            // Tunes to an index returned by GSI
            if (args.Size() == 1)
            {
                int nChannel = GetInt(args.GetAt(0), engine);
                bool res = nChannel >= 0 ? engine->GetContext()->TuneTo(
                               nChannel, engine->GetTuneInfo()) : false;
                SetSuccessFlag(success, res, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_Name.Equal("TII"))   // SI_TuneIndexInfo
        {
            // Indicates whether to perform a subsequent TIn quietly or normally.
            if (args.Size() == 1)
            {
                int tuneinfo = GetInt(args.GetAt(0), engine);
                engine->SetTuneInfo(tuneinfo);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_Name.Equal("BSI"))   // SI_GetBasicSI
        {
            // Returns basic SI information about the service indicated by an index
            // returned by GSI.
            // Returns networkID, origNetworkID, transportStreamID, serviceID
            if (args.Size() == 5)
            {
                int channelId = GetInt(args.GetAt(0), engine);
                int netId, origNetId, transportId, serviceId;
                // Look the information up in the database.
                bool res = engine->GetContext()->GetServiceInfo(channelId, netId, origNetId,
                                                                transportId, serviceId);

                if (res)
                {
                    engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(netId);
                    engine->FindObject(*(args.GetAt(2)->GetReference()))->SetVariableValue(origNetId);
                    engine->FindObject(*(args.GetAt(3)->GetReference()))->SetVariableValue(transportId);
                    engine->FindObject(*(args.GetAt(4)->GetReference()))->SetVariableValue(serviceId);
                }

                SetSuccessFlag(success, res, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_Name.Equal("GBI"))   // GetBootInfo
        {
            // Gets the NB_info field.
            MHERROR("GetBootInfo ResidentProgram is not implemented");
        }
        else if (m_Name.Equal("CCR"))   // CheckContentRef
        {
            // Sees if an item with a particular content reference is available
            // in the carousel.  This looks like it should block until the file
            // is available.  The profile recommends that this should be forked
            // rather than called.
            if (args.Size() == 3)
            {
                MHUnion un;
                un.GetValueFrom(*(args.GetAt(0)), engine);
                un.CheckType(MHUnion::U_ContentRef);
                MHContentRef fileName;
                fileName.Copy(un.m_ContentRefVal);
                QString csPath = engine->GetPathName(fileName.m_ContentRef);
                bool result = false;
                QByteArray text;

                // Try to load the object.
                if (! csPath.isEmpty())
                {
                    result = engine->GetContext()->GetCarouselData(csPath, text);
                }

                // Set the result variable.
                MHParameter *pResFlag = args.GetAt(1);
                engine->FindObject(*(pResFlag->GetReference()))->SetVariableValue(result);
                MHParameter *pResCR = args.GetAt(2);
                // Copy the file name to the resulting content ref.
                engine->FindObject(*(pResCR->GetReference()))->SetVariableValue(fileName);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_Name.Equal("CGR"))   // CheckGroupIDRef
        {
            // Sees if an application or scene with a particular group id
            // is available in the carousel.
            MHERROR("CheckGroupIDRef ResidentProgram is not implemented");
        }
        else if (m_Name.Equal("VTG"))   // VideoToGraphics
        {
            // Video to graphics transformation.
            MHERROR("VideoToGraphics ResidentProgram is not implemented");
        }
        else if (m_Name.Equal("SWA"))   // SetWidescreenAlignment
        {
            // Sets either LetterBox or Centre-cut-out mode.
            // Seems to be concerned with aligning a 4:3 scene with an underlying 16:9 video
            MHERROR("SetWidescreenAlignment ResidentProgram is not implemented");
        }
        else if (m_Name.Equal("GDA"))   // GetDisplayAspectRatio
        {
            // Returns the aspcet ratio.  4:3 => 1, 16:9 => 2
            MHERROR("GetDisplayAspectRatio ResidentProgram is not implemented");
        }
        else if (m_Name.Equal("CIS"))   // CI_SendMessage
        {
            // Sends a message to a DVB CI application
            MHERROR("CI_SendMessage ResidentProgram is not implemented");
        }
        else if (m_Name.Equal("SSM"))   // SetSubtitleMode
        {
            // Enable or disable subtitles in addition to MHEG.
            if (args.Size() == 1) {
                bool status = GetBool(args.GetAt(0), engine);
                MHLOG(MHLogNotifications, QString("NOTE SetSubtitleMode %1")
                    .arg(status ? "enabled" : "disabled"));
                // TODO Notify player
                SetSuccessFlag(success, true, engine);
            }
            else SetSuccessFlag(success, false, engine);
        }

        else if (m_Name.Equal("WAI"))   // WhoAmI
        {
            // Return a concatenation of the strings we respond to in
            // GetEngineSupport(UKEngineProfile(X))
            if (args.Size() == 1)
            {
                MHOctetString result;
                result.Copy(engine->MHEGEngineProviderIdString);
                result.Append(" ");
                result.Append(engine->GetContext()->GetReceiverId());
                result.Append(" ");
                result.Append(engine->GetContext()->GetDSMCCId());
                engine->FindObject(*(args.GetAt(0)->GetReference()))->SetVariableValue(result);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_Name.Equal("DBG"))   // Debug - optional
        {
            QString message = "DEBUG: ";

            for (int i = 0; i < args.Size(); i++)
            {
                MHUnion un;
                un.GetValueFrom(*(args.GetAt(i)), engine);

                switch (un.m_Type)
                {
                    case MHUnion::U_Int:
                        message.append(QString("%1").arg(un.m_nIntVal));
                        break;
                    case MHParameter::P_Bool:
                        message.append(un.m_fBoolVal ? "True" : "False");
                        break;
                    case MHParameter::P_String:
                        message.append(QString::fromUtf8((const char *)un.m_StrVal.Bytes(), un.m_StrVal.Size()));
                        break;
                    case MHParameter::P_ObjRef:
                        message.append(un.m_ObjRefVal.Printable());
                        break;
                    case MHParameter::P_ContentRef:
                        message.append(un.m_ContentRefVal.Printable());
                        break;
                    case MHParameter::P_Null:
                        break;
                }
            }

            MHLOG(MHLogNotifications, message);
        }

        else if (m_Name.Equal("SBI"))   // SetBroadcastInterrupt
        {
            // Required for NativeApplicationExtension
            // En/dis/able program interruptions e.g. green button
            if (args.Size() == 1) {
                bool status = GetBool(args.GetAt(0), engine);
                MHLOG(MHLogNotifications, QString("NOTE SetBroadcastInterrupt %1")
                    .arg(status ? "enabled" : "disabled"));
                // Nothing todo at present
                SetSuccessFlag(success, true, engine);
            }
            else SetSuccessFlag(success, false, engine);
        }

        // InteractionChannelExtension
        else if (m_Name.Equal("GIS")) { // GetICStatus
            if (args.Size() == 1)
            {
                int ICstatus = engine->GetContext()->GetICStatus();
                MHLOG(MHLogNotifications, "NOTE InteractionChannel " + QString(
                    ICstatus == 0 ? "active" : ICstatus == 1 ? "inactive" :
                    ICstatus == 2 ? "disabled" : "undefined"));
                engine->FindObject(*(args.GetAt(0)->GetReference()))->SetVariableValue(ICstatus);
                SetSuccessFlag(success, true, engine);
            }
            else SetSuccessFlag(success, false, engine);
        }
        else if (m_Name.Equal("RDa")) { // ReturnData
            if (args.Size() >= 3)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                QString url = QString::fromUtf8((const char *)string.Bytes(), string.Size());

                // Variable name/value pairs
                QStringList params;
                int i = 1;
                for (; i + 2 < args.Size(); i += 2)
                {
                    GetString(args.GetAt(i), string, engine);
                    QString name = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                    QString val;
                    MHUnion un;
                    un.GetValueFrom(*(args.GetAt(i+1)), engine);
                    switch (un.m_Type) {
                    case MHUnion::U_Int:
                        val = QString::number(un.m_nIntVal);
                        break;
                    case MHParameter::P_Bool:
                        val = un.m_fBoolVal ? "true" : "false";
                        break;
                    case MHParameter::P_String:
                        val = QString::fromUtf8((const char*)un.m_StrVal.Bytes(), un.m_StrVal.Size());
                        break;
                    case MHParameter::P_ObjRef:
                        val = un.m_ObjRefVal.Printable();
                        break;
                    case MHParameter::P_ContentRef:
                        val = un.m_ContentRefVal.Printable();
                        break;
                    case MHParameter::P_Null:
                        val = "<NULL>";
                        break;
                    default:
                        val = QString("<type %1>").arg(un.m_Type);
                        break;
                    }
                    params += name + "=" + val;
                }
                // TODO
                MHLOG(MHLogNotifications, "NOTE ReturnData '" + url + "' { " + params.join(" ") + " }");
                // HTTP response code, 0= none
                engine->FindObject(*(args.GetAt(i)->GetReference()))->SetVariableValue(0);
                // HTTP response data
                string = "";
                engine->FindObject(*(args.GetAt(i+1)->GetReference()))->SetVariableValue(string);
                SetSuccessFlag(success, false, engine);
            }
            else SetSuccessFlag(success, false, engine);
        }
        else if (m_Name.Equal("SHF")) { // SetHybridFileSystem
            if (args.Size() == 2)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                QString str = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                GetString(args.GetAt(1), string, engine);
                QString str2 = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                // TODO
                MHLOG(MHLogNotifications, QString("NOTE SetHybridFileSystem %1=%2")
                    .arg(str).arg(str2));
                SetSuccessFlag(success, false, engine);
            }
            else SetSuccessFlag(success, false, engine);
        }

        else
        {
            MHERROR(QString("Unknown ResidentProgram %1").arg(m_Name.Printable()));
        }
    }
    catch (char const *)
    {
        // If something went wrong set the succeeded flag to false
        SetSuccessFlag(success, false, engine);
        // And continue on.  In particular we need to deactivate.
    }

    Deactivation(engine);

    // At the moment we always treat Fork as Call.  If we do get a Fork we should signal that we're done.
    if (fIsFork)
    {
        engine->EventTriggered(this, EventAsyncStopped);
    }
}

MHRemoteProgram::MHRemoteProgram()
{

}

MHRemoteProgram::~MHRemoteProgram()
{

}

void MHRemoteProgram::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHProgram::Initialise(p, engine);
    //
}

void MHRemoteProgram::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:RemotePrg");
    MHProgram::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

MHInterChgProgram::MHInterChgProgram()
{

}

MHInterChgProgram::~MHInterChgProgram()
{

}

void MHInterChgProgram::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHProgram::Initialise(p, engine);
    //
}

void MHInterChgProgram::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:InterchgPrg");
    MHProgram::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

// Actions.
void MHCall::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    m_Succeeded.Initialise(p->GetArgN(1), engine); // Call/fork succeeded flag
    // Arguments.
    MHParseNode *args = p->GetArgN(2);

    for (int i = 0; i < args->GetSeqCount(); i++)
    {
        MHParameter *pParm = new MHParameter;
        m_Parameters.Append(pParm);
        pParm->Initialise(args->GetSeqN(i), engine);
    }
}

void MHCall::PrintArgs(FILE *fd, int nTabs) const
{
    m_Succeeded.PrintMe(fd, nTabs);
    fprintf(fd, " ( ");

    for (int i = 0; i < m_Parameters.Size(); i++)
    {
        m_Parameters.GetAt(i)->PrintMe(fd, 0);
    }

    fprintf(fd, " )");
}

void MHCall::Perform(MHEngine *engine)
{
    // Output parameters are handled by IndirectRefs so we don't evaluate the parameters here.
    Target(engine)->CallProgram(m_fIsFork, m_Succeeded, m_Parameters, engine);
}
