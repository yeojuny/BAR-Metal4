/*
 * ExtAS.h
 *
 *  Created on: Jul 7, 2026
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_EXTAS_H_
#define SRC_CIRCUIT_UTIL_EXTAS_H_

#ifdef DEBUG
	#include <cassert>
	#define ASSERT(x)	assert(x)
#else
	#define ASSERT(x)	if (!(x)) throw
#endif

#define asMETHOD2PR(c,b,m,p,r) asSMethodPtr<sizeof(void (c::*)())>::Convert(AS_METHOD_AMBIGUITY_CAST(r (c::*)p)(&b::m))

#endif /* SRC_CIRCUIT_UTIL_EXTAS_H_ */
