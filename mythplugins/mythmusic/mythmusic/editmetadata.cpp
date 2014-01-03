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
    MythScreenType(parent, name), m_doneButton(NULL)
{
    // make a copy so we can abandon changes
    m_metadata = new MusicMetadata(*source_metadata);
    m_sourceMetadata = source_metadata;

    metadataOnly = false;
}

EditMetadataCommon::EditMetadataCommon(MythScreenStack *parent,
                                       const QString &name) :
    MythScreenType(parent, name), m_doneButton(NULL)
{
}

EditMetadataCommon::~EditMetadataCommon()
{
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
    if (GetMythDB()->GetNumSetting("AllowTagWriting", 0))
    {
        MetaIO *tagger = m_metadata->getTagger();

        if (tagger)
        {
            tagger->write(m_metadata);
            delete tagger;
        }
    }

    saveToDatabase();

    cleanupAndClose();
}

void EditMetadataCommon::setSaveMetadataOnly(void)
{
    metadataOnly = true;

    MythUIButton *albumartButton = dynamic_cast<MythUIButton *>(GetChild("albumart_button"));
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

    QFileInfo  fi(m_metadata->Filename());

    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), fi.canonicalPath() + '/', "front.jpg");
}

void EditMetadataCommon::scanForImages(void)
{
    // clear the original images
    AlbumArtList *imageList = m_metadata->getAlbumArtImages()->getImageList();
    while (!imageList->isEmpty())
    {
        delete imageList->back();
        imageList->pop_back();
    }

    // scan the directory for images
    QFileInfo fi(m_metadata->Filename());
    QDir dir = fi.absoluteDir();

    QString nameFilter = gCoreContext->GetSetting("AlbumArtFilter",
                                                  "*.png;*.jpg;*.jpeg;*.gif;*.bmp");
    dir.setNameFilters(nameFilter.split(";"));

    QStringList files = dir.entryList();

    for (int x = 0; x < files.size(); x++)
    {
        AlbumArtImage *image = new AlbumArtImage();
        image->filename = dir.absolutePath() + '/' + files.at(x);
        image->embedded = false;
        image->imageType = AlbumArtImages::guessImageType(image->filename);
        image->description = "";
        m_metadata->getAlbumArtImages()->addImage(image);
        delete image;
    }

    // scan the tracks tag for any images
    MetaIO *tagger = m_metadata->getTagger();

    if (tagger && tagger->supportsEmbeddedImages())
    {
        AlbumArtList art = tagger->getAlbumArtList(m_metadata->Filename());
        for (int x = 0; x < art.count(); x++)
        {
            AlbumArtImage image = art.at(x);
            m_metadata->getAlbumArtImages()->addImage(image);
        }
    }

    if (tagger)
        delete tagger;
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
        file = findIcon("artist", artist.toLower());
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
        file = findIcon("genre", genre.toLower());
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

    QString cleanName = fixFilename(m_metadata->Genre().toLower());
    QString file = GetConfDir() + QString("/MythMusic/Icons/%1/%2.jpg").arg("genre").arg(cleanName);

    QFileInfo fi(file);
    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), fi.absolutePath() + '/', fi.fileName());
}

