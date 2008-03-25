#ifndef THUMBFINDER_H_
#define THUMBFINDER_H_

#include <qthread.h>
#include <qstring.h>
#include <qstringlist.h>
//Added by qt3to4:
#include <QPixmap>
#include <QKeyEvent>
#include <Q3PtrList>

using namespace std;
#include <iostream>

#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>
extern "C" {
#include <mythtv/ffmpeg/avcodec.h>
#include <mythtv/ffmpeg/avformat.h>
}

#include "archiveutil.h"

typedef struct SeekAmount
{
    QString name;
    int amount;
} SeekAmount;

extern struct SeekAmount SeekAmounts[];
extern int SeekAmountsCount;

class ThumbFinder : public MythThemedDialog
{

  Q_OBJECT

  public:

      ThumbFinder(ArchiveItem *archiveItem, const QString &menuTheme,
                  MythMainWindow *parent, const QString &window_name,
                  const QString &theme_filename, const char *name = 0);
    ~ThumbFinder();


  private slots:
    void keyPressEvent(QKeyEvent *e);
    void wireUpTheme(void);
    bool getThumbImages(void);
    void cancelPressed(void);
    void savePressed(void);
    void gridItemChanged(ImageGridItem *item);
    void showMenu(void);
    void closePopupMenu(void);
    void menuSavePressed(void);
    void menuCancelPressed(void);

  private:
    int  getChapterCount(const QString &menuTheme);
    QPixmap *createScaledPixmap(QString filename, int width, int height,
                                  Qt::AspectRatioMode mode);
    void changeSeekAmount(bool up);
    void updateThumb(void);
    void updateCurrentPos(void);
    bool seekToFrame(int frame, bool checkPos = true);
    QString createThumbDir(void);
    QString frameToTime(int64_t frame, bool addFrame = false);

    ArchiveItem    *m_archiveItem;
    int             m_thumbCount;
    Q3PtrList<ThumbImage> m_thumbList;
    QString         m_thumbDir;

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

    AVFormatContext *m_inputFC;
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
    QMap<long long, int> m_deleteMap;
    int              m_finalDuration;
    int              m_offset;

    // GUI stuff
    UITextButtonType     *m_frameButton;
    UITextButtonType     *m_saveButton;
    UITextButtonType     *m_cancelButton;
    UIImageType          *m_frameImage;
    UIImageType          *m_positionImage;
    UIImageGridType      *m_imageGrid;
    UITextType           *m_seekAmountText;
    UITextType           *m_currentPosText;

    // popup menu
    MythPopupBox         *m_popupMenu;
};

#endif
