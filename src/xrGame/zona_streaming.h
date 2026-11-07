////////////////////////////////////////////////////////////////////////////
// ZONA — in-session zone transitions (streaming POC)
////////////////////////////////////////////////////////////////////////////
#pragma once

#include "game_graph.h"

struct NET_Packet;

struct ZonaChangeLevelRequest
{
	GameGraph::_GRAPH_ID game_vertex_id = GameGraph::_GRAPH_ID(-1);
	u32 level_vertex_id = u32(-1);
	Fvector position;
	Fvector angles;
};

namespace ZonaStreaming
{
	// Config + console (zona_stream_world, zona_stream_debug, zona_stream_radius_m)
	void ReloadRegistry();
	void OnLevelLoaded();
	void OnFrame();

	bool IsActive();
	bool IsStreamableLevel(LPCSTR level_name);

	bool ParseChangeLevelPacket(NET_Packet& packet, ZonaChangeLevelRequest& out);
	LPCSTR DestinationLevelName(const ZonaChangeLevelRequest& req);

	// Server: apply ALife actor move without autosave / reconnect command line.
	bool ServerApplyChangeLevel(const ZonaChangeLevelRequest& req);

	// Client: mount dest zone if possible, teleport actor, skip MakeReconnect.
	bool ClientHandoffChangeLevel(const ZonaChangeLevelRequest& req);

	// Preload / mount hook (P2 geometry — returns true only for already-mounted zones).
	bool EnsureZoneLoaded(LPCSTR level_name);

	void WorldPositionForRequest(const ZonaChangeLevelRequest& req, Fvector& world_pos, Fvector& world_dir);

	bool SuppressLoadUI();
}
