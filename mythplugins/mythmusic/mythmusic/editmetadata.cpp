// qt
#include <QKeyEvent>

// mythtv
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythuihelper.h>
#include <mythdirs.h>
#include <mythdialogbox.h>
#include <mythuitextedit.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuicheckbox.h>
#include <mythuistatetype.h>
#include <mythuibuttonlist.h>
#include <mythuispinbox.h>
#include <mythuiimage.h>
#include <mythuiwebbrowser.h>
#include <mythuifilebrowser.h>
#include <musicutils.h>
#include <mythprogressdialog.h>
#include <remotefile.h>

// mythmusic
#include "musicdata.h"
#include "decoder.h"
#include "genres.h"
#include "metaio.h"
#include "musicplayer.h"


#include "editmetadata.h"

// these need to be static so both screens can pick them up
bool EditMetadataCommon::metadataOnly = false;
MusicMetadata *EditMetadataCommon::m_metadata = NULL;
MusicMetadata *EditMetadataCommon::m_sourceMetadata = NULL;

EditMetadataCommon::EditMetadataCommon(MythScreenStack *parent,
                                       MusicMetadata *source_metadata,
                                       const QString &name) :
    MythScreenType(parent, name), m_albumArtChanged(false), m_doneButton(NULL)
{
    // make a copy so we can abandon changes
    m_metadata = new MusicMetadata(*source_metadata);
    m_sourceMetadata = source_metadata;

    metadataOnly = false;
}

EditMetadataCommon::EditMetadataCommon(MythScreenStack *parent,
                                       const QString &name) :
    MythScreenType(parent, name), m_albumArtChanged(false), m_doneButton(NULL)
{
}

