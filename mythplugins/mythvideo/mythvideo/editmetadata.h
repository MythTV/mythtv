#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

// MythUI headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythlistbutton.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuitextedit.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuicheckbox.h>

class Metadata;
class MetadataListManager;

class EditMetadataDialog : public MythScreenType
{
    Q_OBJECT

  public:
     EditMetadataDialog(MythScreenStack *parent,
                       const QString &name,
                       Metadata *source_metadata,
                       const MetadataListManager &cache);
    ~EditMetadataDialog();

    bool Create(void);

    void fillWidgets();

  signals:
    void Finished(void);

  public slots:

    void saveAndExit();
    void setTitle();
    void setCategory(MythListButtonItem*);
    void setPlayer();
    void setLevel(MythListButtonItem*);
    void setChild(MythListButtonItem*);
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
    MythListButton      *m_categoryList;
    MythListButton      *m_levelList;
    MythListButton      *m_childList;
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
