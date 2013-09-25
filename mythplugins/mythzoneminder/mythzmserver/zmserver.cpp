/* Implementation of the ZMServer class.
 * ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */


#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>

#ifdef linux
#  include <sys/vfs.h>
#  include <sys/statvfs.h>
#  include <sys/sysinfo.h>
#else
#  include <sys/param.h>
#  include <sys/mount.h>
#  if CONFIG_CYGWIN
#    include <sys/statfs.h>
#  else // if !CONFIG_CYGWIN
#    include <sys/sysctl.h>
#  endif // !CONFIG_CYGWIN
#endif

#include "mythtv/mythconfig.h"

#if CONFIG_DARWIN
#define MSG_NOSIGNAL 0  // Apple also has SO_NOSIGPIPE?
#endif

#include "zmserver.h"

// the version of the protocol we understand
#define ZM_PROTOCOL_VERSION "8"

// the maximum image size we are ever likely to get from ZM
#define MAX_IMAGE_SIZE  (2048*1536*3)

#define ADD_STR(list,s)  list += s; list += "[]:[]";

// error messages
#define ERROR_TOKEN_COUNT      "Invalid token count"
#define ERROR_MYSQL_QUERY      "Mysql Query Error"
#define ERROR_MYSQL_ROW        "Mysql Get Row Error"
#define ERROR_FILE_OPEN        "Cannot open event file"
#define ERROR_INVALID_MONITOR  "Invalid Monitor"
#define ERROR_INVALID_POINTERS "Cannot get shared memory pointers"
#define ERROR_INVALID_MONITOR_FUNCTION  "Invalid Monitor Function"
#define ERROR_INVALID_MONITOR_ENABLE_VALUE "Invalid Monitor Enable Value"

MYSQL   g_dbConn;
string  g_zmversion = "";
string  g_password = "";
string  g_server = "";
string  g_database = "";
string  g_webPath = "";
string  g_user = "";
string  g_webUser = "";
string  g_binPath = "";

time_t  g_lastDBKick = 0;

void loadZMConfig(const string &configfile)
{
    cout << "loading zm config from " << configfile << endl;
    FILE *cfg;
    char line[512];
    char val[250];

    if ( (cfg = fopen(configfile.c_str(), "r")) == NULL )
    {
        fprintf(stderr,"Can't open %s\n", configfile.c_str());
        exit(1);
    }

    while ( fgets( line, sizeof(line), cfg ) != NULL )
    {
        char *line_ptr = line;
        // Trim off any cr/lf line endings
        size_t chomp_len = strcspn( line_ptr, "\r\n" );
        line_ptr[chomp_len] = '\0';

        // Remove leading white space
        size_t white_len = strspn( line_ptr, " \t" );
        line_ptr += white_len;

        // Check for comment or empty line
        if ( *line_ptr == '\0' || *line_ptr == '#' )
            continue;

        // Remove trailing white space
        char *temp_ptr = line_ptr+strlen(line_ptr)-1;
        while ( *temp_ptr == ' ' || *temp_ptr == '\t' )
        {
            *temp_ptr-- = '\0';
            temp_ptr--;
        }

        // Now look for the '=' in the middle of the line
        temp_ptr = strchr( line_ptr, '=' );
        if ( !temp_ptr )
        {
            fprintf(stderr,"Invalid data in %s: '%s'\n", configfile.c_str(), line );
            continue;
        }

        // Assign the name and value parts
        char *name_ptr = line_ptr;
        char *val_ptr = temp_ptr+1;

        // Trim trailing space from the name part
        do
        {
            *temp_ptr = '\0';
            temp_ptr--;
        }
        while ( *temp_ptr == ' ' || *temp_ptr == '\t' );

        // Remove leading white space from the value part
        white_len = strspn( val_ptr, " \t" );
        val_ptr += white_len;

        strncpy( val, val_ptr, strlen(val_ptr)+1 );
        if ( strcasecmp( name_ptr, "ZM_DB_HOST" ) == 0 )       g_server = val;
        else if ( strcasecmp( name_ptr, "ZM_DB_NAME" ) == 0 )  g_database = val;
        else if ( strcasecmp( name_ptr, "ZM_DB_USER" ) == 0 )  g_user = val;
        else if ( strcasecmp( name_ptr, "ZM_DB_PASS" ) == 0 )  g_password = val;
        else if ( strcasecmp( name_ptr, "ZM_PATH_WEB" ) == 0 ) g_webPath = val;
        else if ( strcasecmp( name_ptr, "ZM_PATH_BIN" ) == 0 ) g_binPath = val;
        else if ( strcasecmp( name_ptr, "ZM_WEB_USER" ) == 0 ) g_webUser = val;
        else if ( strcasecmp( name_ptr, "ZM_VERSION" ) == 0 ) g_zmversion = val;
    }
    fclose(cfg);
}

void connectToDatabase(void)
{
    if (!mysql_init(&g_dbConn))
    {
        cout << "Error: Can't initialise structure: " <<  mysql_error(&g_dbConn) << endl;
        exit(mysql_errno(&g_dbConn));
    }

    if (!mysql_real_connect(&g_dbConn, g_server.c_str(), g_user.c_str(),
         g_password.c_str(), 0, 0, 0, 0))
    {
        cout << "Error: Can't connect to server: " <<  mysql_error(&g_dbConn) << endl;
        exit(mysql_errno( &g_dbConn));
    }

    if (mysql_select_db(&g_dbConn, g_database.c_str()))
    {
        cout << "Error: Can't select database: " << mysql_error(&g_dbConn) << endl;
        exit(mysql_errno(&g_dbConn));
    }
}

