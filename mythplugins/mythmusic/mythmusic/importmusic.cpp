// qt
#include <QApplication>
#include <QDir>
#include <QFontMetrics>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/remotefile.h>
#include <libmythbase/storagegroup.h>
#include <libmythmetadata/metaio.h>
#include <libmythmetadata/musicmetadata.h>
#include <libmythmetadata/musicutils.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuifilebrowser.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// mythmusic
#include "editmetadata.h"
#include "genres.h"
#include "importmusic.h"
#include "musicplayer.h"


///////////////////////////////////////////////////////////////////////////////

FileScannerThread::FileScannerThread(ImportMusicDialog *parent) :
    MThread("FileScanner"), m_parent(parent)
{
}

void FileScannerThread::run()
{
    RunProlog();
    m_parent->doScan();
    RunEpilog();
}

///////////////////////////////////////////////////////////////////////////////

void FileCopyThread::run()
{
    RunProlog();
    m_result = RemoteFile::CopyFile(m_srcFile, m_dstFile, true);
    RunEpilog();
}

///////////////////////////////////////////////////////////////////////////////


ImportMusicDialog::ImportMusicDialog(MythScreenStack *parent) :
    MythScreenType(parent, "musicimportfiles"),
    m_tracks(new std::vector<TrackInfo*>)
{
    QString lastHost = gCoreContext->GetSetting("MythMusicLastImportHost", gCoreContext->GetMasterHostName());
    QStringList dirs = StorageGroup::getGroupDirs("Music", lastHost);
    if (dirs.count() > 0)
        m_musicStorageDir = StorageGroup::getGroupDirs("Music", lastHost).at(0);
}

ImportMusicDialog::~ImportMusicDialog()
{
    if (gPlayer->getCurrentMetadata() && m_playingMetaData)
    {
        if (gPlayer->isPlaying() && gPlayer->getCurrentMetadata()->Filename() == m_playingMetaData->Filename())
            gPlayer->stop(true);
    }

    if (m_locationEdit)
        gCoreContext->SaveSetting("MythMusicLastImportDir", m_locationEdit->GetText());

    delete m_tracks;

    if (m_somethingWasImported)
        emit importFinished();
}

void ImportMusicDialog::fillWidgets()
{
    if (!m_tracks->empty())
    {
        // update current
        //: %1 is the current track,
        //: %2 is the number of tracks
        m_currentText->SetText(tr("%1 of %2", "Current track position")
                .arg(m_currentTrack + 1).arg(m_tracks->size()));

        MusicMetadata *meta = m_tracks->at(m_currentTrack)->metadata;
        m_filenameText->SetText(meta->Filename());
        m_compilationCheck->SetCheckState(meta->Compilation());
        m_compartistText->SetText(meta->CompilationArtist());
        m_artistText->SetText(meta->Artist());
        m_albumText->SetText(meta->Album());
        m_titleText->SetText(meta->Title());
        m_genreText->SetText(meta->Genre());
        m_yearText->SetText(QString::number(meta->Year()));
        m_trackText->SetText(QString::number(meta->Track()));
        if (m_tracks->at(m_currentTrack)->isNewTune)
        {
            m_coverartButton->SetVisible(false);
            m_statusText->SetText(tr("New File"));
        }
        else
        {
            m_coverartButton->SetVisible(true);
            m_statusText->SetText(tr("Already in Database"));
        }
    }
    else
    {
        // update current
        m_currentText->SetText(tr("Not found"));
        m_filenameText->Reset();
        m_compilationCheck->SetCheckState(false);
        m_compartistText->Reset();
        m_artistText->Reset();
        m_albumText->Reset();
        m_titleText->Reset();
        m_genreText->Reset();
        m_yearText->Reset();
        m_trackText->Reset();
        m_statusText->Reset();
        m_coverartButton->SetVisible(false);
    }
}

