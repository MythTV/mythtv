#ifndef STORAGEGROUPEDITOR_H
#define STORAGEGROUPEDITOR_H

#include "libmythbase/storagegroup.h"
#include "standardsettings.h"
#include "mythuiexp.h"

class MUI_PUBLIC StorageGroupEditor :
    public GroupSetting
{
    Q_OBJECT
  public:
    explicit StorageGroupEditor(QString group);
    void Load(void) override; // StandardSetting
    bool canDelete(void) override; // GroupSetting

  protected slots:
    void DoDeleteSlot(bool doDelete);
    void ShowFileBrowser(void);

  protected:
    bool keyPressEvent(QKeyEvent *event) override; // StandardSetting
    void customEvent(QEvent *event) override; // QObject
    void ShowDeleteDialog();
    void SetLabel(void);
    QString         m_group;
};

class MUI_PUBLIC StorageGroupListEditor :
    public GroupSetting
{
    Q_OBJECT
  public:
    StorageGroupListEditor(void);
    void Load(void) override; // StandardSetting
    void AddSelection(const QString &label, const QString &value);

public slots:
    void ShowNewGroupDialog(void) const;
    void CreateNewGroup(const QString& name);
};

class StorageGroupDirStorage : public SimpleDBStorage
{
  public:
    StorageGroupDirStorage(StorageUser *_user, int id,
                           QString group);

  protected:
    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage

    int m_id;
    QString m_group;
};

class StorageGroupDirSetting : public MythUIFileBrowserSetting
{
    Q_OBJECT

  public:
      StorageGroupDirSetting(int id, const QString &group);
      ~StorageGroupDirSetting() override;

    bool keyPressEvent(QKeyEvent *event) override; // StandardSetting

    void ShowDeleteDialog();

  protected slots:
    void DoDeleteSlot(bool doDelete);

  protected:
    int m_id;
    QString m_group;
};

#endif // STORAGEGROUPEDITOR_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
