#ifndef IMPORTNATIVE_H_
#define IMPORTNATIVE_H_

#include <cstdint>
#include <iostream>

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

struct FileInfo
{
    bool directory;
    bool selected;
    QString filename;
    int64_t size;
};

struct FileDetails
{
    QString title;
    QString subtitle;
    QDateTime startTime;
    QString description;
    QString chanID;
    QString chanNo;
    QString chanName;
    QString callsign;
};


class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

class ArchiveFileSelector : public FileSelector
{
    Q_OBJECT

  public:
    explicit ArchiveFileSelector(MythScreenStack *parent);
    ~ArchiveFileSelector(void);

    bool Create(void) override; // FileSelector

  private slots:
    void nextPressed(void);
    void prevPressed(void);
    void cancelPressed(void);
    void itemSelected(MythUIButtonListItem *item);

  private:
    FileDetails   m_details;
    QString       m_xmlFile;

    MythUIButton *m_nextButton    {nullptr};
    MythUIButton *m_prevButton    {nullptr};
    MythUIText   *m_progTitle     {nullptr};
    MythUIText   *m_progSubtitle  {nullptr};
    MythUIText   *m_progStartTime {nullptr};
};

class ImportNative : public MythScreenType
{
    Q_OBJECT

  public:
      ImportNative(MythScreenStack *parent, MythScreenType *previousScreen,
                   const QString &xmlFile, FileDetails details)
          : MythScreenType(parent, "ImportNative"),
            m_xmlFile(xmlFile),
            m_details(details),
            m_previousScreen(previousScreen) {}
      ~ImportNative() = default;

      bool Create(void) override; // MythScreenType
      bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

  private slots:
    void finishedPressed();
    void prevPressed();
    void cancelPressed();
    void searchChanID(void);
    void searchChanNo(void);
    void searchName(void);
    void searchCallsign(void);
    void gotChanID(const QString& value);
    void gotChanNo(const QString& value);
    void gotName(const QString& value);
    void gotCallsign(const QString& value);

  private:
    void findChannelMatch(const QString &chanid, const QString &chanNo,
                          const QString &name, const QString &callsign);
    void fillSearchList(const QString &field);
    void showList(const QString &caption, QString &value, const char *slot);

    QString         m_xmlFile;
    FileDetails     m_details;
    MythScreenType *m_previousScreen      {nullptr};

    QStringList     m_searchList;

    MythUIText   *m_progTitle_text        {nullptr};
    MythUIText   *m_progDateTime_text     {nullptr};
    MythUIText   *m_progDescription_text  {nullptr};

    MythUIText   *m_chanID_text           {nullptr};
    MythUIText   *m_chanNo_text           {nullptr};
    MythUIText   *m_chanName_text         {nullptr};
    MythUIText   *m_callsign_text         {nullptr};

    MythUIText   *m_localChanID_text      {nullptr};
    MythUIText   *m_localChanNo_text      {nullptr};
    MythUIText   *m_localChanName_text    {nullptr};
    MythUIText   *m_localCallsign_text    {nullptr};

    MythUIButton *m_searchChanID_button   {nullptr};
    MythUIButton *m_searchChanNo_button   {nullptr};
    MythUIButton *m_searchChanName_button {nullptr};
    MythUIButton *m_searchCallsign_button {nullptr};

    MythUIButton *m_finishButton          {nullptr};
    MythUIButton *m_prevButton            {nullptr};
    MythUIButton *m_cancelButton          {nullptr};

    bool          m_isValidXMLSelected    {false};
};

#endif
