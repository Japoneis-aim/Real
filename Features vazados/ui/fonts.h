#pragma once
#include <imgui.h>
#include <imgui_freetype.h>
#include "fonts/inter.h"
#include "fonts/stolzl.h"
#include "fonts/fa6_bytes.h"
#include "fonts/fa6_icons.h"
#include "fonts/raiox.h"

#ifdef IMGUI_IMPL_OPENGL_USE_GLFW
#include <GLFW/glfw3.h>
#else
#include <Windows.h>
#include <LazyDlls/Lazyimporter.hpp>
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
#define GL_TEXTURE_2D     0x0DE1
#define GL_RGBA           0x1908
#define GL_UNSIGNED_BYTE  0x1401
#define GL_LINEAR         0x2601
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800

// Lazy GL — evita dllimport estatico de OPENGL32. opengl32.dll ja foi
// carregada pelo wglCreateContext em ogl::Init. Cache de fn pointer
// resolvido por GetProcAddress (idem padrao de avatar.cpp).
namespace UI_lazy_gl {
    typedef void (__stdcall *PFN_glGenTextures)(GLsizei, GLuint*);
    typedef void (__stdcall *PFN_glBindTexture)(GLenum, GLuint);
    typedef void (__stdcall *PFN_glTexParameteri)(GLenum, GLenum, GLint);
    typedef void (__stdcall *PFN_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    inline HMODULE GetGL() {
        static HMODULE h = LI_CACHED(GetModuleHandleA)("opengl32.dll");
        if (!h) h = LI_CACHED(LoadLibraryA)("opengl32.dll");
        return h;
    }
    inline void glGenTextures(GLsizei n, GLuint* t) {
        static auto f = (PFN_glGenTextures)LI_CACHED(GetProcAddress)(GetGL(), "glGenTextures"); if (f) f(n, t);
    }
    inline void glBindTexture(GLenum tg, GLuint x) {
        static auto f = (PFN_glBindTexture)LI_CACHED(GetProcAddress)(GetGL(), "glBindTexture"); if (f) f(tg, x);
    }
    inline void glTexParameteri(GLenum tg, GLenum pn, GLint v) {
        static auto f = (PFN_glTexParameteri)LI_CACHED(GetProcAddress)(GetGL(), "glTexParameteri"); if (f) f(tg, pn, v);
    }
    inline void glTexImage2D(GLenum tg, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum fm, GLenum ty, const void* p) {
        static auto f = (PFN_glTexImage2D)LI_CACHED(GetProcAddress)(GetGL(), "glTexImage2D"); if (f) f(tg, l, i, w, h, b, fm, ty, p);
    }
}
using UI_lazy_gl::glGenTextures;
using UI_lazy_gl::glBindTexture;
using UI_lazy_gl::glTexParameteri;
using UI_lazy_gl::glTexImage2D;
#endif

namespace UI {

    namespace Fonts {
        inline ImFont* Normal = nullptr;
        inline ImFont* Big = nullptr;
        inline ImFont* Title = nullptr;
        inline ImFont* Small = nullptr;
        inline ImFont* Icons = nullptr;
        inline ImFont* IconsBig = nullptr;
    }

    inline ImTextureID LogoTexture = 0;
    inline int LogoWidth = 0;
    inline int LogoHeight = 0;

    inline void LoadLogo() {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, raiox_width, raiox_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, raiox_rgba);
        glBindTexture(GL_TEXTURE_2D, 0);
        LogoTexture = (ImTextureID)tex;
        LogoWidth = raiox_width;
        LogoHeight = raiox_height;
    }

    inline void LoadFonts() {
        ImGuiIO& io = ImGui::GetIO();

        io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
        io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting;

        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 2;
        cfg.PixelSnapH = false;

        // Sizes do amigo: Mini=13 / Normal=14 / Big=16 (Custom.hpp:114-116).
        // Subimos 1px em cada porque Stolzl tem x-height menor que Inter e
        // texto estava sumindo pequeno demais.
        Fonts::Normal = io.Fonts->AddFontFromMemoryCompressedTTF(
            StolzlRegular_compressed_data, StolzlRegular_compressed_size, 15.f, &cfg);
        Fonts::Big = io.Fonts->AddFontFromMemoryCompressedTTF(
            StolzlRegular_compressed_data, StolzlRegular_compressed_size, 17.f, &cfg);
        Fonts::Title = io.Fonts->AddFontFromMemoryCompressedTTF(
            StolzlRegular_compressed_data, StolzlRegular_compressed_size, 28.f, &cfg);
        Fonts::Small = io.Fonts->AddFontFromMemoryCompressedTTF(
            StolzlRegular_compressed_data, StolzlRegular_compressed_size, 13.f, &cfg);

        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig iconCfg;
        iconCfg.MergeMode = false;
        iconCfg.PixelSnapH = true;

        // FA sizes do amigo: Normal=17 / Big=22 (Custom.hpp:120-121).
        Fonts::Icons = io.Fonts->AddFontFromMemoryCompressedTTF(
            FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size,
            17.f, &iconCfg, iconRanges);
        Fonts::IconsBig = io.Fonts->AddFontFromMemoryCompressedTTF(
            FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size,
            22.f, &iconCfg, iconRanges);
    }

}