void kickDatabase(bool debug)
{
    if (time(NULL) < g_lastDBKick + DB_CHECK_TIME)
        return;

    if (debug)
        cout << "Kicking database connection" << endl;

    g_lastDBKick = time(NULL);

    if (mysql_query(&g_dbConn, "SELECT NULL;") == 0)
    {
        MYSQL_RES *res = mysql_store_result(&g_dbConn);
        if (res)
            mysql_free_result(res);
        return;
    }

    cout << "Lost connection to DB - trying to reconnect" << endl;

    // failed so try to reconnect to the DB
    mysql_close(&g_dbConn);
    connectToDatabase();
}

ZMServer::ZMServer(int sock, bool debug)
{
    if (debug)
        cout << "Using server protocol version '" << ZM_PROTOCOL_VERSION << "'\n";

    m_sock = sock;
    m_debug = debug;

    // get the shared memory key
    char buf[100];
    m_shmKey = 0x7a6d2000;
    string setting = getZMSetting("ZM_SHM_KEY");

    if (setting != "")
    {
        unsigned long long tmp = m_shmKey;
        sscanf(setting.c_str(), "%20llx", &tmp);
        m_shmKey = tmp;
    }

    if (m_debug)
    {
        snprintf(buf, sizeof(buf), "0x%x", (unsigned int)m_shmKey);
        cout << "Shared memory key is: " << buf << endl;
    }

    // get the MMAP path
    m_mmapPath = getZMSetting("ZM_PATH_MAP");
    if (m_debug)
    {
        cout << "Memory path directory is: " << m_mmapPath << endl;
    }

    // get the event filename format
    setting = getZMSetting("ZM_EVENT_IMAGE_DIGITS");
    int eventDigits = atoi(setting.c_str());
    snprintf(buf, sizeof(buf), "%%0%dd-capture.jpg", eventDigits);
    m_eventFileFormat = buf;
    if (m_debug)
        cout << "Event file format is: " << m_eventFileFormat << endl;

    // get the analyse filename format
    snprintf(buf, sizeof(buf), "%%0%dd-analyse.jpg", eventDigits);
    m_analyseFileFormat = buf;
    if (m_debug)
        cout << "Analyse file format is: " << m_analyseFileFormat << endl;

    // is ZM using the deep storage directory format?
    m_useDeepStorage = (getZMSetting("ZM_USE_DEEP_STORAGE") == "1");
    if (m_debug)
    {
        if (m_useDeepStorage)
            cout << "using deep storage directory structure" << endl;
        else
            cout << "using flat directory structure" << endl;
    }

    getMonitorList();
}

ZMServer::~ZMServer()
{
    for (std::map<int, MONITOR*>::iterator it = m_monitors.begin(); it != m_monitors.end(); ++it)
    {
        MONITOR *mon = it->second;
        if (mon->mapFile != -1)
        {
            if (close(mon->mapFile) == -1)
                cout << "Failed to close mapFile" << endl;
            else
                if (m_debug)
                    cout << "Closed mapFile for monitor: " << mon->name << endl;
        }

        delete mon;
    }

    m_monitors.clear();

    if (m_debug)
        cout << "ZMServer destroyed\n";
}

void ZMServer::tokenize(const string &command, vector<string> &tokens)
{
    string token = "";
    tokens.clear();
    string::size_type startPos = 0;
    string::size_type endPos = 0;

    while((endPos = command.find("[]:[]", startPos)) != string::npos)
    {
        token = command.substr(startPos, endPos - startPos);
        tokens.push_back(token);
        startPos = endPos + 5;
    }

    // make sure we add the last token
    if (endPos != command.length())
    {
        token = command.substr(startPos);
        tokens.push_back(token);
    }
}

// returns true if we get a QUIT command from the client
bool ZMServer::processRequest(char* buf, int nbytes)
{
#if 0
    // first 8 bytes is the length of the following data
    char len[9];
    memcpy(len, buf, 8);
    len[8] = '\0';
    int dataLen = atoi(len);
#endif

    buf[nbytes] = '\0';
    string s(buf+8);
    vector<string> tokens;
    tokenize(s, tokens);

    if (tokens.empty())
        return false;

    if (m_debug)
        cout << "Processing: '" << tokens[0] << "'" << endl;

    if (tokens[0] == "HELLO")
        handleHello();
    else if (tokens[0] == "QUIT")
        return true;
    else if (tokens[0] == "GET_SERVER_STATUS")
        handleGetServerStatus();
    else if (tokens[0] == "GET_MONITOR_STATUS")
        handleGetMonitorStatus();
    else if (tokens[0] == "GET_EVENT_LIST")
        handleGetEventList(tokens);
    else if (tokens[0] == "GET_EVENT_DATES")
        handleGetEventDates(tokens);
    else if (tokens[0] == "GET_EVENT_FRAME")
        handleGetEventFrame(tokens);
    else if (tokens[0] == "GET_ANALYSE_FRAME")
        handleGetAnalyseFrame(tokens);
    else if (tokens[0] == "GET_LIVE_FRAME")
        handleGetLiveFrame(tokens);
    else if (tokens[0] == "GET_FRAME_LIST")
        handleGetFrameList(tokens);
    else if (tokens[0] == "GET_CAMERA_LIST")
        handleGetCameraList();
    else if (tokens[0] == "GET_MONITOR_LIST")
        handleGetMonitorList();
    else if (tokens[0] == "DELETE_EVENT")
        handleDeleteEvent(tokens);
    else if (tokens[0] == "DELETE_EVENT_LIST")
        handleDeleteEventList(tokens);
    else if (tokens[0] == "RUN_ZMAUDIT")
        handleRunZMAudit();
    else if (tokens[0] == "SET_MONITOR_FUNCTION")
        handleSetMonitorFunction(tokens);
    else
        send("UNKNOWN_COMMAND");

    return false;
}

bool ZMServer::send(const string &s) const
{
    // send length
    size_t len = s.size();
    char buf[9];
    sprintf(buf, "%8u", (unsigned int) len);
    int status = ::send(m_sock, buf, 8, MSG_NOSIGNAL);
    if (status == -1)
        return false;

    // send message
    status = ::send(m_sock, s.c_str(), s.size(), MSG_NOSIGNAL);
    if ( status == -1 )
        return false;
    else
        return true;
}

