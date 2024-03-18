#ifndef LCDPROCCLIENT_H_
#define LCDPROCCLIENT_H_

#include <QStringList>
#include <QObject>
#include <QList>
#include <QTcpSocket>

#include "libmythtv/tvremoteutil.h"

using namespace std::chrono_literals;

class LCDServer;
class LCDTextItem;
class LCDMenuItem;
class QEvent;
class QTimer;


class LCDProcClient : public QObject
{
    Q_OBJECT

  public:

    explicit LCDProcClient(LCDServer *lparent);

    void customEvent(QEvent  *e) override; // QObject

   ~LCDProcClient() override;

    bool SetupLCD(void);
    void reset(void);

    void setStartupMessage(QString msg, std::chrono::seconds messagetime);

    // Used to actually connect to an LCD device       
    bool connectToHost(const QString &hostname, unsigned int port);

    void switchToTime();
    void switchToMusic(const QString &artist, const QString &album, const QString &track);
    void setMusicProgress(QString time, float value);
    void setMusicRepeat(int repeat);
    void setMusicShuffle(int shuffle);
    void switchToChannel(const QString& channum = "", const QString& title = "", 
                         const QString& subtitle = "");
    void setChannelProgress(const QString &time, float value);
    void switchToMenu(QList<LCDMenuItem> *menuItems, const QString& app_name = "",
                      bool popMenu = true);
    void switchToGeneric(QList<LCDTextItem> *textItems);
    void setGenericProgress(bool busy, float value);

    void switchToVolume(const QString& app_name);
    void setVolumeLevel(float value);

    void switchToNothing();

    void shutdown();
    void removeWidgets();
    void updateLEDs(int mask);
    void stopAll(void);

    int  getLCDWidth(void) const { return m_lcdWidth; }
    int  getLCDHeight(void) const { return m_lcdHeight; }

  private slots: 
    void veryBadThings(QAbstractSocket::SocketError error); // Communication Errors
    void serverSendingData();      // Data coming back from LCDd

    void checkConnections();       // check connections to LCDd and mythbackend
                                   // every 10 seconds

    void dobigclock(void);         // Large display
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
    void outputCenteredText(const QString& theScreen, QString theText,
                            const QString& widget = "topWidget", int row = 1);

    void outputLeftText(const QString& theScreen, QString theText,
                        const QString& widget = "topWidget", int row = 1);
    void outputRightText(const QString& theScreen, QString theText,
                         const QString& widget = "topWidget", int row = 1);

    void outputScrollerText(const QString& theScreen, const QString& theText,
                         const QString& widget = "scroller", int top = 1, int bottom = 1);

    QStringList formatScrollerText(const QString &text) const;
    void outputText(QList<LCDTextItem> *textItems);

    void sendToServer(const QString &someText);

    enum PRIORITY : std::uint8_t {TOP, URGENT, HIGH, MEDIUM, LOW, OFF};
    void setPriority(const QString &screen, PRIORITY priority);

    void setHeartbeat (const QString &screen, bool onoff);
    QString expandString(const QString &aString) const;

    void init();
    void loadSettings();   //reload the settings from the db

    void assignScrollingList(QStringList theList, QString theScreen, 
                             QString theWidget = "topWidget", int theRow = 1);

    // Scroll 1 or more widgets on a screen
    void assignScrollingWidgets(const QString& theText, const QString& theScreen,
                             const QString& theWidget = "topWidget", int theRow = 1);
    void formatScrollingWidgets(void);

    void startTime();
    void startMusic(QString artist, const QString& album, const QString& track);
    void startChannel(const QString& channum, const QString& title, const QString& subtitle);
    void startGeneric(QList<LCDTextItem> * textItems);
    void startMenu(QList<LCDMenuItem> *menuItems, QString app_name,
                   bool popMenu);
    void startVolume(const QString& app_name);
    void showStartupMessage(void);

    void setWidth(unsigned int x);
    void setHeight(unsigned int x);
    void setCellWidth(unsigned int x);
    void setCellHeight(unsigned int x);
    void setVersion(const QString &sversion, const QString &pversion);
    void describeServer();

