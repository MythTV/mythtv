/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *      Taylor Jacob (rtjacob@earthlink.net)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "dvbtransporteditor.h"
#include "dvbtypes.h"
#include "dvbdev.h"


void DVBTransportList::fillSelections(void)
{
    clearSelections();
    addSelection("(New Transport)");

    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = QString("SELECT mplexid, networkid, transportid, "
                       " frequency, symbolrate, modulation FROM dtv_multiplex channel "
                       " WHERE sourceid=%1 ORDER by networkid, transportid ")
                       .arg(strSourceID);
    query.prepare(querystr);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while(query.next()) 
        {
            QString DisplayText;
            DisplayText = QString("%1 Hz (%2) (%3) (%4)")
                                  .arg(query.value(3).toString())
                                  .arg(query.value(4).toString())
                                  .arg(query.value(1).toInt())
                                  .arg(query.value(2).toInt());
            addSelection(DisplayText, query.value(0).toString());
        }
    }
    setHelpText(QObject::tr("This section lists each transport that MythTV "
                "currently knows about. The display fields are Frequency, "
                "SymbolRate, NetworkID, and TransportID "));
}

class DVBTSourceSetting: public ComboBoxSetting {
public:
    DVBTSourceSetting(): ComboBoxSetting() {
        setLabel(QObject::tr("Video Source"));
    };

    void save() { };
    void load() 
    {
        MSqlQuery query(MSqlQuery::InitCon());

        QString querystr = QString(
        "SELECT DISTINCT videosource.name, videosource.sourceid "
        "FROM cardinput, videosource, capturecard "
        "WHERE cardinput.sourceid=videosource.sourceid "
        "AND cardinput.cardid=capturecard.cardid "
        "AND capturecard.cardtype in (\"DVB\") "
        "AND capturecard.hostname=\"%1\"").arg(gContext->GetHostName());

        query.prepare(querystr);

        if (query.exec() && query.isActive() && query.size() > 0)
            while(query.next())
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
    };
};

DVBTransportsEditor::DVBTransportsEditor():
    VerticalConfigurationGroup(false)
{
    setLabel(tr("DVB Transport Editor"));
    setUseLabel(true);
    addChild(m_list = new DVBTransportList());
    addChild(m_videoSource = new DVBTSourceSetting());

    connect(m_videoSource,SIGNAL(valueChanged(const QString&)),
            m_list,SLOT(sourceID(const QString&)));
    connect(m_videoSource,SIGNAL(valueChanged(const QString&)),
            this,SLOT(videoSource(const QString&)));

    connect(m_list, SIGNAL(accepted(int)), this, SLOT(edit(int)));
    connect(m_list, SIGNAL(menuButtonPressed(int)), this, SLOT(menu(int)));
    m_nID =0;
}

void DVBTransportsEditor::videoSource(const QString& str )
{
    MSqlQuery query(MSqlQuery::InitCon());

    bool fEnable = false;

    QString querystr = QString(
        "SELECT count(cardtype) "
        "FROM cardinput, videosource, capturecard "
        "WHERE cardinput.sourceid=videosource.sourceid "
        "AND cardinput.cardid=capturecard.cardid "
        "AND capturecard.cardtype in (\"DVB\") "
        "AND videosource.sourceid = %1 "
        "AND capturecard.hostname=\"%2\"").arg(str).arg(gContext->GetHostName());
    
    query.prepare(querystr);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
         query.next();
         if (query.value(0).toInt() > 0)
             fEnable = true;
    }

    m_list->setEnabled(fEnable);
}

int DVBTransportsEditor::exec()
{
    while (ConfigurationDialog::exec() == QDialog::Accepted)  {}
    return QDialog::Rejected;
}

void DVBTransportsEditor::edit()
{
    DVBTransportWizard dvbt(m_nID,m_videoSource->getValue().toInt());
    dvbt.exec();
    m_list->fillSelections();
}

void DVBTransportsEditor::edit(int /*tid*/)
{
    m_nID = m_list->getValue().toInt();
    edit();
}

