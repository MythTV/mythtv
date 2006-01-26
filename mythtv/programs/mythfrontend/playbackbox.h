// -*- Mode: c++ -*-
#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qdatetime.h>
#include <qdom.h>
#include <qmutex.h>

#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"
#include "jobqueue.h"

#include <qvaluelist.h>
#include <pthread.h>

class QListViewItem;
class QLabel;
class QProgressBar;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;
class PreviewGenerator;

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
    void showPlayGroupChanger();
    void showRecTitleChanger();
    void showRecGroupChooser();
    void showRecGroupPasswordChanger();
    void showPlayFromPopup();
    void showRecordingPopup();
    void showJobPopup();
    void showStoragePopup();
    void showPlaylistPopup();
    void showPlaylistJobPopup();
    void showProgramDetails();

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

    void listChanged(void);
    void setUpdateFreeSpace() { updateFreeSpace = true; }

    void initRecGroupPopup(QString title, QString name);
    void closeRecGroupPopup(bool refreshList = true);
    void setGroupFilter(void);
    void recGroupChangerListBoxChanged(void);
    void recGroupChooserListBoxChanged(void);
    void setRecGroup(void);
    void setPlayGroup(void);
    void setRecTitle(void);
    void setRecGroupPassword();
    void recGroupOldPasswordChanged(const QString &newText);

    void doJobQueueJob(int jobType, int jobFlags = 0);
    void doPlaylistJobQueueJob(int jobType, int jobFlags = 0);
    void stopPlaylistJobQueueJob(int jobType);
    void doBeginFlagging();
    void doBeginTranscoding()         {   doJobQueueJob(JOB_TRANSCODE,
                                                        JOB_USE_CUTLIST);      }
    void doBeginUserJob1()            {   doJobQueueJob(JOB_USERJOB1);         }
    void doBeginUserJob2()            {   doJobQueueJob(JOB_USERJOB2);         }
    void doBeginUserJob3()            {   doJobQueueJob(JOB_USERJOB3);         }
    void doBeginUserJob4()            {   doJobQueueJob(JOB_USERJOB4);         }
    void doPlaylistBeginTranscoding() {   doPlaylistJobQueueJob(JOB_TRANSCODE,
                                                             JOB_USE_CUTLIST); }
    void stopPlaylistTranscoding()    { stopPlaylistJobQueueJob(JOB_TRANSCODE);}
    void doPlaylistBeginFlagging()    {   doPlaylistJobQueueJob(JOB_COMMFLAG); }
    void stopPlaylistFlagging()       { stopPlaylistJobQueueJob(JOB_COMMFLAG); }
    void doPlaylistBeginUserJob1()    {   doPlaylistJobQueueJob(JOB_USERJOB1); }
    void stopPlaylistUserJob1()       { stopPlaylistJobQueueJob(JOB_USERJOB1); }
    void doPlaylistBeginUserJob2()    {   doPlaylistJobQueueJob(JOB_USERJOB2); }
    void stopPlaylistUserJob2()       { stopPlaylistJobQueueJob(JOB_USERJOB2); }
    void doPlaylistBeginUserJob3()    {   doPlaylistJobQueueJob(JOB_USERJOB3); }
    void stopPlaylistUserJob3()       { stopPlaylistJobQueueJob(JOB_USERJOB3); }
    void doPlaylistBeginUserJob4()    {   doPlaylistJobQueueJob(JOB_USERJOB4); }
    void stopPlaylistUserJob4()       { stopPlaylistJobQueueJob(JOB_USERJOB4); }
    void doClearPlaylist();
    void doPlaylistDelete();
    void doPlaylistChangeRecGroup();
    void doPlaylistChangePlayGroup();
    void togglePlayListTitle(void);
    void togglePlayListItem(void);
    void playSelectedPlaylist(bool random);
    void doPlayList(void);
    void showViewChanger(void);

    void previewThreadDone(const QString &fn, bool &success)
        { success = SetPreviewGenerator(fn, NULL); }
    void previewReady(const ProgramInfo *pginfo);

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

    bool SetPreviewGenerator(const QString &fn, PreviewGenerator *g);
    bool IsGeneratingPreview(const QString &fn) const;

  private:
    bool FillList(void);
    void UpdateProgressBar(void);

    QString cutDown(QString, QFont *, int);
    QDateTime getPreviewLastModified(ProgramInfo *);
    QPixmap getPixmap(ProgramInfo *);
    QPainter backup;
    bool play(ProgramInfo *rec, bool inPlaylist = false);
    void stop(ProgramInfo *);
    void remove(ProgramInfo *);
    void expire(ProgramInfo *);
    void showActions(ProgramInfo *);

    void togglePlayListItem(ProgramInfo *pginfo);
    void randomizePlayList(void);

    void processNetworkControlCommands(void);
    void processNetworkControlCommand(QString command);
    QValueList<QString> networkControlCommands;
    QMutex ncLock;
    bool underNetworkControl;

    bool haveGroupInfoSet;
    
    bool skipUpdate;
    bool pageDowner;
    bool connected;
    ProgramInfo *curitem;
    ProgramInfo *delitem;
    ProgramInfo *lastProgram;

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
     
    mutable QMutex killLock;
    killStateType killState;
    MythTimer killTimeout;

    MythTimer nvpTimeout;    
    MythTimer waitToStartPreviewTimer;
    bool waitToStart;
 
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

    QTimer *fillListTimer;
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

    int recGroupLastItem;
    MythPopupBox *recGroupPopup;
    MythListBox *recGroupListBox;
    MythLineEdit *recGroupLineEdit;
    MythLineEdit *recGroupLineEdit1;
    MythLineEdit *recGroupOldPassword;
    MythLineEdit *recGroupNewPassword;
    MythPushButton *recGroupOkButton;
    QString recGroupChooserPassword;
    bool groupnameAsAllProg;
    QPixmap *previewPixmap;
    QDateTime previewLastModified;
    QDateTime previewFilets;
    QDateTime previewStartts;
    bool      previewSuspend;
    QString previewChanid;
    int listOrder;

    bool playingSomething;
    bool titleView;

    bool useRecGroups;
    bool useCategories;
    void setDefaultView(int defaultView);

    bool everStartedVideo;

    mutable QMutex previewGeneratorLock;
    QMap<QString, PreviewGenerator*> previewGenerator;
};

#endif
