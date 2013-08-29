/* -*- Mode: c++ -*-
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

#ifndef _DTVCONFPARSER_H_
#define _DTVCONFPARSER_H_

// POSIX headers
#include <stdint.h>
#include <unistd.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>

// MythTV headers
#include "dtvmultiplex.h"

class QStringList;

class DTVChannelInfo
{
  public:
    DTVChannelInfo() :
        name(QString::null), serviceid(0), lcn(-1) {}

    QString toString() const;

 public:
    QString name;
    uint    serviceid;
    int     lcn;
};
typedef vector<DTVChannelInfo> DTVChannelInfoList;

class DTVTransport : public DTVMultiplex
{
  public:
    DTVTransport(const DTVMultiplex &other) : DTVMultiplex(other) { }

  public:
    DTVChannelInfoList channels;
};
typedef vector<DTVTransport> DTVChannelList;

/** \class DTVConfParser
 *  \brief Parses dvb-utils channel scanner output files.
 */
class DTVConfParser
{
  public:
    enum return_t   { ERROR_CARDTYPE, ERROR_OPEN, ERROR_PARSE, OK };
    enum cardtype_t { ATSC, OFDM, QPSK, QAM, DVBS2, UNKNOWN };

    DTVConfParser(enum cardtype_t _type, uint sourceid, const QString &_file);
    virtual ~DTVConfParser() { }

    return_t Parse(void);

    DTVChannelList GetChannels(void) const { return channels; }

  private:
    bool ParseVDR(     const QStringList &tokens, int channelNo = -1);
    bool ParseConf(    const QStringList &tokens);
    bool ParseConfOFDM(const QStringList &tokens);
    bool ParseConfQPSK(const QStringList &tokens);
    bool ParseConfQAM( const QStringList &tokens);
    bool ParseConfATSC(const QStringList &tokens);

  private:
    cardtype_t type;
    uint       sourceid;
    QString    filename;

    void AddChannel(const DTVMultiplex &mux, DTVChannelInfo &chan);

    DTVChannelList channels;
};

#endif // _DTVCONFPARSER_H_