void DVBTransportsEditor::del() {
    int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "",
                                             tr("Are you sure you would like to"
                                             " delete this transport?"),
                                             tr("Yes, delete the transport"),
                                             tr("No, don't"), 2);

    if (val == 0) 
    {
        MSqlQuery query(MSqlQuery::InitCon());
        QString querystr = QString("DELETE FROM dtv_multiplex "
                                   "WHERE mplexid ='%1'").arg(m_nID);
        query.prepare(querystr);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("TransportEditor Delete DVBTransport", query);

        querystr = QString("DELETE FROM channel "
                                   "WHERE mplexid ='%1'").arg(m_nID);
        query.prepare(querystr);
        if (!query.exec() || !query.isActive())
            MythContext::DBError("TransportEditor Delete associated channels", query);
        m_list->fillSelections();
    }
}

void DVBTransportsEditor::menu(int /*iSelected*/) {
    m_nID = m_list->getValue().toInt();
    if (m_nID == 0) {
       edit();
    } else {
        int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                                 "",
                                                 tr("Transport Menu"),
                                                 tr("Edit.."),
                                                 tr("Delete.."), 1);

        if (val == 0)
            emit edit();
        else if (val == 1)
            emit del();
        else
            m_list->setFocus();
    }
}

class DvbTransSetting: public SimpleDBStorage {
protected:
    DvbTransSetting(const DVBTID& id, QString name):
      SimpleDBStorage("dtv_multiplex", name), id(id)
    {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);

    const DVBTID& id;
};

QString DvbTransSetting::whereClause(void) {
    return QString("%1=%2").arg(id.getField()).arg(id.getValue());
}

QString DvbTransSetting::setClause(void) {
    return QString("%1=%2, %3='%4'").arg(id.getField()).arg(id.getValue())
                   .arg(getName()).arg(getValue());
}


class DvbTVideoSourceID: public IntegerSetting, public DvbTransSetting {
public:
    DvbTVideoSourceID(const DVBTID& id,unsigned _nVideoSource) :
        DvbTransSetting(id, "sourceid")
    {
        setVisible(false);
        setValue(_nVideoSource);
    }
};

class DTVTStandard: public ComboBoxSetting, public DvbTransSetting {
public:
    DTVTStandard(const DVBTID& id): ComboBoxSetting(),
                 DvbTransSetting(id, "sistandard")
    {
        setLabel(QObject::tr("Standard"));
        setHelpText(QObject::tr("Digital TV standard.\n"));
        addSelection(QObject::tr("DVB"), "dvb");
        addSelection(QObject::tr("ATSC"), "atsc");
    };
};

class DvbTFrequency: public LineEditSetting, public DvbTransSetting {
public:
    DvbTFrequency(const DVBTID& id):
        LineEditSetting(), DvbTransSetting(id, "frequency") {
        setLabel(QObject::tr("Frequency"));
        setHelpText(QObject::tr("Frequency (Option has no default)\n"
                    "The frequency for this channel in Hz."));
    };
};

class DvbTSymbolrate: public LineEditSetting, public DvbTransSetting {
public:
    DvbTSymbolrate(const DVBTID& id):
        LineEditSetting(), DvbTransSetting(id, "symbolrate") {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(QObject::tr("Symbol Rate (Option has no default)"));
    };
};

class DvbTPolarity: public ComboBoxSetting, public DvbTransSetting {
public:
    DvbTPolarity(const DVBTID& id):
        ComboBoxSetting(), DvbTransSetting(id, "polarity") {
        setLabel(QObject::tr("Polarity"));
        setHelpText(QObject::tr("Polarity (Option has no default)"));
        addSelection(QObject::tr("Horizontal"), "h");
        addSelection(QObject::tr("Vertical"), "v");
        addSelection(QObject::tr("Right Circular"), "r");
        addSelection(QObject::tr("Left Circular"), "l");
    };
};

class DvbTATSCModulation: public ComboBoxSetting, public DvbTransSetting {
public:
    DvbTATSCModulation(const DVBTID& id):
        ComboBoxSetting(), DvbTransSetting(id, "modulation") {
        setLabel(QObject::tr("Modulation"));
        setHelpText(QObject::tr("Modulation Used"));
        addSelection(QObject::tr("8VSB"), "8vsb");
        addSelection(QObject::tr("QAM64"), "qam_64");
        addSelection(QObject::tr("QAM256"), "qam_256");
    };
};



