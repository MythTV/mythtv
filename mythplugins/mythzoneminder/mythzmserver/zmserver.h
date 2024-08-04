/* Definition of the ZMServer class.
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

#ifndef ZMSERVER_H
#define ZMSERVER_H

#include <chrono>
#include <cstdint>
#include <map>
#include <mysql/mysql.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
using namespace std::chrono_literals;
using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock>;

// the maximum image size we are ever likely to get from ZM
static constexpr size_t MAX_IMAGE_SIZE { static_cast<size_t>(2048) * 1536 * 3 };
using FrameData = std::array<uint8_t,MAX_IMAGE_SIZE>;

extern bool checkVersion(int major, int minor, int revision);
extern void loadZMConfig(const std::string &configfile);
extern void connectToDatabase(void);
extern void kickDatabase(bool debug);

// these are shared by all ZMServer's
extern MYSQL   g_dbConn;
extern std::string  g_zmversion;
extern std::string  g_password;
extern std::string  g_server;
extern std::string  g_database;
extern std::string  g_webPath;
extern std::string  g_user;
extern std::string  g_webUser;
extern std::string  g_binPath;
extern int     g_majorVersion;
extern int     g_minorVersion;
extern int     g_revisionVersion;

static constexpr std::chrono::seconds DB_CHECK_TIME { 60s };
extern TimePoint g_lastDBKick;

const std::string FUNCTION_MONITOR = "Monitor";
const std::string FUNCTION_MODECT  = "Modect";
const std::string FUNCTION_NODECT  = "Nodect";
const std::string FUNCTION_RECORD  = "Record";
const std::string FUNCTION_MOCORD  = "Mocord";
const std::string FUNCTION_NONE    = "None";

const std::string RESTART          = "restart";
const std::string RELOAD           = "reload";
const std::string RUNNING          = "running";

enum State : std::uint8_t
{
    IDLE,
    PREALARM,
    ALARM,
    ALERT,
    TAPE
};

// Prevent clang-tidy modernize-avoid-c-arrays warnings in these
// library structures
extern "C" {

// shared data for ZM version 1.24.x and 1.25.x
struct SharedData
{
    int size;
    bool valid;
    bool active;
    bool signal;
    State state;
    int last_write_index;
    int last_read_index;
    time_t last_write_time;
    time_t last_read_time;
    int last_event;
    int action;
    int brightness;
    int hue;
    int colour;
    int contrast;
    int alarm_x;
    int alarm_y;
    char control_state[256];
};

// shared data for ZM version 1.26.x
struct SharedData26
{
    uint32_t size;
    uint32_t last_write_index;
    uint32_t last_read_index;
    uint32_t state;
    uint32_t last_event;
    uint32_t action;
    int32_t brightness;
    int32_t hue;
    int32_t colour;
    int32_t contrast;
    int32_t alarm_x;
    int32_t alarm_y;
    uint8_t valid;
    uint8_t active;
    uint8_t signal;
    uint8_t format;
    uint32_t imagesize;
    uint32_t epadding1;
    uint32_t epadding2;
    union {
            time_t last_write_time;
            uint64_t extrapad1;
    };
    union {
            time_t last_read_time;
            uint64_t extrapad2;
    };
    uint8_t control_state[256];
};

// shared data for ZM version 1.32.x
struct SharedData32
{
    uint32_t size;
    uint32_t last_write_index;
    uint32_t last_read_index;
    uint32_t state;
    uint64_t last_event;
    uint32_t action;
    int32_t brightness;
    int32_t hue;
    int32_t colour;
    int32_t contrast;
    int32_t alarm_x;
    int32_t alarm_y;
    uint8_t valid;
    uint8_t active;
    uint8_t signal;
    uint8_t format;
    uint32_t imagesize;
    uint32_t epadding1;
     union {
      time_t startup_time;
      uint64_t extrapad1;
    };
    union {
      time_t last_write_time;
      uint64_t extrapad2;
    };
    union {
      time_t last_read_time;
      uint64_t extrapad3;
    };
    uint8_t control_state[256];

    char alarm_cause[256];
};

// shared data for ZM version 1.34.x
struct SharedData34
{
    uint32_t size;
    uint32_t last_write_index;
    uint32_t last_read_index;
    uint32_t state;
    uint64_t last_event;
    uint32_t action;
    int32_t brightness;
    int32_t hue;
    int32_t colour;
    int32_t contrast;
    int32_t alarm_x;
    int32_t alarm_y;
    uint8_t valid;
    uint8_t active;
    uint8_t signal;
    uint8_t format;
    uint32_t imagesize;
    uint32_t epadding1;
     union {
      time_t startup_time;
      uint64_t extrapad1;
    };
    union {                     /* +72   */
      time_t zmc_heartbeat_time;                        /* Constantly updated by zmc.  Used to determine if the process is alive or hung or dead */
      uint64_t extrapad2;
    };
    union {                     /* +80   */
      time_t zma_heartbeat_time;                        /* Constantly updated by zma.  Used to determine if the process is alive or hung or dead */
      uint64_t extrapad3;
    };
    union {
      time_t last_write_time;
      uint64_t extrapad4;
    };
    union {
      time_t last_read_time;
      uint64_t extrapad5;
    };
    uint8_t control_state[256];

    char alarm_cause[256];
};



