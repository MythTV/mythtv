/**
 * @file keyconflict.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Header for the key conflict class.
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
#ifndef KEYCONFLICT_H
#define KEYCONFLICT_H

#include "actionidentifier.h"

class KeyConflict {

 public:

  /**
   * @brief Create an empty key conflict.
   *
   * Unless your name is QValueList, please dont use this.
   */
  inline KeyConflict() {}

  /**
   * @brief Create a new key conflict
   * @param key The key causing the conflict.
   * @param first The first action the key is bound to.
   * @param second The second action the key is bound to.
   */
  KeyConflict(const QString & key, const ActionIdentifier & first,
	      const ActionIdentifier & second);

  /**
   * @brief Get the key causing the conflict.
   * @return The key causing the conflict.
   */
  inline const QString & getKey() const { return this->_key; }

  /**
   * @brief Get the first action to which the key is bound.
   * @return The first action to which the key is bound.
   */
  inline const ActionIdentifier & getFirst() const { return this->_first; }

  /**
   * @brief Get the second action to which the key is bound.
   * @return The second action to which the key is bound.
   */
  inline const ActionIdentifier & getSecond() const { return this->_second; }

 protected:

  /**
   * @brief Get a reference to the key.
   * @return A reference to the key.
   */
  inline QString & key() { return this->_key; }

  /**
   * @brief Get a reference to the first action.
   * @return A reference to the first action.
   */
  inline ActionIdentifier & first() { return this->_first; }

  /**
   * @brief Get a reference to the second action.
   * @return A reference to the second action.
   */
  inline ActionIdentifier & second() { return this->_second; }

 private:
  
  QString _key;
  ActionIdentifier _first;
  ActionIdentifier _second;
};

#endif /* KEYCONFLICT_H */
