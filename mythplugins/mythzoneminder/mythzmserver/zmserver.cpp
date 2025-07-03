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


#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>

#ifdef __linux__
#  include <sys/vfs.h>
#  include <sys/statvfs.h>
#  include <sys/sysinfo.h>
#else
#  include <sys/param.h>
#  include <sys/mount.h>
#  ifdef __CYGWIN__
#    include <sys/statfs.h>
#  else // if !__CYGWIN__
#    include <sys/sysctl.h>
#  endif // !__CYGWIN__
#endif

#ifndef MSG_NOSIGNAL
static constexpr int MSG_NOSIGNAL { 0 };  // Apple also has SO_NOSIGPIPE?
#endif

#include "zmserver.h"

// the version of the protocol we understand
static constexpr const char* ZM_PROTOCOL_VERSION { "11" };

static inline void ADD_STR(std::string& list, const std::string& s)
{ list += s; list += "[]:[]"; };
static inline void ADD_INT(std::string& list, int n)
{ list += std::to_string(n); list += "[]:[]"; };

// error messages
static constexpr const char* ERROR_TOKEN_COUNT      { "Invalid token count" };
static constexpr const char* ERROR_MYSQL_QUERY      { "Mysql Query Error" };
static constexpr const char* ERROR_MYSQL_ROW        { "Mysql Get Row Error" };
static constexpr const char* ERROR_FILE_OPEN        { "Cannot open event file" };
static constexpr const char* ERROR_INVALID_MONITOR  { "Invalid Monitor" };
static constexpr const char* ERROR_INVALID_POINTERS { "Cannot get shared memory pointers" };
static constexpr const char* ERROR_INVALID_MONITOR_FUNCTION  { "Invalid Monitor Function" };
static constexpr const char* ERROR_INVALID_MONITOR_ENABLE_VALUE { "Invalid Monitor Enable Value" };
static constexpr const char* ERROR_NO_FRAMES         { "No frames found for event" };

// Subpixel ordering (from zm_rgb.h)
// Based on byte order naming. For example, for ARGB (on both little endian or big endian)
// byte+0 should be alpha, byte+1 should be red, and so on.
enum ZM_SUBPIX_ORDER : std::uint8_t {
    ZM_SUBPIX_ORDER_NONE =  2,
    ZM_SUBPIX_ORDER_RGB  =  6,
    ZM_SUBPIX_ORDER_BGR  =  5,
    ZM_SUBPIX_ORDER_BGRA =  7,
    ZM_SUBPIX_ORDER_RGBA =  8,
    ZM_SUBPIX_ORDER_ABGR =  9,
    ZM_SUBPIX_ORDER_ARGB = 10,
};

MYSQL   g_dbConn;
std::string  g_zmversion;
std::string  g_password;
std::string  g_server;
std::string  g_database;
std::string  g_webPath;
std::string  g_user;
std::string  g_webUser;
std::string  g_binPath;
std::string  g_mmapPath;
std::string  g_eventsPath;
int     g_majorVersion = 0;
int     g_minorVersion = 0;
int     g_revisionVersion = 0;

TimePoint  g_lastDBKick {};

// returns true if the ZM version >= the requested version
bool checkVersion(int major, int minor, int revision)
{
    return g_majorVersion    >= major &&
           g_minorVersion    >= minor &&
           g_revisionVersion >= revision;
}

void loadZMConfig(const std::string &configfile)
{
    std::cout << "loading zm config from " << configfile << std::endl;

    std::ifstream ifs(configfile);
    if ( ifs.fail() )
    {
        fprintf(stderr, "Can't open %s\n", configfile.c_str());
    }

    std::string line {};
    while ( std::getline(ifs, line) )
    {
        // Trim off begining and ending whitespace including cr/lf line endings
        constexpr const char *whitespace = " \t\r\n";
        auto begin = line.find_first_not_of(whitespace);
        if (begin == std::string::npos)
            continue; // Only whitespace
        auto end = line.find_last_not_of(whitespace);
        if (end != std::string::npos)
            end = end + 1;
        line = line.substr(begin, end);

        // Check for comment or empty line
        if ( line.empty() || line[0] == '#' )
            continue;

        // Now look for the '=' in the middle of the line
        auto index = line.find('=');
        if (index == std::string::npos)
        {
            fprintf(stderr,"Invalid data in %s: '%s'\n", configfile.c_str(), line.c_str() );
            continue;
        }

        // Assign the name and value parts
        std::string name = line.substr(0,index);
        std::string val = line.substr(index+1);

        // Trim trailing space from the name part
        end = name.find_last_not_of(whitespace);
        if (end != std::string::npos)
            end = end + 1;
        name = name.substr(0, end);

        // Remove leading white space from the value part
        begin = val.find_first_not_of(whitespace);
        if (begin != std::string::npos)
            val = val.substr(begin);

        // convert name to uppercase
        std::transform(name.cbegin(), name.cend(), name.begin(), ::toupper);

        if      ( name == "ZM_DB_HOST"    ) g_server = val;
        else if ( name == "ZM_DB_NAME"    ) g_database = val;
        else if ( name == "ZM_DB_USER"    ) g_user = val;
        else if ( name == "ZM_DB_PASS"    ) g_password = val;
        else if ( name == "ZM_PATH_WEB"   ) g_webPath = val;
        else if ( name == "ZM_PATH_BIN"   ) g_binPath = val;
        else if ( name == "ZM_WEB_USER"   ) g_webUser = val;
        else if ( name == "ZM_VERSION"    ) g_zmversion = val;
        else if ( name == "ZM_PATH_MAP"   ) g_mmapPath = val;
        else if ( name == "ZM_DIR_EVENTS" ) g_eventsPath = val;
    }
}

#if !defined(MARIADB_BASE_VERSION) && MYSQL_VERSION_ID >= 80000
using reconnect_t = int;
#else
using reconnect_t = my_bool;
#endif

void connectToDatabase(void)
{
    if (!mysql_init(&g_dbConn))
    {
        std::cout << "Error: Can't initialise structure: " <<  mysql_error(&g_dbConn) << std::endl;
        exit(static_cast<int>(mysql_errno(&g_dbConn)));
    }

    reconnect_t reconnect = 1;
    mysql_options(&g_dbConn, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(&g_dbConn, g_server.c_str(), g_user.c_str(),
         g_password.c_str(), nullptr, 0, nullptr, 0))
    {
        std::cout << "Error: Can't connect to server: " <<  mysql_error(&g_dbConn) << std::endl;
        exit(static_cast<int>(mysql_errno( &g_dbConn)));
    }

    if (mysql_select_db(&g_dbConn, g_database.c_str()))
    {
        std::cout << "Error: Can't select database: " << mysql_error(&g_dbConn) << std::endl;
        exit(static_cast<int>(mysql_errno(&g_dbConn)));
    }
}

