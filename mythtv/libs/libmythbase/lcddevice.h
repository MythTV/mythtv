#ifndef LCDDEVICE_H_
#define LCDDEVICE_H_

#include <iostream>

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QDateTime>
#include <QMutex>
#include <QList>

#include "mythbaseexp.h"

enum CHECKED_STATE {CHECKED = 0, UNCHECKED, NOTCHECKABLE };

class QTcpSocket;

class MBASE_PUBLIC LCDMenuItem
{
  public:
    LCDMenuItem(bool item_selected, CHECKED_STATE item_checked,
                QString item_name, unsigned int item_indent  = 0,
                bool item_scroll = false)
    {
        selected = item_selected;
        checked = item_checked;
        name = item_name;
        scroll = item_scroll;
        indent = item_indent;
        scrollPosition = indent;
    }

    CHECKED_STATE isChecked() const { return checked; }
    bool isSelected() const { return selected; }
    QString ItemName() const { return name; }
    bool Scroll() const { return scroll; }
    unsigned int getIndent() const { return indent; }
    unsigned int getScrollPos() const { return scrollPosition; }

    void setChecked(CHECKED_STATE value) { checked = value; }
    void setSelected(bool value) { selected = value; }
    void setItemName(QString value) { name = value; }
    void setScroll(bool value) { scroll = value; }
    void setIndent(unsigned int value) { indent = value; }
    void setScrollPos(unsigned int value) { scrollPosition = value; }
    void incrementScrollPos() { ++scrollPosition; }

  private:
    bool selected;
    CHECKED_STATE checked;
    QString name;
    bool scroll;
    unsigned int indent;
    unsigned int scrollPosition;
};

enum TEXT_ALIGNMENT {ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTERED };

class MBASE_PUBLIC LCDTextItem
{
  public:
    LCDTextItem(unsigned int row, TEXT_ALIGNMENT align, QString text,
                QString screen = "Generic", bool scroll = false,
                QString widget = "textWidget")
    {
        itemRow = row;
        itemAlignment = align;
        itemText = text;
        itemScreen = screen;
        itemWidget = widget;
        itemScrollable = scroll;
    }

    unsigned int getRow() const { return itemRow; }
    TEXT_ALIGNMENT getAlignment() const { return itemAlignment; }
    QString getText() const { return itemText; }
    QString getScreen() const { return itemScreen; }
    QString getWidget() const { return itemWidget; }
    int getScroll() const { return itemScrollable; }

    void setRow(unsigned int value) { itemRow = value; }
    void setAlignment(TEXT_ALIGNMENT value) { itemAlignment = value; }
    void setText(QString value) { itemText = value; }
    void setScreen(QString value) { itemScreen = value; }
    void setWidget(QString value) { itemWidget = value; }
    void setScrollable(bool value) { itemScrollable = value; }

  private:
    unsigned int itemRow;
    TEXT_ALIGNMENT itemAlignment;
    QString itemText;
    QString itemScreen;
    QString itemWidget;
    bool itemScrollable;
};

//only one active at a time
enum LCDSpeakerSet {
    SPEAKER_MASK = 0x00000030,
    SPEAKER_LR = 1 << 4,
    SPEAKER_51 = 2 << 4,
    SPEAKER_71 = 3 << 4,
};

//only one active at a time
enum LCDAudioFormatSet {
    AUDIO_MASK = 0x0000E000 | 0x00070000,

    AUDIO_MP3  = 1 << 13,
    AUDIO_OGG  = 2 << 13,
    AUDIO_WMA2 = 3 << 13,
    AUDIO_WAV  = 4 << 13,

    AUDIO_MPEG2 = 1 << 16,
    AUDIO_AC3   = 2 << 16,
    AUDIO_DTS   = 3 << 16,
    AUDIO_WMA   = 4 << 16,
};

//only one active at a time
enum LCDVideoFormatSet {
    VIDEO_MASK = 0x00380000,
    VIDEO_MPG  = 1 << 19,
    VIDEO_DIVX = 2 << 19,
    VIDEO_XVID = 3 << 19,
    VIDEO_WMV  = 4 << 19,
};

//only one active at a time
enum LCDTunerSet {
    TUNER_MASK = 0x00000080 | 0x00000800 | 0x00001000,
    TUNER_SRC  = 0x00000080,
    TUNER_SRC1 = 0x00000800,
    TUNER_SRC2 = 0x00001000,
};

//only one active at a time
enum LCDVideoSourceSet {
    VSRC_MASK = 0x00000100 | 0x00000200,
    VSRC_FIT  = 0x00000100,
    VSRC_TV   = 0x00000200,
};

//can be enabled/disabled separately
enum LCDVariousFlags {
    VARIOUS_VOL = 0x00400000,
    VARIOUS_TIME = 0x00800000,
    VARIOUS_ALARM = 0x01000000,
    VARIOUS_RECORD = 0x02000000,
    VARIOUS_REPEAT = 0x04000000,
    VARIOUS_SHUFFLE = 0x08000000,
    VARIOUS_DISC_IN = 0x20000000,
    VARIOUS_HDTV = 0x00000400,
    VARIOUS_SPDIF = 0x1 << 9,
    SPDIF_MASK = 0x00000040,
};


//only one active at a time
enum LCDFunctionSet {
    //0=none, 1=music, 2=movie, 3=photo, 4=CD/DVD, 5=TV, 6=Web, 7=News/Weather  * 2
    FUNC_MASK = 0xE,
    FUNC_MUSIC = 1 << 1,
    FUNC_MOVIE = 2 << 1,
    FUNC_PHOTO = 3 << 1,
    FUNC_DVD = 4 << 1,
    FUNC_TV = 5 << 1,
    FUNC_WEB = 6 << 1,
    FUNC_NEWS = 7 << 1,
};

