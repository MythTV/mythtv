#ifndef OMXCONTEXT_H
#define OMXCONTEXT_H

#include <cstring> // memset

#include <OMX_Component.h>
#include <OMX_Image.h>
#include <OMX_Video.h>
#ifdef USING_BROADCOM
#include <OMX_Broadcom.h>
#endif
#ifdef USING_BELLAGIO
#include <bellagio/omxcore.h>
#endif

#include <QString>
#include <QVector>
#include <QMutex>
#include <QSemaphore>

#include "mythexp.h"
#include "mythlogging.h"

class OMXComponentCtx;
class OMXComponentAbstractCB;

class MPUBLIC OMXComponent
{
    friend class OMXComponentCtx;

    // No copying
    OMXComponent(OMXComponent&);
    OMXComponent& operator =(OMXComponent &rhs);

  public:
    // NB Shutdown() must be called before OMXComponentCtx is invalidated
    OMXComponent(const QString &name, OMXComponentCtx &);
    virtual ~OMXComponent();

    OMX_ERRORTYPE Init(OMX_INDEXTYPE); // OMX_IndexParamVideoInit etc
    void Shutdown();

    bool IsValid() const { return m_state >= OMX_StateLoaded && Ports() > 0; }
    OMX_HANDLETYPE Handle() const { return m_handle; }
    operator OMX_HANDLETYPE () const { return m_handle; }
    int Id() const { return m_ref; }

    OMX_U32 Base() const { return m_base; }
    unsigned Ports() const { return unsigned(m_portdefs.size()); }
    OMX_ERRORTYPE GetPortDef(unsigned index = 0);
    const OMX_PARAM_PORTDEFINITIONTYPE &PortDef(unsigned index = 0) const;
    OMX_PARAM_PORTDEFINITIONTYPE &PortDef(unsigned index = 0);
    void ShowPortDef(unsigned index = 0, LogLevel_t = LOG_INFO, uint64_t = VB_PLAYBACK) const;
    void ShowFormats(unsigned index = 0, LogLevel_t = LOG_INFO, uint64_t = VB_PLAYBACK) const;

    OMX_ERRORTYPE SendCommand(OMX_COMMANDTYPE cmd, OMX_U32 nParam = 0,
            void *pCmdData = 0, int ms = -1, OMXComponentAbstractCB *cb = 0);
    bool IsCommandComplete() const { return m_state_sema.available() > 0; }
    OMX_ERRORTYPE WaitComplete(int ms = -1);
    OMX_ERRORTYPE LastError();

    OMX_ERRORTYPE SetState(OMX_STATETYPE state, int ms = -1,
            OMXComponentAbstractCB *cb = 0);
    OMX_STATETYPE GetState();

    OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE type, OMX_PTR p) {
        return OMX_SetParameter(m_handle, type, p); }
    OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE type, OMX_PTR p) const {
        return OMX_GetParameter(m_handle, type, p); }

    OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE type, OMX_PTR p) {
        return OMX_SetConfig(m_handle, type, p); }
    OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE type, OMX_PTR p) const {
        return OMX_GetConfig(m_handle, type, p); }

    OMX_ERRORTYPE PortEnable(unsigned index = 0, int ms = -1,
            OMXComponentAbstractCB *cb = 0) {
        return SendCommand(OMX_CommandPortEnable, Base() + index, 0, ms, cb); }
    OMX_ERRORTYPE PortDisable(unsigned index = 0, int ms = -1,
            OMXComponentAbstractCB *cb = 0) {
        return SendCommand(OMX_CommandPortDisable, Base() + index, 0, ms, cb); }

    static void EnumComponents(const QRegExp &re, LogLevel_t level = LOG_INFO,
                                uint64_t mask = VB_PLAYBACK)
        { GetComponent(0, re, level, mask); }

  private:
    static OMX_ERRORTYPE GetComponent(OMXComponent*, const QRegExp&,
                    LogLevel_t level = LOG_INFO, uint64_t mask = VB_PLAYBACK);
    static bool IncrRef();
    static void DecrRef();

    static OMX_ERRORTYPE EventCB(OMX_IN OMX_HANDLETYPE, OMX_IN OMX_PTR,
        OMX_IN OMX_EVENTTYPE, OMX_IN OMX_U32, OMX_IN OMX_U32, OMX_IN OMX_PTR);
    static OMX_ERRORTYPE EmptyBufferCB(OMX_IN OMX_HANDLETYPE, OMX_IN OMX_PTR,
        OMX_IN OMX_BUFFERHEADERTYPE *);
    static OMX_ERRORTYPE FillBufferCB(OMX_IN OMX_HANDLETYPE, OMX_IN OMX_PTR,
        OMX_IN OMX_BUFFERHEADERTYPE *);

    OMX_ERRORTYPE ReleaseBuffers();

  private:
    int const m_ref;
    OMXComponentCtx &m_cb;
    OMX_HANDLETYPE m_handle;    // OMX component handle
    OMX_U32 m_base;             // OMX port base index
    QVector<OMX_PARAM_PORTDEFINITIONTYPE> m_portdefs;
    QMutex mutable m_state_lock;
    OMX_STATETYPE m_state;
    OMX_ERRORTYPE m_error;
    QSemaphore m_state_sema;    // EventCB signal
};

