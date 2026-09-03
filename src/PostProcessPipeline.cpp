#include <Canis/PostProcessPipeline.hpp>

#include <Canis/Asset.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Shader.hpp>
#include <Canis/Time.hpp>

#include <algorithm>

namespace Canis
{
    namespace
    {
        struct InternalTarget
        {
            unsigned int framebuffer = 0;
            unsigned int colorTexture = 0;
            int width = 0;
            int height = 0;
        };

        InternalTarget g_pingTarget = {};
        InternalTarget g_pongTarget = {};
        unsigned int g_fullscreenVao = 0;
        unsigned int g_fullscreenVbo = 0;

        void EnsureFullscreenGeometry()
        {
            if (g_fullscreenVao == 0)
                glGenVertexArrays(1, &g_fullscreenVao);
            if (g_fullscreenVbo == 0)
                glGenBuffers(1, &g_fullscreenVbo);

            static const float kFullscreenVertices[] = {
                // pos              // uv
                -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
                 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
                 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
                -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
                 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
                -1.0f,  1.0f, 0.0f,  0.0f, 1.0f
            };

            glBindVertexArray(g_fullscreenVao);
            glBindBuffer(GL_ARRAY_BUFFER, g_fullscreenVbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(kFullscreenVertices), kFullscreenVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        void EnsureInternalTarget(InternalTarget &_target, int _width, int _height)
        {
            if (_width <= 0 || _height <= 0)
                return;

            if (_target.framebuffer != 0 && _target.width == _width && _target.height == _height)
                return;

            if (_target.colorTexture != 0)
            {
                glDeleteTextures(1, &_target.colorTexture);
                _target.colorTexture = 0;
            }

            if (_target.framebuffer != 0)
            {
                glDeleteFramebuffers(1, &_target.framebuffer);
                _target.framebuffer = 0;
            }

            glGenFramebuffers(1, &_target.framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, _target.framebuffer);

            glGenTextures(1, &_target.colorTexture);
            glBindTexture(GL_TEXTURE_2D, _target.colorTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, _width, _height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _target.colorTexture, 0);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                Debug::Warning("Post-process internal framebuffer incomplete.");

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            _target.width = _width;
            _target.height = _height;
        }

        bool EnsureLinkedPostProcessShader(Shader *_shader)
        {
            if (_shader == nullptr)
                return false;

            if (!_shader->IsLinked())
            {
                _shader->AddAttribute("vertexPosition");
                _shader->AddAttribute("vertexUV");
                _shader->Link();
            }

            return _shader->IsLinked();
        }

        template <typename T>
        void DestroyTextureTarget(T &_target)
        {
            if (_target.colorTexture != 0)
            {
                glDeleteTextures(1, &_target.colorTexture);
                _target.colorTexture = 0;
            }

            if (_target.framebuffer != 0)
            {
                glDeleteFramebuffers(1, &_target.framebuffer);
                _target.framebuffer = 0;
            }

            _target.width = 0;
            _target.height = 0;
        }
    } // namespace

    void EnsureRenderTarget(RenderTarget &_target, int _width, int _height)
    {
        if (_width <= 0 || _height <= 0)
            return;

        if (_target.framebuffer != 0 && _target.width == _width && _target.height == _height)
            return;

        DestroyRenderTarget(_target);

        glGenFramebuffers(1, &_target.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, _target.framebuffer);

        glGenTextures(1, &_target.colorTexture);
        glBindTexture(GL_TEXTURE_2D, _target.colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, _width, _height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _target.colorTexture, 0);

        glGenTextures(1, &_target.depthTexture);
        glBindTexture(GL_TEXTURE_2D, _target.depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, _width, _height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _target.depthTexture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            Debug::Warning("Render target framebuffer incomplete.");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        _target.width = _width;
        _target.height = _height;
    }

    void DestroyRenderTarget(RenderTarget &_target)
    {
        if (_target.depthTexture != 0)
        {
            glDeleteTextures(1, &_target.depthTexture);
            _target.depthTexture = 0;
        }

        if (_target.colorTexture != 0)
        {
            glDeleteTextures(1, &_target.colorTexture);
            _target.colorTexture = 0;
        }

        if (_target.framebuffer != 0)
        {
            glDeleteFramebuffers(1, &_target.framebuffer);
            _target.framebuffer = 0;
        }

        _target.width = 0;
        _target.height = 0;
    }

    PostProcessResult ApplyPostProcessChain(
        const PostProcessAsset *_asset,
        unsigned int _sourceFramebuffer,
        unsigned int _sourceColorTexture,
        unsigned int _sourceDepthTexture,
        int _width,
        int _height,
        const Matrix4 &_projection,
        RenderTarget *_outputTarget)
    {
        PostProcessResult result = {
            .framebuffer = _sourceFramebuffer,
            .colorTexture = _sourceColorTexture,
        };

        if (_sourceColorTexture == 0 || _width <= 0 || _height <= 0)
            return result;

        if (_asset != nullptr)
        {
            const std::vector<PostProcessPass> &passes = _asset->GetPasses();
            int enabledPassCount = 0;
            for (const PostProcessPass &pass : passes)
            {
                if (pass.enabled && pass.shaderId >= 0)
                    ++enabledPassCount;
            }

            if (enabledPassCount > 0)
            {
                EnsureFullscreenGeometry();
                EnsureInternalTarget(g_pingTarget, _width, _height);
                EnsureInternalTarget(g_pongTarget, _width, _height);

                GLint previousFramebuffer = 0;
                GLint previousViewport[4] = { 0, 0, _width, _height };
                glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
                glGetIntegerv(GL_VIEWPORT, previousViewport);

                const bool depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
                const bool blendEnabled = glIsEnabled(GL_BLEND);
                const bool cullFaceEnabled = glIsEnabled(GL_CULL_FACE);

                glDisable(GL_DEPTH_TEST);
                glDisable(GL_BLEND);
                glDisable(GL_CULL_FACE);

                const Matrix4 inverseProjection = glm::inverse(_projection);
                const Vector2 resolution(static_cast<float>(_width), static_cast<float>(_height));
                const Vector2 texelSize(
                    _width > 0 ? 1.0f / static_cast<float>(_width) : 0.0f,
                    _height > 0 ? 1.0f / static_cast<float>(_height) : 0.0f);
                const float elapsedTime = static_cast<float>(Time::TimeSinceLaunch()) / 1000.0f;

                unsigned int currentFramebuffer = _sourceFramebuffer;
                unsigned int currentColorTexture = _sourceColorTexture;

                int renderedPassIndex = 0;
                for (const PostProcessPass &pass : passes)
                {
                    if (!pass.enabled || pass.shaderId < 0)
                        continue;

                    ShaderAsset *shaderAsset = AssetManager::Get<ShaderAsset>(pass.shaderId);
                    if (shaderAsset == nullptr)
                        continue;

                    Shader *shader = shaderAsset->GetShader();
                    if (shader == nullptr || !EnsureLinkedPostProcessShader(shader))
                        continue;

                    InternalTarget &target = (renderedPassIndex % 2 == 0) ? g_pingTarget : g_pongTarget;
                    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
                    glViewport(0, 0, _width, _height);
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);

                    shader->Use();
                    shader->SetInt("inputTexture", 0);
                    shader->SetInt("sceneDepthTexture", 1);
                    shader->SetMat4("inverseProjection", inverseProjection);
                    shader->SetVec2("resolution", resolution);
                    shader->SetVec2("texelSize", texelSize);
                    shader->SetFloat("time", elapsedTime);
                    shader->SetInt("passIndex", renderedPassIndex);
                    shader->SetInt("passCount", enabledPassCount);
                    shader->SetFloat("exposure", pass.exposure);
                    shader->SetFloat("contrast", pass.contrast);
                    shader->SetFloat("saturation", pass.saturation);
                    shader->SetFloat("bloomThreshold", pass.bloomThreshold);
                    shader->SetFloat("bloomIntensity", pass.bloomIntensity);
                    shader->SetFloat("ssaoRadius", pass.ssaoRadius);
                    shader->SetFloat("ssaoBias", pass.ssaoBias);
                    shader->SetFloat("ssaoStrength", pass.ssaoStrength);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, currentColorTexture);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, _sourceDepthTexture);

                    glBindVertexArray(g_fullscreenVao);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    glBindVertexArray(0);

                    glBindTexture(GL_TEXTURE_2D, 0);
                    glActiveTexture(GL_TEXTURE0);
                    shader->UnUse();

                    currentFramebuffer = target.framebuffer;
                    currentColorTexture = target.colorTexture;
                    ++renderedPassIndex;
                }

                result.framebuffer = currentFramebuffer;
                result.colorTexture = currentColorTexture;

                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
                glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

                if (depthTestEnabled)
                    glEnable(GL_DEPTH_TEST);
                else
                    glDisable(GL_DEPTH_TEST);

                if (blendEnabled)
                    glEnable(GL_BLEND);
                else
                    glDisable(GL_BLEND);

                if (cullFaceEnabled)
                    glEnable(GL_CULL_FACE);
                else
                    glDisable(GL_CULL_FACE);
            }
        }

