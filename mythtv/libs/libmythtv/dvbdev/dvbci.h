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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 * The author can be reached at kls@cadsoft.de
 *
 * The project's page is at http://www.cadsoft.de/people/kls/vdr
 *
 */

#ifndef __CI_H
#define __CI_H

#if HAVE_STDINT_H
#include <stdint.h>
#endif 

#include <stdio.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/uio.h>

#define MAXCASYSTEMIDS 64

class cMutex {
  friend class cCondVar;
private:
  pthread_mutex_t mutex;
  pid_t lockingPid;
  int locked;
public:
  cMutex(void);
  ~cMutex();
  void Lock(void);
  void Unlock(void);
  };

class cMutexLock {
private:
  cMutex *mutex;
  bool locked;
public:
  cMutexLock(cMutex *Mutex = NULL);
  ~cMutexLock();
  bool Lock(cMutex *Mutex);
  };


class cCiMMI;

class cCiMenu {
  friend class cCiMMI;
private:
  enum { MAX_CIMENU_ENTRIES = 64 }; ///< XXX is there a specified maximum?
  cCiMMI *mmi;
  bool selectable;
  char *titleText;
  char *subTitleText;
  char *bottomText;
  char *entries[MAX_CIMENU_ENTRIES];
  int numEntries;
  bool AddEntry(char *s);
  cCiMenu(cCiMMI *MMI, bool Selectable);
public:
  ~cCiMenu();
  const char *TitleText(void) { return titleText; }
  const char *SubTitleText(void) { return subTitleText; }
  const char *BottomText(void) { return bottomText; }
  const char *Entry(int n) { return n < numEntries ? entries[n] : NULL; }
  int NumEntries(void) { return numEntries; }
  bool Selectable(void) { return selectable; }
  bool Select(int Index);
  bool Cancel(void);
  };

class cCiEnquiry {
  friend class cCiMMI;
private:
  cCiMMI *mmi;
  char *text;
  bool blind;
  int expectedLength;
  cCiEnquiry(cCiMMI *MMI);
public:
  ~cCiEnquiry();
  const char *Text(void) { return text; }
  bool Blind(void) { return blind; }
  int ExpectedLength(void) { return expectedLength; }
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
  int length;
  int infoLengthPos;
  uint8_t capmt[2048]; ///< XXX is there a specified maximum?
public:
  cCiCaPmt(int ProgramNumber, uint8_t cplm = CPLM_ONLY);
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
  virtual ~cCiHandler() {};
  static cCiHandler *CreateCiHandler(const char *FileName);
  virtual int NumSlots(void) = 0;
  virtual bool Process(void) = 0;
  virtual bool HasUserIO(void) = 0;
  virtual bool NeedCaPmt(void) = 0;
  virtual bool EnterMenu(int Slot) = 0;
  virtual cCiMenu *GetMenu(void) = 0;
  virtual cCiEnquiry *GetEnquiry(void) = 0;
  virtual const unsigned short *GetCaSystemIds(int Slot) = 0;
  virtual bool SetCaPmt(cCiCaPmt &CaPmt, int Slot) = 0;
  virtual void SetTimeOffset(double /*offset_in_seconds*/) { }
  };

class cLlCiHandler : public cCiHandler {
  friend class cCiHandler;
private:
  cMutex mutex;
  int fdCa;
  int numSlots;
  bool newCaSupport;
  bool hasUserIO;
  bool needCaPmt;
  cCiSession *sessions[MAX_CI_SESSION];
  cCiTransportLayer *tpl;
  cCiTransportConnection *tc;
  int ResourceIdToInt(const uint8_t *Data);
  bool Send(uint8_t Tag, int SessionId, int ResourceId = 0, int Status = -1);
  cCiSession *GetSessionBySessionId(int SessionId);
  cCiSession *GetSessionByResourceId(int ResourceId, int Slot);
  cCiSession *CreateSession(int ResourceId);
  bool OpenSession(int Length, const uint8_t *Data);
  bool CloseSession(int SessionId);
  int CloseAllSessions(int Slot);
  cLlCiHandler(int Fd, int NumSlots);
public:
  virtual ~cLlCiHandler();
  int NumSlots(void) { return numSlots; }
  bool Process(void);
  bool HasUserIO(void) { return hasUserIO; }
  bool NeedCaPmt(void) { return needCaPmt; }
  bool EnterMenu(int Slot);
  cCiMenu *GetMenu(void);
  cCiEnquiry *GetEnquiry(void);
  bool SetCaPmt(cCiCaPmt &CaPmt);
  const unsigned short *GetCaSystemIds(int Slot);
  bool SetCaPmt(cCiCaPmt &CaPmt, int Slot);
  void SetTimeOffset(double offset_in_seconds);
  bool Reset(int Slot);
  bool connected() const;
  };

class cHlCiHandler : public cCiHandler {
    friend class cCiHandler;
  private:
    cMutex mutex;
    int fdCa;
    int numSlots;
    int state;
    int numCaSystemIds;
    unsigned short caSystemIds[MAXCASYSTEMIDS + 1]; // list is zero terminated!
    cHlCiHandler(int Fd, int NumSlots);
    int CommHL(unsigned tag, unsigned function, struct ca_msg *msg);
    int GetData(unsigned tag, struct ca_msg *msg);
    int SendData(unsigned tag, struct ca_msg *msg);
  public:
    virtual ~cHlCiHandler();
    int NumSlots(void) { return numSlots; }
    bool Process(void);
    bool HasUserIO(void) { return false; }//hasUserIO; }
    bool NeedCaPmt(void);
    bool EnterMenu(int Slot);
    cCiMenu *GetMenu(void);
    cCiEnquiry *GetEnquiry(void);
    bool SetCaPmt(cCiCaPmt &CaPmt);
    const unsigned short *GetCaSystemIds(int Slot);
    bool SetCaPmt(cCiCaPmt &CaPmt, int Slot);
    bool Reset(int Slot);
    bool connected() const;
};

int tcp_listen(struct sockaddr_in *name,int sckt,unsigned long address=INADDR_ANY);
int accept_tcp(int ip_sock,struct sockaddr_in *ip_name);
int udp_listen(struct sockaddr_un *name,char const * const filename);
int accept_udp(int ip_sock,struct sockaddr_un *ip_name);

#endif //__CI_H
