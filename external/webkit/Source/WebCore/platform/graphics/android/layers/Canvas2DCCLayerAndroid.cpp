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

#include "Canvas2DCCLayerAndroid.h"
#include "TilesManager.h"
#include <pthread.h>
#include "SkDevice.h"

namespace WebCore {
Canvas2DCCLayerAndroid::Canvas2DCCLayerAndroid(GraphicsContext3D* context, const IntSize &size)
    : m_context(context)
    , m_size(size)
    , m_fbo(0)
    , m_frontTextureId(0)
    , m_canvas(NULL)
    , m_needFlipY(true)
    , m_forceCopy(false)
    , m_deleteTexture(false)
    , m_needTexture(false)
    , m_isSoftwareCanvas(true)
    , m_isPerfFallback(true)
    , m_pendingGLContext(false)
{
    // init mutex
    pthread_mutex_init(&m_mutex, NULL);
    m_frontBitmap.setConfig(SkBitmap::kARGB_8888_Config, m_size.width(), m_size.height());
    m_frontBitmap.allocPixels();
    m_frontBitmap.eraseColor(0);
}

Canvas2DCCLayerAndroid::~Canvas2DCCLayerAndroid()
{
    pthread_mutex_destroy(&m_mutex);
    if (m_frontTextureId)
        deleteTexture(m_context, m_frontTextureId);
    SkSafeUnref(m_canvas);
}

void Canvas2DCCLayerAndroid::setCanvasInfo(GraphicsContext3D* context, SkCanvas* canvas, GLuint fbo)
{
    if (canvas != 0) {
        SkSafeUnref(m_canvas);
        m_canvas = canvas;
        SkSafeRef(m_canvas);
    }
    if (fbo != (unsigned)-1)
        m_fbo = fbo;
    pthread_mutex_lock(&m_mutex);
    if (m_context != context) {
        if (context && !m_frontTextureId && !m_deleteTexture) {
            m_frontTextureId = createTexture(context, m_size.width(), m_size.height());
        } else if (!m_context && m_frontTextureId) {
            deleteTexture(m_context, m_frontTextureId);
            m_frontTextureId = 0;
        }
        m_context = context;
    }
    pthread_mutex_unlock(&m_mutex);
}

void Canvas2DCCLayerAndroid::setBufferInfo(PassRefPtr<ImageBufferWeakReference> buffer)
{
    RefPtr<ImageBufferWeakReference> buf = buffer;
    if (m_bufferReference && m_bufferReference != buf) {
        android_printLog(ANDROID_LOG_WARN, "Canvas2DCCLayerAndroid", "bufferref is overwritten\n");
    } else if (!m_bufferReference) {
        m_bufferReference = buf;
    }
}

void Canvas2DCCLayerAndroid::deleteTexture(GraphicsContext3D *context, GLuint id)
{
    GLuint textures[1];
    textures[0] = id;
    if (context) {
        context->makeContextCurrent();
        glDeleteTextures(1, textures);
    } else {
        android_printLog(ANDROID_LOG_WARN, "Canvas2DCCLayerAndroid", "Texture to be deleted but context is null\n");
    }

}

GLuint Canvas2DCCLayerAndroid::createTexture(GraphicsContext3D *context, int width, int height)
{
    GLuint texture;
    context->makeContextCurrent();
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}

void Canvas2DCCLayerAndroid::publish(bool needCopy)
{
    int width = m_size.width();
    int height = m_size.height();


    if (width == 0)
        return;

    pthread_mutex_lock(&m_mutex);
    if (m_deleteTexture && m_frontTextureId) {
        //ASSERT(!m_needTexture);
        deleteTexture(m_context, m_frontTextureId);
        m_frontTextureId = 0;
        m_deleteTexture = false;
    }

    if (m_needTexture && !m_frontTextureId && m_context) {
        //ASSERT(!m_deleteTexture);
        m_frontTextureId = createTexture(m_context, m_size.width(), m_size.height());
        m_needTexture = false;
    }
    pthread_mutex_unlock(&m_mutex);

    // copy if dirty
    if (needCopy || m_forceCopy) {
        if (m_fbo && m_context && m_frontTextureId) {
            m_context->grContext()->flush();
            GLint oldTex, oldFbo;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTex);
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFbo);
            if ((GLuint) oldFbo != m_fbo) {
                glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            }

            glBindTexture(GL_TEXTURE_2D, m_frontTextureId);

            pthread_mutex_lock(&m_mutex);
            glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, width, height, 0);
            pthread_mutex_unlock(&m_mutex);
            m_needFlipY = true;
            glBindTexture(GL_TEXTURE_2D, oldTex);
            if ((GLuint) oldFbo != m_fbo)
                glBindFramebuffer(GL_FRAMEBUFFER, oldFbo);
        } else if (!m_fbo && m_context && m_frontTextureId && m_canvas) {
            m_needFlipY = false;
            updateFrontTextureWithBitmap(m_canvas);
        } else if (m_canvas) {
            updateFrontBitmapWithBitmap(m_canvas);
            m_needFlipY = false;
        } else {
            android_printLog(ANDROID_LOG_ERROR, "Canvas2DCCLayer", "unexpected branch!");
        }
        m_forceCopy = false;
    }
}

