////////////////////////////////////////////////////////////////////////////
// ZONA — in-session zone transitions (streaming POC)
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "zona_streaming.h"
#include "Level.h"
#include "actor.h"
#include "ai_space.h"
#include "game_graph.h"
#include "alife_simulator.h"
#include "alife_graph_registry.h"
#include "xrmessages.h"
#include "Actor.h"
#include "../xrEngine/xr_ioconsole.h"
#include "../xrEngine/xr_ioc_cmd.h"
#include "../xrCore/xr_ini.h"
#include "zona_streaming_mount.h"

namespace
{
	struct SZoneEntry
	{
		shared_str level_name;
		Fvector offset;
		bool streamable = false;
	};

	struct SBorderEdge
	{
		shared_str from_level;
		shared_str to_level;
		Fvector hint_pos;
		bool has_hint = false;
		bool enabled = true;
	};

	xr_vector<SZoneEntry> g_zones;
	xr_vector<SBorderEdge> g_border_edges;
	bool g_registry_loaded = false;
	bool g_cfg_enabled = false;
	float g_load_radius_m = 400.f;
	float g_unload_radius_m = 600.f;
	float g_border_preload_m = 91.f;
	BOOL g_no_loadscreen = FALSE;
	shared_str g_anchor_level;

	xr_map<shared_str, bool> g_mounted;
	xr_map<shared_str, bool> g_armed;

	BOOL g_zona_stream_world = FALSE;
	BOOL g_zona_stream_debug = FALSE;
	BOOL g_zona_stream_geom_lab = FALSE;
	float g_zona_stream_radius_m = 400.f;

	class CCC_ZonaStreamWorld : public IConsole_Command
	{
	public:
		CCC_ZonaStreamWorld() : IConsole_Command("zona_stream_world") { bLowerCaseArgs = false; }
		void Execute(LPCSTR args) override
		{
			g_zona_stream_world = atoi(args) != 0;
			Msg("* zona_stream_world = %d", g_zona_stream_world);
		}
		void Info(TInfo& I) override { xr_strcpy(I, "1 = in-session borders for registered zones (ZONA streaming exes)"); }
	};

	class CCC_ZonaStreamDebug : public IConsole_Command
	{
	public:
		CCC_ZonaStreamDebug() : IConsole_Command("zona_stream_debug") { bLowerCaseArgs = false; }
		void Execute(LPCSTR args) override
		{
			g_zona_stream_debug = atoi(args) != 0;
			Msg("* zona_stream_debug = %d", g_zona_stream_debug);
		}
	};

	class CCC_ZonaStreamRadius : public IConsole_Command
	{
	public:
		CCC_ZonaStreamRadius() : IConsole_Command("zona_stream_radius_m") { bLowerCaseArgs = false; }
		void Execute(LPCSTR args) override
		{
			g_zona_stream_radius_m = (float)atof(args);
			if (g_zona_stream_radius_m < 50.f)
				g_zona_stream_radius_m = 50.f;
			Msg("* zona_stream_radius_m = %.0f", g_zona_stream_radius_m);
		}
	};

	class CCC_ZonaStreamNoLoadScreen : public IConsole_Command
	{
	public:
		CCC_ZonaStreamNoLoadScreen() : IConsole_Command("zona_stream_no_loadscreen") { bLowerCaseArgs = false; }
		void Execute(LPCSTR args) override
		{
			g_no_loadscreen = atoi(args) != 0;
			Msg("* zona_stream_no_loadscreen = %d", g_no_loadscreen);
		}
		void Info(TInfo& I) override
		{
			xr_strcpy(I,
			          "1 = arm streamable zones at border + suppress reconnect (needs mounted geometry or lab)");
		}
	};

	class CCC_ZonaStreamBorderPreload : public IConsole_Command
	{
	public:
		CCC_ZonaStreamBorderPreload() : IConsole_Command("zona_stream_border_preload_m") { bLowerCaseArgs = false; }
		void Execute(LPCSTR args) override
		{
			g_border_preload_m = (float)atof(args);
			if (g_border_preload_m < 25.f)
				g_border_preload_m = 25.f;
			Msg("* zona_stream_border_preload_m = %.0f", g_border_preload_m);
		}
	};