bool ImportMusicDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            m_prevButton->Push();
        }
        else if (action == "RIGHT")
        {
            m_nextButton->Push();
        }
        else if (action == "EDIT")
        {
            showEditMetadataDialog();
        }
        else if (action == "MENU")
        {
            ShowMenu();
        }
        else if (action == "ESCAPE" && !GetMythMainWindow()->IsExitingToMain())
        {
            bool found = false;
            if (!m_tracks->empty())
            {
                uint track = 0;
                while (track < m_tracks->size())
                {
                    if (m_tracks->at(track)->isNewTune)
                    {
                        found = true;
                        break;
                    }
                    track++;
                }

                if (found)
                {
                    QString msg = tr("You might have unsaved changes.\nAre you sure you want to exit this screen?");
                    ShowOkPopup(msg, this, &ImportMusicDialog::doExit, true);
                }
            }

            handled = found;
        }
        else if (action == "1")
        {
            setCompilation();
        }
        else if (action == "2")
        {
            setCompilationArtist();
        }
        else if (action == "3")
        {
            setArtist();
        }
        else if (action == "4")
        {
            setAlbum();
        }
        else if (action == "5")
        {
            setGenre();
        }
        else if (action == "6")
        {
            setYear();
        }
        else if (action == "7")
        {
            setRating();
        }
        else if (action == "8")
        {
            setTitleWordCaps();
        }
        else if (action == "9")
        {
            setTitleInitialCap();
        }
        else if (action == "0")
        {
            setTrack();
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

bool ImportMusicDialog::Create()
{
    if (!LoadWindowFromXML("music-ui.xml", "import_music", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_locationEdit,    "location", &err);
    UIUtilE::Assign(this, m_locationButton,  "directoryfinder", &err);
    UIUtilE::Assign(this, m_scanButton,      "scan", &err);
    UIUtilE::Assign(this, m_coverartButton,  "coverart", &err);
    UIUtilE::Assign(this, m_filenameText,    "filename", &err);
    UIUtilE::Assign(this, m_compartistText,  "compartist", &err);
    UIUtilE::Assign(this, m_artistText,      "artist", &err);
    UIUtilE::Assign(this, m_albumText,       "album", &err);
    UIUtilE::Assign(this, m_titleText,       "title", &err);
    UIUtilE::Assign(this, m_genreText,       "genre", &err);
    UIUtilE::Assign(this, m_yearText,        "year", &err);
    UIUtilE::Assign(this, m_trackText,       "track", &err);
    UIUtilE::Assign(this, m_currentText,     "position", &err);
    UIUtilE::Assign(this, m_statusText,      "status", &err);
    UIUtilE::Assign(this, m_compilationCheck,"compilation", &err);
    UIUtilE::Assign(this, m_playButton,      "play", &err);
    UIUtilE::Assign(this, m_nextnewButton,   "nextnew", &err);
    UIUtilE::Assign(this, m_addButton,       "add", &err);
    UIUtilE::Assign(this, m_addallnewButton, "addallnew", &err);
    UIUtilE::Assign(this, m_nextButton,      "next", &err);
    UIUtilE::Assign(this, m_prevButton,      "prev", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'import_music'");
        return false;
    }

    connect(m_prevButton, &MythUIButton::Clicked, this, &ImportMusicDialog::prevPressed);
    connect(m_locationButton, &MythUIButton::Clicked, this, &ImportMusicDialog::locationPressed);
    connect(m_scanButton, &MythUIButton::Clicked, this, &ImportMusicDialog::startScan);
    connect(m_coverartButton, &MythUIButton::Clicked, this, &ImportMusicDialog::coverArtPressed);
    connect(m_playButton, &MythUIButton::Clicked, this, &ImportMusicDialog::playPressed);
    connect(m_nextnewButton, &MythUIButton::Clicked, this, &ImportMusicDialog::nextNewPressed);
    connect(m_addButton, &MythUIButton::Clicked, this, &ImportMusicDialog::addPressed);
    connect(m_addallnewButton, &MythUIButton::Clicked, this, &ImportMusicDialog::addAllNewPressed);
    connect(m_nextButton, &MythUIButton::Clicked, this, &ImportMusicDialog::nextPressed);

    fillWidgets();

    BuildFocusList();

    m_locationEdit->SetText(gCoreContext->GetSetting("MythMusicLastImportDir", "/"));

    return true;
}

void ImportMusicDialog::doExit(bool ok)
{
    if (ok)
        Close();
}

void ImportMusicDialog::locationPressed()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *fb = new MythUIFileBrowser(popupStack, m_locationEdit->GetText());
    // TODO Install a name filter on supported music formats
    fb->SetTypeFilter(QDir::AllDirs | QDir::Readable);
    if (fb->Create())
    {
        fb->SetReturnEvent(this, "locationchange");
        popupStack->AddScreen(fb);
    }
    else
    {
        delete fb;
    }
}

void ImportMusicDialog::coverArtPressed()
{
    showImportCoverArtDialog();
}

void ImportMusicDialog::playPressed()
{
    if (m_tracks->empty())
        return;

    m_playingMetaData = m_tracks->at(m_currentTrack)->metadata;

    gPlayer->playFile(*m_playingMetaData);
}

void ImportMusicDialog::prevPressed()
{
    if (m_currentTrack > 0)
    {
        m_currentTrack--;
        fillWidgets();
    }
}

void ImportMusicDialog::nextPressed()
{
    if (m_currentTrack < (int) m_tracks->size() - 1)
    {
        m_currentTrack++;
        fillWidgets();
    }
}

void ImportMusicDialog::addPressed()
{
    if (m_tracks->empty())
        return;

    MusicMetadata *meta = m_tracks->at(m_currentTrack)->metadata;

    // is the current track a new file?
    if (m_tracks->at(m_currentTrack)->isNewTune)
    {
        // do we need to update the tags?
        if (m_tracks->at(m_currentTrack)->metadataHasChanged)
        {
            MetaIO *tagger = MetaIO::createTagger(meta->Filename());
            if (tagger)
            {
                tagger->write(meta->Filename(), meta);
                delete tagger;
            }
        }

        // get the save filename
        QString origFilename = meta->Filename();
        QString saveFilename = filenameFromMetadata(meta);
        QString fullFilename;

        QUrl url(m_musicStorageDir);
        fullFilename = MythCoreContext::GenMythURL(url.host(), 0, saveFilename, "Music");


        // we need to manually copy the file extension
        QFileInfo fi(origFilename);
        saveFilename += "." + fi.suffix();
        fullFilename += "." + fi.suffix();

        LOG(VB_FILE, LOG_INFO, QString("Copying file from: %1").arg(origFilename));
        LOG(VB_FILE, LOG_INFO, QString("to: ").arg(fullFilename));

        // copy the file to the new location
        if (!copyFile(origFilename, fullFilename))
        {
            ShowOkPopup(tr("Copy Failed\nCould not copy file to: %1").arg(fullFilename));
            return;
        }

        meta->setFilename(saveFilename);
        meta->setHostname(url.host());
        meta->setFileSize((quint64)QFileInfo(origFilename).size());

        // update the database
        meta->dumpToDatabase();

        // find any albumart for this track
        meta->getAlbumArtImages()->scanForImages();
        meta->getAlbumArtImages()->dumpToDatabase();

        m_somethingWasImported = true;

        m_tracks->at(m_currentTrack)->isNewTune =
                isNewTune(meta->Artist(), meta->Album(), meta->Title());

        // update the UI
        fillWidgets();
    }
    else
    {
        ShowOkPopup(tr("This track is already in the database"));
    }
}

void ImportMusicDialog::addAllNewPressed()
{
    if (m_tracks->empty())
        return;

    m_currentTrack = 0;
    int newCount = 0;

    while (m_currentTrack < (int) m_tracks->size())
    {
        fillWidgets();
        QCoreApplication::processEvents();

        if (m_tracks->at(m_currentTrack)->isNewTune)
        {
            addPressed();
            newCount++;
        }

        QCoreApplication::processEvents();

        m_currentTrack++;
    }

    m_currentTrack--;

    ShowOkPopup(tr("%1 new tracks were added to the database").arg(newCount));
}

void ImportMusicDialog::nextNewPressed()
{
    if (m_tracks->empty())
        return;

    uint track = m_currentTrack + 1;
    while (track < m_tracks->size())
    {
        if (m_tracks->at(track)->isNewTune)
        {
            m_currentTrack = track;
            fillWidgets();
            break;
        }
        track++;
    }
}

bool ImportMusicDialog::copyFile(const QString &src, const QString &dst)
{
    bool res = false;
    QString host = QUrl(dst).host();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *busy =
            new MythUIBusyDialog(tr("Copying music file to the 'Music' storage group on %1").arg(host),
                                    popupStack,
                                    "scanbusydialog");

    if (busy->Create())
    {
        popupStack->AddScreen(busy, false);
    }
    else
    {
        delete busy;
        busy = nullptr;
    }

    auto *copy = new FileCopyThread(src, dst);
    copy->start();

    while (!copy->isFinished())
    {
        const struct timespec halfms {0, 500000};
        nanosleep(&halfms, nullptr);
        QCoreApplication::processEvents();
    }

    res = copy->GetResult();

    delete copy;

    if (busy)
        busy->Close();

    return res;
}

void ImportMusicDialog::startScan()
{
    // sanity check - make sure the user isn't trying to import tracks from the music directory
    QString location = m_locationEdit->GetText();
    if (!location.endsWith('/'))
        location.append('/');

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *busy = new MythUIBusyDialog(tr("Searching for music files"),
                                      popupStack, "scanbusydialog");

    if (busy->Create())
    {
        popupStack->AddScreen(busy, false);
    }
    else
    {
        delete busy;
        busy = nullptr;
    }
    auto *scanner = new FileScannerThread(this);
    scanner->start();

    while (!scanner->isFinished())
    {
        const struct timespec halfms {0, 500000};
        nanosleep(&halfms, nullptr);
        QCoreApplication::processEvents();
    }

    delete scanner;

    m_currentTrack = 0;
    fillWidgets();

    if (busy)
        busy->Close();
}

void ImportMusicDialog::doScan()
{
    m_tracks->clear();
    m_sourceFiles.clear();
    QString location = m_locationEdit->GetText();
    scanDirectory(location, m_tracks);
}

void ImportMusicDialog::scanDirectory(QString &directory, std::vector<TrackInfo*> *tracks)
{
    QDir d(directory);

    if (!d.exists())
        return;

    d.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    const QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    while (it != list.end())
    {
        const QFileInfo *fi = &(*it);
        ++it;
        QString filename = fi->absoluteFilePath();
        if (fi->isDir())
            scanDirectory(filename, tracks);
        else
        {
            MetaIO *tagger = MetaIO::createTagger(filename);
            if (tagger)
            {
                MusicMetadata *metadata = tagger->read(filename);
                if (metadata)
                {
                    auto * track = new TrackInfo;
                    track->metadata = metadata;
                    track->isNewTune = isNewTune(metadata->Artist(), metadata->Album(),
                                                 metadata->Title());
                    track->metadataHasChanged = false;
                    tracks->push_back(track);
                    m_sourceFiles.append(filename);
                }

                delete tagger;
            }
        }
    }
}

void ImportMusicDialog::showEditMetadataDialog()
{
    if (m_tracks->empty())
        return;

    MusicMetadata *editMeta = m_tracks->at(m_currentTrack)->metadata;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *editDialog = new EditMetadataDialog(mainStack, editMeta);

    if (!editDialog->Create())
    {
        delete editDialog;
        return;
    }

    editDialog->setSaveMetadataOnly();

    connect(editDialog, &EditMetadataCommon::metadataChanged, this, &ImportMusicDialog::metadataChanged);

    mainStack->AddScreen(editDialog);
}

void ImportMusicDialog::metadataChanged(void)
{
    MusicMetadata *editMeta = m_tracks->at(m_currentTrack)->metadata;
    m_tracks->at(m_currentTrack)->metadataHasChanged = true;
    m_tracks->at(m_currentTrack)->isNewTune =
            isNewTune(editMeta->Artist(), editMeta->Album(), editMeta->Title());
    fillWidgets();
}

void ImportMusicDialog::ShowMenu()
{
    if (m_tracks->empty())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox("", popupStack, "importmusicmenu");

    if (menu->Create())
        popupStack->AddScreen(menu);
    else
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "menu");
    menu->AddButton(tr("Select Where To Save Tracks"), &ImportMusicDialog::chooseBackend);
    menu->AddButton(tr("Save Defaults"), &ImportMusicDialog::saveDefaults);

    if (m_haveDefaults)
    {
        menu->AddButton(tr("Change Compilation Flag"), &ImportMusicDialog::setCompilation);
        menu->AddButton(tr("Change Compilation Artist"),
                                &ImportMusicDialog::setCompilationArtist);
        menu->AddButton(tr("Change Artist"), &ImportMusicDialog::setArtist);
        menu->AddButton(tr("Change Album"), &ImportMusicDialog::setAlbum);
        menu->AddButton(tr("Change Genre"), &ImportMusicDialog::setGenre);
        menu->AddButton(tr("Change Year"), &ImportMusicDialog::setYear);
        menu->AddButton(tr("Change Rating"), &ImportMusicDialog::setRating);
    }
}

