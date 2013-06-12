#ifndef LCDPROCCLIENT_H_
#define LCDPROCCLIENT_H_

#include <QStringList>
#include <QObject>
#include <QList>
#include <QTcpSocket>

#include "tvremoteutil.h"

class LCDServer;
class LCDTextItem;
class LCDMenuItem;
class QEvent;
class QTimer;


class LCDProcClient : public QObject
{
    Q_OBJECT

  public:

    LCDProcClient(LCDServer *lparent);

    void customEvent(QEvent  *e);

   ~LCDProcClient();

    bool SetupLCD(void);
    void reset(void);

    void setStartupMessage(QString msq, uint messagetime);

    // Used to actually connect to an LCD device       
    bool connectToHost(const QString &hostname, unsigned int port);

    void switchToTime();
    void switchToMusic(const QString &artist, const QString &album, const QString &track);
    void setMusicProgress(QString time, float generic_progress);
    void setMusicRepeat(int repeat);
    void setMusicShuffle(int shuffle);
    void switchToChannel(QString channum = "", QString title = "", 
                         QString subtitle = "");
    void setChannelProgress(const QString &time, float percentViewed);
    void switchToMenu(QList<LCDMenuItem> *menuItems, QString app_name = "",
                      bool popMenu = true);
    void switchToGeneric(QList<LCDTextItem> *textItems);
    void setGenericProgress(bool busy, float generic_progress);

    void switchToVolume(QString app_name);
    void setVolumeLevel(float volume_level);

    void switchToNothing();

    void shutdown();
    void removeWidgets();
    void updateLEDs(int mask);
    void stopAll(void);

    int  getLCDWidth(void) { return m_lcdWidth; }
    int  getLCDHeight(void) { return m_lcdHeight; }

  private slots: 
    void veryBadThings(QAbstractSocket::SocketError error); // Communication Errors
    void serverSendingData();      // Data coming back from LCDd

    void checkConnections();       // check connections to LCDd and mythbackend
                                   // every 10 seconds

    void dobigclock(bool);         // Large display
    void dostdclock();             // Small display
    void outputTime();             // Fire from a timer
    void outputMusic();            // Short timer (equalizer)
    void outputChannel();          // Longer timer (progress bar)
    void outputGeneric();          // Longer timer (progress bar)
    void outputVolume();
    void outputRecStatus();
    void scrollMenuText();         // Scroll the menu items if need be
    void beginScrollingMenuText(); // But only after a bit of time has gone by
    void unPopMenu();              // Remove the Pop Menu display
    void scrollList();             // display a list line by line
    void updateRecordingList(void);
    void removeStartupMessage(void);
    void beginScrollingWidgets(void);
    void scrollWidgets(void);

  private:
    void outputCenteredText(QString theScreen, QString theText,
                            QString widget = "topWidget", int row = 1);

    void outputLeftText(QString theScreen, QString theText,
                        QString widget = "topWidget", int row = 1);
    void outputRightText(QString theScreen, QString theText,
                         QString widget = "topWidget", int row = 1);

    void outputScrollerText(QString theScreen, QString theText,
                         QString widget = "scroller", int top = 1, int bottom = 1);

    QStringList formatScrollerText(const QString &text);
    void outputText(QList<LCDTextItem> *textItems);

    void sendToServer(const QString &someText);

    enum PRIORITY {TOP, URGENT, HIGH, MEDIUM, LOW, OFF};
    void setPriority(const QString &screen, PRIORITY priority);

    void setHeartbeat (const QString &screen, bool onoff);
    QString expandString(const QString &aString);

    void init();
    void loadSettings();   //reload the settings from the db

    void assignScrollingList(QStringList theList, QString theScreen, 
                             QString theWidget = "topWidget", int theRow = 1);

    // Scroll 1 or more widgets on a screen
    void assignScrollingWidgets(QString theText, QString theScreen,
                             QString theWidget = "topWidget", int theRow = 1);
    void formatScrollingWidgets(void);