/// search google for artist images
void EditMetadataDialog::searchForArtistImages(void)
{
    QString artist = m_metadata->Artist().replace(' ', '+');
    artist = QUrl::toPercentEncoding(artist, "+");

    QUrl url("http://www.google.co.uk/images?q=" + artist, QUrl::TolerantMode);

    QString cleanName = fixFilename(m_metadata->Artist().toLower());
    QString file = GetConfDir() + QString("/MythMusic/Icons/%1/%2.jpg").arg("artist").arg(cleanName);

    QFileInfo fi(file);
    GetMythMainWindow()->HandleMedia("WebBrowser", url.toString(), fi.absolutePath() + '/', fi.fileName());
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
                int length = calcTrackLength(m_metadata->Filename());

                if (length != m_metadata->Length() / 1000)
                {
                    int oldLength = m_metadata->Length() / 1000;

                    // save the new length to our working copy of the metadata
                    m_metadata->setLength(length * 1000);

                    // save the new length to the source copy of the metadata
                    m_sourceMetadata->setLength(length * 1000);
                    m_sourceMetadata->dumpToDatabase();

                    // this will update any track lengths displayed on screen
                    gPlayer->sendMetadataChangedEvent(m_sourceMetadata->ID());

                    // this will force the playlist stats to update
                    MusicPlayerEvent me(MusicPlayerEvent::TrackChangeEvent, gPlayer->getCurrentTrackPos());
                    gPlayer->dispatch(me);

                    ShowOkPopup(QString("Updated track length to %1 seconds\nwas %2 seconds").arg(length).arg(oldLength));
                }
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
    m_albumArt(m_metadata->getAlbumArtImages()),
    m_albumArtChanged(false),    m_metadataButton(NULL),
    m_doneButton(NULL),          m_coverartImage(NULL),
    m_coverartList(NULL),        m_imagetypeText(NULL),
    m_imagefilenameText(NULL)
{
    gCoreContext->addListener(this);
}

EditAlbumartDialog::~EditAlbumartDialog()
{
    // do we need to save anything?
    if (m_albumArtChanged)
    {
        m_albumArt->dumpToDatabase();

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
    AlbumArtList *albumArtList = m_albumArt->getImageList();

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

    menu->AddButton(m_albumArt->getTypeName(IT_UNKNOWN),    qVariantFromValue((int)IT_UNKNOWN),    false, (imageType == IT_UNKNOWN));
    menu->AddButton(m_albumArt->getTypeName(IT_FRONTCOVER), qVariantFromValue((int)IT_FRONTCOVER), false, (imageType == IT_FRONTCOVER));
    menu->AddButton(m_albumArt->getTypeName(IT_BACKCOVER),  qVariantFromValue((int)IT_BACKCOVER),  false, (imageType == IT_BACKCOVER));
    menu->AddButton(m_albumArt->getTypeName(IT_CD),         qVariantFromValue((int)IT_CD),         false, (imageType == IT_CD));
    menu->AddButton(m_albumArt->getTypeName(IT_INLAY),      qVariantFromValue((int)IT_INLAY),      false, (imageType == IT_INLAY));
    menu->AddButton(m_albumArt->getTypeName(IT_ARTIST),     qVariantFromValue((int)IT_ARTIST),     false, (imageType == IT_ARTIST));

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

    MetaIO *tagger = m_metadata->getTagger();

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
                    item->SetText(m_albumArt->getTypeName((ImageType) type));
                    AlbumArtImage *image = qVariantValue<AlbumArtImage*> (item->GetData());
                    if (image)
                    {
                        AlbumArtImage oldImage = *image;

                        image->imageType = (ImageType) type;

                        if (image->imageType == oldImage.imageType)
                            return;

                        // rename any cached image to match the new type
                        if (image->embedded)
                        {
                            // update the new cached image filename
                            image->filename = QString(GetConfDir() + "/MythMusic/AlbumArt/%1-%2.jpg")
                                                .arg(m_metadata->ID())
                                                .arg(AlbumArtImages::getTypeFilename(image->imageType));

                            if (image->filename != oldImage.filename && QFile::exists(oldImage.filename))
                            {
                                // remove any old cached file with the same name as the new one
                                QFile::remove(image->filename);
                                // rename the old cached file to the new one
                                QFile::rename(oldImage.filename, image->filename);

                                // force the theme image cache to refresh the image
                                GetMythUI()->RemoveFromCacheByFile(image->filename);
                            }

                            // change the image type in the tag if it supports it
                            MetaIO *tagger = m_metadata->getTagger();

                            if (tagger && tagger->supportsEmbeddedImages())
                            {
                                if (!tagger->changeImageType(m_metadata->Filename(), &oldImage, image->imageType))
                                    LOG(VB_GENERAL, LOG_INFO, "EditAlbumartDialog: failed to change image type");
                            }

                            if (tagger)
                                delete tagger;
                        }
                        else
                        {
                            QFileInfo fi(oldImage.filename);

                            // get the new images filename
                            image->filename = QString(fi.absolutePath() + "/%1.jpg")
                                    .arg(AlbumArtImages::getTypeFilename(image->imageType));

                            if (image->filename != oldImage.filename && QFile::exists(oldImage.filename))
                            {
                                // remove any old cached file with the same name as the new one
                                QFile::remove(image->filename);
                                // rename the old cached file to the new one
                                QFile::rename(oldImage.filename, image->filename);

                                // force the theme image cache to refresh the image
                                GetMythUI()->RemoveFromCacheByFile(image->filename);
                            }
                        }

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
        }
    }
}

/// search the tracks tag and the tracks directory for images
void EditAlbumartDialog::rescanForImages(void)
{
    // scan the tracks directory and tag for any images
    scanForImages();

    updateImageGrid();

    if (m_albumArt->getImageCount() > 0)
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
            MetaIO *tagger = m_metadata->getTagger();

            if (tagger && !tagger->supportsEmbeddedImages())
            {
                LOG(VB_GENERAL, LOG_ERR, "EditAlbumartDialog: asked to remove an image from the tag "
                                         "but the tagger doesn't support it!");
                delete tagger;
                return;
            }

            if (!tagger->removeAlbumArt(m_metadata->Filename(), image))
                LOG(VB_GENERAL, LOG_ERR, "EditAlbumartDialog: failed to remove album art from tag");
            else
                LOG(VB_GENERAL, LOG_INFO, "EditAlbumartDialog: album art removed from tag");

            removeCachedImage(image);
            rescanForImages();

            if (tagger)
                delete tagger;
        }
    }
}

void EditAlbumartDialog::doCopyImageToTag(const AlbumArtImage *image)
{
    MetaIO *tagger = m_metadata->getTagger();

    if (tagger && !tagger->supportsEmbeddedImages())
    {
        LOG(VB_GENERAL, LOG_ERR, "EditAlbumartDialog: asked to write album art to the tag "
                              "but the tagger does't support it!");
        delete tagger;
        return;
    }

    if (!tagger->writeAlbumArt(m_metadata->Filename(), image))
        LOG(VB_GENERAL, LOG_ERR, "EditAlbumartDialog: failed to write album art to tag");
    else
        LOG(VB_GENERAL, LOG_INFO, "EditAlbumartDialog: album art written to tag");

    removeCachedImage(image);

    rescanForImages();

    if (tagger)
        delete tagger;
}

void EditAlbumartDialog::removeCachedImage(const AlbumArtImage *image)
{
    if (!image->embedded)
        return;

    QString imageFilename = QString(GetConfDir() + "/MythMusic/AlbumArt/%1-%2.jpg")
            .arg(m_metadata->ID()).arg(AlbumArtImages::getTypeFilename(image->imageType));

    if (QFile::exists(imageFilename))
        QFile::remove(imageFilename);

    GetMythUI()->RemoveFromCacheByFile(imageFilename);
}
