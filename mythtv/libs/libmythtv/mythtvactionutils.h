#ifndef MYTHTVACTIONUTILS_H
#define MYTHTVACTIONUTILS_H

// MythTV
#include "tv_actions.h"
#include "videoouttypes.h"

inline bool IsActionable(const QString& Action, const QStringList& Actions)
{
    return std::find(Actions.cbegin(), Actions.cend(), Action) != Actions.cend();
}

inline bool IsActionable(const QStringList& Action, const QStringList& Actions)
{
    return std::any_of(Action.cbegin(), Action.cend(), [&](const QString& A) { return IsActionable(A, Actions); });
}

inline StereoscopicMode ActionToStereoscopic(const QString& Action)
{
    if (ACTION_3DSIDEBYSIDEDISCARD == Action)   return kStereoscopicModeSideBySideDiscard;
    if (ACTION_3DTOPANDBOTTOMDISCARD == Action) return kStereoscopicModeTopAndBottomDiscard;
    if (ACTION_3DIGNORE == Action)              return kStereoscopicModeIgnore3D;
    return kStereoscopicModeAuto;
}

#endif
