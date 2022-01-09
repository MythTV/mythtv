/*
 * ci.hh: Common Interface
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

#ifndef DVBCI_H
#define DVBCI_H

#include <cstdint>
#include <cstdio>
#include <vector>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "libmythbase/mythchrono.h"

using dvbca_vector = std::vector<uint16_t>;

class cMutex {
  friend class cCondVar;
private:
  pthread_mutex_t m_mutex      {};
  pid_t           m_lockingPid {0};
  int             m_locked     {0};
public:
  cMutex(void) { pthread_mutex_init(&m_mutex, nullptr); }
  ~cMutex() { pthread_mutex_destroy(&m_mutex); }
  void Lock(void);
  void Unlock(void);
  };

class cMutexLock {
private:
  cMutex *m_mutex  {nullptr};
  bool    m_locked {false};
public:
    explicit cMutexLock(cMutex *Mutex = nullptr) { Lock(Mutex); }
  ~cMutexLock();
  bool Lock(cMutex *Mutex);
  };


class cCiMMI;

class cCiMenu {
  friend class cCiMMI;
private:
  enum { MAX_CIMENU_ENTRIES = 64 }; ///< XXX is there a specified maximum?
  cCiMMI *m_mmi          {nullptr};
  bool    m_selectable;
  char   *m_titleText    {nullptr};
  char   *m_subTitleText {nullptr};
  char   *m_bottomText   {nullptr};
  char   *m_entries[MAX_CIMENU_ENTRIES] {};
  int     m_numEntries   {0};
  bool AddEntry(char *s);
  cCiMenu(cCiMMI *MMI, bool Selectable);
public:
  ~cCiMenu();
  const char *TitleText(void) { return m_titleText; }
  const char *SubTitleText(void) { return m_subTitleText; }
  const char *BottomText(void) { return m_bottomText; }
  const char *Entry(int n) { return n < m_numEntries ? m_entries[n] : nullptr; }
  int NumEntries(void) const { return m_numEntries; }
  bool Selectable(void) const { return m_selectable; }
  bool Select(int Index);
  bool Cancel(void);
  };

class cCiEnquiry {
  friend class cCiMMI;
private:
  cCiMMI *m_mmi            {nullptr};
  char   *m_text           {nullptr};
  bool    m_blind          {false};
  int     m_expectedLength {0};
  explicit cCiEnquiry(cCiMMI *MMI) : m_mmi(MMI) {}
public:
  ~cCiEnquiry();
  const char *Text(void) { return m_text; }
  bool Blind(void) const { return m_blind; }
  int ExpectedLength(void) const { return m_expectedLength; }
  bool Reply(const char *s);
  bool Cancel(void);
  };

// Ca Pmt List Management:

#define CPLM_MORE    0x00
#define CPLM_FIRST   0x01
#define CPLM_LAST    0x02
#define CPLM_ONLY    0x03
#define CPLM_ADD     0x04
#define CPLM_UPDATE  0x05

class cCiCaPmt {
  friend class cCiConditionalAccessSupport;
  friend class cHlCiHandler;
private:
  int     m_length        {0};
  int     m_infoLengthPos {0};
  uint8_t m_capmt[2048]   {0}; ///< XXX is there a specified maximum?
public:
  explicit cCiCaPmt(int ProgramNumber, uint8_t cplm = CPLM_ONLY);
  void AddElementaryStream(int type, int pid);
  void AddCaDescriptor(int ca_system_id, int ca_pid, int data_len,
                       const uint8_t *data);
  };

#define MAX_CI_SESSION  16 //XXX

class cCiSession;
class cCiTransportLayer;
class cCiTransportConnection;

class cCiHandler {
public:
  virtual ~cCiHandler() = default;
  static cCiHandler *CreateCiHandler(const char *FileName);
  virtual int NumSlots(void) = 0;
  virtual bool Process(void) = 0;
  virtual bool HasUserIO(void) = 0;
  virtual bool NeedCaPmt(void) = 0;
  virtual bool EnterMenu(int Slot) = 0;
  virtual cCiMenu *GetMenu(void) = 0;
  virtual cCiEnquiry *GetEnquiry(void) = 0;
  virtual dvbca_vector GetCaSystemIds(int Slot) = 0;
  virtual bool SetCaPmt(cCiCaPmt &CaPmt, int Slot) = 0;
  virtual void SetTimeOffset(double /*offset_in_seconds*/) { }
  };

