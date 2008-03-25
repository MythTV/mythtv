// qt
#include <qdir.h>
#include <qapplication.h>
#include <qfontmetrics.h>
//Added by qt3to4:
#include <QKeyEvent>
#include <QLabel>
#include <Q3Frame>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/audiooutput.h>

// mythmusic
#include "importmusic.h"
#include "decoder.h"
#include "genres.h"
#include "metadata.h"
#include "directoryfinder.h"
#include "cdrip.h"
#include "editmetadata.h"
#include "musicplayer.h"

static QString truncateFilename(QString filename, UITextType *textType)
{
    int width = textType->DisplayArea().width();
    QFontMetrics fm(textType->GetFont()->face);
    QStringList list = QStringList::split('/', filename);

    QString s = filename;
    int newWidth = width + 1;

    for (uint x = 0; x < filename.length(); x++)
    {
        if (x != 0)
            newWidth = fm.width(QString("...") + s, -1);
        else
            newWidth = fm.width(s, -1);

        if (newWidth < width)
            break;

        s = s.right(s.length() -1);
    }

    if (s == filename)
        return s;

    return QString("...") + s;
}

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

    len = s.readBlock(buffer, bufferSize);
    do
    {
        d.writeBlock(buffer, len);
        len = s.readBlock(buffer, bufferSize);
    } while (len > 0);

    s.close();
    d.close();

    return true;
}

///////////////////////////////////////////////////////////////////////////////

FileScannerThread::FileScannerThread(ImportMusicDialog *parent)
{
    m_parent = parent;
}

void FileScannerThread::run()
{
    m_parent->doScan();
}

///////////////////////////////////////////////////////////////////////////////


ImportMusicDialog::ImportMusicDialog(MythMainWindow *parent, const char* name)
                :MythThemedDialog(parent, "import_music", "music-", name)
{
    m_popupMenu = NULL;

    m_defaultCompilation = false;
    m_defaultCompArtist = "";
    m_defaultArtist = "";
    m_defaultAlbum = "";
    m_defaultGenre = "";
    m_defaultYear = 0;
    m_defaultRating = 0;
    m_haveDefaults = false;

    wireUpTheme();
    assignFirstFocus();
    m_somethingWasImported = false;
    m_tracks = new vector<TrackInfo*>;
    fillWidgets();
    m_location_edit->setText(gContext->GetSetting("MythMusicLastImportDir", "/"));
}

ImportMusicDialog::~ImportMusicDialog()
{
    gContext->SaveSetting("MythMusicLastImportDir", m_location_edit->getText());

    delete m_tracks;
}

void ImportMusicDialog::fillWidgets()
{
    if (m_tracks->size() > 0)
    {
        // update current
        m_current_text->SetText(QString("%1 of %2")
                .arg(m_currentTrack + 1).arg(m_tracks->size()));

        Metadata *meta = m_tracks->at(m_currentTrack)->metadata;
        m_filename_text->SetText(truncateFilename(meta->Filename(), m_filename_text));
        m_compilation_check->setState(meta->Compilation());
        m_compartist_text->SetText(meta->CompilationArtist());
        m_artist_text->SetText(meta->Artist());
        m_album_text->SetText(meta->Album());
        m_title_text->SetText(meta->Title());
        m_genre_text->SetText(meta->Genre());
        m_year_text->SetText(QString::number(meta->Year()));
        m_track_text->SetText(QString::number(meta->Track()));
        if (m_tracks->at(m_currentTrack)->isNewTune)
        {
            m_coverart_button->hide();
            m_status_text->SetText(tr("New File"));
        }
        else
        {
            m_coverart_button->show();
            m_status_text->SetText(tr("All Ready in Database"));
        }
    }
    else
    {
        // update current
        m_current_text->SetText(tr("Non found"));
        m_filename_text->SetText("");
        m_compilation_check->setState(false);
        m_compartist_text->SetText("");
        m_artist_text->SetText("");
        m_album_text->SetText("");
        m_title_text->SetText("");
        m_genre_text->SetText("");
        m_year_text->SetText("");
        m_track_text->SetText("");
        m_status_text->SetText("");
        m_coverart_button->hide();
    }

    buildFocusList();
}

void ImportMusicDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if (action == "LEFT")
        {
            m_prev_button->push();
        }
        else if (action == "RIGHT")
        {
            m_next_button->push();
        }
        else if (action == "SELECT")
        {
            if (getCurrentFocusWidget() != m_compilation_check)
                activateCurrent();
        }
        else if (action == "INFO")
        {
            showEditMetadataDialog();
        }
        else if (action == "MENU")
        {
            showMenu();
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
            if (m_tracks->size() > 0 && !m_tracks->at(m_currentTrack)->isNewTune)
                showImportCoverArtDialog();
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void ImportMusicDialog::wireUpTheme()
{
    m_location_edit = getUIRemoteEditType("location_edit");
    if (m_location_edit)
    {
        m_location_edit->createEdit(this);
//        connect(m_location_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    m_location_button = getUIPushButtonType("location_button");
    if (m_location_button)
    {
        connect(m_location_button, SIGNAL(pushed()), this, SLOT(locationPressed()));
    }

    m_scan_button = getUITextButtonType("scan_button");
    if (m_scan_button)
    {
        m_scan_button->setText(tr("Search"));
        connect(m_scan_button, SIGNAL(pushed()), this, SLOT(scanPressed()));
    }

    m_coverart_button = getUITextButtonType("coverart_button");
    if (m_coverart_button)
    {
        m_coverart_button->setText(tr("Cover Art"));
        connect(m_coverart_button, SIGNAL(pushed()), this, SLOT(coverArtPressed()));
    }

    m_filename_text = getUITextType("filename_text");
    m_compartist_text = getUITextType("compartist_text");
    m_artist_text = getUITextType("artist_text");
    m_album_text = getUITextType("album_text");
    m_title_text = getUITextType("title_text");
    m_genre_text = getUITextType("genre_text");
    m_year_text = getUITextType("year_text");
    m_track_text = getUITextType("track_text");
    m_current_text = getUITextType("current_text");
    m_status_text = getUITextType("status_text");

    m_compilation_check = getUICheckBoxType("compilation_check");

    m_play_button = getUITextButtonType("play_button");
    if (m_play_button)
    {
        m_play_button->setText(tr("Play"));
        connect(m_play_button, SIGNAL(pushed()), this, SLOT(playPressed()));
    }

    m_nextnew_button = getUITextButtonType("nextnew_button");
    if (m_nextnew_button)
    {
        m_nextnew_button->setText(tr("Next New"));
        connect(m_nextnew_button, SIGNAL(pushed()), this, SLOT(nextNewPressed()));
    }

    m_add_button = getUITextButtonType("add_button");
    if (m_add_button)
    {
        m_add_button->setText(tr("Add"));
        connect(m_add_button, SIGNAL(pushed()), this, SLOT(addPressed()));
    }

    m_addallnew_button = getUITextButtonType("addallnew_button");
    if (m_addallnew_button)
    {
        m_addallnew_button->setText(tr("Add All New"));
        connect(m_addallnew_button, SIGNAL(pushed()), this, SLOT(addAllNewPressed()));
    }

    m_next_button = getUIPushButtonType("next_button");
    if (m_next_button)
    {
        connect(m_next_button, SIGNAL(pushed()), this, SLOT(nextPressed()));
    }

    m_prev_button = getUIPushButtonType("prev_button");
    if (m_prev_button)
    {
        connect(m_prev_button, SIGNAL(pushed()), this, SLOT(prevPressed()));
    }

    buildFocusList();
}

void ImportMusicDialog::locationPressed()
{
    DirectoryFinder finder(m_location_edit->getText(),
                           gContext->GetMainWindow(), "directory finder");
    DialogCode res = finder.exec();

    if (kDialogCodeRejected != res)
    {
        m_location_edit->setText(finder.getSelected());
        editLostFocus();
    }
}

void ImportMusicDialog::coverArtPressed()
{
    showImportCoverArtDialog();
}

void ImportMusicDialog::playPressed()
{
    if (m_tracks->size() == 0)
        return;

    Metadata *meta = m_tracks->at(m_currentTrack)->metadata;

    gPlayer->playFile(*meta);
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
    if (m_tracks->size() == 0)
        return;

    Metadata *meta = m_tracks->at(m_currentTrack)->metadata;

    // is the current track a new file?
    if (m_tracks->at(m_currentTrack)->isNewTune)
    {
        // get the save filename - this also creates the correct directory stucture
        QString saveFilename = Ripper::filenameFromMetadata(meta);

        // we need to manually copy the file extension
        QFileInfo fi(meta->Filename());
        saveFilename += "." + fi.extension(false);

        // copy the file to the new location
        if (!copyFile(meta->Filename(), saveFilename))
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Copy Failed"),
                                      tr("Could not copy file to:") + saveFilename);
            return;
        }

        meta->setFilename(saveFilename);

        // do we need to update the tags?
        if (m_tracks->at(m_currentTrack)->metadataHasChanged)
        {
            Decoder *decoder = Decoder::create(saveFilename, NULL, NULL, true);
            if (decoder)
            {
                decoder->commitMetadata(meta);
                delete decoder;
            }
        }

        // update the database
        meta->dumpToDatabase();
        m_somethingWasImported = true;

        m_tracks->at(m_currentTrack)->isNewTune = Ripper::isNewTune(
                meta->Artist(), meta->Album(), meta->Title());

        // update the UI
        fillWidgets();
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Duplicate Track"),
                                  tr("This track is already in the database"));
    }
}

