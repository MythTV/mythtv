#ifndef MYTHTVACTIONUTILS_H
#define MYTHTVACTIONUTILS_H

// C++ headers
#include <algorithm>

// MythTV
#include "tv_actions.h"
#include "videoouttypes.h"

inline bool IsActionable(const QString& Action, const QStringList& Actions)
{
    return Actions.contains(Action);
}

inline bool IsActionable(const QStringList& Action, const QStringList& Actions)
{
    return std::ranges::any_of(std::as_const(Action),
                       [&](const QString& A) { return Actions.contains(A); });
}

inline StereoscopicMode ActionToStereoscopic(const QString& Action)
{
    if (ACTION_3DSIDEBYSIDEDISCARD == Action)   return kStereoscopicModeSideBySideDiscard;
    if (ACTION_3DTOPANDBOTTOMDISCARD == Action) return kStereoscopicModeTopAndBottomDiscard;
    if (ACTION_3DIGNORE == Action)              return kStereoscopicModeIgnore3D;
    return kStereoscopicModeAuto;
}

#endif
