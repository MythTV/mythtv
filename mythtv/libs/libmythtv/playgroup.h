#ifndef PLAYGROUP_H
#define PLAYGROUP_H

#include <QStringList>

#include "mythtvexp.h"
#include "standardsettings.h"

class ProgramInfo;

class MTV_PUBLIC PlayGroup
{
  public:
    static QStringList GetNames(void);
    static int GetCount(void);
    static QString GetInitialName(const ProgramInfo *pi);
    static int GetSetting(const QString &name, const QString &field,
                          int defval);
};

class MTV_PUBLIC PlayGroupEditor : public GroupSetting
{
    Q_OBJECT

  public:
    PlayGroupEditor(void);
    virtual void Load(void);

  public slots:
    void CreateNewPlayBackGroup();
    void CreateNewPlayBackGroupSlot(const QString&);

  private:
    ButtonStandardSetting *m_addGroupButton;
};

class MTV_PUBLIC PlayGroupConfig : public GroupSetting
{
    Q_OBJECT

  public:
    PlayGroupConfig(const QString &label, const QString &name, bool isNew=false);
    virtual void updateButton(MythUIButtonListItem *item);
    virtual void Save();
    virtual bool canDelete(void);
    virtual void deleteEntry(void);

  private:
    StandardSetting            *m_titleMatch;
    MythUISpinBoxSetting       *m_skipAhead;
    MythUISpinBoxSetting       *m_skipBack;
    MythUISpinBoxSetting       *m_jumpMinutes;
    MythUISpinBoxSetting       *m_timeStrech;
    bool                        m_isNew;
};

#endif
