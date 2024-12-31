/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan         <john@pullan.org>
 *      Taylor Jacob        <rtjacob@earthlink.net>
 *      Daniel Kristjansson <danielk@cuymedia.net>
 *
 * Description:
 *     Collection of classes to provide dvb a transport editor
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

// C++ includes
#include <vector>

// MythTV includes
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"

#include "recorders/satiputils.h"
#include "sourceutil.h"
#include "transporteditor.h"
#include "videosource.h"

class MultiplexID : public AutoIncrementSetting
{
  public:
    MultiplexID() : AutoIncrementSetting("dtv_multiplex", "mplexid")
    {
        setVisible(false);
        setName("MPLEXID");
    }

  public:
    QString GetColumnName(void) const { return m_column; }
};

static QString pp_modulation(const QString& mod)
{
    if (mod.endsWith("vsb"))
        return mod.left(mod.length() - 3) + "-VSB";

    if (mod.startsWith("qam_"))
        return "QAM-" + mod.mid(4, mod.length());

    if (mod == "analog")
        return QObject::tr("Analog");

    return mod.toUpper();
}

static CardUtil::INPUT_TYPES get_cardtype(uint sourceid)
{
    std::vector<uint> cardids;

    // Work out what card we have.. (doesn't always work well)
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT capturecard.cardid "
        "FROM  capturecard "
        "WHERE capturecard.sourceid = :SOURCEID AND "
        "      capturecard.parentid = 0         AND "
        "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("TransportWizard()", query);
        return CardUtil::INPUT_TYPES::ERROR_PROBE;
    }
    while (query.next())
        cardids.push_back(query.value(0).toUInt());

    if (cardids.empty())
    {
        ShowOkPopup(QObject::tr(
            "Sorry, the Transport Editor can only edit transports "
            "of a video source that is connected to a capture card."));

        return CardUtil::INPUT_TYPES::ERROR_PROBE;
    }

    std::vector<CardUtil::INPUT_TYPES> cardtypes;

    for (uint cardid : cardids)
    {
        CardUtil::INPUT_TYPES nType = CardUtil::INPUT_TYPES::ERROR_PROBE;
        QString cardtype = CardUtil::GetRawInputType(cardid);
        if (cardtype == "DVB")
            cardtype = CardUtil::ProbeSubTypeName(cardid);
        nType = CardUtil::toInputType(cardtype);

        if (nType == CardUtil::INPUT_TYPES::HDHOMERUN)
        {
            if (CardUtil::HDHRdoesDVBC(CardUtil::GetVideoDevice(cardid)))
                nType = CardUtil::INPUT_TYPES::DVBC;
            else if (CardUtil::HDHRdoesDVB(CardUtil::GetVideoDevice(cardid)))
                nType = CardUtil::INPUT_TYPES::DVBT2;
        }
#ifdef USING_SATIP
        if (nType == CardUtil::INPUT_TYPES::SATIP)
        {
            QString deviceid = CardUtil::GetVideoDevice(cardid);
            nType = SatIP::toDVBInputType(deviceid);
        }
#endif // USING_SATIP

        if ((CardUtil::INPUT_TYPES::ERROR_OPEN    == nType) ||
            (CardUtil::INPUT_TYPES::ERROR_UNKNOWN == nType) ||
            (CardUtil::INPUT_TYPES::ERROR_PROBE   == nType))
        {
            ShowOkPopup(
                QObject::tr(
                    "Failed to probe a capture card connected to this "
                    "transport's video source. Please make sure the "
                    "backend is not running."));

            return CardUtil::INPUT_TYPES::ERROR_PROBE;
        }

        cardtypes.push_back(nType);
    }

    // This should never happen... (unless DB has changed under us)
    if (cardtypes.empty())
        return CardUtil::INPUT_TYPES::ERROR_PROBE;

    // If there are multiple cards connected to this video source
    // check if they are the same type or compatible.
    for (size_t i = 1; i < cardtypes.size(); i++)
    {
        CardUtil::INPUT_TYPES typeA = cardtypes[i - 1];
        CardUtil::INPUT_TYPES typeB = cardtypes[i + 0];

        // MPEG devices are seen as V4L (historical)
        typeA = (CardUtil::INPUT_TYPES::MPEG == typeA) ? CardUtil::INPUT_TYPES::V4L  : typeA;
        typeB = (CardUtil::INPUT_TYPES::MPEG == typeB) ? CardUtil::INPUT_TYPES::V4L  : typeB;

        // HDHOMERUN devices can be DVBC, DVBT/T2, ATSC or a combination of those.
        // If there are other non-HDHR devices connected to this videosource that
        // have an explicit type then assume that the HDHOMERUN is also of that type.
        typeA = (CardUtil::INPUT_TYPES::HDHOMERUN == typeA) ? typeB : typeA;
        typeB = (CardUtil::INPUT_TYPES::HDHOMERUN == typeB) ? typeA : typeB;

        if (typeA == typeB)
            continue;

        ShowOkPopup(
            QObject::tr(
                "The capture cards connected to this transport's video source "
                "are incompatible. Please create separate video sources "
                "per capture card type."));

        return CardUtil::INPUT_TYPES::ERROR_PROBE;
    }

    // Look for tuner type to decide on transport editor list format
    CardUtil::INPUT_TYPES retval = cardtypes[0];
    for (size_t i = 1; i < cardtypes.size(); i++)
    {
        if ((cardtypes[i] == CardUtil::INPUT_TYPES::DVBS ) ||
            (cardtypes[i] == CardUtil::INPUT_TYPES::DVBC ) ||
            (cardtypes[i] == CardUtil::INPUT_TYPES::DVBT ) ||
            (cardtypes[i] == CardUtil::INPUT_TYPES::ATSC ) ||
            (cardtypes[i] == CardUtil::INPUT_TYPES::DVBS2) ||
            (cardtypes[i] == CardUtil::INPUT_TYPES::DVBT2)  )
        {
            retval = cardtypes[i];
        }
    }

    return retval;
}