void ImportMusicDialog::addAllNewPressed()
{
    if (m_tracks->size() == 0)
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

    MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Add Tracks"),
                              tr("%1 new tracks were added to the database").arg(newCount));
}

void ImportMusicDialog::nextNewPressed()
{
    if (m_tracks->size() == 0)
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

void ImportMusicDialog::scanPressed()
{
    startScan();
}

void ImportMusicDialog::editLostFocus()
{
    startScan();
}

void ImportMusicDialog::startScan()
{
    MythBusyDialog *busy = new MythBusyDialog(QObject::tr("Searching for music files"));

    FileScannerThread *scanner = new FileScannerThread(this);
    busy->start();
    scanner->start();

    while (!scanner->isFinished())
    {
        usleep(500);
        qApp->processEvents();
    }

    delete scanner;

    m_currentTrack = 0;
    fillWidgets();

    busy->close();
    busy->deleteLater();
}

void ImportMusicDialog::doScan()
{
    m_tracks->clear();
    m_sourceFiles.clear();
    QString location = m_location_edit->getText();
    scanDirectory(location, m_tracks);
}

void ImportMusicDialog::scanDirectory(QString &directory, vector<TrackInfo*> *tracks)
{
    QDir d(directory);

    if (!d.exists())
        return;

    const QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();
        if (fi->isDir())
            scanDirectory(filename, tracks);
        else
        {
            Decoder *decoder = Decoder::create(filename, NULL, NULL, true);
            if (decoder)
            {
                Metadata *metadata = decoder->getMetadata();
                if (metadata)
                {
                    TrackInfo * track = new TrackInfo;
                    track->metadata = metadata;
                    track->isNewTune = Ripper::isNewTune(metadata->Artist(),
                            metadata->Album(), metadata->Title());
                    track->metadataHasChanged = false;
                    tracks->push_back(track);
                    m_sourceFiles.append(filename);
                }

                delete decoder;
            }
        }
    }
}

void ImportMusicDialog::showEditMetadataDialog()
{
    if (m_tracks->size() == 0)
        return;

    Metadata *editMeta = m_tracks->at(m_currentTrack)->metadata;

    EditMetadataDialog editDialog(editMeta, gContext->GetMainWindow(),
                                  "edit_metadata", "music-", "edit metadata");
    editDialog.setSaveMetadataOnly();

    if (kDialogCodeRejected != editDialog.exec())
    {
        m_tracks->at(m_currentTrack)->metadataHasChanged = true;
        m_tracks->at(m_currentTrack)->isNewTune = Ripper::isNewTune(
                editMeta->Artist(), editMeta->Album(), editMeta->Title());
        fillWidgets();
    }
}

void ImportMusicDialog::showMenu()
{
    if (m_popupMenu)
        return;

    if (m_tracks->size() == 0)
        return;

    m_popupMenu = new MythPopupBox(gContext->GetMainWindow(),
                                      "menu");

    QAbstractButton *button = m_popupMenu->addButton(tr("Save Defaults"), this,
            SLOT(saveDefaults()));

    QLabel *splitter = m_popupMenu->addLabel(" ", MythPopupBox::Small);
    splitter->setLineWidth(2);
    splitter->setFrameShape(Q3Frame::HLine);
    splitter->setFrameShadow(Q3Frame::Sunken);
    splitter->setMaximumHeight((int) (5 * hmult));
    splitter->setMaximumHeight((int) (5 * hmult));

    if (m_haveDefaults)
    {
        m_popupMenu->addButton(tr("Change Compilation Flag"), this,
                                SLOT(setCompilation()));
        m_popupMenu->addButton(tr("Change Compilation Artist"), this,
                                SLOT(setCompilationArtist()));
        m_popupMenu->addButton(tr("Change Artist"), this,
                                SLOT(setArtist()));
        m_popupMenu->addButton(tr("Change Album"), this,
                            SLOT(setAlbum()));
        m_popupMenu->addButton(tr("Change Genre"), this,
                            SLOT(setGenre()));
        m_popupMenu->addButton(tr("Change Year"), this,
                            SLOT(setYear()));
        m_popupMenu->addButton(tr("Change Rating"), this,
                            SLOT(setRating()));
    }

    m_popupMenu->addButton(tr("Cancel"), this,
                           SLOT(closeMenu()));

    m_popupMenu->ShowPopup(this, SLOT(closeMenu()));

    button->setFocus();
}

