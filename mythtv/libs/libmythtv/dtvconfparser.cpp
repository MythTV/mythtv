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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// Qt headers
#include <QTextStream>
#include <QFile>

// MythTV headers
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "dtvconfparser.h"
#include "channelutil.h"

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define PARSE_SKIP(VAR) do { \
    if (it == tokens.end()) return false; \
    ++it; } while(false)

#define PARSE_CONF(VAR) do { \
    if (it == tokens.end()) return false; \
    if (!(VAR).ParseConf(*it)) return false; \
    it++; } while(false)

#define PARSE_STR(VAR) do { \
    if (it != tokens.end()) (VAR) = *it++; else return false; } while(false)

#define PARSE_UINT(VAR) do { \
    if (it != tokens.end()) \
         (VAR) = (*it++).toUInt(); else return false; } while(false)

#define PARSE_UINT_1000(VAR) do { \
    if (it != tokens.end()) \
         (VAR) = (*it++).toUInt() * 1000ULL; else return false; } while(false)
// NOLINTEND(cppcoreguidelines-macro-usage)


QString DTVChannelInfo::toString() const
{
    return QString("%1 %2 %3 ").arg(m_name).arg(m_serviceid).arg(m_lcn);
}

DTVConfParser::return_t DTVConfParser::Parse(void)
{
    m_channels.clear();

    QFile file(m_filename);
    if (!file.open(QIODevice::ReadOnly))
        return return_t::ERROR_OPEN;

    bool ok = true;
    QTextStream stream(&file);
    QString line;
    while (!stream.atEnd())
    {
        line = stream.readLine(); // line of text excluding '\n'
        line = line.trimmed();
        if (line.startsWith("#"))
            continue;

        QStringList list = line.split(":", Qt::SkipEmptyParts);
        if (list.empty())
            continue;

        QString str = list[0];
        int channelNo = -1;

        if ((str.length() >= 1) && (str.at(0) == '@'))
        {
            channelNo = str.mid(1).toInt();
            line = stream.readLine();
            list = line.split(":", Qt::SkipEmptyParts);
        }

        if (list.size() < 4)
            continue;

        str = list[3];

        if ((str == "T") || (str == "C") || (str == "S"))
        {
            if (((m_type == cardtype_t::OFDM) && (str == "T")) ||
                ((m_type == cardtype_t::QPSK || m_type == cardtype_t::DVBS2) && (str == "S")) ||
                ((m_type == cardtype_t::QAM) && (str == "C")))
                ok &= ParseVDR(list, channelNo);
        }
        else if (m_type == cardtype_t::OFDM)
        {
            ok &= ParseConfOFDM(list);
        }
        else if (m_type == cardtype_t::ATSC)
        {
            ok &= ParseConfATSC(list);
        }
        else if (m_type == cardtype_t::QPSK || m_type == cardtype_t::DVBS2)
        {
            ok &= ParseConfQPSK(list);
        }
        else if (m_type == cardtype_t::QAM)
        {
            ok &= ParseConfQAM(list);
        }
    }
    file.close();

    return (ok) ? return_t::OK : return_t::ERROR_PARSE;
}

bool DTVConfParser::ParseConfOFDM(const QStringList &tokens)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    PARSE_SKIP(unknown);
    PARSE_UINT(mux.m_frequency);
    PARSE_CONF(mux.m_inversion);
    PARSE_CONF(mux.m_bandwidth);
    PARSE_CONF(mux.m_hpCodeRate);
    PARSE_CONF(mux.m_lpCodeRate);
    PARSE_CONF(mux.m_modulation);
    PARSE_CONF(mux.m_transMode);
    PARSE_CONF(mux.m_guardInterval);
    PARSE_CONF(mux.m_hierarchy);
    PARSE_SKIP(unknown);
    PARSE_SKIP(unknown);
    PARSE_UINT(chan.m_serviceid);

    AddChannel(mux, chan);

    return true;
}

bool DTVConfParser::ParseConfATSC(const QStringList &tokens)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    PARSE_STR(chan.m_name);
    PARSE_UINT(mux.m_frequency);
    PARSE_CONF(mux.m_modulation);
    PARSE_SKIP(Ignore_Video_PID);
    PARSE_SKIP(Ignore_Audio_PID);
    PARSE_UINT(chan.m_serviceid);

    AddChannel(mux, chan);

    return true;
}

bool DTVConfParser::ParseConfQAM(const QStringList &tokens)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    PARSE_SKIP(unknown);
    PARSE_UINT(mux.m_frequency);
    PARSE_CONF(mux.m_inversion);
    PARSE_UINT(mux.m_symbolRate);
    PARSE_CONF(mux.m_fec);
    PARSE_CONF(mux.m_modulation);
    PARSE_SKIP(unknown);
    PARSE_SKIP(unknown);
    PARSE_UINT(chan.m_serviceid);

    AddChannel(mux, chan);

    return true;
}

bool DTVConfParser::ParseConfQPSK(const QStringList &tokens)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    PARSE_STR(chan.m_name);
    PARSE_UINT_1000(mux.m_frequency);
    PARSE_CONF(mux.m_polarity);
    PARSE_SKIP(Satelite_Number);
    PARSE_UINT_1000(mux.m_symbolRate);
    PARSE_SKIP(unknown);
    PARSE_SKIP(unknown);
    PARSE_UINT(chan.m_serviceid);

    AddChannel(mux, chan);

    return true;
}

bool DTVConfParser::ParseVDR(const QStringList &tokens, int channelNo)
{
    DTVChannelInfo chan;
    DTVMultiplex   mux;

    QStringList::const_iterator it = tokens.begin();

    chan.m_lcn = channelNo;

// BBC ONE:754166:I999B8C34D34M16T2G32Y0:T:27500:600:601, 602:0:0:4168:0:0:0

    PARSE_SKIP(unknown);

    PARSE_UINT_1000(mux.m_frequency);

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
                mux.m_inversion.ParseVDR(params);
                break;
            case 'B':
                mux.m_bandwidth.ParseVDR(params);
                break;
            case 'C':
                mux.m_hpCodeRate.ParseVDR(params);
                break;
            case 'D':
                mux.m_lpCodeRate.ParseVDR(params);
                break;
            case 'M':
                mux.m_modulation.ParseVDR(params);
                break;
            case 'T':
                mux.m_transMode.ParseVDR(params);
                break;
            case 'G':
                mux.m_guardInterval.ParseVDR(params);
                break;
            case 'Y':
                mux.m_hierarchy.ParseVDR(params);
                break;
            case 'V':
            case 'H':
            case 'R':
            case 'L':
                mux.m_polarity.ParseVDR(ori);
                break;
            case 'S':
                mux.m_modSys.ParseVDR(params);
                break;
            case 'O':
                mux.m_rolloff.ParseVDR(params);
                break;
            default:
                return false;
        }
    }

    for (uint i = 0; i < 6; i++)
        PARSE_SKIP(unknown);

    PARSE_UINT(chan.m_serviceid);

    AddChannel(mux, chan);

    return true;
}

void DTVConfParser::AddChannel(const DTVMultiplex &mux, DTVChannelInfo &chan)
{
    for (auto & channel : m_channels)
    {
        if (channel == mux)
        {
            channel.channels.push_back(chan);

            LOG(VB_GENERAL, LOG_INFO, "Imported channel: " + chan.toString() +
                    " on " + mux.toString());
            return;
        }
    }

    m_channels.emplace_back(mux);
    m_channels.back().channels.push_back(chan);

    LOG(VB_GENERAL, LOG_INFO, "Imported channel: " + chan.toString() +
            " on " + mux.toString());
}
