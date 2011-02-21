#ifndef CLEANUP_H_
#define CLEANUP_H_

#include "mythmetaexp.h"

class META_PUBLIC CleanupProc
{
  public:
    virtual void doClean() = 0;
    virtual ~CleanupProc();
};

class META_PUBLIC CleanupHooks
{
  public:
    static CleanupHooks *getInstance();

  public:
    void addHook(CleanupProc *clean_proc);
    void removeHook(CleanupProc *clean_proc);
    void cleanup();

  private:
    CleanupHooks();
    ~CleanupHooks();
    class CleanupHooksImp *m_imp;
};

template <typename T>
class META_PUBLIC SimpleCleanup : public CleanupProc
{
  public:
    SimpleCleanup(T *inst) : m_inst(inst)
    {
        CleanupHooks::getInstance()->addHook(this);
    }

    ~SimpleCleanup()
    {
        CleanupHooks::getInstance()->removeHook(this);
    }

    void doClean()
    {
        m_inst->cleanup();
    }

  private:
    T *m_inst;
};

#endif // CLEANUP_H_