EditMetadataCommon::~EditMetadataCommon()
{
        // do we need to save anything?
    if (m_albumArtChanged)
    {
        m_metadata->getAlbumArtImages()->dumpToDatabase();

        // force a reload of the images for any tracks affected
        MetadataPtrList *allMusic =  gMusicData->all_music->getAllMetadata();
        for (int x = 0; x < allMusic->count(); x++)
        {
            if ((allMusic->at(x)->ID() == m_sourceMetadata->ID()) ||
                (allMusic->at(x)->getDirectoryId() == m_sourceMetadata->getDirectoryId()))
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

    connect(m_doneButton, SIGNAL(Clicked()), SLOT(showSaveMenu()));

    return err;
}

bool EditMetadataCommon::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
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
    MythUITextEdit *edit = NULL;

    edit = dynamic_cast<MythUITextEdit *>(GetChild("albumedit"));
    if (edit)
        m_metadata->setAlbum(edit->GetText());

    edit = dynamic_cast<MythUITextEdit *>(GetChild("artistedit"));
    if (edit)
        m_metadata->setArtist(edit->GetText());

    edit = dynamic_cast<MythUITextEdit *>(GetChild("compartistedit"));
    if (edit)
        m_metadata->setCompilationArtist(edit->GetText());

    edit = dynamic_cast<MythUITextEdit *>(GetChild("titleedit"));
    if (edit)
        m_metadata->setTitle(edit->GetText());

    edit = dynamic_cast<MythUITextEdit *>(GetChild("genreedit"));
    if (edit)
        m_metadata->setGenre(edit->GetText());

    MythUISpinBox *spin = dynamic_cast<MythUISpinBox *>(GetChild("yearspin"));
    if (spin)
        m_metadata->setYear(spin->GetIntValue());

    spin = dynamic_cast<MythUISpinBox *>(GetChild("tracknumspin"));
    if (spin)
        m_metadata->setTrack(spin->GetIntValue());

    spin = dynamic_cast<MythUISpinBox *>(GetChild("ratingspin"));
    if (spin)
        m_metadata->setRating(spin->GetIntValue());

    MythUICheckBox *check = dynamic_cast<MythUICheckBox *>(GetChild("compilationcheck"));
    if (check)
        m_metadata->setCompilation(check->GetBooleanCheckState());
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

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "savechangesmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "savechangesmenu");

    if (metadataOnly)
        menu->AddButton(tr("Save Changes"), SLOT(saveToMetadata()));
    else
        menu->AddButton(tr("Save Changes"), SLOT(saveAll()));

    menu->AddButton(tr("Exit/Do Not Save"), SLOT(cleanupAndClose()));

    popupStack->AddScreen(menu);
}

void EditMetadataCommon::cleanupAndClose(void)
{
    if (m_metadata)
    {
        delete m_metadata;
        m_metadata = NULL;
    }

    Close();
}

void EditMetadataCommon::saveToMetadata()
{
    *m_sourceMetadata = *m_metadata;
    emit metadataChanged();
    cleanupAndClose();
}

void EditMetadataCommon::saveToDatabase()
{
    m_metadata->setAlbumId(-1);
    m_metadata->setGenreId(-1);
    m_metadata->setArtistId(-1);

    m_metadata->dumpToDatabase();
    *m_sourceMetadata = *m_metadata;

    gPlayer->sendMetadataChangedEvent(m_sourceMetadata->ID());
}

void EditMetadataCommon::saveAll()
{
    saveToDatabase();

    // only write to the tag if it's enabled by the user
    if (GetMythDB()->GetNumSetting("AllowTagWriting", 0))
    {
        QStringList strList;
        strList << "MUSIC_TAG_UPDATE_METADATA %1 %2"
                << m_metadata->Hostname()
                << QString::number(m_metadata->ID());

        SendStringListThread *thread = new SendStringListThread(strList);
        MThreadPool::globalInstance()->start(thread, "UpdateMetadata");
    }

    cleanupAndClose();
}

void EditMetadataCommon::setSaveMetadataOnly(void)
{
    metadataOnly = true;

    MythUIButton *albumartButton = dynamic_cast<MythUIButton *>(GetChild("albumartbutton"));
    if (albumartButton)
        albumartButton->Hide();
}

bool EditMetadataCommon::hasMetadataChanged(void)
{
    bool changed = false;

    changed |= (m_metadata->Album() != m_sourceMetadata->Album());
    changed |= (m_metadata->Artist() != m_sourceMetadata->Artist());
    changed |= (m_metadata->CompilationArtist() != m_sourceMetadata->CompilationArtist());
    changed |= (m_metadata->Title() != m_sourceMetadata->Title());
    changed |= (m_metadata->Genre() != m_sourceMetadata->Genre());
    changed |= (m_metadata->Year() != m_sourceMetadata->Year());
    changed |= (m_metadata->Track() != m_sourceMetadata->Track());
    changed |= (m_metadata->Compilation() != m_sourceMetadata->Compilation());
    changed |= (m_metadata->Rating() != m_sourceMetadata->Rating());

    return changed;
}

/// search Google for images
void EditMetadataCommon::searchForAlbumImages(void)
{
    QString artist = m_metadata->Artist().replace(' ', '+');
    artist = QUrl::toPercentEncoding(artist, "+");

    QString album = m_metadata->Album().replace(' ', '+');
    album = QUrl::toPercentEncoding(album, "+");

    QUrl url("http://www.google.co.uk/images?q=" + artist + "+" + album, QUrl::TolerantMode);

    m_searchType = "album";

    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), GetConfDir() + "/MythMusic/", "front.jpg");
}

void EditMetadataCommon::scanForImages(void)
{
    m_metadata->getAlbumArtImages()->scanForImages();
}

///////////////////////////////////////////////////////////////////////////////
// EditMatadataDialog

EditMetadataDialog::EditMetadataDialog(MythScreenStack *parent, MusicMetadata *source_metadata)
                  : EditMetadataCommon(parent, source_metadata, "EditMetadataDialog"),
    m_artistEdit(NULL),             m_compArtistEdit(NULL),
    m_albumEdit(NULL),              m_titleEdit(NULL),
    m_genreEdit(NULL),              m_yearSpin(NULL),
    m_trackSpin(NULL),              m_ratingSpin(NULL),
    m_ratingState(NULL),            m_incRatingButton(NULL),
    m_decRatingButton(NULL),        m_searchArtistButton(NULL),
    m_searchCompArtistButton(NULL), m_searchAlbumButton(NULL),
    m_searchGenreButton(NULL),      m_artistIcon(NULL),
    m_albumIcon(NULL),              m_genreIcon(NULL),
    m_compilationCheck(NULL),       m_albumartButton(NULL)
{
    gCoreContext->addListener(this);
}

