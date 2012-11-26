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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 * The author can be reached at kls@cadsoft.de
 *
 * The project's page is at http://www.cadsoft.de/people/kls/vdr
 *
 */

#include "dvbci.h"
#include <errno.h>
#include <ctype.h>
#include <linux/dvb/ca.h>
#include <malloc.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <QString>

#include "mythlogging.h"

#ifndef MALLOC
#define MALLOC(type, size)  (type *)malloc(sizeof(type) * (size))
#endif

#define esyslog(a...) LOG(VB_GENERAL, LOG_ERR, QString().sprintf(a))
#define isyslog(a...) LOG(VB_DVBCAM, LOG_INFO, QString().sprintf(a))
#define dsyslog(a...) LOG(VB_DVBCAM, LOG_DEBUG, QString().sprintf(a))

#define LOG_ERROR         esyslog("ERROR (%s,%d): %m", __FILE__, __LINE__)
#define LOG_ERROR_STR(s)  esyslog("ERROR: %s: %m", s)


// Set these to 'true' for debug output:
static bool DumpTPDUDataTransfer = false;
static bool DebugProtocol = false;
static bool _connected = false;

#define dbgprotocol(a...) if (DebugProtocol) LOG(VB_DVBCAM, LOG_DEBUG, QString().sprintf(a))

#define OK       0
#define TIMEOUT -1
#define ERROR   -2

// --- Workarounds -----------------------------------------------------------

// The Irdeto AllCAM 4.7 (and maybe others, too) does not react on AOT_ENTER_MENU
// during the first few seconds of a newly established connection
#define WRKRND_TIME_BEFORE_ENTER_MENU  15 // seconds

// --- Helper functions ------------------------------------------------------

#define SIZE_INDICATOR 0x80

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
///< Gets the length field from the beginning of Data.
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
///< Sets the length field at the beginning of Data.
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

static char *CopyString(int Length, const uint8_t *Data)
///< Copies the string at Data.
///< \return Returns a pointer to a newly allocated string.
{
  char *s = MALLOC(char, Length + 1);
  strncpy(s, (char *)Data, Length);
  s[Length] = 0;
  return s;
}

static char *GetString(int &Length, const uint8_t **Data)
///< Gets the string at Data.
///< \return Returns a pointer to a newly allocated string, or NULL in case of error.
///< Upon return Length and Data represent the remaining data after the string has been skipped.
{
  if (Length > 0 && Data && *Data) {
     int l = 0;
     const uint8_t *d = GetLength(*Data, l);
     char *s = CopyString(l, d);
     Length -= d - *Data + l;
     *Data = d + l;
     return s;
     }
  return NULL;
}



// --- cMutex ----------------------------------------------------------------

cMutex::cMutex(void)
{
  lockingPid = 0;
  locked = 0;
  pthread_mutex_init(&mutex, NULL);
}

cMutex::~cMutex()
{
  pthread_mutex_destroy(&mutex);
}

void cMutex::Lock(void)
{
  if (getpid() != lockingPid || !locked) {
     pthread_mutex_lock(&mutex);
     lockingPid = getpid();
     }
  locked++;
}

void cMutex::Unlock(void)
{
 if (--locked <= 0) {
    if (locked < 0) {
        esyslog("cMutex Lock inbalance detected");
        locked = 0;
        }
    lockingPid = 0;
    pthread_mutex_unlock(&mutex);
    }
}
// --- cMutexLock ------------------------------------------------------------

cMutexLock::cMutexLock(cMutex *Mutex)
{
  mutex = NULL;
  locked = false;
  Lock(Mutex);
}

cMutexLock::~cMutexLock()
{
  if (mutex && locked)
     mutex->Unlock();
}

bool cMutexLock::Lock(cMutex *Mutex)
{
  if (Mutex && !mutex) {
     mutex = Mutex;
     Mutex->Lock();
     locked = true;
     return true;
     }
  return false;
}



// --- cTPDU -----------------------------------------------------------------

#define MAX_TPDU_SIZE  2048
#define MAX_TPDU_DATA  (MAX_TPDU_SIZE - 4)

#define DATA_INDICATOR 0x80

#define T_SB           0x80
#define T_RCV          0x81
#define T_CREATE_TC    0x82
#define T_CTC_REPLY    0x83
#define T_DELETE_TC    0x84
#define T_DTC_REPLY    0x85
#define T_REQUEST_TC   0x86
#define T_NEW_TC       0x87
#define T_TC_ERROR     0x88
#define T_DATA_LAST    0xA0
#define T_DATA_MORE    0xA1

class cTPDU {
private:
  int size;
  uint8_t data[MAX_TPDU_SIZE];
  const uint8_t *GetData(const uint8_t *Data, int &Length);
public:
  cTPDU(void) { size = 0; memset(data, 0, sizeof(uint8_t) * MAX_TPDU_SIZE); }
  cTPDU(uint8_t Slot, uint8_t Tcid, uint8_t Tag, int Length = 0, const uint8_t *Data = NULL);
  uint8_t Slot(void) { return data[0]; }
  uint8_t Tcid(void) { return data[1]; }
  uint8_t Tag(void)  { return data[2]; }
  const uint8_t *Data(int &Length) { return GetData(data + 3, Length); }
  uint8_t Status(void);
  int Write(int fd);
  int Read(int fd);
  void Dump(bool Outgoing);
  };

cTPDU::cTPDU(uint8_t Slot, uint8_t Tcid, uint8_t Tag, int Length, const uint8_t *Data)
{
  size = 0;
  data[0] = Slot;
  data[1] = Tcid;
  data[2] = Tag;
  switch (Tag) {
    case T_RCV:
    case T_CREATE_TC:
    case T_CTC_REPLY:
    case T_DELETE_TC:
    case T_DTC_REPLY:
    case T_REQUEST_TC:
         data[3] = 1; // length
         data[4] = Tcid;
         size = 5;
         break;
    case T_NEW_TC:
    case T_TC_ERROR:
         if (Length == 1) {
            data[3] = 2; // length
            data[4] = Tcid;
            data[5] = Data[0];
            size = 6;
            }
         else
            esyslog("ERROR: illegal data length for TPDU tag 0x%02X: %d", Tag, Length);
         break;
    case T_DATA_LAST:
    case T_DATA_MORE:
         if (Length <= MAX_TPDU_DATA) {
            uint8_t *p = data + 3;
            p = SetLength(p, Length + 1);
            *p++ = Tcid;
            if (Length)
               memcpy(p, Data, Length);
            size = Length + (p - data);
            }
         else
            esyslog("ERROR: illegal data length for TPDU tag 0x%02X: %d", Tag, Length);
         break;
    default:
         esyslog("ERROR: unknown TPDU tag: 0x%02X", Tag);
    }
 }

