/*
 * ci.cc: Common Interface
 *
 * Copyright (C) 2000 Klaus Schmidinger
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 * The author can be reached at kls@cadsoft.de
 *
 * The project's page is at http://www.cadsoft.de/people/kls/vdr
 *
 */

#include "dvbci.h"

#include <array>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <linux/dvb/ca.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef __FreeBSD__
#  include <stdlib.h>
#else
#  include <malloc.h>
#endif

#include <QString>

#include "libmythbase/mythlogging.h"

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define esyslog(a...) LOG(VB_GENERAL, LOG_ERR, QString::asprintf(a))
#define isyslog(a...) LOG(VB_DVBCAM, LOG_INFO, QString::asprintf(a))
#define dsyslog(a...) LOG(VB_DVBCAM, LOG_DEBUG, QString::asprintf(a))

#define LOG_ERROR         esyslog("ERROR (%s,%d): %m", __FILE__, __LINE__)
#define LOG_ERROR_STR(s)  esyslog("ERROR: %s: %m", s)
// NOLINTEND(cppcoreguidelines-macro-usage)


// Set these to 'true' for debug output:
static bool sDumpTPDUDataTransfer = false;
static bool sDebugProtocol = false;
static bool sConnected = false;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define dbgprotocol(a...) if (sDebugProtocol) LOG(VB_DVBCAM, LOG_DEBUG, QString::asprintf(a))

static constexpr int OK       {  0 };
static constexpr int TIMEOUT  { -1 };
static constexpr int ERROR    { -2 };

// --- Workarounds -----------------------------------------------------------

// The Irdeto AllCAM 4.7 (and maybe others, too) does not react on AOT_ENTER_MENU
// during the first few seconds of a newly established connection
static constexpr time_t WRKRND_TIME_BEFORE_ENTER_MENU { 15 }; // seconds

// --- Helper functions ------------------------------------------------------

static constexpr int SIZE_INDICATOR { 0x80 };

static ssize_t safe_read(int filedes, void *buffer, size_t size)
{
  for (;;) {
      ssize_t p = read(filedes, buffer, size);
      if (p < 0 && (errno == EINTR || errno == EAGAIN)) {
         dsyslog("EINTR while reading from file handle %d - retrying", filedes);
         continue;
         }
      return p;
      }
}

static const uint8_t *GetLength(const uint8_t *Data, int &Length)
///< Gets the length field from the beginning of Data.  This number is
///  encoded into the output buffer. If the high order bit of the
///  first byte is zero, then the remaining seven bits hold the actual
///  length.  If the high order bit is set then the remaining bits of
///  that byte indicate how many bytes are used to hold the length.
///  The subsequent bytes hold the actual length value.
///< \param Data A pointer to current location for reading data.
///< \param Length Used to store the length from the data stream.
///< \return Returns a pointer to the first byte after the length and
///< stores the length value in Length.
{
  Length = *Data++;
  if ((Length & SIZE_INDICATOR) != 0) {
     int l = Length & ~SIZE_INDICATOR;
     Length = 0;
     for (int i = 0; i < l; i++)
         Length = (Length << 8) | *Data++;
     }
  return Data;
}

static uint8_t *SetLength(uint8_t *Data, int Length)
///< Sets the length field at the beginning of Data.  This number is
///  encoded into the output buffer. If the length is less than 128,
///  it is written directly into the first byte.  If 128 or more, the
///  high order bit of the first byte is set and the remaining bits
///  indicate how many bytes are needed to hold the length.  The
///  subsequent bytes hold the actual length.
///< \param Data A pointer to current location for writing data.
///< \param Length A number to encode into the data stream.
///< \return Returns a pointer to the first byte after the length.
{
  uint8_t *p = Data;
  if (Length < 128)
     *p++ = Length;
  else {
     int n = sizeof(Length);
     for (int i = n - 1; i >= 0; i--) {
         int b = (Length >> (8 * i)) & 0xFF;
         if (p != Data || b)
            *++p = b;
         }
     *Data = (p - Data) | SIZE_INDICATOR;
     p++;
     }
  return p;
}

//! @copydoc SetLength(uint8_t *Data, int Length)
static void SetLength(std::vector<uint8_t> &Data, int Length)
{
  if (Length < 128)
  {
      Data.push_back(Length);
      return;
  }

  // This will be replaced with the number of bytes in the length
  size_t len_offset = Data.size();
  Data.push_back(0);

  int n = sizeof(Length);
  for (int i = n - 1; i >= 0; i--) {
      int b = (Length >> (8 * i)) & 0xFF;
      if ((len_offset != Data.size()) || b)
          Data.push_back(b);
  }
  Data[len_offset] = (Data.size() - len_offset) | SIZE_INDICATOR;
}

static char *CopyString(int Length, const uint8_t *Data)
///< Copies the string at Data.
///< \param Length The number of bytes to copy from Data.
///< \param Data A pointer to current location for reading data.
///< \return Returns a pointer to a newly allocated string.
{
  char *s = (char *)malloc(Length + 1);
  strncpy(s, (char *)Data, Length);
  s[Length] = 0;
  return s;
}

static char *GetString(int &Length, const uint8_t **Data)
///< Gets the string at Data.
///< Upon return Length and Data represent the remaining data after the string has been copied off.
///< \param[in,out] Length The number of bytes to copy from Data. Updated for
///                 the size of the string read.
///< \param[in,out] Data A pointer to current location for reading data.
///                 Updated for the size of the string read.
///< \return Returns a pointer to a newly allocated string, or nullptr in case of error.
{
  if (Length > 0 && Data && *Data) {
     int l = 0;
     const uint8_t *d = GetLength(*Data, l);
     char *s = CopyString(l, d);
     Length -= d - *Data + l;
     *Data = d + l;
     return s;
     }
  return nullptr;
}



// --- cMutex ----------------------------------------------------------------

void cMutex::Lock(void)
{
  if (getpid() != m_lockingPid || !m_locked) {
     pthread_mutex_lock(&m_mutex);
     m_lockingPid = getpid();
     }
  m_locked++;
}

void cMutex::Unlock(void)
{
 if (--m_locked <= 0) {
    if (m_locked < 0) {
        esyslog("cMutex Lock inbalance detected");
        m_locked = 0;
        }
    m_lockingPid = 0;
    pthread_mutex_unlock(&m_mutex);
    }
}
// --- cMutexLock ------------------------------------------------------------

cMutexLock::~cMutexLock()
{
  if (m_mutex && m_locked)
     m_mutex->Unlock();
}

bool cMutexLock::Lock(cMutex *Mutex)
{
  if (Mutex && !m_mutex) {
     m_mutex = Mutex;
     Mutex->Lock();
     m_locked = true;
     return true;
     }
  return false;
}



// --- cTPDU -----------------------------------------------------------------

static constexpr size_t  MAX_TPDU_SIZE  { 2048 };
static constexpr int     MAX_TPDU_DATA  { MAX_TPDU_SIZE - 4 };

static constexpr uint8_t DATA_INDICATOR { 0x80 };

enum T_VALUES : std::uint8_t {
    T_SB             = 0x80,
    T_RCV            = 0x81,
    T_CREATE_TC      = 0x82,
    T_CTC_REPLY      = 0x83,
    T_DELETE_TC      = 0x84,
    T_DTC_REPLY      = 0x85,
    T_REQUEST_TC     = 0x86,
    T_NEW_TC         = 0x87,
    T_TC_ERROR       = 0x88,
    T_DATA_LAST      = 0xA0,
    T_DATA_MORE      = 0xA1,
};

class cTPDU {
private:
  ssize_t m_size {0};
  std::array<uint8_t,MAX_TPDU_SIZE>  m_data {0};
  const uint8_t *GetData(const uint8_t *Data, int &Length) const;
public:
  cTPDU(void) = default;
  cTPDU(uint8_t Slot, uint8_t Tcid, uint8_t Tag, int Length = 0, const uint8_t *Data = nullptr);
  uint8_t Slot(void) { return m_data[0]; }
  uint8_t Tcid(void) { return m_data[1]; }
  uint8_t Tag(void)  { return m_data[2]; }
  const uint8_t *Data(int &Length) { return GetData(m_data.data() + 3, Length); }
  uint8_t Status(void);
  int Write(int fd);
  int Read(int fd);
  void Dump(bool Outgoing);
  };