void ImportMusicDialog::chooseBackend(void) const
{
    QStringList hostList;

    // get a list of hosts with a directory defined for the 'Music' storage group
    MSqlQuery query(MSqlQuery::InitCon());
    QString sql = "SELECT DISTINCT hostname "
                  "FROM storagegroup "
                  "WHERE groupname = 'Music'";
    if (!query.exec(sql) || !query.isActive())
        MythDB::DBError("ImportMusicDialog::chooseBackend get host list", query);
    else
    {
        while(query.next())
        {
            hostList.append(query.value(0).toString());
        }
    }

    if (hostList.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "ImportMusicDialog::chooseBackend: No backends found");
        return;
    }

    QString msg = tr("Select where to save tracks");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, hostList, false, "");

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &ImportMusicDialog::setSaveHost);

    popupStack->AddScreen(searchDlg);
}

void ImportMusicDialog::setSaveHost(const QString& host)
{
    gCoreContext->SaveSetting("MythMusicLastImportHost", host);

    QStringList dirs = StorageGroup::getGroupDirs("Music", host);
    if (dirs.count() > 0)
        m_musicStorageDir = StorageGroup::getGroupDirs("Music", host).at(0);

}

void ImportMusicDialog::saveDefaults(void)
{
    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    m_defaultCompilation = data->Compilation();
    m_defaultCompArtist = data->CompilationArtist();
    m_defaultArtist = data->Artist();
    m_defaultAlbum = data->Album();
    m_defaultGenre = data->Genre();
    m_defaultYear = data->Year();
    m_defaultRating = data->Rating();

    m_haveDefaults = true;
}

