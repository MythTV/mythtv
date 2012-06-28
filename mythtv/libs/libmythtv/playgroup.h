#ifndef PLAYGROUP_H
#define PLAYGROUP_H

#include <QStringList>

#include "mythtvexp.h"
#include "settings.h"

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

class MTV_PUBLIC PlayGroupEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    PlayGroupEditor(void);
    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }
    virtual void Load(void);
    virtual void Save(void) { }
    virtual void Save(QString) { }
    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);

  protected slots:
    void open(QString name);
    void doDelete(void);

  protected:
    ListBoxSetting *listbox;
    QString         lastValue;
};

#endif
