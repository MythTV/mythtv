/*
	titledialog.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    the dialog where you actually choose titles to rip
*/

#ifndef TITLEDIALOG_H_
#define TITLEDIALOG_H_

#include <qtimer.h>
#include <qsocket.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "dvdinfo.h"

class TitleDialog : public MythThemedDialog
{
    Q_OBJECT

  public:
  
    TitleDialog(QSocket *a_socket, 
                QString d_name, 
                QPtrList<DVDTitleInfo> *titles,
                MythMainWindow *parent, 
                QString window_name,
                QString theme_filename,
                const char* name = 0);
   ~TitleDialog();

    void keyPressEvent(QKeyEvent *e);


  public slots:
  
    void showCurrentTitle();
    void viewTitle();
    void nextTitle();
    void prevTitle();
    void gotoTitle(uint title_number);
    void toggleTitle(bool);
    void changeName(QString new_name);
    void setAudio(int);
    void setQuality(int which_quality);
    void setSubTitle(int which_subtitle);
    void toggleAC3(bool);
    void ripTitles();
    void takeFocusAwayFromEditor(bool up_or_down);
    
  private:
  
    void    wireUpTheme();
  
    QTimer                 *check_dvd_timer;
    QString                disc_name;
    QPtrList<DVDTitleInfo> *dvd_titles;
    DVDTitleInfo           *current_title;
    QSocket                *socket_to_mtd;

    MythRemoteLineEdit     *name_editor;

    //
    //  GUI "widgets"
    //
    
    UIBlackHoleType     *editor_hack;
    UISelectorType      *audio_select;
    UISelectorType      *quality_select;
    UISelectorType      *subtitle_select;
    UICheckBoxType      *ripcheck;
    UICheckBoxType      *ripacthree;
    UITextType          *playlength_text;
    UITextType          *numb_titles_text;
    UIPushButtonType    *view_button;
    UIPushButtonType    *next_title_button;
    UIPushButtonType    *prev_title_button;
    UITextButtonType    *ripaway_button;
};

#endif
