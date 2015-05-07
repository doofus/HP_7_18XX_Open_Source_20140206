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

#ifndef CanvasTexture_h
#define CanvasTexture_h

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayer.h"
#include "CanvasRenderingContext2D.h"

#include <wtf/RefPtr.h>
#include <utils/threads.h>

namespace android {
class SurfaceTexture;
class SurfaceTextureClient;
}

namespace WebCore {

class CanvasTexture : public ThreadSafeRefCounted<CanvasTexture> {

public:
    /********************************************
     * Called by both threads
     ********************************************/
    static PassRefPtr<CanvasTexture> getCanvasTexture(CanvasLayer* layer);
    bool setHwAccelerated(bool hwAccelerated);
    void lock() { m_surfaceLock.lock(); }
    void unlock() { m_surfaceLock.unlock(); }

    /********************************************
     * Called by WebKit thread
     ********************************************/
    void setSize(const IntSize& size);
    bool uploadImageBuffer(ImageBuffer* imageBuffer);
    bool hasValidTexture() { return m_hasValidTexture; }
    bool copyFromFBO(unsigned int fbo);

    /********************************************
     * Called by UI thread WITH GL context
     ********************************************/
    virtual ~CanvasTexture();
    void requireTexture(bool needLock = true);
    void destoryTexture();
    void require2DTexture(bool needLock = true);
    GLuint texture(bool needLock = true) {
        if (m_texTarget == GL_TEXTURE_EXTERNAL_OES)
            requireTexture(needLock);
        else if (m_texTarget == GL_TEXTURE_2D)
            require2DTexture(needLock);
        return m_texture;
    }
    bool updateTexImage();
    void setUploadHint(CanvasRenderingContext2D::CanvasDrawHint hint) { m_uploadHint = (CanvasRenderingContext2D::CanvasDrawHint) (hint | m_uploadHint); }
    void resetHint() { m_uploadHint = CanvasRenderingContext2D::DRAWHINT_NONE; }
    CanvasRenderingContext2D::CanvasDrawHint uploadHint(){ return m_uploadHint; }

    /********************************************
     * Called by UI thread
     ********************************************/
    bool needTransfer() { return m_needTransfer; }
    const IntSize& size() { return m_size; }

private:
    /********************************************
     * Called by both threads
     ********************************************/
    void destroySurfaceTextureLocked();

    /********************************************
     * Called by WebKit thread
     ********************************************/
    CanvasTexture(int layerId);
    bool useSurfaceTexture();
    SurfaceTextureClient* nativeWindow();

    IntSize m_size;
    int m_layerId;
    GLuint m_texture;
    android::Mutex m_surfaceLock;
    sp<android::SurfaceTexture> m_surfaceTexture;
    sp<android::SurfaceTextureClient> m_ANW;
    bool m_hasValidTexture;
    bool m_useHwAcceleration;
    bool m_needTransfer;
    GLenum m_texTarget;
    CanvasRenderingContext2D::CanvasDrawHint m_uploadHint;

public:
    EGLContext m_context;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // CanvasTexture_h