void kickDatabase(bool debug)
{
    if (Clock::now() < g_lastDBKick + DB_CHECK_TIME)
        return;

    if (debug)
        std::cout << "Kicking database connection" << std::endl;

    g_lastDBKick = Clock::now();

    if (mysql_query(&g_dbConn, "SELECT NULL;") == 0)
    {
        MYSQL_RES *res = mysql_store_result(&g_dbConn);
        if (res)
            mysql_free_result(res);
        return;
    }

    std::cout << "Lost connection to DB - trying to reconnect" << std::endl;

    // failed so try to reconnect to the DB
    mysql_close(&g_dbConn);
    connectToDatabase();
}

///////////////////////////////////////////////////////////////////////

void MONITOR::initMonitor(bool debug, const std::string &mmapPath, int shmKey)
{
    size_t shared_data_size = 0;
    size_t frame_size = static_cast<size_t>(m_width) * m_height * m_bytesPerPixel;

    if (!m_enabled)
        return;

    if (checkVersion(1, 34, 0))
    {
        shared_data_size = sizeof(SharedData34) +
            sizeof(TriggerData26) +
            ((m_imageBufferCount) * (sizeof(struct timeval))) +
            ((m_imageBufferCount) * frame_size) + 64;
    }
    else if (checkVersion(1, 32, 0))
    {
        shared_data_size = sizeof(SharedData32) +
            sizeof(TriggerData26) +
            ((m_imageBufferCount) * (sizeof(struct timeval))) +
            ((m_imageBufferCount) * frame_size) + 64;
    }
    else if (checkVersion(1, 26, 0))
    {
        shared_data_size = sizeof(SharedData26) +
            sizeof(TriggerData26) +
            ((m_imageBufferCount) * (sizeof(struct timeval))) +
            ((m_imageBufferCount) * frame_size) + 64;
    }
    else
    {
        shared_data_size = sizeof(SharedData) +
            sizeof(TriggerData) +
            ((m_imageBufferCount) * (sizeof(struct timeval))) +
            ((m_imageBufferCount) * frame_size);
    }

#if _POSIX_MAPPED_FILES > 0L
    /*
     * Try to open the mmap file first if the architecture supports it.
     * Otherwise, legacy shared memory will be used below.
     */
    std::stringstream mmap_filename;
    mmap_filename << mmapPath << "/zm.mmap." << m_monId;

    m_mapFile = open(mmap_filename.str().c_str(), O_RDONLY, 0x0);
    if (m_mapFile >= 0)
    {
        if (debug)
            std::cout << "Opened mmap file: " << mmap_filename.str() << std::endl;

        m_shmPtr = mmap(nullptr, shared_data_size, PROT_READ,
                       MAP_SHARED, m_mapFile, 0x0);
        if (m_shmPtr == MAP_FAILED)
        {
            std::cout << "Failed to map shared memory from file ["
                      << mmap_filename.str() << "] " << "for monitor: "
                      << m_monId << std::endl;
            m_status = "Error";

            if (close(m_mapFile) == -1)
                std::cout << "Failed to close mmap file" << std::endl;

            m_mapFile = -1;
            m_shmPtr = nullptr;

            return;
        }
    }
    else
    {
        // this is not necessarily a problem, maybe the user is still
        // using the legacy shared memory support
        if (debug)
        {
            std::cout << "Failed to open mmap file [" << mmap_filename.str() << "] "
                      << "for monitor: " << m_monId
                      << " : " << strerror(errno) << std::endl;
            std::cout << "Falling back to the legacy shared memory method" << std::endl;
        }
    }
#endif

    if (m_shmPtr == nullptr)
    {
        // fail back to shmget() functionality if mapping memory above failed.
        int shmid = shmget((shmKey & 0xffff0000) | m_monId,
                           shared_data_size, SHM_R);
        if (shmid == -1)
        {
            std::cout << "Failed to shmget for monitor: " << m_monId << std::endl;
            m_status = "Error";
            switch(errno)
            {
                case EACCES: std::cout << "EACCES - no rights to access segment\n"; break;
                case EEXIST: std::cout << "EEXIST - segment already exists\n"; break;
                case EINVAL: std::cout << "EINVAL - size < SHMMIN or size > SHMMAX\n"; break;
                case ENFILE: std::cout << "ENFILE - limit on open files has been reached\n"; break;
                case ENOENT: std::cout << "ENOENT - no segment exists for the given key\n"; break;
                case ENOMEM: std::cout << "ENOMEM - couldn't reserve memory for segment\n"; break;
                case ENOSPC: std::cout << "ENOSPC - shmmni or shmall limit reached\n"; break;
            }

            return;
        }

        m_shmPtr = shmat(shmid, nullptr, SHM_RDONLY);


        if (m_shmPtr == nullptr)
        {
            std::cout << "Failed to shmat for monitor: " << m_monId << std::endl;
            m_status = "Error";
            return;
        }
    }

    if (checkVersion(1, 34, 0))
    {
        m_sharedData = nullptr;
        m_sharedData26 = nullptr;
        m_sharedData32 = nullptr;
        m_sharedData34 = (SharedData34*)m_shmPtr;

        m_sharedImages = (unsigned char*) m_shmPtr +
            sizeof(SharedData34) + sizeof(TriggerData26) + sizeof(VideoStoreData) +
            ((m_imageBufferCount) * sizeof(struct timeval)) ;

        if (((unsigned long)m_sharedImages % 64) != 0)
        {
            // align images buffer to nearest 64 byte boundary
            m_sharedImages += (64 - ((unsigned long)m_sharedImages % 64));
        }
    }
    else if (checkVersion(1, 32, 0))
    {
        m_sharedData = nullptr;
        m_sharedData26 = nullptr;
        m_sharedData32 = (SharedData32*)m_shmPtr;
        m_sharedData34 = nullptr;

        m_sharedImages = (unsigned char*) m_shmPtr +
            sizeof(SharedData32) + sizeof(TriggerData26) + sizeof(VideoStoreData) +
            ((m_imageBufferCount) * sizeof(struct timeval)) ;

        if (((unsigned long)m_sharedImages % 64) != 0)
        {
            // align images buffer to nearest 64 byte boundary
            m_sharedImages += (64 - ((unsigned long)m_sharedImages % 64));
        }
    }
    else if (checkVersion(1, 26, 0))
    {
        m_sharedData = nullptr;
        m_sharedData26 = (SharedData26*)m_shmPtr;
        m_sharedData32 = nullptr;
        m_sharedData34 = nullptr;

        m_sharedImages = (unsigned char*) m_shmPtr +
            sizeof(SharedData26) + sizeof(TriggerData26) +
            ((m_imageBufferCount) * sizeof(struct timeval));

        if (((unsigned long)m_sharedImages % 16) != 0)
        {
            // align images buffer to nearest 16 byte boundary
            m_sharedImages += (16 - ((unsigned long)m_sharedImages % 16));
        }
    }
    else
    {
        m_sharedData = (SharedData*)m_shmPtr;
        m_sharedData26 = nullptr;
        m_sharedData32 = nullptr;
        m_sharedData34 = nullptr;

        m_sharedImages = (unsigned char*) m_shmPtr +
            sizeof(SharedData) + sizeof(TriggerData) +
            ((m_imageBufferCount) * sizeof(struct timeval));
    }
}