EditMetadataDialog::EditMetadataDialog(MythScreenStack *parent)
                  : EditMetadataCommon(parent, "EditMetadataDialog"),
    m_artistEdit(NULL),             m_compArtistEdit(NULL),
    m_albumEdit(NULL),              m_titleEdit(NULL),
    m_genreEdit(NULL),              m_yearSpin(NULL),
    m_trackSpin(NULL),              m_ratingSpin(NULL),
    m_ratingState(NULL),            m_incRatingButton(NULL),
    m_decRatingButton(NULL),        m_searchArtistButton(NULL),
    m_searchCompArtistButton(NULL), m_searchAlbumButton(NULL),
    m_searchGenreButton(NULL),      m_artistIcon(NULL),
    m_albumIcon(NULL),              m_genreIcon(NULL),
    m_compilationCheck(NULL),       m_albumartButton(NULL)
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

    if (m_ratingSpin)
    {
        m_ratingSpin->SetRange(0, 10, 1, 2);
        connect(m_ratingSpin, SIGNAL(itemSelected(MythUIButtonListItem*)),
                SLOT(ratingSpinChanged(MythUIButtonListItem*)));
    }

    connect(m_artistEdit, SIGNAL(LosingFocus()), SLOT(artistLostFocus()));
    connect(m_albumEdit, SIGNAL(LosingFocus()), SLOT(albumLostFocus()));
    connect(m_genreEdit, SIGNAL(LosingFocus()), SLOT(genreLostFocus()));

    connect(m_searchArtistButton, SIGNAL(Clicked()), SLOT(searchArtist()));
    connect(m_searchCompArtistButton, SIGNAL(Clicked()), SLOT(searchCompilationArtist()));
    connect(m_searchAlbumButton, SIGNAL(Clicked()), SLOT(searchAlbum()));
    connect(m_searchGenreButton, SIGNAL(Clicked()), SLOT(searchGenre()));

    if (m_incRatingButton && m_decRatingButton)
    {
        connect(m_incRatingButton, SIGNAL(Clicked()), SLOT(incRating()));
        connect(m_decRatingButton, SIGNAL(Clicked()), SLOT(decRating()));
    }

    connect(m_compilationCheck, SIGNAL(toggled(bool)), SLOT(checkClicked(bool)));

    connect(m_albumartButton, SIGNAL(Clicked()), SLOT(switchToAlbumArt()));

    fillWidgets();

    BuildFocusList();

    return true;
}

void EditMetadataDialog::fillWidgets()
{
    m_compArtistEdit->SetText(m_metadata->CompilationArtist());
    m_albumEdit->SetText(m_metadata->Album());
    m_artistEdit->SetText(m_metadata->Artist());
    m_genreEdit->SetText(m_metadata->Genre());
    m_titleEdit->SetText(m_metadata->Title());
    m_yearSpin->SetValue(m_metadata->Year());
    m_trackSpin->SetValue(m_metadata->Track());
    m_compilationCheck->SetCheckState(m_metadata->Compilation());

    updateRating();

    updateArtistImage();
    updateAlbumImage();
    updateGenreImage();
}

void EditMetadataDialog::incRating(void)
{
    m_metadata->incRating();
    updateRating();
}

void EditMetadataDialog::decRating(void)
{
    m_metadata->decRating();
    updateRating();
}

void EditMetadataDialog::ratingSpinChanged(MythUIButtonListItem *item)
{
    if (item)
    {
        int rating = qVariantValue<int>(item->GetData());
        m_metadata->setRating(rating);

        if (m_ratingState)
            m_ratingState->DisplayState(QString("%1").arg(m_metadata->Rating()));
    }
}

