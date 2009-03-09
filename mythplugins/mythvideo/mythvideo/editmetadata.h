#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

#include <mythtv/libmythui/mythscreentype.h>

class Metadata;
class MetadataListManager;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythUICheckBox;

class EditMetadataDialog : public MythScreenType
{
    Q_OBJECT

  public:
     EditMetadataDialog(MythScreenStack *lparent,
                       QString lname,
                       Metadata *source_metadata,
                       const MetadataListManager &cache);
    ~EditMetadataDialog();

    bool Create();
    void customEvent(QEvent *levent);

    void fillWidgets();

  signals:
    void Finished();

  public slots:
    void SaveAndExit();
    void SetTitle();
    void SetCategory(MythUIButtonListItem*);
    void SetPlayer();
    void SetLevel(MythUIButtonListItem*);
    void SetChild(MythUIButtonListItem*);
    void ToggleBrowse();
    void FindCoverArt();
    void FindBanner();
    void FindFanart();
    void FindScreenshot();
    void NewCategoryPopup();
    void AddCategory(QString category);
    void SetCoverArt(QString file);
    void SetBanner(QString file);
    void SetFanart(QString file);
    void SetScreenshot(QString file);

  private:

    Metadata            *m_workingMetadata;
    Metadata            *m_origMetadata;

    //
    //  GUI stuff
    //

    MythUITextEdit      *m_titleEdit;
    MythUITextEdit      *m_playerEdit;
    MythUIButtonList      *m_categoryList;
    MythUIButtonList      *m_levelList;
    MythUIButtonList      *m_childList;
    MythUICheckBox      *m_browseCheck;
    MythUIButton        *m_coverartButton;
    MythUIText          *m_coverartText;
    MythUIButton        *m_screenshotButton;
    MythUIText          *m_screenshotText;
    MythUIButton        *m_bannerButton;
    MythUIText          *m_bannerText;
    MythUIButton        *m_fanartButton;
    MythUIText          *m_fanartText;
    MythUIButton        *m_trailerButton;
    MythUIText          *m_trailerText;
    MythUIButton        *m_doneButton;

    //
    //  Remember video-to-play-next index number when the user is toggling
    //  child videos on and off
    //

    int cachedChildSelection;

    const MetadataListManager &m_metaCache;
};

#endif
