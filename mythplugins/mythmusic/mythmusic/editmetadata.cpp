// C++
#include <utility>

// qt
#include <QKeyEvent>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mthreadpool.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/remotefile.h>
#include <libmythmetadata/metaio.h>
#include <libmythmetadata/musicutils.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuifilebrowser.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuispinbox.h>
#include <libmythui/mythuistatetype.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// mythmusic
#include "decoder.h"
#include "genres.h"
#include "musicdata.h"
#include "musicplayer.h"


#include "editmetadata.h"

// these need to be static so both screens can pick them up
bool           EditMetadataCommon::s_metadataOnly = false;
MusicMetadata *EditMetadataCommon::s_metadata = nullptr;
MusicMetadata *EditMetadataCommon::s_sourceMetadata = nullptr;

EditMetadataCommon::EditMetadataCommon(MythScreenStack *parent,
                                       MusicMetadata *source_metadata,
                                       const QString &name) :
    MythScreenType(parent, name)
{
    // make a copy so we can abandon changes
    s_metadata = new MusicMetadata(*source_metadata);
    s_sourceMetadata = source_metadata;

    s_metadataOnly = false;
}

EditMetadataCommon::~EditMetadataCommon()
{
        // do we need to save anything?
    if (m_albumArtChanged)
    {
        s_metadata->getAlbumArtImages()->dumpToDatabase();

        // force a reload of the images for any tracks affected
        MetadataPtrList *allMusic =  gMusicData->m_all_music->getAllMetadata();
        for (int x = 0; x < allMusic->count(); x++)
        {
            if ((allMusic->at(x)->ID() == s_sourceMetadata->ID()) ||
                (allMusic->at(x)->getDirectoryId() == s_sourceMetadata->getDirectoryId()))
            {
                allMusic->at(x)->reloadAlbumArtImages();
                gPlayer->sendAlbumArtChangedEvent(allMusic->at(x)->ID());
            }
        }
    }
}

bool EditMetadataCommon::CreateCommon(void)
{
    bool err = false;

    UIUtilE::Assign(this, m_doneButton, "donebutton", &err);

    connect(m_doneButton, &MythUIButton::Clicked, this, &EditMetadataCommon::showSaveMenu);

    return err;
}

bool EditMetadataCommon::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "ESCAPE")
            showSaveMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void EditMetadataCommon::updateMetadata()
{
    MythUITextEdit *edit = dynamic_cast<MythUITextEdit *>(GetChild("albumedit"));
    if (edit)
        s_metadata->setAlbum(edit->GetText());

    edit = dynamic_cast<MythUITextEdit *>(GetChild("artistedit"));
    if (edit)
        s_metadata->setArtist(edit->GetText());

    edit = dynamic_cast<MythUITextEdit *>(GetChild("compartistedit"));
    if (edit)
        s_metadata->setCompilationArtist(edit->GetText());

    edit = dynamic_cast<MythUITextEdit *>(GetChild("titleedit"));
    if (edit)
        s_metadata->setTitle(edit->GetText());

    edit = dynamic_cast<MythUITextEdit *>(GetChild("genreedit"));
    if (edit)
        s_metadata->setGenre(edit->GetText());

    MythUISpinBox *spin = dynamic_cast<MythUISpinBox *>(GetChild("yearspin"));
    if (spin)
        s_metadata->setYear(spin->GetIntValue());

    spin = dynamic_cast<MythUISpinBox *>(GetChild("tracknumspin"));
    if (spin)
        s_metadata->setTrack(spin->GetIntValue());

    spin = dynamic_cast<MythUISpinBox *>(GetChild("discnumspin"));
    if (spin)
        s_metadata->setDiscNumber(spin->GetIntValue());

    spin = dynamic_cast<MythUISpinBox *>(GetChild("ratingspin"));
    if (spin)
        s_metadata->setRating(spin->GetIntValue());

    MythUICheckBox *check = dynamic_cast<MythUICheckBox *>(GetChild("compilationcheck"));
    if (check)
        s_metadata->setCompilation(check->GetBooleanCheckState());
}