enum TriggerState : std::uint8_t { TRIGGER_CANCEL, TRIGGER_ON, TRIGGER_OFF };

// Triggerdata for ZM version 1.24.x and 1.25.x
struct TriggerData
{
    int size;
    TriggerState trigger_state;
    int trigger_score;
    char trigger_cause[32];
    char trigger_text[256];
    char trigger_showtext[256];
};

// Triggerdata for ZM version 1.26.x , 1.32.x and 1.34.x
struct TriggerData26
{
    uint32_t size;
    uint32_t trigger_state;
    uint32_t trigger_score;
    uint32_t padding;
    char trigger_cause[32];
    char trigger_text[256];
    char trigger_showtext[256];
};

// VideoStoreData for ZM version 1.32.x and 1.34.x
struct VideoStoreData
{
    uint32_t size;
    uint64_t current_event;
    char event_file[4096];
    timeval recording;
};

// end of library structures.
};

class MONITOR
{
  public:
    MONITOR() = default;

    void initMonitor(bool debug, const std::string &mmapPath, int shmKey);

    bool isValid(void);

    std::string getIdStr(void);
    int getLastWriteIndex(void);
    int getSubpixelOrder(void);
    int getState(void);
    int getFrameSize(void);

    std::string    m_name;
    std::string    m_type;
    std::string    m_function;
    int            m_enabled            {0};
    std::string    m_device;
    std::string    m_host;
    int            m_imageBufferCount   {0};
    int            m_width              {0};
    int            m_height             {0};
    int            m_bytesPerPixel      {3};
    int            m_monId              {0};
    unsigned char *m_sharedImages       {nullptr};
    int            m_lastRead           {0};
    std::string    m_status;
    int            m_palette            {0};
    int            m_controllable       {0};
    int            m_trackMotion        {0};
    int            m_mapFile            {-1};
    void          *m_shmPtr             {nullptr};
  private:
    SharedData    *m_sharedData         {nullptr};
    SharedData26  *m_sharedData26       {nullptr};
    SharedData32  *m_sharedData32       {nullptr};
    SharedData34  *m_sharedData34       {nullptr};
    std::string    m_id;
};

class ZMServer
{
  public:
    ZMServer(int sock, bool debug);
    ~ZMServer();

    bool processRequest(char* buf, int nbytes);

  private:
    std::string getZMSetting(const std::string &setting) const;
    bool send(const std::string &s) const;
    bool send(const std::string &s, const unsigned char *buffer, int dataLen) const;
    void sendError(const std::string &error);
    void getMonitorList(void);
    static int  getFrame(FrameData &buffer, MONITOR *monitor);
    static long long getDiskSpace(const std::string &filename, long long &total, long long &used);
    static void tokenize(const std::string &command, std::vector<std::string> &tokens);
    void handleHello(void);
    static std::string runCommand(const std::string& command);
    static void getMonitorStatus(const std::string &id, const std::string &type,
                                 const std::string &device, const std::string &host,
                                 const std::string &channel, const std::string &function,
                                 std::string &zmcStatus, std::string &zmaStatus,
                                 const std::string &enabled);
    void handleGetServerStatus(void);
    void handleGetMonitorStatus(void);
    void handleGetAlarmStates(void);
    void handleGetMonitorList(void);
    void handleGetCameraList(void);
    void handleGetEventList(std::vector<std::string> tokens);
    void handleGetEventFrame(std::vector<std::string> tokens);
    void handleGetAnalysisFrame(std::vector<std::string> tokens);
    void handleGetLiveFrame(std::vector<std::string> tokens);
    void handleGetFrameList(std::vector<std::string> tokens);
    void handleDeleteEvent(std::vector<std::string> tokens);
    void handleDeleteEventList(std::vector<std::string> tokens);
    void handleGetEventDates(std::vector<std::string> tokens);
    void handleRunZMAudit(void);
    void handleSetMonitorFunction(std::vector<std::string> tokens);
    void zmcControl(MONITOR *monitor, const std::string &mode);
    void zmaControl(MONITOR *monitor, const std::string &mode);

    bool                 m_debug              {false};
    int                  m_sock               {-1};
    std::vector<MONITOR *> m_monitors;
    std::map<int, MONITOR *> m_monitorMap;
    bool                 m_useDeepStorage     {false};
    bool                 m_useAnalysisImages  {false};
    std::string          m_eventFileFormat;
    std::string          m_analysisFileFormat;
    key_t                m_shmKey;
    std::string          m_mmapPath;
};


#endif
