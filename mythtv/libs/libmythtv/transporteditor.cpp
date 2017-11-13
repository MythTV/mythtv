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

#include <vector>
using namespace std;

#include "transporteditor.h"
#include "videosource.h"
#include "cardutil.h"
#include "mythcorecontext.h"
#include "mythdb.h"

#define LOC QString("DTVMux: ")

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

static QString pp_modulation(QString mod)
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
    vector<uint> cardids;

    // Work out what card we have.. (doesn't always work well)
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT capturecard.cardid "
        "FROM  capturecard "
        "WHERE capturecard.sourceid = :SOURCEID AND "
        "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("TransportWizard()", query);
        return CardUtil::ERROR_PROBE;
    }
    else
    {
        while (query.next())
            cardids.push_back(query.value(0).toUInt());
    }

    if (cardids.empty())
    {
        ShowOkPopup(QObject::tr(
            "Sorry, the Transport Editor can only be used to "
            "edit transports which are connected to a card input."));

        return CardUtil::ERROR_PROBE;
    }

    vector<CardUtil::INPUT_TYPES> cardtypes;

    vector<uint>::const_iterator it = cardids.begin();
    for (; it != cardids.end(); ++it)
    {
        CardUtil::INPUT_TYPES nType = CardUtil::ERROR_PROBE;
        QString cardtype = CardUtil::GetRawInputType(*it);
        if (cardtype == "DVB")
            cardtype = CardUtil::ProbeSubTypeName(*it);
        nType = CardUtil::toInputType(cardtype);

        if ((CardUtil::ERROR_OPEN    == nType) ||
            (CardUtil::ERROR_UNKNOWN == nType) ||
            (CardUtil::ERROR_PROBE   == nType))
        {
            ShowOkPopup(
                QObject::tr(
                    "Failed to probe a capture card connected to this "
                    "transport's video source. Please make sure the "
                    "backend is not running."));

            return CardUtil::ERROR_PROBE;
        }

        cardtypes.push_back(nType);
    }

    // This should never happen... (unless DB has changed under us)
    if (cardtypes.empty())
        return CardUtil::ERROR_PROBE;

    for (uint i = 1; i < cardtypes.size(); i++)
    {
        CardUtil::INPUT_TYPES typeA = cardtypes[i - 1];
        typeA = (CardUtil::HDHOMERUN == typeA) ? CardUtil::ATSC : typeA;
        typeA = (CardUtil::MPEG      == typeA) ? CardUtil::V4L  : typeA;

        CardUtil::INPUT_TYPES typeB = cardtypes[i + 0];
        typeB = (CardUtil::HDHOMERUN == typeB) ? CardUtil::ATSC : typeB;
        typeB = (CardUtil::MPEG      == typeB) ? CardUtil::V4L  : typeB;

        if (typeA == typeB)
            continue;

        ShowOkPopup(
            QObject::tr(
                "The Video Sources to which this Transport is connected "
                "are incompatible, please create separate video sources "
                "for these cards. "));

        return CardUtil::ERROR_PROBE;
    }

    return cardtypes[0];
}

void TransportListEditor::SetSourceID(uint _sourceid)
{
    for (auto setting : m_list)
        removeChild(setting);
    m_list.clear();

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("TransportList::SetSourceID(%1)")
                                   .arg(_sourceid));
#endif

    if (!_sourceid)
    {
        m_sourceid = 0;
    }
    else
    {
        m_cardtype = get_cardtype(_sourceid);
        m_sourceid = ((CardUtil::ERROR_OPEN    == m_cardtype) ||
                      (CardUtil::ERROR_UNKNOWN == m_cardtype) ||
                      (CardUtil::ERROR_PROBE   == m_cardtype)) ? 0 : _sourceid;
    }
}

TransportListEditor::TransportListEditor(uint sourceid) :
    m_videosource(new VideoSourceSelector(sourceid, QString::null, false)), isLoading(false)
{
    setLabel(tr("Multiplex Editor"));

    addChild(m_videosource);
    ButtonStandardSetting *newTransport =
        new ButtonStandardSetting("(" + tr("New Transport") + ")");
    connect(newTransport, SIGNAL(clicked()), SLOT(NewTransport(void)));

    addChild(newTransport);

    connect(m_videosource, SIGNAL(valueChanged(const QString&)),
            this, SLOT(SetSourceID(const QString&)));

    SetSourceID(sourceid);
}

void TransportListEditor::SetSourceID(const QString& sourceid)
{
    if (isLoading)
        return;
    SetSourceID(sourceid.toUInt());
    Load();
}

