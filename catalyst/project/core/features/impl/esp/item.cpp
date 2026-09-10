#include <stdafx.hpp>

namespace features::esp {

	void item::on_render( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_esp.m_item;
		if ( !cfg.enabled )
			return;

		const auto view_origin = systems::g_view.origin( );

		systems::g_collector.with_items( [&]( const std::vector<systems::collector::item>& items )
		{
		for ( const auto& item : items )
		{
			const auto distance = view_origin.distance( item.origin ) * 0.01905f;
			if ( distance > cfg.max_distance )
				continue;

			if ( !this->passes_filter( item.subtype, cfg.m_filters ) )
				continue;

			const auto screen = systems::g_view.project( item.origin );
			if ( !systems::g_view.projection_valid( screen ) )
				continue;

			auto y_offset{ 0.0f };

			if ( cfg.m_icon.enabled )
				this->add_icon( draw_list, screen, item, cfg.m_icon, y_offset );

			if ( cfg.m_name.enabled )
				this->add_name( draw_list, screen, item, cfg.m_name, y_offset );

			if ( cfg.m_ammo.enabled && item.max_ammo > 0 )
				this->add_ammo( draw_list, screen, item, cfg.m_ammo, y_offset );
		}
		} ); // with_items
	}

	void item::add_icon( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::icon& cfg, float& y_offset )
	{
		const auto* ico = systems::g_icons.get( this->get_icon( item.subtype ) );
		if ( !ico || !ico->texture )
		{
			return;
		}

		constexpr auto target_h{ 16.0f };
		const auto target_w = ico->height > 0 ? target_h * static_cast< float >( ico->width ) / static_cast< float >( ico->height ) : target_h;
		const auto x = std::floorf( screen.x - target_w * 0.5f );
		const auto y = std::floorf( screen.y - target_h * 0.5f + y_offset );
		constexpr auto outline = zdraw::rgba{ 0, 0, 0, 255 };

		draw_list.add_rect_textured( x - 1.0f, y, target_w, target_h, ico->texture.Get( ), 0.0f, 0.0f, 1.0f, 1.0f, outline );
		draw_list.add_rect_textured( x + 1.0f, y, target_w, target_h, ico->texture.Get( ), 0.0f, 0.0f, 1.0f, 1.0f, outline );
		draw_list.add_rect_textured( x, y - 1.0f, target_w, target_h, ico->texture.Get( ), 0.0f, 0.0f, 1.0f, 1.0f, outline );
		draw_list.add_rect_textured( x, y + 1.0f, target_w, target_h, ico->texture.Get( ), 0.0f, 0.0f, 1.0f, 1.0f, outline );
		draw_list.add_rect_textured( x, y, target_w, target_h, ico->texture.Get( ), 0.0f, 0.0f, 1.0f, 1.0f, cfg.color );

		y_offset += target_h - 5.5f;
	}

	void item::add_name( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::name& cfg, float& y_offset )
	{
		zdraw::push_font( g::render.fonts( ).pixel7_10 );

		const auto name = this->get_display_name( item.subtype );
		const auto [text_w, text_h] = zdraw::measure_text( name );
		const auto x = std::floorf( screen.x - text_w * 0.5f );
		const auto y = std::floorf( screen.y + y_offset );

		draw_list.add_text( x, y, name, nullptr, cfg.color, zdraw::text_style::outlined );

		zdraw::pop_font( );

		y_offset += text_h - 5.5f;
	}

	void item::add_ammo( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::ammo& cfg, float& y_offset )
	{
		zdraw::push_font( g::render.fonts( ).pixel7_10 );

		const auto text = std::format( "{}/{}", std::clamp( item.ammo, 0, item.max_ammo ), item.max_ammo );
		const auto [text_w, text_h] = zdraw::measure_text( text );
		const auto x = std::floorf( screen.x - text_w * 0.5f );
		const auto y = std::floorf( screen.y + y_offset );

		const auto fraction = static_cast< float >( std::clamp( item.ammo, 0, item.max_ammo ) ) / item.max_ammo;
		const auto color = fraction > 0.0f ? cfg.color.value : cfg.empty_color.value;

		draw_list.add_text( x, y, text, nullptr, color, zdraw::text_style::outlined );

		zdraw::pop_font( );

		y_offset += text_h;
	}

	bool item::passes_filter( systems::collector::item_subtype subtype, const settings::esp::item::filters& filters ) const
	{
		const auto cat = this->get_category( subtype );
		switch ( cat )
		{
		case category::rifle:   return filters.rifles;
		case category::smg:     return filters.smgs;
		case category::shotgun: return filters.shotguns;
		case category::sniper:  return filters.snipers;
		case category::pistol:  return filters.pistols;
		case category::heavy:   return filters.heavy;
		case category::grenade: return filters.grenades;
		case category::utility: return filters.utility;
		}
		return false;
	}

