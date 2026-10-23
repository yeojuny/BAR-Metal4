/*
 * Profiler.h
 *
 *  Created on: Oct 10, 2023
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_PROFILER_H_
#define SRC_CIRCUIT_UTIL_PROFILER_H_

#if !defined(CIRCUIT_PROFILING)
	#undef TRACY_ENABLE
#endif
#include <tracy/Tracy.hpp>

#include <vector>
#include <cstdio>
#include <string>
#include <array>

namespace tracy {
#ifdef TRACY_ON_DEMAND
TRACY_API LuaZoneState& GetLuaZoneState();
#endif
}

namespace circuit {

class CProfiler;
extern CProfiler profiler;

class CProfiler final {
public:
	CProfiler() {}
	~CProfiler() {}

	static CProfiler& GetInstance() { return profiler; }

	static void InitNames(int skirmishAIId);
	static void ReleaseNames(int skirmishAIId);

	// @see rts/lib/tracy/public/tracy/TracyLua.hpp  or
	//      rts/lib/tracy/public/client/TracyScoped.hpp
	static inline void TracyZoneBegin(uint32_t line, const char* source, const char* function, const std::string name, uint32_t color = 0) {
#ifdef CIRCUIT_PROFILING
	#ifdef TRACY_ON_DEMAND
		const auto zoneCnt = tracy::GetLuaZoneState().counter++;
		if( zoneCnt != 0 && !tracy::GetLuaZoneState().active ) return;
		tracy::GetLuaZoneState().active = tracy::GetProfiler().IsConnected();
		if( !tracy::GetLuaZoneState().active ) return;
	#endif

		const auto srcloc = tracy::Profiler::AllocSourceLocation( line, source, function, name.c_str(), name.size(), color );

		TracyQueuePrepare( tracy::QueueType::ZoneBeginAllocSrcLoc );
		tracy::MemWrite( &item->zoneBegin.time, tracy::Profiler::GetTime() );
		tracy::MemWrite( &item->zoneBegin.srcloc, srcloc );
		TracyQueueCommit( zoneBeginThread );
#endif
	}
	static inline void TracyZoneText(const std::string& text) {
#ifdef CIRCUIT_PROFILING
	#ifdef TRACY_ON_DEMAND
		if( !tracy::GetLuaZoneState().active ) return;
		if( !tracy::GetProfiler().IsConnected() )
		{
			tracy::GetLuaZoneState().active = false;
			return;
		}
	#endif

		auto ptr = (char*)tracy::tracy_malloc( text.size() );
		memcpy( ptr, text.c_str(), text.size() );
		TracyQueuePrepare( tracy::QueueType::ZoneText );
		tracy::MemWrite( &item->zoneTextFat.text, (uint64_t)ptr );
		tracy::MemWrite( &item->zoneTextFat.size, (uint16_t)text.size() );
		TracyQueueCommit( zoneTextFatThread );
#endif
	}
	static inline void TracyZoneEnd() {
#ifdef CIRCUIT_PROFILING
	#ifdef TRACY_ON_DEMAND
		assert( tracy::GetLuaZoneState().counter != 0 );
		tracy::GetLuaZoneState().counter--;
		if( !tracy::GetLuaZoneState().active ) return;
		if( !tracy::GetProfiler().IsConnected() )
		{
			tracy::GetLuaZoneState().active = false;
			return;
		}
	#endif

		TracyQueuePrepare( tracy::QueueType::ZoneEnd );
		tracy::MemWrite( &item->zoneEnd.time, tracy::Profiler::GetTime() );
		TracyQueueCommit( zoneEndThread );
#endif
	}

	// NOTE: char[] length is wrong as it references a format string
	#define NAME_ARRAY(txt, fmt)	\
	public:	\
		static const char* const Get##txt##Name(int skirmishAIId) {	\
			return name##txt[skirmishAIId].data();	\
		}	\
		static size_t Get##txt##Size(int skirmishAIId) {	\
			return name##txt[skirmishAIId].size();	\
		}	\
	private:	\
		static constexpr std::string_view str##txt = fmt;	\
		static std::vector<std::array<char, str##txt.size() + 1>> name##txt

	NAME_ARRAY(EventInit,    "%02i EVENT_INIT");
	NAME_ARRAY(EventRelease, "%02i EVENT_RELEASE");
	NAME_ARRAY(EventUpdate,  "%02i EVENT_UPDATE");
	NAME_ARRAY(EventMessage, "%02i EVENT_MESSAGE");

	NAME_ARRAY(EventUnitCreated,    "%02i EVENT_UNIT_CREATED");
	NAME_ARRAY(EventUnitFinished,   "%02i EVENT_UNIT_FINISHED");
	NAME_ARRAY(EventUnitIdle,       "%02i EVENT_UNIT_IDLE");
	NAME_ARRAY(EventUnitMoveFailed, "%02i EVENT_UNIT_MOVE_FAILED");
	NAME_ARRAY(EventUnitDamaged,    "%02i EVENT_UNIT_DAMAGED");
	NAME_ARRAY(EventUnitDestroyed,  "%02i EVENT_UNIT_DESTROYED");
	NAME_ARRAY(EventUnitGiven,      "%02i EVENT_UNIT_GIVEN");
	NAME_ARRAY(EventUnitCaptured,   "%02i EVENT_UNIT_CAPTURED");

	NAME_ARRAY(EventEnemyEnterLOS,   "%02i EVENT_ENEMY_ENTER_LOS");
	NAME_ARRAY(EventEnemyLeaveLOS,   "%02i EVENT_ENEMY_LEAVE_LOS");
	NAME_ARRAY(EventEnemyEnterRadar, "%02i EVENT_ENEMY_ENTER_RADAR");
	NAME_ARRAY(EventEnemyLeaveRadar, "%02i EVENT_ENEMY_LEAVE_RADAR");
	NAME_ARRAY(EventEnemyDamaged,    "%02i EVENT_ENEMY_DAMAGED");
	NAME_ARRAY(EventEnemyDestroyed,  "%02i EVENT_ENEMY_DESTROYED");

	NAME_ARRAY(EventWeaponFired,     "%02i EVENT_WEAPON_FIRED");
	NAME_ARRAY(EventPlayerCommand,   "%02i EVENT_PLAYER_COMMAND");
	NAME_ARRAY(EventSeismicPing,     "%02i EVENT_SEISMIC_PING");
	NAME_ARRAY(EventCommandFinished, "%02i EVENT_COMMAND_FINISHED");
	NAME_ARRAY(EventLoad,            "%02i EVENT_LOAD");
	NAME_ARRAY(EventSave,            "%02i EVENT_SAVE");
	NAME_ARRAY(EventEnemyCreated,    "%02i EVENT_ENEMY_CREATED");
	NAME_ARRAY(EventEnemyFinished,   "%02i EVENT_ENEMY_FINISHED");
	NAME_ARRAY(EventLuaMessage,      "%02i EVENT_LUA_MESSAGE");

	NAME_ARRAY(EventReleaseEnd,    "%02i EVENT_RELEASE::END");
	NAME_ARRAY(EventReleaseResign, "%02i EVENT_RELEASE::RESIGN");
	NAME_ARRAY(EventUpdateResign,  "%02i EVENT_UPDATE::RESIGN");

	NAME_ARRAY(ThreatUpdate, "%02i Threat::Update");
	NAME_ARRAY(InflUpdate, "%02i Influence::Update");

	#undef NAME_ARRAY

private:
	static int numInit;
};

} /* namespace circuit */

#endif /* SRC_CIRCUIT_UTIL_PROFILER_H_ */