void EditMetadataDialog::updateRating(void)
{
    if (m_ratingState)
        m_ratingState->DisplayState(QString("%1").arg(m_metadata->Rating()));

    if (m_ratingSpin)
        m_ratingSpin->SetValue(m_metadata->Rating());
}

bool EditMetadataDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
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
    if (metadataOnly)
        return;

    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "optionsmenu");

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

    EditAlbumartDialog *editDialog = new EditAlbumartDialog(mainStack);

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
        m_compArtistEdit->SetText(m_metadata->Artist());
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

void EditMetadataDialog::searchArtist()
{
    QString msg = tr("Select an Artist");
    QStringList searchList = MusicMetadata::fillFieldList("artist");
    QString s = m_metadata->Artist();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUISearchDialog *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, SIGNAL(haveResult(QString)), SLOT(setArtist(QString)));

    popupStack->AddScreen(searchDlg);
}

void EditMetadataDialog::setArtist(QString artist)
{
    m_artistEdit->SetText(artist);
    updateArtistImage();
}

void EditMetadataDialog::updateArtistImage(void)
{
    QString artist =  m_artistEdit->GetText();

    QString file;

    if (m_artistIcon)
    {
        file = findIcon("artist", artist.toLower(), true);
        if (!file.isEmpty())
        {
            m_artistIcon->SetFilename(file);
            m_artistIcon->Load();
        }
        else
            m_artistIcon->Reset();
    }
}

void EditMetadataDialog::searchCompilationArtist()
{
    QString msg = tr("Select a Compilation Artist");
    QStringList searchList = MusicMetadata::fillFieldList("compilation_artist");
    QString s = m_metadata->CompilationArtist();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUISearchDialog *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, SIGNAL(haveResult(QString)), SLOT(setCompArtist(QString)));

    popupStack->AddScreen(searchDlg);
}

void EditMetadataDialog::setCompArtist(QString compArtist)
{
    m_compArtistEdit->SetText(compArtist);
}

void EditMetadataDialog::searchAlbum()
{
    QString msg = tr("Select an Album");
    QStringList searchList = MusicMetadata::fillFieldList("album");
    QString s = m_metadata->Album();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUISearchDialog *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, SIGNAL(haveResult(QString)), SLOT(setAlbum(QString)));

    popupStack->AddScreen(searchDlg);
}

void EditMetadataDialog::setAlbum(QString album)
{
    m_albumEdit->SetText(album);
    updateAlbumImage();
}

void EditMetadataDialog::updateAlbumImage(void)
{
    QString file;

    if (m_albumIcon)
    {
        file = m_metadata->getAlbumArtFile();
        if (!file.isEmpty())
        {
            m_albumIcon->SetFilename(file);
            m_albumIcon->Load();
        }
        else
            m_albumIcon->Reset();
    }
}

void EditMetadataDialog::searchGenre()
{
    QString msg = tr("Select a Genre");
    QStringList searchList = MusicMetadata::fillFieldList("genre");
    // load genre list
    /*
    searchList.clear();
    for (int x = 0; x < genre_table_size; x++)
        searchList.push_back(QString(genre_table[x]));
    searchList.sort();
    */

    QString s = m_metadata->Genre();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUISearchDialog *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, s);

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, SIGNAL(haveResult(QString)), SLOT(setGenre(QString)));

    popupStack->AddScreen(searchDlg);
}

void EditMetadataDialog::setGenre(QString genre)
{
    m_genreEdit->SetText(genre);
    updateGenreImage();
}

void EditMetadataDialog::updateGenreImage(void)
{
    QString genre = m_genreEdit->GetText();
    QString file;

    if (m_genreIcon)
    {
        file = findIcon("genre", genre.toLower(), true);
        if (!file.isEmpty())
        {
            m_genreIcon->SetFilename(file);
            m_genreIcon->Load();
        }
        else
            m_genreIcon->Reset();
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
    QString genre= m_metadata->Genre().replace(' ', '+');
    genre = QUrl::toPercentEncoding(genre, "+");

    QUrl url("http://www.flickr.com/search/groups/?w=908425%40N22&m=pool&q=" + genre, QUrl::TolerantMode);

    m_searchType = "genre";
    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), GetConfDir() + "/MythMusic/", "genre.jpg");
}