class cLlCiHandler : public cCiHandler {
  friend class cCiHandler;
private:
  cMutex                  m_mutex;
  int                     m_fdCa;
  int                     m_numSlots;
  bool                    m_newCaSupport {false};
  bool                    m_hasUserIO    {false};
  bool                    m_needCaPmt    {false};
  cCiSession             *m_sessions[MAX_CI_SESSION] {};
  cCiTransportLayer      *m_tpl          {nullptr};
  cCiTransportConnection *m_tc           {nullptr};
  static int ResourceIdToInt(const uint8_t *Data);
  bool Send(uint8_t Tag, int SessionId, int ResourceId = 0, int Status = -1);
  cCiSession *GetSessionBySessionId(int SessionId);
  cCiSession *GetSessionByResourceId(int ResourceId, int Slot);
  cCiSession *CreateSession(int ResourceId);
  bool OpenSession(int Length, const uint8_t *Data);
  bool CloseSession(int SessionId);
  int CloseAllSessions(int Slot);
  cLlCiHandler(int Fd, int NumSlots);
public:
  ~cLlCiHandler() override;
  cLlCiHandler(const cLlCiHandler &) = delete;            // not copyable
  cLlCiHandler &operator=(const cLlCiHandler &) = delete; // not copyable
  int NumSlots(void) override // cCiHandler
      { return m_numSlots; }
  bool Process(void) override; // cCiHandler
  bool HasUserIO(void) override // cCiHandler
      { return m_hasUserIO; }
  bool NeedCaPmt(void) override // cCiHandler
      { return m_needCaPmt; }
  bool EnterMenu(int Slot) override; // cCiHandler
  cCiMenu *GetMenu(void) override; // cCiHandler
  cCiEnquiry *GetEnquiry(void) override; // cCiHandler
  bool SetCaPmt(cCiCaPmt &CaPmt);
  dvbca_vector GetCaSystemIds(int Slot) override; // cCiHandler
  bool SetCaPmt(cCiCaPmt &CaPmt, int Slot) override; // cCiHandler
  void SetTimeOffset(double offset_in_seconds) override; // cCiHandler
  bool Reset(int Slot);
  static bool connected();
  };

class cHlCiHandler : public cCiHandler {
    friend class cCiHandler;
  private:
    cMutex         m_mutex;
    int            m_fdCa;
    int            m_numSlots;
    int            m_state          {0};
    int            m_numCaSystemIds {0};
    dvbca_vector   m_caSystemIds    {};
    cHlCiHandler(int Fd, int NumSlots);
    int CommHL(unsigned tag, unsigned function, struct ca_msg *msg) const;
    int GetData(unsigned tag, struct ca_msg *msg);
    int SendData(unsigned tag, struct ca_msg *msg);
  public:
    ~cHlCiHandler() override;
    int NumSlots(void) override // cCiHandler
        { return m_numSlots; }
    bool Process(void) override; // cCiHandler
    bool HasUserIO(void) override { return false; } // cCiHandler
    bool NeedCaPmt(void) override; // cCiHandler
    bool EnterMenu(int Slot) override; // cCiHandler
    cCiMenu *GetMenu(void) override; // cCiHandler
    cCiEnquiry *GetEnquiry(void) override; // cCiHandler
    bool SetCaPmt(cCiCaPmt &CaPmt);
    dvbca_vector GetCaSystemIds(int Slot) override; // cCiHandler
    bool SetCaPmt(cCiCaPmt &CaPmt, int Slot) override; // cCiHandler
    bool Reset(int Slot) const;
    bool connected() const;
};

int tcp_listen(struct sockaddr_in *name,int sckt,unsigned long address=INADDR_ANY);
int accept_tcp(int ip_sock,struct sockaddr_in *ip_name);
int udp_listen(struct sockaddr_un *name,char const * filename);
int accept_udp(int ip_sock,struct sockaddr_un *ip_name);

#endif // DVBCI_H