class MBASE_PUBLIC LCD : public QObject
{
    Q_OBJECT

  protected:
    LCD();

    static bool m_server_unavailable;
    static LCD *m_lcd;
    static bool m_enabled;

  public:
   ~LCD();

    enum {
        MUSIC_REPEAT_NONE  = 0,
        MUSIC_REPEAT_TRACK = 1,
        MUSIC_REPEAT_ALL   = 2,
    };

    enum {
        MUSIC_SHUFFLE_NONE  = 0,
        MUSIC_SHUFFLE_RAND  = 1,
        MUSIC_SHUFFLE_SMART = 2,
        MUSIC_SHUFFLE_ALBUM = 3,
        MUSIC_SHUFFLE_ARTIST = 4
    };

    static LCD *Get(void);
    static void SetupLCD (void);

    // Used to actually connect to an LCD device
    bool connectToHost(const QString &hostname, unsigned int port);

    // When nothing else is going on, show the time
    void switchToTime();

    // Extended functionality for eg SoundGraph iMON LCD devices
    void setSpeakerLEDs(enum LCDSpeakerSet speaker, bool on);
    void setAudioFormatLEDs(enum LCDAudioFormatSet acodec, bool on);
    void setVideoFormatLEDs(enum LCDVideoFormatSet vcodec, bool on);
    void setVideoSrcLEDs(enum LCDVideoSourceSet vsrc, bool on);
    void setFunctionLEDs(enum LCDFunctionSet video, bool on);
    void setTunerLEDs(enum LCDTunerSet tuner, bool on);
    void setVariousLEDs(enum LCDVariousFlags various, bool on);

    // When playing music, switch to this and give artist and track name
    //
    // Note: the use of switchToMusic is discouraged, because it
    // has become obvious that most LCD devices cannot handle communications
    // fast enough to make them useful.
    void switchToMusic(const QString &artist, const QString &album,
                       const QString &track);

    // For Live TV, supply the channel number, program title and subtitle
    //
    // Note that the "channel" screen can be used for any kind of progress meter
    // just put whatever you want in the strings, and update the progress as
    // appropriate;
    void switchToChannel(QString channum = "", QString title = "",
                         QString subtitle = "");

    // While watching Live/Recording/Pause Buffer, occasionaly describe how
    // much of the program has been seen (between 0.0 and 1.0)
    // (e.g. [current time - start time] / [end time - start time]  )
    void setChannelProgress(const QString &time, float percentViewed);

    // Show the Menu
    // QPtrList is a pointer to a bunch of menu items
    // See mythmusic/databasebox.cpp for an example
    void switchToMenu(QList<LCDMenuItem> &menuItems, QString app_name = "",
                      bool popMenu = true);

    // Show the Generic Progress
    // QPtrList contains pointers to LCDTextItem objects which allow you to
    // define the screen, row, and alignment of the text
    void switchToGeneric(QList<LCDTextItem> &textItems);

    /** \brief Update the generic progress bar.
        \param generic_progress a value between 0 and 1.0
    */
    void setGenericProgress(float generic_progress);

    /** \brief Update the generic screen to display a busy spinner.
        \note The LCD busy spinner only 'moves' when this is called
              instead of the lcdserver just handling it itself.
    */
    void setGenericBusy();

    // Do a music progress bar with the generic level between 0 and 1.0
    void setMusicProgress(QString time, float generic_progress);

    /** \brief Set music player's repeat properties
        \param repeat the state of repeat
    */
    void setMusicRepeat(int repeat);

    /** \brief Set music player's shuffle properties
        \param shuffle the state of shuffle
    */
    void setMusicShuffle(int shuffle);

    // Show the Volume Level top_text scrolls
    void switchToVolume(QString app_name);

    // Do a progress bar with the volume level between 0 and 1.0
    void setVolumeLevel(float volume_level);

    // If some other process should be getting all the LCDd screen time (e.g.
    // mythMusic) we can use this to try and prevent and screens from showing
    // up without having to actual destroy the LCD object
    void switchToNothing();

    // If you want to be pleasant, call shutdown() before deleting your LCD
    // device
    void shutdown();

    void setupLEDs(int(*LedMaskFunc)(void));

    void stopAll(void);

    uint getLCDHeight(void) { return lcd_height; }
    uint getLCDWidth(void) { return lcd_width; }

    void resetServer(void); // tell the mythlcdserver to reload its settings

  private slots:
    void restartConnection();      // Try to re-establish the connection to
                                   // LCDServer every 10 seconds
    void outputLEDs();

  private:
    bool startLCDServer(void);
    void sendToServer(const QString &someText);
    void init();
    void handleKeyPress(QString key);
    QString quotedString(const QString &s);
    void describeServer();

  private slots:
    void ReadyRead(void);
    void Disconnected(void);

  private:
    QTcpSocket *socket;
    QMutex   socketLock;
    QString  hostname;
    uint     port;
    bool     bConnected;

    QTimer *retryTimer;
    QTimer *LEDTimer;

    QString send_buffer;
    QString last_command;

    int  lcd_width;
    int  lcd_height;

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
    int  lcd_popuptime;
    QString lcd_showmusic_items;
    QString lcd_keystring;

    int lcd_ledmask;

    int (*GetLEDMask)(void);
};

#endif