	class CCC_ZonaStreamGeomLab : public IConsole_Command
	{
	public:
		CCC_ZonaStreamGeomLab() : IConsole_Command("zona_stream_geom_lab") { bLowerCaseArgs = false; }
		void Execute(LPCSTR args) override
		{
			g_zona_stream_geom_lab = atoi(args) != 0;
			Msg("* zona_stream_geom_lab = %d (pretend geometry mounted — lab only)", g_zona_stream_geom_lab);
		}
	};

	static CCC_ZonaStreamWorld s_cmd_stream_world;
	static CCC_ZonaStreamDebug s_cmd_stream_debug;
	static CCC_ZonaStreamRadius s_cmd_stream_radius;
	static CCC_ZonaStreamNoLoadScreen s_cmd_no_loadscreen;
	static CCC_ZonaStreamBorderPreload s_cmd_border_preload;
	static CCC_ZonaStreamGeomLab s_cmd_stream_geom_lab;

	u32 g_last_border_preload_log_time = 0;
	shared_str g_last_border_preload_key;

	void DbgMsg(LPCSTR msg, ...)
	{
		if (!g_zona_stream_debug && !g_cfg_enabled)
			return;
		string1024 buf;
		va_list list;
		va_start(list, msg);
		vsprintf_s(buf, sizeof(buf), msg, list);
		va_end(list);
		Msg("%s", buf);
	}

	void ParseFvector3(LPCSTR src, Fvector& dst)
	{
		dst.set(0, 0, 0);
		if (!src || !*src)
			return;
		sscanf(src, "%f,%f,%f", &dst.x, &dst.y, &dst.z);
	}

	bool LoadRegistryFromLtx()
	{
		g_zones.clear();
		g_mounted.clear();
		g_registry_loaded = false;
		g_cfg_enabled = false;

		string_path fn;
		FS.update_path(fn, "$game_config$", "zona_streaming\\zona_zones.ltx");
		if (!FS.exist(fn))
		{
			DbgMsg("! [ZONA stream] missing %s", fn);
			return false;
		}

		CInifile ini(fn, TRUE, TRUE, FALSE);
		if (ini.section_exist("zona_streaming"))
		{
			g_cfg_enabled = ini.r_bool("zona_streaming", "enabled");
			g_load_radius_m = ini.line_exist("zona_streaming", "load_radius_m")
			                    ? ini.r_float("zona_streaming", "load_radius_m")
			                    : 400.f;
			g_unload_radius_m = ini.line_exist("zona_streaming", "unload_radius_m")
			                      ? ini.r_float("zona_streaming", "unload_radius_m")
			                      : 600.f;
			g_border_preload_m = ini.line_exist("zona_streaming", "border_preload_m")
			                       ? ini.r_float("zona_streaming", "border_preload_m")
			                       : 91.f;
			g_no_loadscreen = ini.r_bool("zona_streaming", "no_loadscreen");
			g_anchor_level = ini.line_exist("zona_streaming", "anchor_level")
			                   ? ini.r_string_wb("zona_streaming", "anchor_level")
			                   : shared_str("l01_escape");
		}

		CInifile::Root& sections = ini.sections();
		for (CInifile::Root::iterator it = sections.begin(); it != sections.end(); ++it)
		{
			LPCSTR sec = it->Name.c_str();
			if (0 != strncmp(sec, "zona_zone_", 10))
				continue;

			SZoneEntry e;
			e.level_name = ini.r_string_wb(sec, "level_name");

			e.offset.set(0, 0, 0);
			if (ini.line_exist(sec, "offset"))
				ParseFvector3(ini.r_string(sec, "offset"), e.offset);

			e.streamable = ini.r_bool(sec, "streamable");

			g_zones.push_back(e);
		}

		g_registry_loaded = !g_zones.empty();
		DbgMsg("* [ZONA stream] registry: %u zones (cfg enabled=%d)", g_zones.size(), g_cfg_enabled);
		return g_registry_loaded;
	}

