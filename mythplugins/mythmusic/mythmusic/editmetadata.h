#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

// C++
#include <iostream>

// MythTV
#include <libmythmetadata/musicmetadata.h>
#include <libmythui/mythscreentype.h>

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
    EditMetadataCommon(MythScreenStack *parent, const QString &name)
        : MythScreenType(parent, name) {}

    ~EditMetadataCommon(void) override;

    bool CreateCommon(void);

    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    void setSaveMetadataOnly(void);

  signals:
    void metadataChanged(void);

  protected slots:
    void showSaveMenu(void);
    static void saveToDatabase(void);
    void saveToMetadata(void);
    void saveAll(void);
    void cleanupAndClose(void);

  protected:
    static bool hasMetadataChanged(void);
    void updateMetadata(void);
    void searchForAlbumImages(void);
    static void scanForImages(void);

    static bool           s_metadataOnly;
    static MusicMetadata *s_metadata;
    static MusicMetadata *s_sourceMetadata;

    bool                  m_albumArtChanged {false};
    QString               m_searchType;
    MythUIButton         *m_doneButton      {nullptr};
};

class EditMetadataDialog : public EditMetadataCommon
{
    Q_OBJECT

  public:
    EditMetadataDialog(MythScreenStack *parent, MusicMetadata *source_metadata);
    explicit EditMetadataDialog(MythScreenStack *parent);
    ~EditMetadataDialog(void) override;

    bool Create(void) override; // MythScreenType

    bool keyPressEvent(QKeyEvent *event) override; // EditMetadataCommon
    void customEvent(QEvent *event) override; // MythUIType

  protected slots:
    void searchArtist(void) const;
    void searchCompilationArtist(void) const;
    void searchAlbum(void) const;
    void searchGenre(void) const;

    void setArtist(const QString& artist);
    void setCompArtist(const QString& compArtist);
    void setAlbum(const QString& album);
    void setGenre(const QString& genre);
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
    MythUITextEdit    *m_artistEdit             {nullptr};
    MythUITextEdit    *m_compArtistEdit         {nullptr};
    MythUITextEdit    *m_albumEdit              {nullptr};
    MythUITextEdit    *m_titleEdit              {nullptr};
    MythUITextEdit    *m_genreEdit              {nullptr};

    MythUISpinBox     *m_yearSpin               {nullptr};
    MythUISpinBox     *m_trackSpin              {nullptr};
    MythUISpinBox     *m_discSpin               {nullptr};
    MythUISpinBox     *m_ratingSpin             {nullptr};

    MythUIStateType   *m_ratingState            {nullptr};
    MythUIButton      *m_incRatingButton        {nullptr};
    MythUIButton      *m_decRatingButton        {nullptr};

    MythUIButton      *m_searchArtistButton     {nullptr};
    MythUIButton      *m_searchCompArtistButton {nullptr};
    MythUIButton      *m_searchAlbumButton      {nullptr};
    MythUIButton      *m_searchGenreButton      {nullptr};

    MythUIImage       *m_artistIcon             {nullptr};
    MythUIImage       *m_albumIcon              {nullptr};
    MythUIImage       *m_genreIcon              {nullptr};

    MythUICheckBox    *m_compilationCheck       {nullptr};

    MythUIButton      *m_albumartButton         {nullptr};
};

class EditAlbumartDialog : public EditMetadataCommon
{
    Q_OBJECT

  public:
    explicit EditAlbumartDialog(MythScreenStack *parent);
    ~EditAlbumartDialog() override;

    bool Create(void) override; // MythScreenType

    bool keyPressEvent(QKeyEvent *event) override; // EditMetadataCommon
    void customEvent(QEvent *event) override; // MythUIType

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
    static void removeCachedImage(const AlbumArtImage *image);

    QString         m_imageFilename;

    //
    //  GUI stuff
    //
    MythUIButton      *m_metadataButton    {nullptr};

    MythUIImage       *m_coverartImage     {nullptr};
    MythUIButtonList  *m_coverartList      {nullptr};
    MythUIText        *m_imagetypeText     {nullptr};
    MythUIText        *m_imagefilenameText {nullptr};
};

#endif