cTPDU::cTPDU(uint8_t Slot, uint8_t Tcid, uint8_t Tag, int Length, const uint8_t *Data)
{
  m_data[0] = Slot;
  m_data[1] = Tcid;
  m_data[2] = Tag;
  switch (Tag) {
    case T_RCV:
    case T_CREATE_TC:
    case T_CTC_REPLY:
    case T_DELETE_TC:
    case T_DTC_REPLY:
    case T_REQUEST_TC:
         m_data[3] = 1; // length
         m_data[4] = Tcid;
         m_size = 5;
         break;
    case T_NEW_TC:
    case T_TC_ERROR:
         if (Length == 1) {
            m_data[3] = 2; // length
            m_data[4] = Tcid;
            m_data[5] = Data[0];
            m_size = 6;
            }
         else {
            esyslog("ERROR: illegal data length for TPDU tag 0x%02X: %d", Tag, Length);
         }
         break;
    case T_DATA_LAST:
    case T_DATA_MORE:
         if (Length <= MAX_TPDU_DATA) {
            uint8_t *p = m_data.data() + 3;
            p = SetLength(p, Length + 1);
            *p++ = Tcid;
            if (Length)
               memcpy(p, Data, Length);
            m_size = Length + (p - m_data.data());
            }
         else {
            esyslog("ERROR: illegal data length for TPDU tag 0x%02X: %d", Tag, Length);
         }
         break;
    default:
         esyslog("ERROR: unknown TPDU tag: 0x%02X", Tag);
    }
 }

int cTPDU::Write(int fd)
{
  Dump(true);
  if (m_size)
     return write(fd, m_data.data(), m_size) == m_size ? OK : ERROR;
  esyslog("ERROR: attemp to write TPDU with zero size");
  return ERROR;
}

int cTPDU::Read(int fd)
{
  m_size = safe_read(fd, m_data.data(), m_data.size());
  if (m_size < 0) {
     esyslog("ERROR: %m");
     m_size = 0;
     return ERROR;
     }
  Dump(false);
  return OK;
}

void cTPDU::Dump(bool Outgoing)
{
  if (sDumpTPDUDataTransfer) {
     static constexpr ssize_t MAX_DUMP { 256 };
     QString msg = QString("%1 ").arg(Outgoing ? "-->" : "<--");
     for (int i = 0; i < m_size && i < MAX_DUMP; i++)
         msg += QString("%1 ").arg((short int)m_data[i], 2, 16, QChar('0'));
     if (m_size >= MAX_DUMP)
         msg += "...";
     LOG(VB_DVBCAM, LOG_INFO, msg);
     if (!Outgoing) {
        msg = QString("   ");
        for (int i = 0; i < m_size && i < MAX_DUMP; i++)
            msg += QString("%1 ").arg(isprint(m_data[i]) ? m_data[i] : '.', 2);
        if (m_size >= MAX_DUMP)
            msg += "...";
        LOG(VB_DVBCAM, LOG_INFO, msg);
        }
     }
}

const uint8_t *cTPDU::GetData(const uint8_t *Data, int &Length) const
{
  if (m_size) {
     Data = GetLength(Data, Length);
     if (Length) {
        Length--; // the first byte is always the tcid
        return Data + 1;
        }
     }
  return nullptr;
}

uint8_t cTPDU::Status(void)
{
  if (m_size >= 4 && m_data[m_size - 4] == T_SB && m_data[m_size - 3] == 2) {
     //XXX test tcid???
     return m_data[m_size - 1];
     }
  return 0;
}

// --- cCiTransportConnection ------------------------------------------------

enum eState : std::uint8_t { stIDLE, stCREATION, stACTIVE, stDELETION };

class cCiTransportConnection {
  friend class cCiTransportLayer;
private:
  int             m_fd            {-1};
  uint8_t         m_slot          {0};
  uint8_t         m_tcid          {0};
  eState          m_state         {stIDLE};
  cTPDU          *m_tpdu          {nullptr};
  std::chrono::milliseconds  m_lastPoll {0ms};
  int             m_lastResponse  {ERROR};
  bool            m_dataAvailable {false};
  void Init(int Fd, uint8_t Slot, uint8_t Tcid);
  int SendTPDU(uint8_t Tag, int Length = 0, const uint8_t *Data = nullptr) const;
  int RecvTPDU(void);
  int CreateConnection(void);
  int Poll(void);
  eState State(void) { return m_state; }
  int LastResponse(void) const { return m_lastResponse; }
  bool DataAvailable(void) const { return m_dataAvailable; }
public:
  cCiTransportConnection(void);
  ~cCiTransportConnection();
  int Slot(void) const { return m_slot; }
  int SendData(int Length, const uint8_t *Data);
  int SendData(std::vector<uint8_t> &Data)
        { return SendData(Data.size(), Data.data()); }
  int RecvData(void);
  const uint8_t *Data(int &Length);
  //XXX Close()
  };

cCiTransportConnection::cCiTransportConnection(void)
{
  Init(-1, 0, 0);
}

cCiTransportConnection::~cCiTransportConnection()
{
  delete m_tpdu;
}

void cCiTransportConnection::Init(int Fd, uint8_t Slot, uint8_t Tcid)
{
  m_fd = Fd;
  m_slot = Slot;
  m_tcid = Tcid;
  m_state = stIDLE;
  if (m_fd >= 0 && !m_tpdu)
     m_tpdu = new cTPDU;
  m_lastResponse = ERROR;
  m_dataAvailable = false;
//XXX Clear()???
}

int cCiTransportConnection::SendTPDU(uint8_t Tag, int Length, const uint8_t *Data) const
{
  cTPDU TPDU(m_slot, m_tcid, Tag, Length, Data);
  return TPDU.Write(m_fd);
}

static constexpr int CAM_READ_TIMEOUT { 5000 }; // ms

int cCiTransportConnection::RecvTPDU(void)
{
  std::array<struct pollfd,1> pfd {};
  pfd[0].fd = m_fd;
  pfd[0].events = POLLIN;
  m_lastResponse = ERROR;

  for (;;) {
      int ret = poll(pfd.data(), 1, CAM_READ_TIMEOUT);
      if (ret == -1 && (errno == EAGAIN || errno == EINTR))
          continue;
      break;
  }

  if (
        (pfd[0].revents & POLLIN) &&
        m_tpdu->Read(m_fd) == OK &&
        m_tpdu->Tcid() == m_tcid
     )
  {
     switch (m_state) {
       case stIDLE:     break;
       case stCREATION: if (m_tpdu->Tag() == T_CTC_REPLY) {
                           m_dataAvailable = ((m_tpdu->Status() & DATA_INDICATOR) != 0);
                           m_state = stACTIVE;
                           m_lastResponse = m_tpdu->Tag();
                           }
                        break;
       case stACTIVE:   switch (m_tpdu->Tag()) {
                          case T_SB:
                          case T_DATA_LAST:
                          case T_DATA_MORE:
                          case T_REQUEST_TC: break;
                          case T_DELETE_TC:  if (SendTPDU(T_DTC_REPLY) != OK)
                                                return ERROR;
                                             Init(m_fd, m_slot, m_tcid);
                                             break;
                          default: return ERROR;
                          }
                        m_dataAvailable = ((m_tpdu->Status() & DATA_INDICATOR) != 0);
                        m_lastResponse = m_tpdu->Tag();
                        break;
       case stDELETION: if (m_tpdu->Tag() == T_DTC_REPLY) {
                           Init(m_fd, m_slot, m_tcid);
                           //XXX Status()???
                           m_lastResponse = m_tpdu->Tag();
                           }
                        break;
       }
     }
  else {
     esyslog("ERROR: CAM: Read failed: slot %d, tcid %d\n", m_slot, m_tcid);
     if (m_tpdu->Tcid() == m_tcid)
        Init(-1, m_slot, m_tcid);
     }
  return m_lastResponse;
}

int cCiTransportConnection::SendData(int Length, const uint8_t *Data)
{
  while (m_state == stACTIVE && Length > 0) {
        uint8_t Tag = T_DATA_LAST;
        int l = Length;
        if (l > MAX_TPDU_DATA) {
           Tag = T_DATA_MORE;
           l = MAX_TPDU_DATA;
           }
        if (SendTPDU(Tag, l, Data) != OK || RecvTPDU() != T_SB)
           break;
        Length -= l;
        Data += l;
        }
  return Length ? ERROR : OK;
}

int cCiTransportConnection::RecvData(void)
{
  if (SendTPDU(T_RCV) == OK)
     return RecvTPDU();
  return ERROR;
}