int cTPDU::Write(int fd)
{
  Dump(true);
  if (size)
     return write(fd, data, size) == size ? OK : ERROR;
  esyslog("ERROR: attemp to write TPDU with zero size");
  return ERROR;
}

int cTPDU::Read(int fd)
{
  size = safe_read(fd, data, sizeof(data));
  if (size < 0) {
     esyslog("ERROR: %m");
     size = 0;
     return ERROR;
     }
  Dump(false);
  return OK;
}

void cTPDU::Dump(bool Outgoing)
{
  if (DumpTPDUDataTransfer) {
#define MAX_DUMP 256
     QString msg = QString("%1 ").arg(Outgoing ? "-->" : "<--");
     for (int i = 0; i < size && i < MAX_DUMP; i++)
         msg += QString("%1 ").arg((short int)data[i], 2, 16, QChar('0'));
     if (size >= MAX_DUMP)
         msg += "...";
     LOG(VB_DVBCAM, LOG_INFO, msg);
     if (!Outgoing) {
        msg = QString("   ");
        for (int i = 0; i < size && i < MAX_DUMP; i++)
            msg += QString("%1 ").arg(isprint(data[i]) ? data[i] : '.', 2);
        if (size >= MAX_DUMP)
            msg += "...";
        LOG(VB_DVBCAM, LOG_INFO, msg);
        }
     }
}

const uint8_t *cTPDU::GetData(const uint8_t *Data, int &Length)
{
  if (size) {
     Data = GetLength(Data, Length);
     if (Length) {
        Length--; // the first byte is always the tcid
        return Data + 1;
        }
     }
  return NULL;
}

uint8_t cTPDU::Status(void)
{
  if (size >= 4 && data[size - 4] == T_SB && data[size - 3] == 2) {
     //XXX test tcid???
     return data[size - 1];
     }
  return 0;
}

// --- cCiTransportConnection ------------------------------------------------

enum eState { stIDLE, stCREATION, stACTIVE, stDELETION };

class cCiTransportConnection {
  friend class cCiTransportLayer;
private:
  int fd;
  uint8_t slot;
  uint8_t tcid;
  eState state;
  cTPDU *tpdu;
  struct timeval last_poll;
  int lastResponse;
  bool dataAvailable;
  void Init(int Fd, uint8_t Slot, uint8_t Tcid);
  int SendTPDU(uint8_t Tag, int Length = 0, const uint8_t *Data = NULL);
  int RecvTPDU(void);
  int CreateConnection(void);
  int Poll(void);
  eState State(void) { return state; }
  int LastResponse(void) { return lastResponse; }
  bool DataAvailable(void) { return dataAvailable; }
public:
  cCiTransportConnection(void);
  ~cCiTransportConnection();
  int Slot(void) const { return slot; }
  int SendData(int Length, const uint8_t *Data);
  int RecvData(void);
  const uint8_t *Data(int &Length);
  //XXX Close()
  };

cCiTransportConnection::cCiTransportConnection(void)
{
  tpdu = NULL;
  last_poll.tv_sec = 0;
  last_poll.tv_usec = 0;
  Init(-1, 0, 0);
}

cCiTransportConnection::~cCiTransportConnection()
{
  delete tpdu;
}

void cCiTransportConnection::Init(int Fd, uint8_t Slot, uint8_t Tcid)
{
  fd = Fd;
  slot = Slot;
  tcid = Tcid;
  state = stIDLE;
  if (fd >= 0 && !tpdu)
     tpdu = new cTPDU;
  lastResponse = ERROR;
  dataAvailable = false;
//XXX Clear()???
}

int cCiTransportConnection::SendTPDU(uint8_t Tag, int Length, const uint8_t *Data)
{
  cTPDU TPDU(slot, tcid, Tag, Length, Data);
  return TPDU.Write(fd);
}

#define CAM_READ_TIMEOUT  5000 // ms

int cCiTransportConnection::RecvTPDU(void)
{
  struct pollfd pfd[1];
  pfd[0].fd = fd;
  pfd[0].events = POLLIN;
  lastResponse = ERROR;

  for (;;) {
      int ret = poll(pfd, 1, CAM_READ_TIMEOUT);
      if (ret == -1 && (errno == EAGAIN || errno == EINTR))
          continue;
      break;
  }

  if (
        (pfd[0].revents & POLLIN) &&
        tpdu->Read(fd) == OK &&
        tpdu->Tcid() == tcid
     )
  {
     switch (state) {
       case stIDLE:     break;
       case stCREATION: if (tpdu->Tag() == T_CTC_REPLY) {
                           dataAvailable = tpdu->Status() & DATA_INDICATOR;
                           state = stACTIVE;
                           lastResponse = tpdu->Tag();
                           }
                        break;
       case stACTIVE:   switch (tpdu->Tag()) {
                          case T_SB:
                          case T_DATA_LAST:
                          case T_DATA_MORE:
                          case T_REQUEST_TC: break;
                          case T_DELETE_TC:  if (SendTPDU(T_DTC_REPLY) != OK)
                                                return ERROR;
                                             Init(fd, slot, tcid);
                                             break;
                          default: return ERROR;
                          }
                        dataAvailable = tpdu->Status() & DATA_INDICATOR;
                        lastResponse = tpdu->Tag();
                        break;
       case stDELETION: if (tpdu->Tag() == T_DTC_REPLY) {
                           Init(fd, slot, tcid);
                           //XXX Status()???
                           lastResponse = tpdu->Tag();
                           }
                        break;
       }
     }
  else {
     esyslog("ERROR: CAM: Read failed: slot %d, tcid %d\n", slot, tcid);
     Init(-1, slot, tcid);
     }
  return lastResponse;
}