	bool LoadBorderEdgesFromLtx()
	{
		g_border_edges.clear();

		string_path fn;
		FS.update_path(fn, "$game_config$", "plugins\\zona_openworld_borders.ltx");
		if (!FS.exist(fn))
		{
			DbgMsg("! [ZONA stream] missing %s", fn);
			return false;
		}

		CInifile ini(fn, TRUE, TRUE, FALSE);
		const int edge_count = (int)ini.r_float("meta", "edge_count");
		const int limit = edge_count > 0 ? edge_count : 8192;

		for (int i = 1; i <= limit; ++i)
		{
			string32 sec;
			xr_sprintf(sec, "edge_%04d", i);
			if (!ini.section_exist(sec))
			{
				if (edge_count > 0)
					break;
				continue;
			}

			if (ini.line_exist(sec, "enabled") && !ini.r_bool(sec, "enabled"))
				continue;

			SBorderEdge e;
			e.from_level = ini.r_string_wb(sec, "from_level");
			e.to_level = ini.r_string_wb(sec, "to_level");
			e.enabled = true;
			e.has_hint = false;
			if (ini.line_exist(sec, "hint_pos"))
			{
				ParseFvector3(ini.r_string(sec, "hint_pos"), e.hint_pos);
				e.has_hint = true;
			}
			g_border_edges.push_back(e);
		}

		DbgMsg("* [ZONA stream] borders: %u edges loaded", g_border_edges.size());
		return !g_border_edges.empty();
	}

	const SZoneEntry* FindZone(LPCSTR level_name)
	{
		if (!level_name)
			return nullptr;
		for (const auto& z : g_zones)
		{
			if (z.level_name.size() && !xr_strcmp(z.level_name.c_str(), level_name))
				return &z;
		}
		return nullptr;
	}

	void ArmStreamableHandoff(LPCSTR level_name)
	{
		if (!level_name || !*level_name)
			return;
		if (!g_no_loadscreen && !g_zona_stream_geom_lab)
			return;
		const SZoneEntry* z = FindZone(level_name);
		if (!z || !z->streamable)
			return;
		g_armed[level_name] = true;
		DbgMsg("* [ZONA stream] handoff armed for %s", level_name);
	}
}

namespace ZonaStreaming
{
	void ReloadRegistry()
	{
		LoadRegistryFromLtx();
		LoadBorderEdgesFromLtx();
	}

	void OnLevelLoaded()
	{
		ReloadRegistry();
		if (g_pGameLevel)
		{
			const shared_str physical = Level().name();
			g_mounted.clear();
			g_mounted.insert(std::make_pair(physical, true));
			EnsureZoneLoaded(physical.c_str());
		}
	}

	bool IsActive()
	{
		return (g_zona_stream_world || g_cfg_enabled) && g_registry_loaded;
	}

	bool IsStreamableLevel(LPCSTR level_name)
	{
		const SZoneEntry* z = FindZone(level_name);
		return z && z->streamable;
	}

	bool ParseChangeLevelPacket(NET_Packet& packet, ZonaChangeLevelRequest& out)
	{
		u16 type;
		packet.r_begin(type);
		if (type != M_CHANGE_LEVEL)
			return false;

		packet.r(&out.game_vertex_id, sizeof(out.game_vertex_id));
		packet.r(&out.level_vertex_id, sizeof(out.level_vertex_id));
		packet.r_vec3(out.position);
		packet.r_vec3(out.angles);
		return ai().get_game_graph() && ai().game_graph().valid_vertex_id(out.game_vertex_id);
	}

	LPCSTR DestinationLevelName(const ZonaChangeLevelRequest& req)
	{
		if (!ai().get_game_graph())
			return nullptr;
		const CGameGraph& gg = ai().game_graph();
		if (!gg.valid_vertex_id(req.game_vertex_id))
			return nullptr;
		const u8 lid = gg.vertex(req.game_vertex_id)->level_id();
		const CGameGraph::SLevel& lv = gg.header().level(lid);
		return lv.name().c_str();
	}

	void WorldPositionForRequest(const ZonaChangeLevelRequest& req, Fvector& world_pos, Fvector& world_dir)
	{
		world_pos = req.position;
		world_dir = req.angles;
		if (!ai().get_game_graph())
			return;

		const CGameGraph& gg = ai().game_graph();
		if (gg.valid_vertex_id(req.game_vertex_id))
			world_pos = gg.vertex(req.game_vertex_id)->game_point();

		world_dir.getHP(world_dir.y, world_dir.x);
	}

	bool EnsureZoneLoaded(LPCSTR level_name)
	{
		if (!level_name || !*level_name)
			return false;

		const auto it = g_mounted.find(level_name);
		if (it != g_mounted.end() && it->second)
			return true;

		if (g_pGameLevel && !xr_strcmp(Level().name().c_str(), level_name))
		{
			g_mounted[level_name] = true;
			return true;
		}

		if (IsStreamableLevel(level_name) && g_armed[level_name] && (g_no_loadscreen || g_zona_stream_geom_lab))
		{
			if (ZonaStreamingMount::InPlaceZoneSwap(level_name))
			{
				g_mounted[level_name] = true;
				g_armed.clear();
				return true;
			}
		}

		if (g_zona_stream_geom_lab && IsStreamableLevel(level_name))
		{
			g_mounted[level_name] = true;
			return true;
		}

		if (g_zona_stream_debug)
			Msg("~ [ZONA stream] EnsureZoneLoaded(%s) failed", level_name);

		return false;
	}

