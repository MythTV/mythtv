#include <cstdlib>

// Qt
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QVariant>

// Myth
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/stringutil.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// mytharchive
#include "archiveutil.h"
#include "fileselector.h"

////////////////////////////////////////////////////////////////

FileSelector::~FileSelector()
{
    while (!m_fileData.isEmpty())
        delete m_fileData.takeFirst();
}

bool FileSelector::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "file_selector", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilW::Assign(this, m_titleText, "title_text");
    UIUtilE::Assign(this, m_fileButtonList, "filelist", &err);
    UIUtilE::Assign(this, m_locationEdit, "location_edit", &err);
    UIUtilE::Assign(this, m_backButton, "back_button", &err);
    UIUtilE::Assign(this, m_homeButton, "home_button", &err);
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'file_selector'");
        return false;
    }

    if (m_titleText)
    {
        switch (m_selectorType)
        {
            case FSTYPE_FILE:
                m_titleText->SetText(tr("Find File"));
                break;
            case FSTYPE_DIRECTORY:
                m_titleText->SetText(tr("Find Directory"));
                break;
            default:
                m_titleText->SetText(tr("Find Files"));
                break;
        }
    }

    connect(m_okButton, &MythUIButton::Clicked, this, &FileSelector::OKPressed);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &FileSelector::cancelPressed);

    connect(m_locationEdit, &MythUIType::LosingFocus,
            this, &FileSelector::locationEditLostFocus);
    m_locationEdit->SetText(m_curDirectory);

    connect(m_backButton, &MythUIButton::Clicked, this, &FileSelector::backPressed);
    connect(m_homeButton, &MythUIButton::Clicked, this, &FileSelector::homePressed);

    connect(m_fileButtonList, &MythUIButtonList::itemClicked,
            this, &FileSelector::itemClicked);

    BuildFocusList();

    SetFocusWidget(m_fileButtonList);

    updateSelectedList();
    updateFileList();

    return true;
}

bool FileSelector::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
        {

        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void FileSelector::itemClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *fileData = item->GetData().value<FileData*>();
    if (!fileData)
        return;

    if (fileData->directory)
    {
        if (fileData->filename == "..")
        {
            // move up on directory
            int pos = m_curDirectory.lastIndexOf('/');
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

            if (item->state() == MythUIButtonListItem::FullChecked)
            {
                m_selectedList.removeAll(fullPath);
                item->setChecked(MythUIButtonListItem::NotChecked);
            }
            else
            {
                if (m_selectedList.indexOf(fullPath) == -1)
                    m_selectedList.append(fullPath);
                item->setChecked(MythUIButtonListItem::FullChecked);
            }
        }
    }
}

QString FileSelector::getSelected(void)
{
    return m_curDirectory;
}

void FileSelector::locationEditLostFocus()
{
    m_curDirectory = m_locationEdit->GetText();
    updateFileList();
}

void FileSelector::backPressed()
{
    // move up one directory
    int pos = m_curDirectory.lastIndexOf('/');
    if (pos > 0)
        m_curDirectory = m_curDirectory.left(pos);
    else
        m_curDirectory = "/";

    updateFileList();
}

void FileSelector::homePressed()
{
    m_curDirectory = qEnvironmentVariable("HOME");

    updateFileList();
}

