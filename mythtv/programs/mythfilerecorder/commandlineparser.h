
#include <QString>

#include "mythcommandlineparser.h"

class MythFileRecorderCommandLineParser : public MythCommandLineParser
{
  public:
    MythFileRecorderCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