const uint8_t *cCiTransportConnection::Data(int &Length)
{
  return m_tpdu->Data(Length);
}

static constexpr int8_t MAX_CONNECT_RETRIES { 25 };

int cCiTransportConnection::CreateConnection(void)
{
  if (m_state == stIDLE) {
     if (SendTPDU(T_CREATE_TC) == OK) {
        m_state = stCREATION;
        if (RecvTPDU() == T_CTC_REPLY) {
           sConnected=true;
           return OK;
        // the following is a workaround for CAMs that don't quite follow the specs...
        }

        for (int i = 0; i < MAX_CONNECT_RETRIES; i++) {
            dsyslog("CAM: retrying to establish connection");
            if (RecvTPDU() == T_CTC_REPLY) {
                dsyslog("CAM: connection established");
                sConnected=true;
                return OK;
            }
        }
        return ERROR;
     }
  }
  return ERROR;
}

// Polls can be done with a 100ms interval (EN50221 - A.4.1.12)
static constexpr std::chrono::milliseconds POLL_INTERVAL { 100ms };

int cCiTransportConnection::Poll(void)
{
  if (m_state != stACTIVE)
    return ERROR;

  auto curr_time = nowAsDuration<std::chrono::milliseconds>();
  std::chrono::milliseconds msdiff = curr_time - m_lastPoll;

  if (msdiff < POLL_INTERVAL)
    return OK;

  m_lastPoll = curr_time;

  if (SendTPDU(T_DATA_LAST) != OK)
    return ERROR;

  return RecvTPDU();
}

// --- cCiTransportLayer -----------------------------------------------------

static constexpr size_t MAX_CI_CONNECT { 16 }; // maximum possible value is 254

class cCiTransportLayer {
private:
  int                    m_fd;
  int                    m_numSlots;
  std::array<cCiTransportConnection,MAX_CI_CONNECT> m_tc;
public:
  cCiTransportLayer(int Fd, int NumSlots);
  cCiTransportConnection *NewConnection(int Slot);
  bool ResetSlot(int Slot) const;
  bool ModuleReady(int Slot) const;
  cCiTransportConnection *Process(int Slot);
  };

cCiTransportLayer::cCiTransportLayer(int Fd, int NumSlots)
  : m_fd(Fd),
    m_numSlots(NumSlots)
{
  for (int s = 0; s < m_numSlots; s++)
      ResetSlot(s);
}

cCiTransportConnection *cCiTransportLayer::NewConnection(int Slot)
{
  for (size_t i = 0; i < MAX_CI_CONNECT; i++) {
      if (m_tc[i].State() == stIDLE) {
         dbgprotocol("Creating connection: slot %d, tcid %zd\n", Slot, i + 1);
         m_tc[i].Init(m_fd, Slot, i + 1);
         if (m_tc[i].CreateConnection() == OK)
            return &m_tc[i];
         break;
         }
      }
  return nullptr;
}

bool cCiTransportLayer::ResetSlot(int Slot) const
{
  dbgprotocol("Resetting slot %d...", Slot);
  if (ioctl(m_fd, CA_RESET, 1 << Slot) != -1) {
     dbgprotocol("ok.\n");
     return true;
     }
  esyslog("ERROR: can't reset CAM slot %d: %m", Slot);
  dbgprotocol("failed!\n");
  return false;
}

bool cCiTransportLayer::ModuleReady(int Slot) const
{
  ca_slot_info_t sinfo;
  sinfo.num = Slot;
  if (ioctl(m_fd, CA_GET_SLOT_INFO, &sinfo) != -1)
     return (sinfo.flags & CA_CI_MODULE_READY) != 0U;
  esyslog("ERROR: can't get info on CAM slot %d: %m", Slot);
  return false;
}

cCiTransportConnection *cCiTransportLayer::Process(int Slot)
{
  for (auto & conn : m_tc) {
      cCiTransportConnection *Tc = &conn;
      if (Tc->Slot() == Slot) {
         switch (Tc->State()) {
           case stCREATION:
           case stACTIVE:
                if (!Tc->DataAvailable()) {
                    Tc->Poll();
                   }
                switch (Tc->LastResponse()) {
                  case T_REQUEST_TC:
                       //XXX
                       break;
                  case T_DATA_MORE:
                  case T_DATA_LAST:
                  case T_CTC_REPLY:
                  case T_SB:
                       if (Tc->DataAvailable())
                          Tc->RecvData();
                       break;
                  case TIMEOUT:
                  case ERROR:
                  default:
                       //XXX Tc->state = stIDLE;//XXX Init()???
                       return nullptr;
                       break;
                  }
                //XXX this will only work with _one_ transport connection per slot!
                return Tc;
                break;
           default: ;
           }
         }
      }
  return nullptr;
}

// -- cCiSession -------------------------------------------------------------

// Session Tags:

enum SESSION_TAGS : std::uint8_t {
    ST_SESSION_NUMBER             = 0x90,
    ST_OPEN_SESSION_REQUEST       = 0x91,
    ST_OPEN_SESSION_RESPONSE      = 0x92,
    ST_CREATE_SESSION             = 0x93,
    ST_CREATE_SESSION_RESPONSE    = 0x94,
    ST_CLOSE_SESSION_REQUEST      = 0x95,
    ST_CLOSE_SESSION_RESPONSE     = 0x96,
};

// Session Status:

enum SESSION_STATUS : std::uint8_t {
    SS_OK               = 0x00,
    SS_NOT_ALLOCATED    = 0xF0,
};

// Resource Identifiers:

enum IDENTIFIERS {
    RI_RESOURCE_MANAGER              = 0x00010041,
    RI_APPLICATION_INFORMATION       = 0x00020041,
    RI_CONDITIONAL_ACCESS_SUPPORT    = 0x00030041,
    RI_HOST_CONTROL                  = 0x00200041,
    RI_DATE_TIME                     = 0x00240041,
    RI_MMI                           = 0x00400041,
};

// Application Object Tags:

enum OBJECT_TAG {
    AOT_NONE                      = 0x000000,
    AOT_PROFILE_ENQ               = 0x9F8010,
    AOT_PROFILE                   = 0x9F8011,
    AOT_PROFILE_CHANGE            = 0x9F8012,
    AOT_APPLICATION_INFO_ENQ      = 0x9F8020,
    AOT_APPLICATION_INFO          = 0x9F8021,
    AOT_ENTER_MENU                = 0x9F8022,
    AOT_CA_INFO_ENQ               = 0x9F8030,
    AOT_CA_INFO                   = 0x9F8031,
    AOT_CA_PMT                    = 0x9F8032,
    AOT_CA_PMT_REPLY              = 0x9F8033,
    AOT_TUNE                      = 0x9F8400,
    AOT_REPLACE                   = 0x9F8401,
    AOT_CLEAR_REPLACE             = 0x9F8402,
    AOT_ASK_RELEASE               = 0x9F8403,
    AOT_DATE_TIME_ENQ             = 0x9F8440,
    AOT_DATE_TIME                 = 0x9F8441,
    AOT_CLOSE_MMI                 = 0x9F8800,
    AOT_DISPLAY_CONTROL           = 0x9F8801,
    AOT_DISPLAY_REPLY             = 0x9F8802,
    AOT_TEXT_LAST                 = 0x9F8803,
    AOT_TEXT_MORE                 = 0x9F8804,
    AOT_KEYPAD_CONTROL            = 0x9F8805,
    AOT_KEYPRESS                  = 0x9F8806,
    AOT_ENQ                       = 0x9F8807,
    AOT_ANSW                      = 0x9F8808,
    AOT_MENU_LAST                 = 0x9F8809,
    AOT_MENU_MORE                 = 0x9F880A,
    AOT_MENU_ANSW                 = 0x9F880B,
    AOT_LIST_LAST                 = 0x9F880C,
    AOT_LIST_MORE                 = 0x9F880D,
    AOT_SUBTITLE_SEGMENT_LAST     = 0x9F880E,
    AOT_SUBTITLE_SEGMENT_MORE     = 0x9F880F,
    AOT_DISPLAY_MESSAGE           = 0x9F8810,
    AOT_SCENE_END_MARK            = 0x9F8811,
    AOT_SCENE_DONE                = 0x9F8812,
    AOT_SCENE_CONTROL             = 0x9F8813,
    AOT_SUBTITLE_DOWNLOAD_LAST    = 0x9F8814,
    AOT_SUBTITLE_DOWNLOAD_MORE    = 0x9F8815,
    AOT_FLUSH_DOWNLOAD            = 0x9F8816,
    AOT_DOWNLOAD_REPLY            = 0x9F8817,
    AOT_COMMS_CMD                 = 0x9F8C00,
    AOT_CONNECTION_DESCRIPTOR     = 0x9F8C01,
    AOT_COMMS_REPLY               = 0x9F8C02,
    AOT_COMMS_SEND_LAST           = 0x9F8C03,
    AOT_COMMS_SEND_MORE           = 0x9F8C04,
    AOT_COMMS_RCV_LAST            = 0x9F8C05,
    AOT_COMMS_RCV_MORE            = 0x9F8C06,
};

