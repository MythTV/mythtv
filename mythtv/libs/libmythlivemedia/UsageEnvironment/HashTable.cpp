/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**********/
// Copyright (c) 1996-2003 Live Networks, Inc.  All rights reserved.
// Generic Hash Table
// Implementation

#include "HashTable.hh"

HashTable::HashTable() {
}

HashTable::~HashTable() {
}

HashTable::Iterator::Iterator() {
}

HashTable::Iterator::~Iterator() {}

void* HashTable::RemoveNext() {
  Iterator* iter = Iterator::create(*this);
  char const* key;
  void* removedValue = iter->next(key);
  if (removedValue != 0) Remove(key);

  delete iter;
  return removedValue;
}
