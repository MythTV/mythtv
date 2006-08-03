#ifndef IMPORTNATIVEWIZARD_H_
#define IMPORTNATIVEWIZARD_H_

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
} FileInfo;

class ImportNativeWizard : public MythThemedDialog
{

  Q_OBJECT

  public:

      ImportNativeWizard(const QString &startDir, 
                   const QString &filemask, MythMainWindow *parent,
                   const QString &window_name, const QString &theme_filename,
                   const char *name = 0);
      ~ImportNativeWizard();

  private slots:
    void keyPressEvent(QKeyEvent *e);
    void nextPressed();
    void prevPressed();
    void cancelPressed();
    void backPressed();
    void homePressed();
    void locationEditLostFocus();
    void selectedChanged(UIListBtnTypeItem *item);
    void searchChanID();
    void searchChanNo();
    void searchName();
    void searchCallsign();

  private:
    void updateFileList();
    void updateWidgets(void);
    void wireUpTheme(void);
    void updateScrollArrows(void);
    void loadXML(const QString &filename);
    void findChannelMatch(const QString &chanid, const QString &chanNo,
                     const QString &name, const QString &callsign);
    void fillSearchList(const QString &field);
    bool showList(const QString &caption, QString &value);

    QString            m_filemask;
    QString            m_curDirectory;
    QPtrList<FileInfo> m_fileData;
    QStringList        m_selectedList;

    //
    //  GUI stuff
    //

    // first page
    UIListBtnType        *m_fileList;
    UIRemoteEditType     *m_locationEdit;
    UITextButtonType     *m_backButton;
    UITextButtonType     *m_homeButton;
    UITextType           *m_title_text;
    UITextType           *m_subtitle_text;
    UITextType           *m_starttime_text;

    // second page
    UITextType           *m_progTitle_text;
    UITextType           *m_progDateTime_text;
    UITextType           *m_progDescription_text;

    UITextType           *m_chanID_text;
    UITextType           *m_chanNo_text;
    UITextType           *m_chanName_text;
    UITextType           *m_callsign_text;

    UITextType           *m_localChanID_text;
    UITextType           *m_localChanNo_text;
    UITextType           *m_localChanName_text;
    UITextType           *m_localCallsign_text;

    UIPushButtonType     *m_searchChanID_button;
    UIPushButtonType     *m_searchChanNo_button;
    UIPushButtonType     *m_searchChanName_button;
    UIPushButtonType     *m_searchCallsign_button;

    // common buttons
    UITextButtonType     *m_nextButton;
    UITextButtonType     *m_prevButton;
    UITextButtonType     *m_cancelButton;

    QPixmap              *m_directoryPixmap;

    QStringList           m_searchList;
    bool                  m_isValidXMLSelected;
};

#endif
