// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <OpenGL/gl.h>
#include <signal.h>
#include <atomic>
#include "GrBackendSurface.h"
#include "GrContext.h"
#include "SDL.h"
#include "SkCanvas.h"
#include "SkRandom.h"
#include "SkSurface.h"
#include "SkTypeface.h"
#include "application.h"
#include "buffer.h"
#include "client.h"
#include "gl/GrGLInterface.h"
#include "gl/GrGLUtil.h"

static const int kMsaaSampleCount = 0;  // 4;
static const int kStencilBits = 8;      // Skia needs 8 stencil bits

class GUICtx {
 public:
  explicit GUICtx(SDL_Window* window)
      : winsz_(window),
        target_(winsz_.width, winsz_.height, kMsaaSampleCount, kStencilBits,
                kSkia8888_GrPixelConfig, MakeFrameBufferInfo()),
        surface_(SkSurface::MakeFromBackendRenderTarget(
            grcontext_.get(), target_, kBottomLeft_GrSurfaceOrigin, nullptr,
            &props_)),
        canvas_(surface_->getCanvas()) {
    SkASSERT(grcontext_);
  }

  SkCanvas* canvas() const { return canvas_; }

 private:
  struct WinSz {
    int width, height;
    WinSz(SDL_Window* window) {
      SDL_GL_GetDrawableSize(window, &width, &height);
    }
  };
  GrGLFramebufferInfo MakeFrameBufferInfo() {
    GrGLint buffer;
    GR_GL_GetIntegerv(interface_.get(), GR_GL_FRAMEBUFFER_BINDING, &buffer);
    GrGLFramebufferInfo info;
    info.fFBOID = (GrGLuint)buffer;
    return info;
  }
  const WinSz winsz_;
  const sk_sp<const GrGLInterface> interface_{GrGLCreateNativeInterface()};
  const sk_sp<GrContext> grcontext_{GrContext::MakeGL(interface_.get())};
  GrBackendRenderTarget target_;
  SkSurfaceProps props_{SkSurfaceProps::kLegacyFontHost_InitType};
  const sk_sp<SkSurface> surface_;
  SkCanvas* canvas_;
};

class GUI : public Application {
 public:
  GUI(int argc, char** argv) : client_(argv[0], FileFromCmdLine(argc, argv)) {
    uint32_t windowFlags = 0;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GLContext glContext = nullptr;
#if defined(SK_BUILD_FOR_ANDROID) || defined(SK_BUILD_FOR_IOS)
    // For Android/iOS we need to set up for OpenGL ES and we make the window hi
    // res & full screen
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                  SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN_DESKTOP |
                  SDL_WINDOW_ALLOW_HIGHDPI;
#else
    // For all other clients we use the core profile and operate in a window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#endif
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, kStencilBits);

    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    if (kMsaaSampleCount != 0) {
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, kMsaaSampleCount);
    }

    /*
     * In a real application you might want to initialize more subsystems
     */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
      throw std::runtime_error(SDL_GetError());
    }

    // Setup window
    // This code will create a window with the same resolution as the user's
    // desktop.
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
      throw std::runtime_error(SDL_GetError());
    }

    window_ = SDL_CreateWindow("SDL Window", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, dm.w, dm.h, windowFlags);

    if (!window_) {
      throw std::runtime_error(SDL_GetError());
    }

    // To go fullscreen
    // SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);

    // try and setup a GL context
    glContext = SDL_GL_CreateContext(window_);
    if (!glContext) {
      throw std::runtime_error(SDL_GetError());
    }

    int success = SDL_GL_MakeCurrent(window_, glContext);
    if (success != 0) {
      throw std::runtime_error(SDL_GetError());
    }
  }

  ~GUI() {}

  int Run() {
    while (!done_) {
      Render();
      HandleEvents();
    }

    return 0;
  }

 private:
  void Render() {
    if (!ctx_) {
      ctx_.reset(new GUICtx(window_));
    }
    SkCanvas* canvas = ctx_->canvas();
    canvas->scale(1, 1);
    canvas->clear(SK_ColorWHITE);
    SkPaint paint;
    paint.setColor(SK_ColorBLACK);
    paint.setTypeface(SkTypeface::MakeFromName("Fira Code", SkFontStyle()));
    paint.setAntiAlias(true);
    paint.setFlags(paint.getFlags() | SkPaint::kSubpixelText_Flag |
                   SkPaint::kLCDRenderText_Flag);
    std::string hello = "Hello world!";
    canvas->drawText(hello.c_str(), hello.length(), SkIntToScalar(100),
                     SkIntToScalar(100), paint);
    canvas->flush();
#if 0
    int dw, dh;
    SDL_GL_GetDrawableSize(window_, &dw, &dh);
    glViewport(0, 0, dw, dh);
    glClearColor(1, 1, 1, 1);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
#endif
    SDL_GL_SwapWindow(window_);
  }

  void HandleEvents() {
    SDL_Event event;
    std::function<bool(SDL_Event*)> next_event =
        animating_ ? [](SDL_Event* ev) { return SDL_WaitEventTimeout(ev, 5); }
                   : [](SDL_Event* ev) { return SDL_WaitEvent(ev); };
    while (next_event(&event)) {
      switch (event.type) {
        case SDL_KEYDOWN: {
          SDL_Keycode key = event.key.keysym.sym;
          if (key == SDLK_ESCAPE) {
            done_ = true;
          }
          break;
        }
        case SDL_WINDOWEVENT: {
          switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
              ctx_.reset();
              break;
          }
          break;
        }
        case SDL_QUIT:
          done_ = true;
          break;
        default:
          break;
      }
      // after the first event we go back to polling to flush out the rest
      next_event = [](SDL_Event* ev) { return SDL_PollEvent(ev); };
    }
  }

  static boost::filesystem::path FileFromCmdLine(int argc, char** argv) {
    if (argc != 2) {
      throw std::runtime_error("Expected filename to open");
    }
    return argv[1];
  }

  bool done_ = false;
  bool animating_ = false;
  SDL_Window* window_ = nullptr;
  Client client_;
  std::unique_ptr<GUICtx> ctx_;
};

REGISTER_APPLICATION(GUI);
