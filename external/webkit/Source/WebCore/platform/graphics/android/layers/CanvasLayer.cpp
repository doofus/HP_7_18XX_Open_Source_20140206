/*
 * Copyright 2012, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "CanvasLayer"
#define LOG_NDEBUG 1

#include "config.h"
#include "CanvasLayer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "CanvasTexture.h"
#include "DrawQuadData.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "RenderLayerCompositor.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "TilesManager.h"
#include "CanvasRenderingContext.h"
#include "CanvasRenderingContext2D.h"

namespace WebCore {

CanvasLayer::CanvasLayer(RenderLayer* owner, HTMLCanvasElement* canvas)
    : LayerAndroid(owner)
    , m_canvas(canvas)
    , m_dirtyCanvas()
    , m_bitmap(0)
{
    init();
    m_canvas->addObserver(this);
    // Make sure we initialize in case the canvas has already been laid out
    canvasResized(m_canvas);
}

CanvasLayer::CanvasLayer(const CanvasLayer& layer)
    : LayerAndroid(layer)
    , m_canvas(0)
    , m_bitmap(0)
{
    init();
    if (!layer.m_canvas) {
        // The canvas has already been destroyed - this shouldn't happen
        ALOGW("Creating a CanvasLayer for a destroyed canvas!");
        m_visibleContentRect = IntRect();
        m_offsetFromRenderer = IntSize();
        m_texture->setHwAccelerated(false);
        return;
    }
    // We are making a copy for the UI, sync the interesting bits
    m_visibleContentRect = layer.visibleContentRect();
    m_offsetFromRenderer = layer.offsetFromRenderer();
    bool previousState = m_texture->hasValidTexture();
    if (!previousState && layer.m_dirtyCanvas.isEmpty()) {
        // We were previously in software and don't have anything new to draw,
        // so stay in software
        m_bitmap = layer.bitmap();
        SkSafeRef(m_bitmap);
    } else {
        if (layer.m_canvas->hasCreatedImageBuffer() && layer.m_canvas->buffer()->isAccelerated()) {
            GraphicsContext3D* ctx3D = layer.m_canvas->buffer()->context3D();
            ctx3D->makeContextCurrent();
            EGLContext ctx = eglGetCurrentContext();
            if ((ctx != EGL_NO_CONTEXT) &&
                    ((ctx == m_texture->m_context) || (m_texture->m_context == EGL_NO_CONTEXT))) {
                m_texture->require2DTexture();
                m_texture->copyFromFBO(layer.m_canvas->buffer()->canvasFBO());
            } else {
                m_texture->destoryTexture();
            }
            m_texture->m_context = ctx;
        } else if (!m_texture->uploadImageBuffer(layer.m_canvas->buffer())) {
            // Attempt to upload to a surface texture
            // Blargh, no surface texture or ImageBuffer - fall back to software
            m_bitmap = layer.bitmap();
            SkSafeRef(m_bitmap);
            // Merge the canvas invals with the layer's invals to repaint the needed
            // tiles.
            SkRegion::Iterator iter(layer.m_dirtyCanvas);
            const IntPoint& offset = m_visibleContentRect.location();
            for (; !iter.done(); iter.next()) {
                SkIRect diff = iter.rect();
                diff.fLeft += offset.x();
                diff.fRight += offset.x();
                diff.fTop += offset.y();
                diff.fBottom += offset.y();
                m_dirtyRegion.op(diff, SkRegion::kUnion_Op);
            }
        } else {
        }
        if (previousState != m_texture->hasValidTexture()) {
            // Need to do a full inval of the canvas content as we are mode switching
            m_dirtyRegion.op(m_visibleContentRect.x(), m_visibleContentRect.y(),
                    m_visibleContentRect.maxX(), m_visibleContentRect.maxY(), SkRegion::kUnion_Op);
        }
    }
    if (layer.m_canvas && layer.m_canvas->hasCreatedImageBuffer()) {
        CanvasRenderingContext* context = layer.m_canvas->renderingContext();
        if (context && context->is2d()) {
            CanvasRenderingContext2D* context2d = static_cast<CanvasRenderingContext2D*>(context);
            context2d->checkRenderingHint(m_texture->uploadHint());
            m_texture->resetHint();
        }
    }
}

CanvasLayer::~CanvasLayer()
{
    if (m_canvas)
        m_canvas->removeObserver(this);
    SkSafeUnref(m_bitmap);
}

void CanvasLayer::init()
{
    m_texture = CanvasTexture::getCanvasTexture(this);
}

void CanvasLayer::canvasChanged(HTMLCanvasElement*, const FloatRect& changedRect)
{
    if (!m_texture->hasValidTexture()) {
        // We only need to track invals if we aren't using a SurfaceTexture.
        // If we drop out of hwa, we will do a full inval anyway
        SkIRect irect = SkIRect::MakeXYWH(changedRect.x(), changedRect.y(),
                                          changedRect.width(), changedRect.height());
        m_dirtyCanvas.op(irect, SkRegion::kUnion_Op);
    }
    owningLayer()->compositor()->scheduleLayerFlush();
}

void CanvasLayer::canvasResized(HTMLCanvasElement*)
{
    const IntSize& size = m_canvas->size();
    m_dirtyCanvas.setRect(0, 0, size.width(), size.height());
    // If we are smaller than one tile, don't bother using a surface texture
    if (size.width() <= TilesManager::tileWidth()
            && size.height() <= TilesManager::tileHeight())
        m_texture->setSize(IntSize());
    else
        m_texture->setSize(size);
}

void CanvasLayer::canvasDestroyed(HTMLCanvasElement*)
{
    m_canvas = 0;
}

void CanvasLayer::clearDirtyRegion()
{
    LayerAndroid::clearDirtyRegion();
    m_dirtyCanvas.setEmpty();
    if (m_canvas)
        m_canvas->clearDirtyRect();
}

SkBitmapRef* CanvasLayer::bitmap() const
{
    if (!m_canvas || !m_canvas->buffer())
        return 0;
    return m_canvas->copiedImage()->nativeImageForCurrentFrame();
}

IntRect CanvasLayer::visibleContentRect() const
{
    if (!m_canvas
            || !m_canvas->renderer()
            || !m_canvas->renderer()->style()
            || !m_canvas->inDocument()
            || m_canvas->renderer()->style()->visibility() != VISIBLE)
        return IntRect();
    return m_canvas->renderBox()->contentBoxRect();
}

IntSize CanvasLayer::offsetFromRenderer() const
{
    return m_canvas->renderBox()->layer()->backing()->graphicsLayer()->offsetFromRenderer();
}

bool CanvasLayer::needsTexture()
{
    return (m_bitmap && !masksToBounds()) || LayerAndroid::needsTexture();
}

void CanvasLayer::contentDraw(SkCanvas* canvas, PaintStyle style)
{
    m_texture->setUploadHint(CanvasRenderingContext2D::DRAWHINT_SOFT);
    LayerAndroid::contentDraw(canvas, style);
    if (m_bitmap && !masksToBounds()) {
        SkBitmap& bitmap = m_bitmap->bitmap();
        SkRect dst = SkRect::MakeXYWH(m_visibleContentRect.x() - m_offsetFromRenderer.width(),
                m_visibleContentRect.y() - m_offsetFromRenderer.height(),
                m_visibleContentRect.width(), m_visibleContentRect.height());
        canvas->drawBitmapRect(bitmap, 0, dst, 0);
    } else if (m_texture->hasValidTexture() && !m_texture->needTransfer()) {
        m_texture->lock();
        if (!m_texture->hasValidTexture() || m_texture->needTransfer()) {
            m_texture->unlock();
            return;
        }
        SkBitmap target;
        int savedFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);
        unsigned int framebuffer = 0;

        glGenFramebuffers( 1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture->texture(false), 0);

        target.setConfig(SkBitmap::kARGB_8888_Config, m_texture->size().width(), m_texture->size().height());
        target.allocPixels();

        glReadPixels(0, 0, m_texture->size().width(), m_texture->size().height(), GL_RGBA, GL_UNSIGNED_BYTE, target.getPixels());
        m_texture->unlock();

        glDeleteFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, savedFBO);

        SkRect dst = SkRect::MakeXYWH(m_visibleContentRect.x() - m_offsetFromRenderer.width(),
                m_visibleContentRect.y() - m_offsetFromRenderer.height(),
                m_visibleContentRect.width(), m_visibleContentRect.height());
        canvas->drawBitmapRect(target, NULL, dst, NULL);

    }
}

bool CanvasLayer::drawGL(bool layerTilesDisabled)
{
    m_texture->setUploadHint(CanvasRenderingContext2D::DRAWHINT_HARD);
    bool ret = LayerAndroid::drawGL(layerTilesDisabled);
    if (!m_texture->needTransfer()) {
        m_texture->lock();
        if (m_texture->needTransfer()) {
            m_texture->unlock();
            return ret;
        }
        SkRect rect = SkRect::MakeXYWH(m_visibleContentRect.x() - m_offsetFromRenderer.width(),
                                       m_visibleContentRect.y() - m_offsetFromRenderer.height(),
                                       m_visibleContentRect.width(), m_visibleContentRect.height());

        TransformationMatrix m = m_drawTransform;
        m.flipY();
        m.translate(0, -m_visibleContentRect.height());

        TextureQuadData data(m_texture->texture(false), GL_TEXTURE_2D,
                             GL_LINEAR, LayerQuad, &m, &rect);

        TilesManager::instance()->shader()->drawQuad(&data);
        m_texture->unlock();
    } else if (!m_bitmap && m_texture->updateTexImage()) {
        SkRect rect = SkRect::MakeXYWH(m_visibleContentRect.x() - m_offsetFromRenderer.width(),
                                       m_visibleContentRect.y() - m_offsetFromRenderer.height(),
                                       m_visibleContentRect.width(), m_visibleContentRect.height());
        TextureQuadData data(m_texture->texture(), GL_TEXTURE_EXTERNAL_OES,
                             GL_LINEAR, LayerQuad, &m_drawTransform, &rect);
        TilesManager::instance()->shader()->drawQuad(&data);
    }
    return ret;
}

LayerAndroid::InvalidateFlags CanvasLayer::onSetHwAccelerated(bool hwAccelerated)
{
    if (m_texture->setHwAccelerated(hwAccelerated))
        return LayerAndroid::InvalidateLayers;
    return LayerAndroid::InvalidateNone;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
