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

class MultiplexID : public AutoIncrementDBSetting
{
  public:
    MultiplexID() : AutoIncrementDBSetting("dtv_multiplex", "mplexid")
    {
        setVisible(false);
        setName("MPLEXID");
    }

  public:
    QString GetColumnName(void) const { return DBStorage::GetColumnName(); }
};

class TransportWizard : public ConfigurationWizard
{
  public:
    TransportWizard(
        uint mplexid, uint sourceid, CardUtil::CARD_TYPES _cardtype);

  private:
    MultiplexID *mplexid;
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

void TransportList::fillSelections(void)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("TransportList::fillSelections() %1")
                                   .arg(sourceid));
#endif

    clearSelections();
    addSelection("(" + tr("New Transport") + ")", "0");

    setHelpText(QObject::tr(
                    "This section lists each transport that MythTV "
                    "currently knows about. The display fields are "
                    "video source, modulation, frequency, and when "
                    "relevant symbol rate, network id, and transport id."));

    if (!sourceid)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT mplexid, modulation, frequency, "
        "       symbolrate, networkid, transportid, constellation "
        "FROM dtv_multiplex, videosource "
        "WHERE dtv_multiplex.sourceid = :SOURCEID AND "
        "      dtv_multiplex.sourceid = videosource.sourceid "
        "ORDER by networkid, transportid, frequency, mplexid");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("TransportList::fillSelections", query);
        return;
    }

    while (query.next())
    {
        QString rawmod = (CardUtil::OFDM == cardtype) ?
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

        QString hz = (CardUtil::QPSK == cardtype) ? "kHz" : "Hz";

        QString type = "";
        if (CardUtil::OFDM == cardtype)
            type = "(DVB-T)";
        if (CardUtil::QPSK == cardtype)
            type = "(DVB-S)";
        if (CardUtil::QAM == cardtype)
            type = "(DVB-C)";

        QString txt = QString("%1 %2 %3 %4 %5 %6 %7")
            .arg(mod).arg(query.value(2).toString())
            .arg(hz).arg(rate).arg(netid).arg(tid).arg(type);

        addSelection(txt, query.value(0).toString());
    }
}

static CardUtil::CARD_TYPES get_cardtype(uint sourceid)
{
    vector<uint> cardids;

    // Work out what card we have.. (doesn't always work well)
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT capturecard.cardid "
        "FROM cardinput, capturecard "
        "WHERE capturecard.cardid = cardinput.cardid AND "
        "      cardinput.sourceid = :SOURCEID AND "
        "    capturecard.hostname = :HOSTNAME");
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
        MythPopupBox::showOkPopup(
            GetMythMainWindow(), 
            QObject::tr("Transport Editor"), 
            QObject::tr(
                "Sorry, the Transport Editor can only be used to "
                "edit transports which are connected to a card input."));

        return CardUtil::ERROR_PROBE;
    }

    vector<CardUtil::CARD_TYPES> cardtypes;

    vector<uint>::const_iterator it = cardids.begin();
    for (; it != cardids.end(); ++it)
    {
        CardUtil::CARD_TYPES nType = CardUtil::ERROR_PROBE;
        QString cardtype = CardUtil::GetRawCardType(*it);
        if (cardtype == "DVB")
            cardtype = CardUtil::ProbeSubTypeName(*it);
        nType = CardUtil::toCardType(cardtype);

        if ((CardUtil::ERROR_OPEN    == nType) ||
            (CardUtil::ERROR_UNKNOWN == nType) ||
            (CardUtil::ERROR_PROBE   == nType))
        {
            MythPopupBox::showOkPopup(
                GetMythMainWindow(), 
                QObject::tr("Transport Editor"), 
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
        CardUtil::CARD_TYPES typeA = cardtypes[i - 1];
        typeA = (CardUtil::HDHOMERUN == typeA) ? CardUtil::ATSC : typeA;
        typeA = (CardUtil::MPEG      == typeA) ? CardUtil::V4L  : typeA;

        CardUtil::CARD_TYPES typeB = cardtypes[i + 0];
        typeB = (CardUtil::HDHOMERUN == typeB) ? CardUtil::ATSC : typeB;
        typeB = (CardUtil::MPEG      == typeB) ? CardUtil::V4L  : typeB;

        if (typeA == typeB)
            continue;

        MythPopupBox::showOkPopup(
            GetMythMainWindow(), 
            QObject::tr("Transport Editor"), 
            QObject::tr(
                "The Video Sources to which this Transport is connected "
                "are incompatible, please create seperate video sources "
                "for these cards. "));

        return CardUtil::ERROR_PROBE;
    }

    return cardtypes[0];
}

void TransportList::SetSourceID(uint _sourceid)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("TransportList::SetSourceID(%1)")
                                   .arg(_sourceid));
