// qt
#include <QDir>
#include <QFontMetrics>
#include <QApplication>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <audiooutput.h>

// mythmusic
#include "importmusic.h"
#include "decoder.h"
#include "genres.h"
#include "metadata.h"
#include "cdrip.h"
#include "editmetadata.h"
#include "musicplayer.h"
#include "metaio.h"
#include "musicutils.h"

#include <mythdialogbox.h>
#include <mythuitext.h>
#include <mythuiimage.h>
#include <mythuicheckbox.h>
#include <mythuitextedit.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythprogressdialog.h>
#include <mythuifilebrowser.h>
#include "mythlogging.h"

static bool copyFile(const QString &src, const QString &dst)
{
    const int bufferSize = 16*1024;

    QFile s(src);
    QFile d(dst);
    char buffer[bufferSize];
    int len;

    if (!s.open(QIODevice::ReadOnly))
        return false;

    if (!d.open(QIODevice::WriteOnly))
    {
        s.close();
        return false;
    }

    len = s.read(buffer, bufferSize);
    do
    {
        d.write(buffer, len);
        len = s.read(buffer, bufferSize);
    } while (len > 0);

    s.close();
    d.close();

    return true;
}

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


ImportMusicDialog::ImportMusicDialog(MythScreenStack *parent) :
    MythScreenType(parent, "musicimportfiles"),

    m_somethingWasImported(false),
    m_tracks(new vector<TrackInfo*>),
    m_currentTrack(0),
    m_playingMetaData(NULL),
    // GUI stuff
    m_locationEdit(NULL),
    m_locationButton(NULL),
    m_scanButton(NULL),
    m_coverartButton(NULL),
    m_filenameText(NULL),
    m_compartistText(NULL),
    m_artistText(NULL),
    m_albumText(NULL),
    m_titleText(NULL),
    m_genreText(NULL),
    m_yearText(NULL),
    m_trackText(NULL),
    m_nextButton(NULL),
    m_prevButton(NULL),
    m_currentText(NULL),
    m_statusText(NULL),
    m_playButton(NULL),
    m_addButton(NULL),
    m_addallnewButton(NULL),
    m_nextnewButton(NULL),
    m_compilationCheck(NULL),
    m_popupMenu(NULL),
    // default metadata values
    m_defaultCompilation(false),
    m_defaultYear(0),
    m_defaultRating(0),
    m_haveDefaults(false)
{
}