class cCiSession {
private:
  int m_sessionId;
  int m_resourceId;
  cCiTransportConnection *m_tc;
protected:
  static int GetTag(int &Length, const uint8_t **Data);
  static const uint8_t *GetData(const uint8_t *Data, int &Length);
  int SendData(int Tag, int Length = 0, const uint8_t *Data = nullptr);
    int SendData(int Tag, std::vector<uint8_t> &Data)
        { return SendData(Tag, Data.size(), Data.data()); };
public:
  cCiSession(int SessionId, int ResourceId, cCiTransportConnection *Tc);
  virtual ~cCiSession() = default;
  const cCiTransportConnection *Tc(void) { return m_tc; }
  int SessionId(void) const { return m_sessionId; }
  int ResourceId(void) const { return m_resourceId; }
  virtual bool HasUserIO(void) { return false; }
  virtual bool Process(int Length = 0, const uint8_t *Data = nullptr);
  };

cCiSession::cCiSession(int SessionId, int ResourceId, cCiTransportConnection *Tc)
  : m_sessionId(SessionId),
    m_resourceId(ResourceId),
    m_tc(Tc)
{
}

int cCiSession::GetTag(int &Length, const uint8_t **Data)
///< Gets the tag at Data.
///< \param[in,out] Length The number of bytes to copy from Data. Updated for
///                 the size of the string read.
///< \param[in,out] Data A pointer to current location for reading data.
///                 Updated for the size of the string read.
///< \return Returns the actual tag, or AOT_NONE in case of error.
///< Upon return Length and Data represent the remaining data after the tag has been skipped.
{
  if (Length >= 3 && Data && *Data) {
     int t = 0;
     for (int i = 0; i < 3; i++)
         t = (t << 8) | *(*Data)++;
     Length -= 3;
     return t;
     }
  return AOT_NONE;
}

const uint8_t *cCiSession::GetData(const uint8_t *Data, int &Length)
{
  Data = GetLength(Data, Length);
  return Length ? Data : nullptr;
}

int cCiSession::SendData(int Tag, int Length, const uint8_t *Data)
{
  if (Length < 0)
  {
    esyslog("ERROR: CAM: data length (%d) is negative", Length);
    return ERROR;
  }

  if ((Length > 0) && !Data)
  {
    esyslog("ERROR: CAM: Data pointer null");
    return ERROR;
  }

  std::vector<uint8_t> buffer {
    ST_SESSION_NUMBER, 0x02,
    static_cast<uint8_t>((m_sessionId >> 8) & 0xFF),
    static_cast<uint8_t>((m_sessionId     ) & 0xFF),
    static_cast<uint8_t>((Tag >> 16) & 0xFF),
    static_cast<uint8_t>((Tag >>  8) & 0xFF),
    static_cast<uint8_t>((Tag      ) & 0xFF)} ;
  buffer.reserve(2048);

  SetLength(buffer, Length);
  if (buffer.size() + Length >= buffer.capacity())
  {
    esyslog("ERROR: CAM: data length (%d) exceeds buffer size", Length);
    return ERROR;
  }

  if (Length != 0)
  {
    buffer.insert(buffer.end(), Data, Data + Length);
  }
  return m_tc->SendData(buffer);
}

bool cCiSession::Process([[maybe_unused]] int Length,
                         [[maybe_unused]] const uint8_t *Data)
{
  return true;
}

// -- cCiResourceManager -----------------------------------------------------

class cCiResourceManager : public cCiSession {
private:
  int m_state;
public:
  cCiResourceManager(int SessionId, cCiTransportConnection *Tc);
  bool Process(int Length = 0, const uint8_t *Data = nullptr) override; // cCiSession
  };

cCiResourceManager::cCiResourceManager(int SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_RESOURCE_MANAGER, Tc)
{
  dbgprotocol("New Resource Manager (session id %d)\n", SessionId);
  m_state = 0;
}

bool cCiResourceManager::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_PROFILE_ENQ: {
            dbgprotocol("%d: <== Profile Enquiry\n", SessionId());
            const std::array<const uint32_t,5> resources
            {
                htonl(RI_RESOURCE_MANAGER),
                htonl(RI_APPLICATION_INFORMATION),
                htonl(RI_CONDITIONAL_ACCESS_SUPPORT),
                htonl(RI_DATE_TIME),
                htonl(RI_MMI)
            };
            dbgprotocol("%d: ==> Profile\n", SessionId());
            SendData(AOT_PROFILE, resources.size() * sizeof(uint32_t),
                     reinterpret_cast<const uint8_t*>(resources.data()));
            m_state = 3;
            }
            break;
       case AOT_PROFILE: {
            dbgprotocol("%d: <== Profile\n", SessionId());
            if (m_state == 1) {
               int l = 0;
               const uint8_t *d = GetData(Data, l);
               if (l > 0 && d)
                  esyslog("CI resource manager: unexpected data");
               dbgprotocol("%d: ==> Profile Change\n", SessionId());
               SendData(AOT_PROFILE_CHANGE);
               m_state = 2;
               }
            else {
               esyslog("ERROR: CI resource manager: unexpected tag %06X in state %d", Tag, m_state);
               }
            }
            break;
       default: esyslog("ERROR: CI resource manager: unknown tag %06X", Tag);
                return false;
       }
     }
  else if (m_state == 0) {
     dbgprotocol("%d: ==> Profile Enq\n", SessionId());
     SendData(AOT_PROFILE_ENQ);
     m_state = 1;
     }
  return true;
}

// --- cCiApplicationInformation ---------------------------------------------

class cCiApplicationInformation : public cCiSession {
private:
  int       m_state;
  time_t    m_creationTime;
  uint8_t   m_applicationType;
  uint16_t  m_applicationManufacturer;
  uint16_t  m_manufacturerCode;
  char     *m_menuString;
public:
  cCiApplicationInformation(int SessionId, cCiTransportConnection *Tc);
  ~cCiApplicationInformation() override;
  bool Process(int Length = 0, const uint8_t *Data = nullptr) override; // cCiSession
  bool EnterMenu(void);
  char    *GetApplicationString()       { return strdup(m_menuString); };
  uint16_t GetApplicationManufacturer() const { return m_applicationManufacturer; };
  uint16_t GetManufacturerCode() const        { return m_manufacturerCode; };
  };

cCiApplicationInformation::cCiApplicationInformation(int SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_APPLICATION_INFORMATION, Tc)
{
  dbgprotocol("New Application Information (session id %d)\n", SessionId);
  m_state = 0;
  m_creationTime = time(nullptr);
  m_applicationType = 0;
  m_applicationManufacturer = 0;
  m_manufacturerCode = 0;
  m_menuString = nullptr;
}

cCiApplicationInformation::~cCiApplicationInformation()
{
  free(m_menuString);
}

bool cCiApplicationInformation::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_APPLICATION_INFO: {
            dbgprotocol("%d: <== Application Info\n", SessionId());
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            l -= 1;
            if (l < 0) break;
            m_applicationType = *d++;
            l -= 2;
            if (l < 0) break;
            m_applicationManufacturer = ntohs(*(uint16_t *)d);
            d += 2;
            l -= 2;
            if (l < 0) break;
            m_manufacturerCode = ntohs(*(uint16_t *)d);
            d += 2;
            free(m_menuString);
            m_menuString = GetString(l, &d);
            isyslog("CAM: %s, %02X, %04X, %04X", m_menuString, m_applicationType,
                            m_applicationManufacturer, m_manufacturerCode);
            }
            m_state = 2;
            break;
       default: esyslog("ERROR: CI application information: unknown tag %06X", Tag);
                return false;
       }
     }
  else if (m_state == 0) {
     dbgprotocol("%d: ==> Application Info Enq\n", SessionId());
     SendData(AOT_APPLICATION_INFO_ENQ);
     m_state = 1;
     }
  return true;
}

