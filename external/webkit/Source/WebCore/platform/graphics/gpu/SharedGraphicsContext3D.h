/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef SharedGraphicsContext3D_h
#define SharedGraphicsContext3D_h

#include "GraphicsContext3D.h"
#include "GraphicsTypes.h"
#include "ImageSource.h"
#include "Texture.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(SKIA_GPU)
class GrContext;
#endif

namespace WebCore {

class AffineTransform;
class BicubicShader;
class Color;
class ConvolutionShader;
class DrawingBuffer;
class FloatRect;
class HostWindow;
class IntSize;
class LoopBlinnSolidFillShader;

typedef HashMap<NativeImagePtr, RefPtr<Texture> > TextureHashMap;

class SharedGraphicsContext3D : public RefCounted<SharedGraphicsContext3D> {
public:
    static GraphicsContext3D* create(HostWindow* webview);

#if ENABLE(SKIA_GPU)
    GrContext* m_grContext;
#endif

private:
    RefPtr<GraphicsContext3D> m_context;
};

const float cMaxSigma = 4.0f;
const int cMaxKernelWidth = 25;

} // namespace WebCore

#endif // SharedGraphicsContext3D_h
