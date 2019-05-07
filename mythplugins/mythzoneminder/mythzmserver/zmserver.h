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

#include <cstdint>
#include <map>
#include <mysql/mysql.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;

extern bool checkVersion(int major, int minor, int revision);
extern void loadZMConfig(const string &configfile);
extern void connectToDatabase(void);
extern void kickDatabase(bool debug);

// these are shared by all ZMServer's
extern MYSQL   g_dbConn;
extern string  g_zmversion;
extern string  g_password;
extern string  g_server;
extern string  g_database;
extern string  g_webPath;
extern string  g_user;
extern string  g_webUser;
extern string  g_binPath;
extern int     g_majorVersion;
extern int     g_minorVersion;
extern int     g_revisionVersion;

#define DB_CHECK_TIME 60
extern time_t  g_lastDBKick;

const string FUNCTION_MONITOR = "Monitor";
const string FUNCTION_MODECT  = "Modect";
const string FUNCTION_NODECT  = "Nodect";
const string FUNCTION_RECORD  = "Record";
const string FUNCTION_MOCORD  = "Mocord";
const string FUNCTION_NONE    = "None";

const string RESTART          = "restart";
const string RELOAD           = "reload";
const string RUNNING          = "running";

typedef enum 
{
    IDLE,
    PREALARM,
    ALARM,
    ALERT,
    TAPE
} State;

// shared data for ZM version 1.24.x and 1.25.x
typedef struct
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
} SharedData;

// shared data for ZM version 1.26.x
typedef struct
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
} SharedData26;

// shared data for ZM version 1.32.x
typedef struct
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
} SharedData32;

typedef enum { TRIGGER_CANCEL, TRIGGER_ON, TRIGGER_OFF } TriggerState;

// Triggerdata for ZM version 1.24.x and 1.25.x
typedef struct
{
    int size;
    TriggerState trigger_state;
    int trigger_score;
    char trigger_cause[32];
    char trigger_text[256];
    char trigger_showtext[256];
} TriggerData;

// Triggerdata for ZM version 1.26.x and 1.32.x
typedef struct
{
    uint32_t size;
    uint32_t trigger_state;
    uint32_t trigger_score;
    uint32_t padding;
    char trigger_cause[32];
    char trigger_text[256];
    char trigger_showtext[256];
} TriggerData26;

// VideoStoreData for ZM version 1.32.x
typedef struct
{
    uint32_t size;
    uint64_t current_event;
    char event_file[4096];
    timeval recording;
} VideoStoreData;

class MONITOR
{
  public:
    MONITOR() = default;

    void initMonitor(bool debug, const string &mmapPath, int shmKey);

    bool isValid(void);

    string getIdStr(void);
    int getLastWriteIndex(void);
    int getSubpixelOrder(void);
    int getState(void);
    int getFrameSize(void);

    string         m_name               {""};
    string         m_type               {""};
    string         m_function           {""};
    int            m_enabled            {0};
    string         m_device             {""};
    string         m_host               {""};
    int            m_image_buffer_count {0};
    int            m_width              {0};
    int            m_height             {0};
    int            m_bytes_per_pixel    {3};
    int            m_mon_id             {0};
    unsigned char *m_shared_images      {nullptr};
    int            m_last_read          {0};
    string         m_status             {""};
    int            m_palette            {0};
    int            m_controllable       {0};
    int            m_trackMotion        {0};
    int            m_mapFile            {-1};
    void          *m_shm_ptr            {nullptr};
  private:
    SharedData    *m_shared_data        {nullptr};
    SharedData26  *m_shared_data26      {nullptr};
    SharedData32  *m_shared_data32      {nullptr};
    string         m_id                 {""};
};

class ZMServer
{
  public:
    ZMServer(int sock, bool debug);
    ~ZMServer();

    bool processRequest(char* buf, int nbytes);

  private:
    string getZMSetting(const string &setting);
    bool send(const string &s) const;
    bool send(const string &s, const unsigned char *buffer, int dataLen) const;
    void sendError(const string &error);
    void getMonitorList(void);
    int  getFrame(unsigned char *buffer, int bufferSize, MONITOR *monitor);
    long long getDiskSpace(const string &filename, long long &total, long long &used);
    void tokenize(const string &command, vector<string> &tokens);
    void handleHello(void);
    string runCommand(const string& command);
    void getMonitorStatus(const string &id, const string &type,
                          const string &device, const string &host,
                          const string &channel, const string &function,
                          string &zmcStatus, string &zmaStatus,
                          const string &enabled);
    void handleGetServerStatus(void);
    void handleGetMonitorStatus(void);
    void handleGetAlarmStates(void);
    void handleGetMonitorList(void);
    void handleGetCameraList(void);
    void handleGetEventList(vector<string> tokens);
    void handleGetEventFrame(vector<string> tokens);
    void handleGetAnalysisFrame(vector<string> tokens);
    void handleGetLiveFrame(vector<string> tokens);
    void handleGetFrameList(vector<string> tokens);
    void handleDeleteEvent(vector<string> tokens);
    void handleDeleteEventList(vector<string> tokens);
    void handleGetEventDates(vector<string> tokens);
    void handleRunZMAudit(void);
    void handleSetMonitorFunction(vector<string> tokens);
    void zmcControl(MONITOR *monitor, const string &mode);
    void zmaControl(MONITOR *monitor, const string &mode);

    bool                 m_debug;
    int                  m_sock;
    vector<MONITOR *>    m_monitors;
    map<int, MONITOR *>  m_monitorMap;
    bool                 m_useDeepStorage;
    bool                 m_useAnalysisImages;
    string               m_eventFileFormat;
    string               m_analysisFileFormat;
    key_t                m_shmKey;
    string               m_mmapPath;
    char                 m_buf[10];
};


#endif
