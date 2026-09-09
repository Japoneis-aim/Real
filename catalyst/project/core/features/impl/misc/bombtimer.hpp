#pragma once
#include <external/zdraw/zdraw.hpp>

namespace features::misc {

class bomb_timer {
public:
	void tick();
	void on_render(zdraw::draw_list& draw_list);

private:
	struct info {
		bool planted = false;
		bool defused = false;
		bool defusing = false;
		float time_left = 0.f;
		float defuse_left = 0.f;
		int site = 0;
		int damage = 0;
		std::string defuser_name;
	};

	info m_info{};
	float m_anim_alpha = 0.f;
};

inline bomb_timer g_bomb_timer{};

}