bool MONITOR::isValid(void)
{
    if (checkVersion(1, 34, 0))
        return m_sharedData34 != nullptr && m_sharedImages != nullptr;

    if (checkVersion(1, 32, 0))
        return m_sharedData32 != nullptr && m_sharedImages != nullptr;

    if (checkVersion(1, 26, 0))
        return m_sharedData26 != nullptr && m_sharedImages != nullptr;

    // must be version >= 1.24.0 and < 1.26.0
    return  m_sharedData != nullptr && m_sharedImages != nullptr;
}


std::string MONITOR::getIdStr(void)
{
    if (m_id.empty())
    {
        std::stringstream out;
        out << m_monId;
        m_id = out.str();
    }
    return m_id;
}

int MONITOR::getLastWriteIndex(void)
{
    if (m_sharedData)
        return m_sharedData->last_write_index;

    if (m_sharedData26)
        return m_sharedData26->last_write_index;

    if (m_sharedData32)
        return m_sharedData32->last_write_index;

    if (m_sharedData34)
        return m_sharedData34->last_write_index;

    return 0;
}

int MONITOR::getState(void)
{
    if (m_sharedData)
        return m_sharedData->state;

    if (m_sharedData26)
        return m_sharedData26->state;

    if (m_sharedData32)
        return m_sharedData32->state;

    if (m_sharedData34)
        return m_sharedData34->state;

    return 0;
}

int MONITOR::getSubpixelOrder(void)
{
    if (m_sharedData)
    {
        if (m_bytesPerPixel == 1)
            return ZM_SUBPIX_ORDER_NONE;
        return ZM_SUBPIX_ORDER_RGB;
    }

    if (m_sharedData26)
      return m_sharedData26->format;

    if (m_sharedData32)
      return m_sharedData32->format;

    if (m_sharedData34)
      return m_sharedData34->format;

    return ZM_SUBPIX_ORDER_NONE;
}

int MONITOR::getFrameSize(void)
{
    if (m_sharedData)
        return m_width * m_height * m_bytesPerPixel;

    if (m_sharedData26)
      return m_sharedData26->imagesize;

    if (m_sharedData32)
      return m_sharedData32->imagesize;

    if (m_sharedData34)
      return m_sharedData34->imagesize;

    return 0;
}

///////////////////////////////////////////////////////////////////////

ZMServer::ZMServer(int sock, bool debug)
{
    if (debug)
        std::cout << "Using server protocol version '" << ZM_PROTOCOL_VERSION << "'\n";

    m_sock = sock;
    m_debug = debug;

    // get the shared memory key
    m_shmKey = 0x7a6d2000;
    std::string setting = getZMSetting("ZM_SHM_KEY");

    if (!setting.empty())
    {
        unsigned long long tmp = m_shmKey;
        sscanf(setting.c_str(), "%20llx", &tmp);
        m_shmKey = tmp;
    }

    if (m_debug)
    {
        std::cout << "Shared memory key is: 0x"
                  << std::hex << (unsigned int)m_shmKey
                  << std::dec << std::endl;
    }

    // get the MMAP path
    if (checkVersion(1, 32, 0))
        m_mmapPath = g_mmapPath;
    else
        m_mmapPath = getZMSetting("ZM_PATH_MAP");

    if (m_debug)
    {
        std::cout << "Memory path directory is: " << m_mmapPath << std::endl;
    }

    // get the event filename format
    setting = getZMSetting("ZM_EVENT_IMAGE_DIGITS");
    int eventDigits = atoi(setting.c_str());
    std::string eventDigitsFmt = "%0" + std::to_string(eventDigits) + "d";
    m_eventFileFormat = eventDigitsFmt + "-capture.jpg";
    if (m_debug)
        std::cout << "Event file format is: " << m_eventFileFormat << std::endl;

    // get the analysis filename format
    m_analysisFileFormat = eventDigitsFmt + "-analyse.jpg";
    if (m_debug)
        std::cout << "Analysis file format is: " << m_analysisFileFormat << std::endl;

    // is ZM using the deep storage directory format?
    m_useDeepStorage = (getZMSetting("ZM_USE_DEEP_STORAGE") == "1");
    if (m_debug)
    {
        if (m_useDeepStorage)
            std::cout << "using deep storage directory structure" << std::endl;
        else
            std::cout << "using flat directory structure" << std::endl;
    }

    // is ZM creating analysis images?
    m_useAnalysisImages = (getZMSetting("ZM_CREATE_ANALYSIS_IMAGES") == "1");
    if (m_debug)
    {
        if (m_useAnalysisImages)
            std::cout << "using analysis images" << std::endl;
        else
            std::cout << "not using analysis images" << std::endl;
    }

    getMonitorList();
}

ZMServer::~ZMServer()
{
    for (auto *mon : m_monitors)
    {
        if (mon->m_mapFile != -1)
        {
            if (close(mon->m_mapFile) == -1)
                std::cout << "Failed to close mapFile" << std::endl;
            else
                if (m_debug)
                    std::cout << "Closed mapFile for monitor: " << mon->m_name << std::endl;
        }

        delete mon;
    }

    m_monitors.clear();
    m_monitorMap.clear();

    if (m_debug)
        std::cout << "ZMServer destroyed\n";
}