bool cCiApplicationInformation::EnterMenu(void)
{
  if (m_state == 2 && time(nullptr) - m_creationTime > WRKRND_TIME_BEFORE_ENTER_MENU) {
     dbgprotocol("%d: ==> Enter Menu\n", SessionId());
     SendData(AOT_ENTER_MENU);
     return true;//XXX
     }
  return false;
}

// --- cCiConditionalAccessSupport -------------------------------------------

class cCiConditionalAccessSupport : public cCiSession {
private:
  int m_state {0};
  dvbca_vector m_caSystemIds;
  bool m_needCaPmt {false};
public:
  cCiConditionalAccessSupport(int SessionId, cCiTransportConnection *Tc);
  bool Process(int Length = 0, const uint8_t *Data = nullptr) override; // cCiSession
  dvbca_vector GetCaSystemIds(void) { return m_caSystemIds; }
  bool SendPMT(const cCiCaPmt &CaPmt);
  bool NeedCaPmt(void) const { return m_needCaPmt; }
  };

cCiConditionalAccessSupport::cCiConditionalAccessSupport(
    int SessionId, cCiTransportConnection *Tc) :
    cCiSession(SessionId, RI_CONDITIONAL_ACCESS_SUPPORT, Tc)
{
  dbgprotocol("New Conditional Access Support (session id %d)\n", SessionId);
}

bool cCiConditionalAccessSupport::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_CA_INFO: {
            dbgprotocol("%d: <== Ca Info", SessionId());
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            while (l > 1) {
                  unsigned short id = ((unsigned short)(*d) << 8) | *(d + 1);
                  dbgprotocol(" %04X", id);
                  d += 2;
                  l -= 2;

                  // Make sure the id is not already present
                  if (std::find(m_caSystemIds.cbegin(), m_caSystemIds.cend(), id)
                      != m_caSystemIds.end())
                      continue;

                  // Insert before the last element.
                  m_caSystemIds.emplace_back(id);
            }

            dbgprotocol("\n");
            }
            m_state = 2;
            m_needCaPmt = true;
            break;
       default: esyslog("ERROR: CI conditional access support: unknown tag %06X", Tag);
                return false;
       }
     }
  else if (m_state == 0) {
     dbgprotocol("%d: ==> Ca Info Enq\n", SessionId());
     SendData(AOT_CA_INFO_ENQ);
     m_state = 1;
     }
  return true;
}

bool cCiConditionalAccessSupport::SendPMT(const cCiCaPmt &CaPmt)
{
  if (m_state == 2) {
     SendData(AOT_CA_PMT, CaPmt.m_length, CaPmt.m_capmt);
     m_needCaPmt = false;
     return true;
     }
  return false;
}

// --- cCiDateTime -----------------------------------------------------------

class cCiDateTime : public cCiSession {
private:
  int    m_interval   { 0 };
  time_t m_lastTime   { 0 };
  int    m_timeOffset { 0 };
  bool SendDateTime(void);
public:
  cCiDateTime(int SessionId, cCiTransportConnection *Tc);
  bool Process(int Length = 0, const uint8_t *Data = nullptr) override; // cCiSession
  void SetTimeOffset(double offset);
  };

cCiDateTime::cCiDateTime(int SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_DATE_TIME, Tc)
{
  dbgprotocol("New Date Time (session id %d)\n", SessionId);
}

void cCiDateTime::SetTimeOffset(double offset)
{
    m_timeOffset = (int) offset;
    dbgprotocol("New Time Offset: %i secs\n", m_timeOffset);
}

static constexpr uint8_t DEC2BCD(uint8_t d)
    { return ((d / 10) << 4) + (d % 10); }
static constexpr uint8_t BYTE0(uint16_t a)
    { return static_cast<uint8_t>(a & 0xFF); }
static constexpr uint8_t BYTE1(uint16_t a)
    { return static_cast<uint8_t>((a >> 8) & 0xFF); }

bool cCiDateTime::SendDateTime(void)
{
  time_t t = time(nullptr);
  struct tm tm_gmt {};
  struct tm tm_loc {};

  // Avoid using signed time_t types
  if (m_timeOffset < 0)
      t -= (time_t)(-m_timeOffset);
  else
      t += (time_t)(m_timeOffset);

  if (gmtime_r(&t, &tm_gmt) && localtime_r(&t, &tm_loc)) {
     int Y = tm_gmt.tm_year;
     int M = tm_gmt.tm_mon + 1;
     int D = tm_gmt.tm_mday;
     int L = (M == 1 || M == 2) ? 1 : 0;
     int MJD = 14956 + D + int((Y - L) * 365.25) + int((M + 1 + L * 12) * 30.6001);
     uint16_t mjd = htons(MJD);
     int16_t local_offset = htons(tm_loc.tm_gmtoff / 60);
     std::vector<uint8_t> T {
         BYTE0(mjd),
         BYTE1(mjd),
         DEC2BCD(tm_gmt.tm_hour),
         DEC2BCD(tm_gmt.tm_min),
         DEC2BCD(tm_gmt.tm_sec),
         BYTE0(local_offset),
         BYTE1(local_offset)
     };

     dbgprotocol("%d: ==> Date Time\n", SessionId());
     SendData(AOT_DATE_TIME, T);
     //XXX return value of all SendData() calls???
     return true;
     }
  return false;
}

bool cCiDateTime::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_DATE_TIME_ENQ: {
            m_interval = 0;
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0)
               m_interval = *d;
            dbgprotocol("%d: <== Date Time Enq, interval = %d\n", SessionId(), m_interval);
            m_lastTime = time(nullptr);
            return SendDateTime();
            }
            break;
       default: esyslog("ERROR: CI date time: unknown tag %06X", Tag);
                return false;
       }
     }
  else if (m_interval && time(nullptr) - m_lastTime > m_interval) {
     m_lastTime = time(nullptr);
     return SendDateTime();
     }
  return true;
}

// --- cCiMMI ----------------------------------------------------------------

// Close MMI Commands:

enum CLOSE_MMI : std::uint8_t {
    CLOSE_MMI_IMMEDIATE                  = 0x00,
    CLOSE_MMI_DELAY                      = 0x01,
};

// Display Control Commands:

enum DISPLAY_CONTROL : std::uint8_t {
    DCC_SET_MMI_MODE                            = 0x01,
    DCC_DISPLAY_CHARACTER_TABLE_LIST            = 0x02,
    DCC_INPUT_CHARACTER_TABLE_LIST              = 0x03,
    DCC_OVERLAY_GRAPHICS_CHARACTERISTICS        = 0x04,
    DCC_FULL_SCREEN_GRAPHICS_CHARACTERISTICS    = 0x05,
};

// MMI Modes:

enum MMI_MODES : std::uint8_t {
    MM_HIGH_LEVEL                        = 0x01,
    MM_LOW_LEVEL_OVERLAY_GRAPHICS        = 0x02,
    MM_LOW_LEVEL_FULL_SCREEN_GRAPHICS    = 0x03,
};

// Display Reply IDs:

enum DISPLAY_REPLY_IDS : std::uint8_t {
    DRI_MMI_MODE_ACK                                = 0x01,
    DRI_LIST_DISPLAY_CHARACTER_TABLES               = 0x02,
    DRI_LIST_INPUT_CHARACTER_TABLES                 = 0x03,
    DRI_LIST_GRAPHIC_OVERLAY_CHARACTERISTICS        = 0x04,
    DRI_LIST_FULL_SCREEN_GRAPHIC_CHARACTERISTICS    = 0x05,
    DRI_UNKNOWN_DISPLAY_CONTROL_CMD                 = 0xF0,
    DRI_UNKNOWN_MMI_MODE                            = 0xF1,
    DRI_UNKNOWN_CHARACTER_TABLE                     = 0xF2,
};

// Enquiry Flags:

static constexpr uint8_t EF_BLIND { 0x01 };

// Answer IDs:

enum ANSWER_IDS : std::uint8_t {
    AI_CANCEL    = 0x00,
    AI_ANSWER    = 0x01,
};

