////////////////////////////////////////////////////////////////////////////
// ZONA — in-place zone swap (no full reconnect)
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "zona_streaming_mount.h"
#include "Level.h"
#include "../xrEngine/x_ray.h"
#include "../xrCore/xrCore.h"
#include "../xrEngine/render.h"
#include "../xrEngine/IGame_Level.h"
#include "../xrEngine/xrLevel.h"
#include "../xrCDB/xr_area.h"
namespace
{
	BOOL g_suppress_load_ui = FALSE;
	shared_str g_mounted_neighbor;

	LPCSTR DefaultLevelVersion(LPCSTR level_name)
	{
		// Anomaly overworld packs typically register as 1.0; server options carry map_ver when needed.
		if (CInifile* hdr = pApp->GetArchiveHeader(level_name, "1.0"))
			return "1.0";
		return "1.0";
	}

	static void __stdcall zona_cform_callback(Fvector* V, int Vcnt, CDB::TRI* T, int Tcnt, void*)
	{
		if (g_pGameLevel)
			g_pGameLevel->Load_GameSpecific_CFORM(T, Tcnt);
	}
}

namespace ZonaStreamingMount
{
	bool SuppressLoadUI()
	{
		return g_suppress_load_ui != FALSE;
	}

	bool InPlaceZoneSwap(LPCSTR dest_level_name)
	{
		if (!dest_level_name || !*dest_level_name || !g_pGameLevel)
			return false;

		LPCSTR ver = DefaultLevelVersion(dest_level_name);
		const int level_index = pApp->Level_ID(dest_level_name, ver, true);
		if (level_index < 0)
		{
			Msg("! [ZONA stream] InPlaceZoneSwap: unknown level %s", dest_level_name);
			return false;
		}

		if (Level().name().size() && 0 == stricmp(Level().name().c_str(), dest_level_name))
			return true;

		Msg("* [ZONA stream] InPlaceZoneSwap → %s", dest_level_name);

		g_suppress_load_ui = TRUE;

		Render->level_Unload();

		pApp->Level_Set(level_index);

		g_pGameLevel->ObjectSpace.Load(zona_cform_callback);

		IReader* level_stream = FS.r_open("$level$", "level");
		if (!level_stream)
		{
			g_suppress_load_ui = FALSE;
			Msg("! [ZONA stream] InPlaceZoneSwap: missing level blob for %s", dest_level_name);
			return false;
		}

		Render->level_Load(level_stream);
		FS.r_close(level_stream);

		Level().SetStreamingLevelName(dest_level_name);

		g_suppress_load_ui = FALSE;

		Msg("* [ZONA stream] InPlaceZoneSwap complete (%s)", dest_level_name);
		g_mounted_neighbor = "";
		return true;
	}

	bool MountNeighborCollision(LPCSTR dest_level_name, const Fvector& offset_delta)
	{
		if (!dest_level_name || !*dest_level_name || !g_pGameLevel)
			return false;
		if (g_mounted_neighbor.size())
			return 0 == stricmp(g_mounted_neighbor.c_str(), dest_level_name);

		if (pApp->Level_ID(dest_level_name, DefaultLevelVersion(dest_level_name), false) < 0)
		{
			Msg("! [ZONA stream] neighbor collision: unknown level %s", dest_level_name);
			return false;
		}

		string_path rel;
		xr_sprintf(rel, "%s\\level.cform", dest_level_name);
		IReader* F = FS.r_open("$game_levels$", rel);
		if (!F)
		{
			Msg("! [ZONA stream] neighbor collision: missing %s", rel);
			return false;
		}

		hdrCFORM H;
		F->r(&H, sizeof(H));
		if (H.version != CFORM_CURRENT_VERSION || H.vertcount < 4 || H.facecount < 2)
		{
			FS.r_close(F);
			Msg("! [ZONA stream] neighbor collision: bad cform %s", dest_level_name);
			return false;
		}

		xr_vector<Fvector> verts(H.vertcount);
		xr_vector<CDB::TRI> tris(H.facecount);
		F->r(verts.data(), H.vertcount * sizeof(Fvector));
		F->r(tris.data(), H.facecount * sizeof(CDB::TRI));
		FS.r_close(F);

		for (Fvector& v : verts)
			v.add(offset_delta);

		g_pGameLevel->Load_GameSpecific_CFORM(tris.data(), H.facecount);
		g_pGameLevel->ObjectSpace.AppendStatic(verts.data(), int(verts.size()), tris.data(), int(tris.size()));

		g_mounted_neighbor = dest_level_name;
		Msg("* [ZONA stream] neighbor collision mounted: %s offset %.0f %.0f %.0f (%u tris)",
		    dest_level_name, offset_delta.x, offset_delta.y, offset_delta.z, H.facecount);
		return true;
	}
}