void ZMServer::tokenize(const std::string &command, std::vector<std::string> &tokens)
{
    std::string token;
    tokens.clear();
    std::string::size_type startPos = 0;
    std::string::size_type endPos = 0;

    while((endPos = command.find("[]:[]", startPos)) != std::string::npos)
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
    std::string s(buf+8);
    std::vector<std::string> tokens;
    tokenize(s, tokens);

    if (tokens.empty())
        return false;

    if (m_debug)
        std::cout << "Processing: '" << tokens[0] << "'" << std::endl;

    if (tokens[0] == "HELLO")
        handleHello();
    else if (tokens[0] == "QUIT")
        return true;
    else if (tokens[0] == "GET_SERVER_STATUS")
        handleGetServerStatus();
    else if (tokens[0] == "GET_MONITOR_STATUS")
        handleGetMonitorStatus();
    else if (tokens[0] == "GET_ALARM_STATES")
        handleGetAlarmStates();
    else if (tokens[0] == "GET_EVENT_LIST")
        handleGetEventList(tokens);
    else if (tokens[0] == "GET_EVENT_DATES")
        handleGetEventDates(tokens);
    else if (tokens[0] == "GET_EVENT_FRAME")
        handleGetEventFrame(tokens);
    else if (tokens[0] == "GET_ANALYSE_FRAME")
        handleGetAnalysisFrame(tokens);
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

bool ZMServer::send(const std::string &s) const
{
    // send length
    std::string str = "0000000" + std::to_string(s.size());
    str.erase(0, str.size()-8);
    int status = ::send(m_sock, str.data(), 8, MSG_NOSIGNAL);
    if (status == -1)
        return false;

    // send message
    status = ::send(m_sock, s.c_str(), s.size(), MSG_NOSIGNAL);
    return status != -1;
}

bool ZMServer::send(const std::string &s, const unsigned char *buffer, int dataLen) const
{
    // send length
    std::string str = "0000000" + std::to_string(s.size());
    str.erase(0, str.size()-8);
    int status = ::send(m_sock, str.data(), 8, MSG_NOSIGNAL);
    if (status == -1)
        return false;

    // send message
    status = ::send(m_sock, s.c_str(), s.size(), MSG_NOSIGNAL);
    if ( status == -1 )
        return false;

    // send data
    status = ::send(m_sock, buffer, dataLen, MSG_NOSIGNAL);
    return status != -1;
}

void ZMServer::sendError(const std::string &error)
{
    std::string outStr;
    ADD_STR(outStr, std::string("ERROR - ") + error);
    send(outStr);
}

void ZMServer::handleHello()
{
    // just send OK so the client knows all is well
    // followed by the protocol version we understand
    std::string outStr;
    ADD_STR(outStr, "OK");
    ADD_STR(outStr, ZM_PROTOCOL_VERSION);
    send(outStr);
}

long long ZMServer::getDiskSpace(const std::string &filename, long long &total, long long &used)
{
    struct statfs statbuf {};
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
    std::string outStr;
    ADD_STR(outStr, "OK");

    // server status
    std::string status = runCommand(g_binPath + "/zmdc.pl check");
    ADD_STR(outStr, status);

    // get load averages
    std::array<double,3> loads {};
    if (getloadavg(loads.data(), 3) == -1)
    {
        ADD_STR(outStr, "Unknown");
    }
    else
    {
        // to_string gives six decimal places.  Drop last four.
        std::string buf = std::to_string(loads[0]);
        buf.resize(buf.size() - 4);
        ADD_STR(outStr, buf);
    }

    // get free space on the disk where the events are stored
    long long total = 0;
    long long used = 0;
    std::string eventsDir = g_webPath + "/events/";
    getDiskSpace(eventsDir, total, used);
    std::string buf = std::to_string(static_cast<int>((used * 100) / total)) + "%";
    ADD_STR(outStr, buf);

    send(outStr);
}

void ZMServer::handleGetAlarmStates(void)
{
    std::string outStr;
    ADD_STR(outStr, "OK");

    // add the monitor count
    ADD_INT(outStr, (int)m_monitors.size());

    for (auto *monitor : m_monitors)
    {
        // add monitor ID
        ADD_INT(outStr, monitor->m_monId);

        // add monitor status
        ADD_INT(outStr, monitor->getState());
    }

    send(outStr);
}

void ZMServer::handleGetEventList(std::vector<std::string> tokens)
{
    std::string outStr;

    if (tokens.size() != 5)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    const std::string& monitor = tokens[1];
    bool oldestFirst = (tokens[2] == "1");
    const std::string& date = tokens[3];
    bool includeContinuous = (tokens[4] == "1");

    if (m_debug)
        std::cout << "Loading events for monitor: " << monitor << ", date: " << date << std::endl;

    ADD_STR(outStr, "OK");

    std::string sql("SELECT E.Id, E.Name, M.Id AS MonitorID, M.Name AS MonitorName, E.StartTime,  "
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
        {
            sql += "WHERE DATE(E.StartTime) = DATE('" + date + "') ";

            if (!includeContinuous)
                sql += "AND Cause != 'Continuous' ";
        }
        else
            if (!includeContinuous)
            {
                sql += "WHERE Cause != 'Continuous' ";
            }
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

    MYSQL_RES *res = mysql_store_result(&g_dbConn);
    int eventCount = mysql_num_rows(res);

    if (m_debug)
        std::cout << "Got " << eventCount << " events" << std::endl;

    ADD_INT(outStr, eventCount);

    for (int x = 0; x < eventCount; x++)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            ADD_STR(outStr, row[0]); // eventID
            ADD_STR(outStr, row[1]); // event name
            ADD_STR(outStr, row[2]); // monitorID
            ADD_STR(outStr, row[3]); // monitor name
            row[4][10] = 'T';
            ADD_STR(outStr, row[4]); // start time
            ADD_STR(outStr, row[5]); // length
        }
        else
        {
            std::cout << "Failed to get mysql row" << std::endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

void ZMServer::handleGetEventDates(std::vector<std::string> tokens)
{
    std::string outStr;

    if (tokens.size() != 3)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    const std::string& monitor = tokens[1];
    bool oldestFirst = (tokens[2] == "1");

    if (m_debug)
        std::cout << "Loading event dates for monitor: " << monitor << std::endl;

    ADD_STR(outStr, "OK");

    std::string sql("SELECT DISTINCT DATE(E.StartTime) "
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

    MYSQL_RES *res = mysql_store_result(&g_dbConn);
    int dateCount = mysql_num_rows(res);

    if (m_debug)
        std::cout << "Got " << dateCount << " dates" << std::endl;

    ADD_INT(outStr, dateCount);

    for (int x = 0; x < dateCount; x++)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            ADD_STR(outStr, row[0]); // event date
        }
        else
        {
            std::cout << "Failed to get mysql row" << std::endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

void ZMServer::handleGetMonitorStatus(void)
{
    std::string outStr;
    ADD_STR(outStr, "OK");

    // get monitor list
    // Function is reserverd word so but ticks around it
    std::string sql("SELECT Id, Name, Type, Device, Host, Channel, `Function`, Enabled "
               "FROM Monitors;");
    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    MYSQL_RES *res = mysql_store_result(&g_dbConn);

     // add monitor count
    int monitorCount = mysql_num_rows(res);

    if (m_debug)
        std::cout << "Got " << monitorCount << " monitors" << std::endl;

    ADD_INT(outStr, monitorCount);

    for (int x = 0; x < monitorCount; x++)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            std::string id = row[0];
            std::string type = row[2];
            std::string device = row[3];
            std::string host = row[4] ? row[4] : "";
            std::string channel = row[5];
            std::string function = row[6];
            std::string enabled = row[7];
            std::string name = row[1];
            std::string events;
            std::string zmcStatus;
            std::string zmaStatus;
            getMonitorStatus(id, type, device, host, channel, function,
                             zmcStatus, zmaStatus, enabled);

            std::string sql2("SELECT count(if(Archived=0,1,NULL)) AS EventCount "
                             "FROM Events AS E "
                             "WHERE MonitorId = " + id);

            if (mysql_query(&g_dbConn, sql2.c_str()))
            {
                fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
                sendError(ERROR_MYSQL_QUERY);
                return;
            }

            MYSQL_RES *res2 = mysql_store_result(&g_dbConn);
            if (mysql_num_rows(res2) > 0)
            {
                MYSQL_ROW row2 = mysql_fetch_row(res2);
                if (row2)
                    events = row2[0];
                else
                {
                    std::cout << "Failed to get mysql row" << std::endl;
                    sendError(ERROR_MYSQL_ROW);
                    return;
                }
            }

            ADD_STR(outStr, id);
            ADD_STR(outStr, name);
            ADD_STR(outStr, zmcStatus);
            ADD_STR(outStr, zmaStatus);
            ADD_STR(outStr, events);
            ADD_STR(outStr, function);
            ADD_STR(outStr, enabled);

            mysql_free_result(res2);
        }
        else
        {
            std::cout << "Failed to get mysql row" << std::endl;
            sendError(ERROR_MYSQL_ROW);
            return;
        }
    }

    mysql_free_result(res);

    send(outStr);
}

std::string ZMServer::runCommand(const std::string& command)
{
    std::string outStr;
    FILE *fd = popen(command.c_str(), "r");
    std::array<char,100> buffer {};

    while (fgets(buffer.data(), buffer.size(), fd) != nullptr)
    {
        outStr += buffer.data();
    }
    pclose(fd);
    return outStr;
}

void ZMServer::getMonitorStatus(const std::string &id, const std::string &type,
                                const std::string &device, const std::string &host,
                                const std::string &channel, const std::string &function,
                                std::string &zmcStatus, std::string &zmaStatus,
                                const std::string &enabled)
{
    zmaStatus = "";
    zmcStatus = "";

    std::string command(g_binPath + "/zmdc.pl status");
    std::string status = runCommand(command);

    if (type == "Local")
    {
        if (enabled == "0")
            zmaStatus = device + "(" + channel + ") [-]";
        else if (status.find("'zma -m " + id + "' running") != std::string::npos)
            zmaStatus = device + "(" + channel + ") [R]";
        else
            zmaStatus = device + "(" + channel + ") [S]";
    }
    else
    {
        if (enabled == "0")
            zmaStatus = host + " [-]";
        else if (status.find("'zma -m " + id + "' running") != std::string::npos)
            zmaStatus = host + " [R]";
        else
            zmaStatus = host + " [S]";
    }

    if (type == "Local")
    {
        if (enabled == "0")
            zmcStatus = function + " [-]";
        else if (status.find("'zmc -d "+ device + "' running") != std::string::npos)
            zmcStatus = function + " [R]";
        else
            zmcStatus = function + " [S]";
    }
    else
    {
        if (enabled == "0")
            zmcStatus = function + " [-]";
        else if (status.find("'zmc -m " + id + "' running") != std::string::npos)
            zmcStatus = function + " [R]";
        else
            zmcStatus = function + " [S]";
    }
}

void ZMServer::handleGetEventFrame(std::vector<std::string> tokens)
{
    static FrameData s_buffer {};

    if (tokens.size() != 5)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    const std::string& monitorID(tokens[1]);
    const std::string& eventID(tokens[2]);
    int frameNo = atoi(tokens[3].c_str());
    const std::string& eventTime(tokens[4]);

    if (m_debug)
    {
        std::cout << "Getting frame " << frameNo << " for event " << eventID
                  << " on monitor " << monitorID  << " event time is " << eventTime
                  << std::endl;
    }

    std::string outStr;

    ADD_STR(outStr, "OK");

    // try to find the frame file
    std::string filepath;
    std::string str (100,'\0');

    if (checkVersion(1, 32, 0))
    {
        int year = 0;
        int month = 0;
        int day = 0;

        sscanf(eventTime.data(), "%2d/%2d/%2d", &year, &month, &day);
        sprintf(str.data(), "20%02d-%02d-%02d", year, month, day);

        filepath = g_eventsPath + "/" + monitorID + "/" + str + "/" + eventID + "/";
        sprintf(str.data(), m_eventFileFormat.c_str(), frameNo);
        filepath += str;
    }
    else
    {
        if (m_useDeepStorage)
        {
            filepath = g_webPath + "/events/" + monitorID + "/" + eventTime + "/";
            sprintf(str.data(), m_eventFileFormat.c_str(), frameNo);
            filepath += str;
        }
        else
        {
            filepath = g_webPath + "/events/" + monitorID + "/" + eventID + "/";
            sprintf(str.data(), m_eventFileFormat.c_str(), frameNo);
            filepath += str;
        }
    }

    int fileSize = 0;
    FILE *fd = fopen(filepath.c_str(), "r" );
    if (fd != nullptr)
    {
        fileSize = fread(s_buffer.data(), 1, s_buffer.size(), fd);
        fclose(fd);
    }
    else
    {
        std::cout << "Can't open " << filepath << ": " << strerror(errno) << std::endl;
        sendError(ERROR_FILE_OPEN + std::string(" - ") + filepath + " : " + strerror(errno));
        return;
    }

    if (m_debug)
        std::cout << "Frame size: " <<  fileSize << std::endl;

    // get the file size
    ADD_INT(outStr, fileSize);

    // send the data
    send(outStr, s_buffer.data(), fileSize);
}

void ZMServer::handleGetAnalysisFrame(std::vector<std::string> tokens)
{
    static FrameData s_buffer {};
    std::array<char,100> str {};

    if (tokens.size() != 5)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    const std::string& monitorID(tokens[1]);
    const std::string& eventID(tokens[2]);
    int frameNo = atoi(tokens[3].c_str());
    const std::string& eventTime(tokens[4]);
    int frameID = 0;
    int frameCount = 0;

    if (m_debug)
    {
        std::cout << "Getting analysis frame " << frameNo << " for event " << eventID
                  << " on monitor " << monitorID << " event time is " << eventTime
                  << std::endl;
    }

    // get the 'alarm' frames from the Frames table for this event
    std::string sql;
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

    MYSQL_RES *res = mysql_store_result(&g_dbConn);
    frameCount = mysql_num_rows(res);

    // if we didn't find any alarm frames get the list of normal frames
    if (frameCount == 0)
    {
        mysql_free_result(res);

        sql  = "SELECT FrameId FROM Frames ";
        sql += "WHERE EventID = " + eventID + " ";
        sql += "ORDER BY FrameID";

        if (mysql_query(&g_dbConn, sql.c_str()))
        {
            fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
            sendError(ERROR_MYSQL_QUERY);
            return;
        }

        res = mysql_store_result(&g_dbConn);
        frameCount = mysql_num_rows(res);
    }

    // if frameCount is 0 then we can't go any further
    if (frameCount == 0)
    {
        std::cout << "handleGetAnalyseFrame: Failed to find any frames" << std::endl;
        sendError(ERROR_NO_FRAMES);
        return;
    }

    // if the required frame mumber is 0 or out of bounds then use the middle frame
    if (frameNo == 0 || frameNo < 0 || frameNo > frameCount)
        frameNo = (frameCount / 2) + 1;

    // move to the required frame in the table
    MYSQL_ROW row = nullptr;
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
        std::cout << "handleGetAnalyseFrame: Failed to get mysql row for frameNo " << frameNo << std::endl;
        sendError(ERROR_MYSQL_ROW);
        return;
    }

    mysql_free_result(res);

    std::string outStr;
    std::string filepath;
    std::string frameFile;

    if (checkVersion(1, 32, 0))
    {
        int year = 0;
        int month = 0;
        int day = 0;

        sscanf(eventTime.c_str(), "%2d/%2d/%2d", &year, &month, &day);
        sprintf(str.data(), "20%02d-%02d-%02d", year, month, day);
        filepath = g_eventsPath + "/" + monitorID + "/" + str.data() + "/" + eventID + "/";
    }
    else
    {
        if (m_useDeepStorage)
            filepath = g_webPath + "/events/" + monitorID + "/" + eventTime + "/";
        else
            filepath = g_webPath + "/events/" + monitorID + "/" + eventID + "/";
    }

    ADD_STR(outStr, "OK");

    FILE *fd = nullptr;
    int fileSize = 0;

    // try to find an analysis frame for the frameID
    if (m_useAnalysisImages)
    {
        sprintf(str.data(), m_analysisFileFormat.c_str(), frameID);
        frameFile = filepath + str.data();

        fd = fopen(frameFile.c_str(), "r" );
        if (fd != nullptr)
        {
            fileSize = fread(s_buffer.data(), 1, s_buffer.size(), fd);
            fclose(fd);

            if (m_debug)
                std::cout << "Frame size: " <<  fileSize << std::endl;

            // get the file size
            ADD_INT(outStr, fileSize);

            // send the data
            send(outStr, s_buffer.data(), fileSize);
            return;
        }
    }

    // try to find a normal frame for the frameID these should always be available
    sprintf(str.data(), m_eventFileFormat.c_str(), frameID);
    frameFile = filepath + str.data();

    fd = fopen(frameFile.c_str(), "r" );
    if (fd != nullptr)
    {
        fileSize = fread(s_buffer.data(), 1, s_buffer.size(), fd);
        fclose(fd);
    }
    else
    {
        std::cout << "Can't open " << frameFile << ": " << strerror(errno) << std::endl;
        sendError(ERROR_FILE_OPEN + std::string(" - ") + frameFile + " : " + strerror(errno));
        return;
    }

    if (m_debug)
        std::cout << "Frame size: " <<  fileSize << std::endl;

    // get the file size
    ADD_INT(outStr, fileSize);

    // send the data
    send(outStr, s_buffer.data(), fileSize);
}

void ZMServer::handleGetLiveFrame(std::vector<std::string> tokens)
{
    static FrameData s_buffer {};

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
        std::cout << "Getting live frame from monitor: " << monitorID << std::endl;

    std::string outStr;

    ADD_STR(outStr, "OK");

    // echo the monitor id
    ADD_INT(outStr, monitorID);

    // try to find the correct MONITOR
    if (m_monitorMap.find(monitorID) == m_monitorMap.end())
    {
        sendError(ERROR_INVALID_MONITOR);
        return;
    }
    MONITOR *monitor = m_monitorMap[monitorID];

    // are the data pointers valid?
    if (!monitor->isValid())
    {
        sendError(ERROR_INVALID_POINTERS);
        return;
    }

    // read a frame from the shared memory
    int dataSize = getFrame(s_buffer, monitor);

    if (m_debug)
        std::cout << "Frame size: " <<  dataSize << std::endl;

    if (dataSize == 0)
    {
        // not really an error
        outStr = "";
        ADD_STR(outStr, "WARNING - No new frame available");
        send(outStr);
        return;
    }

    // add status
    ADD_STR(outStr, monitor->m_status);

    // send the data size
    ADD_INT(outStr, dataSize);

    // send the data
    send(outStr, s_buffer.data(), dataSize);
}

void ZMServer::handleGetFrameList(std::vector<std::string> tokens)
{
    std::string eventID;
    std::string outStr;

    if (tokens.size() != 2)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    eventID = tokens[1];

    if (m_debug)
        std::cout << "Loading frames for event: " << eventID << std::endl;

    ADD_STR(outStr, "OK");

    // check to see what type of event this is
    std::string sql = "SELECT Cause, Length, Frames FROM Events ";
    sql += "WHERE Id = " + eventID + " ";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    MYSQL_RES *res = mysql_store_result(&g_dbConn);
    MYSQL_ROW row = mysql_fetch_row(res);

    // make sure we have some frames to display
    if (row[1] == nullptr || row[2] == nullptr)
    {
        sendError(ERROR_NO_FRAMES);
        return;
    }

    std::string cause = row[0];
    double length = atof(row[1]);
    int frameCount = atoi(row[2]);

    mysql_free_result(res);

    if (cause == "Continuous")
    {
        // event is a continuous recording so guess the frame delta's

        if (m_debug)
            std::cout << "Got " << frameCount << " frames (continuous event)" << std::endl;

        ADD_INT(outStr, frameCount);

        if (frameCount > 0)
        {
            double delta = length / frameCount;

            for (int x = 0; x < frameCount; x++)
            {
                ADD_STR(outStr, "Normal"); // Type
                ADD_STR(outStr, std::to_string(delta)); // Delta
            }
        }
    }
    else
    {
        sql  = "SELECT Type, Delta FROM Frames ";
        sql += "WHERE EventID = " + eventID + " ";
        sql += "ORDER BY FrameID";

        if (mysql_query(&g_dbConn, sql.c_str()))
        {
            fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
            sendError(ERROR_MYSQL_QUERY);
            return;
        }

        res = mysql_store_result(&g_dbConn);
        frameCount = mysql_num_rows(res);

        if (m_debug)
            std::cout << "Got " << frameCount << " frames" << std::endl;

        ADD_INT(outStr, frameCount);

        for (int x = 0; x < frameCount; x++)
        {
            row = mysql_fetch_row(res);
            if (row)
            {
                ADD_STR(outStr, row[0]); // Type
                ADD_STR(outStr, row[1]); // Delta
            }
            else
            {
                std::cout << "handleGetFrameList: Failed to get mysql row " << x << std::endl;
                sendError(ERROR_MYSQL_ROW);
                return;
            }
        }

        mysql_free_result(res);
    }

    send(outStr);
}

void ZMServer::handleGetCameraList(void)
{
    std::string outStr;

    ADD_STR(outStr, "OK");

    ADD_INT(outStr, (int)m_monitors.size());

    for (auto & monitor : m_monitors)
    {
        ADD_STR(outStr, monitor->m_name);
    }

    send(outStr);
}

void ZMServer::handleGetMonitorList(void)
{
    std::string outStr;

    ADD_STR(outStr, "OK");

    if (m_debug)
        std::cout << "We have " << m_monitors.size() << " monitors" << std::endl;

    ADD_INT(outStr, (int)m_monitors.size());;

    for (auto *mon : m_monitors)
    {
        ADD_INT(outStr, mon->m_monId);
        ADD_STR(outStr, mon->m_name);
        ADD_INT(outStr, mon->m_width);
        ADD_INT(outStr, mon->m_height);
        ADD_INT(outStr, mon->m_bytesPerPixel);

        if (m_debug)
        {
            std::cout << "id:             " << mon->m_monId            << std::endl;
            std::cout << "name:           " << mon->m_name             << std::endl;
            std::cout << "width:          " << mon->m_width            << std::endl;
            std::cout << "height:         " << mon->m_height           << std::endl;
            std::cout << "palette:        " << mon->m_palette          << std::endl;
            std::cout << "byte per pixel: " << mon->m_bytesPerPixel    << std::endl;
            std::cout << "sub pixel order:" << mon->getSubpixelOrder() << std::endl;
            std::cout << "-------------------" << std::endl;
        }
    }

    send(outStr);
}

void ZMServer::handleDeleteEvent(std::vector<std::string> tokens)
{
    std::string eventID;
    std::string outStr;

    if (tokens.size() != 2)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    eventID = tokens[1];

    if (m_debug)
        std::cout << "Deleting event: " << eventID << std::endl;

    ADD_STR(outStr, "OK");

    std::string sql;
    sql += "DELETE FROM Events WHERE Id = " + eventID;

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    // run zmaudit.pl to clean everything up
    std::string command(g_binPath + "/zmaudit.pl &");
    errno = 0;
    if (system(command.c_str()) < 0 && errno)
        std::cerr << "Failed to run '" << command << "'" << std::endl;

    send(outStr);
}

void ZMServer::handleDeleteEventList(std::vector<std::string> tokens)
{
    std::string eventList;
    std::string outStr;

    auto it = tokens.begin();
    if (it != tokens.end())
        ++it;
    while (it != tokens.end())
    {
        if (eventList.empty())
            eventList = (*it);
        else
            eventList += "," + (*it);

        ++it;
    }

    if (m_debug)
        std::cout << "Deleting events: " << eventList << std::endl;

    std::string sql;
    sql += "DELETE FROM Events WHERE Id IN (" + eventList + ")";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    ADD_STR(outStr, "OK");
    send(outStr);
}

void ZMServer::handleRunZMAudit(void)
{
    std::string outStr;

    // run zmaudit.pl to clean up orphaned db entries etc
    std::string command(g_binPath + "/zmaudit.pl &");

    if (m_debug)
        std::cout << "Running command: " << command << std::endl;

    errno = 0;
    if (system(command.c_str()) < 0 && errno)
        std::cerr << "Failed to run '" << command << "'" << std::endl;

    ADD_STR(outStr, "OK");
    send(outStr);
}

void ZMServer::getMonitorList(void)
{
    m_monitors.clear();
    m_monitorMap.clear();

    // Function is reserverd word so but ticks around it
    std::string sql("SELECT Id, Name, Width, Height, ImageBufferCount, MaxFPS, Palette, ");
    sql += " Type, `Function`, Enabled, Device, Host, Controllable, TrackMotion";

    if (checkVersion(1, 26, 0))
        sql += ", Colours";

    sql += " FROM Monitors";
    sql += " ORDER BY Sequence";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(&g_dbConn);
    int monitorCount = mysql_num_rows(res);

    if (m_debug)
        std::cout << "Got " << monitorCount << " monitors" << std::endl;

    for (int x = 0; x < monitorCount; x++)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            auto *m = new MONITOR;
            m->m_monId = atoi(row[0]);
            m->m_name = row[1];
            m->m_width = atoi(row[2]);
            m->m_height = atoi(row[3]);
            m->m_imageBufferCount = atoi(row[4]);
            m->m_palette = atoi(row[6]);
            m->m_type = row[7];
            m->m_function = row[8];
            m->m_enabled = atoi(row[9]);
            m->m_device = row[10];
            m->m_host = row[11] ? row[11] : "";
            m->m_controllable = atoi(row[12]);
            m->m_trackMotion = atoi(row[13]);

            // from version 1.26.0 ZM can have 1, 3 or 4 bytes per pixel
            // older versions can be 1 or 3
            if (checkVersion(1, 26, 0))
                m->m_bytesPerPixel = atoi(row[14]);
            else
                if (m->m_palette == 1)
                    m->m_bytesPerPixel = 1;
                else
                    m->m_bytesPerPixel = 3;

            m_monitors.push_back(m);
            m_monitorMap[m->m_monId] = m;

            m->initMonitor(m_debug, m_mmapPath, m_shmKey);
        }
        else
        {
            std::cout << "Failed to get mysql row" << std::endl;
            return;
        }
    }

    mysql_free_result(res);
}