        if (_outputTarget != nullptr)
        {
            EnsureRenderTarget(*_outputTarget, _width, _height);
            if (_outputTarget->framebuffer != 0 && result.framebuffer != 0)
            {
                BlitFramebuffer(
                    result.framebuffer,
                    _width,
                    _height,
                    _outputTarget->framebuffer,
                    _width,
                    _height);
                result.framebuffer = _outputTarget->framebuffer;
                result.colorTexture = _outputTarget->colorTexture;
            }
        }
        return result;
    }

    void BlitFramebuffer(unsigned int _sourceFramebuffer, int _sourceWidth, int _sourceHeight, unsigned int _destinationFramebuffer, int _destinationWidth, int _destinationHeight)
    {
        if (_sourceFramebuffer == 0 || _sourceWidth <= 0 || _sourceHeight <= 0 || _destinationWidth <= 0 || _destinationHeight <= 0)
            return;

        GLint previousReadFramebuffer = 0;
        GLint previousDrawFramebuffer = 0;
        GLint previousViewport[4] = { 0, 0, _destinationWidth, _destinationHeight };
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
        glGetIntegerv(GL_VIEWPORT, previousViewport);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, _sourceFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _destinationFramebuffer);
        glBlitFramebuffer(
            0, 0, _sourceWidth, _sourceHeight,
            0, 0, _destinationWidth, _destinationHeight,
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
        glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    }
} // namespace Canis
