#include <stdafx.hpp>
#include "stream_mode.hpp"
#include <Windows.h>

namespace core::render {

void stream_mode::toggle(bool enabled) {
	if (m_enabled == enabled) return;

	m_enabled = enabled;

	auto hwnd = g::render.hwnd();
	if (!hwnd) {
		g::console.print("[Stream Mode] No window handle found");
		return;
	}

	// SetWindowDisplayAffinity
	// 0x11 = WDA_MONITOR | WDA_EXCLUDEFROMCAPTURE
	BOOL result = SetWindowDisplayAffinity(hwnd, enabled ? 0x11 : 0x0);

	if (result) {
		g::console.print("[Stream Mode] {}", enabled ? "ON" : "OFF");
	} else {
		g::console.print("[Stream Mode] Failed to set display affinity (error: {})", GetLastError());
	}
}

} // namespace core::render
