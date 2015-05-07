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

#include "config.h"

#if ENABLE(ACCELERATED_2D_CANVAS) || USE(CANVAS2D_GPUSKIA)

#include "SharedGraphicsContext3D.h"

#include "AffineTransform.h"
#include "BicubicShader.h"
#include "Color.h"
#include "ConvolutionShader.h"
#include "DrawingBuffer.h"
#include "Extensions3D.h"
#include "FloatRect.h"
#include "IntSize.h"
#include "LoopBlinnSolidFillShader.h"
#include "SolidFillShader.h"
#include "TexShader.h"

#if ENABLE(SKIA_GPU)
#include "GrContext.h"
// Limit the number of textures we hold in the bitmap->texture cache.
static const int maxTextureCacheCount = 512;
// Limit the bytes allocated toward textures in the bitmap->texture cache.
static const size_t maxTextureCacheBytes = 50 * 1024 * 1024;
#endif

#include <wtf/OwnArrayPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// static
GraphicsContext3D* SharedGraphicsContext3D::create(HostWindow* host)
{
    GraphicsContext3D::Attributes attr;
    attr.depth = false;
    attr.stencil = false;
    attr.antialias = true;
    attr.canRecoverFromContextLoss = false; // Canvas contexts can not handle lost contexts.
    static RefPtr<GraphicsContext3D> context = GraphicsContext3D::create(attr, host);
    return context.get();
}

#if ENABLE(SKIA_GPU)
GrContext* SharedGraphicsContext3D::grContext()
{
    if (!m_grContext) {
        m_grContext = GrContext::CreateGLShaderContext();
        m_grContext->setTextureCacheLimits(maxTextureCacheCount, maxTextureCacheBytes);
    }
    return m_grContext;
}
#endif

} // namespace WebCore

#endif
