////////////////////////////////////////////////////////////////////////////
// ZONA — in-place zone swap (no KERNEL:disconnect, no load screen UI)
////////////////////////////////////////////////////////////////////////////
#pragma once

namespace ZonaStreamingMount
{
	bool SuppressLoadUI();

	// Swap active $level$ render + collision to dest (single-zone-at-a-time POC).
	bool InPlaceZoneSwap(LPCSTR dest_level_name);
}
