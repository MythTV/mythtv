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
#include "fileselector.h"
#include "mytharchivewizard.h"


////////////////////////////////////////////////////////////////

FileSelector::FileSelector(FSTYPE type, const QString &startDir, 
                const QString &filemask, MythMainWindow *parent, 
                const QString &window_name, const QString &theme_filename, 
                const char *name)
                :MythThemedDialog(parent, window_name, theme_filename, name)
{
    m_selectorType = type;
    m_filemask = filemask;
    m_curDirectory = startDir;
    wireUpTheme();
}

FileSelector::~FileSelector()
{
}

QString FileSelector::getSelected(void)
{
    return m_curDirectory;
}

void FileSelector::keyPressEvent(QKeyEvent *e)
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
                FileData *fileData = (FileData*)item->getData();

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
                    if (m_selectorType == FSTYPE_FILELIST)
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

void FileSelector::wireUpTheme()
{
    m_fileList = getUIListBtnType("filelist");

    m_locationEdit = getUIRemoteEditType("location_edit");
    if (m_locationEdit)
    {
        m_locationEdit->createEdit(this);
        connect(m_locationEdit, SIGNAL(loosingFocus()), 
                this, SLOT(locationEditLostFocus()));
    }

    // ok button
    m_okButton = getUITextButtonType("ok_button");
    if (m_okButton)
    {
        m_okButton->setText(tr("OK"));
        connect(m_okButton, SIGNAL(pushed()), this, SLOT(OKPressed()));
    }

    // cancel button
    m_cancelButton = getUITextButtonType("cancel_button");
    if (m_cancelButton)
    {
        m_cancelButton->setText(tr("Cancel"));
        connect(m_cancelButton, SIGNAL(pushed()), this, SLOT(cancelPressed()));
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

    if (!m_fileList || !m_locationEdit || !m_backButton || !m_okButton 
         || !m_cancelButton || !m_homeButton)
    {
        cout << "FileSelector: Your theme is missing some UI elements! Bailing out." << endl;
        QTimer::singleShot(100, this, SLOT(done(int)));
    }

    // load pixmaps
    m_directoryPixmap = gContext->LoadScalePixmap("ma_folder.png");

    buildFocusList();
    assignFirstFocus();
    updateSelectedList();
    updateFileList();
}

void FileSelector::locationEditLostFocus()
{
    m_curDirectory = m_locationEdit->getText();
    updateFileList();
}

void FileSelector::backPressed()
{
    // move up one directory
    int pos = m_curDirectory.findRev('/');
    if (pos > 0)
        m_curDirectory = m_curDirectory.left(pos);
    else
        m_curDirectory = "/";

    updateFileList();
}

void FileSelector::homePressed()
{
    char *home = getenv("HOME");
    m_curDirectory = home;

    updateFileList();
}

void FileSelector::OKPressed()
{
    if (m_selectorType == FSTYPE_FILELIST)
    {
        if (m_selectedList.count() == 0)
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
                                    tr("You need to select at least one file!"));
            return;
        }

        // loop though selected files and add them to the archiveitems table
        QString s;
        QStringList::iterator it;
        for (it = m_selectedList.begin(); it != m_selectedList.end(); ++it)
        {
            s = (*it);

            //check this file is not already in the archiveitems table
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT * FROM archiveitems WHERE filename = :FILENAME");
            query.bindValue(":FILENAME", s);
            query.exec();
            if (query.isActive() && query.numRowsAffected())
            {
                //already there
            }
            else
            {
                QFile file(s);
                if (file.exists())
                {
                    QString title = s;
                    int pos = s.findRev('/');
                    if (pos > 0)
                        title = s.mid(pos + 1);

                    query.prepare("INSERT INTO archiveitems (type, title, subtitle,"
                            "description, startdate, starttime, size, filename, hascutlist) "
                            "VALUES(:TYPE, :TITLE, :SUBTITLE, :DESCRIPTION, :STARTDATE, "
                            ":STARTTIME, :SIZE, :FILENAME, :HASCUTLIST);");
                    query.bindValue(":TYPE", "File");
                    query.bindValue(":TITLE", title);
                    query.bindValue(":SUBTITLE", "");
                    query.bindValue(":DESCRIPTION", "");
                    query.bindValue(":STARTDATE", "");
                    query.bindValue(":STARTTIME", "");
                    query.bindValue(":SIZE", (long long)file.size());
                    query.bindValue(":FILENAME", s);
                    query.bindValue(":HASCUTLIST", 0);
                    if (!query.exec())
                        MythContext::DBError("archive item insert", query);
                }
            }
        }
    }
    else
    {
        UIListBtnTypeItem *item = m_fileList->GetItemCurrent();
        FileData *fileData = (FileData*)item->getData();

        if (m_selectorType == FSTYPE_DIRECTORY)
        {
            if (!fileData->directory)
            {
                MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
                                          tr("The selected item is not a directory!"));
                return;
            }

            if (fileData->filename != "..")
            {
                if (!m_curDirectory.endsWith("/"))
                    m_curDirectory += "/";
                m_curDirectory += fileData->filename;
            }
        }
        else
        {
            if (fileData->directory)
            {
                if (!m_curDirectory.endsWith("/"))
                    m_curDirectory += "/";
            }
            else
            {
                if (!m_curDirectory.endsWith("/"))
                    m_curDirectory += "/";
                m_curDirectory += fileData->filename;
            }
        }
    }

    done(Accepted);
}

void FileSelector::cancelPressed()
{
    reject();
}

void FileSelector::updateSelectedList()
{
    m_selectedList.clear();
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT filename FROM archiveitems WHERE type = 'File'");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            QString filename = QString::fromUtf8(query.value(0).toString());
            if (m_selectedList.findIndex(filename) == -1)
                m_selectedList.append(filename);
        }
    }
}

void FileSelector::updateFileList()
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
                FileData  *data = new FileData; 
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

        if (m_selectorType != FSTYPE_DIRECTORY)
        {
            // second get a list of file's in the current directory
            list = d.entryInfoList(m_filemask, QDir::Files, QDir::Name);
            it = QFileInfoListIterator(*list);

            while ( (fi = it.current()) != 0 )
            {
                FileData  *data = new FileData; 
                data->selected = false;
                data->directory = false;
                data->filename = fi->fileName();
                data->size = fi->size();
                m_fileData.append(data);
                                // add a row to the UIListBtnArea
                UIListBtnTypeItem* item = new UIListBtnTypeItem(
                        m_fileList,
                        data->filename + " (" + formatSize(data->size / 1024, 2) + ")");

                if (m_selectorType == FSTYPE_FILELIST)
                {
                    item->setCheckable(true);

                    //item->setPixmap(m_filePixmap);

                    QString fullPath = m_curDirectory;
                    if (!fullPath.endsWith("/"))
                        fullPath += "/";
                    fullPath += data->filename;

                    if (m_selectedList.findIndex(fullPath) != -1)
                    {
                        item->setChecked(UIListBtnTypeItem::FullChecked);
                    }
                    else 
                    {
                        item->setChecked(UIListBtnTypeItem::NotChecked);
                    }
                }
                else
                    item->setCheckable(false);

                item->setData(data);

                ++it;
            }
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