void EditMetadataCommon::showSaveMenu()
{
    updateMetadata();

    if (!hasMetadataChanged())
    {
        Close();
        return;
    }

    QString label = tr("Save Changes?");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox(label, popupStack, "savechangesmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "savechangesmenu");

    if (s_metadataOnly)
        menu->AddButton(tr("Save Changes"), &EditMetadataCommon::saveToMetadata);
    else
        menu->AddButton(tr("Save Changes"), &EditMetadataCommon::saveAll);

    menu->AddButton(tr("Exit/Do Not Save"), &EditMetadataCommon::cleanupAndClose);

    popupStack->AddScreen(menu);
}

void EditMetadataCommon::cleanupAndClose(void)
{
    if (s_metadata)
    {
        delete s_metadata;
        s_metadata = nullptr;
    }

    Close();
}

void EditMetadataCommon::saveToMetadata()
{
    *s_sourceMetadata = *s_metadata;
    emit metadataChanged();
    cleanupAndClose();
}

void EditMetadataCommon::saveToDatabase()
{
    s_metadata->setAlbumId(-1);
    s_metadata->setGenreId(-1);
    s_metadata->setArtistId(-1);

    s_metadata->dumpToDatabase();
    *s_sourceMetadata = *s_metadata;

    gPlayer->sendMetadataChangedEvent(s_sourceMetadata->ID());
}

void EditMetadataCommon::saveAll()
{
    saveToDatabase();

    // only write to the tag if it's enabled by the user
    if (GetMythDB()->GetBoolSetting("AllowTagWriting", false))
    {
        QStringList strList;
        strList << "MUSIC_TAG_UPDATE_METADATA %1 %2"
                << s_metadata->Hostname()
                << QString::number(s_metadata->ID());

        auto *thread = new SendStringListThread(strList);
        MThreadPool::globalInstance()->start(thread, "UpdateMetadata");
    }

    cleanupAndClose();
}

void EditMetadataCommon::setSaveMetadataOnly(void)
{
    s_metadataOnly = true;

    MythUIButton *albumartButton = dynamic_cast<MythUIButton *>(GetChild("albumartbutton"));
    if (albumartButton)
        albumartButton->Hide();
}

bool EditMetadataCommon::hasMetadataChanged(void)
{
    bool changed = false;

    changed |= (s_metadata->Album() != s_sourceMetadata->Album());
    changed |= (s_metadata->Artist() != s_sourceMetadata->Artist());
    changed |= (s_metadata->CompilationArtist() != s_sourceMetadata->CompilationArtist());
    changed |= (s_metadata->Title() != s_sourceMetadata->Title());
    changed |= (s_metadata->Genre() != s_sourceMetadata->Genre());
    changed |= (s_metadata->Year() != s_sourceMetadata->Year());
    changed |= (s_metadata->Track() != s_sourceMetadata->Track());
    changed |= (s_metadata->DiscNumber() != s_sourceMetadata->DiscNumber());
    changed |= (s_metadata->Compilation() != s_sourceMetadata->Compilation());
    changed |= (s_metadata->Rating() != s_sourceMetadata->Rating());

    return changed;
}

/// search Google for images
void EditMetadataCommon::searchForAlbumImages(void)
{
    QString artist = s_metadata->Artist().replace(' ', '+');
    artist = QUrl::toPercentEncoding(artist, "+");

    QString album = s_metadata->Album().replace(' ', '+');
    album = QUrl::toPercentEncoding(album, "+");

    QUrl url("http://www.google.co.uk/images?q=" + artist + "+" + album, QUrl::TolerantMode);

    m_searchType = "album";

    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), GetConfDir() + "/MythMusic/", "front.jpg");
}

void EditMetadataCommon::scanForImages(void)
{
    s_metadata->getAlbumArtImages()->scanForImages();
}

///////////////////////////////////////////////////////////////////////////////
// EditMatadataDialog

EditMetadataDialog::EditMetadataDialog(MythScreenStack *parent, MusicMetadata *source_metadata)
    : EditMetadataCommon(parent, source_metadata, "EditMetadataDialog")
{
    gCoreContext->addListener(this);
}

EditMetadataDialog::EditMetadataDialog(MythScreenStack *parent)
    : EditMetadataCommon(parent, "EditMetadataDialog")
{
    gCoreContext->addListener(this);
}

EditMetadataDialog::~EditMetadataDialog()
{
    gCoreContext->removeListener(this);
}

