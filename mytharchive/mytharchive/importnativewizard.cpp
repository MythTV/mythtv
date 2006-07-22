// Qt
#include <qdir.h>
#include <qapplication.h>
#include <qfileinfo.h>
#include <qsqldatabase.h>

// Myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/uitypes.h>

// mytharchive
#include "importnativewizard.h"
#include "mytharchivewizard.h"

// last page in wizard
const int LAST_PAGE = 2;

////////////////////////////////////////////////////////////////

ImportNativeWizard::ImportNativeWizard(const QString &startDir, 
                const QString &filemask, MythMainWindow *parent, 
                const QString &window_name, const QString &theme_filename, 
                const char *name)
                :MythThemedDialog(parent, window_name, theme_filename, name)
{
    setContext(1);
    m_filemask = filemask;
    m_curDirectory = startDir;
    m_isValidXMLSelected = false;
    wireUpTheme();
}

ImportNativeWizard::~ImportNativeWizard()
{
}

void ImportNativeWizard::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                UIListBtnTypeItem *item = m_fileList->GetItemCurrent();
                FileInfo *fileData = (FileInfo*)item->getData();

                if (fileData->directory)
                {
                    if (fileData->filename == "..")
                    {
                        // move up on directory
                        int pos = m_curDirectory.findRev('/');
                        if (pos > 0)
                            m_curDirectory = m_curDirectory.left(pos);
                        else
                            m_curDirectory = "/";
                    }
                    else
                    {
                        if (!m_curDirectory.endsWith("/"))
                            m_curDirectory += "/";
                        m_curDirectory += fileData->filename;
                    }
                    updateFileList();
                }
                else
                {
                    QString fullPath = m_curDirectory;
                    if (!fullPath.endsWith("/"))
                        fullPath += "/";
                    fullPath += fileData->filename;

                    if (item->state() == UIListBtnTypeItem::FullChecked)
                    {
                        m_selectedList.remove(fullPath);
                        item->setChecked(UIListBtnTypeItem::NotChecked);
                    }
                    else
                    {
                        if (m_selectedList.findIndex(fullPath) == -1)
                            m_selectedList.append(fullPath);
                        item->setChecked(UIListBtnTypeItem::FullChecked);
                    }

                    m_fileList->refresh();
                }
            }
            else
                activateCurrent();
        }
        else if (action == "PAUSE")
        {
        }
        else if (action == "UP")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                m_fileList->MoveUp(UIListBtnType::MoveItem);
                m_fileList->refresh();
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                m_fileList->MoveDown(UIListBtnType::MoveItem);
                m_fileList->refresh();
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "LEFT")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                m_fileList->MoveUp(UIListBtnType::MovePage);
                m_fileList->refresh();
            }
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                m_fileList->MoveDown(UIListBtnType::MovePage);
                m_fileList->refresh();
            }
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void ImportNativeWizard::wireUpTheme()
{
    m_fileList = getUIListBtnType("filelist");
    if (m_fileList)
    {
        connect(m_fileList, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(selectedChanged(UIListBtnTypeItem *)));
    }

    m_locationEdit = getUIRemoteEditType("location_edit");
    if (m_locationEdit)
    {
        m_locationEdit->createEdit(this);
        connect(m_locationEdit, SIGNAL(loosingFocus()), 
                this, SLOT(locationEditLostFocus()));
    }

    // back button
    m_backButton = getUITextButtonType("back_button");
    if (m_backButton)
    {
        m_backButton->setText(tr("Back"));
        connect(m_backButton, SIGNAL(pushed()), this, SLOT(backPressed()));
    }

    // home button
    m_homeButton = getUITextButtonType("home_button");
    if (m_homeButton)
    {
        m_homeButton->setText(tr("Home"));
        connect(m_homeButton, SIGNAL(pushed()), this, SLOT(homePressed()));
    }

    m_title_text = getUITextType("title_text");
    m_subtitle_text = getUITextType("subtitle_text");
    m_starttime_text = getUITextType("starttime_text");

    // second page
    m_progTitle_text = getUITextType("progtitle");
    m_progDateTime_text = getUITextType("progdatetime");
    m_progDescription_text = getUITextType("progdescription");

    m_chanID_text = getUITextType("chanid");
    m_chanNo_text = getUITextType("channo");
    m_chanName_text = getUITextType("name");
    m_callsign_text = getUITextType("callsign");

    m_localChanID_text = getUITextType("local_chanid");
    m_localChanNo_text = getUITextType("local_channo");
    m_localChanName_text = getUITextType("local_name");
    m_localCallsign_text = getUITextType("local_callsign");

    m_searchChanID_button = getUIPushButtonType("searchchanid_button");
    if (m_searchChanID_button)
    {
        connect(m_searchChanID_button, SIGNAL(pushed()), this, SLOT(searchChanID()));
    }

    m_searchChanNo_button = getUIPushButtonType("searchchanno_button");
    if (m_searchChanNo_button)
    {
        connect(m_searchChanNo_button, SIGNAL(pushed()), this, SLOT(searchChanNo()));
    }

    m_searchChanName_button = getUIPushButtonType("searchname_button");
    if (m_searchChanName_button)
    {
        connect(m_searchChanName_button, SIGNAL(pushed()), this, SLOT(searchName()));
    }

    m_searchCallsign_button = getUIPushButtonType("searchcallsign_button");
    if (m_searchCallsign_button)
    {
        connect(m_searchCallsign_button, SIGNAL(pushed()), this, SLOT(searchCallsign()));
    }

    // common buttons
    // next button
    m_nextButton = getUITextButtonType("next_button");
    if (m_nextButton)
    {
        m_nextButton->setText(tr("Next"));
        connect(m_nextButton, SIGNAL(pushed()), this, SLOT(nextPressed()));
    }

    // prev button
    m_prevButton = getUITextButtonType("prev_button");
    if (m_prevButton)
    {
        m_prevButton->setText(tr("Previous"));
        connect(m_prevButton, SIGNAL(pushed()), this, SLOT(prevPressed()));
    }

    // cancel button
    m_cancelButton = getUITextButtonType("cancel_button");
    if (m_cancelButton)
    {
        m_cancelButton->setText(tr("Cancel"));
        connect(m_cancelButton, SIGNAL(pushed()), this, SLOT(cancelPressed()));
    }

    if (!m_fileList || !m_locationEdit || !m_backButton || !m_nextButton 
         || !m_prevButton || !m_cancelButton || !m_homeButton)
    {
        cout << "ImportNativeWizard: Your theme is missing some UI elements! Bailing out." << endl;
        QTimer::singleShot(100, this, SLOT(done(int)));
    }

    // load pixmaps
    m_directoryPixmap = gContext->LoadScalePixmap("ma_folder.png");

    buildFocusList();
    assignFirstFocus();
    updateFileList();
}

