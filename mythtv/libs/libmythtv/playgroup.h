#ifndef PLAYGROUP_H
#define PLAYGROUP_H

#include "qstringlist.h"
#include "libmyth/settings.h"
#include "libmyth/mythwidgets.h"

class ProgramInfo;

class MPUBLIC PlayGroup: public ConfigurationWizard
{
 public:
    PlayGroup(QString _name);
    QString getName(void) const { return name; }

    static QStringList GetNames(void);
    static int GetCount(void);
    static QString GetInitialName(const ProgramInfo *pi);
    static int GetSetting(const QString &name, const QString &field, 
                          int defval);

 private:
    QString name;
};

class MPUBLIC PlayGroupEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
 public:
    PlayGroupEditor(void);
    virtual int exec(void);
    virtual void load(void);
    virtual void save(void) { };
    virtual void save(QString) { };
    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);

protected slots:
    void open(QString name);
    void doDelete(void);

 protected:
    QString lastValue;
};

#endif
