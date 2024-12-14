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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/
#include <algorithm>
#include <limits>
#include <random>

#include "Programs.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"
#include "Logging.h"
#include "freemheg.h"

#include <QDateTime>
#include <QLocale>
#include <QStringList>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>

/*
 * Resident programs are subroutines to provide various string and date functions
 * but also interface to surrounding tuner.  They are defined in the UK MHEG profile.
 * As with many of the more abstruse aspects of MHEG they are not all used in any
 * applications I've seen so I haven't implemented them.
 */

void MHProgram::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHIngredient::Initialise(p, engine);
    MHParseNode *pCmdNode = p->GetNamedArg(C_NAME);

    if (pCmdNode)
    {
        pCmdNode->GetArgN(0)->GetStringValue(m_name);    // Program name
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
    m_name.PrintMe(fd, 0);
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
    str.Copy(un.m_strVal);
}

/** @brief Midnight on 17 November 1858, the epoch of the modified Julian day.

This is Qt::LocalTime, to match GetCurrentDate's use of local time.

ETSI ES 202 184 V2.4.1 (2016-06) does not mention timezones.
§11.10.4.2 GetCurrentDate "Retrieves the current @e local date and time."
Emphasis mine.

Therefore, for consistency, I will assume all dates are in the local timezone.
Thus, the meaning of FormatDate using the output of GetCurrentDate is equivalent
to QDateTime::currentDateTime().toString(…) with a suitably converted format string.
*/
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
static const QDateTime k_mJD_epoch = QDateTime(QDate(1858, 11, 17), QTime(0, 0), Qt::LocalTime);
#else
static const QDateTime k_mJD_epoch = QDateTime(QDate(1858, 11, 17), QTime(0, 0),
                                               QTimeZone(QTimeZone::LocalTime));
#endif

// match types with Qt
static inline QDateTime recoverDateTime(int64_t mJDN, int64_t seconds)
{
    QDateTime dt = k_mJD_epoch;
    return dt.addDays(mJDN).addSecs(seconds);
}

static void GetCurrentDate(int64_t& mJDN, int& seconds)
{
    auto dt = QDateTime::currentDateTime();
    mJDN    = k_mJD_epoch.daysTo(dt); // returns a qint64
    seconds = dt.time().msecsSinceStartOfDay() / 1000;
}