class cCiMMI : public cCiSession {
private:
  char *GetText(int &Length, const uint8_t **Data);
  cCiMenu    *m_menu;
  cCiEnquiry *m_enquiry;
public:
  cCiMMI(int SessionId, cCiTransportConnection *Tc);
  ~cCiMMI() override;
  bool Process(int Length = 0, const uint8_t *Data = nullptr) override; // cCiSession
  bool HasUserIO(void) override { return m_menu || m_enquiry; } // cCiSession
  cCiMenu *Menu(void);
  cCiEnquiry *Enquiry(void);
  bool SendMenuAnswer(uint8_t Selection);
  bool SendAnswer(const char *Text);
  };

cCiMMI::cCiMMI(int SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_MMI, Tc)
{
  dbgprotocol("New MMI (session id %d)\n", SessionId);
  m_menu = nullptr;
  m_enquiry = nullptr;
}

cCiMMI::~cCiMMI()
{
  delete m_menu;
  delete m_enquiry;
}

char *cCiMMI::GetText(int &Length, const uint8_t **Data)
///< Gets the text at Data.
///< \param[in,out] Length The number of bytes to copy from Data. Updated for
///                 the size of the string read.
///< \param[in,out] Data A pointer to current location for reading data.
///                 Updated for the size of the string read.
///< \return Returns a pointer to a newly allocated string, or nullptr in case of error.
///< Upon return Length and Data represent the remaining data after the text has been skipped.
{
  int Tag = GetTag(Length, Data);
  if (Tag == AOT_TEXT_LAST) {
     char *s = GetString(Length, Data);
     dbgprotocol("%d: <== Text Last '%s'\n", SessionId(), s);
     return s;
     }
  esyslog("CI MMI: unexpected text tag: %06X", Tag);
  return nullptr;
}

bool cCiMMI::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_DISPLAY_CONTROL: {
            dbgprotocol("%d: <== Display Control\n", SessionId());
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               switch (*d) {
                 case DCC_SET_MMI_MODE:
                      if (l == 2 && *++d == MM_HIGH_LEVEL) {
                         struct tDisplayReply { uint8_t m_id; uint8_t m_mode; };
                         tDisplayReply dr {};
                         dr.m_id = DRI_MMI_MODE_ACK;
                         dr.m_mode = MM_HIGH_LEVEL;
                         dbgprotocol("%d: ==> Display Reply\n", SessionId());
                         SendData(AOT_DISPLAY_REPLY, 2, (uint8_t *)&dr);
                         }
                      break;
                 default: esyslog("CI MMI: unsupported display control command %02X", *d);
                          return false;
                 }
               }
            }
            break;
       case AOT_LIST_LAST:
       case AOT_MENU_LAST: {
            dbgprotocol("%d: <== Menu Last\n", SessionId());
            delete m_menu;
            m_menu = new cCiMenu(this, Tag == AOT_MENU_LAST);
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               // since the specification allows choiceNb to be undefined it is useless, so let's just skip it:
               d++;
               l--;
               if (l > 0) m_menu->m_titleText = GetText(l, &d);
               if (l > 0) m_menu->m_subTitleText = GetText(l, &d);
               if (l > 0) m_menu->m_bottomText = GetText(l, &d);
               while (l > 0) {
                     char *s = GetText(l, &d);
                     if (s) {
                        if (!m_menu->AddEntry(s))
                           free(s);
                        }
                     else {
                        break;
                        }
                     }
               }
            }
            break;
       case AOT_ENQ: {
            dbgprotocol("%d: <== Enq\n", SessionId());
            delete m_enquiry;
            m_enquiry = new cCiEnquiry(this);
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               uint8_t blind = *d++;
               //XXX GetByte()???
               l--;
               m_enquiry->m_blind = ((blind & EF_BLIND) != 0);
               m_enquiry->m_expectedLength = *d++;
               l--;
               // I really wonder why there is no text length field here...
               m_enquiry->m_text = CopyString(l, d);
               }
            }
            break;
       case AOT_CLOSE_MMI: {
            int l = 0;
            const uint8_t *d = GetData(Data, l);

            if(l > 0){
                switch(*d){
                case CLOSE_MMI_IMMEDIATE:
                    dbgprotocol("%d <== Menu Close: immediate\n", SessionId());
                    break;
                case CLOSE_MMI_DELAY:
                    dbgprotocol("%d <== Menu Close: delay\n", SessionId());
                    break;
                default: esyslog("ERROR: CI MMI: unknown close_mmi_cmd_id %02X", *d);
                    return false;
                }
            }

            break;
       }
       default: esyslog("ERROR: CI MMI: unknown tag %06X", Tag);
                return false;
       }
     }
  return true;
}

cCiMenu *cCiMMI::Menu(void)
{
  cCiMenu *m = m_menu;
  m_menu = nullptr;
  return m;
}

cCiEnquiry *cCiMMI::Enquiry(void)
{
  cCiEnquiry *e = m_enquiry;
  m_enquiry = nullptr;
  return e;
}

bool cCiMMI::SendMenuAnswer(uint8_t Selection)
{
  dbgprotocol("%d: ==> Menu Answ\n", SessionId());
  SendData(AOT_MENU_ANSW, 1, &Selection);
  //XXX return value of all SendData() calls???
  return true;
}

// Define protocol structure
extern "C" {
    struct tAnswer { uint8_t m_id; char m_text[256]; };
}

bool cCiMMI::SendAnswer(const char *Text)
{
  dbgprotocol("%d: ==> Answ\n", SessionId());
  tAnswer answer {};
  answer.m_id = Text ? AI_ANSWER : AI_CANCEL;
  if (Text) {
     strncpy(answer.m_text, Text, sizeof(answer.m_text) - 1);
     answer.m_text[255] = '\0';
  }
  SendData(AOT_ANSW, Text ? strlen(Text) + 1 : 1, (uint8_t *)&answer);
  //XXX return value of all SendData() calls???
  return true;
}

// --- cCiMenu ---------------------------------------------------------------

cCiMenu::cCiMenu(cCiMMI *MMI, bool Selectable)
  : m_mmi(MMI),
    m_selectable(Selectable)
{
}

cCiMenu::~cCiMenu()
{
  free(m_titleText);
  free(m_subTitleText);
  free(m_bottomText);
  for (int i = 0; i < m_numEntries; i++)
      free(m_entries[i]);
}

bool cCiMenu::AddEntry(char *s)
{
  if (m_numEntries < MAX_CIMENU_ENTRIES) {
     m_entries[m_numEntries++] = s;
     return true;
     }
  return false;
}

bool cCiMenu::Select(int Index)
{
  if (m_mmi && -1 <= Index && Index < m_numEntries)
     return m_mmi->SendMenuAnswer(Index + 1);
  return false;
}

bool cCiMenu::Cancel(void)
{
  return Select(-1);
}

// --- cCiEnquiry ------------------------------------------------------------

cCiEnquiry::~cCiEnquiry()
{
  free(m_text);
}

bool cCiEnquiry::Reply(const char *s)
{
  return m_mmi ? m_mmi->SendAnswer(s) : false;
}

bool cCiEnquiry::Cancel(void)
{
  return Reply(nullptr);
}

// --- cCiCaPmt --------------------------------------------------------------

// Ca Pmt Cmd Ids:

enum CPCI_IDS : std::uint8_t {
    CPCI_OK_DESCRAMBLING    = 0x01,
    CPCI_OK_MMI             = 0x02,
    CPCI_QUERY              = 0x03,
    CPCI_NOT_SELECTED       = 0x04,
};

cCiCaPmt::cCiCaPmt(int ProgramNumber, uint8_t cplm)
{
  m_capmt[m_length++] = cplm; // ca_pmt_list_management
  m_capmt[m_length++] = (ProgramNumber >> 8) & 0xFF;
  m_capmt[m_length++] =  ProgramNumber       & 0xFF;
  m_capmt[m_length++] = 0x01; // version_number, current_next_indicator - apparently vn doesn't matter, but cni must be 1

  // program_info_length
  m_infoLengthPos = m_length;// NOLINT(cppcoreguidelines-prefer-member-initializer)
  m_capmt[m_length++] = 0x00;
  m_capmt[m_length++] = 0x00;
}

void cCiCaPmt::AddElementaryStream(int type, int pid)
{
  if (m_length + 5 > int(sizeof(m_capmt)))
  {
    esyslog("ERROR: buffer overflow in CA_PMT");
    return;
  }

  m_capmt[m_length++] = type & 0xFF;
  m_capmt[m_length++] = (pid >> 8) & 0xFF;
  m_capmt[m_length++] =  pid       & 0xFF;

  // ES_info_length
  m_infoLengthPos = m_length;
  m_capmt[m_length++] = 0x00;
  m_capmt[m_length++] = 0x00;
}

