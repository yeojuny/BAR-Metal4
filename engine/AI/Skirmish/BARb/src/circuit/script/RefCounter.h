/*
 * RefCounter.h
 *
 *  Created on: Apr 6, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_REFCOUNTER_H_
#define SRC_CIRCUIT_SCRIPT_REFCOUNTER_H_

#include "util/ExtAS.h"
#include "angelscript/include/angelscript.h"

#include <atomic>

namespace circuit {

template<class T>
class IRefCounter {
public:
	IRefCounter() : refCount(1) {}
	virtual ~IRefCounter() {}

public:
	void AddRef() { ++refCount; }  // atomic
	void Release() {
		if (--refCount == 0) {  // atomic
			delete this;
		}
	}
	int GetRefCount() const { return refCount; }

	static void RegisterRefCounted(asIScriptEngine* engine, const char* name) {
		// Registering the reference type
		int r = engine->RegisterObjectType(name, 0, asOBJ_REF); ASSERT(r >= 0);

		// Registering the addref/release behaviours
		// NOTE: In prior version used asMETHODPR to register IRefCounter,
		//       asMETHOD may not detect ptrdiff_t of member function (multiple inheritance)
		r = engine->RegisterObjectBehaviour(name, asBEHAVE_ADDREF, "void f()", asMETHOD(T, AddRef), asCALL_THISCALL); ASSERT(r >= 0);
		r = engine->RegisterObjectBehaviour(name, asBEHAVE_RELEASE, "void f()", asMETHOD(T, Release), asCALL_THISCALL); ASSERT(r >= 0);
		r = engine->RegisterObjectMethod(name, "int GetRefCount() const", asMETHOD(T, GetRefCount), asCALL_THISCALL); ASSERT(r >= 0);
	}

private:
	std::atomic<int> refCount;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_REFCOUNTER_H_