void ImportMusicDialog::setCompilation(void)
{
    if (!m_haveDefaults)
        return;

    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;

    if (m_defaultCompilation)
    {
        data->setCompilation(m_defaultCompilation);
        data->setCompilationArtist(m_defaultCompArtist);
    }
    else
    {
        data->setCompilation(m_defaultCompilation);
        data->setCompilationArtist(m_defaultArtist);
    }

    fillWidgets();
}

void ImportMusicDialog::setCompilationArtist(void)
{
    if (!m_haveDefaults)
        return;

    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setCompilationArtist(m_defaultCompArtist);

    fillWidgets();
}

void ImportMusicDialog::setArtist(void)
{
    if (!m_haveDefaults)
        return;

    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setArtist(m_defaultArtist);

    m_tracks->at(m_currentTrack)->isNewTune =
            isNewTune(data->Artist(), data->Album(), data->Title());

    fillWidgets();
}

void ImportMusicDialog::setAlbum(void)
{
    if (!m_haveDefaults)
        return;

    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setAlbum(m_defaultAlbum);

    m_tracks->at(m_currentTrack)->isNewTune =
            isNewTune(data->Artist(), data->Album(), data->Title());

    fillWidgets();
}

void ImportMusicDialog::setYear(void)
{
    if (!m_haveDefaults)
        return;

    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setYear(m_defaultYear);

    fillWidgets();
}

