#ifndef DIRECTORYFINDER_H_
#define DIRECTORYFINDER_H_

#include <iostream>

// qt
#include <qstring.h>
#include <qstringlist.h>
//Added by qt3to4:
#include <QKeyEvent>
#include <QPixmap>

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

class DirectoryFinder : public MythThemedDialog
{

  Q_OBJECT

  public:

      DirectoryFinder(const QString &startDir,
                      MythMainWindow *parent,
                      const char *name = 0);
      ~DirectoryFinder();

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

    QString m_curDirectory;
    QStringList        m_directoryList;

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