void TransportListEditor::SetSourceID(uint sourceid)
{
    for (auto *setting : std::as_const(m_list))
        removeChild(setting);
    m_list.clear();

    if (!sourceid)
    {
        m_sourceid = 0;
    }
    else
    {
        m_cardtype = get_cardtype(sourceid);
        m_sourceid = ((CardUtil::INPUT_TYPES::ERROR_OPEN    == m_cardtype) ||
                      (CardUtil::INPUT_TYPES::ERROR_UNKNOWN == m_cardtype) ||
                      (CardUtil::INPUT_TYPES::ERROR_PROBE   == m_cardtype)) ? 0 : sourceid;
    }
}

TransportListEditor::TransportListEditor(uint sourceid) :
    m_videosource(new VideoSourceShow(sourceid))
{
    setLabel(tr("Transport Editor"));

    addChild(m_videosource);

    auto *newTransport =
        new ButtonStandardSetting("(" + tr("New Transport") + ")");
    connect(newTransport, &ButtonStandardSetting::clicked, this, &TransportListEditor::NewTransport);

    addChild(newTransport);

    connect(m_videosource, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, qOverload<const QString&>(&TransportListEditor::SetSourceID));

    SetSourceID(sourceid);
}

void TransportListEditor::SetSourceID(const QString& name)
{
    if (m_isLoading)
        return;

    uint sourceid = SourceUtil::GetSourceID(name);

    SetSourceID(sourceid);
    Load();
}

