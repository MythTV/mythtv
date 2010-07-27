#ifndef _MAIN_HELPERS_H_
#define _MAIN_HELPERS_H_

// C++ headers
#include <iostream>
#include <fstream>
using namespace std;

class MythCommandLineParser;
class QString;
class QSize;

bool setupTVs(bool ismaster, bool &error);
bool setup_context(const MythCommandLineParser &cmdline);
void cleanup(void);
int log_rotate(int report_error);
void log_rotate_handler(int);
void upnp_rebuild(int);
void showUsage(const MythCommandLineParser &cmdlineparser,
               const QString &version);
void setupLogfile(void);
bool openPidfile(ofstream &pidfs, const QString &pidfilename);
bool setUser(const QString &username);
int handle_command(const MythCommandLineParser &cmdline);
int connect_to_master(void);
int setup_basics(const MythCommandLineParser &cmdline);
void print_warnings(const MythCommandLineParser &cmdline);
int run_backend(const MythCommandLineParser &cmdline);

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
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

#endif // _MAIN_HELPERS_H_
