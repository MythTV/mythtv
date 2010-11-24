#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>

#include "editmetadata.h"
#include "decoder.h"
#include "genres.h"
#include "metadata.h"

#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythuihelper.h>

EditMetadataDialog::EditMetadataDialog(Metadata       *source_metadata,
                                       MythMainWindow *parent,
                                       const QString  &window_name,
                                       const QString  &theme_filename,
                                       const char     *name)
    : MythThemedDialog(parent, window_name, theme_filename, name)
{
    // make a copy so we can abandon changes
    m_metadata = new Metadata(*source_metadata);
    m_sourceMetadata = source_metadata;
    albumArt = new AlbumArtImages(m_metadata);
    metadataOnly = false;
    setContext(1);
    wireUpTheme();
    fillWidgets();
    assignFirstFocus();
}

EditMetadataDialog::~EditMetadataDialog()
{
    delete m_metadata;
    delete albumArt;
}

void EditMetadataDialog::fillWidgets()
{
    if (album_edit)
    {
        album_edit->setText(m_metadata->Album());
    }

    if (artist_edit)
    {
        artist_edit->setText(m_metadata->Artist());
    }

    if (compilation_artist_edit)
    {
        compilation_artist_edit->setText(m_metadata->CompilationArtist());
    }

    if (title_edit)
    {
        title_edit->setText(m_metadata->Title());
    }

    if (genre_edit)
    {
        genre_edit->setText(m_metadata->Genre());
    }

    if (year_edit)
    {
        QString s;
        s = s.setNum(m_metadata->Year());
        year_edit->setText(s);
    }

    if (track_edit)
    {
        QString s;
        s = s.setNum(m_metadata->Track());
        track_edit->setText(s);
    }

    if (playcount_text)
    {
        QString s;
        s = s.setNum(m_metadata->Playcount());
        playcount_text->SetText(s);
    }

    if (lastplay_text)
    {
        lastplay_text->SetText(m_metadata->LastPlay()
                               .toString(gCoreContext->GetSetting("dateformat")
                                 + " " + gCoreContext->GetSetting("timeformat")));
    }

    if (filename_text)
    {
        filename_text->SetText(m_metadata->Filename());
    }

    if (rating_image)
    {
        rating_image->setRepeat(m_metadata->Rating());
    }

    if (compilation_check)
    {
        compilation_check->setState(m_metadata->Compilation());
    }

    if (coverart_grid)
    {
        updateImageGrid();
    }
}

void EditMetadataDialog::gridItemChanged(ImageGridItem *item)
{
    if (!item)
        return;

    if (coverart_image)
    {
        AlbumArtImage *image = (AlbumArtImage*) item->data;
        if (image)
        {
            coverart_image->SetImage(image->filename);
            coverart_image->LoadImage();
            if (imagetype_text)
                imagetype_text->SetText(image->typeName);
            if (imagefilename_text)
            {
                QFileInfo fi(image->filename);
                imagefilename_text->SetText(fi.fileName());
            }
        }
    }
}

void EditMetadataDialog::updateImageGrid()
{
    vector<AlbumArtImage*> *albumArtList = albumArt->getImageList();

    QSize size = coverart_grid->getImageItemSize();

    for (uint x = 0; x < albumArtList->size(); x++)
    {
        if (albumArtList->at(x)->embedded)
            continue;

        QPixmap *pixmap = createScaledPixmap(albumArtList->at(x)->filename,
                                             size.width(), size.height(),
                                             Qt::KeepAspectRatio);

        ImageGridItem *item = new ImageGridItem(albumArtList->at(x)->typeName,
                pixmap, false, (void*) albumArtList->at(x));
        coverart_grid->appendItem(item);
    }

    coverart_grid->setItemCount(albumArtList->size());
    coverart_grid->recalculateLayout();

    if (!albumArtList->empty())
        gridItemChanged(coverart_grid->getItemAt(0));

    coverart_grid->refresh();
}

QPixmap *EditMetadataDialog::createScaledPixmap(QString filename,
                                         int width, int height, Qt::AspectRatioMode mode)
{
    QPixmap *pixmap = NULL;

    if (!filename.isEmpty())
    {
        QImage *img = GetMythUI()->LoadScaleImage(filename);
        if (!img)
        {
            VERBOSE(VB_IMPORTANT, QString("EditMetadataDialog: Failed to load image %1").arg(filename));
            return NULL;
        }
        else
        {
            pixmap = new QPixmap(img->scaled(width, height,
                                    mode, Qt::SmoothTransformation));
            delete img;
        }
    }

    return pixmap;
}