void TransportListEditor::Load()
{
    if (m_isLoading)
        return;
    m_isLoading = true;
    if (m_sourceid)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT mplexid, modulation, frequency, "
            "       symbolrate, networkid, transportid, "
            "       constellation, mod_sys "
            "FROM dtv_multiplex, videosource "
            "WHERE dtv_multiplex.sourceid = :SOURCEID AND "
            "      dtv_multiplex.sourceid = videosource.sourceid "
            "ORDER by networkid, transportid, frequency, mplexid");
        query.bindValue(":SOURCEID", m_sourceid);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("TransportList::fillSelections", query);
            m_isLoading = false;
            return;
        }

        while (query.next())
        {
            QString rawmod = ((CardUtil::INPUT_TYPES::OFDM  == m_cardtype) ||
                              (CardUtil::INPUT_TYPES::DVBT2 == m_cardtype)) ?
                query.value(6).toString() : query.value(1).toString();

            QString mod = pp_modulation(rawmod);
            while (mod.length() < 7)
                mod += " ";

            QString rate  = query.value(3).toString();
            rate = (rate == "0") ? "" : QString("rate %1").arg(rate);

            QString netid = query.value(4).toUInt() ?
                QString("netid %1").arg(query.value(4).toUInt(), 5) : "";

            QString tid = query.value(5).toUInt() ?
                QString("tid %1").arg(query.value(5).toUInt(), 5) : "";

            QString hz = "Hz";
            if (CardUtil::INPUT_TYPES::QPSK == m_cardtype ||
                CardUtil::INPUT_TYPES::DVBS2 == m_cardtype)
                hz = "kHz";

            QString type = "";
            if (CardUtil::INPUT_TYPES::OFDM == m_cardtype)
                type = "(DVB-T)";
            if (CardUtil::INPUT_TYPES::DVBT2 == m_cardtype)
                type = QString("(%1)").arg(query.value(7).toString());
            if (CardUtil::INPUT_TYPES::QPSK == m_cardtype)
                type = "(DVB-S)";
            if (CardUtil::INPUT_TYPES::QAM == m_cardtype)
                type = "(DVB-C)";
            if (CardUtil::INPUT_TYPES::DVBS2 == m_cardtype)
                type = "(DVB-S2)";

            QString txt = QString("%1 %2 %3 %4 %5 %6 %7")
                .arg(mod, query.value(2).toString(),
                     hz, rate, netid, tid, type);

            auto *transport = new TransportSetting(txt, query.value(0).toUInt(),
                                                   m_sourceid, m_cardtype);
            connect(transport, &TransportSetting::deletePressed,
                    this, [transport, this] () { Delete(transport); });
            connect(transport, &TransportSetting::openMenu,
                    this, [transport, this] () { Menu(transport); });
            addChild(transport);
            m_list.push_back(transport);
        }
    }

    GroupSetting::Load();
    m_isLoading = false;
}

void TransportListEditor::NewTransport()
{
    auto *transport = new TransportSetting(QString("New Transport"), 0,
                                           m_sourceid, m_cardtype);
    addChild(transport);
    m_list.push_back(transport);
    emit settingsChanged(this);
}


void TransportListEditor::Delete(TransportSetting *transport)
{
    if (m_isLoading)
        return;

    ShowOkPopup(
        tr("Are you sure you would like to delete this transport?"),
        this,
        [transport, this](bool result)
        {
            if (!result)
                return;

            uint mplexid = transport->getMplexId();

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("DELETE FROM dtv_multiplex WHERE mplexid = :MPLEXID");
            query.bindValue(":MPLEXID", mplexid);

            if (!query.exec() || !query.isActive())
                MythDB::DBError("TransportEditor -- delete multiplex", query);

            query.prepare("UPDATE channel SET deleted = NOW() "
                          "WHERE deleted IS NULL AND mplexid = :MPLEXID");
            query.bindValue(":MPLEXID", mplexid);

            if (!query.exec() || !query.isActive())
                MythDB::DBError("TransportEditor -- delete channels", query);

            removeChild(transport);
            m_list.removeAll(transport);
        },
        true);
}

void TransportListEditor::Menu(TransportSetting *transport)
{
    if (m_isLoading)
        return;

    auto *menu = new MythMenu(tr("Transport Menu"), this, "transportmenu");
    menu->AddItem(tr("Delete..."), [transport, this] () { Delete(transport); });

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(menu, popupStack, "menudialog");
    menuPopup->SetReturnEvent(this, "transportmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

class MuxDBStorage : public SimpleDBStorage
{
  protected:
    MuxDBStorage(StorageUser *_setting, const MultiplexID *_id, const QString& _name) :
        SimpleDBStorage(_setting, "dtv_multiplex", _name), m_mplexId(_id)
    {
    }

    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage

    const MultiplexID *m_mplexId;
};

QString MuxDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString muxTag = ":WHERE" + m_mplexId->GetColumnName().toUpper();

    bindings.insert(muxTag, m_mplexId->getValue());

    // return query
    return m_mplexId->GetColumnName() + " = " + muxTag;
}

QString MuxDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString muxTag  = ":SET" + m_mplexId->GetColumnName().toUpper();
    QString nameTag = ":SET" + GetColumnName().toUpper();

    bindings.insert(muxTag,  m_mplexId->getValue());
    bindings.insert(nameTag, m_user->GetDBValue());

    // return query
    return (m_mplexId->GetColumnName() + " = " + muxTag + ", " +
            GetColumnName()   + " = " + nameTag);
}