/// search google for artist images
void EditMetadataDialog::searchForArtistImages(void)
{
    QString artist = m_metadata->Artist().replace(' ', '+');
    artist = QUrl::toPercentEncoding(artist, "+");

    QUrl url("http://www.google.co.uk/images?q=" + artist, QUrl::TolerantMode);

    m_searchType = "artist";
    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), GetConfDir() + "/MythMusic/", "artist.jpg");
}

void EditMetadataDialog::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        // make sure the user didn't ESCAPE out of the menu
        if (dce->GetResult() < 0)
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
                        << m_metadata->Hostname()
                        << QString::number(m_metadata->ID());

                SendStringListThread *thread = new SendStringListThread(strList);
                MThreadPool::globalInstance()->start(thread, "Send MUSIC_CALC_TRACK_LENGTH");

                ShowOkPopup(tr("Asked the backend to check the tracks length"));
            }
        }
    }
    else if (event->type() == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QStringList tokens = me->Message().split(" ", QString::SkipEmptyParts);

        if (!tokens.isEmpty())
        {
            if (tokens[0] == "BROWSER_DOWNLOAD_FINISHED")
            {
                QStringList args = me->ExtraDataList();
                QString oldFilename = args[1];
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();

                if ((errorCode != 0) || (fileSize == 0))
                    return;

                QString newFilename;

                if (m_searchType == "artist")
                {
                    QString cleanName = fixFilename(m_metadata->Artist().toLower());
                    QString file = QString("Icons/%1/%2.jpg").arg("artist").arg(cleanName);
                    newFilename = gCoreContext->GenMythURL(gCoreContext->GetMasterHostName(),
                                                           0, file, "MusicArt");
                }
                else if (m_searchType == "genre")
                {
                    QString cleanName = fixFilename(m_metadata->Genre().toLower());
                    QString file = QString("Icons/%1/%2.jpg").arg("genre").arg(cleanName);
                    newFilename = gCoreContext->GenMythURL(gCoreContext->GetMasterHostName(),
                                                           0, file, "MusicArt");
                }
                else if (m_searchType == "album")
                {
                    // move the image from the MythMusic config dir to the tracks
                    // dir in the 'Music' storage group
                    newFilename = m_metadata->Filename();
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

                RemoteFile::CopyFile(oldFilename, newFilename);
                QFile::remove(oldFilename);

                if (m_searchType == "album")
                    scanForImages();

                // force the icons to update
                updateAlbumImage();
                updateArtistImage();
                updateGenreImage();

                m_metadata->getAlbumArtImages()->dumpToDatabase();
                // force a reload of the images for any tracks affected
                MetadataPtrList *allMusic =  gMusicData->all_music->getAllMetadata();
                for (int x = 0; x < allMusic->count(); x++)
                {
                    if ((allMusic->at(x)->ID() == m_sourceMetadata->ID()) ||
                        (allMusic->at(x)->getDirectoryId() == m_sourceMetadata->getDirectoryId()))
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
                  : EditMetadataCommon(parent, "EditAlbumartDialog"),
    m_metadataButton(NULL),
    m_doneButton(NULL),          m_coverartImage(NULL),
    m_coverartList(NULL),        m_imagetypeText(NULL),
    m_imagefilenameText(NULL)
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

    connect(m_coverartList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(gridItemChanged(MythUIButtonListItem*)));

    connect(m_metadataButton, SIGNAL(Clicked()), SLOT(switchToMetadata()));

    BuildFocusList();

    return true;
}

void EditAlbumartDialog::gridItemChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_coverartImage)
    {
        AlbumArtImage *image = qVariantValue<AlbumArtImage*> (item->GetData());
        if (image)
        {
            m_coverartImage->SetFilename(image->filename);
            m_coverartImage->Load();
            if (m_imagetypeText)
                m_imagetypeText->SetText(AlbumArtImages::getTypeName(image->imageType));
            if (m_imagefilenameText)
            {
                QFileInfo fi(image->filename);
                m_imagefilenameText->SetText(fi.fileName());
            }
        }
    }
}

void EditAlbumartDialog::updateImageGrid(void)
{
    AlbumArtList *albumArtList = m_metadata->getAlbumArtImages()->getImageList();

    m_coverartList->Reset();

    for (int x = 0; x < albumArtList->size(); x++)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_coverartList,
                                     AlbumArtImages::getTypeName(albumArtList->at(x)->imageType),
                                     qVariantFromValue(albumArtList->at(x)));
        item->SetImage(albumArtList->at(x)->filename);
        QString state = albumArtList->at(x)->embedded ? "tag" : "file";
        item->DisplayState(state, "locationstate");
    }
}

