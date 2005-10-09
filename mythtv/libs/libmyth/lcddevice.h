#ifndef LCDDEVICE_H_
#define LCDDEVICE_H_

#include <iostream>
#include <qobject.h>
#include <qstringlist.h>
#include <qvaluevector.h>
#include <qsocket.h>
#include <qtimer.h>
#include <qdatetime.h>

using namespace std;

enum CHECKED_STATE {CHECKED = 0, UNCHECKED, NOTCHECKABLE };

class LCDMenuItem
{
  public:
    LCDMenuItem() {}
    LCDMenuItem(bool item_selected, CHECKED_STATE item_checked,
                QString item_name, unsigned int item_indent  = 0)
    {
        selected = item_selected;
        checked = item_checked;
        name = item_name;
        indent = item_indent;
        scrollPosition = indent;
    }

   ~LCDMenuItem() {}

    CHECKED_STATE isChecked() { return checked; }
    bool isSelected() { return selected; }
    QString ItemName() { return name; }
    bool Scroll() { return scroll; }
    unsigned int getIndent() { return indent; }
    unsigned int getScrollPos() { return scrollPosition; }

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

class LCDTextItem
{
  public:
    LCDTextItem() {}
    LCDTextItem(unsigned int row, TEXT_ALIGNMENT align, QString text,
                QString screen = "Generic", bool scroll = false)
    {
        itemRow = row;
        itemAlignment = align;
        itemText = text;
        itemScreen = screen;
        itemScrollable = scroll;
    }

   ~LCDTextItem(){};

    unsigned int getRow() { return itemRow; }
    TEXT_ALIGNMENT getAlignment() { return itemAlignment; }
    QString getText() { return itemText; }
    QString getScreen() { return itemScreen; }
    int getScroll() { return itemScrollable; }

    void setRow(unsigned int value) { itemRow = value; }
    void setAlignment(TEXT_ALIGNMENT value) { itemAlignment = value; }
    void setText(QString value) { itemText = value; }
    void setScreen(QString value) { itemScreen = value; }
    void setScrollable(bool value) { itemScrollable = value; }

  private:
    unsigned int itemRow;
    TEXT_ALIGNMENT itemAlignment;
    QString itemText;
    QString itemScreen;
    bool itemScrollable;
};

class LCD : public QObject
{
    Q_OBJECT

  protected:
    LCD();

    static bool m_server_unavailable;
    static class LCD * m_lcd;
    
  public:
   ~LCD();

    static class LCD * Get(void);
    static void SetupLCD (void);

    // Used to actually connect to an LCD device       
    bool connectToHost(const QString &hostname, unsigned int port);

    // When nothing else is going on, show the time
    void switchToTime();
        
    // When playing music, switch to this and give artist and track name
    //
    // Note: the use of switchToMusic and setLevels is discouraged, because it 
    // has become obvious that most LCD devices cannot handle communications 
    // fast enough to make them useful.
    void switchToMusic(const QString &artist, const QString &album, const QString &track);

    // You can set 10 (or less) equalizer values here (between 0.0 and 1.0)
    void setLevels(int numbLevels, float *values);
    
    // For Live TV, supply the channel number, program title and subtitle
    //    
    // Note that the "channel" screen can be used for any kind of progress meter
    // just put whatever you want in the strings, and update the progress as 
    // appropriate; see the demo app mythlcd for an example)
    void switchToChannel(QString channum = "", QString title = "", 
                         QString subtitle = "");

    // While watching Live/Recording/Pause Buffer, occasionaly describe how 
    // much of the program has been seen (between 0.0 and 1.0)
    // (e.g. [current time - start time] / [end time - start time]  )
    void setChannelProgress(float percentViewed);
        
    // Show the Menu
    // QPtrList is a pointer to a bunch of menu items
    // See mythmusic/databasebox.cpp for an example
    void switchToMenu(QPtrList<LCDMenuItem> *menuItems, QString app_name = "",
                      bool popMenu = true);

    // Show the Generic Progress
    // QPtrList contains pointers to LCDTextItem objects which allow you to 
    // define the screen, row, and alignment of the text
    void switchToGeneric(QPtrList<LCDTextItem> *textItems);

    // Do a progress bar with the generic level between 0 and 1.0
    void setGenericProgress(float generic_progress);

    // Do a music progress bar with the generic level between 0 and 1.0
    void setMusicProgress(QString time, float generic_progress);

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
    
    // outputText spins through the ptr list and outputs the text according to 
    // the params set in the LCDTextItem object
    // gContext->LCDsetGenericProgress(percent_heard) for an example
//    void outputText(QPtrList<LCDTextItem> *textItems);

    void setupLEDs(int(*LedMaskFunc)(void));

    void stopAll(void);
    
    uint getLCDHeight(void) { return lcd_height; }
    uint getLCDWidth(void) { return lcd_width; }
    
    void resetServer(void); // tell the mythlcdserver to reload its settings
    
  private slots: 
    void veryBadThings(int);       // Communication Errors
    void serverSendingData();      // Data coming back from LCDd

    void restartConnection();      // Try to re-establish the connection to 
                                   // LCDServer every 10 seconds
    void outputLEDs(); 
         
  private:
    void sendToServer(const QString &someText);
    void init();
    void handleKeyPress(QString key);
    QString quotedString(const QString &s);
    
    QSocket *socket;
    QTimer *retryTimer;
    QTimer *LEDTimer;
    
    void describeServer();

    bool connected;

    QString send_buffer;
    QString last_command;
    QString hostname;
    unsigned int port;

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
    
    int (*GetLEDMask)(void);
};

#endif
