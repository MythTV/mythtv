#ifndef FILESELECTOR_H_
#define FILESELECTOR_H_

#include <iostream>

// qt
#include <qstring.h>
#include <qstringlist.h>

// myth
#include <mythtv/mythdialogs.h>
#include <mythtv/uilistbtntype.h>

typedef struct
{
    bool directory;
    bool selected;
    QString filename;
    long long size;
} FileData;

typedef enum
{
    FSTYPE_FILELIST = 0,
    FSTYPE_FILE = 1,
    FSTYPE_DIRECTORY = 2
} FSTYPE;

class FileSelector : public MythThemedDialog
{

  Q_OBJECT

  public:

      FileSelector(FSTYPE type, const QString &startDir, 
                   const QString &filemask, MythMainWindow *parent,
                   const QString &window_name, const QString &theme_filename,
                   const char *name = 0);
      ~FileSelector();

      QString getSelected(void);

  private slots:
    void keyPressEvent(QKeyEvent *e);
    void OKPressed();
    void cancelPressed();
    void backPressed();
    void homePressed();
    void locationEditLostFocus();

  private:
    void updateFileList();
    void updateSelectedList();
    void updateWidgets(void);
    void wireUpTheme(void);
    void updateScrollArrows(void);

    FSTYPE  m_selectorType;
    QString m_filemask;
    QString m_curDirectory;
    QPtrList<FileData> m_fileData;
    QStringList        m_selectedList;

    //
    //  GUI stuff
    //
    UIListBtnType        *m_fileList;
    UIRemoteEditType     *m_locationEdit;
    UITextButtonType     *m_okButton;
    UITextButtonType     *m_cancelButton;
    UITextButtonType     *m_backButton;
    UITextButtonType     *m_homeButton;

    QPixmap              *m_directoryPixmap;
};

#endif