class VideoSourceID : public StandardSetting, public MuxDBStorage
{
  public:
    VideoSourceID(const MultiplexID *id, uint _sourceid) :
        StandardSetting(this),
        MuxDBStorage(this, id, "sourceid")
    {
        setVisible(false);
        setValue(_sourceid);
    }
    void edit(MythScreenType * /*screen*/) override { } // StandardSetting
    void resultEdit(DialogCompletionEvent * /*dce*/) override { } // StandardSetting
};

class DTVStandard : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    DTVStandard(const MultiplexID *id,
                bool is_dvb_country,
                bool is_atsc_country) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "sistandard")
    {
        setLabel(QObject::tr("Digital TV Standard"));
        setHelpText(QObject::tr(
                        "Guiding standard to use for making sense of the "
                        "data streams after they have been demodulated, "
                        "error corrected and demultiplexed."));
        if (is_dvb_country)
            addSelection(QObject::tr("DVB"),       "dvb");

        if (is_atsc_country)
        {
            addSelection(QObject::tr("ATSC"),      "atsc");
            addSelection(QObject::tr("OpenCable"), "opencable");
        }

        addSelection(QObject::tr("MPEG"),      "mpeg");
    };
};

class Frequency : public MythUITextEditSetting, public MuxDBStorage
{
  public:
    explicit Frequency(const MultiplexID *id, bool in_kHz = false) :
        MythUITextEditSetting(this), MuxDBStorage(this, id, "frequency")
    {
        QString hz = (in_kHz) ? "kHz" : "Hz";
        setLabel(QObject::tr("Frequency") + " (" + hz + ")");
        setHelpText(QObject::tr(
                        "Frequency (Option has no default).\n"
                        "The frequency for this transport (multiplex) in") + " " + hz + ".");
    };
};

class DVBSSymbolRate : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBSSymbolRate(const MultiplexID *id) :
        MythUIComboBoxSetting(this, true), MuxDBStorage(this, id, "symbolrate")
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(
            QObject::tr(
                "Symbol Rate (symbols/sec).\n"
                "Most DVB-S transponders transmit at 27.5 "
                "million symbols per second."));
        addSelection("3333000");
        addSelection("22000000");
        addSelection("22500000");
        addSelection("23000000");
        addSelection("27500000", "27500000", true);
        addSelection("28000000");
        addSelection("28500000");
        addSelection("29500000");
        addSelection("29700000");
        addSelection("29900000");
    };
};

class DVBCSymbolRate : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBCSymbolRate(const MultiplexID *id) :
        MythUIComboBoxSetting(this, true), MuxDBStorage(this, id, "symbolrate")
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(
             QObject::tr(
                "Symbol Rate (symbols/second).\n"
                "Most DVB-C transports transmit at 6.9 or 6.875 "
                "million symbols per second."));
        addSelection("3450000");
        addSelection("5000000");
        addSelection("5900000");
        addSelection("6875000");
        addSelection("6900000", "6900000", true);
        addSelection("6950000");
    };
};

class SignalPolarity : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit SignalPolarity(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "polarity")
    {
        setLabel(QObject::tr("Polarity"));
        setHelpText(QObject::tr("Polarity (Option has no default)"));
        addSelection(QObject::tr("Horizontal"),     "h");
        addSelection(QObject::tr("Vertical"),       "v");
        addSelection(QObject::tr("Right Circular"), "r");
        addSelection(QObject::tr("Left Circular"),  "l");
    };
};

class Modulation : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    Modulation(const MultiplexID *id,  CardUtil::INPUT_TYPES nType);
};

