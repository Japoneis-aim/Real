#pragma once
#include <atomic>
#include <shared_mutex>
#include <external/zdraw/zdraw.hpp>

namespace features::misc {

struct wallbang_result {
	bool can_penetrate = false;
	float damage = 0.f;
	float thickness = 0.f;
	bool hit_player = false;
	float hit_distance = 0.f;
};

class wallbang_indicator {
public:
	void tick();
	void on_render(zdraw::draw_list& draw_list);

	void set_enabled(bool enabled) { m_enabled = enabled; }
	bool is_enabled() const { return m_enabled; }

private:
	std::atomic<bool> m_enabled{ false };
	mutable std::shared_mutex m_result_mutex{};
	wallbang_result m_result{};
	float m_anim_alpha = 0.f;
	float m_damage_anim = 0.f;

	void check_wallbang();
	void draw_crosshair(zdraw::draw_list& dl, float cx, float cy);
	void draw_damage_text(zdraw::draw_list& dl, float cx, float cy);
};

inline wallbang_indicator g_wallbang{};

}
