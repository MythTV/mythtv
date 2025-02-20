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
static const QRegularExpression themeparts
    { "v([0-9]+)(\\.([0-9]+))?(\\.([0-9]+))?(-pre)?-*", QRegularExpression::CaseInsensitiveOption };

bool ParseMythSourceVersion(const QString version, bool& devel, uint8_t& major, uint8_t &minor)
{
    auto match = themeparts.match(version);
    if (!match.isValid() || !match.hasMatch())
        return false;
    devel = !match.captured(6).isEmpty();
    major = match.captured(1).toUInt();
    minor = match.captured(3).toUInt();
    return true;
}
bool ParseMythSourceVersion(bool& devel, uint8_t& major, uint8_t &minor)
{
    return ParseMythSourceVersion(MYTH_SOURCE_VERSION, devel, major, minor);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
