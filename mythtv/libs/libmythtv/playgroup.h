#ifndef PLAYGROUP_H
#define PLAYGROUP_H

#include <QStringList>

#include "libmythui/standardsettings.h"
#include "libmythtv/mythtvexp.h"

class ProgramInfo;

class MTV_PUBLIC PlayGroup
{
  public:
    static QStringList GetNames(void);
    static int GetCount(void);
    static QString GetInitialName(const ProgramInfo *pi);
    static int GetSetting(const QString &name, const QString &field,
                          int defval);
    template <typename T>
        static std::enable_if_t<std::chrono::__is_duration<T>::value, T>
        GetDurSetting(const QString &name, const QString &field, T defval)
    { return T(GetSetting(name, field, static_cast<int>(defval.count()))); }
};

class MTV_PUBLIC PlayGroupEditor : public GroupSetting
{
    Q_OBJECT

  public:
    PlayGroupEditor(void);
    void Load(void) override; // StandardSetting

  public slots:
    void CreateNewPlayBackGroup() const;
    void CreateNewPlayBackGroupSlot(const QString &name);

  private:
    ButtonStandardSetting *m_addGroupButton {nullptr};
};

class MTV_PUBLIC PlayGroupConfig : public GroupSetting
{
    Q_OBJECT

  public:
    PlayGroupConfig(const QString &label, const QString &name, bool isNew=false);
    void updateButton(MythUIButtonListItem *item) override; // GroupSetting
    void Save() override; // StandardSetting
    bool canDelete(void) override; // GroupSetting
    void deleteEntry(void) override; // GroupSetting

  private:
    StandardSetting            *m_titleMatch  {nullptr};
    MythUISpinBoxSetting       *m_skipAhead   {nullptr};
    MythUISpinBoxSetting       *m_skipBack    {nullptr};
    MythUISpinBoxSetting       *m_jumpMinutes {nullptr};
    MythUISpinBoxSetting       *m_timeStrech  {nullptr};
    bool                        m_isNew       {false};
};

#endif
