// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageEncoder_h
#define ImageEncoder_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Vector.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace blink {

class VectorWStream : public SkWStream {
 public:
  VectorWStream(Vector<unsigned char>* dst) : dst_(dst) {
    DCHECK(dst_);
    DCHECK_EQ(0UL, dst->size());
  }

  bool write(const void* buffer, size_t size) override {
    dst_->Append((const unsigned char*)buffer, size);
    return true;
  }

  size_t bytesWritten() const override { return dst_->size(); }

 private:
  // Does not have ownership.
  Vector<unsigned char>* dst_;
};

class PLATFORM_EXPORT ImageEncoder {
 public:
  static bool Encode(Vector<unsigned char>* dst,
                     const SkPixmap& src,
                     const SkJpegEncoder::Options&);

  static bool Encode(Vector<unsigned char>* dst,
                     const SkPixmap& src,
                     const SkPngEncoder::Options&);

  static std::unique_ptr<ImageEncoder> Create(Vector<unsigned char>* dst,
                                              const SkPixmap& src,
                                              const SkJpegEncoder::Options&);

  static std::unique_ptr<ImageEncoder> Create(Vector<unsigned char>* dst,
                                              const SkPixmap& src,
                                              const SkPngEncoder::Options&);

  bool encodeRows(int numRows) { return encoder_->encodeRows(numRows); }

  /**
   *  If quality is in [0, 1], this will simply convert to a [0, 100]
   *  integer scale (which is what is used by libjpeg-turbo).
   *
   *  Otherwise, this will return the default value.
   */
  static int ComputeJpegQuality(double quality);

 private:
  ImageEncoder(Vector<unsigned char>* dst) : dst_(dst) {}

  VectorWStream dst_;
  std::unique_ptr<SkEncoder> encoder_;
};
};

#endif
