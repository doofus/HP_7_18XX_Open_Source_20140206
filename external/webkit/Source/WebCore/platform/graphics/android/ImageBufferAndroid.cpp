/*
 * Copyright 2007, The Android Open Source Project
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
#include "ImageBuffer.h"

#include "Base64.h"
#include "BitmapImage.h"
#include "ColorSpace.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "PlatformBridge.h"
#include "PlatformGraphicsContext.h"
#include "SharedGraphicsContext3D.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkData.h"
#include "SkDevice.h"
#include "SkImageEncoder.h"
#include "SkStream.h"
#include "SkCanvas.h"
#include "SkGpuDevice.h"
#include "SkUnPreMultiply.h"
#include "GLUtils.h"
#include "GrGLRenderTarget.h"

#include <cutils/properties.h>
#include <utils/Log.h>

using namespace std;

namespace WebCore {

#define SMALLCANVAS 128*128

// =======================
// ImageBufferData
ImageBufferData::ImageBufferData(const IntSize& size)
    : m_platformContext(0)
{
}

static unsigned int SetupAcceleratedCanvas(SkCanvas* canvas, const IntSize& size, ImageBufferData* data, bool sync) {
    GraphicsContext3D* context3D = data->m_platformContext->context3D();
    if (!context3D && (GLUtils::mContext != EGL_NO_CONTEXT)) {
        context3D = SharedGraphicsContext3D::create(0);
        data->m_platformContext->setGraphicsContext3D(context3D);
    }

    if (context3D) {
        context3D->makeContextCurrent();

        GrContext* gr = context3D->grContext();
        GrTextureDesc desc;
        desc.fFlags = kRenderTarget_GrTextureFlagBit;
        desc.fAALevel = kNone_GrAALevel;
        desc.fWidth = size.width();
        desc.fHeight = size.height();
        desc.fConfig = kRGBA_8888_GrPixelConfig;

        GrTexture* texture = gr->createUncachedTexture(desc, 0, 0);
        if (texture == NULL) return 0;
        unsigned int fbo = ((GrGLRenderTarget*)texture->asRenderTarget())->renderFBOID();

        canvas->setDevice(new SkGpuDevice(gr, texture))->unref();
        if (sync) {
            SkBitmap bitmap;
            SkDevice* device = canvas->getDevice();
            const SkBitmap& orig = device->accessBitmap(false);
            bitmap = orig;
            canvas->drawBitmap(bitmap, 0, 0, NULL);
        }
        texture->unref();
        return fbo;
    }
    return 0;
}

static void SetupUnacceleratedCanvas(SkCanvas* canvas, const IntSize& size, ImageBufferData* data, bool sync, bool alloc = true)
{
    //3d context used by canvas 2D layer for H/W compositing
    GraphicsContext3D* context3D = data->m_platformContext->context3D();
    if (!context3D && (GLUtils::mContext != EGL_NO_CONTEXT)) {
        context3D = SharedGraphicsContext3D::create(0);
        data->m_platformContext->setGraphicsContext3D(context3D);
    }

    SkBitmap bitmap;
    if (sync) {
        SkDevice* device = canvas->getDevice();
        const SkBitmap& orig = device->accessBitmap(false);

        orig.copyTo(&bitmap, orig.config());
        canvas->setBitmapDevice(bitmap);
    } else if (alloc) {
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
        bitmap.allocPixels();
        bitmap.eraseColor(0);
        canvas->setBitmapDevice(bitmap);
    }
}


// =====================
// ImageBuffer
ImageBuffer::ImageBuffer(const IntSize& size, ColorSpace, RenderingMode renderingMode, bool& success)
    : m_data(size)
    , m_size(size)
    , m_canTryHWRendering(false)
    , m_useHardware(true)
{
    // GraphicsContext creates a 32bpp SkBitmap, so 4 bytes per pixel.
    if (!PlatformBridge::canSatisfyMemoryAllocation(size.width() * size.height() * 4))
        success = false;
    else {
        m_context.set(GraphicsContext::createOffscreenContext(size.width(), size.height()));
        m_data.m_platformContext = m_context->platformContext();
        char prop_hardware[16];
        property_get("browser.canvas2d.usegpu", prop_hardware, "1");
        if (0 == strcmp(prop_hardware, "0")) {
            m_useHardware = false;
            m_accelerateRendering = false;
            success = true;
            return;
        }

        android_printLog(ANDROID_LOG_DEBUG, "ImageBuffer", "(%p) ready to switch to hardware", this);
        SkCanvas* canvas = android_gc2canvas(m_context.get());
        SetupUnacceleratedCanvas(canvas, size, &m_data, false, false);
        m_accelerateRendering = false;
        if ((size.width() * size.height() > SMALLCANVAS) && (renderingMode == Accelerated)) {
            m_canTryHWRendering = true;
        }
        success = true;
    }
}

ImageBuffer::~ImageBuffer()
{
}

void ImageBuffer::softwareRenderingFallback(bool fallback, bool force)
{
    if (fallback && m_accelerateRendering) {
        SkCanvas* canvas = android_gc2canvas(m_context.get());
        SetupUnacceleratedCanvas(canvas, m_size, &m_data, true);
        m_canvasFBO = 0;
        m_accelerateRendering = false;
        android_printLog(ANDROID_LOG_DEBUG, "ImageBuffer", "(%p) setup sw canvas force: %d", this, force);
    } else if (m_useHardware && !fallback && !m_accelerateRendering && (force || m_canTryHWRendering)) {
        if (GLUtils::mContext != EGL_NO_CONTEXT) {
            SkCanvas* canvas = android_gc2canvas(m_context.get());
            m_canvasFBO = SetupAcceleratedCanvas(canvas, m_size, &m_data, true);
            m_accelerateRendering = true;
            m_canTryHWRendering = false;
            android_printLog(ANDROID_LOG_DEBUG, "ImageBuffer", "(%p) setup hw canvas force: %d", this, force);
        }
    }
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

GraphicsContext3D* ImageBuffer::context3D() const
{
    return m_context->platformContext()->context3D();
}

bool ImageBuffer::drawsUsingCopy() const
{
    return true;
}

PassRefPtr<Image> ImageBuffer::copyImage() const
{
    ASSERT(context());

    SkCanvas* canvas = context()->platformContext()->getCanvas();
    if (!canvas)
      return 0;

    SkDevice* device = canvas->getDevice();
    const SkBitmap& orig = device->accessBitmap(false);

    SkBitmap copy;
    if (PlatformBridge::canSatisfyMemoryAllocation(orig.getSize()))
        orig.copyTo(&copy, orig.config());

    SkBitmapRef* ref = new SkBitmapRef(copy);
    RefPtr<Image> image = BitmapImage::create(ref, 0);
    ref->unref();
    return image;
}

void ImageBuffer::clip(GraphicsContext* context, const FloatRect& rect) const
{
    SkDebugf("xxxxxxxxxxxxxxxxxx clip not implemented\n");
}

void ImageBuffer::draw(GraphicsContext* dstContext, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, bool useLowQualityScale)
{
    SkCanvas * canvas = context()->platformContext()->getCanvas();
    SkDevice * device = canvas->getDevice();
    const SkBitmap & orig = device->accessBitmap(false);

    SkBitmapRef * ref = new SkBitmapRef(orig);
    RefPtr<Image> imageCopy = BitmapImage::create(ref, 0);
    ref->unref();
    dstContext->drawImage(imageCopy.get(), styleColorSpace, destRect, srcRect, op, useLowQualityScale);
}

void ImageBuffer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    RefPtr<Image> imageCopy = copyImage();
    imageCopy->drawPattern(context, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
}

template <Multiply multiplied>
PassRefPtr<ByteArray> getImageData(const IntRect& rect, SkCanvas* canvas,
                                   const IntSize& size)
{
    float area = 4.0f * rect.width() * rect.height();
    if (area > static_cast<float>(std::numeric_limits<int>::max()))
        return 0;

    RefPtr<ByteArray> result = ByteArray::create(rect.width() * rect.height() * 4);
    unsigned char* data = result->data();

    if (rect.x() < 0 || rect.y() < 0 || rect.maxX() > size.width() || rect.maxY() > size.height())
        memset(data, 0, result->length());

    int originX = rect.x();
    int destX = 0;
    if (originX < 0) {
        destX = -originX;
        originX = 0;
    }

    int endX = rect.maxX();
    if (endX > size.width())
        endX = size.width();
    int numColumns = endX - originX;

    if (numColumns <= 0)
        return result.release();

    int originY = rect.y();
    int destY = 0;
    if (originY < 0) {
        destY = -originY;
        originY = 0;
    }

    int endY = rect.maxY();
    if (endY > size.height())
        endY = size.height();
    int numRows = endY - originY;

    if (numRows <= 0)
        return result.release();

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;


    unsigned destBytesPerRow = 4 * rect.width();
    SkBitmap destBitmap;
    destBitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height(), destBytesPerRow);
    destBitmap.setPixels(data);

    canvas->readPixels(&destBitmap, originX, originY, config8888);
    return result.release();
}

template <Multiply multiplied>
void putImageData(ByteArray*& source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint,
                  SkDevice* dstDevice, const IntSize& size)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originX = sourceRect.x();
    int destX = destPoint.x() + sourceRect.x();
    ASSERT(destX >= 0);
    ASSERT(destX < size.width());
    ASSERT(originX >= 0);
    ASSERT(originX < sourceRect.maxX());

    int endX = destPoint.x() + sourceRect.maxX();
    ASSERT(endX <= size.width());

    int numColumns = endX - destX;

    int originY = sourceRect.y();
    int destY = destPoint.y() + sourceRect.y();
    ASSERT(destY >= 0);
    ASSERT(destY < size.height());
    ASSERT(originY >= 0);
    ASSERT(originY < sourceRect.maxY());

    int endY = destPoint.y() + sourceRect.maxY();
    ASSERT(endY <= size.height());
    int numRows = endY - destY;

    unsigned srcBytesPerRow = 4 * sourceSize.width();

    // If the device's bitmap doesn't have pixels we will make a temp and call writePixels on the device.
    SkBitmap destBitmap;
    destBitmap.setConfig(SkBitmap::kARGB_8888_Config, numColumns, numRows, srcBytesPerRow);
    destBitmap.setPixels(source->data() + originY * srcBytesPerRow + originX * 4);

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;
    dstDevice->writePixels(destBitmap, destX, destY, config8888);

}

PassRefPtr<ByteArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    GraphicsContext* gc = this->context();
    return getImageData<Unmultiplied>(rect, android_gc2canvas(gc), m_size);
}

void ImageBuffer::putUnmultipliedImageData(ByteArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    GraphicsContext* gc = this->context();
    putImageData<Unmultiplied>(source, sourceSize, sourceRect, destPoint, android_gc2canvas(gc)->getDevice(), m_size);
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    // Encode the image into a vector.
    SkDynamicMemoryWStream pngStream;
    SkCanvas* canvas = android_gc2canvas(context());
    SkDevice* device = canvas->getDevice();
    SkBitmap bitmap = device->accessBitmap(false);
    bitmap.lockPixels(); // balanced by our destructor, or explicitly if getPixels() fails
    if (!bitmap.getPixels()) {
        bitmap.unlockPixels();
        if (!canvas->readPixels(&bitmap, 0, 0, SkCanvas::kRGBA_Unpremul_Config8888))
            return "data:,";
    }
    SkImageEncoder::EncodeStream(&pngStream, bitmap, SkImageEncoder::kPNG_Type, 100);

    // Convert it into base64.
    Vector<char> pngEncodedData;
    SkData* streamData = pngStream.copyToData();
    pngEncodedData.append((char*)streamData->data(), streamData->size());
    streamData->unref();
    Vector<char> base64EncodedData;
    base64Encode(pngEncodedData, base64EncodedData);
    // Append with a \0 so that it's a valid string.
    base64EncodedData.append('\0');

    // And the resulting string.
    return String::format("data:image/png;base64,%s", base64EncodedData.data());
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookupTable)
{
    notImplemented();
}

size_t ImageBuffer::dataSize() const
{
    return m_size.width() * m_size.height() * 4;
}

}
