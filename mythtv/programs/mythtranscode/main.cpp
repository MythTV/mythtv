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

void usage(char *progname) 
{
    cerr << "Usage: " << progname << " <--chanid <channelid>>\n";
    cerr << "\t<--starttime <starttime>> <--profile <profile>>\n";
    cerr << "\t<honorcutlist> <--allkeys> <--help>\n\n";
    cerr << "\t--chanid       or -c: Takes a channel id. REQUIRED\n";
    cerr << "\t--starttime    or -s: Takes a starttime for the\n";
    cerr << "\t\trecording. REQUIRED\n";
    cerr << "\t--profile      or -p: Takes the named of an existing\n";
    cerr << "\t\trecording profile. REQUIRED\n";
    cerr << "\t--honorcutlist or -l: Specifies whether to use the cutlist.\n";
    cerr << "\t--allkeys      or -k: Specifies that the output file\n";
    cerr << "\t\tshould be made entirely of keyframes.\n";
    cerr << "\t--help         or -h: Prints this help statement.\n";
}

int main(int argc, char *argv[])
{
    QString chanid, starttime, profilename;
    bool honorcutlist = false, keyframesonly = false;
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
                usage(a.argv()[0]);
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
                usage(a.argv()[0]);
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
                usage(a.argv()[0]);
                return -1;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-l") ||
                 !strcmp(a.argv()[argpos],"--honorcutlist")) 
        {
            honorcutlist = true;
        }
        else if (!strcmp(a.argv()[argpos],"-k") ||
                 !strcmp(a.argv()[argpos],"--allkeys")) 
        {
            keyframesonly = true;
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help")) 
        {
            usage(a.argv()[0]);
            return(0);
        }
    }

    if (!found_profile || !found_chanid || !found_starttime) 
    {
         cerr << "Must specify -p, -c, and -s options!\n";
         return -1;
    }

    //  Load the context
    gContext = new MythContext(MYTH_BINARY_VERSION, false);

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

    if (!profile.loadByName(db, profilename) && (profilename.lower() != "raw"))
    {
        cerr << "Illegal profile : " << profilename << endl;
        return -1;
    }

    QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
    ProgramInfo *pginfo = ProgramInfo::GetProgramFromRecorded(chanid, startts);

    if (!pginfo)
    {
        cerr << "Couldn't find recording " << chanid << " " << startts.toString() << endl;
        return -1;
    }

    QString fileprefix = gContext->GetFilePrefix();
    QString infile = pginfo->GetRecordFilename(fileprefix);
    QString tmpfile = infile + ".tmp";

    StoreTranscodeState(pginfo, 1);
    NuppelVideoPlayer *nvp = new NuppelVideoPlayer(db, pginfo);

    cout << "Transcoding from " << infile << " to " << tmpfile << "\n";

    if (nvp->ReencodeFile((char *)infile.ascii(), (char *)tmpfile.ascii(), 
                          profile, honorcutlist, keyframesonly)) 
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