void ImportNativeWizard::locationEditLostFocus()
{
    m_curDirectory = m_locationEdit->getText();
    updateFileList();
}

void ImportNativeWizard::backPressed()
{
    // move up one directory
    int pos = m_curDirectory.findRev('/');
    if (pos > 0)
        m_curDirectory = m_curDirectory.left(pos);
    else
        m_curDirectory = "/";

    updateFileList();
}

void ImportNativeWizard::homePressed()
{
    char *home = getenv("HOME");
    m_curDirectory = home;

    updateFileList();
}

void ImportNativeWizard::nextPressed()

{
    if (getContext() == 1 && !m_isValidXMLSelected)
    {
        // only move to next page if a valid xml file is selected
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
                                  tr("You need to select a valid archive XML file!"));
        return;
    }

    if (getContext() == LAST_PAGE)
    {
        if (m_localChanID_text->GetText() == "")
        {
            // only move to next page if a valid chanID is selected
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
                                      tr("You need to select a valid chanID!"));
            return;
        }

        QString commandline;
        QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");
        QString chanID = m_localChanID_text->GetText();

        if (tempDir == "")
            return;

        if (!tempDir.endsWith("/"))
            tempDir += "/";

        QString logDir = tempDir + "logs";

        // remove existing progress.log if prescent
        if (QFile::exists(logDir + "/progress.log"))
            QFile::remove(logDir + "/progress.log");


        UIListBtnTypeItem *item = m_fileList->GetItemCurrent();
        FileInfo *fileInfo = (FileInfo*)item->getData();

        QString filename = m_curDirectory;
        if (!filename.endsWith("/"))
                filename += "/";
        filename += fileInfo->filename;

        commandline = "mytharchivehelper -f \"" + filename + "\" " + chanID;
        commandline += " > "  + logDir + "/progress.log 2>&1 &";

        int state = system(commandline);

        if (state != 0) 
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("Myth Archive"),
                                      QObject::tr("It was not possible to import the Archive. "  
                                              " An error occured when running 'mytharchivehelper'") );
            done(Rejected);
            return;
        }

        done(Accepted);
    }
    else
        setContext(getContext() + 1);

    if (m_nextButton)
    {
        if (getContext() == LAST_PAGE)
            m_nextButton->setText(tr("Finish"));
        else
            m_nextButton->setText(tr("Next"));
    }

    updateForeground();
    buildFocusList();
}