bool EditMetadataDialog::Create(void)
{
    if (! LoadWindowFromXML("music-ui.xml", "editmetadata", this))
        return false;

    bool err = CreateCommon();

    UIUtilE::Assign(this, m_titleEdit,      "titleedit",      &err);
    UIUtilE::Assign(this, m_artistEdit,     "artistedit",     &err);
    UIUtilE::Assign(this, m_compArtistEdit, "compartistedit", &err);
    UIUtilE::Assign(this, m_albumEdit,      "albumedit",      &err);
    UIUtilE::Assign(this, m_genreEdit,      "genreedit",      &err);

    UIUtilE::Assign(this, m_yearSpin,       "yearspin",       &err);
    UIUtilE::Assign(this, m_trackSpin,      "tracknumspin",   &err);
    UIUtilW::Assign(this, m_discSpin,       "discnumspin",    &err);

    UIUtilE::Assign(this, m_searchArtistButton,     "searchartistbutton",     &err);
    UIUtilE::Assign(this, m_searchCompArtistButton, "searchcompartistbutton", &err);
    UIUtilE::Assign(this, m_searchAlbumButton,      "searchalbumbutton",      &err);
    UIUtilE::Assign(this, m_searchGenreButton,      "searchgenrebutton",      &err);

    UIUtilW::Assign(this, m_artistIcon,     "artisticon",     &err);
    UIUtilW::Assign(this, m_albumIcon,      "albumicon",      &err);
    UIUtilW::Assign(this, m_genreIcon,      "genreicon",      &err);

    UIUtilW::Assign(this, m_ratingState,      "ratingstate",      &err);
    UIUtilW::Assign(this, m_ratingSpin,       "ratingspin",       &err);

    UIUtilW::Assign(this, m_incRatingButton,  "incratingbutton",  &err);
    UIUtilW::Assign(this, m_decRatingButton,  "decratingbutton",  &err);

    UIUtilE::Assign(this, m_compilationCheck, "compilationcheck", &err);

    UIUtilE::Assign(this, m_albumartButton,   "albumartbutton", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'editmetadata'");
        return false;
    }

    m_yearSpin->SetRange(QDate::currentDate().year(), 1000, 1);
    m_yearSpin->AddSelection(0, "None");
    m_trackSpin->SetRange(0, 999, 1);

    if (m_discSpin)
        m_discSpin->SetRange(0, 999, 1);

    if (m_ratingSpin)
    {
        m_ratingSpin->SetRange(0, 10, 1, 2);
        connect(m_ratingSpin, &MythUIButtonList::itemSelected,
                this, &EditMetadataDialog::ratingSpinChanged);
    }

    connect(m_artistEdit, &MythUIType::LosingFocus, this, &EditMetadataDialog::artistLostFocus);
    connect(m_albumEdit, &MythUIType::LosingFocus, this, &EditMetadataDialog::albumLostFocus);
    connect(m_genreEdit, &MythUIType::LosingFocus, this, &EditMetadataDialog::genreLostFocus);

    connect(m_searchArtistButton, &MythUIButton::Clicked, this, &EditMetadataDialog::searchArtist);
    connect(m_searchCompArtistButton, &MythUIButton::Clicked, this, &EditMetadataDialog::searchCompilationArtist);
    connect(m_searchAlbumButton, &MythUIButton::Clicked, this, &EditMetadataDialog::searchAlbum);
    connect(m_searchGenreButton, &MythUIButton::Clicked, this, &EditMetadataDialog::searchGenre);

    if (m_incRatingButton && m_decRatingButton)
    {
        connect(m_incRatingButton, &MythUIButton::Clicked, this, &EditMetadataDialog::incRating);
        connect(m_decRatingButton, &MythUIButton::Clicked, this, &EditMetadataDialog::decRating);
    }

    connect(m_compilationCheck, &MythUICheckBox::toggled, this, &EditMetadataDialog::checkClicked);

    connect(m_albumartButton, &MythUIButton::Clicked, this, &EditMetadataDialog::switchToAlbumArt);

    fillWidgets();

    BuildFocusList();

    return true;
}

void EditMetadataDialog::fillWidgets()
{
    m_compArtistEdit->SetText(s_metadata->CompilationArtist());
    m_albumEdit->SetText(s_metadata->Album());
    m_artistEdit->SetText(s_metadata->Artist());
    m_genreEdit->SetText(s_metadata->Genre());
    m_titleEdit->SetText(s_metadata->Title());
    m_yearSpin->SetValue(s_metadata->Year());
    m_trackSpin->SetValue(s_metadata->Track());
    m_compilationCheck->SetCheckState(s_metadata->Compilation());

    if (m_discSpin)
        m_discSpin->SetValue(s_metadata->DiscNumber());

    updateRating();

    updateArtistImage();
    updateAlbumImage();
    updateGenreImage();
}

void EditMetadataDialog::incRating(void)
{
    s_metadata->incRating();
    updateRating();
}

void EditMetadataDialog::decRating(void)
{
    s_metadata->decRating();
    updateRating();
}

void EditMetadataDialog::ratingSpinChanged(MythUIButtonListItem *item)
{
    if (item)
    {
        int rating = item->GetData().value<int>();
        s_metadata->setRating(rating);

        if (m_ratingState)
            m_ratingState->DisplayState(QString("%1").arg(s_metadata->Rating()));
    }
}

void EditMetadataDialog::updateRating(void)
{
    if (m_ratingState)
        m_ratingState->DisplayState(QString("%1").arg(s_metadata->Rating()));

    if (m_ratingSpin)
        m_ratingSpin->SetValue(s_metadata->Rating());
}

