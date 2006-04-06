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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

#if !defined(LOGGING_H)
#define LOGGING_H

#include <qglobal.h> // For Q_ASSERT

#ifdef ASSERT
#undef ASSERT
#endif

#ifdef _DEBUG

#define THIS_FILE          __FILE__
#define ASSERT(f) \
    do \
    { \
        if (!(f)) { Q_ASSERT(f); _asm { int 3 } } \
    } while (0) \

#define VERIFY(f)          ASSERT(f)
#else
#define ASSERT(f)          Q_ASSERT(f)
#define VERIFY(f)          ((void)(f))
#endif

// A number of MHEG actions do not appear to actually be used.  ASSERT(UNTESTED("Action name")) is used to give
// some idea of those that still need to be tested.
#define UNTESTED(x) (false)

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
    ASSERT(false); \
    throw "Failed"; \
} while (0)

#endif
