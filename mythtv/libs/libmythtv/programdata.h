#ifndef _PROGRAMDATA_H_
#define _PROGRAMDATA_H_

// Qt headers
#include <qstring.h>
#include <qdatetime.h>
#include <qvaluelist.h>
#include <qmap.h>

// MythTV headers
#include "mythexp.h"

struct ProgRating
{
    QString system;
    QString rating;
};

struct ProgCredit
{
    QString role;
    QString name;
};

class ProgInfo
{
  public:
    ProgInfo() { }
    ProgInfo(const ProgInfo &other) { channel = other.channel;
                                      startts = other.startts;
                                      endts = other.endts;
                                      start = other.start;
                                      end = other.end;
                                      title = other.title;
                                      subtitle = other.subtitle;
                                      desc = other.desc;
                                      category = other.category;
                                      catType = other.catType;
                                      airdate = other.airdate;
                                      stars = other.stars;
                                      previouslyshown = other.previouslyshown;
                                      title_pronounce = other.title_pronounce;
                                      stereo = other.stereo;
                                      subtitled = other.subtitled;
                                      hdtv = other.hdtv;
                                      closecaptioned = other.closecaptioned;
                                      partnumber = other.partnumber;
                                      parttotal = other.parttotal;
                                      seriesid = other.seriesid;
                                      originalairdate = other.originalairdate;
                                      showtype = other.showtype;
                                      colorcode = other.colorcode;
                                      syndicatedepisodenumber = other.syndicatedepisodenumber;
                                      programid = other.programid;
        
                                      clumpidx = other.clumpidx;
                                      clumpmax = other.clumpmax;
                                      ratings = other.ratings;
                                      credits = other.credits;
                                      content = other.content;
                                    }


    QString channel;
    QString startts;
    QString endts;
    QDateTime start;
    QDateTime end;
    QString title;
    QString subtitle;
    QString desc;
    QString category;
    QString catType;
    QString airdate;
    QString stars;
    bool previouslyshown;
    QString title_pronounce;
    bool stereo;
    bool subtitled;
    bool hdtv;
    bool closecaptioned;
    QString partnumber;
    QString parttotal;
    QString seriesid;                                
    QString originalairdate;
    QString showtype;
    QString colorcode;
    QString syndicatedepisodenumber;
    QString programid;

    QString clumpidx;
    QString clumpmax;
    QValueList<ProgRating> ratings;
    QValueList<ProgCredit> credits;
    QString content;
};

class MPUBLIC ProgramData
{
  public:
    ProgramData() : quiet(false), no_delete(false), listing_wrap_offset(0) {}

    void clearOldDBEntries(void);
    void handlePrograms(int id,
                        QMap<QString, QValueList<ProgInfo> > *proglist);

    static int  fix_end_times(void);
    static void clearDataByChannel(int chanid, QDateTime from, QDateTime to);
    static void clearDataBySource(int sourceid, QDateTime from, QDateTime to);

  private:
    void fixProgramList(QValueList<ProgInfo> *fixlist);

  public:
    bool quiet;
    bool no_delete;
    int  listing_wrap_offset;
};

#endif // _PROGRAMDATA_H_