#endif

    if (!_sourceid)
    {
        sourceid = 0;
    }
    else
    {
        cardtype = get_cardtype(_sourceid);
        sourceid = ((CardUtil::ERROR_OPEN    == cardtype) ||
                    (CardUtil::ERROR_UNKNOWN == cardtype) ||
                    (CardUtil::ERROR_PROBE   == cardtype)) ? 0 : _sourceid;
    }

    fillSelections();
}

TransportListEditor::TransportListEditor(uint sourceid) :
    m_videosource(new VideoSourceSelector(sourceid, QString::null, false)),
    m_list(new TransportList())
{
    setLabel(tr("Multiplex Editor"));

    m_list->SetSourceID(m_videosource->GetSourceID());

    addChild(m_videosource);
    addChild(m_list);

    connect(m_videosource, SIGNAL(valueChanged(const QString&)),
            m_list,        SLOT(  SetSourceID( const QString&)));

    connect(m_list, SIGNAL(accepted(int)),            this, SLOT(Edit()));
    connect(m_list, SIGNAL(menuButtonPressed(int)),   this, SLOT(Menu()));
    connect(m_list, SIGNAL(editButtonPressed(int)),   this, SLOT(Edit()));
    connect(m_list, SIGNAL(deleteButtonPressed(int)), this, SLOT(Delete()));
}

DialogCode TransportListEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted);

    return kDialogCodeRejected;
}

void TransportListEditor::Edit(void)
{
    uint sourceid = m_videosource->getValue().toUInt();
    CardUtil::CARD_TYPES cardtype = get_cardtype(sourceid);

    if ((CardUtil::ERROR_OPEN    != cardtype) &&
        (CardUtil::ERROR_UNKNOWN != cardtype) &&
        (CardUtil::ERROR_PROBE   != cardtype))
    {
        uint mplexid = m_list->getValue().toUInt();
        TransportWizard wiz(mplexid, sourceid, cardtype);
        wiz.exec();
        m_list->fillSelections();
    }
}