	bool SuppressLoadUI()
	{
		return ZonaStreamingMount::SuppressLoadUI();
	}

	bool ServerApplyChangeLevel(const ZonaChangeLevelRequest& req)
	{
		if (!IsActive())
			return false;

		LPCSTR dest = DestinationLevelName(req);
		if (!dest || !IsStreamableLevel(dest))
			return false;

		if (!ai().get_alife())
			return false;

		CSE_ALifeCreatureActor* actor =
		    const_cast<CSE_ALifeCreatureActor*>(ai().alife().graph().actor());
		if (!actor)
			return false;

		if (!EnsureZoneLoaded(dest))
		{
			if (g_zona_stream_debug)
				Msg("~ [ZONA stream] server: dest %s not mounted — vanilla change_level", dest);
			return false;
		}

		Level().ClientSend();

		Fvector wp, wd;
		WorldPositionForRequest(req, wp, wd);

		actor->m_tGraphID = req.game_vertex_id;
		actor->m_tNodeID = req.level_vertex_id;
		actor->o_Position = wp;
		actor->o_Angle = req.angles;
		actor->o_torso.yaw = actor->o_Angle.y;
		actor->o_torso.pitch = actor->o_Angle.x;
		actor->o_torso.roll = 0.f;

		if (CActor* a = Actor())
			a->MoveActor(wp, wd);

		DbgMsg("* [ZONA stream] server handoff → %s (no autosave reconnect)", dest);
		return true;
	}

	bool ClientHandoffChangeLevel(const ZonaChangeLevelRequest& req)
	{
		if (!IsActive())
			return false;

		LPCSTR dest = DestinationLevelName(req);
		if (!dest || !IsStreamableLevel(dest))
			return false;

		if (!EnsureZoneLoaded(dest))
			return false;

		Fvector wp, wd;
		WorldPositionForRequest(req, wp, wd);

		if (CActor* a = Actor())
			a->MoveActor(wp, wd);

		Msg("Device.LuaGC clear");
		Device.LuaGC.clear();
		Device.LuaGCDebug.clear();

		DbgMsg("* [ZONA stream] client handoff → %s (skipped MakeReconnect)", dest);
		return true;
	}

	void OnFrame()
	{
		if (!IsActive() || !g_pGameLevel)
			return;

		CActor* a = Actor();
		if (!a)
			return;

		const Fvector ap = a->Position();
		const shared_str physical = Level().name();
		const float coarse_radius = g_zona_stream_radius_m > 0.f ? g_zona_stream_radius_m : g_load_radius_m;
		const float border_radius = g_border_preload_m > 0.f ? g_border_preload_m : 91.f;

		for (const auto& edge : g_border_edges)
		{
			if (!edge.enabled || !edge.has_hint)
				continue;
			if (physical.size() && xr_strcmp(physical.c_str(), edge.from_level.c_str()))
				continue;
			if (!IsStreamableLevel(edge.to_level.c_str()))
				continue;

			const float dist = ap.distance_to(edge.hint_pos);
			if (dist > border_radius)
				continue;

			shared_str key;
			key.printf("%s->%s", edge.from_level.c_str(), edge.to_level.c_str());
			const bool newly = !g_last_border_preload_key.size() || g_last_border_preload_key != key;
			const u32 now = Device.dwTimeGlobal;
			if (newly || now - g_last_border_preload_log_time > 5000)
			{
				g_last_border_preload_key = key;
				g_last_border_preload_log_time = now;
				DbgMsg("* [ZONA stream] border preload %s (%.0fm) -> %s", key.c_str(), dist,
				       edge.to_level.c_str());
			}
			ArmStreamableHandoff(edge.to_level.c_str());
		}

		for (const auto& z : g_zones)
		{
			if (!z.streamable)
				continue;
			if (physical.size() && !xr_strcmp(physical.c_str(), z.level_name.c_str()))
				continue;

			if (ap.distance_to_sqr(z.offset) <= coarse_radius * coarse_radius)
				EnsureZoneLoaded(z.level_name.c_str());
		}
	}
}