bool EditMetadataDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "THMBUP")
            incRating();
        else if (action == "THMBDOWN")
            decRating();
        else if (action == "MENU")
            showMenu();
        else
            handled = false;
    }

    if (!handled && EditMetadataCommon::keyPressEvent(event))
        handled = true;

    return handled;
}

void EditMetadataDialog::showMenu(void )
{
    if (s_metadataOnly)
        return;

    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox(label, popupStack, "optionsmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "optionsmenu");

    menu->AddButton(tr("Edit Albumart Images"));
    menu->AddButton(tr("Search Internet For Artist Image"));
    menu->AddButton(tr("Search Internet For Album Image"));
    menu->AddButton(tr("Search Internet For Genre Image"));
    menu->AddButton(tr("Check Track Length"));

    popupStack->AddScreen(menu);
}

void EditMetadataDialog::switchToAlbumArt()
{
    updateMetadata();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *editDialog = new EditAlbumartDialog(mainStack);

    if (!editDialog->Create())
    {
        delete editDialog;
        return;
    }

    mainStack->AddScreen(editDialog);

    Close();
}

void EditMetadataDialog::checkClicked(bool state)
{
    if (!state)
    {
        m_compArtistEdit->SetText(s_metadata->Artist());
    }
    else
    {
        if (m_compArtistEdit->GetText().isEmpty() ||
            m_compArtistEdit->GetText() == m_artistEdit->GetText())
        {
            m_compArtistEdit->SetText(tr("Various Artists"));
        }
    }
}

void EditMetadataDialog::searchArtist() const
{
    QString msg = tr("Select an Artist");
    QStringList searchList = MusicMetadata::fillFieldList("artist");
    QString s = s_metadata->Artist();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &EditMetadataDialog::setArtist);

    popupStack->AddScreen(searchDlg);
}

void EditMetadataDialog::setArtist(const QString& artist)
{
    m_artistEdit->SetText(artist);
    updateArtistImage();
}

void EditMetadataDialog::updateArtistImage(void)
{
    QString artist =  m_artistEdit->GetText();

    if (m_artistIcon)
    {
        QString file = findIcon("artist", artist.toLower(), true);
        if (!file.isEmpty())
        {
            m_artistIcon->SetFilename(file);
            m_artistIcon->Load();
        }
        else
        {
            m_artistIcon->Reset();
        }
    }
}

void EditMetadataDialog::searchCompilationArtist() const
{
    QString msg = tr("Select a Compilation Artist");
    QStringList searchList = MusicMetadata::fillFieldList("compilation_artist");
    QString s = s_metadata->CompilationArtist();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &EditMetadataDialog::setCompArtist);

    popupStack->AddScreen(searchDlg);
}

void EditMetadataDialog::setCompArtist(const QString& compArtist)
{
    m_compArtistEdit->SetText(compArtist);
}

void EditMetadataDialog::searchAlbum() const
{
    QString msg = tr("Select an Album");
    QStringList searchList = MusicMetadata::fillFieldList("album");
    QString s = s_metadata->Album();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &EditMetadataDialog::setAlbum);

    popupStack->AddScreen(searchDlg);
}

void EditMetadataDialog::setAlbum(const QString& album)
{
    m_albumEdit->SetText(album);
    updateAlbumImage();
}

void EditMetadataDialog::updateAlbumImage(void)
{
    if (m_albumIcon)
    {
        QString file = s_metadata->getAlbumArtFile();
        if (!file.isEmpty())
        {
            m_albumIcon->SetFilename(file);
            m_albumIcon->Load();
        }
        else
        {
            m_albumIcon->Reset();
        }
    }
}

void EditMetadataDialog::searchGenre() const
{
    QString msg = tr("Select a Genre");
    QStringList searchList = MusicMetadata::fillFieldList("genre");
    // load genre list
    /*
    searchList.clear();
    for (const auto &genre : genre_table)
        searchList.push_back(QString::fromStdString(genre));
    searchList.sort();
    */

    QString s = s_metadata->Genre();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &EditMetadataDialog::setGenre);

    popupStack->AddScreen(searchDlg);
}

void EditMetadataDialog::setGenre(const QString& genre)
{
    m_genreEdit->SetText(genre);
    updateGenreImage();
}

void EditMetadataDialog::updateGenreImage(void)
{
    QString genre = m_genreEdit->GetText();

    if (m_genreIcon)
    {
        QString file = findIcon("genre", genre.toLower(), true);
        if (!file.isEmpty())
        {
            m_genreIcon->SetFilename(file);
            m_genreIcon->Load();
        }
        else
        {
            m_genreIcon->Reset();
        }
    }
}

