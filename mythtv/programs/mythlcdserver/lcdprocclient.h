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

    int  getLCDWidth(void) { return lcdWidth; }
    int  getLCDHeight(void) { return lcdHeight; }

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

    QString activeScreen;

    QTcpSocket *socket;
    QTimer *timeTimer;
    QTimer *scrollWTimer;
    QTimer *preScrollWTimer;
    QTimer *menuScrollTimer;
    QTimer *menuPreScrollTimer;
    QTimer *popMenuTimer;
    QTimer *checkConnectionsTimer;
    QTimer *recStatusTimer;
    QTimer *scrollListTimer;
    QTimer *showMessageTimer;
    QTimer *updateRecInfoTimer;

    void setWidth(unsigned int);
    void setHeight(unsigned int);
    void setCellWidth(unsigned int);
    void setCellHeight(unsigned int);
    void setVersion(const QString &, const QString &);
    void describeServer();

    QString  prioTop;
    QString  prioUrgent;
    QString  prioHigh;
    QString  prioMedium;
    QString  prioLow;
    QString  prioOff;

    unsigned int lcdWidth;
    unsigned int lcdHeight;
    unsigned int cellWidth;
    unsigned int cellHeight;

    QString serverVersion;
    QString protocolVersion;
    int pVersion;

    float EQlevels[10];
    float progress;
    QString channel_time;
    /** true if the generic progress indicator is a busy
        (ie. doesn't have a known total steps */
    bool busy_progress;
    /** Current position of the busy indicator,
        used if @p busy_progress is true. */
    int busy_pos;
    /** How many "blocks" the busy indicator must be,
        used if @p busy_progress is true. */
    float busy_indicator_size;
    /** Direction of the busy indicator on the, -1 or 1,
        used if @p busy_progress is true. */
    int busy_direction;
    float generic_progress;
    float volume_level;

    float music_progress;
    QString music_time;
    int music_repeat;
    int music_shuffle;

    QList<LCDTextItem> *lcdTextItems;
    QString scrollingText;
    QString scrollScreen;
    unsigned int scrollPosition;
    QString timeformat;
    QString dateformat;

    QStringList scrollListItems;
    QString scrollListScreen, scrollListWidget;
    int scrollListRow;
    unsigned int scrollListItem;

    unsigned int menuScrollPosition;
    QList<LCDMenuItem> *lcdMenuItems;

    bool connected;
    bool timeFlash;

    QString send_buffer;
    QString last_command;
    QString hostname;
    unsigned int port;

    bool lcd_ready;

    bool lcd_showtime;
    bool lcd_showmenu;
    bool lcd_showgeneric;
    bool lcd_showmusic;
    bool lcd_showchannel;
    bool lcd_showvolume;
    bool lcd_showrecstatus;
    bool lcd_backlighton;
    bool lcd_heartbeaton;
    bool lcd_bigclock;
    int  lcd_popuptime;    
    QString lcd_showmusic_items;
    QString lcd_keystring;
    LCDServer *m_parent;
    QString startup_message;
    uint startup_showtime; 

    bool isRecording;
    bool isTimeVisible;
    int lcdTunerNo;

    vector<TunerStatus> tunerList;
};

#endif