void ImportMusicDialog::setTrack(void)
{
    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setTrack(data->Track() + 100);

    fillWidgets();
}

void ImportMusicDialog::setGenre(void)
{
    if (!m_haveDefaults)
        return;

    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setGenre(m_defaultGenre);

    fillWidgets();
}

void ImportMusicDialog::setRating(void)
{
    if (!m_haveDefaults)
        return;

    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setRating(m_defaultRating);
}

void ImportMusicDialog::setTitleInitialCap(void)
{
    QLocale locale = gCoreContext->GetQLocale();
    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    QString title = locale.toLower(data->Title().simplified());
    title[0] = title[0].toUpper();
    data->setTitle(title);
    fillWidgets();
}

void ImportMusicDialog::setTitleWordCaps(void)
{
    QLocale locale = gCoreContext->GetQLocale();
    MusicMetadata *data = m_tracks->at(m_currentTrack)->metadata;
    QString title = locale.toLower(data->Title().simplified());
    QStringList title_words = title.split(' ');

    // NOLINTNEXTLINE(modernize-loop-convert)
    for (int x = 0; x < title_words.size(); ++x)
        title_words[x][0] = title_words[x][0].toUpper();

    data->setTitle(title_words.join(' '));
    fillWidgets();
}

void ImportMusicDialog::showImportCoverArtDialog(void)
{
    if (m_tracks->empty())
        return;

    QFileInfo fi(m_sourceFiles.at(m_currentTrack));

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *import = new ImportCoverArtDialog(mainStack,
                                        fi.absolutePath(),
                                        m_tracks->at(m_currentTrack)->metadata,
                                        m_musicStorageDir);

    if (import->Create())
        mainStack->AddScreen(import);
    else
        delete import;
}