void EditMetadataDialog::artistLostFocus(void)
{
    updateArtistImage();
}

void EditMetadataDialog::albumLostFocus(void)
{
    updateAlbumImage();
}

void EditMetadataDialog::genreLostFocus(void)
{
    updateGenreImage();
}

/// search flickr for genre images
void EditMetadataDialog::searchForGenreImages(void)
{
    QString genre= s_metadata->Genre().replace(' ', '+');
    genre = QUrl::toPercentEncoding(genre, "+");

    QUrl url("http://www.flickr.com/search/groups/?w=908425%40N22&m=pool&q=" + genre, QUrl::TolerantMode);

    m_searchType = "genre";
    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), GetConfDir() + "/MythMusic/", "genre.jpg");
}

/// search google for artist images
void EditMetadataDialog::searchForArtistImages(void)
{
    QString artist = s_metadata->Artist().replace(' ', '+');
    artist = QUrl::toPercentEncoding(artist, "+");

    QUrl url("http://www.google.co.uk/images?q=" + artist, QUrl::TolerantMode);

    m_searchType = "artist";
    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), GetConfDir() + "/MythMusic/", "artist.jpg");
}

void EditMetadataDialog::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);
        // make sure the user didn't ESCAPE out of the menu
        if ((dce == nullptr) || (dce->GetResult() < 0))
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();


        if (resultid == "optionsmenu")
        {
            if (resulttext == tr("Edit Albumart Images"))
                switchToAlbumArt();
            else if (resulttext == tr("Search Internet For Genre Image"))
            {
                updateMetadata();
                searchForGenreImages();
            }
            else if (resulttext == tr("Search Internet For Artist Image"))
            {
                updateMetadata();
                searchForArtistImages();
            }
            else if (resulttext == tr("Search Internet For Album Image"))
            {
                updateMetadata();
                searchForAlbumImages();
            }
            else if (resulttext == tr("Check Track Length"))
            {
                QStringList strList;
                strList << "MUSIC_CALC_TRACK_LENGTH"
                        << s_metadata->Hostname()
                        << QString::number(s_metadata->ID());

                auto *thread = new SendStringListThread(strList);
                MThreadPool::globalInstance()->start(thread, "Send MUSIC_CALC_TRACK_LENGTH");

                ShowOkPopup(tr("Asked the backend to check the tracks length"));
            }
        }
    }
    else if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;
        QStringList tokens = me->Message().split(" ", Qt::SkipEmptyParts);

        if (!tokens.isEmpty())
        {
            if (tokens[0] == "BROWSER_DOWNLOAD_FINISHED")
            {
                QStringList args = me->ExtraDataList();
                const QString& oldFilename = args[1];
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();

                if ((errorCode != 0) || (fileSize == 0))
                    return;

                QString newFilename;

                if (m_searchType == "artist")
                {
                    QString cleanName = fixFilename(s_metadata->Artist().toLower());
                    QString file = QString("Icons/%1/%2.jpg").arg("artist", cleanName);
                    newFilename = MythCoreContext::GenMythURL(gCoreContext->GetMasterHostName(),
                                                              0, file, "MusicArt");
                }
                else if (m_searchType == "genre")
                {
                    QString cleanName = fixFilename(s_metadata->Genre().toLower());
                    QString file = QString("Icons/%1/%2.jpg").arg("genre", cleanName);
                    newFilename = MythCoreContext::GenMythURL(gCoreContext->GetMasterHostName(),
                                                              0, file, "MusicArt");
                }
                else if (m_searchType == "album")
                {
                    // move the image from the MythMusic config dir to the tracks
                    // dir in the 'Music' storage group
                    newFilename = s_metadata->Filename();
                    newFilename = newFilename.section( '/', 0, -2);
                    newFilename = newFilename + '/' +  oldFilename.section( '/', -1, -1);
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Got unknown search type '%1' "
                                                     "in BROWSER_DOWNLOAD_FINISHED event")
                                                     .arg(m_searchType));
                    return;
                }

                RemoteFile::CopyFile(oldFilename, newFilename, true);
                QFile::remove(oldFilename);

                if (m_searchType == "album")
                    scanForImages();

                // force the icons to update
                updateAlbumImage();
                updateArtistImage();
                updateGenreImage();

                s_metadata->getAlbumArtImages()->dumpToDatabase();
                // force a reload of the images for any tracks affected
                MetadataPtrList *allMusic =  gMusicData->m_all_music->getAllMetadata();
                for (int x = 0; x < allMusic->count(); x++)
                {
                    if ((allMusic->at(x)->ID() == s_sourceMetadata->ID()) ||
                        (allMusic->at(x)->getDirectoryId() == s_sourceMetadata->getDirectoryId()))
                    {
                        allMusic->at(x)->reloadAlbumArtImages();
                        gPlayer->sendAlbumArtChangedEvent(allMusic->at(x)->ID());
                    }
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// EditAlbumartDialog

EditAlbumartDialog::EditAlbumartDialog(MythScreenStack *parent)
    : EditMetadataCommon(parent, "EditAlbumartDialog")
{
    gCoreContext->addListener(this);
}

EditAlbumartDialog::~EditAlbumartDialog()
{
    gCoreContext->removeListener(this);
}

bool EditAlbumartDialog::Create(void)
{
    if (! LoadWindowFromXML("music-ui.xml", "editalbumart", this))
        return false;

    bool err = CreateCommon();

    UIUtilE::Assign(this, m_coverartList,      "coverartlist",      &err);
    UIUtilE::Assign(this, m_imagetypeText,     "imagetypetext",     &err);
    UIUtilE::Assign(this, m_imagefilenameText, "imagefilenametext", &err);
    UIUtilE::Assign(this, m_coverartImage,     "coverartimage",     &err);

    UIUtilE::Assign(this, m_metadataButton,    "metadatabutton",    &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'editalbumart'");
        return false;
    }

    updateImageGrid();

    connect(m_coverartList, &MythUIButtonList::itemSelected,
            this, &EditAlbumartDialog::gridItemChanged);

    connect(m_metadataButton, &MythUIButton::Clicked, this, &EditAlbumartDialog::switchToMetadata);

    BuildFocusList();

    return true;
}

void EditAlbumartDialog::gridItemChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_coverartImage)
    {
        auto *image = item->GetData().value<AlbumArtImage*>();
        if (image)
        {
            m_coverartImage->SetFilename(image->m_filename);
            m_coverartImage->Load();
            if (m_imagetypeText)
                m_imagetypeText->SetText(AlbumArtImages::getTypeName(image->m_imageType));
            if (m_imagefilenameText)
            {
                QFileInfo fi(image->m_filename);
                m_imagefilenameText->SetText(fi.fileName());
            }
        }
    }
}

