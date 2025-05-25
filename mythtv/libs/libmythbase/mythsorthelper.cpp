// -*- Mode: c++ -*-
// vim: set expandtab tabstop=4 shiftwidth=4

#include "mythsorthelper.h"

#include "mythcorecontext.h"
#include "mythlogging.h"

/**
 *  \brief Common code for creating a MythSortHelper object.
 *
 *  Given an object with the three user specified parameters
 *  initialized, this function initializes the rest of the object.
 */
void MythSortHelper::MythSortHelperCommon(void)
{
    m_prefixes = tr("^(The |A |An )",
                    "Regular Expression for what to ignore when sorting");
    m_prefixes = m_prefixes.trimmed();
    if (not hasPrefixes()) {
        // This language doesn't ignore any words when sorting
        m_prefixMode = SortPrefixKeep;
        return;
    }
    if (not m_prefixes.startsWith("^"))
        m_prefixes = "^" + m_prefixes;

    if (m_caseSensitive == Qt::CaseInsensitive)
    {
        m_prefixes = m_prefixes.toLower();
        m_exclusions = m_exclusions.toLower();
    }
    m_prefixesRegex = QRegularExpression(m_prefixes);
    m_prefixesRegex2 = QRegularExpression(m_prefixes + "(.*)");
    m_exclList = m_exclusions.split(";", Qt::SkipEmptyParts);
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (int i = 0; i < m_exclList.size(); i++)
      m_exclList[i] = m_exclList[i].trimmed();
}

/**
 *  \brief Create a MythSortHelper object.
 *
 *  Create the object and read settings from the core context.
 *
 *  \note This handles the case where the gCoreContext object doesn't
 *  exists (i.e. running under the Qt test harness) which allows this
 *  code and the objects using it to be tested.
 */
MythSortHelper::MythSortHelper()
{
    if (gCoreContext) {
#if 0
        // Last minute change.  QStringRef::localeAwareCompare appears to
        // always do case insensitive sorting, so there's no point in
        // presenting this option to a user.
        m_caseSensitive =
            gCoreContext->GetBoolSetting("SortCaseSensitive", false)
            ? Qt::CaseSensitive : Qt::CaseInsensitive;
#endif
        m_prefixMode =
            gCoreContext->GetBoolSetting("SortStripPrefixes", true)
            ? SortPrefixRemove : SortPrefixKeep;
        m_exclusions =
            gCoreContext->GetSetting("SortPrefixExceptions", "");
    }
    MythSortHelperCommon();
}

/**
 *  \brief Fully specify the creation of a MythSortHelper object.
 *
 *  This function creates a MythSortHelper object based on the
 *  parameters provided.  It does not attempt to retrieve the user's
 *  preferred preferences from the database.
 *
 *  \warning This function should never be called directly.  It exists
 *  solely for use in test code.
 *
 *  \return A pointer to the MythSortHelper singleton.
 */
MythSortHelper::MythSortHelper(
    Qt::CaseSensitivity case_sensitive,
    SortPrefixMode prefix_mode,
    QString exclusions) :
    m_caseSensitive(case_sensitive),
    m_prefixMode(prefix_mode),
    m_exclusions(std::move(exclusions))
{
    MythSortHelperCommon();
}

/**
 *  \brief Copy a MythSortHelper object.
 *
 *  This function is required for the shared pointer code to work.
 *
 *  \warning This function should never be called directly.
 *
 *  \return A new MythSortHelper object.
 */
MythSortHelper::MythSortHelper(MythSortHelper *other)
{
    if (other == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythSortHelper created from null pointer."));
        return;
    }
    m_caseSensitive   = other->m_caseSensitive;
    m_prefixMode      = other->m_prefixMode;
    m_prefixes        = other->m_prefixes;
    m_prefixesRegex   = other->m_prefixesRegex;
    m_prefixesRegex2  = other->m_prefixesRegex2;
    m_exclusions      = other->m_exclusions;
    m_exclList        = other->m_exclList;
    m_exclMode        = other->m_exclMode;
}

static std::shared_ptr<MythSortHelper> singleton = nullptr;

/**
 *  \brief Get a pointer to the MythSortHelper singleton.
 *
 *  This function creates the MythSortHelper singleton, and returns a
 *  shared pointer to it.
 *
 *  \return A pointer to the MythSortHelper singleton.
 */
std::shared_ptr<MythSortHelper> getMythSortHelper(void)
{
    if (singleton == nullptr)
        singleton = std::make_shared<MythSortHelper>();
    return singleton;
}

/**
 *  \brief Delete the MythSortHelper singleton.
 *
 *  This function deletes the pointer to the MythSortHelper singleton.
 *  This will force the next call to get the singleton to create a new
 *  one, which will pick up any modified parameters.  The old one will
 *  be deleted when the last reference goes away (which probably
 *  happens with this assignment).
 */
void resetMythSortHelper(void)
{
    singleton = nullptr;
}

/**
 *  \brief Create the sortable form of an title string.
 *
 *  This function converts a title string to a version that can be
 *  used for sorting. Depending upon user settings, it may ignore the
 *  case of the string (by forcing all strings to lower case) and may
 *  strip the common prefix words "A", "An" and "The" from the
 *  beginning of the string.
 *
 * \param title The title of an item.
 * \return The conversion of the title to use when sorting.
 */
QString MythSortHelper::doTitle(const QString& title) const
{
    QString ltitle = title;
    if (m_caseSensitive == Qt::CaseInsensitive)
        ltitle = ltitle.toLower();
    if (m_prefixMode == SortPrefixKeep)
	return ltitle;
    if (m_exclMode == SortExclusionMatch)
    {
        if (m_exclList.contains(ltitle))
            return ltitle;
    } else {
        for (const auto& str : std::as_const(m_exclList))
            if (ltitle.startsWith(str))
                return ltitle;
    }
    if (m_prefixMode == SortPrefixRemove)
        return ltitle.remove(m_prefixesRegex);
    return ltitle.replace(m_prefixesRegex2, "\\2, \\1").trimmed();
}

/**
 *  \brief Create the sortable form of an item.
 *
 *  This function converts a pathname to a version that can be used
 *  for sorting. Depending upon user settings, it may ignore the case
 *  of the string (by forcing all strings to lower case) and may strip
 *  the common prefix words "A", "An" and "The" from the beginning of
 *  each component in the pathname.
 *
 * \param pathname The pathname of an item.
 * \return The conversion of the pathname to use when sorting.
 */
QString MythSortHelper::doPathname(const QString& pathname) const
{
    QString lpathname = pathname;
    if (m_caseSensitive == Qt::CaseInsensitive)
        lpathname = lpathname.toLower();
    if (m_prefixMode == SortPrefixKeep)
	return lpathname;
    QStringList parts = lpathname.split("/");
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (int i = 0; i < parts.size(); ++i) {
        if (std::any_of(m_exclList.cbegin(), m_exclList.cend(),
                        [&parts,i](const QString& excl)
                            { return parts[i].startsWith(excl); } ))
            continue;
        if (m_prefixMode == SortPrefixRemove)
            parts[i] = parts[i].remove(m_prefixesRegex);
        else
            parts[i] = parts[i].replace(m_prefixesRegex2, "\\2, \\1").trimmed();
    }
    return parts.join("/");
}