Modulation::Modulation(const MultiplexID *id,  CardUtil::INPUT_TYPES nType) :
    MythUIComboBoxSetting(this),
    MuxDBStorage(this, id, ((CardUtil::INPUT_TYPES::OFDM == nType) ||
                            (CardUtil::INPUT_TYPES::DVBT2 == nType)) ?
                 "constellation" : "modulation")
{
    setLabel(QObject::tr("Modulation"));
    setHelpText(QObject::tr("Modulation, aka Constellation"));

    if (CardUtil::INPUT_TYPES::QPSK == nType)
    {
        // no modulation options
        setVisible(false);
    }
    else if (CardUtil::INPUT_TYPES::DVBS2 == nType)
    {
        addSelection("QPSK",   "qpsk");
        addSelection("8PSK",   "8psk");
        addSelection("16APSK", "16apsk");
        addSelection("32APSK", "32apsk");
    }
    else if ((CardUtil::INPUT_TYPES::QAM == nType) ||
             (CardUtil::INPUT_TYPES::OFDM == nType) ||
             (CardUtil::INPUT_TYPES::DVBT2 == nType))
    {
        addSelection(QObject::tr("QAM Auto"), "auto");
        addSelection("QAM-16",   "qam_16");
        addSelection("QAM-32",   "qam_32");
        addSelection("QAM-64",   "qam_64");
        addSelection("QAM-128",  "qam_128");
        addSelection("QAM-256",  "qam_256");

        if ((CardUtil::INPUT_TYPES::OFDM == nType) ||
            (CardUtil::INPUT_TYPES::DVBT2 == nType))
        {
            addSelection("QPSK", "qpsk");
        }
    }
    else if ((CardUtil::INPUT_TYPES::ATSC      == nType) ||
             (CardUtil::INPUT_TYPES::HDHOMERUN == nType))
    {
        addSelection("8-VSB",    "8vsb");
        addSelection("QAM-64",   "qam_64");
        addSelection("QAM-256",  "qam_256");
    }
    else
    {
        addSelection(QObject::tr("Analog"), "analog");
        setVisible(false);
    }
};

class DVBInversion : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBInversion(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "inversion")
    {
        setLabel(QObject::tr("Inversion"));
        setHelpText(QObject::tr("Inversion (Default: Auto):\n"
                    "Most cards can autodetect this now, so leave it at Auto"
                    " unless it won't work."));
        addSelection(QObject::tr("Auto"), "a");
        addSelection(QObject::tr("On"), "1");
        addSelection(QObject::tr("Off"), "0");
    };
};

class DVBTBandwidth : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBTBandwidth(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "bandwidth")
    {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("Bandwidth (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "a");
        addSelection(QObject::tr("6 MHz"), "6");
        addSelection(QObject::tr("7 MHz"), "7");
        addSelection(QObject::tr("8 MHz"), "8");
    };
};

class DVBT2Bandwidth : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBT2Bandwidth(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "bandwidth")
    {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("Bandwidth for DVB-T2 (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "a");
        addSelection(QObject::tr("5 MHz"), "5");
        addSelection(QObject::tr("6 MHz"), "6");
        addSelection(QObject::tr("7 MHz"), "7");
        addSelection(QObject::tr("8 MHz"), "8");
        // addSelection(QObject::tr("10 MHz"), "10");
        // addSelection(QObject::tr("1.712 MHz"), "1712");
    };
};

class DVBForwardErrorCorrectionSelector : public MythUIComboBoxSetting
{
  public:
    explicit DVBForwardErrorCorrectionSelector(Storage *_storage) :
        MythUIComboBoxSetting(_storage)
    {
        addSelection(QObject::tr("Auto"), "auto");
        addSelection(QObject::tr("None"), "none");
        addSelection("1/2");
        addSelection("2/3");
        addSelection("3/4");
        addSelection("3/5");
        addSelection("4/5");
        addSelection("5/6");
        addSelection("6/7");
        addSelection("7/8");
        addSelection("8/9");
        addSelection("9/10");
    };
};