void FileSelector::OKPressed()
{
    if (m_selectorType == FSTYPE_FILELIST && m_archiveList)
    {
        // loop though selected files and add them to the list

        // remove any items that have been removed from the list
        QList<ArchiveItem *> tempAList;
        for (auto *a : std::as_const(*m_archiveList))
        {
            if (a->type != "File")
                continue;
            if (std::none_of(m_selectedList.cbegin(), m_selectedList.cend(),
                             [a](const auto & f)
                                 {return a->filename == f; } ))
                tempAList.append(a);
        }

        for (auto *x : std::as_const(tempAList))
            m_archiveList->removeAll(x);

        // remove any items that are already in the list
        QStringList tempSList;
        for (const QString & f : std::as_const(m_selectedList))
        {
            auto namematch = [f](const auto *a){ return a->filename == f; };
            if (std::any_of(m_archiveList->cbegin(), m_archiveList->cend(), namematch))
                tempSList.append(f);
        }

        for (const auto & name : std::as_const(tempSList))
            m_selectedList.removeAll(name);

        // add all that are left
        for (const auto & f : std::as_const(m_selectedList))
        {
            QFile file(f);
            if (file.exists())
            {
                QString title = f;
                int pos = f.lastIndexOf('/');
                if (pos > 0)
                    title = f.mid(pos + 1);

                auto *a = new ArchiveItem;
                a->type = "File";
                a->title = title;
                a->subtitle = "";
                a->description = "";
                a->startDate = "";
                a->startTime = "";
                a->size = (int64_t)file.size();
                a->filename = f;
                a->hasCutlist = false;
                a->useCutlist = false;
                a->duration = 0;
                a->cutDuration = 0;
                a->videoWidth = 0;
                a->videoHeight = 0;
                a->fileCodec = "";
                a->videoCodec = "";
                a->encoderProfile = nullptr;
                a->editedDetails = false;
                m_archiveList->append(a);
            }
        }
    }
    else
    {
        MythUIButtonListItem *item = m_fileButtonList->GetItemCurrent();
        auto *fileData = item->GetData().value<FileData*>();

        if (m_selectorType == FSTYPE_DIRECTORY)
        {
            if (!fileData->directory)
            {
                ShowOkPopup(tr("The selected item is not a directory!"));
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

    if (m_selectorType == FSTYPE_FILELIST)
        emit haveResult(true);
    else
        emit haveResult(getSelected());
    Close();
}

void FileSelector::cancelPressed()
{
    if (m_selectorType == FSTYPE_FILELIST)
        emit haveResult(true);
    else
        emit haveResult(QString());
    Close();
}

void FileSelector::updateSelectedList()
{
    if (!m_archiveList)
        return;

    while (!m_selectedList.isEmpty())
        m_selectedList.takeFirst();
    m_selectedList.clear();

    for (const auto *a : std::as_const(*m_archiveList))
    {
        auto samename = [a](const auto *f)
            { return f->filename == a->filename; };
        auto f = std::find_if(m_fileData.cbegin(), m_fileData.cend(), samename);
        if (f != m_fileData.cend())
        {
            if (m_selectedList.indexOf((*f)->filename) == -1)
                m_selectedList.append((*f)->filename);
        }
    }
}

void FileSelector::updateFileList()
{
    if (!m_fileButtonList)
        return;

    m_fileButtonList->Reset();
    while (!m_fileData.isEmpty())
         delete m_fileData.takeFirst();
    m_fileData.clear();

    QDir d;

    d.setPath(m_curDirectory);
    if (d.exists())
    {
        // first get a list of directory's in the current directory
        QStringList filters;
        filters << "*";
        QFileInfoList list = d.entryInfoList(filters, QDir::Dirs, QDir::Name);

        for (const auto & fi : std::as_const(list))
        {
            if (fi.fileName() != ".")
            {
                auto  *data = new FileData;
                data->selected = false;
                data->directory = true;
                data->filename = fi.fileName();
                data->size = 0;
                m_fileData.append(data);

                // add a row to the MythUIButtonList
                auto* item = new
                        MythUIButtonListItem(m_fileButtonList, data->filename);
                item->setCheckable(false);
                item->SetImage("ma_folder.png");
                item->SetData(QVariant::fromValue(data));
            }
        }

        if (m_selectorType != FSTYPE_DIRECTORY)
        {
            // second get a list of file's in the current directory
            filters.clear();
            filters = m_filemask.split(" ", Qt::SkipEmptyParts);
            list = d.entryInfoList(filters, QDir::Files, QDir::Name);
            for (const auto & fi : std::as_const(list))
            {
                auto  *data = new FileData;
                data->selected = false;
                data->directory = false;
                data->filename = fi.fileName();
                data->size = fi.size();
                m_fileData.append(data);

                // add a row to the UIListBtnArea
                auto* item = 
                     new MythUIButtonListItem(m_fileButtonList, data->filename);
                item->SetText(StringUtil::formatKBytes(data->size / 1024, 2), "size");

                if (m_selectorType == FSTYPE_FILELIST)
                {
                    item->setCheckable(true);

                    QString fullPath = m_curDirectory;
                    if (!fullPath.endsWith("/"))
                        fullPath += "/";
                    fullPath += data->filename;

                    if (m_selectedList.indexOf(fullPath) != -1)
                    {
                        item->setChecked(MythUIButtonListItem::FullChecked);
                    }
                    else
                    {
                        item->setChecked(MythUIButtonListItem::NotChecked);
                    }
                }
                else
                {
                    item->setCheckable(false);
                }

                item->SetData(QVariant::fromValue(data));
            }
        }
        m_locationEdit->SetText(m_curDirectory);
    }
    else
    {
        m_locationEdit->SetText("/");
        LOG(VB_GENERAL, LOG_ERR,
            "MythArchive:  current directory does not exist!");
    }
}
