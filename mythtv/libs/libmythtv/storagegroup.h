#ifndef _STORAGEGROUP_H
#define _STORAGEGROUP_H

#include <qstringlist.h>
#include <qdeepcopy.h>

#include "libmyth/settings.h"
#include "libmyth/mythwidgets.h"

class MPUBLIC StorageGroup: public ConfigurationWizard
{
  public:
    StorageGroup(const QString group = "", const QString hostname = "");

    void    Init(const QString group = "Default",
                 const QString hostname = "");

    QString getName(void) const
        { return QDeepCopy<QString>(m_groupname); }

    QStringList GetDirList(void) const
        { return QDeepCopy<QStringList>(m_dirlist); }

    QString FindRecordingFile(QString filename);
    QString FindRecordingDir(QString filename);

    QString FindNextDirMostFree(void);

    static void CheckAllStorageGroupDirs(void);

    static const char *kDefaultStorageDir;
    static const QStringList kSpecialGroups;

    static QStringList getRecordingsGroups(void);

  private:
    QString      m_groupname;
    QString      m_hostname;
    QStringList  m_dirlist;
};

class MPUBLIC StorageGroupEditor :
    public QObject, public ConfigurationDialog
{
    Q_OBJECT
  public:
    StorageGroupEditor(QString group);
    virtual DialogCode exec(void);
    virtual void load(void);
    virtual void save(void) { };
    virtual void save(QString) { };
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
    virtual void load(void);
    virtual void save(void) { };
    virtual void save(QString) { };
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