/**
 *  \brief Inserts an Access Control (CA) Descriptor into a PMT.
 *
 *   The format of ca_pmt is:
\code
    ca_pmt_list_management
    [program header]
    if (program descriptors > 0)
        CPCI_OK_DESCRAMBLING
    [program descriptors]
    for each stream
        [stream header]
        if (stream descriptors > 0)
            CPCI_OK_DESCRAMBLING
        [stream descriptors]
\endcode
 */
void cCiCaPmt::AddCaDescriptor(int ca_system_id, int ca_pid, int data_len,
                               const uint8_t *data)
{
  if (!m_infoLengthPos)
  {
    esyslog("ERROR: adding CA descriptor without program/stream!");
    return;
  }

  if (m_length + data_len + 7 > int(sizeof(m_capmt)))
  {
    esyslog("ERROR: buffer overflow in CA_PMT");
    return;
  }

  // We are either at start of program descriptors or stream descriptors.
  if (m_infoLengthPos + 2 == m_length)
    m_capmt[m_length++] = CPCI_OK_DESCRAMBLING; // ca_pmt_cmd_id

  m_capmt[m_length++] = 0x09;           // CA descriptor tag
  m_capmt[m_length++] = 4 + data_len;   // descriptor length

  m_capmt[m_length++] = (ca_system_id >> 8) & 0xFF;
  m_capmt[m_length++] = ca_system_id & 0xFF;
  m_capmt[m_length++] = (ca_pid >> 8) & 0xFF;
  m_capmt[m_length++] = ca_pid & 0xFF;

  if (data_len > 0)
  {
    memcpy(&m_capmt[m_length], data, data_len);
    m_length += data_len;
  }

  // update program_info_length/ES_info_length
  int l = m_length - m_infoLengthPos - 2;
  m_capmt[m_infoLengthPos] = (l >> 8) & 0xFF;
  m_capmt[m_infoLengthPos + 1] = l & 0xFF;
}

// -- cLlCiHandler -------------------------------------------------------------

cLlCiHandler::cLlCiHandler(int Fd, int NumSlots)
  : m_fdCa(Fd),
    m_numSlots(NumSlots),
    m_tpl(new cCiTransportLayer(Fd, m_numSlots))
{
}

cLlCiHandler::~cLlCiHandler()
{
  cMutexLock MutexLock(&m_mutex);
  for (auto & session : m_sessions)
      delete session;
  delete m_tpl;
  close(m_fdCa);
}

cCiHandler *cCiHandler::CreateCiHandler(const char *FileName)
{
    int fd_ca = open(FileName, O_RDWR);
    if (fd_ca >= 0)
    {
        ca_caps_t Caps;
        if (ioctl(fd_ca, CA_GET_CAP, &Caps) == 0)
        {
            int NumSlots = Caps.slot_num;
            if (NumSlots > 0)
            {
                if (Caps.slot_type & CA_CI_LINK)
                    return new cLlCiHandler(fd_ca, NumSlots);
                if (Caps.slot_type & CA_CI)
                    return new cHlCiHandler(fd_ca, NumSlots);
                isyslog("CAM doesn't support either high or low level CI,"
                        " Caps.slot_type=%i", Caps.slot_type);
            }
            else
            {
                esyslog("ERROR: no CAM slots found");
            }
        }
        else
        {
            LOG_ERROR_STR(FileName);
        }
        close(fd_ca);
    }
    return nullptr;
}

int cLlCiHandler::ResourceIdToInt(const uint8_t *Data)
{
  return (ntohl(*(int *)Data));
}

bool cLlCiHandler::Send(uint8_t Tag, int SessionId, int ResourceId, int Status)
{
  std::vector<uint8_t> buffer {Tag, 0x00} ; // 0x00 will be replaced with length
  if (Status >= 0)
     buffer.push_back(Status);
  if (ResourceId) {
     buffer.push_back((ResourceId >> 24) & 0xFF);
     buffer.push_back((ResourceId >> 16) & 0xFF);
     buffer.push_back((ResourceId >>  8) & 0xFF);
     buffer.push_back( ResourceId        & 0xFF);
     }
  buffer.push_back((SessionId >> 8) & 0xFF);
  buffer.push_back( SessionId       & 0xFF);
  buffer[1] = buffer.size() - 2; // length
  return m_tc && m_tc->SendData(buffer) == OK;
}

cCiSession *cLlCiHandler::GetSessionBySessionId(int SessionId)
{
  for (auto & session : m_sessions) {
      if (session && session->SessionId() == SessionId)
         return session;
      }
  return nullptr;
}

cCiSession *cLlCiHandler::GetSessionByResourceId(int ResourceId, int Slot)
{
  for (auto & session : m_sessions) {
      if (session && session->Tc()->Slot() == Slot && session->ResourceId() == ResourceId)
         return session;
      }
  return nullptr;
}

cCiSession *cLlCiHandler::CreateSession(int ResourceId)
{
  if (!GetSessionByResourceId(ResourceId, m_tc->Slot())) {
     for (int i = 0; i < MAX_CI_SESSION; i++) {
         if (!m_sessions[i]) {
            switch (ResourceId) {
              case RI_RESOURCE_MANAGER:           return m_sessions[i] = new cCiResourceManager(i + 1, m_tc);
              case RI_APPLICATION_INFORMATION:    return m_sessions[i] = new cCiApplicationInformation(i + 1, m_tc);
              case RI_CONDITIONAL_ACCESS_SUPPORT: m_newCaSupport = true;
                                                  return m_sessions[i] = new cCiConditionalAccessSupport(i + 1, m_tc);
              case RI_HOST_CONTROL:               break; //XXX
              case RI_DATE_TIME:                  return m_sessions[i] = new cCiDateTime(i + 1, m_tc);
              case RI_MMI:                        return m_sessions[i] = new cCiMMI(i + 1, m_tc);
              }
            }
         }
     }
  return nullptr;
}

bool cLlCiHandler::OpenSession(int Length, const uint8_t *Data)
{
  if (Length == 6 && *(Data + 1) == 0x04) {
     int ResourceId = ResourceIdToInt(Data + 2);
     dbgprotocol("OpenSession %08X\n", ResourceId);
     switch (ResourceId) {
       case RI_RESOURCE_MANAGER:
       case RI_APPLICATION_INFORMATION:
       case RI_CONDITIONAL_ACCESS_SUPPORT:
       case RI_HOST_CONTROL:
       case RI_DATE_TIME:
       case RI_MMI:
       {
           cCiSession *Session = CreateSession(ResourceId);
           if (Session)
           {
               Send(ST_OPEN_SESSION_RESPONSE, Session->SessionId(),
                    Session->ResourceId(), SS_OK);
               return true;
           }
           esyslog("ERROR: can't create session for resource identifier: %08X",
                   ResourceId);
           break;
       }
       default: esyslog("ERROR: unknown resource identifier: %08X", ResourceId);
       }
     }
  return false;
}

bool cLlCiHandler::CloseSession(int SessionId)
{
  dbgprotocol("CloseSession %08X\n", SessionId);
  cCiSession *Session = GetSessionBySessionId(SessionId);
  if (Session && m_sessions[SessionId - 1] == Session) {
     delete Session;
     m_sessions[SessionId - 1] = nullptr;
     Send(ST_CLOSE_SESSION_RESPONSE, SessionId, 0, SS_OK);
     return true;
     }

  esyslog("ERROR: unknown session id: %d", SessionId);
  Send(ST_CLOSE_SESSION_RESPONSE, SessionId, 0, SS_NOT_ALLOCATED);
  return false;
}

int cLlCiHandler::CloseAllSessions(int Slot)
{
  int result = 0;
  for (auto & session : m_sessions) {
      if (session && session->Tc()->Slot() == Slot) {
         CloseSession(session->SessionId());
         result++;
         }
      }
  return result;
}