void TransportListEditor::Load()
{
    if (isLoading)
        return;
    isLoading = true;
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
            isLoading = false;
            return;
        }

        while (query.next())
        {
            QString rawmod = ((CardUtil::OFDM == m_cardtype) ||
                            (CardUtil::DVBT2 == m_cardtype)) ?
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

            QString hz = (CardUtil::QPSK == m_cardtype) ? "kHz" : "Hz";

            QString type = "";
            if (CardUtil::OFDM == m_cardtype)
                type = "(DVB-T)";
            if (CardUtil::DVBT2 == m_cardtype)
                type = QString("(%1)").arg(query.value(7).toString());
            if (CardUtil::QPSK == m_cardtype)
                type = "(DVB-S)";
            if (CardUtil::QAM == m_cardtype)
                type = "(DVB-C)";

            QString txt = QString("%1 %2 %3 %4 %5 %6 %7")
                .arg(mod).arg(query.value(2).toString())
                .arg(hz).arg(rate).arg(netid).arg(tid).arg(type);

            TransportSetting *transport =
                new TransportSetting(txt, query.value(0).toUInt(), m_sourceid,
                                    m_cardtype);
            connect(transport, &TransportSetting::deletePressed,
                    this, [transport, this] () { Delete(transport); });
            connect(transport, &TransportSetting::openMenu,
                    this, [transport, this] () { Menu(transport); });
            addChild(transport);
            m_list.push_back(transport);
        }
    }

    GroupSetting::Load();
    isLoading = false;
}

void TransportListEditor::NewTransport()
{
    TransportSetting *transport =
        new TransportSetting(QString("New Transport"), 0,
           m_sourceid, m_cardtype);
    addChild(transport);
    m_list.push_back(transport);
    emit settingsChanged(this);
}


void TransportListEditor::Delete(TransportSetting *transport)
{
    if (isLoading)
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

            query.prepare("DELETE FROM channel WHERE mplexid = :MPLEXID");
            query.bindValue(":MPLEXID", mplexid);

            if (!query.exec() || !query.isActive())
                MythDB::DBError("TransportEditor -- delete channels", query);

            removeChild(transport);
            // m_list.removeAll(transport);
            // Following for QT 5.3 which does not have the removeAll
            // method in QVector
            int ix;
            do
            {
                ix = m_list.indexOf(transport);
                if (ix != -1)
                    m_list.remove(ix);
            } while (ix != -1);
        },
        true);
}

void TransportListEditor::Menu(TransportSetting *transport)
{
    if (isLoading)
        return;

    MythMenu *menu = new MythMenu(tr("Transport Menu"), this, "transportmenu");
    menu->AddItem(tr("Delete..."), [transport, this] () { Delete(transport); });

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack,
                                                 "menudialog");
    menuPopup->SetReturnEvent(this, "transportmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

class MuxDBStorage : public SimpleDBStorage
{
  protected:
    MuxDBStorage(StorageUser *_setting, const MultiplexID *_id, QString _name) :
        SimpleDBStorage(_setting, "dtv_multiplex", _name), mplexid(_id)
    {
    }

    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    const MultiplexID *mplexid;
};

QString MuxDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString muxTag = ":WHERE" + mplexid->GetColumnName().toUpper();

    bindings.insert(muxTag, mplexid->getValue());

    // return query
    return mplexid->GetColumnName() + " = " + muxTag;
}

QString MuxDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString muxTag  = ":SET" + mplexid->GetColumnName().toUpper();
    QString nameTag = ":SET" + GetColumnName().toUpper();

    bindings.insert(muxTag,  mplexid->getValue());
    bindings.insert(nameTag, user->GetDBValue());

    // return query
    return (mplexid->GetColumnName() + " = " + muxTag + ", " +
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
    virtual void edit(MythScreenType * /*screen*/) { }
    virtual void resultEdit(DialogCompletionEvent * /*dce*/) { }
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
    Frequency(const MultiplexID *id, bool in_kHz = false) :
        MythUITextEditSetting(this), MuxDBStorage(this, id, "frequency")
    {
        QString hz = (in_kHz) ? "kHz" : "Hz";
        setLabel(QObject::tr("Frequency") + " (" + hz + ")");
        setHelpText(QObject::tr(
                        "Frequency (Option has no default).\n"
                        "The frequency for this channel in") + " " + hz + ".");
    };
};

class DVBSymbolRate : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBSymbolRate(const MultiplexID *id) :
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
        addSelection("23000000");
        addSelection("27500000", "27500000", true);
        addSelection("28000000");
        addSelection("28500000");
        addSelection("29500000");
        addSelection("29900000");
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
    Modulation(const MultiplexID *id,  uint nType);
};