// Mix-in class for creators of OMXComponent
class MPUBLIC OMXComponentCtx
{
  public:
    // OpenMAX callback handlers, override as required
    virtual OMX_ERRORTYPE Event(OMXComponent&, OMX_EVENTTYPE, OMX_U32, OMX_U32, OMX_PTR);
    virtual OMX_ERRORTYPE EmptyBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE*);
    virtual OMX_ERRORTYPE FillBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE*);
    // Called from OMXComponent::Shutdown to release OpenMAX buffers
    virtual void ReleaseBuffers(OMXComponent&) {}
};

// Interface class to handle callbacks from OMXComponent::SendCommand
class MPUBLIC OMXComponentAbstractCB
{
  public:
    virtual ~OMXComponentAbstractCB() {}
    virtual OMX_ERRORTYPE Action(OMXComponent*) = 0;
};

// Template class for OMXComponent::SendCommand callbacks
// Usage:
// OMX_ERRORTYPE mytype::callback() { ... }
// OMXComponentCB<mytype> cb(this, &mytype::callback);
// e = cmpnt.SetState(OMX_StateIdle, 0, &cb);
template<typename T>
class MPUBLIC OMXComponentCB : public OMXComponentAbstractCB
{
  public:
    OMXComponentCB( T *inst, OMX_ERRORTYPE (T::*cb)() ) :
        m_inst(inst), m_cb(cb) {}

    virtual OMX_ERRORTYPE Action(OMXComponent *) { return  (m_inst->*m_cb)(); }

  protected:
    T * const m_inst;
    OMX_ERRORTYPE (T::* const m_cb)();
};

namespace omxcontext {

#ifndef OMX_VERSION
// For Bellagio.  Assume little endian architecture
#define OMX_VERSION (((SPECSTEP)<<24) | ((SPECREVISION)<<16) | ((SPECVERSIONMINOR)<<8) | (SPECVERSIONMAJOR))
#endif

// Initialize an OpenMAX struct
template<typename T> inline void OMX_DATA_INIT(T &s)
{
    std::memset(&s, 0, sizeof s);
    s.nSize = sizeof s;
#ifdef OMX_VERSION
    s.nVersion.nVersion = OMX_VERSION;
#else
    s.nVersion.s.nVersionMajor = SPECVERSIONMAJOR;
    s.nVersion.s.nVersionMinor = SPECVERSIONMINOR;
    s.nVersion.s.nRevision = SPECREVISION;
    s.nVersion.s.nStep = SPECSTEP;
#endif
}

/*
 * Convert OMX_TICKS <> int64_t
 */
#ifndef OMX_SKIP64BIT
# define TICKS_TO_S64(n) (n)
# define S64_TO_TICKS(n) (n)
#else
static inline int64_t TICKS_TO_S64(OMX_TICKS ticks)
{
    return ticks.nLowPart + (int64_t(ticks.nHighPart) << 32);
}
static inline OMX_TICKS S64_TO_TICKS(int64_t n)
{
    OMX_TICKS ticks;
    ticks.nLowPart = OMX_U32(n);
    ticks.nHighPart = OMX_U32(n >> 32);
    return ticks;
}
#endif

MPUBLIC const char *Coding2String(OMX_VIDEO_CODINGTYPE);
MPUBLIC const char *Coding2String(OMX_IMAGE_CODINGTYPE);
MPUBLIC const char *Coding2String(OMX_AUDIO_CODINGTYPE);
MPUBLIC const char *Other2String(OMX_OTHER_FORMATTYPE);
MPUBLIC const char *Format2String(OMX_COLOR_FORMATTYPE);
MPUBLIC const char *Event2String(OMX_EVENTTYPE);
MPUBLIC const char *Error2String(OMX_ERRORTYPE);
MPUBLIC const char *State2String(OMX_STATETYPE);
MPUBLIC const char *Command2String(OMX_COMMANDTYPE);
MPUBLIC QString HeaderFlags(OMX_U32 nFlags);
#ifdef USING_BROADCOM
MPUBLIC const char *Interlace2String(OMX_INTERLACETYPE);
#endif
MPUBLIC const char *Filter2String(OMX_IMAGEFILTERTYPE);
} // namespace omxcontext

#endif // ndef OMXCONTEXT_H
