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

#include <signal.h>
#include <atomic>
#include "GrBackendSurface.h"
#include "GrContext.h"
#include "SDL.h"
#include "SDL_opengl.h"
#include "SkCanvas.h"
#include "SkRandom.h"
#include "SkSurface.h"
#include "SkTypeface.h"
#include "application.h"
#include "buffer.h"
#include "client.h"
#include "client_collaborator.h"
#include "fontconfig/fontconfig.h"
#include "fontconfig_conf_files.h"
#include "gl/GrGLAssembleInterface.h"
#include "gl/GrGLInterface.h"
#include "gl/GrGLUtil.h"
#include "render.h"

static const int kMsaaSampleCount = 0;  // 4;
static const int kStencilBits = 8;      // Skia needs 8 stencil bits

static void ConfigureFontConfig() {
#ifdef __APPLE__
  std::string config_prefix = R"(<?xml version="1.0"?>
    <!DOCTYPE fontconfig SYSTEM "fonts.dtd">
    <fontconfig>)";
  std::string default_dirs = R"(
    <cachedir>/usr/local/var/cache/fontconfig</cachedir>
    <cachedir>~/.fontconfig</cachedir>
    <dir>/System/Library/Fonts</dir>
    <dir>/Library/Fonts</dir>
    <dir>~/Library/Fonts</dir>)";
  std::string config_suffix = "</fontconfig>";
  FcConfig* cfg = FcConfigCreate();
  FcConfigParseAndLoadFromMemory(
      cfg, reinterpret_cast<const FcChar8*>(
               absl::StrCat(config_prefix, default_dirs, fontconfig_conf_files,
                            config_suffix)
                   .c_str()),
      true);
  FcConfigSetCurrent(cfg);
#endif
}

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

    textual_paint_.setColor(SK_ColorBLACK);
    textual_paint_.setTypeface(
        SkTypeface::MakeFromName("Fira Code", SkFontStyle()));
    textual_paint_.setAntiAlias(true);
    textual_paint_.setHinting(SkPaint::kFull_Hinting);
    textual_paint_.setFlags(textual_paint_.getFlags() |
                            SkPaint::kSubpixelText_Flag |
                            SkPaint::kLCDRenderText_Flag);
  }

  SkCanvas* canvas() const { return canvas_; }
  int width() const { return winsz_.width; }
  int height() const { return winsz_.height; }

  const SkPaint& textual_paint() { return textual_paint_; }

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
  const sk_sp<const GrGLInterface> interface_{
      GrGLAssembleInterface(nullptr, +[](void* ctx, const char* name) {
        return (GrGLFuncPtr)SDL_GL_GetProcAddress(name);
      })};
  const sk_sp<GrContext> grcontext_{GrContext::MakeGL(interface_.get())};
  GrBackendRenderTarget target_;
  SkSurfaceProps props_{SkSurfaceProps::kLegacyFontHost_InitType};
  const sk_sp<SkSurface> surface_;
  SkCanvas* canvas_;
  SkPaint textual_paint_;
};

