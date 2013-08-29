/* Logging.h

   Copyright (C)  David C. J. Matthews 2004  dm at prolingua.co.uk

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

#if !defined(LOGGING_H)
#define LOGGING_H

#include <QtGlobal> // For Q_ASSERT
#include <QString> // For QString

#include "freemheg.h" // For MHLogError

#define MHASSERT(f)          Q_ASSERT(f)

extern int __mhlogoptions;
extern void __mhlog(QString logtext);
extern FILE *__mhlogStream;

#define MHLOG(__level,__text) \
do { \
    if (__level & __mhlogoptions) \
        __mhlog(__text); \
} while (0)

#define MHERROR(__text) \
do { \
    if (MHLogError & __mhlogoptions) \
        __mhlog(__text); \
    throw "Failed"; \
} while (0)

#endif
