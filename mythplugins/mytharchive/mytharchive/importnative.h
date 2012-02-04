#ifndef IMPORTNATIVE_H_
#define IMPORTNATIVE_H_

#include <iostream>
#include <stdint.h>

// qt
#include <QString>
#include <QStringList>
#include <QKeyEvent>
#include <QList>
#include <QDateTime>

// myth
#include <mythscreentype.h>

// mytharchive
#include "fileselector.h"

typedef struct
{
    bool directory;
    bool selected;
    QString filename;
    int64_t size;
} FileInfo;

typedef struct
{
    QString title;
    QString subtitle;
    QDateTime startTime;
    QString description;
    QString chanID;
    QString chanNo;
    QString chanName;
    QString callsign;
} FileDetails;


class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

class ArchiveFileSelector : public FileSelector
{
    Q_OBJECT

  public:
    ArchiveFileSelector(MythScreenStack *parent);
    ~ArchiveFileSelector(void);

    bool Create(void);

  private slots:
    void nextPressed(void);
    void prevPressed(void);
    void cancelPressed(void);
    void itemSelected(MythUIButtonListItem *item);

  private:
    FileDetails   m_details;
    QString       m_xmlFile;

    MythUIButton *m_nextButton;
    MythUIButton *m_prevButton;
    MythUIText   *m_progTitle;
    MythUIText   *m_progSubtitle;
    MythUIText   *m_progStartTime;
};

class ImportNative : public MythScreenType
{
    Q_OBJECT

  public:
      ImportNative(MythScreenStack *parent, MythScreenType *m_previousScreen,
                   const QString &xmlFile, FileDetails details);
      ~ImportNative();

      bool Create(void);
      bool keyPressEvent(QKeyEvent *e);

  private slots:
    void finishedPressed();
    void prevPressed();
    void cancelPressed();
    void searchChanID(void);
    void searchChanNo(void);
    void searchName(void);
    void searchCallsign(void);
    void gotChanID(QString value);
    void gotChanNo(QString value);
    void gotName(QString value);
    void gotCallsign(QString value);

  private:
    void findChannelMatch(const QString &chanid, const QString &chanNo,
                          const QString &name, const QString &callsign);
    void fillSearchList(const QString &field);
    void showList(const QString &caption, QString &value, const char *slot);

    QString         m_xmlFile;
    FileDetails     m_details;
    MythScreenType *m_previousScreen;

    QStringList     m_searchList;

    MythUIText   *m_progTitle_text;
    MythUIText   *m_progDateTime_text;
    MythUIText   *m_progDescription_text;

    MythUIText   *m_chanID_text;
    MythUIText   *m_chanNo_text;
    MythUIText   *m_chanName_text;
    MythUIText   *m_callsign_text;

    MythUIText   *m_localChanID_text;
    MythUIText   *m_localChanNo_text;
    MythUIText   *m_localChanName_text;
    MythUIText   *m_localCallsign_text;

    MythUIButton *m_searchChanID_button;
    MythUIButton *m_searchChanNo_button;
    MythUIButton *m_searchChanName_button;
    MythUIButton *m_searchCallsign_button;

    MythUIButton *m_finishButton;
    MythUIButton *m_prevButton;
    MythUIButton *m_cancelButton;

    bool          m_isValidXMLSelected;
};

#endif
