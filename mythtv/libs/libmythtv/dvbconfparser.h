/*
 * $Id$
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

#ifndef DVBCONFPARSER_H
#define DVBCONFPARSER_H

#include <qobject.h>
#include <qvaluevector.h>
#include <qstring.h>
#include <dvbtypes.h>

/**
 * class DVBConfParser
 * @brief parses channels.conf files into the mythtv structure
 */
class DVBConfParser : public QObject
{
    Q_OBJECT
protected:
    class Multiplex
    {
    public:
        Multiplex() : frequency(0),symbolrate(0),mplexid(0) {}
        bool operator==(const Multiplex& m) const;

        unsigned frequency;
        unsigned symbolrate;
        DVBInversion inversion;
        DVBBandwidth bandwidth;
        DVBCodeRate coderate_hp;
        DVBCodeRate coderate_lp;
        DVBModulation constellation;
        DVBModulation modulation;
        DVBTransmitMode transmit_mode;
        DVBGuardInterval guard_interval;
        DVBHierarchy  hierarchy;
        DVBPolarity  polarity;
        DVBCodeRate fec;
        unsigned mplexid;

        void dump();
    };

    class Channel : public Multiplex
    {
    public:
        Channel() : serviceid(0),mplexnumber(0), lcn(-1) {}

        QString name;
        unsigned serviceid;
        unsigned mplexnumber;
        int lcn;

        void dump();
    };

    typedef QValueList<Channel> ListChannels;

    ListChannels  channels;
    QValueVector<Multiplex> multiplexes;

public:
    enum RETURN {ERROR_OPEN,ERROR_PARSE,OK};
    enum TYPE {ATSC,OFDM,QPSK,QAM};

    DVBConfParser(TYPE _type,unsigned sourceid, const QString& _file);
    virtual ~DVBConfParser() {};
    int parse(); 

signals:
    /** @brief Status message from the scan engine
        @param status the message
    */ 
    void updateText(const QString& status);
protected:
    QString filename;
    TYPE type;
    unsigned sourceid;
    bool parseVDR(QStringList& tokens, int channelNo = -1);
    bool parseConf(QStringList& tokens);
    bool parseConfOFDM(QStringList& tokens);
    bool parseConfQPSK(QStringList& tokens);
    bool parseConfQAM(QStringList& tokens);
    bool parseConfATSC(QStringList& tokens);
    void processChannels();
    int findMultiplex(const Multiplex& m);
    int findChannel(const Channel& c);
    int generateNewChanID(int sourceID);
};

#endif
