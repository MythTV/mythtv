#include <qsqldatabase.h>
#include "settings.h"
#include "mythcontext.h"
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

#ifdef USING_DVB
#include "scanwizard.h"
#endif

ChannelWizard::ChannelWizard(int id, QSqlDatabase* _db)
             : ConfigurationWizard(), db(_db) {
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
    QSqlQuery query = db->exec(QString("SELECT cardtype"
        " FROM capturecard, cardinput, channel"
        " WHERE channel.chanid=%1"
        " AND channel.sourceid = cardinput.sourceid"
        " AND cardinput.cardid = capturecard.cardid")
        .arg(cid->getValue()));
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        return query.value(0).toString();
    } else
        return "";
}

bool ChannelWizard::cardTypesInclude(const QString& thecardtype) {
    QSqlQuery query = db->exec(QString("SELECT count(cardtype)"
        " FROM capturecard, cardinput, channel"
        " WHERE channel.chanid=%1"
        " AND channel.sourceid = cardinput.sourceid"
        " AND cardinput.cardid = capturecard.cardid"
        " AND capturecard.cardtype=\"%2\"")
        .arg(cid->getValue())
        .arg(thecardtype));
    if (query.isActive() && query.numRowsAffected() > 0)
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
    QSqlQuery query = db->exec(QString("SELECT count(DISTINCT cardtype)"
        " FROM capturecard, cardinput, channel"
        " WHERE channel.chanid=%1"
        " AND channel.sourceid = cardinput.sourceid"
        " AND cardinput.cardid = capturecard.cardid")
        .arg(cid->getValue()));
    if (query.isActive() && query.numRowsAffected() > 0)
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
    QString querystr = "SELECT channel.name,channum,chanid ";
    if ((currentSourceID == "") ||
        (currentSourceID == "Unassigned"))
        querystr += ",videosource.name FROM channel "
                    "LEFT JOIN videosource ON "
                    "(channel.sourceid = videosource.sourceid) ";
    else
        querystr += QString("FROM channel WHERE sourceid='%1' ")
                           .arg(currentSourceID);
        
    if (currentSortMode == QObject::tr("Channel Name"))
        querystr += " ORDER BY channel.name";
    else if (currentSortMode == QObject::tr("Channel Number"))
        querystr += " ORDER BY channum + 0";

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
        while(query.next()) {
            QString name = QString::fromUtf8(query.value(0).toString());
            QString channum = query.value(1).toString();
            QString chanid = query.value(2).toString();
            QString sourceid = "Unassigned";
			
            if (!query.value(3).toString().isNull())
            {
                sourceid = query.value(3).toString();
                if (currentSourceID == "Unassigned")
                    continue;
            }
 
            if (channum == "" && currentHideMode) continue;

            if (name == "") name = "(Unnamed : " + chanid + ")";

            if (currentSortMode == QObject::tr("Channel Name")) {
                if (channum != "") name += " (" + channum + ")";
            } else if (currentSortMode == QObject::tr("Channel Number")) {
                if (channum != "")
                    name = channum + ". " + name;
                else
                    name = "???. " + name;
            }

            if ((currentSourceID == "") &&
                (currentSourceID != "Unassigned"))
                name += " (" + sourceid  + ")";

            addSelection(name, chanid, (chanid == currentValue) ? true : false);
        }
}

class SourceSetting: public ComboBoxSetting {
public:
    SourceSetting(): ComboBoxSetting() {
        setLabel(QObject::tr("Video Source"));
        addSelection(QObject::tr("(All)"),"");
    };

    void save(QSqlDatabase* db) { (void)db; };
    void load(QSqlDatabase* db) {
        QSqlQuery query = db->exec(QString(
            "SELECT name, sourceid"
            " FROM videosource"));
        if (query.isActive() && query.numRowsAffected() > 0)
            while(query.next())
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
        addSelection(QObject::tr("(Unassigned)"),"Unassigned");
    };
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
    VerticalConfigurationGroup(), ConfigurationDialog() {

    setLabel(tr("Channels"));
    addChild(list = new ChannelListSetting());

    SortMode* sort = new SortMode();
    SourceSetting* source = new SourceSetting();
    NoChanNumHide* hide = new NoChanNumHide();

    sort->setValue(sort->getValueIndex(list->getSortMode()));
    source->setValue(source->getValueIndex(list->getSourceID()));
    hide->setValue(list->getHideMode());

    addChild(sort);
    addChild(source);
    addChild(hide);

#ifdef USING_DVB
    buttonScan = new TransButtonSetting();
    buttonScan->setLabel(QObject::tr("Scan for channels(s)"));
    buttonScan->setHelpText(QObject::tr("This button will scan for digital channels."));
    buttonAdvanced = new TransButtonSetting();
    buttonAdvanced->setLabel(QObject::tr("Advanced"));
    buttonAdvanced->setHelpText(QObject::tr("Advanced editing options for digital channels"));

    HorizontalConfigurationGroup *h = new HorizontalConfigurationGroup(false, false);
    h->addChild(buttonScan);
    h->addChild(buttonAdvanced);
    addChild(h);
#endif

    connect(source, SIGNAL(valueChanged(const QString&)),
            list, SLOT(setSourceID(const QString&)));
    connect(sort, SIGNAL(valueChanged(const QString&)),
            list, SLOT(setSortMode(const QString&)));
    connect(hide, SIGNAL(valueChanged(bool)),
            list, SLOT(setHideMode(bool)));
    connect(list, SIGNAL(accepted(int)), this, SLOT(edit(int)));
    connect(list, SIGNAL(menuButtonPressed(int)), this, SLOT(menu(int)));
#ifdef USING_DVB
    connect(buttonScan, SIGNAL(pressed()), this, SLOT(scan()));
    connect(buttonAdvanced, SIGNAL(pressed()), this, SLOT(advanced()));
#endif
}

int ChannelEditor::exec(QSqlDatabase* _db) {
    db = _db;

    //Make sure we have dvb card installed
#ifdef USING_DVB
    if (!CardUtil::isCardPresent(db, CardUtil::DVB))
    {
        buttonScan->setEnabled(false);
        buttonAdvanced->setEnabled(false);
    }
#endif

    while (ConfigurationDialog::exec(db) == QDialog::Accepted)  {}
    return QDialog::Rejected;
}

void ChannelEditor::edit() {
    id = list->getValue().toInt();
    ChannelWizard cw(id,db);
    cw.exec(db);

    list->fillSelections();
    list->setFocus();
}

void ChannelEditor::edit(int /*iSelected*/) {
    id = list->getValue().toInt();
    edit();
}

void ChannelEditor::del() {
    int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "",
                                             tr("Are you sure you would like to"
                                             " delete this channel?"), 
                                             tr("Yes, delete the channel"), 
                                             tr("No, don't"), 2);

    if (val == 0) {
        QSqlQuery query;
    
        query = db->exec(QString("DELETE FROM channel "
                                 "WHERE chanid ='%1'").arg(id));
        if (!query.isActive())
            MythContext::DBError("ChannelEditor Delete Channel", query);

        list->fillSelections();
    }
}

void ChannelEditor::menu(int /*iSelected*/) {
    id = list->getValue().toInt();
    if (id == 0) {
       edit();
    } else {
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
#ifdef USING_DVB
    ScanWizard scanwizard(db);
    scanwizard.exec(db,false,true);

    list->fillSelections();
    list->setFocus();
#endif
}

void ChannelEditor::advanced()
{
#ifdef USING_DVB
    DVBTransportsEditor advancedDialog;
    advancedDialog.exec(db);
#endif
}

