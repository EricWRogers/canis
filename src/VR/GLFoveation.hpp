#pragma once
#include <Canis/VR/Foveation.hpp>
#include <Canis/OpenGL.hpp>
#include <string>

namespace Canis::VR
{
    // Experimental NVIDIA desktop path. No claim that XR_FB_foveation works on
    // desktop GL. Destruction occurs while the owning GL context is current.
    class GLFoveation
    {
#ifndef __EMSCRIPTEN__
        GLuint texture = 0;
        GLint columns = 0, rows = 0, oldBinding = 0;
        GLenum oldPalette[3]{};
        bool wasEnabled = false, active = false;
#endif
    public:
        ~GLFoveation() { Reset(); }
        void Reset()
        {
#ifndef __EMSCRIPTEN__
            End();
            if (texture) glDeleteTextures(1, &texture);
            texture = 0; columns = rows = 0;
#endif
        }
        bool Supported() const
        {
#ifndef __EMSCRIPTEN__
            // Experimental GLEW can resolve entry points even when the driver
            // does not advertise the extension (for example Mesa on AMD).
            return glewGetExtension("GL_NV_shading_rate_image") && glBindShadingRateImageNV &&
                glShadingRateImagePaletteNV && glGetShadingRateImagePaletteNV && glTexStorage2D;
#else
            return false;
#endif
        }
        bool Begin(int width, int height, Vector2 center)
        {
#ifndef __EMSCRIPTEN__
            if (!Supported()) return false;
            GLint tileWidth = 0, tileHeight = 0, paletteSize = 0;
            glGetIntegerv(GL_SHADING_RATE_IMAGE_TEXEL_WIDTH_NV, &tileWidth);
            glGetIntegerv(GL_SHADING_RATE_IMAGE_TEXEL_HEIGHT_NV, &tileHeight);
            glGetIntegerv(GL_SHADING_RATE_IMAGE_PALETTE_SIZE_NV, &paletteSize);
            if (tileWidth <= 0 || tileHeight <= 0 || paletteSize < 3) return false;
            auto map = ShadingRateMap(width, height, tileWidth, tileHeight, center);
            int newColumns = (width+tileWidth-1)/tileWidth, newRows = (height+tileHeight-1)/tileHeight;
            GLint oldTexture = 0, unpack = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture); glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
            if (!texture || newColumns != columns || newRows != rows)
            {
                if (texture) glDeleteTextures(1, &texture);
                glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
                glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, newColumns, newRows);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                columns = newColumns; rows = newRows;
            }
            GLint rowLength=0, skipRows=0, skipPixels=0, unpackBuffer=0;
            glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowLength); glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels); glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
            glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
            glBindTexture(GL_TEXTURE_2D, texture); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, columns, rows, GL_RED_INTEGER, GL_UNSIGNED_BYTE, map.data());
            glBindTexture(GL_TEXTURE_2D, oldTexture); glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
            glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength); glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
            glGetIntegerv(GL_SHADING_RATE_IMAGE_BINDING_NV, &oldBinding);
            wasEnabled = glIsEnabled(GL_SHADING_RATE_IMAGE_NV);
            for (GLuint i = 0; i < 3; ++i) glGetShadingRateImagePaletteNV(0, i, &oldPalette[i]);
            const GLenum palette[] = {GL_SHADING_RATE_1_INVOCATION_PER_PIXEL_NV,
                GL_SHADING_RATE_1_INVOCATION_PER_2X2_PIXELS_NV, GL_SHADING_RATE_1_INVOCATION_PER_4X4_PIXELS_NV};
            glShadingRateImagePaletteNV(0, 0, 3, palette);
            glBindShadingRateImageNV(texture); glEnable(GL_SHADING_RATE_IMAGE_NV); active = true;
            return true;
#else
            (void)width; (void)height; (void)center; return false;
#endif
        }
        void End()
        {
#ifndef __EMSCRIPTEN__
            if (!active) return;
            glBindShadingRateImageNV(oldBinding);
            glShadingRateImagePaletteNV(0, 0, 3, oldPalette);
            if (wasEnabled) glEnable(GL_SHADING_RATE_IMAGE_NV); else glDisable(GL_SHADING_RATE_IMAGE_NV);
            active = false;
#endif
        }
    };
}
