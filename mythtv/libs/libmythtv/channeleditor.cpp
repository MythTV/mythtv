#include <qsqldatabase.h>
#include "settings.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythwidgets.h"
#include "channeleditor.h"

#include <qapplication.h>
#include <qlayout.h>
#include <qdialog.h>
#include <qcursor.h>

#include <mythwidgets.h>
#include <mythdialogs.h>
#include <mythwizard.h>

#include "videosource.h"
#include "channelsettings.h"
#include "dvbtransporteditor.h"

#include "scanwizard.h"

ChannelWizard::ChannelWizard(int id)
             : ConfigurationWizard() {
    setLabel(QObject::tr("Channel Options"));

    // Must be first.
    addChild(cid = new ChannelID());
    cid->setValue(id);
        
    ChannelOptionsCommon* common = new ChannelOptionsCommon(*cid);
    addChild(common);

    int cardtypes = countCardtypes();
    bool hasDVB = cardTypesInclude("DVB");

    // add v4l options if no dvb or if dvb and some other card type
    // present
    QString cardtype = getCardtype();
    if (!hasDVB || cardtypes > 1 || id == 0) {
        ChannelOptionsV4L* v4l = new ChannelOptionsV4L(*cid);
        addChild(v4l);
    }
}

QString ChannelWizard::getCardtype() {
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype"
                  " FROM capturecard, cardinput, channel"
                  " WHERE channel.chanid = :CHID "
                  " AND channel.sourceid = cardinput.sourceid"
                  " AND cardinput.cardid = capturecard.cardid");
    query.bindValue(":CHID", cid->getValue()); 

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toString();
    }
    else
    {
        return "";
    }
}

bool ChannelWizard::cardTypesInclude(const QString& thecardtype) {
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT count(cardtype)"
        " FROM capturecard, cardinput, channel"
        " WHERE channel.chanid= :CHID "
        " AND channel.sourceid = cardinput.sourceid"
        " AND cardinput.cardid = capturecard.cardid"
        " AND capturecard.cardtype= :CARDTYPE ");
    query.bindValue(":CHID", cid->getValue());
    query.bindValue(":CARDTYPE", thecardtype);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        int count = query.value(0).toInt();

        if (count > 0)
            return true;
        else
            return false;
    } else
        return false;
}

int ChannelWizard::countCardtypes() {
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT count(DISTINCT cardtype)"
        " FROM capturecard, cardinput, channel"
        " WHERE channel.chanid = :CHID "
        " AND channel.sourceid = cardinput.sourceid"
        " AND cardinput.cardid = capturecard.cardid");
    query.bindValue(":CHID", cid->getValue());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toInt();
    } else
        return 0;
}

void ChannelListSetting::fillSelections(void) 
{
    QString currentValue = getValue();
    clearSelections();
    addSelection(QObject::tr("(New Channel)"));
    bool fAllSources = true;

    QString querystr = "SELECT channel.name,channum,chanid ";

    if ((currentSourceID == "") || (currentSourceID == "Unassigned"))
    {
        querystr += ",videosource.name FROM channel "
                    "LEFT JOIN videosource ON "
                    "(channel.sourceid = videosource.sourceid) ";
        fAllSources = true;
    }
    else
    {
        querystr += QString("FROM channel WHERE sourceid='%1' ")
                           .arg(currentSourceID);
        fAllSources = false;
    }
        
    if (currentSortMode == QObject::tr("Channel Name"))
    {
        querystr += " ORDER BY channel.name";
    }
    else if (currentSortMode == QObject::tr("Channel Number"))
    {
        querystr += " ORDER BY channum + 0";
    }

    MSqlQuery query(MSqlQuery::InitCon()); 
    query.prepare(querystr);
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while(query.next()) 
        {
            QString name = QString::fromUtf8(query.value(0).toString());
            QString channum = query.value(1).toString();
            QString chanid = query.value(2).toString();
            QString sourceid = "Unassigned";

            if (fAllSources && !query.value(3).toString().isNull())
            {
                sourceid = query.value(3).toString();
                if (currentSourceID == "Unassigned")
                    continue;
            }
 
            if (channum == "" && currentHideMode) 
                continue;

            if (name == "") 
                name = "(Unnamed : " + chanid + ")";

            if (currentSortMode == QObject::tr("Channel Name")) 
            {
                if (channum != "") 
                    name += " (" + channum + ")";
            }
            else if (currentSortMode == QObject::tr("Channel Number")) 
            {
                if (channum != "")
                    name = channum + ". " + name;
                else
                    name = "???. " + name;
            }

            if ((currentSourceID == "") && (currentSourceID != "Unassigned"))
                name += " (" + sourceid  + ")";

            addSelection(name, chanid, (chanid == currentValue) ? true : false);
        }
    }
}

class SourceSetting: public ComboBoxSetting {
public:
    SourceSetting(): ComboBoxSetting() {
        setLabel(QObject::tr("Video Source"));
        addSelection(QObject::tr("(All)"),"");
    };

    void save() {};
    void load() 
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT name, sourceid FROM videosource");

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while(query.next())
            {
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
            } 
        } 
        addSelection(QObject::tr("(Unassigned)"),"Unassigned");
    }
};

class SortMode: public ComboBoxSetting, public TransientStorage {
public:
    SortMode(): ComboBoxSetting() {
        setLabel(QObject::tr("Sort Mode"));
        addSelection(QObject::tr("Channel Name"));
        addSelection(QObject::tr("Channel Number"));
    };
};