bool ZMServer::send(const string &s, const unsigned char *buffer, int dataLen) const
{
    // send length
    size_t len = s.size();
    char buf[9];
    sprintf(buf, "%8u", (unsigned int) len);
    int status = ::send(m_sock, buf, 8, MSG_NOSIGNAL);
    if (status == -1)
        return false;

    // send message
    status = ::send(m_sock, s.c_str(), s.size(), MSG_NOSIGNAL);
    if ( status == -1 )
        return false;

    // send data
    status = ::send(m_sock, buffer, dataLen, MSG_NOSIGNAL);
    if ( status == -1 )
        return false;

    return true;
}

void ZMServer::sendError(string error)
{
    string outStr("");
    ADD_STR(outStr, string("ERROR - ") + error);
    send(outStr);
}

void ZMServer::handleHello()
{
    // just send OK so the client knows all is well
    // followed by the protocol version we understand
    string outStr("");
    ADD_STR(outStr, "OK");
    ADD_STR(outStr, ZM_PROTOCOL_VERSION);
    send(outStr);
}

long long ZMServer::getDiskSpace(const string &filename, long long &total, long long &used)
{
    struct statfs statbuf;
    memset(&statbuf, 0, sizeof(statbuf));
    long long freespace = -1;

    total = used = -1;

    // there are cases where statfs will return 0 (good), but f_blocks and
    // others are invalid and set to 0 (such as when an automounted directory
    // is not mounted but still visible because --ghost was used),
    // so check to make sure we can have a total size > 0
    if ((statfs(filename.c_str(), &statbuf) == 0) &&
         (statbuf.f_blocks > 0) &&
         (statbuf.f_bsize > 0))
    {
        total      = statbuf.f_blocks;
        total     *= statbuf.f_bsize;
        total      = total >> 10;

        freespace  = statbuf.f_bavail;
        freespace *= statbuf.f_bsize;
        freespace  = freespace >> 10;

        used       = total - freespace;
    }

    return freespace;
}

void ZMServer::handleGetServerStatus(void)
{
    string outStr("");
    ADD_STR(outStr, "OK")

    // server status
    string status = runCommand(g_binPath + "/zmdc.pl check");
    ADD_STR(outStr, status)

    // get load averages
    double loads[3];
    if (getloadavg(loads, 3) == -1)
    {
        ADD_STR(outStr, "Unknown")
    }
    else
    {
        char buf[30];
        sprintf(buf, "%0.2lf", loads[0]);
        ADD_STR(outStr, buf)
    }

    // get free space on the disk where the events are stored
    char buf[15];
    long long total, used;
    string eventsDir = g_webPath + "/events/";
    getDiskSpace(eventsDir, total, used);
    sprintf(buf, "%d%%", (int) ((100.0 / ((float) total / used))));
    ADD_STR(outStr, buf)

    send(outStr);
}