void EditAlbumartDialog::updateImageGrid(void)
{
    AlbumArtList *albumArtList = s_metadata->getAlbumArtImages()->getImageList();

    m_coverartList->Reset();

    for (auto *art : std::as_const(*albumArtList))
    {
        auto *item = new MythUIButtonListItem(m_coverartList,
                                     AlbumArtImages::getTypeName(art->m_imageType),
                                     QVariant::fromValue(art));
        item->SetImage(art->m_filename);
        QString state = art->m_embedded ? "tag" : "file";
        item->DisplayState(state, "locationstate");
    }
}

bool EditAlbumartDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
            showMenu();
        else if (action == "INFO")
            showTypeMenu();
        else
            handled = false;
    }

    if (!handled && EditMetadataCommon::keyPressEvent(event))
        handled = true;

    return handled;
}

void EditAlbumartDialog::switchToMetadata(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *editDialog = new EditMetadataDialog(mainStack);

    if (!editDialog->Create())
    {
        delete editDialog;
        return;
    }

    mainStack->AddScreen(editDialog);

    Close();
}

void EditAlbumartDialog::showTypeMenu(bool changeType)
{
    if (changeType && m_coverartList->GetCount() == 0)
        return;

    QString label;

    if (changeType)
        label = tr("Change Image Type");
    else
        label = tr("What image type do you want to use for this image?");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox(label, popupStack, "typemenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    ImageType imageType = IT_UNKNOWN;
    if (changeType)
        menu->SetReturnEvent(this, "changetypemenu");
    else
    {
        menu->SetReturnEvent(this, "asktypemenu");
        imageType = AlbumArtImages::guessImageType(m_imageFilename);
    }

    menu->AddButtonV(AlbumArtImages::getTypeName(IT_UNKNOWN),    QVariant::fromValue((int)IT_UNKNOWN),    false, (imageType == IT_UNKNOWN));
    menu->AddButtonV(AlbumArtImages::getTypeName(IT_FRONTCOVER), QVariant::fromValue((int)IT_FRONTCOVER), false, (imageType == IT_FRONTCOVER));
    menu->AddButtonV(AlbumArtImages::getTypeName(IT_BACKCOVER),  QVariant::fromValue((int)IT_BACKCOVER),  false, (imageType == IT_BACKCOVER));
    menu->AddButtonV(AlbumArtImages::getTypeName(IT_CD),         QVariant::fromValue((int)IT_CD),         false, (imageType == IT_CD));
    menu->AddButtonV(AlbumArtImages::getTypeName(IT_INLAY),      QVariant::fromValue((int)IT_INLAY),      false, (imageType == IT_INLAY));
    menu->AddButtonV(AlbumArtImages::getTypeName(IT_ARTIST),     QVariant::fromValue((int)IT_ARTIST),     false, (imageType == IT_ARTIST));

    popupStack->AddScreen(menu);
}

void EditAlbumartDialog::showMenu(void )
{
    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox(label, popupStack, "optionsmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "optionsmenu");

    menu->AddButton(tr("Edit Metadata"));
    menu->AddButton(tr("Rescan For Images"));


    menu->AddButton(tr("Search Internet For Images"));

    MetaIO *tagger = MetaIO::createTagger(s_metadata->Filename(false));

    if (m_coverartList->GetItemCurrent())
    {
        menu->AddButton(tr("Change Image Type"), nullptr, true);

        if (GetMythDB()->GetBoolSetting("AllowTagWriting", false))
        {
            MythUIButtonListItem *item = m_coverartList->GetItemCurrent();
            if (item)
            {
                auto *image = item->GetData().value<AlbumArtImage*>();
                if (image)
                {
                    if (!image->m_embedded)
                    {
                        if (tagger && tagger->supportsEmbeddedImages())
                            menu->AddButton(tr("Copy Selected Image To Tag"));
                    }
                    else
                    {
                        if (tagger && tagger->supportsEmbeddedImages())
                            menu->AddButton(tr("Remove Selected Image From Tag"));
                    }
                }
            }
        }
    }

    if (GetMythDB()->GetBoolSetting("AllowTagWriting", false))
    {
        if (tagger && tagger->supportsEmbeddedImages())
            menu->AddButton(tr("Copy Image To Tag"));
    }

    delete tagger;

    popupStack->AddScreen(menu);
}

void EditAlbumartDialog::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);
        // make sure the user didn't ESCAPE out of the menu
        if ((dce == nullptr) || (dce->GetResult() < 0))
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "changetypemenu")
        {
            int type = dce->GetData().toInt();

            if ((type >= IT_UNKNOWN) && (type < IT_LAST))
            {
                // get selected image in list
                MythUIButtonListItem *item = m_coverartList->GetItemCurrent();
                if (item)
                {
                    item->SetText(AlbumArtImages::getTypeName((ImageType) type));
                    auto *image = item->GetData().value<AlbumArtImage*>();
                    if (image)
                    {
                        QStringList strList("MUSIC_TAG_CHANGEIMAGE");
                        strList << s_metadata->Hostname()
                                << QString::number(s_metadata->ID())
                                << QString::number(image->m_imageType)
                                << QString::number(type);

                        gCoreContext->SendReceiveStringList(strList);

                        m_albumArtChanged = true;

                        gridItemChanged(item);
                    }
                }
            }
        }
        else if (resultid == "asktypemenu")
        {
            int type = dce->GetData().toInt();

            if ((type >= IT_UNKNOWN) && (type < IT_LAST))
                copyImageToTag((ImageType) type);
        }
        else if (resultid == "optionsmenu")
        {
            if (resulttext == tr("Edit Metadata"))
                switchToMetadata();
            else if (resulttext == tr("Rescan For Images"))
                rescanForImages();
            else if (resulttext == tr("Search Internet For Images"))
                searchForAlbumImages();
            else if (resulttext == tr("Change Image Type"))
                showTypeMenu();
            else if (resulttext == tr("Copy Selected Image To Tag"))
                copySelectedImageToTag();
            else if (resulttext == tr("Remove Selected Image From Tag"))
                removeSelectedImageFromTag();
            else if (resulttext == tr("Copy Image To Tag"))
                startCopyImageToTag();
        }
        else if (resultid == "imagelocation")
        {
            m_imageFilename = resulttext;

            // save directory location for next time
            QFileInfo fi(m_imageFilename);
            gCoreContext->SaveSetting("MusicLastImageLocation", fi.canonicalPath());

            showTypeMenu(false);
        }
    }
    else if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;
        QStringList tokens = me->Message().split(" ", Qt::SkipEmptyParts);

        if (!tokens.isEmpty())
        {
            if (tokens[0] == "BROWSER_DOWNLOAD_FINISHED")
                rescanForImages();
            else if (tokens[0] == "MUSIC_ALBUMART_CHANGED")
            {
                if (tokens.size() >= 2)
                {
                    auto songID = (MusicMetadata::IdType)tokens[1].toInt();

                    if (s_metadata->ID() == songID)
                    {
                        // force all the image to reload
                        for (uint x = 0; x < s_metadata->getAlbumArtImages()->getImageCount(); x++)
                            removeCachedImage(s_metadata->getAlbumArtImages()->getImageAt(x));

                        updateImageGrid();
                    }
                }
            }
        }
    }
}