int cCiTransportConnection::SendData(int Length, const uint8_t *Data)
{
  while (state == stACTIVE && Length > 0) {
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
  return tpdu->Data(Length);
}

#define MAX_CONNECT_RETRIES  25

int cCiTransportConnection::CreateConnection(void)
{
  if (state == stIDLE) {
     if (SendTPDU(T_CREATE_TC) == OK) {
        state = stCREATION;
        if (RecvTPDU() == T_CTC_REPLY) {
           _connected=true;
           return OK;
        // the following is a workaround for CAMs that don't quite follow the specs...
        } else {
           for (int i = 0; i < MAX_CONNECT_RETRIES; i++) {
               dsyslog("CAM: retrying to establish connection");
               if (RecvTPDU() == T_CTC_REPLY) {
                  dsyslog("CAM: connection established");
                  _connected=true;
                  return OK;
                  }
               }
           return ERROR;
           }
        }
     }
  return ERROR;
}

// Polls can be done with a 100ms interval (EN50221 - A.4.1.12)
#define POLL_INTERVAL 100

int cCiTransportConnection::Poll(void)
{
  struct timeval curr_time;

  if (state != stACTIVE)
    return ERROR;

  gettimeofday(&curr_time, 0);
  uint64_t msdiff = (curr_time.tv_sec * 1000) + (curr_time.tv_usec / 1000) -
                    (last_poll.tv_sec * 1000) - (last_poll.tv_usec / 1000);

  if (msdiff < POLL_INTERVAL)
    return OK;

  last_poll.tv_sec = curr_time.tv_sec;
  last_poll.tv_usec = curr_time.tv_usec;

  if (SendTPDU(T_DATA_LAST) != OK)
    return ERROR;

  return RecvTPDU();
}

// --- cCiTransportLayer -----------------------------------------------------

#define MAX_CI_CONNECT  16 // maximum possible value is 254

class cCiTransportLayer {
private:
  int fd;
  int numSlots;
  cCiTransportConnection tc[MAX_CI_CONNECT];
public:
  cCiTransportLayer(int Fd, int NumSlots);
  cCiTransportConnection *NewConnection(int Slot);
  bool ResetSlot(int Slot);
  bool ModuleReady(int Slot);
  cCiTransportConnection *Process(int Slot);
  };

cCiTransportLayer::cCiTransportLayer(int Fd, int NumSlots)
{
  fd = Fd;
  numSlots = NumSlots;
  for (int s = 0; s < numSlots; s++)
      ResetSlot(s);
}

cCiTransportConnection *cCiTransportLayer::NewConnection(int Slot)
{
  for (int i = 0; i < MAX_CI_CONNECT; i++) {
      if (tc[i].State() == stIDLE) {
         dbgprotocol("Creating connection: slot %d, tcid %d\n", Slot, i + 1);
         tc[i].Init(fd, Slot, i + 1);
         if (tc[i].CreateConnection() == OK)
            return &tc[i];
         break;
         }
      }
  return NULL;
}

bool cCiTransportLayer::ResetSlot(int Slot)
{
  dbgprotocol("Resetting slot %d...", Slot);
  if (ioctl(fd, CA_RESET, 1 << Slot) != -1) {
     dbgprotocol("ok.\n");
     return true;
     }
  else
     esyslog("ERROR: can't reset CAM slot %d: %m", Slot);
  dbgprotocol("failed!\n");
  return false;
}

bool cCiTransportLayer::ModuleReady(int Slot)
{
  ca_slot_info_t sinfo;
  sinfo.num = Slot;
  if (ioctl(fd, CA_GET_SLOT_INFO, &sinfo) != -1)
     return sinfo.flags & CA_CI_MODULE_READY;
  else
     esyslog("ERROR: can't get info on CAM slot %d: %m", Slot);
  return false;
}

cCiTransportConnection *cCiTransportLayer::Process(int Slot)
{
  for (int i = 0; i < MAX_CI_CONNECT; i++) {
      cCiTransportConnection *Tc = &tc[i];
      if (Tc->Slot() == Slot) {
         switch (Tc->State()) {
           case stCREATION:
           case stACTIVE:
                if (!Tc->DataAvailable()) {
                   if (Tc->Poll() != OK)
                      ;//XXX continue;
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
                       return NULL;
                       break;
                  }
                //XXX this will only work with _one_ transport connection per slot!
                return Tc;
                break;
           default: ;
           }
         }
      }
  return NULL;
}

// -- cCiSession -------------------------------------------------------------

// Session Tags:

#define ST_SESSION_NUMBER           0x90
#define ST_OPEN_SESSION_REQUEST     0x91
#define ST_OPEN_SESSION_RESPONSE    0x92
#define ST_CREATE_SESSION           0x93
#define ST_CREATE_SESSION_RESPONSE  0x94
#define ST_CLOSE_SESSION_REQUEST    0x95
#define ST_CLOSE_SESSION_RESPONSE   0x96

// Session Status:

#define SS_OK             0x00
#define SS_NOT_ALLOCATED  0xF0

// Resource Identifiers:

#define RI_RESOURCE_MANAGER            0x00010041
#define RI_APPLICATION_INFORMATION     0x00020041
#define RI_CONDITIONAL_ACCESS_SUPPORT  0x00030041
#define RI_HOST_CONTROL                0x00200041
#define RI_DATE_TIME                   0x00240041
#define RI_MMI                         0x00400041

// Application Object Tags:

#define AOT_NONE                    0x000000
#define AOT_PROFILE_ENQ             0x9F8010
#define AOT_PROFILE                 0x9F8011
#define AOT_PROFILE_CHANGE          0x9F8012
#define AOT_APPLICATION_INFO_ENQ    0x9F8020
#define AOT_APPLICATION_INFO        0x9F8021
#define AOT_ENTER_MENU              0x9F8022
#define AOT_CA_INFO_ENQ             0x9F8030
#define AOT_CA_INFO                 0x9F8031
#define AOT_CA_PMT                  0x9F8032
#define AOT_CA_PMT_REPLY            0x9F8033
#define AOT_TUNE                    0x9F8400
#define AOT_REPLACE                 0x9F8401
#define AOT_CLEAR_REPLACE           0x9F8402
#define AOT_ASK_RELEASE             0x9F8403
#define AOT_DATE_TIME_ENQ           0x9F8440
#define AOT_DATE_TIME               0x9F8441
#define AOT_CLOSE_MMI               0x9F8800
#define AOT_DISPLAY_CONTROL         0x9F8801
#define AOT_DISPLAY_REPLY           0x9F8802
#define AOT_TEXT_LAST               0x9F8803
#define AOT_TEXT_MORE               0x9F8804
#define AOT_KEYPAD_CONTROL          0x9F8805
#define AOT_KEYPRESS                0x9F8806
#define AOT_ENQ                     0x9F8807
#define AOT_ANSW                    0x9F8808
#define AOT_MENU_LAST               0x9F8809
#define AOT_MENU_MORE               0x9F880A
#define AOT_MENU_ANSW               0x9F880B
#define AOT_LIST_LAST               0x9F880C
#define AOT_LIST_MORE               0x9F880D
#define AOT_SUBTITLE_SEGMENT_LAST   0x9F880E
#define AOT_SUBTITLE_SEGMENT_MORE   0x9F880F
#define AOT_DISPLAY_MESSAGE         0x9F8810
#define AOT_SCENE_END_MARK          0x9F8811
#define AOT_SCENE_DONE              0x9F8812
#define AOT_SCENE_CONTROL           0x9F8813
#define AOT_SUBTITLE_DOWNLOAD_LAST  0x9F8814
#define AOT_SUBTITLE_DOWNLOAD_MORE  0x9F8815
#define AOT_FLUSH_DOWNLOAD          0x9F8816
#define AOT_DOWNLOAD_REPLY          0x9F8817
#define AOT_COMMS_CMD               0x9F8C00
#define AOT_CONNECTION_DESCRIPTOR   0x9F8C01
#define AOT_COMMS_REPLY             0x9F8C02
#define AOT_COMMS_SEND_LAST         0x9F8C03
#define AOT_COMMS_SEND_MORE         0x9F8C04
#define AOT_COMMS_RCV_LAST          0x9F8C05
#define AOT_COMMS_RCV_MORE          0x9F8C06

class cCiSession {
private:
  int sessionId;
  int resourceId;
  cCiTransportConnection *tc;
protected:
  int GetTag(int &Length, const uint8_t **Data);
  const uint8_t *GetData(const uint8_t *Data, int &Length);
  int SendData(int Tag, int Length = 0, const uint8_t *Data = NULL);
public:
  cCiSession(int SessionId, int ResourceId, cCiTransportConnection *Tc);
  virtual ~cCiSession();
  const cCiTransportConnection *Tc(void) { return tc; }
  int SessionId(void) { return sessionId; }
  int ResourceId(void) { return resourceId; }
  virtual bool HasUserIO(void) { return false; }
  virtual bool Process(int Length = 0, const uint8_t *Data = NULL);
  };

cCiSession::cCiSession(int SessionId, int ResourceId, cCiTransportConnection *Tc)
{
  sessionId = SessionId;
  resourceId = ResourceId;
  tc = Tc;
}

cCiSession::~cCiSession()
{
}

int cCiSession::GetTag(int &Length, const uint8_t **Data)
///< Gets the tag at Data.
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
  return Length ? Data : NULL;
}

