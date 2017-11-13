#pragma once

#include <functional>
#include <vector>
#include "layout.h"

// deferred text renderer: pass layout constraints, draw function
// it figures out how to satisfy constraints and calls draw functions
template <class Context>
class Renderer {
 public:
  class Rect {
   public:
    explicit Rect(lay_vec4 v) : rect_(v) {}

    lay_scalar column() const { return rect_[0]; }
    lay_scalar row() const { return rect_[1]; }
    lay_scalar width() const { return rect_[2]; }
    lay_scalar height() const { return rect_[3]; }

   private:
    lay_vec4 rect_;
  };

  class ItemRef {
   public:
    ItemRef() : renderer_(nullptr), id_(0) {}
    ItemRef(Renderer* renderer, lay_id id) : renderer_(renderer), id_(id) {}

    ItemRef& FixSize(lay_scalar width, lay_scalar height) {
      lay_set_size_xy(&renderer_->ctx_, id_, width, height);
      return *this;
    }

    operator bool() const { return renderer_ != nullptr; }

    // available post-Renderer::Layout
    Rect GetRect() { return Rect(lay_get_rect(&renderer_->ctx_, id_)); }

   private:
    Renderer* renderer_;
    lay_id id_;
  };

  class ContainerRef {
   public:
    ContainerRef(Renderer* renderer, lay_id id)
        : renderer_(renderer), id_(id) {}

    ContainerRef& FixSize(lay_scalar width, lay_scalar height) {
      lay_set_size_xy(&renderer_->ctx_, id_, width, height);
      return *this;
    }

    template <class F>
    ItemRef AddItem(uint32_t behave, F&& draw) {
      lay_id id = lay_item(&renderer_->ctx_);
      lay_set_behave(&renderer_->ctx_, id, behave);
      lay_insert(&renderer_->ctx_, id_, id);
      renderer_->draw_.emplace_back(DrawCall{id, std::move(draw)});
      return ItemRef{renderer_, id};
    }

    ContainerRef AddContainer(uint32_t behave, uint32_t flags) {
      lay_id id = lay_item(&renderer_->ctx_);
      lay_set_behave(&renderer_->ctx_, id, behave);
      lay_set_contain(&renderer_->ctx_, id, flags);
      lay_insert(&renderer_->ctx_, id_, id);
      return ContainerRef{renderer_, id};
    }

   private:
    Renderer* renderer_;
    lay_id id_;
  };

  Renderer() { lay_init_context(&ctx_); }
  ~Renderer() { lay_destroy_context(&ctx_); }

  ContainerRef AddContainer(uint32_t flags) {
    lay_id id = lay_item(&ctx_);
    lay_set_contain(&ctx_, id, flags);
    return ContainerRef{this, id};
  }

  void Layout() { lay_run_context(&ctx_); }

  void Draw(Context* ctx) {
    for (const auto& draw : draw_) {
      Rect rect(lay_get_rect(&ctx_, draw.id));
      ctx->window = &rect;
      draw.cb(ctx);
    }
  }

 private:
  struct DrawCall {
    lay_id id;
    std::function<void(Context* context)> cb;
  };

  lay_context ctx_;
  std::vector<DrawCall> draw_;
};
