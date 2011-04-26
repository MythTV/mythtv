// C++ headers
#include <iostream>
#include <fstream>
using namespace std;

class MythCommandLineParser;
class QString;
class QSize;

bool setupTVs(bool ismaster, bool &error);
bool setup_context(MythBackendCommandLineParser &cmdline);
void cleanup(void);
int  log_rotate(int report_error);
void log_rotate_handler(int);
void upnp_rebuild(int);
void showUsage(const MythBackendCommandLineParser &cmdlineparser, const QString &version);
void setupLogfile(void);
bool openPidfile(ofstream &pidfs, const QString &pidfilename);
bool setUser(const QString &username);
int  handle_command(const MythBackendCommandLineParser &cmdline);
int  connect_to_master(void);
int  setup_basics(const MythBackendCommandLineParser &cmdline);
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
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

