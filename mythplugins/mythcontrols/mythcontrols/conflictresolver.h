/**
 * @file conflictresolver.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Header file for the conflict resolution popup box.
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
#ifndef CONFLICTRESOLVER_H
#define CONFLICTRESOLVER_H

#include <mythtv/mythdialogs.h>

#include "keybindings.h"

/**
 * @class ConflictResolver
 * @brief A popup box to Resolve conflict
 */
class ConflictResolver : public MythPopupBox
{

    Q_OBJECT

 public:

    /**
     * @brief Create a conflict resolving dialog box.
     * @param window The main myth window.
     * @param KeyConflict The conflict to resolve.
     */
    ConflictResolver(MythMainWindow *window, KeyConflict *key_conflict);

    const QString & getButtonCaption(size_t number) const;

    KeyConflict *key_conflict;

 private:

    QString first;
    QString second;
};

#endif /* CONFLICTRESOLVER_H */
