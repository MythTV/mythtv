#pragma once

#include <stdint.h>

#define _PFX_8   "hh"
#define _PFX_16  "h"
#define _PFX_32  "l"
#define _PFX_64  "ll"

#ifdef _WIN64
#define _PFX_PTR  "ll"
#else
#define _PFX_PTR  "l"
#endif

#ifdef _FAST16_IS_32 /* compiler test */
#define _PFX_F16  _PFX_32
#else /* _FAST16_IS_32 */
#define _PFX_F16  _PFX_16
#endif /* _FAST16_IS_32 */

/* PRINT FORMAT MACROS */
#define PRId8        _PFX_8 "d"
#define PRId16       _PFX_16 "d"
#define PRId32       _PFX_32 "d"
#define PRIdLEAST8   _PFX_8 "d"
#define PRIdLEAST16  _PFX_16 "d"
#define PRIdLEAST32  _PFX_32 "d"
#define PRIdFAST8    _PFX_8 "d"
#define PRIdFAST16   _PFX_F16 "d"
#define PRIdFAST32   _PFX_32 "d"

#define PRIi8        _PFX_8 "i"
#define PRIi16       _PFX_16 "i"
#define PRIi32       _PFX_32 "i"
#define PRIiLEAST8   _PFX_8 "i"
#define PRIiLEAST16  _PFX_16 "i"
#define PRIiLEAST32  _PFX_32 "i"
#define PRIiFAST8    _PFX_8 "i"
#define PRIiFAST16   _PFX_F16 "i"
#define PRIiFAST32   _PFX_32 "i"

#define PRIo8        _PFX_8 "o"
#define PRIo16       _PFX_16 "o"
#define PRIo32       _PFX_32 "o"
#define PRIoLEAST8   _PFX_8 "o"
#define PRIoLEAST16  _PFX_16 "o"
#define PRIoLEAST32  _PFX_32 "o"
#define PRIoFAST8    _PFX_8 "o"
#define PRIoFAST16   _PFX_F16 "o"
#define PRIoFAST32   _PFX_32 "o"

#define PRIu8        _PFX_8 "u"
#define PRIu16       _PFX_16 "u"
#define PRIu32       _PFX_32 "u"
#define PRIuLEAST8   _PFX_8 "u"
#define PRIuLEAST16  _PFX_16 "u"
#define PRIuLEAST32  _PFX_32 "u"
#define PRIuFAST8    _PFX_8 "u"
#define PRIuFAST16   _PFX_F16 "u"
#define PRIuFAST32   _PFX_32 "u"

#define PRIx8        _PFX_8 "x"
#define PRIx16       _PFX_16 "x"
#define PRIx32       _PFX_32 "x"
#define PRIxLEAST8   _PFX_8 "x"
#define PRIxLEAST16  _PFX_16 "x"
#define PRIxLEAST32  _PFX_32 "x"
#define PRIxFAST8    _PFX_8 "x"
#define PRIxFAST16   _PFX_F16 "x"
#define PRIxFAST32   _PFX_32 "x"

#define PRIX8        _PFX_8 "X"
#define PRIX16       _PFX_16 "X"
#define PRIX32       _PFX_32 "X"
#define PRIXLEAST8   _PFX_8 "X"
#define PRIXLEAST16  _PFX_16 "X"
#define PRIXLEAST32  _PFX_32 "X"
#define PRIXFAST8    _PFX_8 "X"
#define PRIXFAST16   _PFX_F16 "X"
#define PRIXFAST32   _PFX_32 "X"

#define PRId64       _PFX_64 "d"
#define PRIdLEAST64  _PFX_64 "d"
#define PRIdFAST64   _PFX_64 "d"
#define PRIdMAX      _PFX_64 "d"
#define PRIdPTR      _PFX_PTR "d"

#define PRIi64       _PFX_64 "i"
#define PRIiLEAST64  _PFX_64 "i"
#define PRIiFAST64   _PFX_64 "i"
#define PRIiMAX      _PFX_64 "i"
#define PRIiPTR      _PFX_PTR "i"

#define PRIo64       _PFX_64 "o"
#define PRIoLEAST64  _PFX_64 "o"
#define PRIoFAST64   _PFX_64 "o"
#define PRIoMAX      _PFX_64 "o"
#define PRIoPTR      _PFX_PTR "o"

#define PRIu64       _PFX_64 "u"
#define PRIuLEAST64  _PFX_64 "u"
#define PRIuFAST64   _PFX_64 "u"
#define PRIuMAX      _PFX_64 "u"
#define PRIuPTR      _PFX_PTR "u"

#define PRIx64       _PFX_64 "x"
#define PRIxLEAST64  _PFX_64 "x"
#define PRIxFAST64   _PFX_64 "x"
#define PRIxMAX      _PFX_64 "x"
#define PRIxPTR      _PFX_PTR "x"

#define PRIX64       _PFX_64 "X"
#define PRIXLEAST64  _PFX_64 "X"
#define PRIXFAST64   _PFX_64 "X"
#define PRIXMAX      _PFX_64 "X"
#define PRIXPTR      _PFX_PTR "X"
