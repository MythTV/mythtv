/**
 * @file conflictresolver.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Implementation of the conflict resolution popup box.
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 */
#ifndef CONFLICTRESOLVER_CPP
#define CONFLICTRESOLVER_CPP

#include "conflictresolver.h"

ConflictResolver::ConflictResolver(MythMainWindow *window,
				   KeyConflict *conflict)
    : MythPopupBox(window, "conflictresolver")
{

    addLabel("Conflicting Keys", Large, false);

    QString warning = "The \"" + conflict->getKey() +
	"\" key has conflicting bindings.";

    /* conflicts is in the same context */
    if (conflict->getFirst().first == conflict->getSecond().first)
    {
	warning.append("  In the \"" + conflict->getFirst().first
		       + "\" context, which action should be kept?");

	first = conflict->getFirst().second;
	second = conflict->getSecond().second;
    }
    else
    {
	warning.append("  Which action should be kept: \"" +
		       conflict->getFirst().second +
		       "\" from context, \"" +
		       conflict->getFirst().first +
		       "\", or \"" +
		       conflict->getSecond().second +
		       "\" from context, \"" +
		       conflict->getSecond().first + "\"?");

	first = conflict->getFirst().first;
	second = conflict->getSecond().first;
    }

    addLabel(warning, Small, true);

    key_conflict = conflict;
}


const QString & ConflictResolver::getButtonCaption(size_t number) const
{
    if (number == 1) return first;
    else if (number == 2) return second;

    return "";
}

#endif /* CONFLICTRESOLVER_CPP */