/// search the tracks tag and the tracks directory for images
void EditAlbumartDialog::rescanForImages(void)
{
    // scan the tracks directory and tag for any images
    scanForImages();

    updateImageGrid();

    AlbumArtImages *albumArt = s_metadata->getAlbumArtImages();
    if (albumArt->getImageCount() > 0)
        m_albumArtChanged = true;
}

void EditAlbumartDialog::startCopyImageToTag(void)
{
    QString lastLocation = gCoreContext->GetSetting("MusicLastImageLocation", "/");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *fb = new MythUIFileBrowser(popupStack, lastLocation);

    fb->SetTypeFilter(QDir::AllDirs | QDir::Files | QDir::Readable);

    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.gif";
    fb->SetNameFilter(filters);

    if (fb->Create())
    {
        fb->SetReturnEvent(this, "imagelocation");
        popupStack->AddScreen(fb);
    }
    else
    {
        delete fb;
    }
}

void EditAlbumartDialog::copyImageToTag(ImageType imageType)
{
    AlbumArtImage image;
    image.m_filename = m_imageFilename;
    image.m_imageType = imageType;

    doCopyImageToTag(&image);
}

void EditAlbumartDialog::copySelectedImageToTag(void)
{
    MythUIButtonListItem *item = m_coverartList->GetItemCurrent();
    if (item)
    {
        auto *image = item->GetData().value<AlbumArtImage*>();
        if (image)
            doCopyImageToTag(image);
    }
}