class NoChanNumHide: public CheckBoxSetting, public TransientStorage {
public:
    NoChanNumHide() {
        setLabel(QObject::tr("Hide channels without channel number."));
    };
};

ChannelEditor::ChannelEditor():
    VerticalConfigurationGroup(), ConfigurationDialog()
{

    setLabel(tr("Channels"));
    addChild(list = new ChannelListSetting());

    SortMode           *sort   = new SortMode();
    source                     = new SourceSetting();
    TransButtonSetting *del    = new TransButtonSetting();
    NoChanNumHide      *hide   = new NoChanNumHide();

    del->setLabel(tr("Delete Channels"));
    del->setHelpText(
        tr("Delete all channels on currently selected source[s]."));

    HorizontalConfigurationGroup *src =
        new HorizontalConfigurationGroup(false, false, true, true);
    src->addChild(source);
    src->addChild(del);

    sort->setValue(sort->getValueIndex(list->getSortMode()));
    source->setValue(source->getValueIndex(list->getSourceID()));
    hide->setValue(list->getHideMode());

    addChild(sort);
    addChild(src);
    addChild(hide);

    buttonScan = new TransButtonSetting();
    buttonScan->setLabel(QObject::tr("Channel Scanner"));
    buttonScan->setHelpText(QObject::tr("Starts the channel scanner."));

    buttonTransportEditor = new TransButtonSetting();
    buttonTransportEditor->setLabel(QObject::tr("Transport Editor"));
    buttonTransportEditor->setHelpText(
        QObject::tr("Allows you to edit the transports directly") + " " +
        QObject::tr("This is rarely required unless you are using "
                    "a satelite dish and must enter an initial "
                    "frequency to for the channel scanner to try."));

    HorizontalConfigurationGroup *h = 
        new HorizontalConfigurationGroup(false, false);
    h->addChild(buttonScan);
    h->addChild(buttonTransportEditor);
    addChild(h);

    connect(source, SIGNAL(valueChanged(const QString&)),
            list, SLOT(setSourceID(const QString&)));
    connect(sort, SIGNAL(valueChanged(const QString&)),
            list, SLOT(setSortMode(const QString&)));
    connect(hide, SIGNAL(valueChanged(bool)),
            list, SLOT(setHideMode(bool)));
    connect(list, SIGNAL(accepted(int)),
            this, SLOT(edit(int)));
    connect(list, SIGNAL(menuButtonPressed(int)),
            this, SLOT(menu(int)));
    connect(buttonScan, SIGNAL(pressed()),
            this, SLOT(scan()));
    connect(buttonTransportEditor, SIGNAL(pressed()),
            this, SLOT(transportEditor()));
    connect(del,  SIGNAL(pressed()),
            this, SLOT(deleteChannels()));
}

void ChannelEditor::deleteChannels(void)
{
    const QString currentSourceID = source->getValue();

    int val = MythPopupBox::show2ButtonPopup(
        gContext->GetMainWindow(), "",
        tr("Are you sure you would like to delete these channels?"),
        tr("Yes, delete the channels"),
        tr("No, don't"), 2);

    if (val != 0) 
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    if ((currentSourceID == "") || (currentSourceID == "Unassigned"))
    {
        query.prepare("TRUNCATE TABLE channel");
    }
    else
    {
        query.prepare("DELETE FROM channel "
                      "WHERE sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", currentSourceID);
    }
    query.exec();

    list->fillSelections();
}

MythDialog* ChannelEditor::dialogWidget(MythMainWindow* parent,
                                        const char* widgetName)
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(editButtonPressed()),   this, SLOT(edit()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(del()));
    return dialog;
}

int ChannelEditor::exec()
{
    while (ConfigurationDialog::exec() == QDialog::Accepted)  {}
    return QDialog::Rejected;
}

void ChannelEditor::edit()
{
    id = list->getValue().toInt();
    ChannelWizard cw(id);
    cw.exec();

    list->fillSelections();
    list->setFocus();
}

void ChannelEditor::edit(int /*iSelected*/)
{ 
    edit();
}

void ChannelEditor::del()
{
    id = list->getValue().toInt();

    int val = MythPopupBox::show2ButtonPopup(
        gContext->GetMainWindow(), "",
        tr("Are you sure you would like to delete this channel?"),
        tr("Yes, delete the channel"),
        tr("No, don't"), 2);

    if (val == 0) 
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM channel WHERE chanid = :CHID ;");
        query.bindValue(":CHID", id);
        if (!query.exec() || !query.isActive())
            MythContext::DBError("ChannelEditor Delete Channel", query);

        list->fillSelections();
    }
}

void ChannelEditor::menu(int /*iSelected*/)
{
    id = list->getValue().toInt();
    if (id == 0)
       edit();
    else
    {
        int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                                 "",
                                                 tr("Channel Menu"),
                                                 tr("Edit.."),
                                                 tr("Delete.."), 1);

        if (val == 0)
            emit edit();
        else if (val == 1)
            emit del();
        else
            list->setFocus();
    }
}

void ChannelEditor::scan()
{
#ifdef USING_BACKEND
    ScanWizard scanwizard;
    scanwizard.exec(false,true);

    list->fillSelections();
    list->setFocus();
#else
    VERBOSE(VB_IMPORTANT,  "You must compile the backend "
            "to be able to scan for channels");
#endif
}

void ChannelEditor::transportEditor()
{
    DVBTransportsEditor advancedDialog;
    advancedDialog.exec();
    list->fillSelections();
    list->setFocus();
}

