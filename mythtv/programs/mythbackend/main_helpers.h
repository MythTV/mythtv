#ifndef _MAIN_HELPERS_H_
#define _MAIN_HELPERS_H_

// C++ headers
#include <iostream>
#include <fstream>
using namespace std;

class MythBackendCommandLineParser;
class QString;
class QSize;

bool setupTVs(bool ismaster, bool &error);
void cleanup(void);
int  handle_command(const MythBackendCommandLineParser &cmdline);
int  connect_to_master(void);
void print_warnings(const MythBackendCommandLineParser &cmdline);
int  run_backend(MythBackendCommandLineParser &cmdline);

namespace
{
    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
            ReferenceCounter::PrintDebug();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

#endif // _MAIN_HELPERS_H_
