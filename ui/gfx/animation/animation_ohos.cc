// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

namespace gfx {

// static
bool Animation::ShouldRenderRichAnimationImpl() {
  return true;
}

// static
bool Animation::ScrollAnimationsEnabledBySystem() {
  return true;
}

// static
void Animation::UpdatePrefersReducedMotion() {
  prefers_reduced_motion_ = false;
}

}  // namespace gfx
