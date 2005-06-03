#include "mythcontext.h"

#include "mythfontproperties.h"

static GlobalFontMap *gFontMap = NULL;

MythFontProperties *GlobalFontMap::GetFont(const QString &text)
{
    if (text.isEmpty())
        return NULL;

    if (m_globalFontMap.contains(text))
        return &(m_globalFontMap[text]);
    return NULL;
}

bool GlobalFontMap::AddFont(const QString &text, MythFontProperties *fontProp)
{
    if (!fontProp || text.isEmpty())
        return false;

    if (m_globalFontMap.contains(text))
    {
        VERBOSE(VB_IMPORTANT, QString("Already have a font: %1").arg(text));
        return false;
    }

    m_globalFontMap[text] = *fontProp;
    return true;
}

void GlobalFontMap::Clear(void)
{
    m_globalFontMap.clear();
}

GlobalFontMap *GlobalFontMap::GetGlobalFontMap(void)
{
    if (!gFontMap)
        gFontMap = new GlobalFontMap();
    return gFontMap;
}

GlobalFontMap *GetGlobalFontMap(void)
{
    return GlobalFontMap::GetGlobalFontMap();
}

