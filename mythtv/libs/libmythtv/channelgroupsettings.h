#ifndef CHANNELGROUPSETTINGS_H
#define CHANNELGROUPSETTINGS_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythtvexp.h"
#include "settings.h"

class MTV_PUBLIC ChannelGroupConfig: public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(ChannelGroupConfig)

 public:
    ChannelGroupConfig(QString _name);
    QString getName(void) const { return name; }

 private:
    QString name;
};

class MTV_PUBLIC ChannelGroupEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    ChannelGroupEditor(void);
    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }
    virtual void Load(void);
    virtual void Save(void) { };
    virtual void Save(QString) { };
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