void ImportNativeWizard::prevPressed()
{
    if (getContext() > 1)
        setContext(getContext() - 1);

    if (m_nextButton)
        m_nextButton->setText(tr("Next"));

    updateForeground();
    buildFocusList();
}

void ImportNativeWizard::cancelPressed()
{
    reject();
}

void ImportNativeWizard::updateFileList()
{
    if (!m_fileList)
        return;

    m_fileList->Reset();
    m_fileData.clear();
    QDir d;

    d.setPath(m_curDirectory);
    if (d.exists())
    {
        // first get a list of directory's in the current directory
        const QFileInfoList *list = d.entryInfoList("*", QDir::Dirs, QDir::Name);
        QFileInfoListIterator it(*list);
        QFileInfo *fi;

        while ( (fi = it.current()) != 0 )
        {
            if (fi->fileName() != ".")
            {
                FileInfo  *data = new FileInfo; 
                data->selected = false;
                data->directory = true;
                data->filename = fi->fileName();
                data->size = 0;
                m_fileData.append(data);

                // add a row to the UIListBtnArea
                UIListBtnTypeItem* item = new UIListBtnTypeItem(
                        m_fileList, data->filename);
                item->setCheckable(false);
                item->setPixmap(m_directoryPixmap);
                item->setData(data);
            }
            ++it;
        }

        // second get a list of file's in the current directory
        list = d.entryInfoList(m_filemask, QDir::Files, QDir::Name);
        it = QFileInfoListIterator(*list);

        while ( (fi = it.current()) != 0 )
        {
            FileInfo  *data = new FileInfo; 
            data->selected = false;
            data->directory = false;
            data->filename = fi->fileName();
            data->size = fi->size();
            m_fileData.append(data);
            // add a row to the UIListBtnArea
            UIListBtnTypeItem* item = new UIListBtnTypeItem(
                    m_fileList,
                    data->filename + " (" + formatSize(data->size / 1024, 2) + ")");

            item->setCheckable(false);
            item->setData(data);

            ++it;
        }
        m_locationEdit->setText(m_curDirectory);
    }
    else
    {
        m_locationEdit->setText("/");
        cout << "MythArchive:  current directory does not exist!" << endl;
    }

    m_fileList->refresh();
}

void ImportNativeWizard::selectedChanged(UIListBtnTypeItem *item)
{
    m_isValidXMLSelected = false;

    if (!item)
        return;

    FileInfo *f;

    f = (FileInfo *) item->getData();

    if (!f)
        return;

    if (!f->directory && f->filename.endsWith(".xml"))
        loadXML(m_curDirectory + "/" + f->filename);
    else
    {
        m_title_text->SetText("");
        m_subtitle_text->SetText("");
        m_starttime_text->SetText("");
    }
}

