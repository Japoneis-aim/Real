#pragma once

namespace core::render {

class stream_mode {
public:
	void toggle(bool enabled);
	bool is_enabled() const { return m_enabled; }

private:
	bool m_enabled = false;
};

inline stream_mode g_stream_mode{};

}