class DVBForwardErrorCorrection :
    public DVBForwardErrorCorrectionSelector, public MuxDBStorage
{
  public:
    explicit DVBForwardErrorCorrection(const MultiplexID *id) :
        DVBForwardErrorCorrectionSelector(this),
        MuxDBStorage(this, id, "fec")
    {
        setLabel(QObject::tr("FEC"));
        setHelpText(QObject::tr("Forward Error Correction (Default: Auto)"));
    };
};

class DVBTCoderateLP :
    public DVBForwardErrorCorrectionSelector, public MuxDBStorage
{
  public:
    explicit DVBTCoderateLP(const MultiplexID *id) :
        DVBForwardErrorCorrectionSelector(this),
        MuxDBStorage(this, id, "lp_code_rate")
    {
        setLabel(QObject::tr("LP Coderate"));
        setHelpText(QObject::tr("Low Priority Code Rate (Default: Auto)"));
    };
};

class DVBTCoderateHP :
    public DVBForwardErrorCorrectionSelector, public MuxDBStorage
{
  public:
    explicit DVBTCoderateHP(const MultiplexID *id) :
        DVBForwardErrorCorrectionSelector(this),
        MuxDBStorage(this, id, "hp_code_rate")
    {
        setLabel(QObject::tr("HP Coderate"));
        setHelpText(QObject::tr("High Priority Code Rate (Default: Auto)"));
    };
};

