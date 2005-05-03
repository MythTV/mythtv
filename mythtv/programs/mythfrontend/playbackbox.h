#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"
#include "yuv2rgb.h"

#include <pthread.h>

class QListViewItem;
class QLabel;
class QProgressBar;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;

class LayerSet;

class PlaybackBox : public MythDialog
{
    Q_OBJECT
  public:
    typedef enum { Play, Delete } BoxType;
    typedef enum { TitlesOnly, TitlesCategories, TitlesCategoriesRecGroups,
                   TitlesRecGroups, Categories, CategoriesRecGroups, RecGroups} ViewType;


    PlaybackBox(BoxType ltype, MythMainWindow *parent, const char *name = 0);
   ~PlaybackBox(void);
   
    void customEvent(QCustomEvent *e);
  
  protected slots:
    void timeout(void);

    void cursorLeft();
    void cursorRight();
    void cursorDown(bool page = false, bool newview = false);
    void cursorUp(bool page = false, bool newview = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void upcoming();
    void details();
    void selected();
    void playSelected();
    void stopSelected();
    void deleteSelected();
    void expireSelected();
    void showMenu();
    void showActionsSelected();
    void showRecGroupChanger();
    void showRecGroupChooser();
    void showRecGroupPasswordChanger();
    void showPlayFromPopup();
    void showRecordingPopup();
    void showJobPopup();
    void showStoragePopup();
    void showPlaylistPopup();
    void showPlaylistJobPopup();

    void doPlay();
    void doPlayFromBeg();
    void doPlayListRandom();

    void askStop();
    void doStop();
    void noStop();

    void doEditScheduled();

    void askDelete();
    void doDelete();
    void doDeleteForgetHistory();
    void doForceDelete();
    void noDelete();

    void doAutoExpire();
    void noAutoExpire();
    void doPreserveEpisode();
    void noPreserveEpisode();

    void doCancel();
    void toggleTitleView();

    void exitWin();

    void setUpdateFreeSpace() { updateFreeSpace = true; }

    void chooseComboBoxChanged(void);
    void chooseSetViewGroup(void);
    void chooseSetGroupView(void);
    void changeComboBoxChanged(void);
    void changeSetRecGroup(void);
    void changeRecGroupPassword();
    void changeOldPasswordChanged(const QString &newText);
    void doJobQueueJob(int jobType, int jobFlags = 0);
    void doBeginTranscoding();
    void doBeginFlagging();
    void doBeginUserJob1();
    void doBeginUserJob2();
    void doBeginUserJob3();
    void doBeginUserJob4();
    void doPlaylistJobQueueJob(int jobType, int jobFlags = 0);
    void stopPlaylistJobQueueJob(int jobType);
    void doPlaylistBeginTranscoding();
    void stopPlaylistTranscoding();
    void doPlaylistBeginFlagging();
    void stopPlaylistFlagging();
    void doPlaylistBeginUserJob1();
    void stopPlaylistUserJob1();
    void doPlaylistBeginUserJob2();
    void stopPlaylistUserJob2();
    void doPlaylistBeginUserJob3();
    void stopPlaylistUserJob3();
    void doPlaylistBeginUserJob4();
    void stopPlaylistUserJob4();
    void doClearPlaylist();
    void doPlaylistDelete();
    void doPlaylistChangeRecGroup();
    void togglePlayListTitle(void);
    void togglePlayListItem(void);
    void playSelectedPlaylist(bool random);
    void doPlayList(void);
    void showViewChanger(void);
  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    bool FillList(void);
    void UpdateProgressBar(void);

    QString cutDown(QString, QFont *, int);
    QPixmap getPixmap(ProgramInfo *);
    QPainter backup;
    bool play(ProgramInfo *);
    void stop(ProgramInfo *);
    void remove(ProgramInfo *);
    void expire(ProgramInfo *);
    void showActions(ProgramInfo *);

    void togglePlayListItem(ProgramInfo *pginfo);
    void randomizePlayList(void);

    bool haveGroupInfoSet;
    
    bool skipUpdate;
    bool pageDowner;
    bool connected;
    ProgramInfo *curitem;
    ProgramInfo *delitem;

    void LoadWindow(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    void parsePopup(QDomElement &);
    void parseContainer(QDomElement &);

    int skipCnt;
    bool inTitle;
    bool playingVideo;
    bool leftRight;
    int titleIndex;
    int progIndex;
    QStringList titleList;
    QStringList playList;
    bool onPlaylist;
    QMap<QString, ProgramList> progLists;

    ProgramInfo *findMatchingProg(ProgramInfo *);
    ProgramInfo *findMatchingProg(QString key);
    ProgramInfo *findMatchingProg(QString chanid, QString startts);

    BoxType type;

    bool arrowAccel;

    bool killPlayer(void);
    void killPlayerSafe(void);
    void startPlayer(ProgramInfo *rec);

    bool doRemove(ProgramInfo *, bool forgetHistory, bool forceMetadataDelete);
    void promptEndOfRecording(ProgramInfo *);
    typedef enum {
         EndOfRecording,DeleteRecording,AutoExpireRecording,StopRecording,
         ForceDeleteRecording
    } deletePopupType;
    void showDeletePopup(ProgramInfo *, deletePopupType);
    void showActionPopup(ProgramInfo *program);
    void initPopup(MythPopupBox *popup, ProgramInfo *program, 
                   QString message, QString message2);
    void cancelPopup();

    void showAvailablePopup(ProgramInfo *rec);

    bool fileExists(ProgramInfo *pginfo);

    void showIconHelp();

    QString getRecGroupPassword(QString recGroup);
    void fillRecGroupPasswordCache(void);

    QTimer *timer;
    NuppelVideoPlayer *nvp;
    RingBuffer *rbuffer;
    pthread_t decoder;

    typedef enum 
    { kStarting, kPlaying, kKilling, kKilled, kStopping, kStopped, 
      kChanging } playerStateType;

    playerStateType state;

    typedef enum
    { kNvpToPlay, kNvpToStop, kDone } killStateType;
     
    killStateType killState;
    QTime killTimeout;

    QTime nvpTimeout;    
    QTime waitToStartPreviewTimer;
    bool waitToStart;
 
    QDateTime lastUpdateTime;
    bool graphicPopup;
    bool playbackPreview;
    bool generatePreviewPixmap;
    QString dateformat;
    QString timeformat;

    void grayOut(QPainter *);
    void updateBackground(void);
    void updateVideo(QPainter *);
    void updateShowTitles(QPainter *);
    void updateInfo(QPainter *);
    void updateUsage(QPainter *);
    void updateProgramInfo(QPainter *p, QRect& pr, QPixmap& pix);
    void updateCurGroup(QPainter *p);
    void updateGroupInfo(QPainter *p, QRect& pr, QPixmap& pix, QString cont_name = "group_info");
    
    QString showDateFormat;
    QString showTimeFormat;

    MythPopupBox *popup;
    QPixmap myBackground;

    QPixmap *containerPixmap;
    QPixmap *fillerPixmap;
  
    QPixmap *bgTransBackup;

    QRect fullRect;
    QRect listRect;
    QRect infoRect;
    QRect groupRect;
    QRect usageRect;
    QRect videoRect;
    QRect curGroupRect;

    int listsize;

    QColor popupForeground;
    QColor popupBackground;
    QColor popupHighlight;

    bool expectingPopup;

    bool updateFreeSpace;
    QTimer *freeSpaceTimer;
    int freeSpaceTotal;
    int freeSpaceUsed;

    QString groupDisplayName;
    QString recGroup;
    QString recGroupPassword;
    QString curGroupPassword;
    QMap <QString, QString> recGroupType;
    QMap <QString, QString> recGroupPwCache;

    MythPopupBox *choosePopup;
    MythListBox *chooseListBox;
    MythComboBox *chooseComboBox;
    MythLineEdit *chooseLineEdit;
    MythLineEdit *chooseOldPassword;
    MythLineEdit *chooseNewPassword;
    MythPushButton *chooseOkButton;
    MythPushButton *chooseDeleteButton;
    QString chooseGroupPassword;
    bool groupnameAsAllProg;
    QPixmap *previewPixmap;
    QDateTime previewStartts;
    QString previewChanid;
    int listOrder;
    int overrectime;
    int underrectime;

    bool playingSomething;
    bool titleView;

    bool useRecGroups;
    bool useCategories;
    void setDefaultView(int defaultView);

    yuv2rgb_fun    conv_yuv2rgba;
    unsigned char *conv_rgba_buf;
    QSize          conv_rgba_size;
};

#endif
