/*
	titledialog.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    the dialog where you actually choose titles to rip
*/

#ifndef TITLEDIALOG_H_
#define TITLEDIALOG_H_

#include <qtimer.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/mythcontext.h>

#include "dvdinfo.h"

class TitleDialog : public MythDialog
{
    Q_OBJECT

  public:
  
    TitleDialog(QSocket *a_socket, QString d_name, QPtrList<DVDTitleInfo> *titles, MythMainWindow *parent, const char* name = 0);
   ~TitleDialog();

  public slots:
  
    void viewTitle();
    void setQuality(int);
    void setAudio(int);
    void toggleTitle(bool);
    void changeName(QString new_name);
    void ripTitles();

  private:
  
    QVBoxLayout            *bigvb;
    QVBoxLayout            *vbox;
    QFrame                 *firstdiag;
    QLabel                 *info_text;
    QTimer                 *check_dvd_timer;

    QString                disc_name;
    QPtrList<DVDTitleInfo> *dvd_titles;
    QSocket                *socket_to_mtd;
};

#endif