void EditMetadataDialog::incRating(bool up_or_down)
{
    if (up_or_down)
        m_metadata->incRating();
    else
        m_metadata->decRating();

    fillWidgets();
}

void EditMetadataDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            if (getCurrentFocusWidget() == coverart_grid)
            {

                if (coverart_grid->handleKeyPress(action))
                    return;
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == coverart_grid)
            {

                if (coverart_grid->handleKeyPress(action))
                    return;
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "LEFT")
        {
            if (getCurrentFocusWidget() == rating_button)
            {
                rating_button->push();
                incRating(false);
            }
            else if (getCurrentFocusWidget() == compilation_check)
            {
                compilation_check->activate();
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == rating_button)
            {
                rating_button->push();
                incRating(true);
            }
            else if (getCurrentFocusWidget() == compilation_check)
            {
                compilation_check->activate();
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "SELECT")
        {
            activateCurrent();
        }
        else if (action == "MENU" && getContext() == 2)
        {
            showMenu();
        }
        else if (action == "0")
        {
            if (done_button)
                done_button->push();
        }
        else if (action == "1")
        {
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void EditMetadataDialog::wireUpTheme()
{
    artist_edit = getUIRemoteEditType("artist_edit");
    if (artist_edit)
    {
        artist_edit->createEdit(this);
        connect(artist_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    compilation_artist_edit = getUIRemoteEditType("compilation_artist_edit");
    if (compilation_artist_edit)
    {
        compilation_artist_edit->createEdit(this);
        connect(compilation_artist_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    album_edit = getUIRemoteEditType("album_edit");
    if (album_edit)
    {
        album_edit->createEdit(this);
        connect(album_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    title_edit = getUIRemoteEditType("title_edit");
    if (title_edit)
    {
        title_edit->createEdit(this);
        connect(title_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    genre_edit = getUIRemoteEditType("genre_edit");
    if (genre_edit)
    {
        genre_edit->createEdit(this);
        connect(genre_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    year_edit = getUIRemoteEditType("year_edit");
    if (year_edit)
    {
        year_edit->createEdit(this);
        connect(year_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    track_edit = getUIRemoteEditType("track_edit");
    if (track_edit)
    {
        track_edit->createEdit(this);
        connect(track_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    lastplay_text = getUITextType("lastplay_text");
    playcount_text = getUITextType("playcount_text");
    filename_text = getUITextType("filename_text");
    rating_image = getUIRepeatedImageType("rating_image");

    compilation_check = getUICheckBoxType("compilation_check");
    if (compilation_check)
    {
        connect(compilation_check, SIGNAL(pushed(bool)), this, SLOT(checkClicked(bool)));
    }

    searchartist_button = getUIPushButtonType("searchartist_button");
    if (searchartist_button)
    {
        connect(searchartist_button, SIGNAL(pushed()), this, SLOT(searchArtist()));
    }

    searchcompilation_artist_button = getUIPushButtonType("searchcompilation_artist_button");
    if (searchcompilation_artist_button)
    {
        connect(searchcompilation_artist_button, SIGNAL(pushed()), this, SLOT(searchCompilationArtist()));
    }

    searchalbum_button = getUIPushButtonType("searchalbum_button");
    if (searchalbum_button)
    {
        connect(searchalbum_button, SIGNAL(pushed()), this, SLOT(searchAlbum()));
    }

    searchgenre_button = getUIPushButtonType("searchgenre_button");
    if (searchgenre_button)
    {
        connect(searchgenre_button, SIGNAL(pushed()), this, SLOT(searchGenre()));
    }

    metadata_button = getUITextButtonType("metadata_button");
    if (metadata_button)
    {
        metadata_button->setText(tr("Track Info."));
        connect(metadata_button, SIGNAL(pushed()), this, SLOT(switchToMetadata()));
    }

    albumart_button = getUITextButtonType("albumart_button");
    if (albumart_button)
    {
        albumart_button->setText(tr("Album Art"));
        connect(albumart_button, SIGNAL(pushed()), this, SLOT(switchToAlbumArt()));
    }

    dbstatistics_button = getUITextButtonType("dbstats_button");
    if (dbstatistics_button)
    {
        dbstatistics_button->setText(tr("Statistics"));
        connect(dbstatistics_button, SIGNAL(pushed()), this, SLOT(switchToDBStats()));
    }

    done_button = getUITextButtonType("done_button");
    if (done_button)
    {
        done_button->setText(tr("Done"));
        connect(done_button, SIGNAL(pushed()), this, SLOT(showSaveMenu()));
    }

    coverart_image = getUIImageType("coverart_image");
    coverart_grid = getUIImageGridType("coverart_grid");
    if (coverart_grid)
    {
        connect(coverart_grid, SIGNAL(itemChanged(ImageGridItem *)),
                this, SLOT(gridItemChanged(ImageGridItem *)));
    }

    imagetype_text = getUITextType("imagetype_text");
    imagefilename_text = getUITextType("imagefilename_text");

    rating_button = getUISelectorType("rating_button");
    if (rating_button)
    {

    }

    buildFocusList();
}

void EditMetadataDialog::switchToMetadata()
{
    setContext(1);

    updateForeground();
    buildFocusList();
    if (albumart_button)
        setCurrentFocusWidget(albumart_button);
}

void EditMetadataDialog::switchToAlbumArt()
{
    setContext(2);

    updateForeground();
    buildFocusList();
    if (metadata_button)
        setCurrentFocusWidget(metadata_button);
}

void EditMetadataDialog::switchToDBStats()
{
#if 0
    setContext(3);

    updateForeground();
    buildFocusList();
#endif
}

void EditMetadataDialog::editLostFocus()
{
    UIRemoteEditType *whichEditor = (UIRemoteEditType *) getCurrentFocusWidget();

    if (whichEditor == album_edit)
    {
        m_metadata->setAlbum(album_edit->getText());
    }
    else if (whichEditor == artist_edit)
    {
        m_metadata->setArtist(artist_edit->getText());
    }
    else if (whichEditor == compilation_artist_edit)
    {
        m_metadata->setCompilationArtist(compilation_artist_edit->getText());
    }
    else if (whichEditor == title_edit)
    {
        m_metadata->setTitle(title_edit->getText());
    }
    else if (whichEditor == genre_edit)
    {
        m_metadata->setGenre(genre_edit->getText());
    }
    else if (whichEditor == year_edit)
    {
        m_metadata->setYear(year_edit->getText().toInt());
    }
    else if (whichEditor == track_edit)
    {
        m_metadata->setTrack(track_edit->getText().toInt());
    }

}

void EditMetadataDialog::checkClicked(bool state)
{
    m_metadata->setCompilation(state);
    if (!state)
    {
        m_metadata->setCompilationArtist("");

        if (compilation_artist_edit)
        {
            compilation_artist_edit->setText("");
        }
    }
    else
    {
        if (m_metadata->CompilationArtist().isEmpty())
        {
            m_metadata->setCompilationArtist(tr("Various Artists"));

            if (compilation_artist_edit)
            {
                compilation_artist_edit->setText(tr("Various Artists"));
            }
        }
    }
}

bool EditMetadataDialog::showList(QString caption, QString &value)
{
    bool res = false;

    MythSearchDialog *searchDialog = new MythSearchDialog(GetMythMainWindow(), "");
    searchDialog->setCaption(caption);
    searchDialog->setSearchText(value);
    searchDialog->setItems(searchList);
    DialogCode rescode = searchDialog->ExecPopupAtXY(-1, 8);
    if (kDialogCodeRejected != rescode)
    {
        value = searchDialog->getResult();
        res = true;
    }

    searchDialog->deleteLater();
    activateWindow();

    return res;
}

void EditMetadataDialog::searchArtist()
{
    QString s;

    searchList = Metadata::fillFieldList("artist");

    s = m_metadata->Artist();
    if (showList(tr("Select an Artist"), s))
    {
        m_metadata->setArtist(s);
        fillWidgets();
    }
}

void EditMetadataDialog::searchCompilationArtist()
{
    QString s;

    searchList = Metadata::fillFieldList("compilation_artist");

    s = m_metadata->CompilationArtist();
    if (showList(tr("Select a Compilation Artist"), s))
    {
        m_metadata->setCompilationArtist(s);
        fillWidgets();
    }
}

void EditMetadataDialog::searchAlbum()
{
    QString s;

    searchList = Metadata::fillFieldList("album");

    s = m_metadata->Album();
    if (showList(tr("Select an Album"), s))
    {
        m_metadata->setAlbum(s);
        fillWidgets();
    }
}

void EditMetadataDialog::searchGenre()
{
    QString s;

    // load genre list
    /*
    searchList.clear();
    for (int x = 0; x < genre_table_size; x++)
        searchList.push_back(QString(genre_table[x]));
    searchList.sort();
    */
    searchList = Metadata::fillFieldList("genre");

    s = m_metadata->Genre();
    if (showList(tr("Select a Genre"), s))
    {
        m_metadata->setGenre(s);
        fillWidgets();
    }

}

void EditMetadataDialog::closeDialog()
{
    cancelPopup();
    reject();
}

void EditMetadataDialog::showSaveMenu()
{
    popup = new MythPopupBox(GetMythMainWindow(), "Menu");

    QLabel *label = popup->addLabel(tr("Save Changes?"), MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);
    QAbstractButton *topButton;

    if (metadataOnly)
    {
        topButton = popup->addButton(tr("Save Changes"), this,
                                        SLOT(saveToMetadata()));
    }
    else
    {
        topButton = popup->addButton(tr("Save Changes"), this,
                                     SLOT(saveAll()));
    }

    popup->addButton(tr("Exit/Do Not Save"), this,
                            SLOT(closeDialog()));

    popup->addButton(tr("Cancel"), this, SLOT(cancelPopup()));

    popup->ShowPopup(this, SLOT(cancelPopup()));

    topButton->setFocus();
}

void EditMetadataDialog::cancelPopup(void)
{
    if (popup)
    {
        popup->deleteLater();
        popup = NULL;
        activateWindow();
    }
}

void EditMetadataDialog::saveToMetadata()
{
    cancelPopup();

    *m_sourceMetadata = m_metadata;
    accept();
}

void EditMetadataDialog::saveToDatabase()
{
    cancelPopup();

    m_metadata->dumpToDatabase();
    *m_sourceMetadata = m_metadata;

    accept();
}

void EditMetadataDialog::saveAll()
{
    cancelPopup();

    if (GetMythDB()->GetNumSetting("AllowTagWriting", 0))
    {
        Decoder *decoder = Decoder::create(m_metadata->Filename(), NULL, NULL, true);
        if (decoder)
        {
            decoder->commitMetadata(m_metadata);
            delete decoder;
        }
    }

    saveToDatabase();
}

void EditMetadataDialog::showMenu()
{
    if (coverart_grid->getItemCount() == 0)
        return;

    MythPopupBox *menu = new MythPopupBox(GetMythMainWindow(), "menu");

    QLabel *caption = menu->addLabel(tr("Change Image Type"), MythPopupBox::Medium);
    caption->setAlignment(Qt::AlignCenter);

    menu->addButton(albumArt->getTypeName(IT_UNKNOWN));
    menu->addButton(albumArt->getTypeName(IT_FRONTCOVER));
    menu->addButton(albumArt->getTypeName(IT_BACKCOVER));
    menu->addButton(albumArt->getTypeName(IT_CD));
    menu->addButton(albumArt->getTypeName(IT_INLAY));

    DialogCode ret = menu->ExecPopup();
    int res = MythDialog::CalcItemIndex(ret);

    if ((IT_UNKNOWN <= res) && (res < IT_LAST))
    {
        // get selected image in grid
        ImageGridItem *item = coverart_grid->getCurrentItem();
        if (item)
        {
            item->text = albumArt->getTypeName((ImageType) res);
            AlbumArtImage *image = (AlbumArtImage*) item->data;
            if (image)
            {
                image->imageType = (ImageType) res;
                image->typeName = item->text;

                // save the image type to the DB
                albumArt->saveImageType(image->id, image->imageType);

                gridItemChanged(item);
            }
        }
    }

    menu->deleteLater();
}

void EditMetadataDialog::setSaveMetadataOnly()
{
    metadataOnly = true;

    if (metadata_button)
        metadata_button->hide();

    if (albumart_button)
        albumart_button->hide();
}
