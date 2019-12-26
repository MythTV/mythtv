#ifndef CLEANUP_H_
#define CLEANUP_H_

#include "mythmetaexp.h"

class META_PUBLIC CleanupProc
{
  public:
    virtual void doClean() = 0;
    virtual ~CleanupProc() = default;
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
    class CleanupHooksImp *m_imp {nullptr};
};

template <typename T>
class META_PUBLIC SimpleCleanup : public CleanupProc
{
  public:
    explicit SimpleCleanup(T *inst) : m_inst(inst)
    {
        CleanupHooks::getInstance()->addHook(this);
    }

    ~SimpleCleanup() override
    {
        CleanupHooks::getInstance()->removeHook(this);
    }

    void doClean() override // CleanupProc
    {
        m_inst->cleanup();
    }

  private:
    T *m_inst {nullptr};
};

#endif // CLEANUP_H_
