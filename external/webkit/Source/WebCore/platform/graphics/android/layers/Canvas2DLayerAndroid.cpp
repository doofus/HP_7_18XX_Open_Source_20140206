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

#include "config.h"
#include "Canvas2DLayerAndroid.h"
#include "Canvas2DCCLayerAndroid.h"
#include "HTMLCanvasElement.h"
#include "CanvasRenderingContext.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "TilesManager.h"
#include "GLUtils.h"

namespace WebCore {

Canvas2DLayerAndroid::Canvas2DLayerAndroid(GraphicsContext3D* context, const IntSize &size)
    : m_context(context)
    , m_dirty(true)
    , m_size(size)
{
    m_cLayer = adoptRef(new Canvas2DCCLayerAndroid(m_context, size));
}

Canvas2DLayerAndroid::Canvas2DLayerAndroid(const Canvas2DLayerAndroid& layer)
    : CanvasLayerAndroid(layer)
    , m_context(layer.context3D())
    , m_cLayer(layer.ccLayer())
    , m_size(layer.size())
{
}

Canvas2DLayerAndroid::~Canvas2DLayerAndroid()
{
}

LayerAndroid* Canvas2DLayerAndroid::copy() const
{
    Canvas2DLayerAndroid* copiedLayer = new Canvas2DLayerAndroid(*this);

    if (copiedLayer->ccLayer() != NULL)
        copiedLayer->ccLayer()->publish(isDirty());
    return copiedLayer;
}

void Canvas2DLayerAndroid::postcopy()
{
    m_dirty = false;
    if (m_cLayer) {
        m_cLayer->checkRenderingHint();
    }
}

void Canvas2DLayerAndroid::onSerialize()
{
    if (m_cLayer) {
        SkBitmap snapshots;
        m_cLayer->createSnapshots(&snapshots);
        SkBitmapRef *image = new SkBitmapRef(snapshots);
        setContentsImage(image);
        image->unref();
    }
}

bool Canvas2DLayerAndroid::needsTexture()
{
    return false;
}

void Canvas2DLayerAndroid::contentDraw(SkCanvas* canvas)
{
    if (m_context)
        hintRendering(GPU_FALLBACK_CANVAS, true);

    if (m_cLayer) {
        SkRect rect = SkRect::MakeSize(getSize());
        m_cLayer->contentDraw(canvas, rect);
    }
}

bool Canvas2DLayerAndroid::drawGL()
{
    if (m_cLayer) {
        SkRect rect = SkRect::MakeSize(getSize());
        m_cLayer->drawGL(*drawTransform(), rect);
    }

    if (!m_context) {
        if (m_cLayer) {
            m_cLayer->requireGLContext();
        }
    }
    else {
        hintRendering(GPU_FALLBACK_CANVAS, false);
    }

    return drawChildrenGL();
}

void Canvas2DLayerAndroid::setCanvasInfo(GraphicsContext3D* context, SkCanvas* canvas, GLuint fbo)
{
    m_context = context;
    if (m_cLayer)
        m_cLayer->setCanvasInfo(context, canvas, fbo);
}

void Canvas2DLayerAndroid::setBufferInfo(PassRefPtr<ImageBufferWeakReference> buffer)
{
    if (m_cLayer)
        m_cLayer->setBufferInfo(buffer);
}

void Canvas2DLayerAndroid::requireGLContext()
{
    if (m_cLayer)
        m_cLayer->requireGLContext();
}

void Canvas2DLayerAndroid::hintRendering(FallbackPolicy policy, bool SW)
{
    if (m_cLayer)
        m_cLayer->hintRendering(policy, SW);
}

} // namespace WebCore