int cCiSession::SendData(int Tag, int Length, const uint8_t *Data)
{
  uint8_t buffer[2048];
  uint8_t *p = buffer;
  *p++ = ST_SESSION_NUMBER;
  *p++ = 0x02;
  *p++ = (sessionId >> 8) & 0xFF;
  *p++ =  sessionId       & 0xFF;
  *p++ = (Tag >> 16) & 0xFF;
  *p++ = (Tag >>  8) & 0xFF;
  *p++ =  Tag        & 0xFF;
  p = SetLength(p, Length);
  if (p - buffer + Length < int(sizeof(buffer))) {
     memcpy(p, Data, Length);
     p += Length;
     return tc->SendData(p - buffer, buffer);
     }
  esyslog("ERROR: CAM: data length (%d) exceeds buffer size", Length);
  return ERROR;
}

bool cCiSession::Process(int Length, const uint8_t *Data)
{
  (void)Length;
  (void)Data;
  return true;
}

// -- cCiResourceManager -----------------------------------------------------

class cCiResourceManager : public cCiSession {
private:
  int state;
public:
  cCiResourceManager(int SessionId, cCiTransportConnection *Tc);
  virtual bool Process(int Length = 0, const uint8_t *Data = NULL);
  };

cCiResourceManager::cCiResourceManager(int SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_RESOURCE_MANAGER, Tc)
{
  dbgprotocol("New Resource Manager (session id %d)\n", SessionId);
  state = 0;
}

bool cCiResourceManager::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_PROFILE_ENQ: {
            dbgprotocol("%d: <== Profile Enquiry\n", SessionId());
            uint32_t resources[] =
            {
                htonl(RI_RESOURCE_MANAGER),
                htonl(RI_APPLICATION_INFORMATION),
                htonl(RI_CONDITIONAL_ACCESS_SUPPORT),
                htonl(RI_DATE_TIME),
                htonl(RI_MMI)
            };
            dbgprotocol("%d: ==> Profile\n", SessionId());
            SendData(AOT_PROFILE, sizeof(resources), (uint8_t*)resources);
            state = 3;
            }
            break;
       case AOT_PROFILE: {
            dbgprotocol("%d: <== Profile\n", SessionId());
            if (state == 1) {
               int l = 0;
               const uint8_t *d = GetData(Data, l);
               if (l > 0 && d)
                  esyslog("CI resource manager: unexpected data");
               dbgprotocol("%d: ==> Profile Change\n", SessionId());
               SendData(AOT_PROFILE_CHANGE);
               state = 2;
               }
            else {
               esyslog("ERROR: CI resource manager: unexpected tag %06X in state %d", Tag, state);
               }
            }
            break;
       default: esyslog("ERROR: CI resource manager: unknown tag %06X", Tag);
                return false;
       }
     }
  else if (state == 0) {
     dbgprotocol("%d: ==> Profile Enq\n", SessionId());
     SendData(AOT_PROFILE_ENQ);
     state = 1;
     }
  return true;
}

// --- cCiApplicationInformation ---------------------------------------------

class cCiApplicationInformation : public cCiSession {
private:
  int state;
  time_t creationTime;
  uint8_t applicationType;
  uint16_t applicationManufacturer;
  uint16_t manufacturerCode;
  char *menuString;
public:
  cCiApplicationInformation(int SessionId, cCiTransportConnection *Tc);
  virtual ~cCiApplicationInformation();
  virtual bool Process(int Length = 0, const uint8_t *Data = NULL);
  bool EnterMenu(void);
  char *GetApplicationString() { return strdup(menuString); };
  uint16_t GetApplicationManufacturer() { return applicationManufacturer; };
  uint16_t GetManufacturerCode()        { return manufacturerCode; };
  };

cCiApplicationInformation::cCiApplicationInformation(int SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_APPLICATION_INFORMATION, Tc)
{
  dbgprotocol("New Application Information (session id %d)\n", SessionId);
  state = 0;
  creationTime = time(NULL);
  applicationType = 0;
  applicationManufacturer = 0;
  manufacturerCode = 0;
  menuString = NULL;
}

