#include "eit.h"
#include "qdeepcopy.h"

void Event::Reset()
{
    ServiceID = 0;
    TransportID = 0;
    NetworkID = 0;
    ATSC = false;
    clearEventValues();
}

void Event::deepCopy(const Event &e)
{
    SourcePID       = e.SourcePID;
    TransportID     = e.TransportID;
    NetworkID       = e.NetworkID;
    ServiceID       = e.ServiceID;
    EventID         = e.EventID;
    StartTime       = e.StartTime;
    EndTime         = e.EndTime;
    OriginalAirDate = e.OriginalAirDate;
    Stereo          = e.Stereo;
    HDTV            = e.HDTV;
    SubTitled       = e.SubTitled;
    ETM_Location    = e.ETM_Location;
    ATSC            = e.ATSC;
    PartNumber      = e.PartNumber;
    PartTotal       = e.PartTotal;

    // QString is not thread safe, so we must make deep copies
    LanguageCode       = QDeepCopy<QString>(e.LanguageCode);
    Event_Name         = QDeepCopy<QString>(e.Event_Name);
    Event_Subtitle     = QDeepCopy<QString>(e.Event_Subtitle);
    Description        = QDeepCopy<QString>(e.Description);
    ContentDescription = QDeepCopy<QString>(e.ContentDescription);
    Year               = QDeepCopy<QString>(e.Year);
    CategoryType       = QDeepCopy<QString>(e.CategoryType);
    Actors             = QDeepCopy<QStringList>(e.Actors);

    Credits.clear();
    QValueList<Person>::const_iterator pit = e.Credits.begin();
    for (; pit != e.Credits.end(); ++pit)
        Credits.push_back(Person(QDeepCopy<QString>((*pit).role),
                                 QDeepCopy<QString>((*pit).name)));
}

void Event::clearEventValues()
{
    SourcePID    = 0;
    LanguageCode = "";
    Event_Name   = "";
    Description  = "";
    EventID      = 0;
    ETM_Location = 0;
    Event_Subtitle     = "";
    ContentDescription = "";
    Year         = "";
    SubTitled    = false;
    Stereo       = false;
    HDTV         = false;
    ATSC         = false;
    PartNumber   = 0;
    PartTotal    = 0;
    CategoryType = "";
    OriginalAirDate = QDate();
    Actors.clear();
    Credits.clear();
}