Modulation::Modulation(const MultiplexID *id,  uint nType) :
    MythUIComboBoxSetting(this),
    MuxDBStorage(this, id, ((CardUtil::OFDM == nType) ||
                            (CardUtil::DVBT2 == nType)) ?
                 "constellation" : "modulation")
{
    setLabel(QObject::tr("Modulation"));
    setHelpText(QObject::tr("Modulation, aka Constellation"));

    if (CardUtil::QPSK == nType)
    {
        // no modulation options
        setVisible(false);
    }
    else if (CardUtil::DVBS2 == nType)
    {
        addSelection("QPSK",   "qpsk");
        addSelection("8PSK",   "8psk");
        addSelection("16APSK", "16apsk");
        addSelection("32APSK", "32apsk");
    }
    else if ((CardUtil::QAM == nType) || (CardUtil::OFDM == nType) ||
             (CardUtil::DVBT2 == nType))
    {
        addSelection(QObject::tr("QAM Auto"), "auto");
        addSelection("QAM-16",   "qam_16");
        addSelection("QAM-32",   "qam_32");
        addSelection("QAM-64",   "qam_64");
        addSelection("QAM-128",  "qam_128");
        addSelection("QAM-256",  "qam_256");

        if ((CardUtil::OFDM == nType) || (CardUtil::DVBT2 == nType))
        {
            addSelection("QPSK", "qpsk");
        }
    }
    else if ((CardUtil::ATSC      == nType) ||
             (CardUtil::HDHOMERUN == nType))
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
        addSelection("4/5");
        addSelection("5/6");
        addSelection("6/7");
        addSelection("7/8");
        addSelection("8/9");
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
        setHelpText(QObject::tr("Guard Interval (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "auto");
        addSelection("1/4");
        addSelection("1/8");
        addSelection("1/16");
        addSelection("1/32");
    };
};

class DVBTTransmissionMode : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBTTransmissionMode(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "transmission_mode")
    {
        setLabel(QObject::tr("Trans. Mode"));
        setHelpText(QObject::tr("Transmission Mode (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "a");
        addSelection("2K", "2");
        addSelection("8K", "8");
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
        setHelpText(QObject::tr("Modulation System (Default: DVB-T)"));
        addSelection(QObject::tr("DVB-T"),  "DVB-T");
        addSelection(QObject::tr("DVB-T2"), "DVB-T2");
    };
};

class DVBSModulationSystem : public MythUIComboBoxSetting, public MuxDBStorage
{
  public:
    explicit DVBSModulationSystem(const MultiplexID *id) :
        MythUIComboBoxSetting(this), MuxDBStorage(this, id, "mod_sys")
    {
        setLabel(QObject::tr("Modulation System"));
        setHelpText(QObject::tr("Modulation System (Default: DVB-S)"));
        addSelection(QObject::tr("DVB-S"),  "DVB-S");
        addSelection(QObject::tr("DVB-S2"), "DVB-S2");
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
                                   uint sourceid, uint cardtype)
    : m_mplexid(new MultiplexID())
{
    setLabel(label);

    // Must be first.
    m_mplexid->setValue(mplexid);
    addChild(m_mplexid);
    addChild(new VideoSourceID(m_mplexid, sourceid));

    if (CardUtil::OFDM == cardtype)
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
    else if (CardUtil::DVBT2 == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, true, false));
        addChild(new Frequency(m_mplexid));
        addChild(new DVBTBandwidth(m_mplexid));
        addChild(new DVBInversion(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));
        addChild(new DVBTModulationSystem(m_mplexid));

        addChild(new DVBTCoderateLP(m_mplexid));
        addChild(new DVBTCoderateHP(m_mplexid));
        addChild(new DVBTTransmissionMode(m_mplexid));
        addChild(new DVBTGuardInterval(m_mplexid));
        addChild(new DVBTHierarchy(m_mplexid));
    }
    else if (CardUtil::QPSK == cardtype ||
             CardUtil::DVBS2 == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, true, false));
        addChild(new Frequency(m_mplexid, true));
        addChild(new DVBSymbolRate(m_mplexid));

        addChild(new DVBInversion(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));
        addChild(new DVBSModulationSystem(m_mplexid));
        addChild(new DVBForwardErrorCorrection(m_mplexid));
        addChild(new SignalPolarity(m_mplexid));

        if (CardUtil::DVBS2 == cardtype)
            addChild(new RollOff(m_mplexid));
    }
    else if (CardUtil::QAM == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, true, false));
        addChild(new Frequency(m_mplexid));
        addChild(new DVBSymbolRate(m_mplexid));

        addChild(new Modulation(m_mplexid, cardtype));
        addChild(new DVBInversion(m_mplexid));
        addChild(new DVBForwardErrorCorrection(m_mplexid));
    }
    else if (CardUtil::ATSC      == cardtype ||
             CardUtil::HDHOMERUN == cardtype)
    {
        addChild(new DTVStandard(m_mplexid, false, true));
        addChild(new Frequency(m_mplexid));
        addChild(new Modulation(m_mplexid, cardtype));
    }
    else if ((CardUtil::FIREWIRE == cardtype) ||
             (CardUtil::FREEBOX  == cardtype))
    {
        addChild(new DTVStandard(m_mplexid, true, true));
    }
    else if ((CardUtil::V4L  == cardtype) ||
             (CardUtil::MPEG == cardtype))
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
        QString action = actions[i];

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
