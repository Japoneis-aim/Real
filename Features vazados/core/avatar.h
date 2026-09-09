#pragma once
#include <imgui.h>
#include <vector>
#include <cstdint>

namespace Avatar {

    inline std::vector<uint8_t> g_pngBytes;
    inline ImTextureID           g_texture = 0;
    inline int                   g_width = 0, g_height = 0;
    inline bool                  g_decodeAttempted = false;

    inline void SetFromPNG(const uint8_t* data, size_t size) {
        if (!data || size == 0) return;
        g_pngBytes.assign(data, data + size);
        g_decodeAttempted = false;
        g_texture = 0;
    }

    inline void SetFromPNG(const std::vector<uint8_t>& data) {
        SetFromPNG(data.data(), data.size());
    }

    inline bool        HasAvatar()  { return g_texture != 0; }
    inline ImTextureID GetTexture() { return g_texture; }
    inline int         GetWidth()   { return g_width; }
    inline int         GetHeight()  { return g_height; }

    void TickUpload();

}
