
#include "libmythbase/mythcommandlineparser.h"

class MythMetadataLookupCommandLineParser : public MythCommandLineParser
{
  public:
    MythMetadataLookupCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
};

