#ifndef IMPORTNATIVE_H_
#define IMPORTNATIVE_H_

#include <cstdint>
#include <iostream>
#include <utility>

// qt
#include <QDateTime>
#include <QKeyEvent>
#include <QList>
#include <QString>
#include <QStringList>

// myth
#include <libmythui/mythscreentype.h>

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
    ~ArchiveFileSelector(void) override;

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
                   QString xmlFile, FileDetails details)
          : MythScreenType(parent, "ImportNative"),
            m_xmlFile(std::move(xmlFile)),
            m_details(std::move(details)),
            m_previousScreen(previousScreen) {}
      ~ImportNative() override = default;

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
    using INSlot = void (ImportNative::*)(const QString&);
    void showList(const QString &caption, QString &value, INSlot slot);

    QString         m_xmlFile;
    FileDetails     m_details;
    MythScreenType *m_previousScreen      {nullptr};

    QStringList     m_searchList;

    MythUIText   *m_progTitleText         {nullptr};
    MythUIText   *m_progDateTimeText      {nullptr};
    MythUIText   *m_progDescriptionText   {nullptr};

    MythUIText   *m_chanIDText            {nullptr};
    MythUIText   *m_chanNoText            {nullptr};
    MythUIText   *m_chanNameText          {nullptr};
    MythUIText   *m_callsignText          {nullptr};

    MythUIText   *m_localChanIDText       {nullptr};
    MythUIText   *m_localChanNoText       {nullptr};
    MythUIText   *m_localChanNameText     {nullptr};
    MythUIText   *m_localCallsignText     {nullptr};

    MythUIButton *m_searchChanIDButton    {nullptr};
    MythUIButton *m_searchChanNoButton    {nullptr};
    MythUIButton *m_searchChanNameButton  {nullptr};
    MythUIButton *m_searchCallsignButton  {nullptr};

    MythUIButton *m_finishButton          {nullptr};
    MythUIButton *m_prevButton            {nullptr};
    MythUIButton *m_cancelButton          {nullptr};

    bool          m_isValidXMLSelected    {false};
};

#endif
