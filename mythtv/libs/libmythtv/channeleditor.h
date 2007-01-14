#ifndef CHANNELEDITOR_H
#define CHANNELEDITOR_H

#include <qsqldatabase.h>

#include "settings.h"

class SourceSetting;
class ChannelListSetting;
class MPUBLIC ChannelEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT
public:
    ChannelEditor();
    virtual int exec();

    MythDialog* dialogWidget(MythMainWindow *parent, const char* name);

public slots:
    void menu(int);
    void del();
    void edit();
    void edit(int);
    void scan(void);
    void transportEditor();
    void deleteChannels();

private:
    int                 id;
    SourceSetting      *source;
    ChannelListSetting *list;
    TransButtonSetting *buttonScan;
    TransButtonSetting *buttonTransportEditor;
};

class ChannelID;

class MPUBLIC ChannelWizard : public QObject, public ConfigurationWizard
{
    Q_OBJECT
public:
    ChannelWizard(int id, int default_sourceid);
    QString getCardtype();
    bool cardTypesInclude(const QString& cardtype); 
    int countCardtypes();

private:
    ChannelID *cid;
};

class ChannelListSetting : public ListBoxSetting, public Storage
{
    Q_OBJECT
public:
    ChannelListSetting() : ListBoxSetting(this)
    {
        currentSourceID = "";
        currentSortMode = QObject::tr("Channel Name");
        currentHideMode = false;
    };

    void load(void) { fillSelections(); }
    void save(void) { }
    void save(QString /*destination*/) { }

    QString getSourceID() { return currentSourceID; };
    QString getSortMode() { return currentSortMode; };
    bool getHideMode() { return currentHideMode; };

public slots:
    void fillSelections(void);
    void setSortMode(const QString& sort) {
        if (currentSortMode != sort) {
            currentSortMode = sort;
            fillSelections();
        }
    };

    void setSourceID(const QString& sourceID) {
        if (currentSourceID != sourceID) {
            currentSourceID = sourceID;
            fillSelections();
        }
    };

    void setHideMode(bool hide) {
        if (currentHideMode != hide) {
            currentHideMode = hide;
            fillSelections();
        }
    };

private:
    QString currentSourceID;
    QString currentSortMode;
    bool currentHideMode;
};

#endif
