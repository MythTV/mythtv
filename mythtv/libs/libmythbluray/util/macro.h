/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef MACRO_H_
#define MACRO_H_

#include <stdio.h>   /* fprintf() */
#include <stdlib.h>  /* free() */

#define HEX_PRINT(X,Y) { int zz; for(zz = 0; zz < Y; zz++) fprintf(stderr, "%02X", X[zz]); fprintf(stderr, "\n"); }
#define MKINT_BE16(X) ( (X)[0] << 8 | (X)[1] )
#define MKINT_BE24(X) ( (X)[0] << 16 | (X)[1] << 8 | (X)[2] )
#define MKINT_BE32(X) ( (X)[0] << 24 | (X)[1] << 16 |  (X)[2] << 8 | (X)[3] )
#define X_FREE(X)     ( free(X), X = NULL )

/*
 * automatic cast from void* (malloc/calloc/realloc)
 */

#ifdef __cplusplus

template <typename T>
class auto_cast_wrapper
{
public:
    template <typename R> friend auto_cast_wrapper<R> auto_cast(const R& x);
    template <typename U> operator U() { return static_cast<U>(p); }

private:
    auto_cast_wrapper(const T& x) : p(x) {}
    auto_cast_wrapper(const auto_cast_wrapper& o) : p(o.p) {}

    auto_cast_wrapper& operator=(const auto_cast_wrapper&);

    const T& p;
};

template <typename R>
auto_cast_wrapper<R> auto_cast(const R& x)
{
    return auto_cast_wrapper<R>(x);
}

#  define calloc(n,s)  auto_cast(calloc(n,s))
#  define malloc(s)    auto_cast(malloc(s))
#  define realloc(p,s) auto_cast(realloc(p,s))
#endif /* __cplusplus */


#endif /* MACRO_H_ */
