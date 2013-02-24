/*
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionallity
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// Qt headers
#include <QTextStream>
#include <QFile>

// MythTV headers
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "dtvconfparser.h"
#include "channelutil.h"

#define PARSE_SKIP(VAR) do { \
    if (it == tokens.end()) return false; else ++it; } while(0)

#define PARSE_CONF(VAR) do { \
    if (it == tokens.end() || !VAR.ParseConf(*it++)) \
        return false; } while(0)

#define PARSE_STR(VAR) do { \
    if (it != tokens.end()) VAR = *it++; else return false; } while(0)

#define PARSE_UINT(VAR) do { \
    if (it != tokens.end()) \
         VAR = (*it++).toUInt(); else return false; } while(0)

#define PARSE_UINT_1000(VAR) do { \
    if (it != tokens.end()) \
         VAR = (*it++).toUInt() * 1000ULL; else return false; } while(0)


QString DTVChannelInfo::toString() const
{
    return QString("%1 %2 %3 ").arg(name).arg(serviceid).arg(lcn);
}

DTVConfParser::DTVConfParser(enum cardtype_t _type, uint _sourceid,
                             const QString &_file)
    : type(_type), sourceid(_sourceid), filename(_file)
{
}

DTVConfParser::return_t DTVConfParser::Parse(void)
{
    channels.clear();

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return ERROR_OPEN;

    bool ok = true;
    QTextStream stream(&file);
    QString line;
    while (!stream.atEnd())
    {
        line = stream.readLine(); // line of text excluding '\n'
        line = line.trimmed();
        if (line.startsWith("#"))
            continue;

        QStringList list = line.split(":", QString::SkipEmptyParts);

        if (list.size() < 1)
            continue;

        QString str = list[0];
        int channelNo = -1;

        if ((str.length() >= 1) && (str.at(0) == '@'))
        {
            channelNo = str.mid(1).toInt();
            line = stream.readLine();
            list = line.split(":", QString::SkipEmptyParts);
        }

        if (list.size() < 4)
            continue;

        str = list[3];

        if ((str == "T") || (str == "C") || (str == "S"))
        {
            if ((type == OFDM) && (str == "T"))
                ok &= ParseVDR(list, channelNo);
            else if ((type == QPSK || type == DVBS2) && (str == "S"))
                ok &= ParseVDR(list, channelNo);
            else if ((type == QAM) && (str == "C"))
                ok &= ParseVDR(list, channelNo);
        }
        else if (type == OFDM)
            ok &= ParseConfOFDM(list);
        else if (type == ATSC)
            ok &= ParseConfATSC(list);
        else if (type == QPSK || type == DVBS2)
            ok &= ParseConfQPSK(list);
        else if (type == QAM)
            ok &= ParseConfQAM(list);
    }
    file.close();

    return (ok) ? OK : ERROR_PARSE;
}

bool DTVConfParser::ParseConfOFDM(const QStringList &tokens)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    PARSE_SKIP(unknown);
    PARSE_UINT(mux.frequency);
    PARSE_CONF(mux.inversion);
    PARSE_CONF(mux.bandwidth);
    PARSE_CONF(mux.hp_code_rate);
    PARSE_CONF(mux.lp_code_rate);
    PARSE_CONF(mux.modulation);
    PARSE_CONF(mux.trans_mode);
    PARSE_CONF(mux.guard_interval);
    PARSE_CONF(mux.hierarchy);
    PARSE_SKIP(unknown);
    PARSE_SKIP(unknown);
    PARSE_UINT(chan.serviceid);

    AddChannel(mux, chan);

    return true;
}

bool DTVConfParser::ParseConfATSC(const QStringList &tokens)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    PARSE_STR(chan.name);
    PARSE_UINT(mux.frequency);
    PARSE_CONF(mux.modulation);
    PARSE_SKIP(Ignore_Video_PID);
    PARSE_SKIP(Ignore_Audio_PID);
    PARSE_UINT(chan.serviceid);

    AddChannel(mux, chan);

    return true;
}

bool DTVConfParser::ParseConfQAM(const QStringList &tokens)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    PARSE_SKIP(unknown);
    PARSE_UINT(mux.frequency);
    PARSE_CONF(mux.inversion);
    PARSE_UINT(mux.symbolrate);
    PARSE_CONF(mux.fec);
    PARSE_CONF(mux.modulation);
    PARSE_SKIP(unknown);
    PARSE_SKIP(unknown);
    PARSE_UINT(chan.serviceid);

    AddChannel(mux, chan);

    return true;
}

bool DTVConfParser::ParseConfQPSK(const QStringList &tokens)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    PARSE_STR(chan.name);
    PARSE_UINT_1000(mux.frequency);
    PARSE_CONF(mux.polarity);
    PARSE_SKIP(Satelite_Number);
    PARSE_UINT_1000(mux.symbolrate);
    PARSE_SKIP(unknown);
    PARSE_SKIP(unknown);
    PARSE_UINT(chan.serviceid);

    AddChannel(mux, chan);

    return true;
}

bool DTVConfParser::ParseVDR(const QStringList &tokens, int channelNo)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    chan.lcn = channelNo;

// BBC ONE:754166:I999B8C34D34M16T2G32Y0:T:27500:600:601, 602:0:0:4168:0:0:0

    PARSE_SKIP(unknown);

    PARSE_UINT_1000(mux.frequency);

    if (it == tokens.end())
        return false;

    QString params = (*it++);
    while (!params.isEmpty())
    {
        QString ori = params;
        int s = (int) (params.toLatin1().constData()[0]);
        params = params.mid(1);
        switch (s)
        {
            case 'I':
                mux.inversion.ParseVDR(params);
                break;
            case 'B':
                mux.bandwidth.ParseVDR(params);
                break;
            case 'C':
                mux.hp_code_rate.ParseVDR(params);
                break;
            case 'D':
                mux.lp_code_rate.ParseVDR(params);
                break;
            case 'M':
                mux.modulation.ParseVDR(params);
                break;
            case 'T':
                mux.trans_mode.ParseVDR(params);
                break;
            case 'G':
                mux.guard_interval.ParseVDR(params);
                break;
            case 'Y':
                mux.hierarchy.ParseVDR(params);
                break;
            case 'V':
            case 'H':
            case 'R':
            case 'L':
                mux.polarity.ParseVDR(ori);
                break;
            case 'S':
                mux.mod_sys.ParseVDR(params);
                break;
            case 'O':
                mux.rolloff.ParseVDR(params);
                break;
            default:
                return false;
        }
    }

    for (uint i = 0; i < 6; i++)
        PARSE_SKIP(unknown);

    PARSE_UINT(chan.serviceid);

    AddChannel(mux, chan);

    return true;
}

void DTVConfParser::AddChannel(const DTVMultiplex &mux, DTVChannelInfo &chan)
{
    for (uint i = 0; i < channels.size(); i++)
    {
        if (channels[i] == mux)
        {
            channels[i].channels.push_back(chan);

            LOG(VB_GENERAL, LOG_INFO, "Imported channel: " + chan.toString() +
                    " on " + mux.toString());
            return;
        }
    }

    channels.push_back(mux);
    channels.back().channels.push_back(chan);

    LOG(VB_GENERAL, LOG_INFO, "Imported channel: " + chan.toString() +
            " on " + mux.toString());
}