bool EditAlbumartDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            showMenu();
        else if (action == "INFO")
            showTypeMenu();
        else if (action == "ESCAPE")
            showSaveMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void EditAlbumartDialog::switchToMetadata(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    EditMetadataDialog *editDialog = new EditMetadataDialog(mainStack);

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

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "typemenu");

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

    AlbumArtImages *albumArt = m_metadata->getAlbumArtImages();

    menu->AddButton(albumArt->getTypeName(IT_UNKNOWN),    qVariantFromValue((int)IT_UNKNOWN),    false, (imageType == IT_UNKNOWN));
    menu->AddButton(albumArt->getTypeName(IT_FRONTCOVER), qVariantFromValue((int)IT_FRONTCOVER), false, (imageType == IT_FRONTCOVER));
    menu->AddButton(albumArt->getTypeName(IT_BACKCOVER),  qVariantFromValue((int)IT_BACKCOVER),  false, (imageType == IT_BACKCOVER));
    menu->AddButton(albumArt->getTypeName(IT_CD),         qVariantFromValue((int)IT_CD),         false, (imageType == IT_CD));
    menu->AddButton(albumArt->getTypeName(IT_INLAY),      qVariantFromValue((int)IT_INLAY),      false, (imageType == IT_INLAY));
    menu->AddButton(albumArt->getTypeName(IT_ARTIST),     qVariantFromValue((int)IT_ARTIST),     false, (imageType == IT_ARTIST));

    popupStack->AddScreen(menu);
}

