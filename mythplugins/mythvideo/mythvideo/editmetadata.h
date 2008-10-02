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

    void fillWidgets();

  signals:
    void Finished();

  public slots:

    void saveAndExit();
    void setTitle();
    void setCategory(MythUIButtonListItem*);
    void setPlayer();
    void setLevel(MythUIButtonListItem*);
    void setChild(MythUIButtonListItem*);
    void toggleBrowse();
    void findCoverArt();
    void NewCategoryPopup();
    void AddCategory(QString category);

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
    MythUIButton        *m_doneButton;

    //
    //  Remember video-to-play-next index number when the user is toggling
    //  child videos on and off
    //

    int cachedChildSelection;

    const MetadataListManager &m_metaCache;
};

#endif
