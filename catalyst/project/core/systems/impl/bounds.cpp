#include <stdafx.hpp>

namespace systems {

	bool bounds::data::is_valid( ) const
	{
		return this->min.x != 0xdead;
	}

	bounds::data bounds::get( const bones::data& bone_data ) const
	{
		// Índices do backup original (confirmados como funcionando):
		//  6  = head
		//  8  = shoulder_L
		// 12  = shoulder_R
		// 19  = foot_L
		// 22  = foot_R

		const auto head   = bone_data.get_position( 6 );
		const auto sh_l   = bone_data.get_position( 8 );
		const auto sh_r   = bone_data.get_position( 12 );
		const auto foot_l = bone_data.get_position( 19 );
		const auto foot_r = bone_data.get_position( 22 );

		if ( head.length_sqr( ) < 1.0f )
			return { { 0xdead, 0xdead }, {} };

		const auto s_head = g_view.project( head );
		if ( !g_view.projection_valid( s_head ) )
			return { { 0xdead, 0xdead }, {} };

		float screen_top    = s_head.y;
		float screen_bottom = s_head.y;

		const auto s_foot_l = g_view.project( foot_l );
		const auto s_foot_r = g_view.project( foot_r );
		bool has_feet = false;

		if ( foot_l.length_sqr( ) > 1.0f && g_view.projection_valid( s_foot_l ) )
		{ screen_bottom = std::max( screen_bottom, s_foot_l.y ); has_feet = true; }
		if ( foot_r.length_sqr( ) > 1.0f && g_view.projection_valid( s_foot_r ) )
		{ screen_bottom = std::max( screen_bottom, s_foot_r.y ); has_feet = true; }

		if ( !has_feet )
		{
			const auto pelvis   = bone_data.get_position( 1 );
			const auto s_pelvis = g_view.project( pelvis );
			if ( pelvis.length_sqr( ) > 1.0f && g_view.projection_valid( s_pelvis ) )
			{
				const float head_to_pelvis = s_pelvis.y - s_head.y;
				screen_bottom = s_pelvis.y + head_to_pelvis;
			}
			else
			{
				screen_bottom = s_head.y + std::max( 20.0f, ( s_head.y ) * 0.12f );
			}
		}

		// top_pad cobre o raio do hitbox da cabeça (osso projeta no centro da cabeça, não no topo)
		constexpr float top_pad = 9.0f;
		constexpr float bot_pad = 4.0f;
		screen_top    -= top_pad;
		screen_bottom += bot_pad;

		const float screen_height = screen_bottom - screen_top;
		if ( screen_height < 4.0f )
			return { { 0xdead, 0xdead }, {} };

		const auto s_sh_l = g_view.project( sh_l );
		const auto s_sh_r = g_view.project( sh_r );

		float cx;
		float half_w;

		if ( sh_l.length_sqr( ) > 1.0f && sh_r.length_sqr( ) > 1.0f
			&& g_view.projection_valid( s_sh_l ) && g_view.projection_valid( s_sh_r ) )
		{
			// Centro horizontal = média dos ombros (mais preciso que usar só a cabeça)
			cx = ( s_sh_l.x + s_sh_r.x ) * 0.5f;

			// side_pad maior para cobrir braços/corpo que ficam além dos ombros
			constexpr float side_pad = 10.0f;
			half_w = std::abs( s_sh_r.x - s_sh_l.x ) * 0.5f + side_pad;
		}
		else
		{
			cx     = s_head.x;
			// Sem ombros: proporcional à altura, fator 0.35 cobre melhor o corpo inteiro
			half_w = screen_height * 0.35f;
		}

		const math::vector2 out_min{ cx - half_w, screen_top    };
		const math::vector2 out_max{ cx + half_w, screen_bottom };

		return { out_min, out_max };
	}

} // namespace systems