void ImportNativeWizard::loadXML(const QString &filename)
{
    QDomDocument doc("mydocument");
    QFile file(filename);
    if (!file.open(IO_ReadOnly))
        return;

    if (!doc.setContent( &file )) 
    {
        file.close();
        return;
    }
    file.close();

    QString docType = doc.doctype().name();

    if (docType == "MYTHARCHIVEITEM")
    {
        QDomNodeList itemNodeList = doc.elementsByTagName("item");
        QString type, dbVersion;

        if (itemNodeList.count() < 1)
        {
            cout << "Couldn't find an 'item' element in XML file" << endl;
            return;
        }

        QDomNode n = itemNodeList.item(0);
        QDomElement e = n.toElement();
        type = e.attribute("type");
        dbVersion = e.attribute("databaseversion");
        if (type == "recording")
        {
            QDomNodeList nodeList = e.elementsByTagName("recorded");
            if (nodeList.count() < 1)
            {
                cout << "Couldn't find a 'recorded' element in XML file" << endl;
                return;
            }

            n = nodeList.item(0);
            e = n.toElement();
            n = e.firstChild();
            while (!n.isNull())
            {
                e = n.toElement();
                if (!e.isNull()) 
                {
                    if (e.tagName() == "title")
                    {
                        m_title_text->SetText(e.text());
                        m_progTitle_text->SetText(e.text());
                    }

                    if (e.tagName() == "subtitle")
                        m_subtitle_text->SetText(e.text());

                    if (e.tagName() == "starttime")
                    {
                        QDateTime startTime = QDateTime::fromString(e.text(), Qt::ISODate);
                        QString dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
                        QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
                        m_starttime_text->SetText(startTime.toString(
                                dateFormat + " " + timeFormat));
                        m_progDateTime_text->SetText(startTime.toString("dd MMM yy (hh:mm)"));
                    }

                    if (e.tagName() == "description")
                    {
                        m_progDescription_text->SetText(
                                (m_subtitle_text->GetText() != "" ? 
                                m_subtitle_text->GetText() + "\n" : "") + e.text());
                    }
                }
                n = n.nextSibling();
            }

            // get channel info
            n = itemNodeList.item(0);
            e = n.toElement();
            nodeList = e.elementsByTagName("channel");
            if (nodeList.count() < 1)
            {
                cout << "Couldn't find a 'channel' element in XML file" << endl;
                m_chanID_text->SetText("");
                m_chanNo_text->SetText("");
                m_chanName_text->SetText("");
                m_callsign_text->SetText("");
                return;
            }

            n = nodeList.item(0);
            e = n.toElement();
            m_chanID_text->SetText(e.attribute("chanid"));
            m_chanNo_text->SetText(e.attribute("channum"));
            m_chanName_text->SetText(e.attribute("name"));
            m_callsign_text->SetText(e.attribute("callsign"));
            findChannelMatch(e.attribute("chanid"), e.attribute("channum"),
                             e.attribute("name"), e.attribute("callsign"));

            m_isValidXMLSelected = true;
        }
        else if (type == "video")
        {
            QDomNodeList nodeList = e.elementsByTagName("videometadata");
            if (nodeList.count() < 1)
            {
                cout << "Couldn't find a 'videometadata' element in XML file" << endl;
                return;
            }

            n = nodeList.item(0);
            e = n.toElement();
            n = e.firstChild();
            while (!n.isNull())
            {
                e = n.toElement();
                if (!e.isNull()) 
                {
                    if (e.tagName() == "title")
                    {
                        m_title_text->SetText(e.text());
                        m_progTitle_text->SetText(e.text());
                        m_subtitle_text->SetText("");
                        m_starttime_text->SetText("");
                        m_progDateTime_text->SetText("");
                    }

                    if (e.tagName() == "plot")
                    {
                        m_progDescription_text->SetText(e.text());
                    }
                }
                n = n.nextSibling();
            }

            m_chanID_text->SetText("N/A");
            m_chanNo_text->SetText("N/A");
            m_chanName_text->SetText("N/A");
            m_callsign_text->SetText("N/A");
            m_localChanID_text->SetText("N/A");
            m_localChanNo_text->SetText("N/A");
            m_localChanName_text->SetText("N/A");
            m_localCallsign_text->SetText("N/A");

            m_isValidXMLSelected = true;
        }
    }
    else
    {
        m_title_text->SetText("");
        m_subtitle_text->SetText("");
        m_starttime_text->SetText("");
        cout << "Not a native archive file" << endl;
    }
}

