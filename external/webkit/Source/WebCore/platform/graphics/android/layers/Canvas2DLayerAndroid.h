/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef Canvas2DLayerAndroid_h
#define Canvas2DLayerAndroid_h

#include "CanvasLayerAndroid.h"
#include "GraphicsContext3D.h"
#include "Canvas2DCCLayerAndroid.h"
#include "CanvasRenderingContext.h"
#include "gl2.h"

namespace WebCore {

// A layer containing an accelerated 2d canvas
class Canvas2DLayerAndroid : public CanvasLayerAndroid {
public:
    explicit Canvas2DLayerAndroid(GraphicsContext3D*, const IntSize &size);
    explicit Canvas2DLayerAndroid(const Canvas2DLayerAndroid& layer);
    virtual ~Canvas2DLayerAndroid();

    virtual LayerAndroid* copy() const;
    virtual void postcopy();
    virtual bool drawGL();
    virtual void onSerialize();
    virtual bool needsTexture();
    virtual void contentDraw(SkCanvas*);

    void dirty() {m_dirty = true;}
    bool isDirty() const {return m_dirty;}

    GraphicsContext3D* context3D() const {return m_context;}
    RefPtr<Canvas2DCCLayerAndroid> ccLayer() const {return m_cLayer;}
    IntSize size() const {return m_size;}
    void setCanvasInfo(GraphicsContext3D* context, SkCanvas* canvas = NULL, GLuint fbo = -1);
    void setBufferInfo(PassRefPtr<ImageBufferWeakReference> buffer);
    void requireGLContext();
    void hintRendering(FallbackPolicy policy, bool SW);

private:
    GraphicsContext3D* m_context;
    RefPtr<Canvas2DCCLayerAndroid> m_cLayer;
    bool    m_dirty;
    IntSize m_size;
};

}

#endif
