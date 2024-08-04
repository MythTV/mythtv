// -*- Mode: c++ -*-
// vim: set expandtab tabstop=4 shiftwidth=4

#ifndef MYTHSORTHELPER_H_
#define MYTHSORTHELPER_H_

#include <QCoreApplication>
 #include <QRegularExpression>
#include <memory>
#include "mythbaseexp.h"

enum SortPrefixMode : std::uint8_t
{
    SortPrefixKeep,
    SortPrefixRemove,
    SortPrefixToEnd,
};

enum SortExclusionMode : std::uint8_t
{
    SortExclusionMatch,
    SortExclusionPrefix,
};

/**
 *  A class to consolidate all the soring functions.
 */
class MBASE_PUBLIC MythSortHelper
{
    Q_DECLARE_TR_FUNCTIONS(MythSortHelper)

  public:
    MythSortHelper();
    explicit MythSortHelper(MythSortHelper *other);
    MythSortHelper(Qt::CaseSensitivity case_sensitive, SortPrefixMode prefix_mode,
                   QString exclusions);

    QString doTitle(const QString& title) const;
    QString doPathname(const QString& pathname) const;

    /**
     *  \brief Does the language translation specify any prefixes.
     *
     *  Returns true if there are prefixes defined.  False if there are no
     *  prefixes specified.
     */
    bool hasPrefixes(void) { return not m_prefixes.isEmpty(); }

  private:
    void MythSortHelperCommon(void);

    /// Whether sorting two strings should ignore upper/lower case.
    Qt::CaseSensitivity m_caseSensitive {Qt::CaseInsensitive};

    /// Whether or not to ignore prefix words (like A, An, and The)
    /// when sorting two strings.
    SortPrefixMode m_prefixMode {SortPrefixRemove};

    /// A string containing the regular expression of prefixes to
    /// ignore when sorting.  The code will ensure that this is
    /// anchored to the start of the string.
    QString m_prefixes {QString()};
    /// A regular expression used for removing a leading prefix.  It
    /// is created from m_prefixes.
    QRegularExpression m_prefixesRegex {QRegularExpression()};
    /// A regular expression used for moving leading prefix to the end
    /// of a string.  It is created from m_prefixes.
    QRegularExpression m_prefixesRegex2 {QRegularExpression()};

    /// A string containing names that should be ignored when greating
    /// the sortable form of a title.  Multiple titles should be
    /// separated by a semicolon.  This provides a place to specify
    /// things like the show named "A to Z" should not have the prefix
    /// "A " sorted to the end of the name.
    QString m_exclusions {"A to Z"};
    /// The m_exclusion string converted into a string list.
    QStringList m_exclList {QStringList()};
    SortExclusionMode m_exclMode {SortExclusionMatch};
};

MBASE_PUBLIC std::shared_ptr<MythSortHelper> getMythSortHelper(void);
MBASE_PUBLIC void resetMythSortHelper(void);


#endif // MYTHSORTHELPER_H_