ImportMusicDialog::~ImportMusicDialog()
{
    if (gPlayer->getCurrentMetadata() && m_playingMetaData)
    {
        if (gPlayer->isPlaying() && gPlayer->getCurrentMetadata()->Filename() == m_playingMetaData->Filename())
            gPlayer->next();
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
        m_currentText->SetText(QString("%1 of %2")
                .arg(m_currentTrack + 1).arg(m_tracks->size()));

        Metadata *meta = m_tracks->at(m_currentTrack)->metadata;
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

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
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
            showMenu();
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
                    ShowOkPopup(msg, this, SLOT(doExit(bool)), true);
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
            handled = false;
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

    connect(m_prevButton, SIGNAL(Clicked()), SLOT(prevPressed()));
    connect(m_locationButton, SIGNAL(Clicked()), SLOT(locationPressed()));
    connect(m_scanButton, SIGNAL(Clicked()), SLOT(startScan()));
    connect(m_coverartButton, SIGNAL(Clicked()), SLOT(coverArtPressed()));
    connect(m_playButton, SIGNAL(Clicked()), SLOT(playPressed()));
    connect(m_nextnewButton, SIGNAL(Clicked()), SLOT(nextNewPressed()));
    connect(m_addButton, SIGNAL(Clicked()), SLOT(addPressed()));
    connect(m_addallnewButton, SIGNAL(Clicked()), SLOT(addAllNewPressed()));
    connect(m_nextButton, SIGNAL(Clicked()), SLOT(nextPressed()));

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
    MythUIFileBrowser *fb = new MythUIFileBrowser(popupStack, m_locationEdit->GetText());
    // TODO Install a name filter on supported music formats
    fb->SetTypeFilter(QDir::AllDirs | QDir::Readable);
    if (fb->Create())
    {
        fb->SetReturnEvent(this, "locationchange");
        popupStack->AddScreen(fb);
    }
    else
        delete fb;
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

    Metadata *meta = m_tracks->at(m_currentTrack)->metadata;

    // is the current track a new file?
    if (m_tracks->at(m_currentTrack)->isNewTune)
    {
        // get the save filename - this also creates the correct directory stucture
        QString saveFilename = filenameFromMetadata(meta);

        // we need to manually copy the file extension
        QFileInfo fi(meta->Filename());
        saveFilename += "." + fi.suffix();

        // copy the file to the new location
        if (!copyFile(meta->Filename(), gMusicData->musicDir + saveFilename))
        {
            ShowOkPopup(tr("Copy Failed\nCould not copy file to: %1")
                                                        .arg(gMusicData->musicDir + saveFilename));
            return;
        }

        meta->setFilename(saveFilename);

        // do we need to update the tags?
        if (m_tracks->at(m_currentTrack)->metadataHasChanged)
        {
            MetaIO *tagger = MetaIO::createTagger(gMusicData->musicDir + saveFilename);
            if (tagger)
            {
                tagger->write(meta);
                delete tagger;
            }
        }

        meta->setFileSize((quint64)QFileInfo(saveFilename).size());

        // update the database
        meta->dumpToDatabase();

        // read any embedded images from the tag
        MetaIO *tagger = MetaIO::createTagger(meta->Filename(true));
        if (tagger && tagger->supportsEmbeddedImages())
        {
            AlbumArtList artList = tagger->getAlbumArtList(meta->Filename(true));
            meta->setEmbeddedAlbumArt(artList);
            meta->getAlbumArtImages()->dumpToDatabase();
            delete tagger;
        }

        m_somethingWasImported = true;

        m_tracks->at(m_currentTrack)->isNewTune = 
                isNewTune(meta->Artist(), meta->Album(), meta->Title());

        // update the UI
        fillWidgets();
    }
    else
        ShowOkPopup(tr("This track is already in the database"));
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
        qApp->processEvents();

        if (m_tracks->at(m_currentTrack)->isNewTune)
        {
            addPressed();
            newCount++;
        }

        qApp->processEvents();

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

void ImportMusicDialog::startScan()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUIBusyDialog *busy = 
            new MythUIBusyDialog(QObject::tr("Searching for music files"),
                                     popupStack,
                                     "scanbusydialog");

    if (busy->Create())
    {
        popupStack->AddScreen(busy, false);
    }
    else
    {
        delete busy;
        busy = NULL;
    }
    FileScannerThread *scanner = new FileScannerThread(this);
    scanner->start();

    while (!scanner->isFinished())
    {
        usleep(500);
        qApp->processEvents();
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

void ImportMusicDialog::scanDirectory(QString &directory, vector<TrackInfo*> *tracks)
{
    QDir d(directory);

    if (!d.exists())
        return;

    d.setFilter(QDir::NoDotAndDotDot);
    const QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        QString filename = fi->absoluteFilePath();
        if (fi->isDir())
            scanDirectory(filename, tracks);
        else
        {
            MetaIO *tagger = MetaIO::createTagger(filename);
            if (tagger)
            {
                Metadata *metadata = tagger->read(filename);
                if (metadata)
                {
                    TrackInfo * track = new TrackInfo;
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

    Metadata *editMeta = m_tracks->at(m_currentTrack)->metadata;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    EditMetadataDialog *editDialog = new EditMetadataDialog(mainStack, editMeta);
    editDialog->setSaveMetadataOnly();

    if (!editDialog->Create())
    {
        delete editDialog;
        return;
    }

    connect(editDialog, SIGNAL(metadataChanged()), this, SLOT(metadataChanged()));

    mainStack->AddScreen(editDialog);
}

void ImportMusicDialog::metadataChanged(void)
{
    Metadata *editMeta = m_tracks->at(m_currentTrack)->metadata;
    m_tracks->at(m_currentTrack)->metadataHasChanged = true;
    m_tracks->at(m_currentTrack)->isNewTune =
            isNewTune(editMeta->Artist(), editMeta->Album(), editMeta->Title());
    fillWidgets();
}

void ImportMusicDialog::showMenu()
{
    if (m_popupMenu)
        return;

    if (m_tracks->empty())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menu = new MythDialogBox("", popupStack, "importmusicmenu");

    if (menu->Create())
        popupStack->AddScreen(menu);
    else
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "menu");
    menu->AddButton(tr("Save Defaults"), SLOT(saveDefaults()));

    if (m_haveDefaults)
    {
        menu->AddButton(tr("Change Compilation Flag"), SLOT(setCompilation()));
        menu->AddButton(tr("Change Compilation Artist"),
                                SLOT(setCompilationArtist()));
        menu->AddButton(tr("Change Artist"), SLOT(setArtist()));
        menu->AddButton(tr("Change Album"), SLOT(setAlbum()));
        menu->AddButton(tr("Change Genre"), SLOT(setGenre()));
        menu->AddButton(tr("Change Year"), SLOT(setYear()));
        menu->AddButton(tr("Change Rating"), SLOT(setRating()));
    }
}

void ImportMusicDialog::saveDefaults(void)
{
    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
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

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;

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

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setCompilationArtist(m_defaultCompArtist);

    fillWidgets();
}

void ImportMusicDialog::setArtist(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setArtist(m_defaultArtist);

    m_tracks->at(m_currentTrack)->isNewTune =
            isNewTune(data->Artist(), data->Album(), data->Title());

    fillWidgets();
}

void ImportMusicDialog::setAlbum(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setAlbum(m_defaultAlbum);

    m_tracks->at(m_currentTrack)->isNewTune = 
            isNewTune(data->Artist(), data->Album(), data->Title());

    fillWidgets();
}

void ImportMusicDialog::setYear(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setYear(m_defaultYear);

    fillWidgets();
}

void ImportMusicDialog::setTrack(void)
{
    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setTrack(data->Track() + 100);

    fillWidgets();
}

void ImportMusicDialog::setGenre(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setGenre(m_defaultGenre);

    fillWidgets();
}

void ImportMusicDialog::setRating(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setRating(m_defaultRating);
}

void ImportMusicDialog::setTitleInitialCap(void)
{
    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    QString title = data->Title();
    bool bFoundCap = false;

    for (int x = 0; x < title.length(); x++)
    {
        if (title[x].isLetter())
        {
            if (bFoundCap == false)
            {
                title[x] = title[x].toUpper();
                bFoundCap = true;
            }
            else
                title[x] = title[x].toLower();
        }
    }

    data->setTitle(title);
    fillWidgets();
}

void ImportMusicDialog::setTitleWordCaps(void)
{
    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    QString title = data->Title();
    bool bInWord = false;

    for (int x = 0; x < title.length(); x++)
    {
        if (title[x].isSpace())
            bInWord = false;
        else
        {
            if (title[x].isLetter())
            {
                if (!bInWord)
                {
                    title[x] = title[x].toUpper();
                    bInWord = true;
                }
                else
                    title[x] = title[x].toLower();
            }
        }
    }

    data->setTitle(title);
    fillWidgets();
}

void ImportMusicDialog::showImportCoverArtDialog(void)
{
    if (m_tracks->empty())
        return;

    QFileInfo fi(m_sourceFiles.at(m_currentTrack));

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ImportCoverArtDialog *import = new ImportCoverArtDialog(mainStack,
                                        fi.absolutePath(),
                                        m_tracks->at(m_currentTrack)->metadata);

    if (import->Create())
        mainStack->AddScreen(import);
    else
        delete import;
}

void ImportMusicDialog::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);
        if (dce->GetId() == "locationchange")
        {
            m_locationEdit->SetText(dce->GetResultText());
            startScan();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

ImportCoverArtDialog::ImportCoverArtDialog(MythScreenStack *parent,
                                           const QString &sourceDir,
                                           Metadata *metadata) :
    MythScreenType(parent, "import_coverart"),
    m_sourceDir(sourceDir),
    m_metadata(metadata),
    m_currentFile(0),
    //  GUI stuff
    m_filenameText(NULL),
    m_currentText(NULL),
    m_statusText(NULL),
    m_destinationText(NULL),
    m_coverartImage(NULL),
    m_typeList(NULL),
    m_nextButton(NULL),
    m_prevButton(NULL),
    m_copyButton(NULL),
    m_exitButton(NULL)
{
}

ImportCoverArtDialog::~ImportCoverArtDialog()
{

}

bool ImportCoverArtDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
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
            handled = false;
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
                                 qVariantFromValue(0));
        new MythUIButtonListItem(m_typeList, tr("Back Cover"),
                                 qVariantFromValue(1));
        new MythUIButtonListItem(m_typeList, tr("CD"),
                                 qVariantFromValue(2));
        new MythUIButtonListItem(m_typeList, tr("Inlay"),
                                 qVariantFromValue(3));
        new MythUIButtonListItem(m_typeList, tr("<Unknown>"),
                                 qVariantFromValue(4));

        connect(m_typeList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(selectorChanged()));
    }

    if (m_copyButton)
        connect(m_copyButton, SIGNAL(Clicked()), this, SLOT(copyPressed()));

    if (m_exitButton)
        connect(m_exitButton, SIGNAL(Clicked()), this, SLOT(Close()));

    if (m_prevButton)
        connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(prevPressed()));

    if (m_nextButton)
        connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(nextPressed()));

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
    if (m_filelist.size() > 0)
    {
        if (!copyFile(m_filelist[m_currentFile], m_saveFilename))
        {
            ShowOkPopup(QString("Copy CoverArt Failed. \nCopying to %1")
                    .arg(m_saveFilename));
            return;
        }

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
    if (m_currentFile < (int) m_filelist.size() - 1)
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
                                         QDir::NoDotAndDotDot);
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        QString filename = fi->absoluteFilePath();
        if (!fi->isDir())
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
    if (m_filelist.size() > 0)
    {
        if (m_currentText)
            m_currentText->SetText(QString("%1 of %2").arg(m_currentFile + 1)
                                                      .arg(m_filelist.size()));
        m_filenameText->SetText(m_filelist[m_currentFile]);
        m_coverartImage->SetFilename(m_filelist[m_currentFile]);
        m_coverartImage->Load();

        QString saveFilename = gMusicData->musicDir + filenameFromMetadata(m_metadata, false);
        QFileInfo fi(saveFilename);
        QString saveDir = fi.absolutePath();

        fi.setFile(m_filelist[m_currentFile]);
        switch (m_typeList->GetItemCurrent()->GetData().toInt())
        {
            case 0:
                saveFilename = "front." + fi.suffix();
                break;
            case 1:
                saveFilename = "back." + fi.suffix();
                break;
            case 2:
                saveFilename = "cd." + fi.suffix();
                break;
            case 3:
                saveFilename = "inlay." + fi.suffix();
                break;
            default:
                saveFilename = fi.fileName();
        }

        m_saveFilename = saveDir + "/" + saveFilename;
        m_destinationText->SetText(m_saveFilename);

        if (QFile::exists(m_saveFilename))
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
    if (m_filelist.size() == 0)
        return;

    QString filename = m_filelist[m_currentFile];
    QFileInfo fi(filename);
    filename = fi.fileName();

    if (filename.contains("front", Qt::CaseInsensitive) > 0)
        m_typeList->SetValue(tr("Front Cover"));
    else if (filename.contains("back", Qt::CaseInsensitive) > 0)
        m_typeList->SetValue(tr("Back Cover"));
    else if (filename.contains("inlay", Qt::CaseInsensitive) > 0)
        m_typeList->SetValue(tr("Inlay"));
    else if (filename.contains("cd", Qt::CaseInsensitive) > 0)
        m_typeList->SetValue(tr("CD"));
    else
        m_typeList->SetValue(tr("<Unknown>"));
}
