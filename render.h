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
#pragma once

#include <functional>
#include <ostream>
#include <unordered_map>
#include <vector>
#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/types/optional.h"
#include "attr.h"
#include "rhea/simplex_solver.hpp"
#include "rhea/variable.hpp"

class Renderer;
class Widget;
class DeviceContext;

enum class WidgetType {
  CONTENT,
  ROW,
  COLUMN,
  STACK,
  FLOAT,
};

enum class Justify {
  FILL,    // no padding
  START,   // all elements crowd to start
  END,     // all elements crowd to end
  CENTER,  // padding to left & right evenly
  SPACE_BETWEEN,
  SPACE_AROUND,
  SPACE_EVENLY,
};

class Widget {
 public:
  Widget(Renderer* renderer, WidgetType type, uint64_t id)
      : type_(type), id_(id), renderer_(renderer) {}

  typedef absl::InlinedVector<Widget*, 4> ChildVector;

  Renderer* renderer() const { return renderer_; }

  template <class T>
  T as() {
    T r;
    GetValue(&r);
    return r;
  }

  uint64_t id() const { return id_; }

  typedef absl::optional<absl::string_view> OptionalStableID;

  Widget* MakeContent(OptionalStableID id = OptionalStableID()) {
    return Materialize(WidgetType::CONTENT, id);
  }
  Widget* MakeRow(OptionalStableID id = OptionalStableID()) {
    return Materialize(WidgetType::ROW, id);
  }
  Widget* MakeColumn(OptionalStableID id = OptionalStableID()) {
    return Materialize(WidgetType::COLUMN, id);
  }
  Widget* MakeStack(OptionalStableID id = OptionalStableID()) {
    return Materialize(WidgetType::STACK, id);
  }
  Widget* MakeFloat(OptionalStableID id = OptionalStableID()) {
    return Materialize(WidgetType::FLOAT, id);
  }
  Widget* MakeSimpleText(CharFmt fmt, const std::vector<std::string>& text);

  typedef std::function<void(DeviceContext* device)> DrawCall;
  void Draw(DrawCall draw) { draw_ = draw; }

  WidgetType type() const { return type_; }

  const rhea::variable& left() const { return left_; }
  const rhea::variable& right() const { return right_; }
  const rhea::variable& top() const { return top_; }
  const rhea::variable& bottom() const { return bottom_; }
  rhea::linear_expression width() const { return right_ - left_; }
  rhea::linear_expression height() const { return bottom_ - top_; }
  rhea::linear_expression screen_left();
  rhea::linear_expression screen_right();
  rhea::linear_expression screen_top();
  rhea::linear_expression screen_bottom();

  void PaintSelf(DeviceContext* devctx) const {
    if (!draw_) return;
    draw_(devctx);
  }

  const ChildVector& children() const { return children_; }

 private:
  friend class Renderer;

  void EnterFrame(Widget* parent);
  void FinalizeConstraints(Renderer* renderer);
  void UpdateAnimations(Renderer* renderer);
  bool EndFrame();

  uint64_t GenUID(OptionalStableID id);
  Widget* Materialize(WidgetType type, OptionalStableID id);

  void ApplyJustify(rhea::simplex_solver* solver, Justify justify,
                    const ChildVector& widgets, rhea::variable(Widget::*start),
                    rhea::variable(Widget::*end));

  const WidgetType type_;
  const uint64_t id_;
  Renderer* const renderer_;
  uint64_t nextid_;
  DrawCall draw_;
  rhea::variable left_;
  rhea::variable right_;
  rhea::variable top_;
  rhea::variable bottom_;
  rhea::variable container_pad_;
  rhea::variable item_fill_;
  Justify justify_ = Justify::FILL;
  Widget* parent_ = nullptr;
  struct Animator {
    bool initialized = false;
    absl::Duration blend_time = absl::Milliseconds(200);
    double target;
    double initial_value;
    double initial_velocity;
    absl::Time target_set;
    // returns true if animating
    bool DeclTarget(absl::Time time, double target);
    struct Coefficients {
      double a, b, c, d;
    };
    Coefficients CalcCoefficients() const;
    double ScaleTime(absl::Time time) const;
    double ValueAt(absl::Time time, Renderer* renderer) const;
  };
  Animator ani_midx_;
  Animator ani_midy_;
  Animator ani_szx_;
  Animator ani_szy_;
  ChildVector children_;
  bool live_ = false;
};

class Device {
 public:
  struct Extents {
    int win_height;
    int win_width;
    int chr_height;
    int chr_width;
  };

  virtual void Paint(const Renderer* renderer, const Widget& widget) = 0;
  virtual Extents GetExtents() = 0;
};

class DeviceContext {
 public:
  virtual int width() const = 0;
  virtual int height() const = 0;

  virtual void MoveCursor(int row, int col) = 0;
  virtual void PutChar(int row, int col, uint32_t chr, CharFmt fmt) = 0;
};

class Renderer : public Widget {
 public:
  explicit Renderer(Device* device)
      : Widget(this, WidgetType::STACK, 0), device_(device) {}

  void BeginFrame();
  absl::optional<absl::Time> FinishFrame();

  rhea::simplex_solver* solver() { return solver_.get(); }
  const Device::Extents& extents() const { return extents_; }

 private:
  friend class Widget;

  Device* const device_;
  bool animating_ = false;
  absl::Time frame_time_;
  Device::Extents extents_;
  std::unordered_map<uint64_t, std::unique_ptr<Widget>> widgets_;
  std::unique_ptr<rhea::simplex_solver> solver_{new rhea::simplex_solver};
};
