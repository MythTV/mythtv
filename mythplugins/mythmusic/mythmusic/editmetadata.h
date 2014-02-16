#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

#include <iostream>
using namespace std;

#include <mythscreentype.h>
#include <musicmetadata.h>

class MythUIStateType;
class MythUIImage;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUICheckBox;
class MythUISearchDialog;

class EditMetadataCommon : public MythScreenType
{
    Q_OBJECT

  public:
    EditMetadataCommon(MythScreenStack *parent, MusicMetadata *source_metadata, const QString &name);
    EditMetadataCommon(MythScreenStack *parent, const QString &name);

    ~EditMetadataCommon(void);

    bool CreateCommon(void);

    bool keyPressEvent(QKeyEvent *event);

    void setSaveMetadataOnly(void);

  signals:
    void metadataChanged(void);

  protected slots:
    void showSaveMenu(void);
    void saveToDatabase(void);
    void saveToMetadata(void);
    void saveAll(void);
    void cleanupAndClose(void);

  protected:
    bool hasMetadataChanged(void);
    void updateMetadata(void);
    void searchForAlbumImages(void);
    void scanForImages(void);

    static bool            metadataOnly;
    static MusicMetadata  *m_metadata, *m_sourceMetadata;

    bool m_albumArtChanged;

    QString m_searchType;

    MythUIButton *m_doneButton;
};

class EditMetadataDialog : public EditMetadataCommon
{
    Q_OBJECT

  public:
    EditMetadataDialog(MythScreenStack *parent, MusicMetadata *source_metadata);
    EditMetadataDialog(MythScreenStack *parent);
    ~EditMetadataDialog(void);

    bool Create(void);

    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *event);

  protected slots:
    void searchArtist(void);
    void searchCompilationArtist(void);
    void searchAlbum(void);
    void searchGenre(void);

    void setArtist(QString artist);
    void setCompArtist(QString compArtist);
    void setAlbum(QString album);
    void setGenre(QString genre);
    void ratingSpinChanged(MythUIButtonListItem *item);

    void artistLostFocus(void);
    void albumLostFocus(void);
    void genreLostFocus(void);

    void incRating(void);
    void decRating(void);

    void checkClicked(bool state);

    void switchToAlbumArt(void);

  private:
    void showMenu(void);
    void fillWidgets(void);

    void updateArtistImage(void);
    void updateAlbumImage(void);
    void updateGenreImage(void);

    void updateRating(void);

    void searchForArtistImages(void);
    void searchForGenreImages(void);

    //
    //  GUI stuff
    //
    MythUITextEdit    *m_artistEdit;
    MythUITextEdit    *m_compArtistEdit;
    MythUITextEdit    *m_albumEdit;
    MythUITextEdit    *m_titleEdit;
    MythUITextEdit    *m_genreEdit;

    MythUISpinBox     *m_yearSpin;
    MythUISpinBox     *m_trackSpin;
    MythUISpinBox     *m_ratingSpin;

    MythUIStateType   *m_ratingState;
    MythUIButton      *m_incRatingButton;
    MythUIButton      *m_decRatingButton;

    MythUIButton      *m_searchArtistButton;
    MythUIButton      *m_searchCompArtistButton;
    MythUIButton      *m_searchAlbumButton;
    MythUIButton      *m_searchGenreButton;

    MythUIImage       *m_artistIcon;
    MythUIImage       *m_albumIcon;
    MythUIImage       *m_genreIcon;

    MythUICheckBox    *m_compilationCheck;

    MythUIButton      *m_albumartButton;
};

class EditAlbumartDialog : public EditMetadataCommon
{
    Q_OBJECT

  public:
    EditAlbumartDialog(MythScreenStack *parent);
    ~EditAlbumartDialog();

    bool Create(void);

    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *event);

  signals:
    void metadataChanged(void);

  protected slots:
    void switchToMetadata(void);
    void showMenu(void);
    void showTypeMenu(bool changeType = true);
    void gridItemChanged(MythUIButtonListItem *item);

    void rescanForImages(void);

    void doRemoveImageFromTag(bool doIt);

  private:
    void updateImageGrid(void);
    void copySelectedImageToTag(void);
    void removeSelectedImageFromTag(void);
    void startCopyImageToTag(void);
    void copyImageToTag(ImageType imageType);
    void doCopyImageToTag(const AlbumArtImage *image);
    void removeCachedImage(const AlbumArtImage *image);

    QString         m_imageFilename;

    //
    //  GUI stuff
    //
    MythUIButton      *m_metadataButton;
    MythUIButton      *m_doneButton;

    MythUIImage       *m_coverartImage;
    MythUIButtonList  *m_coverartList;
    MythUIText        *m_imagetypeText;
    MythUIText        *m_imagefilenameText;
};

#endif