class DvbTInversion: public ComboBoxSetting, public DvbTransSetting {
public:
    DvbTInversion(const DVBTID& id):
        ComboBoxSetting(), DvbTransSetting(id, "inversion") {
        setLabel(QObject::tr("Inversion"));
        setHelpText(QObject::tr("Inversion (Default: Auto):\n"
                    "Most cards can autodetect this now, so leave it at Auto"
                    " unless it won't work."));
        addSelection(QObject::tr("Auto"), "a");
        addSelection(QObject::tr("On"), "1");
        addSelection(QObject::tr("Off"), "0");
    };
};

class DvbTBandwidth: public ComboBoxSetting, public DvbTransSetting {
public:
    DvbTBandwidth(const DVBTID& id):
        ComboBoxSetting(), DvbTransSetting(id, "bandwidth") {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("Bandwidth (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection(QObject::tr("6 MHz"),"6");
        addSelection(QObject::tr("7 MHz"),"7");
        addSelection(QObject::tr("8 MHz"),"8");
    };
};

class DvbTModulationSetting: public ComboBoxSetting {
public:
    DvbTModulationSetting() {
        addSelection(QObject::tr("Auto"),"auto");
        addSelection("QPSK","qpsk");
        addSelection("QAM 16","qam_16");
        addSelection("QAM 32","qam_32");
        addSelection("QAM 64","qam_64");
        addSelection("QAM 128","qam_128");
        addSelection("QAM 256","qam_256");
    };
};

class DvbTModulation: public DvbTModulationSetting, public DvbTransSetting {
public:
    DvbTModulation(const DVBTID& id):
        DvbTModulationSetting(), DvbTransSetting(id, "modulation") {
        setLabel(QObject::tr("Modulation"));
        setHelpText(QObject::tr("Modulation (Default: Auto)"));
    };
};

class DvbTConstellation: public DvbTModulationSetting, public DvbTransSetting {
public:
    DvbTConstellation(const DVBTID& id):
        DvbTModulationSetting(), DvbTransSetting(id, "constellation") {
        setLabel(QObject::tr("Constellation"));
        setHelpText(QObject::tr("Constellation (Default: Auto)"));
    };
};

class DvbTFecSetting: public ComboBoxSetting {
public:
    DvbTFecSetting(): ComboBoxSetting() {
        addSelection(QObject::tr("Auto"),"auto");
        addSelection(QObject::tr("None"),"none");
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

class DvbTFec: public DvbTFecSetting, public DvbTransSetting {
public:
    DvbTFec(const DVBTID& id):
        DvbTFecSetting(), DvbTransSetting(id, "fec") {
        setLabel(QObject::tr("FEC"));
        setHelpText(QObject::tr("Forward Error Correction (Default: Auto)"));
    };
};

class DvbTCoderateLP: public DvbTFecSetting, public DvbTransSetting {
public:
    DvbTCoderateLP(const DVBTID& id):
        DvbTFecSetting(), DvbTransSetting(id, "lp_code_rate") {
        setLabel(QObject::tr("LP Coderate"));
        setHelpText(QObject::tr("Low Priority Code Rate (Default: Auto)"));
    };
};

class DvbTCoderateHP: public DvbTFecSetting, public DvbTransSetting {
public:
    DvbTCoderateHP(const DVBTID& id):
        DvbTFecSetting(), DvbTransSetting(id, "hp_code_rate") {
        setLabel(QObject::tr("HP Coderate"));
        setHelpText(QObject::tr("High Priority Code Rate (Default: Auto)"));
    };
};

class DvbTGuardInterval: public ComboBoxSetting, public DvbTransSetting {
public:
    DvbTGuardInterval(const DVBTID& id):
        ComboBoxSetting(), DvbTransSetting(id, "guard_interval") {
        setLabel(QObject::tr("Guard Interval"));
        setHelpText(QObject::tr("Guard Interval (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"auto");
        addSelection("1/4");
        addSelection("1/8");
        addSelection("1/16");
        addSelection("1/32");
    };
};

class DvbTTransmissionMode: public ComboBoxSetting, public DvbTransSetting {
public:
    DvbTTransmissionMode(const DVBTID& id):
        ComboBoxSetting(), DvbTransSetting(id, "transmission_mode") {
        setLabel(QObject::tr("Trans. Mode"));
        setHelpText(QObject::tr("Transmission Mode (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection("2K","2");
        addSelection("8K","8");
    };
};

class DvbTHierarchy: public ComboBoxSetting, public DvbTransSetting {
public:
    DvbTHierarchy(const DVBTID& id):
        ComboBoxSetting(), DvbTransSetting(id, "hierarchy") {
        setLabel(QObject::tr("Hierarchy"));
        setHelpText(QObject::tr("Hierarchy (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection(QObject::tr("None"), "n");
        addSelection("1");
        addSelection("2");
        addSelection("4");
    };
};

DVBTransportWizard::DVBTransportWizard(int id, unsigned _nVideoSourceID) :
              ConfigurationWizard()
{
    setLabel(QObject::tr("DVB Transport"));

    // Must be first.
    dvbtid = new DVBTID();
    dvbtid->setValue(id);
    addChild(dvbtid);
    addChild(new DvbTVideoSourceID(*dvbtid,_nVideoSourceID));

    int iVideoDev = -1;

    //Work out what kind of card we've got
    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr = QString(
           "SELECT capturecard.videodevice FROM cardinput,capturecard "
           " WHERE capturecard.cardid = cardinput.cardid "
           " AND cardinput.sourceid=%1 "
           " AND capturecard.cardtype=\"DVB\"").arg(_nVideoSourceID);
    query.prepare(querystr);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        iVideoDev = query.value(0).toInt();
    }

    CardUtil::CARD_TYPES nType = CardUtil::ERROR_PROBE;
    if (iVideoDev >= 0)
        nType = CardUtil::GetCardType(iVideoDev);

     addChild(new DVBTransportPage(*dvbtid,nType));
}

DVBTransportPage::DVBTransportPage(const DVBTID& id, unsigned nType)
                 : HorizontalConfigurationGroup(false,true), id(id)
{
    setLabel(QObject::tr("Transport Options"));
    setUseLabel(false);
    VerticalConfigurationGroup* left = new VerticalConfigurationGroup(false,true);
    VerticalConfigurationGroup* right = new VerticalConfigurationGroup(false,true);
    left->addChild(dtvStandard  = new DTVTStandard(id));
    switch (nType)
    {
    case CardUtil::OFDM:
        left->addChild(frequency  = new DvbTFrequency(id));
        left->addChild(bandwidth = new DvbTBandwidth(id));
        left->addChild(inversion = new DvbTInversion(id));
        left->addChild(constellation = new DvbTConstellation(id));
        right->addChild(coderate_lp = new DvbTCoderateLP(id));
        right->addChild(coderate_hp = new DvbTCoderateHP(id));
        right->addChild(trans_mode = new DvbTTransmissionMode(id));
        right->addChild(guard_interval = new DvbTGuardInterval(id));
        right->addChild(hierarchy = new DvbTHierarchy(id));
        break;
    case CardUtil::QPSK:
        left->addChild(frequency  = new DvbTFrequency(id));
        left->addChild(symbolrate = new DvbTSymbolrate(id));
        right->addChild(inversion = new DvbTInversion(id));
        right->addChild(fec = new DvbTFec(id));
        right->addChild(polarity = new DvbTPolarity(id));
        break;
    case CardUtil::QAM:
        left->addChild(frequency  = new DvbTFrequency(id));
        left->addChild(symbolrate = new DvbTSymbolrate(id));
        right->addChild(modulation = new DvbTModulation(id));
        right->addChild(inversion = new DvbTInversion(id));
        right->addChild(fec = new DvbTFec(id));
        break;
    case CardUtil::ATSC:
        left->addChild(frequency  = new DvbTFrequency(id));
        right->addChild(atscmodulation = new DvbTATSCModulation(id));
        break;
    default:
        cerr << "card detection error\n";
    }
    addChild(left);
    addChild(right);

};