bool cLlCiHandler::Process(void)
{
    bool result = true;
    cMutexLock MutexLock(&m_mutex);

    for (int Slot = 0; Slot < m_numSlots; Slot++)
    {
        m_tc = m_tpl->Process(Slot);
        if (m_tc)
        {
            int Length = 0;
            const uint8_t *Data = m_tc->Data(Length);
            if (Data && Length > 1)
            {
                switch (*Data)
                {
                    case ST_SESSION_NUMBER:
                        if (Length > 4)
                        {
                            int SessionId = ntohs(*(short *)&Data[2]);
                            cCiSession *Session = GetSessionBySessionId(SessionId);
                            if (Session)
                            {
                                Session->Process(Length - 4, Data + 4);
                                if (Session->ResourceId() == RI_APPLICATION_INFORMATION)
                                {
#if 0
                                    esyslog("Test: %x",
                                            ((cCiApplicationInformation*)Session)->GetApplicationManufacturer());
#endif
                                }
                            }
                            else
                            {
                                esyslog("ERROR: unknown session id: %d", SessionId);
                            }
                        }
                        break;

                    case ST_OPEN_SESSION_REQUEST:
                        OpenSession(Length, Data);
                        break;

                    case ST_CLOSE_SESSION_REQUEST:
                        if (Length == 4)
                            CloseSession(ntohs(*(short *)&Data[2]));
                        break;

                    case ST_CREATE_SESSION_RESPONSE: //XXX fall through to default
                    case ST_CLOSE_SESSION_RESPONSE:  //XXX fall through to default
                    default:
                        esyslog("ERROR: unknown session tag: %02X", *Data);
                }
            }
        }
        else if (CloseAllSessions(Slot))
        {
            m_tpl->ResetSlot(Slot);
            result = false;
        }
        else if (m_tpl->ModuleReady(Slot))
        {
            dbgprotocol("Module ready in slot %d\n", Slot);
            m_tpl->NewConnection(Slot);
        }
    }

    bool UserIO = false;
    m_needCaPmt = false;
    for (auto & session : m_sessions)
    {
        if (session && session->Process())
        {
            UserIO |= session->HasUserIO();
            if (session->ResourceId() == RI_CONDITIONAL_ACCESS_SUPPORT)
            {
                auto *cas = dynamic_cast<cCiConditionalAccessSupport *>(session);
                if (cas == nullptr)
                    continue;
                m_needCaPmt |= cas->NeedCaPmt();
            }
        }
    }
    m_hasUserIO = UserIO;

    if (m_newCaSupport)
        m_newCaSupport = result = false; // triggers new SetCaPmt at caller!
    return result;
}

bool cLlCiHandler::EnterMenu(int Slot)
{
  cMutexLock MutexLock(&m_mutex);
  auto *api = dynamic_cast<cCiApplicationInformation *>(GetSessionByResourceId(RI_APPLICATION_INFORMATION, Slot));
  return api ? api->EnterMenu() : false;
}

cCiMenu *cLlCiHandler::GetMenu(void)
{
  cMutexLock MutexLock(&m_mutex);
  for (int Slot = 0; Slot < m_numSlots; Slot++) {
      auto *mmi = dynamic_cast<cCiMMI *>(GetSessionByResourceId(RI_MMI, Slot));
      if (mmi)
         return mmi->Menu();
      }
  return nullptr;
}

cCiEnquiry *cLlCiHandler::GetEnquiry(void)
{
  cMutexLock MutexLock(&m_mutex);
  for (int Slot = 0; Slot < m_numSlots; Slot++) {
      auto *mmi = dynamic_cast<cCiMMI *>(GetSessionByResourceId(RI_MMI, Slot));
      if (mmi)
         return mmi->Enquiry();
      }
  return nullptr;
}

dvbca_vector cLlCiHandler::GetCaSystemIds(int Slot)
 {
  static dvbca_vector empty {};
  cMutexLock MutexLock(&m_mutex);
  auto *cas = dynamic_cast<cCiConditionalAccessSupport *>(GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT, Slot));
  return cas ? cas->GetCaSystemIds() : empty;
}

bool cLlCiHandler::SetCaPmt(cCiCaPmt &CaPmt, int Slot)
{
  cMutexLock MutexLock(&m_mutex);
  auto *cas = dynamic_cast<cCiConditionalAccessSupport *>(GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT, Slot));
  return cas && cas->SendPMT(CaPmt);
}

void cLlCiHandler::SetTimeOffset(double offset_in_seconds)
{
    cMutexLock MutexLock(&m_mutex);
    cCiDateTime *dt = nullptr;

    for (uint i = 0; i < (uint) NumSlots(); i++)
    {
        dt = dynamic_cast<cCiDateTime*>(GetSessionByResourceId(RI_DATE_TIME, i));
        if (dt)
            dt->SetTimeOffset(offset_in_seconds);
    }
}

bool cLlCiHandler::Reset(int Slot)
{
  cMutexLock MutexLock(&m_mutex);
  CloseAllSessions(Slot);
  return m_tpl->ResetSlot(Slot);
}

bool cLlCiHandler::connected()
{
  return sConnected;
}

// -- cHlCiHandler -------------------------------------------------------------

cHlCiHandler::cHlCiHandler(int Fd, int NumSlots)
  : m_fdCa(Fd),
    m_numSlots(NumSlots)
{
    esyslog("New High level CI handler");
}

cHlCiHandler::~cHlCiHandler()
{
    cMutexLock MutexLock(&m_mutex);
    close(m_fdCa);
}

int cHlCiHandler::CommHL(unsigned tag, unsigned function, struct ca_msg *msg) const
{
    if (tag) {
        msg->msg[2] = tag & 0xff;
        msg->msg[1] = (tag & 0xff00) >> 8;
        msg->msg[0] = (tag & 0xff0000) >> 16;
        esyslog("Sending message=[%02x %02x %02x ]",
                       msg->msg[0], msg->msg[1], msg->msg[2]);
    }

    return ioctl(m_fdCa, function, msg);
}

int cHlCiHandler::GetData(unsigned tag, struct ca_msg *msg)
{
    return CommHL(tag, CA_GET_MSG, msg);
}

int cHlCiHandler::SendData(unsigned tag, struct ca_msg *msg)
{
    return CommHL(tag, CA_SEND_MSG, msg);
}

bool cHlCiHandler::Process(void)
{
    cMutexLock MutexLock(&m_mutex);

    struct ca_msg msg {};
    switch(m_state) {
    case 0:
        // Get CA_system_ids
        /*      Enquire         */
        if ((SendData(AOT_CA_INFO_ENQ, &msg)) < 0) {
            esyslog("HLCI communication failed");
        } else {
            dbgprotocol("==> Ca Info Enquiry");
            /*  Receive         */
            if ((GetData(AOT_CA_INFO, &msg)) < 0) {
                esyslog("HLCI communication failed");
            } else {
                QString message("Debug: ");
                for(int i = 0; i < 20; i++) {
                    message += QString("%1 ").arg(msg.msg[i]);
                }
                LOG(VB_GENERAL, LOG_DEBUG, message);
                dbgprotocol("<== Ca Info");
                int l = msg.msg[3];
                const uint8_t *d = &msg.msg[4];
                while (l > 1) {
                    unsigned short id = ((unsigned short)(*d) << 8) | *(d + 1);
                    dbgprotocol(" %04X", id);
                    d += 2;
                    l -= 2;

                    // Insert before the last element.
                    m_caSystemIds.emplace_back(id);
                }
                dbgprotocol("\n");
            }
            m_state = 1;
            break;
        }
    }

    bool result = true;

    return result;
}

bool cHlCiHandler::EnterMenu(int /*Slot*/)
{
    return false;
}

cCiMenu *cHlCiHandler::GetMenu(void)
{
    return nullptr;
}

cCiEnquiry *cHlCiHandler::GetEnquiry(void)
{
    return nullptr;
}

dvbca_vector cHlCiHandler::GetCaSystemIds(int /*Slot*/)
{
    return m_caSystemIds;
}

bool cHlCiHandler::SetCaPmt(cCiCaPmt &CaPmt, int /*Slot*/)
{
    cMutexLock MutexLock(&m_mutex);
    struct ca_msg msg {};

    esyslog("Setting CA PMT.");
    m_state = 2;

    msg.msg[3] = CaPmt.m_length;

    if (CaPmt.m_length > (256 - 4))
    {
        esyslog("CA message too long");
        return false;
    }

    memcpy(&msg.msg[4], CaPmt.m_capmt, CaPmt.m_length);

    if ((SendData(AOT_CA_PMT, &msg)) < 0) {
        esyslog("HLCI communication failed");
        return false;
    }

    return true;
}

bool cHlCiHandler::Reset(int /*Slot*/) const
{
    if ((ioctl(m_fdCa, CA_RESET)) < 0) {
        esyslog("ioctl CA_RESET failed.");
        return false;
    }
    return true;
}

bool cHlCiHandler::NeedCaPmt(void)
{
    return m_state == 1;
}
