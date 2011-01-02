/*
	videoselector.h

    header for the video selector interface screen
*/

#ifndef VIDEOSELECTOR_H_
#define VIDEOSELECTOR_H_

// c++
#include <vector>

// mythtv
#include <mythscreentype.h>
#include <metadata/parentalcontrols.h>

// mytharchive
#include "archiveutil.h"

class ProgramInfo;
class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

typedef struct
{
    int     id;
    QString title;
    QString plot;
    QString category;
    QString filename;
    QString coverfile;
    int     parentalLevel;
    unsigned long long size;
} VideoInfo;

class VideoSelector : public MythScreenType
{
    Q_OBJECT

  public:
    VideoSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList);

    ~VideoSelector(void);

    bool Create();
    bool keyPressEvent(QKeyEvent *e);

  signals:
    void haveResult(bool ok);

  public slots:
    void OKPressed(void);
    void cancelPressed(void);

    void showMenu(void);
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
    std::vector<VideoInfo *> *getVideoListFromDB(void);
    void setParentalLevel(ParentalLevel::Level level);

    ParentalLevelChangeChecker *m_parentalLevelChecker;

    QList<ArchiveItem *>     *m_archiveList;
    std::vector<VideoInfo *> *m_videoList;
    QList<VideoInfo *>        m_selectedList;

    ParentalLevel::Level      m_currentParentalLevel;

    MythUIText       *m_plText;
    MythUIButtonList *m_videoButtonList;
    MythUIText       *m_warningText;
    MythUIButton     *m_okButton;
    MythUIButton     *m_cancelButton;
    MythUIButtonList *m_categorySelector;
    MythUIText       *m_titleText;
    MythUIText       *m_filesizeText;
    MythUIText       *m_plotText;
    MythUIImage      *m_coverImage;
};

Q_DECLARE_METATYPE(VideoInfo*)

#endif


