
#include <QString>

#include "mythcommandlineparser.h"

class MythJobQueueCommandLineParser : public MythCommandLineParser
{
  public:
    MythJobQueueCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

