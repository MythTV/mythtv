#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

#include <iostream>
//Added by qt3to4:
#include <QPixmap>
#include <QKeyEvent>
using namespace std;

#include <mythdialogs.h>

class Metadata;
class ImageGridItem;
class AlbumArtImages;

class EditMetadataDialog : public MythThemedDialog
{
    Q_OBJECT

  public:
    EditMetadataDialog(Metadata       *source_metadata,
                       MythMainWindow *parent,
                       const QString  &window_name,
                       const QString  &theme_filename,
                       const char     *name = "EditMetadataDialog");
    ~EditMetadataDialog();

    void keyPressEvent(QKeyEvent *e);
    void wireUpTheme();
    void fillWidgets();
    void setSaveMetadataOnly();

  public slots:

    void closeDialog();
    void searchArtist();
    void searchCompilationArtist();
    void searchAlbum();
    void searchGenre();
    void incRating(bool up_or_down);
    void showSaveMenu();
    void saveToDatabase();
    void saveToMetadata();
    void saveAll();
    void cancelPopup();
    void editLostFocus();
    void checkClicked(bool state);
    void switchToMetadata(void);
    void switchToAlbumArt(void);
    void switchToDBStats(void);
    void gridItemChanged(ImageGridItem *item);

  private:

    bool showList(QString caption, QString &value);
    void showMenu(void);
    void updateImageGrid(void);
    QPixmap *createScaledPixmap(QString filename, int width, int height,
                                Qt::AspectRatioMode mode);

    bool                   metadataOnly;
    Metadata *m_metadata, *m_sourceMetadata ;
    MythPopupBox *popup;

    //
    //  GUI stuff
    //
    UIRemoteEditType    *artist_edit;
    UIRemoteEditType    *compilation_artist_edit;
    UIRemoteEditType    *album_edit;
    UIRemoteEditType    *title_edit;
    UIRemoteEditType    *genre_edit;
    UIRemoteEditType    *year_edit;
    UIRemoteEditType    *track_edit;

    UITextType          *lastplay_text;
    UITextType          *playcount_text;
    UITextType          *filename_text;

    UIRepeatedImageType *rating_image;

    UIPushButtonType    *searchartist_button;
    UIPushButtonType    *searchcompilation_artist_button;
    UIPushButtonType    *searchalbum_button;
    UIPushButtonType    *searchgenre_button;
    UIPushButtonType    *rating_button;

    UICheckBoxType      *compilation_check;

    UITextButtonType    *metadata_button;
    UITextButtonType    *albumart_button;
    UITextButtonType    *dbstatistics_button;
    UITextButtonType    *done_button;

    UIImageType         *coverart_image;
    UIImageGridType     *coverart_grid;
    UITextType          *imagetype_text;
    UITextType          *imagefilename_text;

    QStringList          searchList;
    AlbumArtImages      *albumArt;
};

#endif