void EditAlbumartDialog::showMenu(void )
{
    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "optionsmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "optionsmenu");

    menu->AddButton(tr("Edit Metadata"));
    menu->AddButton(tr("Rescan For Images"));


    menu->AddButton(tr("Search Internet For Images"));

    MetaIO *tagger = MetaIO::createTagger(m_metadata->Filename(false));

    if (m_coverartList->GetItemCurrent())
    {
        menu->AddButton(tr("Change Image Type"), NULL, true);

        if (GetMythDB()->GetNumSetting("AllowTagWriting", 0))
        {
            MythUIButtonListItem *item = m_coverartList->GetItemCurrent();
            if (item)
            {
                AlbumArtImage *image = qVariantValue<AlbumArtImage*> (item->GetData());
                if (image)
                {
                    if (!image->embedded)
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

    if (GetMythDB()->GetNumSetting("AllowTagWriting", 0))
    {
        if (tagger && tagger->supportsEmbeddedImages())
            menu->AddButton(tr("Copy Image To Tag"));
    }

    if (tagger)
        delete tagger;

    popupStack->AddScreen(menu);
}

void EditAlbumartDialog::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        // make sure the user didn't ESCAPE out of the menu
        if (dce->GetResult() < 0)
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
                    AlbumArtImages *albumArt = m_metadata->getAlbumArtImages();
                    item->SetText(albumArt->getTypeName((ImageType) type));
                    AlbumArtImage *image = qVariantValue<AlbumArtImage*> (item->GetData());
                    if (image)
                    {
                        QStringList strList("MUSIC_TAG_CHANGEIMAGE");
                        strList << m_metadata->Hostname()
                                << QString::number(m_metadata->ID())
                                << QString::number(image->imageType)
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
    else if (event->type() == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QStringList tokens = me->Message().split(" ", QString::SkipEmptyParts);

        if (!tokens.isEmpty())
        {
            if (tokens[0] == "BROWSER_DOWNLOAD_FINISHED")
                rescanForImages();
            else if (tokens[0] == "MUSIC_METADATA_CHANGED")
            {
                if (tokens.size() >= 2)
                {
                    int songID = tokens[1].toInt();

                    if (m_metadata->ID() == songID)
                    {
                        // force all the image to reload
                        for (int x = 0; x < m_metadata->getAlbumArtImages()->getImageCount(); x++)
                            removeCachedImage(m_metadata->getAlbumArtImages()->getImageAt(x));

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

    AlbumArtImages *albumArt = m_metadata->getAlbumArtImages();
    if (albumArt->getImageCount() > 0)
        m_albumArtChanged = true;
}

void EditAlbumartDialog::startCopyImageToTag(void)
{
    QString lastLocation = gCoreContext->GetSetting("MusicLastImageLocation", "/");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUIFileBrowser *fb = new MythUIFileBrowser(popupStack, lastLocation);

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
        delete fb;
}

void EditAlbumartDialog::copyImageToTag(ImageType imageType)
{
    AlbumArtImage image;
    image.filename = m_imageFilename;
    image.imageType = imageType;

    doCopyImageToTag(&image);
}

void EditAlbumartDialog::copySelectedImageToTag(void)
{
    MythUIButtonListItem *item = m_coverartList->GetItemCurrent();
    if (item)
    {
        AlbumArtImage *image = qVariantValue<AlbumArtImage*> (item->GetData());
        if (image)
            doCopyImageToTag(image);
    }
}

void EditAlbumartDialog::removeSelectedImageFromTag(void)
{
    MythUIButtonListItem *item = m_coverartList->GetItemCurrent();
    if (item)
    {
        AlbumArtImage *image = qVariantValue<AlbumArtImage*> (item->GetData());
        if (image)
        {
            QString msg = tr("Are you sure you want to permanently remove this image from the tag?");
            ShowOkPopup(msg, this, SLOT(doRemoveImageFromTag(bool)), true);
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
        AlbumArtImage *image = qVariantValue<AlbumArtImage*> (item->GetData());
        if (image)
        {
            // ask the backend to remove the image from the tracks tag
            QStringList strList("MUSIC_TAG_REMOVEIMAGE");
            strList << m_metadata->Hostname()
                    << QString::number(m_metadata->ID())
                    << QString::number(image->id);

            gCoreContext->SendReceiveStringList(strList);

            removeCachedImage(image);
            rescanForImages();
        }
    }
}

class CopyImageThread: public MThread
{
  public:
    CopyImageThread(QStringList strList) :
            MThread("CopyImage"), m_strList(strList) {}

    virtual void run()
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
    MythUIBusyDialog *busy = new MythUIBusyDialog(tr("Copying image to tag..."),
                                                  popupStack, "copyimagebusydialog");

    if (busy->Create())
    {
        popupStack->AddScreen(busy, false);
    }
    else
    {
        delete busy;
        busy = NULL;
    }

    // copy the image to the tracks host
    QFileInfo fi(image->filename);
    QString saveFilename = gCoreContext->GenMythURL(m_metadata->Hostname(), 0,
                                                    QString("AlbumArt/") + fi.fileName(),
                                                    "MusicArt");

    RemoteFile::CopyFile(image->filename, saveFilename);

    // ask the backend to add the image to the tracks tag
    QStringList strList("MUSIC_TAG_ADDIMAGE");
    strList << m_metadata->Hostname()
            << QString::number(m_metadata->ID())
            << fi.fileName()
            << QString::number(image->imageType);

    CopyImageThread *copyThread = new CopyImageThread(strList);
    copyThread->start();

    while (copyThread->isRunning())
    {
        qApp->processEvents();
        usleep(1000);
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
    if (!image->embedded)
        return;

    GetMythUI()->RemoveFromCacheByFile(image->filename);
}