    QString             m_activeScreen;

    QTcpSocket         *m_socket                {nullptr};
    QTimer             *m_timeTimer             {nullptr};
    QTimer             *m_scrollWTimer          {nullptr};
    QTimer             *m_preScrollWTimer       {nullptr};
    QTimer             *m_menuScrollTimer       {nullptr};
    QTimer             *m_menuPreScrollTimer    {nullptr};
    QTimer             *m_popMenuTimer          {nullptr};
    QTimer             *m_checkConnectionsTimer {nullptr};
    QTimer             *m_recStatusTimer        {nullptr};
    QTimer             *m_scrollListTimer       {nullptr};
    QTimer             *m_showMessageTimer      {nullptr};
    QTimer             *m_updateRecInfoTimer    {nullptr};

    QString             m_prioTop;
    QString             m_prioUrgent;
    QString             m_prioHigh;
    QString             m_prioMedium;
    QString             m_prioLow;
    QString             m_prioOff;

    uint8_t             m_lcdWidth              {5};
    uint8_t             m_lcdHeight             {1};
    uint8_t             m_cellWidth             {1};
    uint8_t             m_cellHeight            {1};

    QString             m_serverVersion;
    QString             m_protocolVersion;
    uint8_t             m_pVersion              {0};

    float               m_progress              {0.0};
    QString             m_channelTime;
    /** true if the generic progress indicator is a busy
        (ie. doesn't have a known total steps */
    bool                m_busyProgress          {false};
    /** Current position of the busy indicator,
        used if @p m_busyProgress is true. */
    int                 m_busyPos               {0};
    /** How many "blocks" the busy indicator must be,
        used if @p m_busyProgress is true. */
    float               m_busyIndicatorSize     {0.0};
    /** Direction of the busy indicator on the, -1 or 1,
        used if @p m_busyProgress is true. */
    int                 m_busyDirection         {1};
    float               m_genericProgress       {0.0};
    float               m_volumeLevel           {0.0};

    float               m_musicProgress         {0.0};
    QString             m_musicTime;
    int                 m_musicRepeat           {0};
    int                 m_musicShuffle          {0};

    QList<LCDTextItem> *m_lcdTextItems          {nullptr};
    //QString           m_scrollingText;
    QString             m_scrollScreen;
    unsigned int        m_scrollPosition        {0};
    QString             m_timeFormat;
    QString             m_dateFormat;

    QStringList         m_scrollListItems;
    QString             m_scrollListScreen;
    QString             m_scrollListWidget;
    int                 m_scrollListRow         {0};
    unsigned int        m_scrollListItem        {0};

    unsigned int        m_menuScrollPosition    {0};
    QList<LCDMenuItem> *m_lcdMenuItems          {nullptr};

    bool                m_connected             {false};
    bool                m_timeFlash             {false};

    QString             m_sendBuffer;
    QString             m_lastCommand;
    QString             m_hostname;
    unsigned int        m_port                  {13666};

    bool                m_lcdReady              {false};

    bool                m_lcdShowTime           {true};
    bool                m_lcdShowMenu           {true};
    bool                m_lcdShowGeneric        {true};
    bool                m_lcdShowMusic          {true};
    bool                m_lcdShowChannel        {true};
    bool                m_lcdShowVolume         {true};
    bool                m_lcdShowRecstatus      {true};
    bool                m_lcdBacklightOn        {true};
    bool                m_lcdHeartbeatOn        {true};
    bool                m_lcdBigClock           {true};
    std::chrono::milliseconds m_lcdPopupTime    {0ms};
    QString             m_lcdShowMusicItems;
    QString             m_lcdKeyString;
    LCDServer *         m_parentLCDServer       {nullptr};
    QString             m_startupMessage;
    std::chrono::seconds m_startupShowTime      {0s};

    bool                m_isRecording           {false};
    bool                m_isTimeVisible         {false};
    int                 m_lcdTunerNo            {0};

    std::vector<TunerStatus> m_tunerList;
};

#endif