void Canvas2DCCLayerAndroid::updateFrontBitmapWithBitmap(SkCanvas* canvas)
{
    SkDevice* device = canvas->getDevice();
    const SkBitmap& orig = device->accessBitmap(false);
    pthread_mutex_lock(&m_mutex);
    orig.copyTo(&m_frontBitmap, orig.config());
    pthread_mutex_unlock(&m_mutex);
}

void Canvas2DCCLayerAndroid::updateFrontTextureWithBitmap(SkCanvas* canvas)
{
    GLint oldTex;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTex);

    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);
    pthread_mutex_lock(&m_mutex);
    if (!m_frontTextureId) {
        m_frontTextureId = createTexture(m_context, m_size.width(), m_size.height());
    }
    updateTextureWithBitmap(m_frontTextureId, bitmap);
    pthread_mutex_unlock(&m_mutex);
    //restore texture binding
    glBindTexture(GL_TEXTURE_2D, oldTex);
}

void Canvas2DCCLayerAndroid::updateTextureWithBitmap(GLuint texture, const SkBitmap& bitmap)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    bitmap.lockPixels();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width(), bitmap.height(),
            0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.getPixels());
    bitmap.unlockPixels();
    if (GLUtils::checkGlError("glTexImage2D")) {
        XLOG("GL ERROR: glTexImage2D parameters are : bitmap.width() %d, bitmap.height() %d,"
                " internalformat 0x%x, type 0x%x, bitmap.getPixels() %p",
                bitmap.width(), bitmap.height(), GL_RGBA, GL_UNSIGNED_BYTE, bitmap.getPixels());
    }
}

// called from UI thread
void Canvas2DCCLayerAndroid::drawGL(const TransformationMatrix& drawMatrix, SkRect& geometry)
{
    if (m_size.width() == 0)
        return;

    // upLoad copied texture
    TransformationMatrix m = drawMatrix;
    if (m_needFlipY) {
        m.flipY();
        m.translate(0, -geometry.height());
    }

    if (m_frontTextureId) {
        pthread_mutex_lock(&m_mutex);
        TilesManager::instance()->shader()->drawLayerQuad(m, geometry, m_frontTextureId, 1.0, true);
        pthread_mutex_unlock(&m_mutex);
    } else {
        m_needTexture = true;
        if (m_context)
            m_forceCopy = true;
        m_deleteTexture = false;
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        pthread_mutex_lock(&m_mutex);
        void* src = m_frontBitmap.getPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_size.width(), m_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
        TilesManager::instance()->shader()->drawLayerQuad(m, geometry, tex, 1.0, true);
        pthread_mutex_unlock(&m_mutex);
        glDeleteTextures(1, &tex);
    }

}

