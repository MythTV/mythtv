/*
    videoselector.h

    header for the video selector interface screen
*/

#ifndef VIDEOSELECTOR_H_
#define VIDEOSELECTOR_H_

// c++
#include <vector>

// mythtv
#include <libmythmetadata/parentalcontrols.h>
#include <libmythui/mythscreentype.h>

// mytharchive
#include "archiveutil.h"

class ProgramInfo;
class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

struct VideoInfo
{
    int     id            { 0 };
    QString title;
    QString plot;
    QString category;
    QString filename;
    QString coverfile;
    int     parentalLevel { ParentalLevel::plNone };
    uint64_t size         { 0 };
};

class VideoSelector : public MythScreenType
{
    Q_OBJECT

  public:
    VideoSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList);

    ~VideoSelector(void) override;

    bool Create() override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

  signals:
    void haveResult(bool ok);

  public slots:
    void OKPressed(void);
    void cancelPressed(void);

    void ShowMenu(void) override; // MythScreenType
    void selectAll(void);
    void clearAll(void);

    void setCategory(MythUIButtonListItem *item);
    void titleChanged(MythUIButtonListItem *item);
    void toggleSelected(MythUIButtonListItem *item);

    void parentalLevelChanged(bool passwordValid, ParentalLevel::Level newLevel);

  private:
    void updateVideoList(void);
    void updateSelectedList(void);
    void getVideoList(void);
    void wireUpTheme(void);
    static std::vector<VideoInfo *> *getVideoListFromDB(void);
    void setParentalLevel(ParentalLevel::Level level);

    ParentalLevelChangeChecker *m_parentalLevelChecker {nullptr};

    QList<ArchiveItem *>     *m_archiveList            {nullptr};
    std::vector<VideoInfo *> *m_videoList              {nullptr};
    QList<VideoInfo *>        m_selectedList;

    ParentalLevel::Level      m_currentParentalLevel   {ParentalLevel::plNone};

    MythUIText       *m_plText                         {nullptr};
    MythUIButtonList *m_videoButtonList                {nullptr};
    MythUIText       *m_warningText                    {nullptr};
    MythUIButton     *m_okButton                       {nullptr};
    MythUIButton     *m_cancelButton                   {nullptr};
    MythUIButtonList *m_categorySelector               {nullptr};
    MythUIText       *m_titleText                      {nullptr};
    MythUIText       *m_filesizeText                   {nullptr};
    MythUIText       *m_plotText                       {nullptr};
    MythUIImage      *m_coverImage                     {nullptr};
};

Q_DECLARE_METATYPE(VideoInfo*)

#endif


