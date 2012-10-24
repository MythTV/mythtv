#ifndef _STORAGEGROUPEDITOR_H
#define _STORAGEGROUPEDITOR_H

#include "storagegroup.h"
#include "settings.h"
#include "mythwidgets.h"
#include "mythexp.h"

class MPUBLIC StorageGroupEditor :
    public QObject, public ConfigurationDialog
{
    Q_OBJECT
  public:
    StorageGroupEditor(QString group);
    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }
    virtual void Load(void);
    virtual void Save(void) { }
    virtual void Save(QString) { }
    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetname=0);

  protected slots:
    void open(QString name);
    void doDelete(void);

  protected:
    QString         m_group;
    ListBoxSetting *listbox;
    QString         lastValue;
};

class MPUBLIC StorageGroupListEditor :
    public QObject, public ConfigurationDialog
{
    Q_OBJECT
  public:
    StorageGroupListEditor(void);
    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }
    virtual void Load(void);
    virtual void Save(void) { }
    virtual void Save(QString) { }
    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetname=0);

  protected slots:
    void open(QString name);
    void doDelete(void);

  protected:
    ListBoxSetting *listbox;
    QString         lastValue;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