	item::category item::get_category( systems::collector::item_subtype subtype ) const
	{
		using st = systems::collector::item_subtype;

		switch ( subtype )
		{
		case st::ak47:
		case st::m4a4:
		case st::m4a1s:
		case st::aug:
		case st::famas:
		case st::galil_ar:
		case st::sg553:
			return category::rifle;

		case st::mac10:
		case st::mp5sd:
		case st::mp7:
		case st::mp9:
		case st::pp_bizon:
		case st::p90:
		case st::ump45:
			return category::smg;

		case st::nova:
		case st::sawed_off:
		case st::xm1014:
		case st::mag7:
			return category::shotgun;

		case st::awp:
		case st::ssg08:
		case st::g3sg1:
		case st::scar20:
			return category::sniper;

		case st::deagle:
		case st::dual_berettas:
		case st::five_seven:
		case st::glock:
		case st::p2000:
		case st::usps:
		case st::p250:
		case st::cz75:
		case st::tec9:
		case st::r8_revolver:
			return category::pistol;

		case st::m249:
		case st::negev:
			return category::heavy;

		case st::he_grenade:
		case st::flashbang:
		case st::smoke_grenade:
		case st::molotov:
		case st::incendiary:
		case st::decoy:
			return category::grenade;

		case st::c4:
		case st::taser:
		case st::healthshot:
		case st::knife:
			return category::utility;

		default:
			return category::utility;
		}
	}

	std::string item::get_icon( systems::collector::item_subtype subtype ) const
	{
		using st = systems::collector::item_subtype;
		switch ( subtype )
		{
		case st::ak47:          return "ak47";
		case st::m4a4:          return "m4a1";
		case st::m4a1s:         return "m4a1_silencer";
		case st::aug:           return "aug";
		case st::famas:         return "famas";
		case st::galil_ar:      return "galilar";
		case st::sg553:         return "sg556";
		case st::awp:           return "awp";
		case st::ssg08:         return "ssg08";
		case st::g3sg1:         return "g3sg1";
		case st::scar20:        return "scar20";
		case st::mac10:         return "mac10";
		case st::mp5sd:         return "mp5sd";
		case st::mp7:           return "mp7";
		case st::mp9:           return "mp9";
		case st::pp_bizon:      return "bizon";
		case st::p90:           return "p90";
		case st::ump45:         return "ump45";
		case st::nova:          return "nova";
		case st::sawed_off:     return "sawedoff";
		case st::xm1014:        return "xm1014";
		case st::mag7:          return "mag7";
		case st::m249:          return "m249";
		case st::negev:         return "negev";
		case st::deagle:        return "deagle";
		case st::dual_berettas: return "elite";
		case st::five_seven:    return "fiveseven";
		case st::glock:         return "glock";
		case st::p2000:         return "hkp2000";
		case st::usps:          return "usp_silencer";
		case st::p250:          return "p250";
		case st::cz75:          return "cz75a";
		case st::tec9:          return "tec9";
		case st::r8_revolver:   return "revolver";
		case st::taser:         return "taser";
		case st::c4:            return "c4";
		case st::he_grenade:    return "hegrenade";
		case st::flashbang:     return "flashbang";
		case st::smoke_grenade: return "smokegrenade";
		case st::molotov:       return "molotov";
		case st::incendiary:    return "incgrenade";
		case st::decoy:         return "decoy";
		case st::knife:         return "knife";
		case st::healthshot:    return "healthshot";
		default:                return "";
		}
	}

	std::string item::get_display_name( systems::collector::item_subtype subtype ) const
	{
		using st = systems::collector::item_subtype;
		switch ( subtype )
		{
		case st::ak47:          return "ak47";
		case st::m4a4:          return "m4a4";
		case st::m4a1s:         return "m4a1-s";
		case st::aug:           return "aug";
		case st::famas:         return "famas";
		case st::galil_ar:      return "galil";
		case st::sg553:         return "sg553";
		case st::awp:           return "awp";
		case st::ssg08:         return "scout";
		case st::g3sg1:         return "g3sg1";
		case st::scar20:        return "scar";
		case st::mac10:         return "mac10";
		case st::mp5sd:         return "mp5";
		case st::mp7:           return "mp7";
		case st::mp9:           return "mp9";
		case st::pp_bizon:      return "bizon";
		case st::p90:           return "p90";
		case st::ump45:         return "ump";
		case st::nova:          return "nova";
		case st::sawed_off:     return "sawed";
		case st::xm1014:        return "xm1014";
		case st::mag7:          return "mag7";
		case st::m249:          return "m249";
		case st::negev:         return "negev";
		case st::deagle:        return "deagle";
		case st::dual_berettas: return "dualies";
		case st::five_seven:    return "five7";
		case st::glock:         return "glock";
		case st::p2000:         return "p2000";
		case st::usps:          return "usp-s";
		case st::p250:          return "p250";
		case st::cz75:          return "cz75";
		case st::tec9:          return "tec9";
		case st::r8_revolver:   return "r8";
		case st::taser:         return "taser";
		case st::c4:            return "c4";
		case st::he_grenade:    return "he";
		case st::flashbang:     return "flash";
		case st::smoke_grenade: return "smoke";
		case st::molotov:       return "molotov";
		case st::incendiary:    return "incendiary";
		case st::decoy:         return "decoy";
		case st::knife:         return "knife";
		case st::healthshot:    return "medkit";
		default:                return "unknown";
		}
	}

} // namespace features::esp