void ImportNativeWizard::findChannelMatch(const QString &chanID, const QString &chanNo,
                                          const QString &name, const QString &callsign)
{

    // find best match in channel table for this recording

    // look for an exact match - maybe the user is importing back an old recording
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign FROM channel "
            "WHERE chanid = :CHANID AND channum = :CHANNUM AND name = :NAME"
            "AND callsign = :CALLSIGN;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":CHANNUM", chanNo);
    query.bindValue(":NAME", name);
    query.bindValue(":CALLSIGN", callsign);

    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        // got match
        query.first();
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
        return;
    }

    // try to find callsign
    query.prepare("SELECT chanid, channum, name, callsign FROM channel "
            "WHERE callsign = :CALLSIGN;");
    query.bindValue(":CALLSIGN", callsign);

    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        // got match
        query.first();
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
        return;
    }

    // try to find name
    query.prepare("SELECT chanid, channum, name, callsign FROM channel "
            "WHERE name = :NAME;");
    query.bindValue(":NAME", callsign);

    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        // got match
        query.first();
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
        return;
    }

    // give up
    m_localChanID_text->SetText("");
    m_localChanNo_text->SetText("");
    m_localChanName_text->SetText("");
    m_localCallsign_text->SetText("");
}

bool ImportNativeWizard::showList(const QString &caption, QString &value)
{
    bool res = false;

    MythSearchDialog *searchDialog = new MythSearchDialog(gContext->GetMainWindow(), "");
    searchDialog->setCaption(caption);
    searchDialog->setSearchText(value);
    searchDialog->setItems(m_searchList);
    if (searchDialog->ExecPopupAtXY(-1, 8) == 0)
    {
        value = searchDialog->getResult();
        res = true;
    }

    delete searchDialog;
    setActiveWindow();

    return res;
}

void ImportNativeWizard::fillSearchList(const QString &field)
{
    m_searchList.clear();

    QString querystr;
    querystr = QString("SELECT %1 FROM channel ORDER BY %2").arg(field).arg(field);

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(querystr);

    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            m_searchList << QString::fromUtf8(query.value(0).toString());
        }
    }
}

void ImportNativeWizard::searchChanID()
{
    QString s;

    fillSearchList("chanid");

    s = m_chanID_text->GetText();
    if (showList(tr("Select a ChanID"), s))
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid, channum, name, callsign "
                "FROM channel WHERE chanid = :CHANID;");
        query.bindValue(":CHANID", s);
        query.exec();

        if (query.isActive() && query.numRowsAffected())
        {
            query.next();
            m_localChanID_text->SetText(query.value(0).toString());
            m_localChanNo_text->SetText(query.value(1).toString());
            m_localChanName_text->SetText(query.value(2).toString());
            m_localCallsign_text->SetText(query.value(3).toString());
        }
    }
}

void ImportNativeWizard::searchChanNo()
{
    QString s;

    fillSearchList("channum");

    s = m_chanNo_text->GetText();
    if (showList(tr("Select a ChanNo"), s))
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid, channum, name, callsign "
                "FROM channel WHERE channum = :CHANNUM;");
        query.bindValue(":CHANNUM", s);
        query.exec();

        if (query.isActive() && query.numRowsAffected())
        {
            query.next();
            m_localChanID_text->SetText(query.value(0).toString());
            m_localChanNo_text->SetText(query.value(1).toString());
            m_localChanName_text->SetText(query.value(2).toString());
            m_localCallsign_text->SetText(query.value(3).toString());
        }
    }
}

void ImportNativeWizard::searchName()
{
    QString s;

    fillSearchList("name");

    s = m_chanName_text->GetText();
    if (showList(tr("Select a Channel Name"), s))
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid, channum, name, callsign "
                "FROM channel WHERE name = :NAME;");
        query.bindValue(":NAME", s);
        query.exec();

        if (query.isActive() && query.numRowsAffected())
        {
            query.next();
            m_localChanID_text->SetText(query.value(0).toString());
            m_localChanNo_text->SetText(query.value(1).toString());
            m_localChanName_text->SetText(query.value(2).toString());
            m_localCallsign_text->SetText(query.value(3).toString());
        }
    }
}

void ImportNativeWizard::searchCallsign()
{
    QString s;

    fillSearchList("callsign");

    s = m_callsign_text->GetText();
    if (showList(tr("Select a Callsign"), s))
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid, channum, name, callsign "
                "FROM channel WHERE callsign = :CALLSIGN;");
        query.bindValue(":CALLSIGN", s);
        query.exec();

        if (query.isActive() && query.numRowsAffected())
        {
            query.next();
            m_localChanID_text->SetText(query.value(0).toString());
            m_localChanNo_text->SetText(query.value(1).toString());
            m_localChanName_text->SetText(query.value(2).toString());
            m_localCallsign_text->SetText(query.value(3).toString());
        }
    }
}