// called from UI thread
void Canvas2DCCLayerAndroid::contentDraw(SkCanvas* canvas, const SkRect& dst)
{

    pthread_mutex_lock(&m_mutex);
    if (m_frontTextureId) {
        m_deleteTexture = true;
        m_needTexture = false;
        m_forceCopy = true;
        // Wish never run here, read pixels from texture m_frontTextureId as it is too slow
        SkBitmap target;
        int savedFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);
        unsigned int framebuffer = 0;

        glGenFramebuffers( 1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frontTextureId, 0);

        target.setConfig(SkBitmap::kARGB_8888_Config, m_size.width(), m_size.height());
        target.allocPixels();

        glReadPixels(0, 0, m_size.width(), m_size.height(), GL_RGBA, GL_UNSIGNED_BYTE, target.getPixels());

        glDeleteFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, savedFBO);

        GLUtils::checkGlError("glReadPixels");
        canvas->drawBitmapRect(target, NULL, dst, NULL);
    } else {
        canvas->drawBitmapRect(m_frontBitmap, NULL, dst, NULL);
    }
    pthread_mutex_unlock(&m_mutex);

}

void Canvas2DCCLayerAndroid::hintRendering(FallbackPolicy policy, bool SW)
{
    switch (policy) {
        case GPU_FALLBACK_PERF:
            m_isPerfFallback = SW;
            break;
        case GPU_FALLBACK_CANVAS:
            m_isSoftwareCanvas = SW;
            break;
    }
}

void Canvas2DCCLayerAndroid::checkRenderingHint()
{
    bool predict = (!m_isPerfFallback) && (!m_isSoftwareCanvas);

    if (m_pendingGLContext) {
        checkGLContext();
        m_pendingGLContext = false;
    }

    if (predict) {
        ensureHardwareRendering();
    } else {
        softwareRenderingFallback(m_isPerfFallback);
    }
}

void Canvas2DCCLayerAndroid::checkGLContext()
{
    ImageBuffer * buffer = NULL;
    if (m_bufferReference) {
        if ((buffer = m_bufferReference->buffer())) {
            buffer->ensureGLContext();
        } else {
            android_printLog(ANDROID_LOG_WARN, "Canvas2DCCLayerAndroid::checkGLContext", "buffer is null\n");
        }
    } else {
        android_printLog(ANDROID_LOG_WARN, "Canvas2DCCLayerAndroid::checkGLContext", "bufferref is null\n");
    }
}

void Canvas2DCCLayerAndroid::requireGLContext()
{
    m_pendingGLContext = true;
}

void Canvas2DCCLayerAndroid::softwareRenderingFallback(bool forever)
{
    ImageBuffer * buffer = NULL;
    if (m_bufferReference) {
        if ((buffer = m_bufferReference->buffer())) {
            buffer->softwareRenderingFallback(true, forever);
        } else {
            android_printLog(ANDROID_LOG_WARN, "Canvas2DCCLayerAndroid::softwareRenderingFallback", "buffer is null %d\n", forever);
        }
    } else {
        android_printLog(ANDROID_LOG_WARN, "Canvas2DCCLayerAndroid::softwareRenderingFallback", "bufferref is null\n");
    }
}

void Canvas2DCCLayerAndroid::ensureHardwareRendering()
{
    ImageBuffer * buffer = NULL;
    if (m_bufferReference) {
        if ((buffer = m_bufferReference->buffer())) {
            buffer->softwareRenderingFallback(false, false);
        } else {
            android_printLog(ANDROID_LOG_WARN, "Canvas2DCCLayerAndroid::ensureHardwareRendering", "buffer is null\n");
        }
    } else {
        android_printLog(ANDROID_LOG_WARN, "Canvas2DCCLayerAndroid::ensureHardwareRendering", "bufferref is null\n");
    }
}

void Canvas2DCCLayerAndroid::createSnapshots(SkBitmap *snapshots)
{
    // Create a copy of the image
    snapshots->setConfig(SkBitmap::kARGB_8888_Config, m_size.width(), m_size.height());
    snapshots->allocPixels();
    snapshots->eraseColor(0);
    SkDevice* device = new SkDevice(NULL, *snapshots, false);
    SkCanvas canvas;
    canvas.setDevice(device);
    device->unref();
    SkRect dest;
    dest.set(0, 0, m_size.width(), m_size.height());
    contentDraw(&canvas, dest);
}

} // namespace WebCore
