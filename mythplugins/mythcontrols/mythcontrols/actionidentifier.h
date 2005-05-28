/**
 * @file actionidentifier.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Header for the action identifier.
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
#ifndef ACTIONIDENTIFIER_H
#define ACTIONIDENTIFIER_H

#include <qstring.h>
#include <qpair.h>

/**
 * @brief A context string and action string identifies a key string
 * and description.
 */
typedef QPair<QString, QString> ActionIdentifier;


#endif /* ACTIONIDENTIFIER_H */
