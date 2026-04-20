#pragma once

#include <Canis/Math.hpp>

namespace Canis
{
    class PostProcessAsset;

    struct RenderTarget
    {
        unsigned int framebuffer = 0;
        unsigned int colorTexture = 0;
        unsigned int depthTexture = 0;
        int width = 0;
        int height = 0;
    };

    struct PostProcessResult
    {
        unsigned int framebuffer = 0;
        unsigned int colorTexture = 0;
    };

    void EnsureRenderTarget(RenderTarget &_target, int _width, int _height);
    void DestroyRenderTarget(RenderTarget &_target);

    PostProcessResult ApplyPostProcessChain(
        const PostProcessAsset *_asset,
        unsigned int _sourceFramebuffer,
        unsigned int _sourceColorTexture,
        unsigned int _sourceDepthTexture,
        int _width,
        int _height,
        const Matrix4 &_projection,
        RenderTarget *_outputTarget = nullptr);

    void BlitFramebuffer(unsigned int _sourceFramebuffer, int _sourceWidth, int _sourceHeight, unsigned int _destinationFramebuffer, int _destinationWidth, int _destinationHeight);
} // namespace Canis