void ImportMusicDialog::closeMenu()
{
    if (!m_popupMenu)
        return;

    m_popupMenu->hide();
    m_popupMenu->deleteLater();
    m_popupMenu = NULL;
}

void ImportMusicDialog::saveDefaults(void)
{
    closeMenu();

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
    closeMenu();

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
    closeMenu();

    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setCompilationArtist(m_defaultCompArtist);

    fillWidgets();
}

void ImportMusicDialog::setArtist(void)
{
    closeMenu();

    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setArtist(m_defaultArtist);

    m_tracks->at(m_currentTrack)->isNewTune = Ripper::isNewTune(
            data->Artist(), data->Album(), data->Title());

    fillWidgets();
}

void ImportMusicDialog::setAlbum(void)
{
    closeMenu();

    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setAlbum(m_defaultAlbum);

    m_tracks->at(m_currentTrack)->isNewTune = Ripper::isNewTune(
            data->Artist(), data->Album(), data->Title());

    fillWidgets();
}

void ImportMusicDialog::setYear(void)
{
    closeMenu();

    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setYear(m_defaultYear);

    fillWidgets();
}

void ImportMusicDialog::setGenre(void)
{
    closeMenu();

    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setGenre(m_defaultGenre);

    fillWidgets();
}

void ImportMusicDialog::setRating(void)
{
    closeMenu();

    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setRating(m_defaultRating);
}

void ImportMusicDialog::setTitleInitialCap(void)
{
    closeMenu();

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    QString title = data->Title();
    bool bFoundCap = false;

    for (uint x = 0; x < title.length(); x++)
    {
        if (title[x].isLetter())
        {
            if (bFoundCap == false)
            {
                title[x] = title[x].upper();
                bFoundCap = true;
            }
            else
                title[x] = title[x].lower();
        }
    }

    data->setTitle(title);
    fillWidgets();
}

void ImportMusicDialog::setTitleWordCaps(void)
{
    closeMenu();
    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    QString title = data->Title();
    bool bInWord = false;

    for (uint x = 0; x < title.length(); x++)
    {
        if (title[x].isSpace())
            bInWord = false;
        else
        {
            if (title[x].isLetter())
            {
                if (!bInWord)
                {
                    title[x] = title[x].upper();
                    bInWord = true;
                }
                else
                    title[x] = title[x].lower();
            }
        }
    }

    data->setTitle(title);
    fillWidgets();
}

void ImportMusicDialog::showImportCoverArtDialog(void)
{
    if (m_tracks->size() == 0)
        return;

    QFileInfo fi(m_sourceFiles.at(m_currentTrack));

    ImportCoverArtDialog dialog(fi.dirPath(true), m_tracks->at(m_currentTrack)->metadata,
                                gContext->GetMainWindow(), "import_coverart");
    dialog.exec();
}

///////////////////////////////////////////////////////////////////////////////

ImportCoverArtDialog::ImportCoverArtDialog(const QString &sourceDir, Metadata *metadata,
                                           MythMainWindow *parent, const char* name)
    :MythThemedDialog(parent, "import_coverart", "music-", name)
{
    m_sourceDir = sourceDir;
    m_metadata = metadata;

    wireUpTheme();
    assignFirstFocus();
    scanDirectory();
}

ImportCoverArtDialog::~ImportCoverArtDialog()
{

}

void ImportCoverArtDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if (action == "LEFT")
        {
            if (getCurrentFocusWidget() == m_type_selector)
                m_type_selector->push(false);
            else
                m_prev_button->push();
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == m_type_selector)
                m_type_selector->push(true);
            else
                m_next_button->push();
        }
        else if (action == "SELECT")
        {
            activateCurrent();
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void ImportCoverArtDialog::wireUpTheme()
{
    m_filename_text = getUITextType("file_text");
    m_current_text = getUITextType("current_text");
    m_status_text = getUITextType("status_text");
    m_destination_text = getUITextType("destination_text");

    m_coverart_image = getUIImageType("coverart_image");

    m_copy_button = getUITextButtonType("copy_button");
    if (m_copy_button)
    {
        m_copy_button->setText(tr("Copy"));
        connect(m_copy_button, SIGNAL(pushed()), this, SLOT(copyPressed()));
    }

    m_exit_button = getUITextButtonType("exit_button");
    if (m_exit_button)
    {
        m_exit_button->setText(tr("Exit"));
        connect(m_exit_button, SIGNAL(pushed()), this, SLOT(reject()));
    }

    m_prev_button = getUIPushButtonType("prev_button");
    if (m_prev_button)
    {
        connect(m_prev_button, SIGNAL(pushed()), this, SLOT(prevPressed()));
    }

    m_next_button = getUIPushButtonType("next_button");
    if (m_next_button)
    {
        connect(m_next_button, SIGNAL(pushed()), this, SLOT(nextPressed()));
    }

    m_type_selector = getUISelectorType("type_selector");
    if (m_type_selector)
    {
        m_type_selector->addItem(0, tr("Front Cover"));
        m_type_selector->addItem(1, tr("Back Cover"));
        m_type_selector->addItem(2, tr("CD"));
        m_type_selector->addItem(3, tr("Inlay"));
        m_type_selector->addItem(4, tr("<Unknown>"));
        m_type_selector->setToItem(0);

        connect(m_type_selector, SIGNAL(pushed(int)), this, SLOT(selectorChanged(int)));
    }
}

void ImportCoverArtDialog::selectorChanged(int item)
{
    (void) item;
    updateStatus();
}

void ImportCoverArtDialog::copyPressed()
{
    if (m_filelist.size() > 0)
    {
        copyFile(m_filelist[m_currentFile], m_saveFilename);
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

    QString nameFilter = gContext->GetSetting("AlbumArtFilter",
                                              "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    QFileInfoList list = d.entryInfoList(nameFilter);
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();
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
        m_current_text->SetText(QString("%1 of %2").arg(m_currentFile + 1).arg(m_filelist.size()));
        m_filename_text->SetText(truncateFilename(m_filelist[m_currentFile], m_filename_text));
        m_coverart_image->SetImage(m_filelist[m_currentFile]);
        m_coverart_image->LoadImage();

        QString saveFilename = Ripper::filenameFromMetadata(m_metadata, false);
        QFileInfo fi(saveFilename);
        QString saveDir = fi.dirPath(true);

        fi.setFile(m_filelist[m_currentFile]);
        switch (m_type_selector->getCurrentInt())
        {
            case 0:
                saveFilename = "front." + fi.extension(false);
                break;
            case 1:
                saveFilename = "back." + fi.extension(false);
                break;
            case 2:
                saveFilename = "cd." + fi.extension(false);
                break;
            case 3:
                saveFilename = "inlay." + fi.extension(false);
                break;
            default:
                saveFilename = fi.fileName();
        }

        m_saveFilename = saveDir + "/" + saveFilename;
        m_destination_text->SetText(truncateFilename(m_saveFilename, m_destination_text));

        if (QFile::exists(m_saveFilename))
            m_status_text->SetText(tr("File Already Exists"));
        else
            m_status_text->SetText(tr("New File"));
    }
    else
    {
        m_current_text->SetText(tr("Non Found"));
        m_status_text->SetText("");
        m_filename_text->SetText("");
        m_coverart_image->SetImage("mm_trans_background.png");
        m_coverart_image->LoadImage();
        m_destination_text->SetText("");
    }
}

void ImportCoverArtDialog::updateTypeSelector()
{
    if (m_filelist.size() == 0)
        return;

    QString filename = m_filelist[m_currentFile]; 
    QFileInfo fi(filename);
    filename = fi.fileName();

    if (filename.contains("front", false) > 0)
        m_type_selector->setToItem(tr("Front Cover"));
    else if (filename.contains("back", false) > 0)
        m_type_selector->setToItem(tr("Back Cover"));
    else if (filename.contains("inlay", false) > 0)
        m_type_selector->setToItem(tr("Inlay"));
    else if (filename.contains("cd", false) > 0)
        m_type_selector->setToItem(tr("CD"));
    else
        m_type_selector->setToItem(tr("<Unknown>"));
}
