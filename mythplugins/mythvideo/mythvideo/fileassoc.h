#ifndef FILEASSOC_H_
#define FILEASSOC_H_

/*
	fileassoc.h

	(c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
	Part of the mythTV project
	
    Classes to manipulate the file associations stored
    in the videotypes table (in the mythconverg database)

*/

#include <iostream>
using namespace std;

#include <qptrlist.h>
#include <qsqldatabase.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

class FileAssociation
{
    //
    //  Simple data structure to hold
    //
    
  public:
  
    FileAssociation(QSqlDatabase *ldb,
                    const QString &new_extension);
    
    FileAssociation(QSqlDatabase *ldb,
                    int   i,
                    const QString &e,
                    const QString &p,
                    bool  g,
                    bool  u);

    int     getID(){return id;}
    QString getExtension(){return extension;}
    QString getCommand(){return player_command;}
    bool    getDefault(){return use_default;}
    bool    getIgnore(){return ignore;}

    void    setChanged(){changed = true;}
    bool    getChanged(){return changed;}
    void    saveYourself();
    void    deleteYourselfFromDB();
    
    void    setDefault(bool yes_or_no){use_default = yes_or_no;}
    void    setIgnore(bool yes_or_no){ignore = yes_or_no;}
    void    setCommand(const QString &new_command){player_command = new_command;}

  private:
  
    int          id;
    QString      extension;
    QString      player_command;
    bool         ignore;
    bool         use_default;
    bool         changed;
    QSqlDatabase *db;
    bool         loaded_from_db;
    
};

class FileAssocDialog : public MythThemedDialog
{

  Q_OBJECT
  
    //
    //  Dialog to manipulate the data
    //
    
  public:
  
    FileAssocDialog(QSqlDatabase *ldb,
                    MythMainWindow *parent, 
                    QString window_name,
                    QString theme_filename,
                    const char* name = 0);
    ~FileAssocDialog();

    void keyPressEvent(QKeyEvent *e);
    void loadFileAssociations();
    void saveFileAssociations();
    void showCurrentFA();
    void wireUpTheme();

  public slots:
  
    void takeFocusAwayFromEditor(bool up_or_down);
    void switchToFA(int which_one);
    void saveAndExit();
    void toggleDefault(bool on_or_off);
    void toggleIgnore(bool on_or_off);
    void setPlayerCommand(QString);
    void deleteCurrent();
    void makeNewExtension();
    void createExtension();
    void removeExtensionPopup();

  private:
  
    QPtrList<FileAssociation>   file_associations;
    FileAssociation             *current_fa;
    QSqlDatabase                *db;    

    //
    //  GUI stuff
    //  
    
    MythRemoteLineEdit  *command_editor;
    UISelectorType      *extension_select;
    UIBlackHoleType     *command_hack;
    UICheckBoxType      *default_check;
    UICheckBoxType      *ignore_check;
    UITextButtonType    *done_button;
    UITextButtonType    *new_button;
    UITextButtonType    *delete_button;
 
    //
    //  Stuff for new extension
    //   
    
    MythPopupBox        *new_extension_popup;
    MythRemoteLineEdit  *new_extension_editor;
};


#endif
