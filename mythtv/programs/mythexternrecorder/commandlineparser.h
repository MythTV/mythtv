
#include <QString>

#include "mythcommandlineparser.h"

class MythExternRecorderCommandLineParser : public MythCommandLineParser
{
  public:
    MythExternRecorderCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};