void TransportListEditor::Delete(void)
{
    uint mplexid = m_list->getValue().toInt();

    DialogCode val = MythPopupBox::Show2ButtonPopup(
        GetMythMainWindow(), "", 
        tr("Are you sure you would like to delete this transport?"), 
        tr("Yes, delete the transport"), 
        tr("No, don't"), kDialogCodeButton1);

    if (kDialogCodeButton0 != val)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM dtv_multiplex WHERE mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("TransportEditor -- delete multiplex", query);

    query.prepare("DELETE FROM channel WHERE mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("TransportEditor -- delete channels", query);

    m_list->fillSelections();
}

void TransportListEditor::Menu(void)
{
    uint mplexid = m_list->getValue().toInt();

    if (!mplexid)
    {
       Edit();
       return;
    }

    DialogCode val = MythPopupBox::Show2ButtonPopup(
        GetMythMainWindow(), 
        "", 
        tr("Transport Menu"), 
        tr("Edit..."), 
        tr("Delete..."), kDialogCodeButton0);

    if (kDialogCodeButton0 == val)
        emit Edit();
    else if (kDialogCodeButton1 == val)
        emit Delete();
    else
        m_list->setFocus();
}

class MuxDBStorage : public SimpleDBStorage
{
  protected:
    MuxDBStorage(Setting *_setting, const MultiplexID *_id, QString _name) :
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


class VideoSourceID : public IntegerSetting, public MuxDBStorage
{
  public:
    VideoSourceID(const MultiplexID *id, uint _sourceid) :
        IntegerSetting(this),
        MuxDBStorage(this, id, "sourceid")
    {
        setVisible(false);
        setValue(_sourceid);
    }
};

class DTVStandard : public ComboBoxSetting, public MuxDBStorage
{
  public:
    DTVStandard(const MultiplexID *id,
                bool is_dvb_country,
                bool is_atsc_country) :
        ComboBoxSetting(this), MuxDBStorage(this, id, "sistandard")
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

class Frequency : public LineEditSetting, public MuxDBStorage
{
  public:
    Frequency(const MultiplexID *id, bool in_kHz = false) :
        LineEditSetting(this), MuxDBStorage(this, id, "frequency")
    {
        QString hz = (in_kHz) ? "kHz" : "Hz";
        setLabel(QObject::tr("Frequency") + " (" + hz + ")");
        setHelpText(QObject::tr(
                        "Frequency (Option has no default).\n"
                        "The frequency for this channel in") + " " + hz + ".");
    };
};

class DVBSymbolRate : public ComboBoxSetting, public MuxDBStorage
{
  public:
    DVBSymbolRate(const MultiplexID *id) :
        ComboBoxSetting(this, true), MuxDBStorage(this, id, "symbolrate")
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(
            QObject::tr(
                "Symbol Rate (symbols/sec).\n"
                "Most DVB-S transponders transmit at 27.5 "
                "million symbols per second."));
        addSelection("3333000");
        addSelection("22000000");
        addSelection("27500000", "27500000", true);
        addSelection("28000000");
        addSelection("28500000");
        addSelection("29900000");
    };
};

class SignalPolarity : public ComboBoxSetting, public MuxDBStorage
{
  public:
    SignalPolarity(const MultiplexID *id) :
        ComboBoxSetting(this), MuxDBStorage(this, id, "polarity")
    {
        setLabel(QObject::tr("Polarity"));
        setHelpText(QObject::tr("Polarity (Option has no default)"));
        addSelection(QObject::tr("Horizontal"),     "h");
        addSelection(QObject::tr("Vertical"),       "v");
        addSelection(QObject::tr("Right Circular"), "r");
        addSelection(QObject::tr("Left Circular"),  "l");
    };
};

class Modulation : public ComboBoxSetting, public MuxDBStorage
{
  public:
    Modulation(const MultiplexID *id,  uint nType);
};

Modulation::Modulation(const MultiplexID *id,  uint nType) :
    ComboBoxSetting(this),
    MuxDBStorage(this, id, (CardUtil::OFDM == nType) ?
                 "constellation" : "modulation")
{
    setLabel(QObject::tr("Modulation"));
    setHelpText(QObject::tr("Modulation, aka Constellation"));

    if (CardUtil::QPSK == nType)
    {
        // no modulation options
        setVisible(false);
    }
    else if ((CardUtil::QAM == nType) || (CardUtil::OFDM == nType))
    {
        addSelection(QObject::tr("QAM Auto"), "auto");
        addSelection("QAM-16",   "qam_16");
        addSelection("QAM-32",   "qam_32");
        addSelection("QAM-64",   "qam_64");
        addSelection("QAM-128",  "qam_128");
        addSelection("QAM-256",  "qam_256");

        if (CardUtil::OFDM == nType)
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

class DVBInversion : public ComboBoxSetting, public MuxDBStorage
{
  public:
    DVBInversion(const MultiplexID *id) :
        ComboBoxSetting(this), MuxDBStorage(this, id, "inversion")
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

class DVBTBandwidth : public ComboBoxSetting, public MuxDBStorage
{
  public:
    DVBTBandwidth(const MultiplexID *id) :
        ComboBoxSetting(this), MuxDBStorage(this, id, "bandwidth")
    {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("Bandwidth (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "a");
        addSelection(QObject::tr("6 MHz"), "6");
        addSelection(QObject::tr("7 MHz"), "7");
        addSelection(QObject::tr("8 MHz"), "8");
    };
};

class DVBForwardErrorCorrectionSelector : public ComboBoxSetting
{
  public:
    DVBForwardErrorCorrectionSelector(Storage *_storage) :
        ComboBoxSetting(_storage)
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
    DVBForwardErrorCorrection(const MultiplexID *id) :
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
    DVBTCoderateLP(const MultiplexID *id) :
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
    DVBTCoderateHP(const MultiplexID *id) :
        DVBForwardErrorCorrectionSelector(this),
        MuxDBStorage(this, id, "hp_code_rate")
    {
        setLabel(QObject::tr("HP Coderate"));
        setHelpText(QObject::tr("High Priority Code Rate (Default: Auto)"));
    };
};

class DVBTGuardInterval : public ComboBoxSetting, public MuxDBStorage
{
  public:
    DVBTGuardInterval(const MultiplexID *id) :
        ComboBoxSetting(this), MuxDBStorage(this, id, "guard_interval")
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

class DVBTTransmissionMode : public ComboBoxSetting, public MuxDBStorage
{
  public:
    DVBTTransmissionMode(const MultiplexID *id) :
        ComboBoxSetting(this), MuxDBStorage(this, id, "transmission_mode")
    {
        setLabel(QObject::tr("Trans. Mode"));
        setHelpText(QObject::tr("Transmission Mode (Default: Auto)"));
        addSelection(QObject::tr("Auto"), "a");
        addSelection("2K", "2");
        addSelection("8K", "8");
    };
};

class DVBTHierarchy : public ComboBoxSetting, public MuxDBStorage
{
  public:
    DVBTHierarchy(const MultiplexID *id) :
        ComboBoxSetting(this), MuxDBStorage(this, id, "hierarchy")
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


class TransportPage : public HorizontalConfigurationGroup
{
  public:
    TransportPage(const MultiplexID *id, uint nType);

  protected:
    const MultiplexID *id;
};

TransportPage::TransportPage(const MultiplexID *_id, uint nType) :
    HorizontalConfigurationGroup(false, true, false, false), id(_id)
{
    setLabel(QObject::tr("Transport Options"));
    setUseLabel(false);

    VerticalConfigurationGroup *left = NULL, *right = NULL;

    left = new VerticalConfigurationGroup(false, true, false, false);

    if (CardUtil::OFDM == nType)
    {
        left->addChild(new DTVStandard(id, true, false));
        left->addChild(new Frequency(id));
        left->addChild(new DVBTBandwidth(id));
        left->addChild(new DVBInversion(id));
        left->addChild(new Modulation(id, nType));

        right = new VerticalConfigurationGroup(false, true, false, false);
        right->addChild(new DVBTCoderateLP(id));
        right->addChild(new DVBTCoderateHP(id));
        right->addChild(new DVBTTransmissionMode(id));
        right->addChild(new DVBTGuardInterval(id));
        right->addChild(new DVBTHierarchy(id));
    }
    else if (CardUtil::QPSK == nType)
    {
        left->addChild(new DTVStandard(id, true, false));
        left->addChild(new Frequency(id, true));
        left->addChild(new DVBSymbolRate(id));

        right = new VerticalConfigurationGroup(false, true, false, false);
        right->addChild(new DVBInversion(id));
        right->addChild(new DVBForwardErrorCorrection(id));
        right->addChild(new SignalPolarity(id));
    }
    else if (CardUtil::QAM == nType)
    {
        left->addChild(new DTVStandard(id, true, false));
        left->addChild(new Frequency(id));
        left->addChild(new DVBSymbolRate(id));

        right = new VerticalConfigurationGroup(false, true, false, false);
        right->addChild(new Modulation(id, nType));
        right->addChild(new DVBInversion(id));
        right->addChild(new DVBForwardErrorCorrection(id));
    }
    else if (CardUtil::ATSC      == nType ||
             CardUtil::HDHOMERUN == nType)
    {
        left->addChild(new DTVStandard(id, false, true));
        left->addChild(new Frequency(id));
        left->addChild(new Modulation(id, nType));
    }
    else if ((CardUtil::FIREWIRE == nType) ||
             (CardUtil::FREEBOX  == nType))
    {
        left->addChild(new DTVStandard(id, true, true));
    }
    else if ((CardUtil::V4L  == nType) ||
             (CardUtil::MPEG == nType))
    {
        left->addChild(new Frequency(id));
        left->addChild(new Modulation(id, nType));
    }

    addChild(left);

    if (right)
        addChild(right);
};

TransportWizard::TransportWizard(
    uint _mplexid, uint _sourceid, CardUtil::CARD_TYPES _cardtype) :
    mplexid(new MultiplexID())
{
    setLabel(QObject::tr("DVB Transport"));

    // Must be first.
    mplexid->setValue(_mplexid);
    addChild(mplexid);
    addChild(new VideoSourceID(mplexid, _sourceid));
    addChild(new TransportPage(mplexid, _cardtype));
}