class GUI : public Application, public Device, public Invalidator {
 public:
  GUI(int argc, char** argv) : client_(argv[0], FileFromCmdLine(argc, argv)) {
    ConfigureFontConfig();

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

    window_ = SDL_CreateWindow("ced", SDL_WINDOWPOS_CENTERED,
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

    buffer_ = client_.MakeBuffer(FileFromCmdLine(argc, argv));
  }

  ~GUI() {}

  int Run() override {
    while (!done_) HandleEvents(Render());

    return 0;
  }

  void ClipboardPut(const std::string& s) override {
    SDL_SetClipboardText(s.c_str());
  }

  std::string ClipboardGet() override {
    char* s = SDL_GetClipboardText();
    if (s == nullptr) return std::string();
    return s;
  }

  Extents GetExtents() override {
    SkPaint::FontMetrics metrics;
    SkScalar spacing = ctx_->textual_paint().getFontMetrics(&metrics);
    return Extents{
        ctx_->height(), ctx_->width(), static_cast<int>(spacing),
        static_cast<int>(metrics.fAvgCharWidth),
    };
  }

  void Paint(const Renderer* renderer, const Widget& widget) override {
    class Ctx final : public DeviceContext {
     public:
      Ctx(GUICtx* guictx)
          : guictx_(guictx),
            canvas_(guictx->canvas()),
            clip_bounds_(canvas_->getLocalClipBounds()) {}

      int width() const override { return clip_bounds_.width(); }
      int height() const override { return clip_bounds_.height(); }
      void MoveCursor(int row, int col) override {}

      void PutChar(int row, int col, uint32_t chr, CharFmt fmt) override {
        SkPaint paint = guictx_->textual_paint();
        paint.setColor(SkColorSetARGB(fmt.foreground.a, fmt.foreground.r,
                                      fmt.foreground.g, fmt.foreground.b));
        char c = chr;
        canvas_->drawText(&c, 1, col, row, paint);
      }

     private:
      GUICtx* const guictx_;
      SkCanvas* const canvas_;
      const SkRect clip_bounds_;
    };
    SkCanvas* canvas = ctx_->canvas();
    SkAutoCanvasRestore restore_canvas(canvas, true);
    canvas->translate(widget.left().value(), widget.top().value());
    canvas->clipRect(
        SkRect::MakeWH(widget.right().value() - widget.left().value(),
                       widget.bottom().value() - widget.top().value()));
    Ctx ctx(ctx_.get());
    widget.PaintSelf(&ctx);
    for (const auto* c : widget.children()) {
      Paint(renderer, *c);
    }
  }

 private:
  absl::optional<absl::Time> Render() {
    if (!ctx_) {
      ctx_.reset(new GUICtx(window_));
    }
    renderer_.BeginFrame();
    auto top = renderer_.MakeRow(Widget::Options().set_id("top"));
    RenderContainers containers{
        top->MakeRow(Widget::Options().set_id("main")),
        top->MakeColumn(Widget::Options().set_id("side_bar")),
    };
    ClientCollaborator::All_Render(containers, &theme_);
    absl::optional<absl::Time> next_frame = renderer_.FinishFrame();
#if 0
    SkCanvas* canvas = ctx_->canvas();
    canvas->scale(1, 1);
    canvas->clear(SK_ColorWHITE);
    SkPaint paint = ctx_->textual_paint();
    std::string hello = "Hello world!";
    canvas->drawText(hello.c_str(), hello.length(), SkIntToScalar(100),
                     SkIntToScalar(100), paint);
#endif
    ctx_->canvas()->flush();
    SDL_GL_SwapWindow(window_);
    return next_frame;
  }

  static uint32_t TimerCB(uint32_t interval, void* param) {
    SDL_Event event;
    SDL_UserEvent userevent;

    /* In this example, our callback pushes an SDL_USEREVENT event
    into the queue, and causes our callback to be called again at the
    same interval: */

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = param;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    return 0;
  }

  void After(int millis, std::function<bool()> cb) {
    SDL_AddTimer(millis, TimerCB, new std::function<void()>(cb));
  }

  void Invalidate() override {
    auto fn_at_inval = frame_number_;
    After(10, [this, fn_at_inval]() { return frame_number_ == fn_at_inval; });
  }

  void HandleEvents(absl::optional<absl::Time> end_time) {
    SDL_Event event;
    std::function<bool(SDL_Event*)> next_event;
    if (end_time) {
      next_event = [end_time](SDL_Event* ev) {
        auto now = absl::Now();
        if (*end_time < now) return SDL_PollEvent(ev);
        return SDL_WaitEventTimeout(ev, ToInt64Milliseconds(*end_time - now));
      };
    } else {
      next_event = [](SDL_Event* ev) { return SDL_WaitEvent(ev); };
    }
    bool brk = false;
    while (next_event(&event)) {
      switch (event.type) {
        case SDL_KEYDOWN: {
          SDL_Keycode key = event.key.keysym.sym;
          if (key == SDLK_ESCAPE) {
            done_ = true;
          }
          brk = true;
          break;
        }
        case SDL_WINDOWEVENT: {
          switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
              ctx_.reset();
              brk = true;
              break;
          }
          break;
        }
        case SDL_QUIT:
          done_ = true;
          break;
        case SDL_USEREVENT: {
          auto* fn = static_cast<std::function<bool()>*>(event.user.data1);
          brk |= (*fn)();
          delete fn;
        }
        default:
          break;
      }
      if (brk) {
        // after the first event we go back to polling to flush out the rest
        next_event = [](SDL_Event* ev) { return SDL_PollEvent(ev); };
      }
    }
    frame_number_++;
  }

  static boost::filesystem::path FileFromCmdLine(int argc, char** argv) {
    if (argc != 2) {
      throw std::runtime_error("Expected filename to open");
    }
    return argv[1];
  }

  bool done_ = false;
  SDL_Window* window_ = nullptr;
  Theme theme_{Theme::DEFAULT};
  Client client_;
  Renderer renderer_{this};
  uint64_t frame_number_ = 0;
  std::unique_ptr<GUICtx> ctx_;
  std::unique_ptr<Buffer> buffer_;
};

REGISTER_APPLICATION(GUI);