class DVBTGuardInterval : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBTGuardInterval(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "guard_interval")
    {
        setLabel(QObject::tr("Guard Interval"));
        setHelpText(QObject::tr("Guard Interval for DVB-T (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "auto");
        addSelection("1/4");
        addSelection("1/8");
        addSelection("1/16");
        addSelection("1/32");
    };
};

class DVBT2GuardInterval : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBT2GuardInterval(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "guard_interval")
    {
        setLabel(QObject::tr("Guard Interval"));
        setHelpText(QObject::tr("Guard Interval for DVB-T2 (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "auto");
        addSelection("1/4");
        addSelection("1/8");
        addSelection("1/16");
        addSelection("1/32");
        addSelection("1/128");
        addSelection("19/128");
        addSelection("19/256");
    };
};

class DVBTTransmissionMode : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBTTransmissionMode(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "transmission_mode")
    {
        setLabel(QObject::tr("Transmission Mode"));
        setHelpText(QObject::tr("Transmission Mode for DVB-T (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "a");
        addSelection("2K", "2");
        addSelection("8K", "8");
    };
};

// The 16k and 32k modes do require a database schema update because
// field dtv_multiplex:transmission_mode is now only one character.
//
class DVBT2TransmissionMode : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBT2TransmissionMode(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "transmission_mode")
    {
        setLabel(QObject::tr("Transmission Mode"));
        setHelpText(QObject::tr("Transmission Mode for DVB-T2 (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "a");
        addSelection("1K", "1");
        addSelection("2K", "2");
        addSelection("4K", "4");
        addSelection("8K", "8");
        // addSelection("16K", "16");
        // addSelection("32K", "32");
    };
};

class DVBTHierarchy : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBTHierarchy(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "hierarchy")
    {
        setLabel(QObject::tr("Hierarchy"));
        setHelpText(QObject::tr("Hierarchy (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "a");
        addSelection(QObject::tr("None"), "n");
        addSelection("1");
        addSelection("2");
        addSelection("4");
    }
};

class DVBTModulationSystem : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBTModulationSystem(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "mod_sys")
    {
        setLabel(QObject::tr("Modulation System"));
        setHelpText(QObject::tr("Modulation System (Default: DVB-T2)"));
        addSelection("DVB-T",  "DVB-T");
        addSelection("DVB-T2", "DVB-T2", true);
    };
};

class DVBSModulationSystem : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBSModulationSystem(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "mod_sys")
    {
        setLabel(QObject::tr("Modulation System"));
        setHelpText(QObject::tr("Modulation System (Default: DVB-S2)"));
        addSelection("DVB-S",  "DVB-S");
        addSelection("DVB-S2", "DVB-S2", true);
    }
};

class DVBCModulationSystem : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBCModulationSystem(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "mod_sys")
    {
        setLabel(QObject::tr("Modulation System"));
        setHelpText(QObject::tr("Modulation System (Default: DVB-C/A)"));
        addSelection("DVB-C/A", "DVB-C/A", true);
        addSelection("DVB-C/B", "DVB-C/B");
        addSelection("DVB-C/C", "DVB-C/C");
    }
};

class RollOff : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit RollOff(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "rolloff")
    {
        setLabel(QObject::tr("Roll-off"));
        setHelpText(QObject::tr("Roll-off factor (Default: 0.35)"));
        addSelection("0.35");
        addSelection("0.20");
        addSelection("0.25");
        addSelection(QObject::tr("Auto"), "auto");
    }
};

TransportSetting::TransportSetting(const QString &label, uint mplexid,
                                   uint sourceid, CardUtil::INPUT_TYPES cardtype)
    : m_mplexid(new MultiplexID())
{
    setLabel(label);

    // Must be first.
    m_mplexid->setValue(mplexid);
    addChild(m_mplexid);
    addChild(new VideoSourceID(m_mplexid, sourceid));

    if (CardUtil::INPUT_TYPES::OFDM == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, true, false));
        addChild(new Frequency(m_mplexid));
        addChild(new DVBTBandwidth(m_mplexid));
        addChild(new DVBInversion(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));

        addChild(new DVBTCoderateLP(m_mplexid));
        addChild(new DVBTCoderateHP(m_mplexid));
        addChild(new DVBTTransmissionMode(m_mplexid));
        addChild(new DVBTGuardInterval(m_mplexid));
        addChild(new DVBTHierarchy(m_mplexid));
    }
    else if (CardUtil::INPUT_TYPES::DVBT2 == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, true, false));
        addChild(new Frequency(m_mplexid));
        addChild(new DVBT2Bandwidth(m_mplexid));
        addChild(new DVBInversion(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));
        addChild(new DVBTModulationSystem(m_mplexid));

        addChild(new DVBTCoderateLP(m_mplexid));
        addChild(new DVBTCoderateHP(m_mplexid));
        addChild(new DVBT2TransmissionMode(m_mplexid));
        addChild(new DVBT2GuardInterval(m_mplexid));
        addChild(new DVBTHierarchy(m_mplexid));
    }
    else if (CardUtil::INPUT_TYPES::QPSK == cardtype ||
             CardUtil::INPUT_TYPES::DVBS2 == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, true, false));
        addChild(new Frequency(m_mplexid, true));
        addChild(new DVBSSymbolRate(m_mplexid));
        addChild(new DVBInversion(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));
        addChild(new DVBSModulationSystem(m_mplexid));
        addChild(new DVBForwardErrorCorrection(m_mplexid));
        addChild(new SignalPolarity(m_mplexid));

        if (CardUtil::INPUT_TYPES::DVBS2 == cardtype)
            addChild(new RollOff(m_mplexid));
    }
    else if (CardUtil::INPUT_TYPES::QAM == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, true, false));
        addChild(new Frequency(m_mplexid));
        addChild(new DVBCSymbolRate(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));
        addChild(new DVBCModulationSystem(m_mplexid));
        addChild(new DVBInversion(m_mplexid));
        addChild(new DVBForwardErrorCorrection(m_mplexid));
    }
    else if (CardUtil::INPUT_TYPES::ATSC      == cardtype ||
             CardUtil::INPUT_TYPES::HDHOMERUN == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, false, true));
        addChild(new Frequency(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));
    }
    else if ((CardUtil::INPUT_TYPES::FIREWIRE == cardtype) ||
             (CardUtil::INPUT_TYPES::FREEBOX  == cardtype))
    {
        addChild(new DTVStandard(m_mplexid, true, true));
    }
    else if ((CardUtil::INPUT_TYPES::V4L  == cardtype) ||
             (CardUtil::INPUT_TYPES::MPEG == cardtype))
    {
        addChild(new Frequency(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));
    }
}

bool TransportSetting::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled =
        GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];

        if (action == "DELETE")
        {
            handled = true;
            emit deletePressed();
        }
        else if (action == "MENU")
        {
            handled = true;
            emit openMenu();
        }
    }

    return handled;
}

uint TransportSetting::getMplexId() const
{
    return m_mplexid->getValue().toUInt();
}