void MHResidentProgram::CallProgram(bool fIsFork, const MHObjectRef &success, const MHSequence<MHParameter *> &args, MHEngine *engine)
{
    if (! m_fAvailable)
    {
        Preparation(engine);
    }

    //  if (m_fRunning) return; // Strictly speaking there should be only one instance of a program running at a time.
    Activation(engine);
    MHLOG(MHLogDetail, QString("Calling program %1").arg(m_name.Printable()));

    try
    {
        // Run the code.
        if (m_name.Equal("GCD"))   // GetCurrentDate - returns local time.
        {
            if (args.Size() == 2)
            {
                int64_t mJDN = 0;
                int nTimeAsSecs = 0;
                GetCurrentDate(mJDN, nTimeAsSecs);
                int nModJulianDate = std::clamp<int64_t>(mJDN, 0, std::numeric_limits<int>::max());

                engine->FindObject(*(args.GetAt(0)->GetReference()))->SetVariableValue(nModJulianDate);
                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(nTimeAsSecs);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_name.Equal("FDa"))   // FormatDate
        {
            if (args.Size() == 4)
            {
                // This is a bit like strftime but not quite.
                MHOctetString format;
                GetString(args.GetAt(0), format, engine);
                int date = GetInt(args.GetAt(1), engine); // As produced in GCD
                int time = GetInt(args.GetAt(2), engine);

                QDateTime dt = recoverDateTime(date, time);
                MHOctetString result;

                for (int i = 0; i < format.Size(); i++)
                {
                    char ch = format.GetAt(i);
                    QString buffer {};

                    if (ch == '%')
                    {
                        i++;

                        if (i == format.Size())
                        {
                            break;
                        }

                        ch = format.GetAt(i);

                        switch (ch)
                        {
                            case 'Y': buffer = dt.toString("yyyy"); break;
                            case 'y': buffer = dt.toString("yy");   break;
                            case 'X': buffer = dt.toString("MM");   break;
                            case 'x': buffer = dt.toString("M");    break;
                            case 'D': buffer = dt.toString("dd");   break;
                            case 'd': buffer = dt.toString("d");    break;
                            case 'H': buffer = dt.toString("HH");   break;
                            case 'h': buffer = dt.toString("H");    break;
                            case 'I':
                                // Need AM/PM to get hours as 1-12
                                buffer = dt.toString("HH AP");
                                buffer.chop(3);
                                break;
                            case 'i':
                                buffer = dt.toString("H AP");
                                buffer.chop(3);
                                break;
                            case 'M': buffer = dt.toString("mm");   break;
                            case 'm': buffer = dt.toString("m");    break;
                            case 'S': buffer = dt.toString("ss");   break;
                            case 's': buffer = dt.toString("s");    break;
                            case 'A': buffer = dt.toString("AP");   break;
                            case 'a': buffer = dt.toString("ap");   break;
                            default:
                                buffer= ch;
                        }

                        result.Append(qPrintable(buffer));
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

        else if (m_name.Equal("GDW"))   // GetDayOfWeek - returns the day of week that the date occurred on.
        {
            if (args.Size() == 2)
            {
                int date = GetInt(args.GetAt(0), engine); // Date as produced in GCD
                QDateTime dt = recoverDateTime(date, 0);
                // ETSI ES 202 184 V2.4.1 (2016-06) §11.10.4.4 GetDayOfWeek
                // specifies "0 represents Sunday, 1 Monday, etc."

                int dayOfWeek = dt.date().dayOfWeek();
                // Gregorian calendar, returns 0 if invalid, 1 = Monday to 7 = Sunday
                if (dayOfWeek == 7)
                {
                    dayOfWeek = 0;
                }
                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(dayOfWeek);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_name.Equal("Rnd"))   // Random
        {
            if (args.Size() == 2)
            {
                int nLimit = GetInt(args.GetAt(0), engine);
                MHParameter *pResInt = args.GetAt(1);
                // ETSI ES 202 184 V2.4.1 (2016-06) §11.10.5 Random number function
                // specifies "The returned value is undefined if the num parameter < 1."
                // so this is fine.
                static std::random_device rd;
                static std::mt19937 generator {rd()};
                std::uniform_int_distribution<int> distrib {0, nLimit};
                int r = distrib(generator);
                engine->FindObject(
                    *(pResInt->GetReference()))->SetVariableValue(r);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_name.Equal("CTC"))   // CastToContentRef
        {
            // Converts a string to a ContentRef.
            if (args.Size() == 2)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                MHContentRef result;
                result.m_contentRef.Copy(string);
                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(result);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_name.Equal("CTO"))   // CastToObjectRef
        {
            // Converts a string and an integer to an ObjectRef.
            if (args.Size() == 3)
            {
                MHObjectRef result;
                GetString(args.GetAt(0), result.m_groupId, engine);
                result.m_nObjectNo = GetInt(args.GetAt(1), engine);
                engine->FindObject(*(args.GetAt(2)->GetReference()))->SetVariableValue(result);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_name.Equal("GSL"))   // GetStringLength
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

        else if (m_name.Equal("GSS"))   // GetSubString
        {
            if (args.Size() == 4)   // Extract a sub-string from a string.
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                int nBeginExtract = GetInt(args.GetAt(1), engine);
                int nEndExtract = GetInt(args.GetAt(2), engine);

                nBeginExtract = std::clamp(nBeginExtract, 1, string.Size());
                nEndExtract = std::clamp(nEndExtract, 1, string.Size());

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

        else if (m_name.Equal("SSS"))   // SearchSubString
        {
            if (args.Size() == 4)
            {
                // Find a substring within a string and return an index to the position.
                MHOctetString string;
                MHOctetString searchString;
                GetString(args.GetAt(0), string, engine);
                int nStart = GetInt(args.GetAt(1), engine);

                nStart = std::max(nStart, 1);

                GetString(args.GetAt(2), searchString, engine);
                // Strings are indexed from one.
                int nPos = 0;

                for (nPos = nStart - 1; nPos <= string.Size() - searchString.Size(); nPos++)
                {
                    int i = 0;

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

        else if (m_name.Equal("SES"))   // SearchAndExtractSubString
        {
            if (args.Size() == 5)
            {
                // Find a substring within a string and return an index to the position
                // and the prefix to the substring.
                MHOctetString string;
                MHOctetString searchString;
                GetString(args.GetAt(0), string, engine);
                int nStart = GetInt(args.GetAt(1), engine);

                nStart = std::max(nStart, 1);

                GetString(args.GetAt(2), searchString, engine);
                // Strings are indexed from one.
                int nPos = 0;

                for (nPos = nStart - 1; nPos <= string.Size() - searchString.Size(); nPos++)
                {
                    int i = 0;

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

        else if (m_name.Equal("GSI"))   // SI_GetServiceIndex
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

        else if (m_name.Equal("TIn"))   // SI_TuneIndex - Fork not allowed
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
        else if (m_name.Equal("TII"))   // SI_TuneIndexInfo
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
        else if (m_name.Equal("BSI"))   // SI_GetBasicSI
        {
            // Returns basic SI information about the service indicated by an index
            // returned by GSI.
            // Returns networkID, origNetworkID, transportStreamID, serviceID
            if (args.Size() == 5)
            {
                int channelId = GetInt(args.GetAt(0), engine);
                int netId = 0;
                int origNetId = 0;
                int transportId = 0;
                int serviceId = 0;
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
        else if (m_name.Equal("GBI"))   // GetBootInfo
        {
            // Gets the NB_info field.
            MHERROR("GetBootInfo ResidentProgram is not implemented");
        }
        else if (m_name.Equal("CCR"))   // CheckContentRef
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
                fileName.Copy(un.m_contentRefVal);
                QString csPath = engine->GetPathName(fileName.m_contentRef);
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
        else if (m_name.Equal("CGR"))   // CheckGroupIDRef
        {
            // Sees if an application or scene with a particular group id
            // is available in the carousel.
            MHERROR("CheckGroupIDRef ResidentProgram is not implemented");
        }
        else if (m_name.Equal("VTG"))   // VideoToGraphics
        {
            // Video to graphics transformation.
            MHERROR("VideoToGraphics ResidentProgram is not implemented");
        }
        else if (m_name.Equal("SWA"))   // SetWidescreenAlignment
        {
            // Sets either LetterBox or Centre-cut-out mode.
            // Seems to be concerned with aligning a 4:3 scene with an underlying 16:9 video
            MHERROR("SetWidescreenAlignment ResidentProgram is not implemented");
        }
        else if (m_name.Equal("GDA"))   // GetDisplayAspectRatio
        {
            // Returns the aspcet ratio.  4:3 => 1, 16:9 => 2
            MHERROR("GetDisplayAspectRatio ResidentProgram is not implemented");
        }
        else if (m_name.Equal("CIS"))   // CI_SendMessage
        {
            // Sends a message to a DVB CI application
            MHERROR("CI_SendMessage ResidentProgram is not implemented");
        }
        else if (m_name.Equal("SSM"))   // SetSubtitleMode
        {
            // Enable or disable subtitles in addition to MHEG.
            if (args.Size() == 1) {
                bool status = GetBool(args.GetAt(0), engine);
                MHLOG(MHLogNotifications, QString("NOTE SetSubtitleMode %1")
                    .arg(status ? "enabled" : "disabled"));
                // TODO Notify player
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        else if (m_name.Equal("WAI"))   // WhoAmI
        {
            // Return a concatenation of the strings we respond to in
            // GetEngineSupport(UKEngineProfile(X))
            if (args.Size() == 1)
            {
                MHOctetString result;
#if 1
                // BBC Freeview requires a recognized model name which is passed
                // in a ?whoami=... http query to the interaction channel server.
                // Otherwise the menu item for iPlayer is not shown
                result.Copy("SNYPVR");
#else
                result.Copy(engine->MHEGEngineProviderIdString);
                result.Append(" ");
                result.Append(engine->GetContext()->GetReceiverId());
                result.Append(" ");
                result.Append(engine->GetContext()->GetDSMCCId());
#endif
                MHLOG(MHLogNotifications, "NOTE WhoAmI -> " + QString::fromUtf8((const char *)result.Bytes(), result.Size()) );
                engine->FindObject(*(args.GetAt(0)->GetReference()))->SetVariableValue(result);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        // Optional resident programs
        else if (m_name.Equal("DBG"))   // Debug - optional
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
                    case MHUnion::U_Bool:
                        message.append(un.m_fBoolVal ? "True" : "False");
                        break;
                    case MHUnion::U_String:
                        message.append(QString::fromUtf8((const char *)un.m_strVal.Bytes(), un.m_strVal.Size()));
                        break;
                    case MHUnion::U_ObjRef:
                        message.append(un.m_objRefVal.Printable());
                        break;
                    case MHUnion::U_ContentRef:
                        message.append(un.m_contentRefVal.Printable());
                        break;
                    case MHUnion::U_None:
                        break;
                }
            }

            MHLOG(MHLogNotifications, message);
        }

        // NativeApplicationExtension
        else if (m_name.Equal("SBI"))   // SetBroadcastInterrupt
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
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        // InteractionChannelExtension
        else if (m_name.Equal("GIS")) { // GetICStatus
            if (args.Size() == 1)
            {
                int ICstatus = engine->GetContext()->GetICStatus();
                MHLOG(MHLogNotifications, "NOTE InteractionChannel " + QString(
                    ICstatus == 0 ? "active" : ICstatus == 1 ? "inactive" :
                    ICstatus == 2 ? "disabled" : "undefined"));
                engine->FindObject(*(args.GetAt(0)->GetReference()))->SetVariableValue(ICstatus);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_name.Equal("RDa")) { // ReturnData
            if (args.Size() >= 3)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                QUrl url = QString::fromUtf8((const char *)string.Bytes(), string.Size());

                // Variable name/value pairs
                int i = 1;
                QUrlQuery q;
                for (; i + 2 < args.Size(); i += 2)
                {
                    GetString(args.GetAt(i), string, engine);
                    QString name = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                    MHUnion un;
                    un.GetValueFrom(*(args.GetAt(i+1)), engine);
                    q.addQueryItem(name, un.Printable());
                }
                url.setQuery(q);

                MHLOG(MHLogNotifications, QString("NOTE ReturnData %1")
                    .arg(url.toEncoded().constData()) );
                // NB MHEG-5 says this should be 'post' but 'get; seems to work ok
                QByteArray text;
                bool ok = engine->GetContext()->GetCarouselData(url.toEncoded(), text);

                MHLOG(MHLogNotifications, QString("NOTE ReturnData got %1 bytes")
                    .arg(text.size()) );

                // HTTP response code, 0= none
                engine->FindObject(*(args.GetAt(i)->GetReference()))->SetVariableValue(ok ? 200 : 0);
                // HTTP response data
                MHOctetString result;
                result.Append(text.constData());
                engine->FindObject(*(args.GetAt(i+1)->GetReference()))->SetVariableValue(result);

                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_name.Equal("SHF")) { // SetHybridFileSystem
            if (args.Size() == 2)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                QString str = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                GetString(args.GetAt(1), string, engine);
                QString str2 = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                // TODO
                MHLOG(MHLogNotifications, QString("NOTE SetHybridFileSystem %1=%2")
                    .arg(str, str2));
                SetSuccessFlag(success, false, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_name.Equal("PST")) { // PersistentStorageInfo
            if (args.Size() == 1)
            {
                engine->FindObject(*(args.GetAt(0)->GetReference()))->SetVariableValue(true);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_name.Equal("SCk")) { // SetCookie
            if (args.Size() == 4)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                QString id = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                int iExpiry = GetInt(args.GetAt(1), engine);
                GetString(args.GetAt(2), string, engine);
                QString val = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                bool bSecure = GetBool(args.GetAt(3), engine);
                // TODO
                MHLOG(MHLogNotifications, QString("NOTE SetCookie id=%1 MJD=%2 value=%3 secure=%4")
                    .arg(id).arg(iExpiry).arg(val).arg(bSecure) );
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_name.Equal("GCk")) { // GetCookie
            MHERROR("GetCookie ResidentProgram is not implemented");
        }

        // ICStreamingExtension
        else if (m_name.Equal("MSP")) // MeasureStreamPerformance
        {
            if (args.Size() == 2)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                QString url = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                // TODO
                MHLOG(MHLogNotifications, QString("NOTE MeasureStreamPerformance '%1' %2 bytes")
                    .arg(url).arg(GetInt(args.GetAt(1), engine)) );

                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(-1);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_name.Equal("PFG")) { // PromptForGuidance
            if (args.Size() == 2)
            {
                MHOctetString string;
                GetString(args.GetAt(0), string, engine);
                QString info = QString::fromUtf8((const char *)string.Bytes(), string.Size());
                MHLOG(MHLogNotifications, QString("NOTE PromptForGuidance '%1'").arg(info) );

                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(true);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }

        }
        else if (m_name.Equal("GAP") || // GetAudioDescPref
                 m_name.Equal("GSP")) { // GetSubtitlePref
            if (args.Size() == 1)
            {
                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(false);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }
        else if (m_name.Equal("GPS")) { // GetPINSupport
            if (args.Size() == 1)
            {
                // -1= PIN is not supported
                //  0= PIN is supported and disabled
                //  1= PIN is supported and enabled
                engine->FindObject(*(args.GetAt(1)->GetReference()))->SetVariableValue(0);
                SetSuccessFlag(success, true, engine);
            }
            else
            {
                SetSuccessFlag(success, false, engine);
            }
        }

        // Undocumented functions
        else if (m_name.Equal("XBM")) {
            // BBC Freeview passes 1 boolean arg
            // Required for BBC Freeview iPlayer
            MHLOG(MHLogNotifications, "NOTE Undocumented ResidentProgram XBM" );
            if (args.Size() == 1)
                engine->FindObject(*(args.GetAt(0)->GetReference()))->SetVariableValue(true);
            SetSuccessFlag(success, true, engine);
        }

        else
        {
            MHERROR(QString("Unknown ResidentProgram %1").arg(m_name.Printable()));
        }
    }
    catch (...)
    {
        QStringList params;
        for (int i = 0; i < args.Size(); ++i)
        {
            MHUnion un;
            un.GetValueFrom(*(args.GetAt(i)), engine);
            params += QString(MHUnion::GetAsString(un.m_Type)) + "=" + un.Printable();
        }
        MHLOG(MHLogWarning, QString("Arguments (%1)").arg(params.join(",")) );

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
    m_succeeded.Initialise(p->GetArgN(1), engine); // Call/fork succeeded flag
    // Arguments.
    MHParseNode *args = p->GetArgN(2);

    for (int i = 0; i < args->GetSeqCount(); i++)
    {
        auto *pParm = new MHParameter;
        m_parameters.Append(pParm);
        pParm->Initialise(args->GetSeqN(i), engine);
    }
}

void MHCall::PrintArgs(FILE *fd, int nTabs) const
{
    m_succeeded.PrintMe(fd, nTabs);
    fprintf(fd, " ( ");

    for (int i = 0; i < m_parameters.Size(); i++)
    {
        m_parameters.GetAt(i)->PrintMe(fd, 0);
    }

    fprintf(fd, " )");
}

void MHCall::Perform(MHEngine *engine)
{
    // Output parameters are handled by IndirectRefs so we don't evaluate the parameters here.
    Target(engine)->CallProgram(m_fIsFork, m_succeeded, m_parameters, engine);
}
