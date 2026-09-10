#pragma once
#include <core/systems/systems.hpp>
#include <external/zdraw/zdraw.hpp>

namespace features::misc {

    class radar
    {
    public:
        void tick( );
        void on_render( zdraw::draw_list& draw_list );
    };

    inline radar g_radar{};

} // namespace features::misc
