// SPDX-License-Identifier: GPL-2.0-or-later
#include <effect/effect.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <opengl/glframebuffer.h>
#include <opengl/gltexture.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <KConfigGroup>
#include <algorithm>
#include <cmath>

namespace KWin {
class NxGlow final : public Effect
{
    Q_OBJECT
public:
    NxGlow() { reconfigure(ReconfigureAll); effects->addRepaintFull(); }
    ~NxGlow() override { effects->makeOpenGLContextCurrent(); }
    bool isActive() const override { return !effects->isScreenLocked() && !effects->activeFullScreenEffect(); }
    int requestedEffectChainPosition() const override { return 90; }
    bool blocksDirectScanout() const override { return isActive(); }
    void reconfigure(ReconfigureFlags) override
    {
        const KConfigGroup c(effects->config(), "Effect-nxglow");
        m_radius = std::clamp(c.readEntry("Radius", 110.0), 10.0, 300.0);
        m_strength = std::clamp(c.readEntry("Strength", 0.85), 0.0, 2.0);
        m_saturation = std::clamp(c.readEntry("Saturation", 1.0), 0.0, 2.0);
        effects->addRepaintFull();
    }
    void prePaintScreen(ScreenPrePaintData &data) override
    {
        // Glow extends beyond window damage and needs the desktop under opaque windows.
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        effects->prePaintScreen(data);
    }
    void paintScreen(const RenderTarget &target, const RenderViewport &viewport,
                     int mask, const Region &region, LogicalOutput *screen) override
    {
        m_drawn = false;
        m_ready = prepare(viewport, target);
        effects->paintScreen(target, viewport, mask, region, screen);
    }
    void paintWindow(const RenderTarget &target, const RenderViewport &viewport,
                     EffectWindow *w, int mask, const Region &region, WindowPaintData &data) override
    {
        // Desktop first, then glow, then every ordinary window and panel.
        if (!w->isDesktop() && !m_drawn && m_ready) {
            composite(viewport);
            m_drawn = true;
        }
        effects->paintWindow(target, viewport, w, mask, region, data);
    }
private:
    struct GLState {
        GLboolean blend = glIsEnabled(GL_BLEND), scissor = glIsEnabled(GL_SCISSOR_TEST);
        GLint srcRGB, dstRGB, srcAlpha, dstAlpha, active, texture;
        GLfloat clear[4];
        GLState() {
            glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
            glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
            glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
            glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
            glDisable(GL_SCISSOR_TEST);
        }
        ~GLState() {
            glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
            blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
            scissor ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
            glClearColor(clear[0], clear[1], clear[2], clear[3]);
            glBindTexture(GL_TEXTURE_2D, texture);
            glActiveTexture(active);
        }
    };
    struct Buffer {
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> fbo;
        bool resize(QSize size) {
            if (texture && texture->size() == size) return fbo && fbo->valid();
            fbo.reset();
            texture = GLTexture::allocate(GL_RGBA16F, size);
            if (!texture) return false;
            texture->setFilter(GL_LINEAR);
            texture->setWrapMode(GL_CLAMP_TO_EDGE);
            fbo = std::make_unique<GLFramebuffer>(texture.get());
            return fbo->valid();
        }
    } m_source, m_horizontal, m_blurred;
    std::unique_ptr<GLShader> m_shader;
    bool m_shaderAttempted = false;
    bool m_drawn = false, m_ready = false;
    double m_radius = 110, m_strength = .85, m_saturation = 1;

    bool prepare(const RenderViewport &viewport, const RenderTarget &target)
    {
        GLState state;
        if (!m_shaderAttempted) {
            m_shaderAttempted = true;
            m_shader = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, {}, R"(
                uniform sampler2D sampler;
                uniform vec2 stepSize;
                uniform float strength;
                uniform float saturation;
                in vec2 texcoord0;
                out vec4 fragColor;
                void main() {
                    if (stepSize == vec2(0.0)) {
                        vec4 light = texture(sampler, texcoord0);
                        float grey = dot(light.rgb, vec3(0.2126, 0.7152, 0.0722));
                        light.rgb = max(vec3(0), mix(vec3(grey), light.rgb, saturation));
                        fragColor = light * strength;
                        return;
                    }
                    vec4 colour = vec4(0.0);
                    float total = 0.0;
                    for (int i = -16; i <= 16; ++i) {
                        float weight = exp(-float(i*i) / 72.0);
                        vec2 uv = texcoord0 + float(i) * stepSize;
                        if (all(greaterThanEqual(uv, vec2(0))) && all(lessThanEqual(uv, vec2(1))))
                            colour += texture(sampler, uv) * weight;
                        total += weight;
                    }
                    fragColor = colour / total * strength;
                }
            )");
        }
        if (!m_shader) return false;
        const auto rect = viewport.renderRect();
        // A low-resolution GPU-only scene is enough for a broad soft halo.
        const double scale = 0.125;
        const QSize size(std::max(1, int(std::ceil(rect.width() * scale))),
                         std::max(1, int(std::ceil(rect.height() * scale))));
        if (!m_source.resize(size) || !m_horizontal.resize(size) || !m_blurred.resize(size)) return false;
        RenderTarget capture(m_source.fbo.get(), target.colorDescription());
        RenderViewport captureViewport(rect, scale, capture, QPoint());
        GLFramebuffer::pushFramebuffer(m_source.fbo.get());
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        for (auto *w : effects->stackingOrder()) {
            if (!(w->isNormalWindow() || w->isDialog()) || w->isDeleted() || w->isMinimized()
                || !w->isOnCurrentDesktop() || !w->isOnCurrentActivity() || !w->isVisible()) continue;
            WindowPaintData data;
            data.setOpacity(w->opacity());
            effects->renderWindow(capture, captureViewport, w, PAINT_WINDOW_TRANSFORMED,
                                  Region::infinite(), data);
        }
        GLFramebuffer::popFramebuffer();
        blur(m_source, m_horizontal, QVector2D(m_radius / 16.0 / rect.width(), 0));
        blur(m_horizontal, m_blurred, QVector2D(0, m_radius / 16.0 / rect.height()));
        return true;
    }
    void blur(Buffer &from, Buffer &to, QVector2D step)
    {
        GLFramebuffer::pushFramebuffer(to.fbo.get());
        glDisable(GL_BLEND);
        ShaderBinder binder(m_shader.get());
        QMatrix4x4 matrix;
        matrix.ortho(0, to.texture->width(), to.texture->height(), 0, -1, 1);
        m_shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
        m_shader->setUniform("sampler", 0);
        m_shader->setUniform("stepSize", step);
        m_shader->setUniform("strength", 1.0f);
        from.texture->bind();
        from.texture->render(QSizeF(to.texture->size()));
        from.texture->unbind();
        GLFramebuffer::popFramebuffer();
    }
    void composite(const RenderViewport &viewport)
    {
        GLState state;
        ShaderBinder binder(m_shader.get());
        QMatrix4x4 matrix = viewport.projectionMatrix();
        const auto rect = viewport.renderRect();
        matrix.translate(rect.x() * viewport.scale(), rect.y() * viewport.scale());
        m_shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, matrix);
        m_shader->setUniform("stepSize", QVector2D(0, 0));
        m_shader->setUniform("strength", float(m_strength));
        m_shader->setUniform("saturation", float(m_saturation));
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        m_blurred.texture->bind();
        m_blurred.texture->render(rect.size() * viewport.scale());
        m_blurred.texture->unbind();
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }
};
KWIN_EFFECT_FACTORY_SUPPORTED(NxGlow, "glow.json", return effects->isOpenGLCompositing();)
}
#include "glow.moc"