void EditAlbumartDialog::removeSelectedImageFromTag(void)
{
    MythUIButtonListItem *item = m_coverartList->GetItemCurrent();
    if (item)
    {
        auto *image = item->GetData().value<AlbumArtImage*>();
        if (image)
        {
            QString msg = tr("Are you sure you want to permanently remove this image from the tag?");
            ShowOkPopup(msg, this, &EditAlbumartDialog::doRemoveImageFromTag, true);
        }
    }
}

void EditAlbumartDialog::doRemoveImageFromTag(bool doIt)
{
    if (!doIt)
        return;

    MythUIButtonListItem *item = m_coverartList->GetItemCurrent();
    if (item)
    {
        auto *image = item->GetData().value<AlbumArtImage*>();
        if (image)
        {
            // ask the backend to remove the image from the tracks tag
            QStringList strList("MUSIC_TAG_REMOVEIMAGE");
            strList << s_metadata->Hostname()
                    << QString::number(s_metadata->ID())
                    << QString::number(image->m_id);

            gCoreContext->SendReceiveStringList(strList);

            removeCachedImage(image);
            rescanForImages();
        }
    }
}

class CopyImageThread: public MThread
{
  public:
    explicit CopyImageThread(QStringList strList) :
            MThread("CopyImage"), m_strList(std::move(strList)) {}

    void run() override // MThread
    {
        RunProlog();
        gCoreContext->SendReceiveStringList(m_strList);
        RunEpilog();
    }

    QStringList getResult(void) { return m_strList; }

  private:
    QStringList m_strList;
};

void EditAlbumartDialog::doCopyImageToTag(const AlbumArtImage *image)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *busy = new MythUIBusyDialog(tr("Copying image to tag..."),
                                      popupStack, "copyimagebusydialog");

    if (busy->Create())
    {
        popupStack->AddScreen(busy, false);
    }
    else
    {
        delete busy;
        busy = nullptr;
    }

    // copy the image to the tracks host
    QFileInfo fi(image->m_filename);
    QString saveFilename = MythCoreContext::GenMythURL(s_metadata->Hostname(), 0,
                                                       QString("AlbumArt/") + fi.fileName(),
                                                       "MusicArt");

    RemoteFile::CopyFile(image->m_filename, saveFilename, true);

    // ask the backend to add the image to the tracks tag
    QStringList strList("MUSIC_TAG_ADDIMAGE");
    strList << s_metadata->Hostname()
            << QString::number(s_metadata->ID())
            << fi.fileName()
            << QString::number(image->m_imageType);

    auto *copyThread = new CopyImageThread(strList);
    copyThread->start();

    while (copyThread->isRunning())
    {
        qApp->processEvents();
        const struct timespec onems {0, 1000000};
        nanosleep(&onems, nullptr);
    }

    strList = copyThread->getResult();

    delete copyThread;

    if (busy)
        busy->Close();

    removeCachedImage(image);

    rescanForImages();
}

void EditAlbumartDialog::removeCachedImage(const AlbumArtImage *image)
{
    if (!image->m_embedded)
        return;

    GetMythUI()->RemoveFromCacheByFile(image->m_filename);
}
