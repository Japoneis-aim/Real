#include <core/avatar.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include <stb_image.h>

#include <Windows.h>
#include <LazyDlls/Lazyimporter.hpp>

namespace Avatar {

    namespace {
        constexpr unsigned ZM_GL_TEXTURE_2D         = 0x0DE1;
        constexpr unsigned ZM_GL_RGBA               = 0x1908;
        constexpr unsigned ZM_GL_LINEAR             = 0x2601;
        constexpr unsigned ZM_GL_UNSIGNED_BYTE      = 0x1401;
        constexpr unsigned ZM_GL_TEXTURE_MIN_FILTER = 0x2801;
        constexpr unsigned ZM_GL_TEXTURE_MAG_FILTER = 0x2800;

        typedef void(__stdcall* GLGenTexturesFn)  (int, unsigned*);
        typedef void(__stdcall* GLBindTextureFn)  (unsigned, unsigned);
        typedef void(__stdcall* GLTexParameteriFn)(unsigned, unsigned, int);
        typedef void(__stdcall* GLTexImage2DFn)   (unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
    }

    void TickUpload() {
        if (g_decodeAttempted) return;
        if (g_pngBytes.empty()) return;
        g_decodeAttempted = true;

        int w = 0, h = 0, ch = 0;
        unsigned char* pixels = stbi_load_from_memory(
            g_pngBytes.data(), static_cast<int>(g_pngBytes.size()), &w, &h, &ch, 4);
        if (!pixels) { g_pngBytes.clear(); return; }

        // GetModuleHandleA/GetProcAddress lazyfied — opengl32.dll ja' carregada
        // pelo imgui backend, so resolvemos os exports via cache.
        HMODULE mod = LI_CACHED(GetModuleHandleA)("opengl32.dll");
        if (!mod) { stbi_image_free(pixels); g_pngBytes.clear(); return; }
        auto fnGen   = reinterpret_cast<GLGenTexturesFn>  (LI_CACHED(GetProcAddress)(mod, "glGenTextures"));
        auto fnBind  = reinterpret_cast<GLBindTextureFn>  (LI_CACHED(GetProcAddress)(mod, "glBindTexture"));
        auto fnParam = reinterpret_cast<GLTexParameteriFn>(LI_CACHED(GetProcAddress)(mod, "glTexParameteri"));
        auto fnImage = reinterpret_cast<GLTexImage2DFn>   (LI_CACHED(GetProcAddress)(mod, "glTexImage2D"));
        if (!fnGen || !fnBind || !fnParam || !fnImage) {
            stbi_image_free(pixels);
            g_pngBytes.clear();
            return;
        }

        unsigned tex = 0;
        fnGen(1, &tex);
        fnBind(ZM_GL_TEXTURE_2D, tex);
        fnParam(ZM_GL_TEXTURE_2D, ZM_GL_TEXTURE_MIN_FILTER, static_cast<int>(ZM_GL_LINEAR));
        fnParam(ZM_GL_TEXTURE_2D, ZM_GL_TEXTURE_MAG_FILTER, static_cast<int>(ZM_GL_LINEAR));
        fnImage(ZM_GL_TEXTURE_2D, 0, static_cast<int>(ZM_GL_RGBA), w, h, 0, ZM_GL_RGBA, ZM_GL_UNSIGNED_BYTE, pixels);

        g_texture = static_cast<ImTextureID>(tex);
        g_width   = w;
        g_height  = h;

        stbi_image_free(pixels);
        g_pngBytes.clear();
    }

}
