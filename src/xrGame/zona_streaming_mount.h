////////////////////////////////////////////////////////////////////////////
// ZONA — in-place zone swap (no KERNEL:disconnect, no load screen UI)
////////////////////////////////////////////////////////////////////////////
#pragma once

namespace ZonaStreamingMount
{
	bool SuppressLoadUI();

	// Swap active $level$ render + collision to dest (single-zone-at-a-time POC).
	bool InPlaceZoneSwap(LPCSTR dest_level_name);

	// Keep the current map and add dest collision shifted into this map's space.
	// offset_delta = dest_zone_offset - current_zone_offset (LAMBDA world offsets).
	bool MountNeighborCollision(LPCSTR dest_level_name, const Fvector& offset_delta);
}