void ImportMusicDialog::customEvent(QEvent *event)
{
    if (auto *dce = dynamic_cast<DialogCompletionEvent*>(event))
    {
        if (dce->GetId() == "locationchange")
        {
            m_locationEdit->SetText(dce->GetResultText());
            startScan();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

bool ImportCoverArtDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            m_prevButton->Push();
        }
        else if (action == "RIGHT")
        {
            m_nextButton->Push();
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

bool ImportCoverArtDialog::Create()
{
    if (!LoadWindowFromXML("music-ui.xml", "import_coverart", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_filenameText,    "file", &err);
    UIUtilE::Assign(this, m_currentText,     "position", &err);
    UIUtilE::Assign(this, m_statusText,      "status", &err);
    UIUtilE::Assign(this, m_destinationText, "destination", &err);
    UIUtilE::Assign(this, m_coverartImage,   "coverart", &err);
    UIUtilE::Assign(this, m_copyButton,      "copy", &err);
    UIUtilE::Assign(this, m_exitButton,      "exit", &err);
    UIUtilE::Assign(this, m_prevButton,      "prev", &err);
    UIUtilE::Assign(this, m_nextButton,      "next", &err);
    UIUtilE::Assign(this, m_typeList,        "type", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'import_coverart'");
        return false;
    }

    if (m_typeList)
    {
        new MythUIButtonListItem(m_typeList, tr("Front Cover"),
                                 QVariant::fromValue((int)IT_FRONTCOVER));
        new MythUIButtonListItem(m_typeList, tr("Back Cover"),
                                 QVariant::fromValue((int)IT_BACKCOVER));
        new MythUIButtonListItem(m_typeList, tr("CD"),
                                 QVariant::fromValue((int)IT_CD));
        new MythUIButtonListItem(m_typeList, tr("Inlay"),
                                 QVariant::fromValue((int)IT_INLAY));
        new MythUIButtonListItem(m_typeList, tr("<Unknown>"),
                                 QVariant::fromValue((int)IT_UNKNOWN));

        connect(m_typeList, &MythUIButtonList::itemSelected,
                this, &ImportCoverArtDialog::selectorChanged);
    }

    if (m_copyButton)
        connect(m_copyButton, &MythUIButton::Clicked, this, &ImportCoverArtDialog::copyPressed);

    if (m_exitButton)
        connect(m_exitButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    if (m_prevButton)
        connect(m_prevButton, &MythUIButton::Clicked, this, &ImportCoverArtDialog::prevPressed);

    if (m_nextButton)
        connect(m_nextButton, &MythUIButton::Clicked, this, &ImportCoverArtDialog::nextPressed);

    BuildFocusList();

    scanDirectory();

    return true;
}

void ImportCoverArtDialog::selectorChanged()
{
    updateStatus();
}

void ImportCoverArtDialog::copyPressed()
{
    if (!m_filelist.empty())
    {
        if (!RemoteFile::CopyFile(m_filelist[m_currentFile], m_saveFilename, true))
        {
            //: %1 is the filename
            ShowOkPopup(tr("Copy CoverArt Failed.\nCopying to %1").arg(m_saveFilename));
            return;
        }

        // add the file to the database
        QString filename = m_saveFilename.section( '/', -1, -1);
        AlbumArtImage image;
        image.m_description = "";
        image.m_embedded = false;
        image.m_filename = filename;
        image.m_hostname = m_metadata->Hostname();
        image.m_imageType = (ImageType)m_typeList->GetItemCurrent()->GetData().toInt();

        m_metadata->getAlbumArtImages()->addImage(&image);
        m_metadata->getAlbumArtImages()->dumpToDatabase();

        updateStatus();
    }
}

void ImportCoverArtDialog::prevPressed()
{
    if (m_currentFile > 0)
    {
        m_currentFile--;
        updateTypeSelector();
        updateStatus();
    }
}

void ImportCoverArtDialog::nextPressed()
{
    if (m_currentFile < m_filelist.size() - 1)
    {
        m_currentFile++;
        updateTypeSelector();
        updateStatus();
    }
}

void ImportCoverArtDialog::scanDirectory()
{
    QDir d(m_sourceDir);

    if (!d.exists())
        return;

    QString nameFilter = gCoreContext->GetSetting("AlbumArtFilter",
                                              "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    QFileInfoList list = d.entryInfoList(nameFilter.split(";"),
                                         QDir::Files | QDir::Dirs |
                                         QDir::NoDotAndDotDot);
    if (list.isEmpty())
        return;

    for (const auto & fi : std::as_const(list))
    {
        QString filename = fi.absoluteFilePath();
        if (!fi.isDir())
        {
            m_filelist.append(filename);
        }
    }

    m_currentFile = 0;
    updateTypeSelector();
    updateStatus();
}

void ImportCoverArtDialog::updateStatus()
{
    if (!m_filelist.empty())
    {
        if (m_currentText)
        {
            //: %1 is the current position of the file being copied,
            //: %2 is the total number of files
            m_currentText->SetText(tr("%1 of %2", "Current file copied")
                                   .arg(m_currentFile + 1)
                                   .arg(m_filelist.size()));
        }
        m_filenameText->SetText(m_filelist[m_currentFile]);
        m_coverartImage->SetFilename(m_filelist[m_currentFile]);
        m_coverartImage->Load();

        QString saveFilename = filenameFromMetadata(m_metadata);
        QString fullFilename;

        QUrl url(m_musicStorageDir);
        fullFilename = MythCoreContext::GenMythURL(url.host(), 0, saveFilename, "Music");
        QString dir = fullFilename.section( '/', 0, -2);

        QFileInfo fi(m_filelist[m_currentFile]);
        switch (m_typeList->GetItemCurrent()->GetData().toInt())
        {
            case IT_FRONTCOVER:
                saveFilename = "front." + fi.suffix();
                break;
            case IT_BACKCOVER:
                saveFilename = "back." + fi.suffix();
                break;
            case IT_CD:
                saveFilename = "cd." + fi.suffix();
                break;
            case IT_INLAY:
                saveFilename = "inlay." + fi.suffix();
                break;
            case IT_UNKNOWN:
                saveFilename = "unknown." + fi.suffix();
                break;
            default:
                saveFilename = fi.fileName();
        }

        m_saveFilename = dir + "/" + saveFilename;
        m_destinationText->SetText(m_saveFilename);

        url.setUrl(m_saveFilename);

        if (!RemoteFile::FindFile(url.path(), "" , "Music").isEmpty())
            m_statusText->SetText(tr("File Already Exists"));
        else
            m_statusText->SetText(tr("New File"));
    }
    else
    {
        if (m_currentText)
            m_currentText->Reset();
        m_statusText->Reset();
        m_filenameText->Reset();
        m_coverartImage->Reset();
        m_destinationText->Reset();
    }
}

void ImportCoverArtDialog::updateTypeSelector()
{
    if (m_filelist.empty())
        return;

    QString filename = m_filelist[m_currentFile];
    QFileInfo fi(filename);
    filename = fi.fileName();

    if (filename.contains("front", Qt::CaseInsensitive))
        m_typeList->SetValue(tr("Front Cover"));
    else if (filename.contains("back", Qt::CaseInsensitive))
        m_typeList->SetValue(tr("Back Cover"));
    else if (filename.contains("inlay", Qt::CaseInsensitive))
        m_typeList->SetValue(tr("Inlay"));
    else if (filename.contains("cd", Qt::CaseInsensitive))
        m_typeList->SetValue(tr("CD"));
    else
        m_typeList->SetValue(tr("<Unknown>"));
}
