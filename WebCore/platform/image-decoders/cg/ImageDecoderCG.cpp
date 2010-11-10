/*
 * Copyright (C) 2010 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ImageDecoder.h"

#include <CoreGraphics/CGColorSpace.h>
#include <CoreGraphics/CGImage.h>

namespace WebCore {

void RGBA32Buffer::copyReferenceToBitmapData(const RGBA32Buffer& other)
{
    ASSERT(this != &other);
    m_backingStore = other.m_backingStore;
    m_bytes = reinterpret_cast<PixelData*>(CFDataGetMutableBytePtr(m_backingStore.get()));
    // FIXME: The rest of this function seems redundant with RGBA32Buffer::copyBitmapData.
    m_size = other.m_size;
    setHasAlpha(other.m_hasAlpha);
}

bool RGBA32Buffer::copyBitmapData(const RGBA32Buffer& other)
{
    if (this == &other)
        return true;

    m_backingStore.adoptCF(CFDataCreateMutableCopy(kCFAllocatorDefault, 0, other.m_backingStore.get()));
    m_bytes = reinterpret_cast<PixelData*>(CFDataGetMutableBytePtr(m_backingStore.get()));
    m_size = other.m_size;
    setHasAlpha(other.m_hasAlpha);
    return true;
}

bool RGBA32Buffer::setSize(int newWidth, int newHeight)
{
    m_backingStore.adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    CFDataSetLength(m_backingStore.get(), newWidth * newHeight * sizeof(PixelData));
    m_bytes = reinterpret_cast<PixelData*>(CFDataGetMutableBytePtr(m_backingStore.get()));
    m_size = IntSize(newWidth, newHeight);

    zeroFill();
    return true;
}

NativeImagePtr RGBA32Buffer::asNewNativeImage() const
{
    // FIXME: Figure out the right color space.
    DEFINE_STATIC_LOCAL(RetainPtr<CGColorSpaceRef>, deviceColorSpace, (AdoptCF, CGColorSpaceCreateDeviceRGB()));
    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithCFData(m_backingStore.get()));

    CGImageAlphaInfo alphaInfo = m_premultiplyAlpha ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaFirst;

    return CGImageCreate(width(), height(), 8, 32, width() * sizeof(PixelData), deviceColorSpace.get(),
        alphaInfo | kCGBitmapByteOrder32Host, dataProvider.get(), 0, false, kCGRenderingIntentDefault);
}

} // namespace WebCore