cCiApplicationInformation::~cCiApplicationInformation()
{
  free(menuString);
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
            if ((l -= 1) < 0) break;
            applicationType = *d++;
            if ((l -= 2) < 0) break;
            applicationManufacturer = ntohs(*(uint16_t *)d);
            d += 2;
            if ((l -= 2) < 0) break;
            manufacturerCode = ntohs(*(uint16_t *)d);
            d += 2;
            free(menuString);
            menuString = GetString(l, &d);
            isyslog("CAM: %s, %02X, %04X, %04X", menuString, applicationType,
                            applicationManufacturer, manufacturerCode);
            }
            state = 2;
            break;
       default: esyslog("ERROR: CI application information: unknown tag %06X", Tag);
                return false;
       }
     }
  else if (state == 0) {
     dbgprotocol("%d: ==> Application Info Enq\n", SessionId());
     SendData(AOT_APPLICATION_INFO_ENQ);
     state = 1;
     }
  return true;
}

bool cCiApplicationInformation::EnterMenu(void)
{
  if (state == 2 && time(NULL) - creationTime > WRKRND_TIME_BEFORE_ENTER_MENU) {
     dbgprotocol("%d: ==> Enter Menu\n", SessionId());
     SendData(AOT_ENTER_MENU);
     return true;//XXX
     }
  return false;
}

// --- cCiConditionalAccessSupport -------------------------------------------

class cCiConditionalAccessSupport : public cCiSession {
private:
  int state;
  int numCaSystemIds;
  unsigned short caSystemIds[MAXCASYSTEMIDS + 1]; // list is zero terminated!
  bool needCaPmt;
public:
  cCiConditionalAccessSupport(int SessionId, cCiTransportConnection *Tc);
  virtual bool Process(int Length = 0, const uint8_t *Data = NULL);
  const unsigned short *GetCaSystemIds(void) { return caSystemIds; }
  bool SendPMT(cCiCaPmt &CaPmt);
  bool NeedCaPmt(void) { return needCaPmt; }
  };

cCiConditionalAccessSupport::cCiConditionalAccessSupport(
    int SessionId, cCiTransportConnection *Tc) :
    cCiSession(SessionId, RI_CONDITIONAL_ACCESS_SUPPORT, Tc),
    state(0), numCaSystemIds(0), needCaPmt(false)
{
  dbgprotocol("New Conditional Access Support (session id %d)\n", SessionId);
  memset(caSystemIds, 0, sizeof(caSystemIds));
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
                  if (numCaSystemIds < MAXCASYSTEMIDS) {
                     int i = 0;
                     // Make sure the id is not already present
                     for (; i < numCaSystemIds; i++)
                        if (caSystemIds[i] == id)
                           break;

                     if (i < numCaSystemIds)
                         continue;

                     caSystemIds[numCaSystemIds++] = id;
                     caSystemIds[numCaSystemIds] = 0;
                     }
                  else
                     esyslog("ERROR: too many CA system IDs!");
                    }
            dbgprotocol("\n");
            }
            state = 2;
            needCaPmt = true;
            break;
       default: esyslog("ERROR: CI conditional access support: unknown tag %06X", Tag);
                return false;
       }
     }
  else if (state == 0) {
     dbgprotocol("%d: ==> Ca Info Enq\n", SessionId());
     SendData(AOT_CA_INFO_ENQ);
     state = 1;
     }
  return true;
}

bool cCiConditionalAccessSupport::SendPMT(cCiCaPmt &CaPmt)
{
  if (state == 2) {
     SendData(AOT_CA_PMT, CaPmt.length, CaPmt.capmt);
     needCaPmt = false;
     return true;
     }
  return false;
}

// --- cCiDateTime -----------------------------------------------------------

class cCiDateTime : public cCiSession {
private:
  int interval;
  time_t lastTime;
  int timeOffset;
  bool SendDateTime(void);
public:
  cCiDateTime(int SessionId, cCiTransportConnection *Tc);
  virtual bool Process(int Length = 0, const uint8_t *Data = NULL);
  void SetTimeOffset(double offset);
  };

cCiDateTime::cCiDateTime(int SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_DATE_TIME, Tc)
{
  interval = 0;
  lastTime = 0;
  timeOffset = 0;
  dbgprotocol("New Date Time (session id %d)\n", SessionId);
}

void cCiDateTime::SetTimeOffset(double offset)
{
    timeOffset = (int) offset;
    dbgprotocol("New Time Offset: %i secs\n", timeOffset);
}

bool cCiDateTime::SendDateTime(void)
{
  time_t t = time(NULL);
  struct tm tm_gmt;
  struct tm tm_loc;

  // Avoid using signed time_t types
  if (timeOffset < 0)
      t -= (time_t)(-timeOffset);
  else
      t += (time_t)(timeOffset);

  if (gmtime_r(&t, &tm_gmt) && localtime_r(&t, &tm_loc)) {
     int Y = tm_gmt.tm_year;
     int M = tm_gmt.tm_mon + 1;
     int D = tm_gmt.tm_mday;
     int L = (M == 1 || M == 2) ? 1 : 0;
     int MJD = 14956 + D + int((Y - L) * 365.25) + int((M + 1 + L * 12) * 30.6001);
#define DEC2BCD(d) (uint8_t(((d / 10) << 4) + (d % 10)))
     struct tTime { unsigned short mjd; uint8_t h, m, s; short offset; };
     tTime T = {
         mjd : htons(MJD),
         h : DEC2BCD(tm_gmt.tm_hour),
         m : DEC2BCD(tm_gmt.tm_min),
         s : DEC2BCD(tm_gmt.tm_sec),
         offset : static_cast<short>(htons(tm_loc.tm_gmtoff / 60))
     };
     dbgprotocol("%d: ==> Date Time\n", SessionId());
     SendData(AOT_DATE_TIME, 7, (uint8_t*)&T);
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
            interval = 0;
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0)
               interval = *d;
            dbgprotocol("%d: <== Date Time Enq, interval = %d\n", SessionId(), interval);
            lastTime = time(NULL);
            return SendDateTime();
            }
            break;
       default: esyslog("ERROR: CI date time: unknown tag %06X", Tag);
                return false;
       }
     }
  else if (interval && time(NULL) - lastTime > interval) {
     lastTime = time(NULL);
     return SendDateTime();
     }
  return true;
}

// --- cCiMMI ----------------------------------------------------------------

// Close MMI Commands:

#define CLOSE_MMI_IMMEDIATE                0x00
#define CLOSE_MMI_DELAY                    0x01

// Display Control Commands:

#define DCC_SET_MMI_MODE                          0x01
#define DCC_DISPLAY_CHARACTER_TABLE_LIST          0x02
#define DCC_INPUT_CHARACTER_TABLE_LIST            0x03
#define DCC_OVERLAY_GRAPHICS_CHARACTERISTICS      0x04
#define DCC_FULL_SCREEN_GRAPHICS_CHARACTERISTICS  0x05

