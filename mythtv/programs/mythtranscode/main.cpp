#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
using namespace std;

#include "NuppelVideoPlayer.h"
#include "recordingprofile.h"
#include "programinfo.h"
#include "mythcontext.h"

MythContext *gContext;
QSqlDatabase *db;

void StoreTranscodeState(ProgramInfo *pginfo, int createdelete);

int main(int argc, char *argv[])
{
    QString chanid, starttime, profilename;
    srand(time(NULL));

    QApplication a(argc, argv, false);

    //  Load the context
    gContext = new MythContext(MYTH_BINARY_VERSION,false);

    int found_starttime = 0;
    int found_chanid = 0;
    int found_profile = 0;

    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-s") ||
            !strcmp(a.argv()[argpos],"--starttime")) 
        {
            if (a.argc() > argpos) 
            {
                starttime = a.argv()[argpos + 1];
                found_starttime = 1;
                ++argpos;
            } 
            else 
            {
                cerr << "Missing argument to -s/--starttime option\n";
                return -1;
            }
         } 
         else if (!strcmp(a.argv()[argpos],"-c") ||
                  !strcmp(a.argv()[argpos],"--chanid")) 
         {
             if (a.argc() > argpos) 
             {
                 chanid = a.argv()[argpos + 1];
                 found_chanid = 1;
                 ++argpos;
             } 
             else 
             {
                 cerr << "Missing argument to -c/--chanid option\n";
                 return -1;
             }
         } 
         else if (!strcmp(a.argv()[argpos],"-p") ||
                  !strcmp(a.argv()[argpos],"--profile")) 
         {
             if (a.argc() > argpos) 
             {
                 profilename = a.argv()[argpos + 1];
                 found_profile = 1;
                 ++argpos;
             } 
             else 
             {
                 cerr << "Missing argument to -c/--chanid option\n";
                 return -1;
             }
         }
    }

    if (!found_profile || !found_chanid || !found_starttime) 
    {
         cerr << "Must specify -p, -c, and -s options!\n";
         return -1;
    }

    chanid = a.argv()[1];
    starttime = a.argv()[2];
    profilename = a.argv()[3];

    db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    RecordingProfile profile;
    MythContext::KickDatabase(db);

    if (!profile.loadByName(db, profilename))
    {
        cerr << "Illegal profile : " << profilename << endl;
        return -1;
    }

    QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
    ProgramInfo *pginfo = ProgramInfo::GetProgramAtDateTime(chanid, startts);

    if (!pginfo)
    {
        cerr << "Couldn't find recording " << chanid << " " << startts.toString() << endl;
        return -1;
    }

    QString fileprefix = gContext->GetFilePrefix();
    QString infile = pginfo->GetRecordFilename(fileprefix);
    QString tmpfile = infile;
    tmpfile += ".tmp";

    StoreTranscodeState(pginfo, 1);
    NuppelVideoPlayer *nvp = new NuppelVideoPlayer;

    cout << "Transcoding from " << infile << " to " << tmpfile << "\n";

    if (nvp->ReencodeFile((char *)infile.ascii(), (char *)tmpfile.ascii(), 
                          profile)) 
    {
        StoreTranscodeState(pginfo, 0);
        cout << "Transcoding " << infile << " done\n";
    } 
    else 
    {
        StoreTranscodeState(pginfo, -1);
        cout << "Transcoding " << infile << " failed\n";
    }

    delete nvp;
    return 0;
}

void StoreTranscodeState(ProgramInfo *pginfo, int createdelete)
{
    bool exists = false;
    QString query = QString("SELECT * FROM transcoding WHERE "
                            "chanid = '%1' AND starttime = '%2';")
                           .arg(pginfo->chanid)
                           .arg(pginfo->startts.toString("yyyyMMddhhmmss"));
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0)
        exists = true;

    if (createdelete == 1) 
    {
        if (exists)
            return;
        query = QString("INSERT INTO transcoding (chanid,starttime,isdone) "
                        "VALUES ('%1','%2',0);")
                        .arg(pginfo->chanid) 
                        .arg(pginfo->startts.toString("yyyyMMddhhmmss"));
    } 
    else if (createdelete == -1) 
    {
        if (!exists)
            return;
        query = QString("DELETE FROM transcoding "
                        "WHERE chanid = '%1' AND starttime = '%2';")
                        .arg(pginfo->chanid)
                        .arg(pginfo->startts.toString("yyyyMMddhhmmss"));
    } 
    else 
    {
        if (!exists) 
        {
            cerr << "Error updating transcoding table. Row is non-existant\n";
            return;
        }
        query = QString("UPDATE transcoding SET isdone = 1, "
                        "starttime = starttime "
                        "WHERE chanid = '%1' AND starttime = '%2';")
                        .arg(pginfo->chanid)
                        .arg(pginfo->startts.toString("yyyyMMddhhmmss"));
    }

    MythContext::KickDatabase(db);
    db->exec(query);
}
