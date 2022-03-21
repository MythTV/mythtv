
#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythJobQueueCommandLineParser : public MythCommandLineParser
{
  public:
    MythJobQueueCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