// MMI Modes:

#define MM_HIGH_LEVEL                      0x01
#define MM_LOW_LEVEL_OVERLAY_GRAPHICS      0x02
#define MM_LOW_LEVEL_FULL_SCREEN_GRAPHICS  0x03

// Display Reply IDs:

#define DRI_MMI_MODE_ACK                              0x01
#define DRI_LIST_DISPLAY_CHARACTER_TABLES             0x02
#define DRI_LIST_INPUT_CHARACTER_TABLES               0x03
#define DRI_LIST_GRAPHIC_OVERLAY_CHARACTERISTICS      0x04
#define DRI_LIST_FULL_SCREEN_GRAPHIC_CHARACTERISTICS  0x05
#define DRI_UNKNOWN_DISPLAY_CONTROL_CMD               0xF0
#define DRI_UNKNOWN_MMI_MODE                          0xF1
#define DRI_UNKNOWN_CHARACTER_TABLE                   0xF2

// Enquiry Flags:

#define EF_BLIND  0x01

// Answer IDs:

#define AI_CANCEL  0x00
#define AI_ANSWER  0x01

class cCiMMI : public cCiSession {
private:
  char *GetText(int &Length, const uint8_t **Data);
  cCiMenu *menu;
  cCiEnquiry *enquiry;
public:
  cCiMMI(int SessionId, cCiTransportConnection *Tc);
  virtual ~cCiMMI();
  virtual bool Process(int Length = 0, const uint8_t *Data = NULL);
  virtual bool HasUserIO(void) { return menu || enquiry; }
  cCiMenu *Menu(void);
  cCiEnquiry *Enquiry(void);
  bool SendMenuAnswer(uint8_t Selection);
  bool SendAnswer(const char *Text);
  };

cCiMMI::cCiMMI(int SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_MMI, Tc)
{
  dbgprotocol("New MMI (session id %d)\n", SessionId);
  menu = NULL;
  enquiry = NULL;
}

cCiMMI::~cCiMMI()
{
  delete menu;
  delete enquiry;
}

char *cCiMMI::GetText(int &Length, const uint8_t **Data)
///< Gets the text at Data.
///< \return Returns a pointer to a newly allocated string, or NULL in case of error.
///< Upon return Length and Data represent the remaining data after the text has been skipped.
{
  int Tag = GetTag(Length, Data);
  if (Tag == AOT_TEXT_LAST) {
     char *s = GetString(Length, Data);
     dbgprotocol("%d: <== Text Last '%s'\n", SessionId(), s);
     return s;
     }
  else
     esyslog("CI MMI: unexpected text tag: %06X", Tag);
  return NULL;
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
                         struct tDisplayReply { uint8_t id; uint8_t mode; };
                         tDisplayReply dr = { id : DRI_MMI_MODE_ACK, mode : MM_HIGH_LEVEL };
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
            delete menu;
            menu = new cCiMenu(this, Tag == AOT_MENU_LAST);
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               // since the specification allows choiceNb to be undefined it is useless, so let's just skip it:
               d++;
               l--;
               if (l > 0) menu->titleText = GetText(l, &d);
               if (l > 0) menu->subTitleText = GetText(l, &d);
               if (l > 0) menu->bottomText = GetText(l, &d);
               while (l > 0) {
                     char *s = GetText(l, &d);
                     if (s) {
                        if (!menu->AddEntry(s))
                           free(s);
                        }
                     else
                        break;
                     }
               }
            }
            break;
       case AOT_ENQ: {
            dbgprotocol("%d: <== Enq\n", SessionId());
            delete enquiry;
            enquiry = new cCiEnquiry(this);
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               uint8_t blind = *d++;
               //XXX GetByte()???
               l--;
               enquiry->blind = blind & EF_BLIND;
               enquiry->expectedLength = *d++;
               l--;
               // I really wonder why there is no text length field here...
               enquiry->text = CopyString(l, d);
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
  cCiMenu *m = menu;
  menu = NULL;
  return m;
}

cCiEnquiry *cCiMMI::Enquiry(void)
{
  cCiEnquiry *e = enquiry;
  enquiry = NULL;
  return e;
}

bool cCiMMI::SendMenuAnswer(uint8_t Selection)
{
  dbgprotocol("%d: ==> Menu Answ\n", SessionId());
  SendData(AOT_MENU_ANSW, 1, &Selection);
  //XXX return value of all SendData() calls???
  return true;
}

bool cCiMMI::SendAnswer(const char *Text)
{
  dbgprotocol("%d: ==> Answ\n", SessionId());
  struct tAnswer { uint8_t id; char text[256]; };//XXX
  tAnswer answer;
  answer.id = Text ? AI_ANSWER : AI_CANCEL;
  if (Text) {
     strncpy(answer.text, Text, sizeof(answer.text) - 1);
     answer.text[255] = '\0';
  }
  SendData(AOT_ANSW, Text ? strlen(Text) + 1 : 1, (uint8_t *)&answer);
  //XXX return value of all SendData() calls???
  return true;
}

// --- cCiMenu ---------------------------------------------------------------

cCiMenu::cCiMenu(cCiMMI *MMI, bool Selectable)
{
  mmi = MMI;
  selectable = Selectable;
  titleText = subTitleText = bottomText = NULL;
  numEntries = 0;
  for (int i = 0; i < MAX_CIMENU_ENTRIES; i++)
      entries[i] = NULL;
}

cCiMenu::~cCiMenu()
{
  free(titleText);
  free(subTitleText);
  free(bottomText);
  for (int i = 0; i < numEntries; i++)
      free(entries[i]);
}

bool cCiMenu::AddEntry(char *s)
{
  if (numEntries < MAX_CIMENU_ENTRIES) {
     entries[numEntries++] = s;
     return true;
     }
  return false;
}

bool cCiMenu::Select(int Index)
{
  if (mmi && -1 <= Index && Index < numEntries)
     return mmi->SendMenuAnswer(Index + 1);
  return false;
}

bool cCiMenu::Cancel(void)
{
  return Select(-1);
}

// --- cCiEnquiry ------------------------------------------------------------

cCiEnquiry::cCiEnquiry(cCiMMI *MMI)
{
  mmi = MMI;
  text = NULL;
  blind = false;;
  expectedLength = 0;;
}

cCiEnquiry::~cCiEnquiry()
{
  free(text);
}

bool cCiEnquiry::Reply(const char *s)
{
  return mmi ? mmi->SendAnswer(s) : false;
}

