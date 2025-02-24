#include "mythversion.h"

#include "version.h"

#include <QRegularExpression>

const char *GetMythSourceVersion()
{
    return MYTH_SOURCE_VERSION;
}

const char *GetMythSourcePath()
{
    return MYTH_SOURCE_PATH;
}

// Require major version. Optional minor and point versions. Optional
// '-pre' tag.  If present, must be in this order.
//
// Match 1: major
// Match 2: .minor
// Match 3: minor
// Match 4: .point
// Match 5: point
// Match 6: -pre
bool ParseMythSourceVersion(bool& devel, uint& major, uint& minor, const char *version)
{
    static const QRegularExpression themeparts
        { R"(\Av([0-9]+)(\.([0-9]+))?(\.([0-9]+))?(-pre)?)",
          QRegularExpression::CaseInsensitiveOption };

    if (nullptr == version)
        version = MYTH_SOURCE_VERSION;
    auto match = themeparts.match(version);
    if (!match.isValid() || !match.hasMatch())
        return false;
    devel = !match.captured(6).isEmpty();
    major = match.captured(1).toUInt();
    minor = match.captured(3).toUInt();
    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
