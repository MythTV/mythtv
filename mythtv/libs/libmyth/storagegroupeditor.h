#ifndef _STORAGEGROUPEDITOR_H
#define _STORAGEGROUPEDITOR_H

#include "storagegroup.h"
#include "settings.h"
#include "standardsettings.h"
#include "mythwidgets.h"
#include "mythexp.h"

class MPUBLIC StorageGroupEditor :
    public GroupSetting
{
    Q_OBJECT
  public:
    explicit StorageGroupEditor(QString group);
    virtual void Load(void);
    virtual bool canDelete(void);

  protected slots:
    void DoDeleteSlot(bool doDelete);
    void ShowFileBrowser(void);

  protected:
    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *event);
    void ShowDeleteDialog();
    void SetLabel(void);
    QString         m_group;
};

class MPUBLIC StorageGroupListEditor :
    public GroupSetting
{
    Q_OBJECT
  public:
    StorageGroupListEditor(void);
    virtual void Load(void);
    void AddSelection(const QString &label, const QString &value);

public slots:
    void ShowNewGroupDialog(void);
    void CreateNewGroup(QString name);
};

class StorageGroupDirStorage : public SimpleDBStorage
{
  public:
    StorageGroupDirStorage(StorageUser *_user, int id,
                           const QString &group);

  protected:
    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    int m_id;
    QString m_group;
};

class StorageGroupDirSetting : public MythUIFileBrowserSetting
{
    Q_OBJECT

  public:
      StorageGroupDirSetting(int id, const QString &group);

    bool keyPressEvent(QKeyEvent *event);

    void ShowDeleteDialog();

  protected slots:
    void DoDeleteSlot(bool doDelete);

  protected:
    int m_id;
    QString m_group;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
