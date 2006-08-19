#ifndef CLEANUP_H_
#define CLEANUP_H_

class CleanupProc
{
  public:
    virtual void doClean() = 0;
    virtual ~CleanupProc();
};

class CleanupHooks
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

#endif // CLEANUP_H_