void ZMServer::handleGetEventList(vector<string> tokens)
{
    string outStr("");

    if (tokens.size() != 4)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    string monitor = tokens[1];
    bool oldestFirst = (tokens[2] == "1");
    string date = tokens[3];

    if (m_debug)
        cout << "Loading events for monitor: " << monitor << ", date: " << date << endl;

    ADD_STR(outStr, "OK")

    MYSQL_RES *res;
    MYSQL_ROW row;

    string sql("SELECT E.Id, E.Name, M.Id AS MonitorID, M.Name AS MonitorName, E.StartTime,  "
            "E.Length, M.Width, M.Height, M.DefaultRate, M.DefaultScale "
            "from Events as E inner join Monitors as M on E.MonitorId = M.Id ");

    if (monitor != "<ANY>")
    {
        sql += "WHERE M.Name = '" + monitor + "' ";

        if (date != "<ANY>")
            sql += "AND DATE(E.StartTime) = DATE('" + date + "') ";
    }
    else
    {
        if (date != "<ANY>")
            sql += "WHERE DATE(E.StartTime) = DATE('" + date + "') ";
    }

    if (oldestFirst)
        sql += "ORDER BY E.StartTime ASC";
    else
        sql += "ORDER BY E.StartTime DESC";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    res = mysql_store_result(&g_dbConn);
    int eventCount = mysql_num_rows(res);

    if (m_debug)
        cout << "Got " << eventCount << " events" << endl;

    char str[10];
    sprintf(str, "%d", eventCount);
    ADD_STR(outStr, str)

    for (int x = 0; x < eventCount; x++)
    {
        row = mysql_fetch_row(res);
        if (row)
        {
            ADD_STR(outStr, row[0]) // eventID
            ADD_STR(outStr, row[1]) // event name
            ADD_STR(outStr, row[2]) // monitorID
            ADD_STR(outStr, row[3]) // monitor name
            row[4][10] = 'T';
            ADD_STR(outStr, row[4]) // start time
            ADD_STR(outStr, row[5]) // length
        }
        else
        {
            cout << "Failed to get mysql row" << endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

void ZMServer::handleGetEventDates(vector<string> tokens)
{
    string outStr("");

    if (tokens.size() != 3)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    string monitor = tokens[1];
    bool oldestFirst = (tokens[2] == "1");

    if (m_debug)
        cout << "Loading event dates for monitor: " << monitor << endl;

    ADD_STR(outStr, "OK")

    MYSQL_RES *res;
    MYSQL_ROW row;

    string sql("SELECT DISTINCT DATE(E.StartTime) "
            "from Events as E inner join Monitors as M on E.MonitorId = M.Id ");

    if (monitor != "<ANY>")
        sql += "WHERE M.Name = '" + monitor + "' ";

    if (oldestFirst)
        sql += "ORDER BY E.StartTime ASC";
    else
        sql += "ORDER BY E.StartTime DESC";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    res = mysql_store_result(&g_dbConn);
    int dateCount = mysql_num_rows(res);

    if (m_debug)
        cout << "Got " << dateCount << " dates" << endl;

    char str[10];
    sprintf(str, "%d", dateCount);
    ADD_STR(outStr, str)

    for (int x = 0; x < dateCount; x++)
    {
        row = mysql_fetch_row(res);
        if (row)
        {
            ADD_STR(outStr, row[0]) // event date
        }
        else
        {
            cout << "Failed to get mysql row" << endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

void ZMServer::handleGetMonitorStatus(void)
{
    string outStr("");
    ADD_STR(outStr, "OK")

    // get monitor list
    MYSQL_RES *res;
    MYSQL_ROW row;

    string sql("SELECT Id, Name, Type, Device, Host, Channel, Function, Enabled "
               "FROM Monitors;");
    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    res = mysql_store_result(&g_dbConn);

     // add monitor count
    int monitorCount = mysql_num_rows(res);

    if (m_debug)
        cout << "Got " << monitorCount << " monitors" << endl;

    char str[10];
    sprintf(str, "%d", monitorCount);
    ADD_STR(outStr, str)

    for (int x = 0; x < monitorCount; x++)
    {
        row = mysql_fetch_row(res);
        if (row)
        {
            string id = row[0];
            string type = row[2];
            string device = row[3];
            string host = row[4];
            string channel = row[5];
            string function = row[6];
            string enabled = row[7];
            string name = row[1];
            string events = "";
            string zmcStatus = "";
            string zmaStatus = "";
            getMonitorStatus(id, type, device, host, channel, function,
                             zmcStatus, zmaStatus, enabled);
            MYSQL_RES *res2;
            MYSQL_ROW row2;

            string sql2("SELECT count(if(Archived=0,1,NULL)) AS EventCount "
                        "FROM Events AS E "
                        "WHERE MonitorId = " + id);

            if (mysql_query(&g_dbConn, sql2.c_str()))
            {
                fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
                sendError(ERROR_MYSQL_QUERY);
                return;
            }

            res2 = mysql_store_result(&g_dbConn);
            if (mysql_num_rows(res2) > 0)
            {
                row2 = mysql_fetch_row(res2);
                if (row2)
                    events = row2[0];
                else
                {
                    cout << "Failed to get mysql row" << endl;
                    sendError(ERROR_MYSQL_ROW);
                    return;
                }
            }

            ADD_STR(outStr, id)
            ADD_STR(outStr, name)
            ADD_STR(outStr, zmcStatus)
            ADD_STR(outStr, zmaStatus)
            ADD_STR(outStr, events)
            ADD_STR(outStr, function)
            ADD_STR(outStr, enabled)

            mysql_free_result(res2);
        }
        else
        {
            cout << "Failed to get mysql row" << endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

string ZMServer::runCommand(string command)
{
    string outStr("");
    FILE *fd = popen(command.c_str(), "r");
    char buffer[100];

    while (fgets(buffer, sizeof(buffer), fd) != NULL)
    {
        outStr += buffer;
    }
    pclose(fd);
    return outStr;
}

void ZMServer::getMonitorStatus(string id, string type, string device, string host, string channel,
                                string function, string &zmcStatus, string &zmaStatus,
                                string enabled)
{
    zmaStatus = "";
    zmcStatus = "";

    string command(g_binPath + "/zmdc.pl status");
    string status = runCommand(command);

    if (type == "Local")
    {
        if (enabled == "0")
            zmaStatus = device + "(" + channel + ") [-]";
        else if (status.find("'zma -m " + id + "' running") != string::npos)
            zmaStatus = device + "(" + channel + ") [R]";
        else
            zmaStatus = device + "(" + channel + ") [S]";
    }
    else
    {
        if (enabled == "0")
            zmaStatus = host + " [-]";
        else if (status.find("'zma -m " + id + "' running") != string::npos)
            zmaStatus = host + " [R]";
        else
            zmaStatus = host + " [S]";
    }

    if (type == "Local")
    {
        if (enabled == "0")
            zmcStatus = function + " [-]";
        else if (status.find("'zmc -d "+ device + "' running") != string::npos)
            zmcStatus = function + " [R]";
        else
            zmcStatus = function + " [S]";
    }
    else
    {
        if (enabled == "0")
            zmcStatus = function + " [-]";
        else if (status.find("'zmc -m " + id + "' running") != string::npos)
            zmcStatus = function + " [R]";
        else
            zmcStatus = function + " [S]";
    }
}

void ZMServer::handleGetEventFrame(vector<string> tokens)
{
    static unsigned char buffer[MAX_IMAGE_SIZE];

    if (tokens.size() != 5)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    string monitorID(tokens[1]);
    string eventID(tokens[2]);
    int frameNo = atoi(tokens[3].c_str());
    string eventTime(tokens[4]);

    if (m_debug)
        cout << "Getting frame " << frameNo << " for event " << eventID
             << " on monitor " << monitorID  << " event time is " << eventTime << endl;

    string outStr("");

    ADD_STR(outStr, "OK")

    // try to find the frame file
    string filepath("");
    char str[100];

    if (m_useDeepStorage)
    {
        filepath = g_webPath + "/events/" + monitorID + "/" + eventTime + "/";
        sprintf(str, m_eventFileFormat.c_str(), frameNo);
        filepath += str;
    }
    else
    {
        filepath = g_webPath + "/events/" + monitorID + "/" + eventID + "/";
        sprintf(str, m_eventFileFormat.c_str(), frameNo);
        filepath += str;
    }

    FILE *fd;
    int fileSize = 0;
    if ((fd = fopen(filepath.c_str(), "r" )))
    {
        fileSize = fread(buffer, 1, sizeof(buffer), fd);
        fclose(fd);
    }
    else
    {
        cout << "Can't open " << filepath << ": " << strerror(errno) << endl;
        sendError(ERROR_FILE_OPEN + string(" - ") + filepath + " : " + strerror(errno));
        return;
    }

    if (m_debug)
        cout << "Frame size: " <<  fileSize << endl;

    // get the file size
    sprintf(str, "%d", fileSize);
    ADD_STR(outStr, str)

    // send the data
    send(outStr, buffer, fileSize);
}

void ZMServer::handleGetAnalyseFrame(vector<string> tokens)
{
    static unsigned char buffer[MAX_IMAGE_SIZE];
    char str[100];

    if (tokens.size() != 5)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    string monitorID(tokens[1]);
    string eventID(tokens[2]);
    int frameNo = atoi(tokens[3].c_str());
    string eventTime(tokens[4]);

    if (m_debug)
        cout << "Getting anaylse frame " << frameNo << " for event " << eventID
             << " on monitor " << monitorID << " event time is " << eventTime << endl;

    // get the 'alarm' frames from the Frames table for this event
    MYSQL_RES *res;
    MYSQL_ROW row = NULL;

    string sql("");
    sql += "SELECT FrameId FROM Frames ";
    sql += "WHERE EventID = " + eventID + " ";
    sql += "AND Type = 'Alarm' ";
    sql += "ORDER BY FrameID";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    res = mysql_store_result(&g_dbConn);
    int frameCount = mysql_num_rows(res);
    int frameID;

    // if the required frame mumber is 0 or out of bounds then use the middle frame
    if (frameNo == 0 || frameNo < 0 || frameNo > frameCount)
        frameNo = (frameCount / 2) + 1;

    // move to the required frame in the table
    for (int x = 0; x < frameNo; x++)
    {
        row = mysql_fetch_row(res);
    }

    if (row)
    {
        frameID = atoi(row[0]);
    }
    else
    {
        cout << "handleGetAnalyseFrame: Failed to get mysql row for frameNo " << frameNo << endl;
        sendError(ERROR_MYSQL_ROW);
        return;
    }

    string outStr("");

    ADD_STR(outStr, "OK")

    // try to find the analyse frame file
    string filepath("");
    if (m_useDeepStorage)
    {
        filepath = g_webPath + "/events/" + monitorID + "/" + eventTime + "/";
        sprintf(str, m_analyseFileFormat.c_str(), frameID);
        filepath += str;
    }
    else
    {
        filepath = g_webPath + "/events/" + monitorID + "/" + eventID + "/";
        sprintf(str, m_analyseFileFormat.c_str(), frameID);
        filepath += str;
    }

    FILE *fd;
    int fileSize = 0;
    if ((fd = fopen(filepath.c_str(), "r" )))
    {
        fileSize = fread(buffer, 1, sizeof(buffer), fd);
        fclose(fd);
    }
    else
    {
        cout << "Can't open " << filepath << ": " << strerror(errno) << endl;
        sendError(ERROR_FILE_OPEN + string(" - ") + filepath + " : " + strerror(errno));
        return;
    }

    if (m_debug)
        cout << "Frame size: " <<  fileSize << endl;

    // get the file size
    sprintf(str, "%d", fileSize);
    ADD_STR(outStr, str)

    // send the data
    send(outStr, buffer, fileSize);
}

void ZMServer::handleGetLiveFrame(vector<string> tokens)
{
    static unsigned char buffer[MAX_IMAGE_SIZE];
    char str[100];

    // we need to periodically kick the DB connection here to make sure it
    // stays alive because the user may have left the frontend on the live
    // view which doesn't query the DB at all and eventually the connection
    // will timeout
    kickDatabase(m_debug);

    if (tokens.size() != 2)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    int monitorID = atoi(tokens[1].c_str());

    if (m_debug)
        cout << "Getting live frame from monitor: " << monitorID << endl;

    string outStr("");

    ADD_STR(outStr, "OK")

    // echo the monitor id
    sprintf(str, "%d", monitorID);
    ADD_STR(outStr, str)

    // try to find the correct MONITOR
    MONITOR *monitor;
    if (m_monitors.find(monitorID) != m_monitors.end())
        monitor = m_monitors[monitorID];
    else
    {
        sendError(ERROR_INVALID_MONITOR);
        return;
    }

    // are the data pointers valid?
    if (monitor->shared_data == NULL ||  monitor->shared_images == NULL)
    {
        sendError(ERROR_INVALID_POINTERS);
        return;
    }

    // read a frame from the shared memory
    int dataSize = getFrame(buffer, sizeof(buffer), monitor);

    if (m_debug)
        cout << "Frame size: " <<  dataSize << endl;

    if (dataSize == 0)
    {
        // not really an error
        outStr = "";
        ADD_STR(outStr, "WARNING - No new frame available");
        send(outStr);
        return;
    }

    // add status
    ADD_STR(outStr, monitor->status)

    // send the data size
    sprintf(str, "%d", dataSize);
    ADD_STR(outStr, str)

    // send the data
    send(outStr, buffer, dataSize);
}

void ZMServer::handleGetFrameList(vector<string> tokens)
{
    string eventID;
    string outStr("");

    if (tokens.size() != 2)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    eventID = tokens[1];

    if (m_debug)
        cout << "Loading frames for event: " << eventID << endl;

    ADD_STR(outStr, "OK")

    MYSQL_RES *res;
    MYSQL_ROW row;

    string sql("");
    sql += "SELECT Type, Delta FROM Frames ";
    sql += "WHERE EventID = " + eventID + " ";
    sql += "ORDER BY FrameID";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    res = mysql_store_result(&g_dbConn);
    int frameCount = mysql_num_rows(res);

    if (m_debug)
        cout << "Got " << frameCount << " frames" << endl;

    char str[10];
    sprintf(str, "%d\n", frameCount);
    ADD_STR(outStr, str)

    for (int x = 0; x < frameCount; x++)
    {
        row = mysql_fetch_row(res);
        if (row)
        {
            ADD_STR(outStr, row[0]) // Type
            ADD_STR(outStr, row[1]) // Delta
        }
        else
        {
            cout << "handleGetFrameList: Failed to get mysql row " << x << endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

void ZMServer::handleGetCameraList(void)
{
    string outStr("");

    ADD_STR(outStr, "OK")

    MYSQL_RES *res;
    MYSQL_ROW row;

    string sql("");
    sql += "SELECT DISTINCT M.Name FROM Events AS E ";
    sql += "INNER JOIN Monitors AS M ON E.MonitorId = M.Id;";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    res = mysql_store_result(&g_dbConn);
    int monitorCount = mysql_num_rows(res);
    char str[10];
    sprintf(str, "%d", monitorCount);
    ADD_STR(outStr, str)

    for (int x = 0; x < monitorCount; x++)
    {
        row = mysql_fetch_row(res);
        if (row)
        {
            ADD_STR(outStr, row[0]) // Name
        }
        else
        {
            cout << "handleGetCameraList: Failed to get mysql row " << x << endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

void ZMServer::handleGetMonitorList(void)
{
    string outStr("");

    ADD_STR(outStr, "OK")

    MYSQL_RES *res;
    MYSQL_ROW row;

    string sql("");
    sql += "SELECT Id, Name, Width, Height, Palette FROM Monitors ORDER BY Id";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    res = mysql_store_result(&g_dbConn);
    int monitorCount = mysql_num_rows(res);

    if (m_debug)
        cout << "Got " << monitorCount << " monitors" << endl;

    char str[10];
    sprintf(str, "%d", monitorCount);
    ADD_STR(outStr, str)

    for (int x = 0; x < monitorCount; x++)
    {
        row = mysql_fetch_row(res);
        if (row)
        {
            ADD_STR(outStr, row[0]) // Id
            ADD_STR(outStr, row[1]) // Name
            ADD_STR(outStr, row[2]) // Width
            ADD_STR(outStr, row[3]) // Height
            ADD_STR(outStr, row[4]) // Palette

            if (m_debug)
            {
                cout << "id:      " << row[0] << endl;
                cout << "name:    " << row[1] << endl;
                cout << "width:   " << row[2] << endl;
                cout << "height:  " << row[3] << endl;
                cout << "palette: " << row[4] << endl;
                cout << "-------------------" << endl;
            }
        }
        else
        {
            cout << "Failed to get mysql row" << endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

void ZMServer::handleDeleteEvent(vector<string> tokens)
{
    string eventID;
    string outStr("");

    if (tokens.size() != 2)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    eventID = tokens[1];

    if (m_debug)
        cout << "Deleting event: " << eventID << endl;

    ADD_STR(outStr, "OK")

    string sql("");
    sql += "DELETE FROM Events WHERE Id = " + eventID;

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    // run zmaudit.pl to clean everything up
    string command(g_binPath + "/zmaudit.pl &");
    errno = 0;
    if (system(command.c_str()) < 0 && errno)
        cerr << "Failed to run '" << command << "'" << endl;

    send(outStr);
}

void ZMServer::handleDeleteEventList(vector<string> tokens)
{
    string eventList("");
    string outStr("");

    vector<string>::iterator it = tokens.begin();
    if (it != tokens.end())
        ++it;
    while (it != tokens.end())
    {
        if (eventList == "")
            eventList = (*it);
        else
            eventList += "," + (*it);

        ++it;
    }

    if (m_debug)
        cout << "Deleting events: " << eventList << endl;

    string sql("");
    sql += "DELETE FROM Events WHERE Id IN (" + eventList + ")";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    ADD_STR(outStr, "OK")
    send(outStr);
}

void ZMServer::handleRunZMAudit(void)
{
    string outStr("");

    // run zmaudit.pl to clean up orphaned db entries etc
    string command(g_binPath + "/zmaudit.pl &");

    if (m_debug)
        cout << "Running command: " << command << endl;

    errno = 0;
    if (system(command.c_str()) < 0 && errno)
        cerr << "Failed to run '" << command << "'" << endl;

    ADD_STR(outStr, "OK")
    send(outStr);
}

void ZMServer::getMonitorList(void)
{
    m_monitors.clear();

    string sql("SELECT Id, Name, Width, Height, ImageBufferCount, MaxFPS, Palette, ");
    sql += " Type, Function, Enabled, Device, Host, Controllable, TrackMotion ";
    sql += "FROM Monitors";

    MYSQL_RES *res;
    MYSQL_ROW row;

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        return;
    }

    res = mysql_store_result(&g_dbConn);
    int monitorCount = mysql_num_rows(res);

    if (m_debug)
        cout << "Got " << monitorCount << " monitors" << endl;

    for (int x = 0; x < monitorCount; x++)
    {
        row = mysql_fetch_row(res);
        if (row)
        {
            MONITOR *m = new MONITOR;
            m->mon_id = atoi(row[0]);
            m->name = row[1];
            m->width = atoi(row[2]);
            m->height = atoi(row[3]);
            m->image_buffer_count = atoi(row[4]);
            m->palette = atoi(row[6]);
            m->type = row[7];
            m->function = row[8];
            m->enabled = atoi(row[9]);
            m->device = row[10];
            m->host = row[11];
            m->controllable = atoi(row[12]);
            m->trackMotion = atoi(row[13]);
            m_monitors[m->mon_id] = m;

            initMonitor(m);
        }
        else
        {
            cout << "Failed to get mysql row" << endl;
            return;
        }
    }

    mysql_free_result(res);
}

void ZMServer::initMonitor(MONITOR *monitor)
{
    monitor->shm_ptr = NULL;
    monitor->mapFile = -1;
    monitor->shared_data = NULL;
    monitor->shared_images = NULL;

    if (monitor->palette == 1)
        monitor->frame_size = monitor->width * monitor->height;
    else
        monitor->frame_size = monitor->width * monitor->height * 3;

    int shared_data_size;

    shared_data_size = sizeof(SharedData) +
            sizeof(TriggerData) +
            ((monitor->image_buffer_count) * (sizeof(struct timeval))) +
            ((monitor->image_buffer_count) * monitor->frame_size);


#if _POSIX_MAPPED_FILES > 0L
    /*
     * Try to open the mmap file first if the architecture supports it.
     * Otherwise, legacy shared memory will be used below.
     */
    stringstream mmap_filename;
    mmap_filename << m_mmapPath << "/zm.mmap." << monitor->mon_id;

    monitor->mapFile = open(mmap_filename.str().c_str(), O_RDONLY, 0x0);
    if (monitor->mapFile >= 0)
    {
        if (m_debug)
            cout << "Opened mmap file: " << mmap_filename << endl;

        monitor->shm_ptr = mmap(NULL, shared_data_size, PROT_READ,
                                MAP_SHARED, monitor->mapFile, 0x0);
        if (monitor->shm_ptr == MAP_FAILED)
        {
            cout << "Failed to map shared memory from file ["
                 << mmap_filename << "] " << "for monitor: "
                 << monitor->mon_id << endl;
            monitor->status = "Error";

            if (close(monitor->mapFile) == -1)
                cout << "Failed to close mmap file" << endl;

            monitor->mapFile = -1;
            monitor->shm_ptr = NULL;

            return;
        }
    }
    else
    {
        // this is not necessarily a problem, maybe the user is still
        // using the legacy shared memory support
        if (m_debug)
        {
            cout << "Failed to open mmap file [" << mmap_filename << "] "
                 << "for monitor: " << monitor->mon_id
                 << " : " << strerror(errno) << endl;
            cout << "Falling back to the legacy shared memory method" << endl;
        }
    }
#endif

    if (monitor->shm_ptr == NULL)
    {
        // fail back to shmget() functionality if mapping memory above failed.
        int shmid;

        if ((shmid = shmget((m_shmKey & 0xffffff00) | monitor->mon_id,
             shared_data_size, SHM_R)) == -1)
        {
            cout << "Failed to shmget for monitor: " << monitor->mon_id << endl;
            monitor->status = "Error";
            switch(errno)
            {
                case EACCES: cout << "EACCES - no rights to access segment\n"; break;
                case EEXIST: cout << "EEXIST - segment already exists\n"; break;
                case EINVAL: cout << "EINVAL - size < SHMMIN or size > SHMMAX\n"; break;
                case ENFILE: cout << "ENFILE - limit on open files has been reached\n"; break;
                case ENOENT: cout << "ENOENT - no segment exists for the given key\n"; break;
                case ENOMEM: cout << "ENOMEM - couldn't reserve memory for segment\n"; break;
                case ENOSPC: cout << "ENOSPC - shmmni or shmall limit reached\n"; break;
            }

            return;
        }

        monitor->shm_ptr = shmat(shmid, 0, SHM_RDONLY);


        if (monitor->shm_ptr == NULL)
        {
            cout << "Failed to shmat for monitor: " << monitor->mon_id << endl;
            monitor->status = "Error";
            return;
        }
    }

    monitor->shared_data = (SharedData*)monitor->shm_ptr;

    monitor->shared_images = (unsigned char*) monitor->shm_ptr +
            sizeof(SharedData) +
            sizeof(TriggerData) +
            ((monitor->image_buffer_count) * sizeof(struct timeval));

#if 0
    // if this a v4l2 source align the buffer to 16 bytes
    if (monitor->palette > 255)
        monitor->shared_images = (unsigned char*) ((size_t(monitor->shared_images) + 15) & ~0x0f);
#endif
}

int ZMServer::getFrame(unsigned char *buffer, int bufferSize, MONITOR *monitor)
{
    (void) bufferSize;

    // is there a new frame available?
    if (monitor->shared_data->last_write_index == monitor->last_read)
        return 0;

    // sanity check last_read
    if (monitor->shared_data->last_write_index < 0 ||
            monitor->shared_data->last_write_index >= monitor->image_buffer_count)
        return 0;

    monitor->last_read = monitor->shared_data->last_write_index;

    switch (monitor->shared_data->state)
    {
        case IDLE:
            monitor->status = "Idle";
            break;
        case PREALARM:
            monitor->status = "Pre Alarm";
            break;
        case ALARM:
            monitor->status = "Alarm";
            break;
        case ALERT:
            monitor->status = "Alert";
            break;
        case TAPE:
            monitor->status = "Tape";
            break;
        default:
            monitor->status = "Unknown";
            break;
    }

    unsigned char *data = monitor->shared_images +
            monitor->frame_size * monitor->last_read;

    // FIXME: should do some sort of compression JPEG??
    // just copy the data to our buffer for now
    memcpy(buffer, data, monitor->frame_size);

    return monitor->frame_size;
}

string ZMServer::getZMSetting(const string &setting)
{
    string result;
    string sql("SELECT Name, Value FROM Config ");
    sql += "WHERE Name = '" + setting + "'";

    MYSQL_RES *res;
    MYSQL_ROW row;

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        return "";
    }

    res = mysql_store_result(&g_dbConn);
    row = mysql_fetch_row(res);
    if (row)
    {
        result = row[1];
    }
    else
    {
        cout << "Failed to get mysql row" << endl;
        result = "";
    }

    if (m_debug)
        cout << "getZMSetting: " << setting << " Result: " << result << endl;

    mysql_free_result(res);

    return result;
}

void ZMServer::handleSetMonitorFunction(vector<string> tokens)
{
    string outStr("");

    if (tokens.size() != 4)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    string monitorID(tokens[1]);
    string function(tokens[2]);
    string enabled(tokens[3]);

    // Check validity of input passed to server. Does monitor exist && is function ok
    if (m_monitors.find(atoi(monitorID.c_str())) == m_monitors.end())
    {
        sendError(ERROR_INVALID_MONITOR);
        return;
    }

    if (function != FUNCTION_NONE && function != FUNCTION_MONITOR &&
        function != FUNCTION_MODECT && function != FUNCTION_NODECT &&
        function != FUNCTION_RECORD && function != FUNCTION_MOCORD)
    {
        sendError(ERROR_INVALID_MONITOR_FUNCTION);
        return;
    }

    if (enabled != "0" && enabled != "1")
    {
        sendError(ERROR_INVALID_MONITOR_ENABLE_VALUE);
        return;
    }

    if (m_debug)
        cout << "User input validated OK" << endl;


    // Now perform db update && (re)start/stop daemons as required.
    MONITOR *monitor = m_monitors[atoi(monitorID.c_str())];
    string oldFunction = monitor->function;
    string newFunction = function;
    int oldEnabled  = monitor->enabled;
    int newEnabled  = atoi(enabled.c_str());
    monitor->function = newFunction;
    monitor->enabled = newEnabled;

    if (m_debug)
        cout << "SetMonitorFunction MonitorId: " << monitorID << endl <<
                "  oldEnabled: " << oldEnabled << endl <<
                "  newEnabled: " << newEnabled << endl <<
                " oldFunction: " << oldFunction << endl <<
                " newFunction: " << newFunction << endl;

    if ( newFunction != oldFunction || newEnabled != oldEnabled)
    {
        string sql("UPDATE Monitors ");
        sql += "SET Function = '" + function + "', ";
        sql += "Enabled = '" + enabled + "' ";
        sql += "WHERE Id = '" + monitorID + "'";

        if (mysql_query(&g_dbConn, sql.c_str()))
        {
            fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
            sendError(ERROR_MYSQL_QUERY);
            return;
        }

        if (m_debug)
            cout << "Monitor function SQL update OK" << endl;

        string status = runCommand(g_binPath + "/zmdc.pl check");

        // Now refresh servers
        if (RUNNING.compare(0, RUNNING.size(), status, 0, RUNNING.size()) == 0)
        {
            if (m_debug)
                cout << "Monitor function Refreshing daemons" << endl;

            bool restart = (oldFunction == FUNCTION_NONE) ||
                           (newFunction == FUNCTION_NONE) ||
                           (newEnabled != oldEnabled);

            if (restart)
                zmcControl(monitor, RESTART);
            else
                zmcControl(monitor, "");
            zmaControl(monitor, RELOAD);
        }
        else
            if (m_debug)
                cout << "zm daemons are not running" << endl;
    }
    else
        cout << "Not updating monitor function as identical to existing configuration" << endl;

    ADD_STR(outStr, "OK")
    send(outStr);
}

void ZMServer::zmcControl(MONITOR *monitor, const string &mode)
{
    string zmcArgs = "";
    string sql = "";
    sql += "SELECT count(if(Function!='None',1,NULL)) as ActiveCount ";
    sql += "FROM Monitors ";

    if (monitor->type == "Local" )
    {
        sql += "WHERE Device = '" + monitor->device + "'";
        zmcArgs = "-d " + monitor->device;
    }
    else
    {
        sql += "WHERE Id = '" + monitor->getIdStr() + "'";
        zmcArgs = "-m " + monitor->mon_id;
    }

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    MYSQL_RES *res;
    MYSQL_ROW row;
    int activeCount;

    res = mysql_store_result(&g_dbConn);
    row = mysql_fetch_row(res);

    if (row)
        activeCount = atoi(row[0]);
    else
    {
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    if (!activeCount)
        runCommand(g_binPath + "/zmdc.pl stop zmc " + zmcArgs);
    else
    {
        if (mode == RESTART)
            runCommand(g_binPath + "/zmdc.pl stop zmc " + zmcArgs);

        runCommand(g_binPath + "/zmdc.pl start zmc " + zmcArgs);
    }
}

void ZMServer::zmaControl(MONITOR *monitor, const string &mode)
{
    int zmOptControl = atoi(getZMSetting("ZM_OPT_CONTROL").c_str());
    int zmOptFrameServer = atoi(getZMSetting("ZM_OPT_FRAME_SERVER").c_str());

    if (monitor->function == FUNCTION_MODECT ||
        monitor->function == FUNCTION_RECORD ||
        monitor->function == FUNCTION_MOCORD ||
        monitor->function == FUNCTION_NODECT)
    {
        if (mode == RESTART)
        {
            if (zmOptControl)
                 runCommand(g_binPath + "/zmdc.pl stop zmtrack.pl -m " + monitor->getIdStr());

            runCommand(g_binPath + "/zmdc.pl stop zma -m " + monitor->getIdStr());

            if (zmOptFrameServer)
                runCommand(g_binPath + "/zmdc.pl stop zmf -m " + monitor->getIdStr());
        }

        if (zmOptFrameServer)
            runCommand(g_binPath + "/zmdc.pl start zmf -m " + monitor->getIdStr());

        runCommand(g_binPath + "/zmdc.pl start zma -m " + monitor->getIdStr());

        if (zmOptControl && monitor->controllable && monitor->trackMotion &&
            ( monitor->function == FUNCTION_MODECT || monitor->function == FUNCTION_MOCORD) )
            runCommand(g_binPath + "/zmdc.pl start zmtrack.pl -m " + monitor->getIdStr());

        if (mode == RELOAD)
            runCommand(g_binPath + "/zmdc.pl reload zma -m " + monitor->getIdStr());
    }
    else
    {
        if (zmOptControl)
                 runCommand(g_binPath + "/zmdc.pl stop zmtrack.pl -m " + monitor->getIdStr());

        runCommand(g_binPath + "/zmdc.pl stop zma -m " + monitor->getIdStr());

        if (zmOptFrameServer)
                runCommand(g_binPath + "/zmdc.pl stop zmf -m " + monitor->getIdStr());
    }
}
