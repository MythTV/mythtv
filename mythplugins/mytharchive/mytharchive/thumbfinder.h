#ifndef THUMBFINDER_H_
#define THUMBFINDER_H_

// qt
#include <QString>
#include <QStringList>

// mythtv
#include <mythscreentype.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include "programtypes.h"

// mytharchive
#include "archiveutil.h"
#include "remoteavformatcontext.h"

typedef struct SeekAmount
{
    QString name;
    int amount;
} SeekAmount;

extern struct SeekAmount SeekAmounts[];
extern int SeekAmountsCount;

class MythUIButton;
class MythUItext;
class MythUIImage;
class MythUIButtonList;
class MythUIButtonListItem;
class MythImage;

class ThumbFinder : public MythScreenType
{

  Q_OBJECT

  public:

      ThumbFinder(MythScreenStack *parent, ArchiveItem *archiveItem,
                  const QString &menuTheme);
    ~ThumbFinder();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);


  private slots:
    void gridItemChanged(MythUIButtonListItem *item);
    void showMenu(void);
    void cancelPressed(void);
    void savePressed(void);
    void updateThumb(void);

  private:
    void Init(void);
    bool getThumbImages(void);
    int  getChapterCount(const QString &menuTheme);
    void changeSeekAmount(bool up);
    void updateCurrentPos(void);
    bool seekToFrame(int frame, bool checkPos = true);
    QString createThumbDir(void);
    QString frameToTime(int64_t frame, bool addFrame = false);

    // avcodec stuff
    bool initAVCodec(const QString &inFile);
    void closeAVCodec();
    bool seekForward();
    bool seekBackward();
    bool getFrameImage(bool needKeyFrame = true, int64_t requiredPTS = -1);
    int  checkFramePosition(int frameNumber);
    void loadCutList(void);
    void updatePositionBar(int64_t frame);
    int  calcFinalDuration(void);

    RemoteAVFormatContext m_inputFC;
    AVCodecContext  *m_codecCtx;
    AVCodec         *m_codec;
    AVFrame         *m_frame;

    float            m_fps;
    unsigned char   *m_outputbuf;
    QString          m_frameFile;
    int              m_frameWidth;
    int              m_frameHeight;
    int              m_videostream;
    int              m_currentSeek;
    int64_t          m_startTime;   // in time_base units
    int64_t          m_startPTS;    // in time_base units
    int64_t          m_currentPTS;  // in time_base units
    int64_t          m_firstIFramePTS;
    int              m_frameTime;   // in time_base units
    bool             m_updateFrame;
    frm_dir_map_t    m_deleteMap;
    int              m_finalDuration;
    int              m_offset;

    ArchiveItem        *m_archiveItem;
    int                 m_thumbCount;
    QList<ThumbImage *> m_thumbList;
    QString             m_thumbDir;

    // GUI stuff
    MythUIButton       *m_frameButton;
    MythUIButton       *m_saveButton;
    MythUIButton       *m_cancelButton;
    MythUIImage        *m_frameImage;
    MythUIImage        *m_positionImage;
    MythUIButtonList   *m_imageGrid;
    MythUIText         *m_seekAmountText;
    MythUIText         *m_currentPosText;
};

#endif