bool cCiEnquiry::Cancel(void)
{
  return Reply(NULL);
}

// --- cCiCaPmt --------------------------------------------------------------

// Ca Pmt Cmd Ids:

#define CPCI_OK_DESCRAMBLING  0x01
#define CPCI_OK_MMI           0x02
#define CPCI_QUERY            0x03
#define CPCI_NOT_SELECTED     0x04

cCiCaPmt::cCiCaPmt(int ProgramNumber, uint8_t cplm)
{
  length = 0;
  capmt[length++] = cplm; // ca_pmt_list_management
  capmt[length++] = (ProgramNumber >> 8) & 0xFF;
  capmt[length++] =  ProgramNumber       & 0xFF;
  capmt[length++] = 0x01; // version_number, current_next_indicator - apparently vn doesn't matter, but cni must be 1

  // program_info_length
  infoLengthPos = length;
  capmt[length++] = 0x00;
  capmt[length++] = 0x00;
}

void cCiCaPmt::AddElementaryStream(int type, int pid)
{
  if (length + 5 > int(sizeof(capmt)))
  {
    esyslog("ERROR: buffer overflow in CA_PMT");
    return;
  }

  capmt[length++] = type & 0xFF;
  capmt[length++] = (pid >> 8) & 0xFF;
  capmt[length++] =  pid       & 0xFF;

  // ES_info_length
  infoLengthPos = length;
  capmt[length++] = 0x00;
  capmt[length++] = 0x00;
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
  if (!infoLengthPos)
  {
    esyslog("ERROR: adding CA descriptor without program/stream!");
    return;
  }

  if (length + data_len + 7 > int(sizeof(capmt)))
  {
    esyslog("ERROR: buffer overflow in CA_PMT");
    return;
  }

  // We are either at start of program descriptors or stream descriptors.
  if (infoLengthPos + 2 == length)
    capmt[length++] = CPCI_OK_DESCRAMBLING; // ca_pmt_cmd_id

  capmt[length++] = 0x09;           // CA descriptor tag
  capmt[length++] = 4 + data_len;   // descriptor length

  capmt[length++] = (ca_system_id >> 8) & 0xFF;
  capmt[length++] = ca_system_id & 0xFF;
  capmt[length++] = (ca_pid >> 8) & 0xFF;
  capmt[length++] = ca_pid & 0xFF;

  if (data_len > 0)
  {
    memcpy(&capmt[length], data, data_len);
    length += data_len;
  }

  // update program_info_length/ES_info_length
  int l = length - infoLengthPos - 2;
  capmt[infoLengthPos] = (l >> 8) & 0xFF;
  capmt[infoLengthPos + 1] = l & 0xFF;
}

// -- cLlCiHandler -------------------------------------------------------------

cLlCiHandler::cLlCiHandler(int Fd, int NumSlots)
{
  numSlots = NumSlots;
  newCaSupport = false;
  hasUserIO = false;
  for (int i = 0; i < MAX_CI_SESSION; i++)
      sessions[i] = NULL;
  tpl = new cCiTransportLayer(Fd, numSlots);
  tc = NULL;
  fdCa = Fd;
  needCaPmt = false;
}

cLlCiHandler::~cLlCiHandler()
{
  cMutexLock MutexLock(&mutex);
  for (int i = 0; i < MAX_CI_SESSION; i++)
    if (sessions[i] != NULL)
      delete sessions[i];
  delete tpl;
  close(fdCa);
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
                else if (Caps.slot_type & CA_CI)
                    return new cHlCiHandler(fd_ca, NumSlots);
                else
                    isyslog("CAM doesn't support either high or low level CI,"
                            " Caps.slot_type=%i", Caps.slot_type);
            }
            else
                esyslog("ERROR: no CAM slots found");
        }
        else
            LOG_ERROR_STR(FileName);
        close(fd_ca);
    }
    return NULL;
}

int cLlCiHandler::ResourceIdToInt(const uint8_t *Data)
{
  return (ntohl(*(int *)Data));
}

bool cLlCiHandler::Send(uint8_t Tag, int SessionId, int ResourceId, int Status)
{
  uint8_t buffer[16];
  uint8_t *p = buffer;
  *p++ = Tag;
  *p++ = 0x00; // will contain length
  if (Status >= 0)
     *p++ = Status;
  if (ResourceId) {
     *(int *)p = htonl(ResourceId);
     p += 4;
     }
  *(short *)p = htons(SessionId);
  p += 2;
  buffer[1] = p - buffer - 2; // length
  return tc && tc->SendData(p - buffer, buffer) == OK;
}

cCiSession *cLlCiHandler::GetSessionBySessionId(int SessionId)
{
  for (int i = 0; i < MAX_CI_SESSION; i++) {
      if (sessions[i] && sessions[i]->SessionId() == SessionId)
         return sessions[i];
      }
  return NULL;
}

cCiSession *cLlCiHandler::GetSessionByResourceId(int ResourceId, int Slot)
{
  for (int i = 0; i < MAX_CI_SESSION; i++) {
      if (sessions[i] && sessions[i]->Tc()->Slot() == Slot && sessions[i]->ResourceId() == ResourceId)
         return sessions[i];
      }
  return NULL;
}

cCiSession *cLlCiHandler::CreateSession(int ResourceId)
{
  if (!GetSessionByResourceId(ResourceId, tc->Slot())) {
     for (int i = 0; i < MAX_CI_SESSION; i++) {
         if (!sessions[i]) {
            switch (ResourceId) {
              case RI_RESOURCE_MANAGER:           return sessions[i] = new cCiResourceManager(i + 1, tc);
              case RI_APPLICATION_INFORMATION:    return sessions[i] = new cCiApplicationInformation(i + 1, tc);
              case RI_CONDITIONAL_ACCESS_SUPPORT: newCaSupport = true;
                                                  return sessions[i] = new cCiConditionalAccessSupport(i + 1, tc);
              case RI_HOST_CONTROL:               break; //XXX
              case RI_DATE_TIME:                  return sessions[i] = new cCiDateTime(i + 1, tc);
              case RI_MMI:                        return sessions[i] = new cCiMMI(i + 1, tc);
              }
            }
         }
     }
  return NULL;
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
  if (Session && sessions[SessionId - 1] == Session) {
     delete Session;
     sessions[SessionId - 1] = NULL;
     Send(ST_CLOSE_SESSION_RESPONSE, SessionId, 0, SS_OK);
     return true;
     }
  else {
     esyslog("ERROR: unknown session id: %d", SessionId);
     Send(ST_CLOSE_SESSION_RESPONSE, SessionId, 0, SS_NOT_ALLOCATED);
     }
  return false;
}