    void startTime();
    void startMusic(QString artist, QString album, QString track);
    void startChannel(QString channum, QString title, QString subtitle);
    void startGeneric(QList<LCDTextItem> * textItems);
    void startMenu(QList<LCDMenuItem> *menuItems, QString app_name,
                   bool popMenu);
    void startVolume(QString app_name);
    void showStartupMessage(void);

    void setWidth(unsigned int);
    void setHeight(unsigned int);
    void setCellWidth(unsigned int);
    void setCellHeight(unsigned int);
    void setVersion(const QString &, const QString &);
    void describeServer();

    QString m_activeScreen;

    QTcpSocket *m_socket;
    QTimer *m_timeTimer;
    QTimer *m_scrollWTimer;
    QTimer *m_preScrollWTimer;
    QTimer *m_menuScrollTimer;
    QTimer *m_menuPreScrollTimer;
    QTimer *m_popMenuTimer;
    QTimer *m_checkConnectionsTimer;
    QTimer *m_recStatusTimer;
    QTimer *m_scrollListTimer;
    QTimer *m_showMessageTimer;
    QTimer *m_updateRecInfoTimer;

    QString  m_prioTop;
    QString  m_prioUrgent;
    QString  m_prioHigh;
    QString  m_prioMedium;
    QString  m_prioLow;
    QString  m_prioOff;

    unsigned int m_lcdWidth;
    unsigned int m_lcdHeight;
    unsigned int m_cellWidth;
    unsigned int m_cellHeight;

    QString m_serverVersion;
    QString m_protocolVersion;
    int m_pVersion;

    float m_EQlevels[10];
    float m_progress;
    QString m_channelTime;
    /** true if the generic progress indicator is a busy
        (ie. doesn't have a known total steps */
    bool m_busyProgress;
    /** Current position of the busy indicator,
        used if @p m_busyProgress is true. */
    int m_busyPos;
    /** How many "blocks" the busy indicator must be,
        used if @p m_busyProgress is true. */
    float m_busyIndicatorSize;
    /** Direction of the busy indicator on the, -1 or 1,
        used if @p m_busyProgress is true. */
    int m_busyDirection;
    float m_genericProgress;
    float m_volumeLevel;

    float m_musicProgress;
    QString m_musicTime;
    int m_musicRepeat;
    int m_musicShuffle;

    QList<LCDTextItem> *m_lcdTextItems;
    //QString m_scrollingText;
    QString m_scrollScreen;
    unsigned int m_scrollPosition;
    QString m_timeFormat;
    QString m_dateFormat;

    QStringList m_scrollListItems;
    QString m_scrollListScreen, m_scrollListWidget;
    int m_scrollListRow;
    unsigned int m_scrollListItem;

    unsigned int m_menuScrollPosition;
    QList<LCDMenuItem> *m_lcdMenuItems;

    bool m_connected;
    bool m_timeFlash;

    QString m_sendBuffer;
    QString m_lastCommand;
    QString m_hostname;
    unsigned int m_port;

    bool m_lcdReady;

    bool m_lcdShowTime;
    bool m_lcdShowMenu;
    bool m_lcdShowGeneric;
    bool m_lcdShowMusic;
    bool m_lcdShowChannel;
    bool m_lcdShowVolume;
    bool m_lcdShowRecstatus;
    bool m_lcdBacklightOn;
    bool m_lcdHeartbeatOn;
    bool m_lcdBigClock;
    int  m_lcdPopupTime;
    QString m_lcdShowMusicItems;
    QString m_lcdKeyString;
    LCDServer *m_parentLCDServer;
    QString m_startupMessage;
    uint m_startupShowTime;

    bool m_isRecording;
    bool m_isTimeVisible;
    int m_lcdTunerNo;

    vector<TunerStatus> m_tunerList;
};

#endif