int ZMServer::getFrame(FrameData &buffer, MONITOR *monitor)
{
    // is there a new frame available?
    if (monitor->getLastWriteIndex() == monitor->m_lastRead )
        return 0;

    // sanity check last_read
    if (monitor->getLastWriteIndex() < 0 ||
            monitor->getLastWriteIndex() >= (monitor->m_imageBufferCount - 1))
        return 0;

    monitor->m_lastRead = monitor->getLastWriteIndex();

    switch (monitor->getState())
    {
        case IDLE:
            monitor->m_status = "Idle";
            break;
        case PREALARM:
            monitor->m_status = "Pre Alarm";
            break;
        case ALARM:
            monitor->m_status = "Alarm";
            break;
        case ALERT:
            monitor->m_status = "Alert";
            break;
        case TAPE:
            monitor->m_status = "Tape";
            break;
        default:
            monitor->m_status = "Unknown";
            break;
    }

    // FIXME: should do some sort of compression JPEG??
    // just copy the data to our buffer for now

    // fixup the colours if necessary we aim to always send RGB24 images
    unsigned char *data = monitor->m_sharedImages +
        (static_cast<ptrdiff_t>(monitor->getFrameSize()) * monitor->m_lastRead);
    unsigned int rpos = 0;
    unsigned int wpos = 0;

    switch (monitor->getSubpixelOrder())
    {
        case ZM_SUBPIX_ORDER_NONE:
        {
            for (wpos = 0, rpos = 0; wpos < (unsigned int) (monitor->m_width * monitor->m_height * 3); wpos += 3, rpos += 1)
            {
                buffer[wpos + 0] = data[rpos + 0]; // r
                buffer[wpos + 1] = data[rpos + 0]; // g
                buffer[wpos + 2] = data[rpos + 0]; // b
            }

            break;
        }

        case ZM_SUBPIX_ORDER_RGB:
        {
            for (wpos = 0, rpos = 0; wpos < (unsigned int) (monitor->m_width * monitor->m_height * 3); wpos += 3, rpos += 3)
            {
                buffer[wpos + 0] = data[rpos + 0]; // r
                buffer[wpos + 1] = data[rpos + 1]; // g
                buffer[wpos + 2] = data[rpos + 2]; // b
            }

            break;
        }

        case ZM_SUBPIX_ORDER_BGR:
        {
            for (wpos = 0, rpos = 0; wpos < (unsigned int) (monitor->m_width * monitor->m_height * 3); wpos += 3, rpos += 3)
            {
                buffer[wpos + 0] = data[rpos + 2]; // r
                buffer[wpos + 1] = data[rpos + 1]; // g
                buffer[wpos + 2] = data[rpos + 0]; // b
            }

            break;
        }
        case ZM_SUBPIX_ORDER_BGRA:
        {
            for (wpos = 0, rpos = 0; wpos < (unsigned int) (monitor->m_width * monitor->m_height * 3); wpos += 3, rpos += 4)
            {
                buffer[wpos + 0] = data[rpos + 2]; // r
                buffer[wpos + 1] = data[rpos + 1]; // g
                buffer[wpos + 2] = data[rpos + 0]; // b
            }

            break;
        }

        case ZM_SUBPIX_ORDER_RGBA:
        {
            for (wpos = 0, rpos = 0; wpos < (unsigned int) (monitor->m_width * monitor->m_height * 3 ); wpos += 3, rpos += 4)
            {
                buffer[wpos + 0] = data[rpos + 0]; // r
                buffer[wpos + 1] = data[rpos + 1]; // g
                buffer[wpos + 2] = data[rpos + 2]; // b
            }

            break;
        }

        case ZM_SUBPIX_ORDER_ABGR:
        {
            for (wpos = 0, rpos = 0; wpos < (unsigned int) (monitor->m_width * monitor->m_height * 3); wpos += 3, rpos += 4)
            {
                buffer[wpos + 0] = data[rpos + 3]; // r
                buffer[wpos + 1] = data[rpos + 2]; // g
                buffer[wpos + 2] = data[rpos + 1]; // b
            }

            break;
        }

        case ZM_SUBPIX_ORDER_ARGB:
        {
            for (wpos = 0, rpos = 0; wpos < (unsigned int) (monitor->m_width * monitor->m_height * 3); wpos += 3, rpos += 4)
            {
                buffer[wpos + 0] = data[rpos + 1]; // r
                buffer[wpos + 1] = data[rpos + 2]; // g
                buffer[wpos + 2] = data[rpos + 3]; // b
            }

            break;
        }
    }

    return monitor->m_width * monitor->m_height * 3;
}