int cLlCiHandler::CloseAllSessions(int Slot)
{
  int result = 0;
  for (int i = 0; i < MAX_CI_SESSION; i++) {
      if (sessions[i] && sessions[i]->Tc()->Slot() == Slot) {
         CloseSession(sessions[i]->SessionId());
         result++;
         }
      }
  return result;
}

bool cLlCiHandler::Process(void)
{
    bool result = true;
    cMutexLock MutexLock(&mutex);

    for (int Slot = 0; Slot < numSlots; Slot++)
    {
        tc = tpl->Process(Slot);
        if (tc)
        {
            int Length;
            const uint8_t *Data = tc->Data(Length);
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
                                esyslog("ERROR: unknown session id: %d", SessionId);
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
            tpl->ResetSlot(Slot);
            result = false;
        }
        else if (tpl->ModuleReady(Slot))
        {
            dbgprotocol("Module ready in slot %d\n", Slot);
            tpl->NewConnection(Slot);
        }
    }

    bool UserIO = false;
    needCaPmt = false;
    for (int i = 0; i < MAX_CI_SESSION; i++)
    {
        if (sessions[i] && sessions[i]->Process())
        {
            UserIO |= sessions[i]->HasUserIO();
            if (sessions[i]->ResourceId() == RI_CONDITIONAL_ACCESS_SUPPORT)
            {
                cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *) sessions[i];
                needCaPmt |= cas->NeedCaPmt();
            }
        }
    }
    hasUserIO = UserIO;

    if (newCaSupport)
        newCaSupport = result = false; // triggers new SetCaPmt at caller!
    return result;
}

bool cLlCiHandler::EnterMenu(int Slot)
{
  cMutexLock MutexLock(&mutex);
  cCiApplicationInformation *api = (cCiApplicationInformation *)GetSessionByResourceId(RI_APPLICATION_INFORMATION, Slot);
  return api ? api->EnterMenu() : false;
}

cCiMenu *cLlCiHandler::GetMenu(void)
{
  cMutexLock MutexLock(&mutex);
  for (int Slot = 0; Slot < numSlots; Slot++) {
      cCiMMI *mmi = (cCiMMI *)GetSessionByResourceId(RI_MMI, Slot);
      if (mmi)
         return mmi->Menu();
      }
  return NULL;
}

cCiEnquiry *cLlCiHandler::GetEnquiry(void)
{
  cMutexLock MutexLock(&mutex);
  for (int Slot = 0; Slot < numSlots; Slot++) {
      cCiMMI *mmi = (cCiMMI *)GetSessionByResourceId(RI_MMI, Slot);
      if (mmi)
         return mmi->Enquiry();
      }
  return NULL;
}

const unsigned short *cLlCiHandler::GetCaSystemIds(int Slot)
 {
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT, Slot);
  return cas ? cas->GetCaSystemIds() : NULL;
}

bool cLlCiHandler::SetCaPmt(cCiCaPmt &CaPmt, int Slot)
{
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT, Slot);
  return cas && cas->SendPMT(CaPmt);
}

void cLlCiHandler::SetTimeOffset(double offset_in_seconds)
{
    cMutexLock MutexLock(&mutex);
    cCiDateTime *dt = NULL;

    for (uint i = 0; i < (uint) NumSlots(); i++)
    {
        dt = (cCiDateTime*) GetSessionByResourceId(RI_DATE_TIME, i);
        if (dt)
            dt->SetTimeOffset(offset_in_seconds);
    }
}

bool cLlCiHandler::Reset(int Slot)
{
  cMutexLock MutexLock(&mutex);
  CloseAllSessions(Slot);
  return tpl->ResetSlot(Slot);
}

bool cLlCiHandler::connected() const
{
  return _connected;
}

// -- cHlCiHandler -------------------------------------------------------------

cHlCiHandler::cHlCiHandler(int Fd, int NumSlots)
{
    numSlots = NumSlots;
    numCaSystemIds = 0;
    caSystemIds[0] = 0;
    fdCa = Fd;
    state = 0;
    esyslog("New High level CI handler");
}

cHlCiHandler::~cHlCiHandler()
{
    cMutexLock MutexLock(&mutex);
    close(fdCa);
}

int cHlCiHandler::CommHL(unsigned tag, unsigned function, struct ca_msg *msg)
{
    if (tag) {
        msg->msg[2] = tag & 0xff;
        msg->msg[1] = (tag & 0xff00) >> 8;
        msg->msg[0] = (tag & 0xff0000) >> 16;
        esyslog("Sending message=[%02x %02x %02x ]",
                       msg->msg[0], msg->msg[1], msg->msg[2]);
    }

    return ioctl(fdCa, function, msg);
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
    cMutexLock MutexLock(&mutex);

    struct ca_msg msg;
    switch(state) {
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
                    if (numCaSystemIds < MAXCASYSTEMIDS) {
                        caSystemIds[numCaSystemIds++] = id;
                        caSystemIds[numCaSystemIds] = 0;
                    }
                    else
                        esyslog("ERROR: too many CA system IDs!");
                }
                dbgprotocol("\n");
            }
            state = 1;
            break;
        }
    }

    bool result = true;

    return result;
}

bool cHlCiHandler::EnterMenu(int)
{
    return false;
}

cCiMenu *cHlCiHandler::GetMenu(void)
{
    return NULL;
}

cCiEnquiry *cHlCiHandler::GetEnquiry(void)
{
    return NULL;
}

const unsigned short *cHlCiHandler::GetCaSystemIds(int)
{
    return caSystemIds;
}

bool cHlCiHandler::SetCaPmt(cCiCaPmt &CaPmt, int)
{
    cMutexLock MutexLock(&mutex);
    struct ca_msg msg;

    esyslog("Setting CA PMT.");
    state = 2;

    msg.msg[3] = CaPmt.length;

    if (CaPmt.length > (256 - 4))
    {
        esyslog("CA message too long");
        return false;
    }

    memcpy(&msg.msg[4], CaPmt.capmt, CaPmt.length);

    if ((SendData(AOT_CA_PMT, &msg)) < 0) {
        esyslog("HLCI communication failed");
        return false;
    }

    return true;
}

bool cHlCiHandler::Reset(int)
{
    if ((ioctl(fdCa, CA_RESET)) < 0) {
        esyslog("ioctl CA_RESET failed.");
        return false;
    }
    return true;
}

bool cHlCiHandler::NeedCaPmt(void)
{
    if(state == 1)
        return true;

    return false;
}
