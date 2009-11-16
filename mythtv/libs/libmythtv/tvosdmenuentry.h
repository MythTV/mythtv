#ifndef TVOSDMENUENTRY_H_
#define TVOSDMENUENTRY_H_

#include <QStringList>
#include <QMetaType>
#include <QMutex>

#include "tv.h"
#include "mythexp.h"

class MPUBLIC TVOSDMenuEntry
{
    public:
        TVOSDMenuEntry(QString category_entry, int livetv_setting,
                    int recorded_setting, int video_setting,
                    int dvd_setting, QString description_entry)
        {
            category = category_entry;
            livetv = livetv_setting;
            recorded = recorded_setting;
            video = video_setting;
            dvd = dvd_setting,
            description = description_entry;

        }
        ~TVOSDMenuEntry(void) {}
        QString GetCategory(void) { return category; }
        QString GetDescription(void) { return description; }
        int LiveTVSetting(void) { return livetv; }
        int RecordedSetting(void) { return recorded; }
        int VideoSetting(void) { return video; }
        int DVDSetting(void) { return dvd; }
        int GetEntry(TVState state);
        QStringList GetData(void);
        void UpdateEntry(int change, TVState state);
        void UpdateEntry(int livetv_entry, int recorded_entry,
                        int video, int dvd);
        void UpdateDBEntry(void);
        void CreateDBEntry(void);

    private:
        QString category;
        int livetv;
        int recorded;
        int video;
        int dvd;
        QString description;
        QMutex  updateEntryLock;

};

Q_DECLARE_METATYPE(TVOSDMenuEntry*)

class MPUBLIC TVOSDMenuEntryList
{
    public:
        TVOSDMenuEntryList(void);
        ~TVOSDMenuEntryList(void);
        bool ShowOSDMenuOption(QString category, TVState state);
        void UpdateDB(void);
        int  GetCount(void);
        QListIterator<TVOSDMenuEntry*> GetIterator(void);
        TVOSDMenuEntry* FindEntry(QString category);

    private:
        int GetEntriesFromDB(void);
        void InitDefaultEntries(void);

        QList<TVOSDMenuEntry*> curMenuEntries;
};

#endif