std::string ZMServer::getZMSetting(const std::string &setting) const
{
    std::string result;
    std::string sql("SELECT Name, Value FROM Config ");
    sql += "WHERE Name = '" + setting + "'";

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        return "";
    }

    MYSQL_RES *res = mysql_store_result(&g_dbConn);
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row)
    {
        result = row[1];
    }
    else
    {
        std::cout << "Failed to get mysql row" << std::endl;
        result = "";
    }

    if (m_debug)
        std::cout << "getZMSetting: " << setting << " Result: " << result << std::endl;

    mysql_free_result(res);

    return result;
}

void ZMServer::handleSetMonitorFunction(std::vector<std::string> tokens)
{
    std::string outStr;

    if (tokens.size() != 4)
    {
        sendError(ERROR_TOKEN_COUNT);
        return;
    }

    const std::string& monitorID(tokens[1]);
    const std::string& function(tokens[2]);
    const std::string& enabled(tokens[3]);

    // Check validity of input passed to server. Does monitor exist && is function ok
    if (m_monitorMap.find(atoi(monitorID.c_str())) == m_monitorMap.end())
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
        std::cout << "User input validated OK" << std::endl;


    // Now perform db update && (re)start/stop daemons as required.
    MONITOR *monitor = m_monitorMap[atoi(monitorID.c_str())];
    std::string oldFunction = monitor->m_function;
    const std::string& newFunction = function;
    int oldEnabled  = monitor->m_enabled;
    int newEnabled  = atoi(enabled.c_str());
    monitor->m_function = newFunction;
    monitor->m_enabled = newEnabled;

    if (m_debug)
    {
        std::cout << "SetMonitorFunction MonitorId: " << monitorID << std::endl
                  << "  oldEnabled: " << oldEnabled << std::endl
                  << "  newEnabled: " << newEnabled << std::endl
                  << " oldFunction: " << oldFunction << std::endl
                  << " newFunction: " << newFunction << std::endl;
    }

    if ( newFunction != oldFunction || newEnabled != oldEnabled)
    {
        std::string sql("UPDATE Monitors ");
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
            std::cout << "Monitor function SQL update OK" << std::endl;

        std::string status = runCommand(g_binPath + "/zmdc.pl check");

        // Now refresh servers
        if (RUNNING.compare(0, RUNNING.size(), status, 0, RUNNING.size()) == 0)
        {
            if (m_debug)
                std::cout << "Monitor function Refreshing daemons" << std::endl;

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
            {
                std::cout << "zm daemons are not running" << std::endl;
            }
    }
    else
    {
        std::cout << "Not updating monitor function as identical to existing configuration" << std::endl;
    }

    ADD_STR(outStr, "OK");
    send(outStr);
}

void ZMServer::zmcControl(MONITOR *monitor, const std::string &mode)
{
    std::string zmcArgs;
    std::string sql;
    sql += "SELECT count(if(Function!='None',1,NULL)) as ActiveCount ";
    sql += "FROM Monitors ";

    if (monitor->m_type == "Local" )
    {
        sql += "WHERE Device = '" + monitor->m_device + "'";
        zmcArgs = "-d " + monitor->m_device;
    }
    else
    {
        sql += "WHERE Id = '" + monitor->getIdStr() + "'";
        zmcArgs = "-m " + monitor->getIdStr();
    }

    if (mysql_query(&g_dbConn, sql.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(&g_dbConn));
        sendError(ERROR_MYSQL_QUERY);
        return;
    }

    MYSQL_RES *res = mysql_store_result(&g_dbConn);
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == nullptr)
    {
        sendError(ERROR_MYSQL_QUERY);
        return;
    }
    int activeCount = atoi(row[0]);

    if (!activeCount)
        runCommand(g_binPath + "/zmdc.pl stop zmc " + zmcArgs);
    else
    {
        if (mode == RESTART)
            runCommand(g_binPath + "/zmdc.pl stop zmc " + zmcArgs);

        runCommand(g_binPath + "/zmdc.pl start zmc " + zmcArgs);
    }
}

void ZMServer::zmaControl(MONITOR *monitor, const std::string &mode)
{
    int zmOptControl = atoi(getZMSetting("ZM_OPT_CONTROL").c_str());
    int zmOptFrameServer = atoi(getZMSetting("ZM_OPT_FRAME_SERVER").c_str());

    if (monitor->m_function == FUNCTION_MODECT ||
        monitor->m_function == FUNCTION_RECORD ||
        monitor->m_function == FUNCTION_MOCORD ||
        monitor->m_function == FUNCTION_NODECT)
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

        if (zmOptControl && monitor->m_controllable && monitor->m_trackMotion &&
            ( monitor->m_function == FUNCTION_MODECT || monitor->m_function == FUNCTION_MOCORD) )
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
