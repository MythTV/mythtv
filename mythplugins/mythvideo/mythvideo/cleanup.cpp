#include <list>
#include <algorithm>

#include "cleanup.h"

#include <mythtv/mythcontext.h>

CleanupProc::~CleanupProc()
{
}

class CleanupHooksImp
{
  private:
    typedef std::list<CleanupProc *> clean_list;

  private:
    clean_list m_clean_list;

  public:
    void addHook(CleanupProc *clean_proc)
    {
        m_clean_list.push_back(clean_proc);
    }

    void removeHook(CleanupProc *clean_proc)
    {
        clean_list::iterator p = std::find(m_clean_list.begin(),
                                           m_clean_list.end(), clean_proc);
        if (p != m_clean_list.end())
        {
            m_clean_list.erase(p);
        }
    }

    void cleanup()
    {
        for (clean_list::iterator p = m_clean_list.begin();
             p != m_clean_list.end();++p)
        {
            (*p)->doClean();
        }
        m_clean_list.clear();
    }
};

namespace
{
    CleanupHooks *g_cleanup_hooks = 0;
}

CleanupHooks *CleanupHooks::getInstance()
{
    if (!g_cleanup_hooks)
    {
        g_cleanup_hooks = new CleanupHooks();
    }
    return g_cleanup_hooks;
}

void CleanupHooks::addHook(CleanupProc *clean_proc)
{
    m_imp->addHook(clean_proc);
}

void CleanupHooks::removeHook(CleanupProc *clean_proc)
{
    m_imp->removeHook(clean_proc);
}

void CleanupHooks::cleanup()
{
    m_imp->cleanup();
    delete g_cleanup_hooks;
    g_cleanup_hooks = 0;
}

CleanupHooks::CleanupHooks()
{
    m_imp = new CleanupHooksImp();
}

CleanupHooks::~CleanupHooks()
{
    delete m_imp;
}
