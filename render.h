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

inline std::ostream& operator<<(std::ostream& out, WidgetType t) {
  switch (t) {
    case WidgetType::CONTENT:
      return out << "CONTENT";
    case WidgetType::ROW:
      return out << "ROW";
    case WidgetType::COLUMN:
      return out << "COLUMN";
    case WidgetType::STACK:
      return out << "STACK";
    case WidgetType::FLOAT:
      return out << "FLOAT";
  }
  return out << "<<UNKNOWNTYPE>>";
}

enum class Justify {
  FILL,    // no padding
  START,   // all elements crowd to start
  END,     // all elements crowd to end
  CENTER,  // padding to left & right evenly
  SPACE_BETWEEN,
  SPACE_AROUND,
  SPACE_EVENLY,
};

inline std::ostream& operator<<(std::ostream& out, Justify j) {
  switch (j) {
    case Justify::FILL:
      return out << "FILL";
    case Justify::START:
      return out << "START";
    case Justify::END:
      return out << "END";
    case Justify::CENTER:
      return out << "CENTER";
    case Justify::SPACE_BETWEEN:
      return out << "SPACE_BETWEEN";
    case Justify::SPACE_AROUND:
      return out << "SPACE_AROUND";
    case Justify::SPACE_EVENLY:
      return out << "SPACE_EVENLY";
  }
  return out << "<<UNKNOWNTYPE>>";
}

class Widget {
 public:
  class Options {
   public:
    typedef absl::optional<absl::string_view> OptionalStableID;

    Options& set_id(absl::string_view id) {
      id_ = id;
      return *this;
    }

    Options& set_justify(Justify justify) {
      justify_ = justify;
      return *this;
    }

    Options& set_activatable(bool activatable = true) {
      activatable_ = activatable;
      return *this;
    }

    OptionalStableID id() const { return id_; }
    Justify justify() const { return justify_; }
    bool activatable() const { return activatable_; }

   private:
    OptionalStableID id_;
    Justify justify_ = Justify::FILL;
    bool activatable_ = false;
  };

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

  static const Options& DefaultOptions() {
    static const Options options{};
    return options;
  }

  Widget* MakeContent(const Options& options = DefaultOptions()) {
    return Materialize(WidgetType::CONTENT, options);
  }
  Widget* MakeRow(const Options& options = DefaultOptions()) {
    return Materialize(WidgetType::ROW, options);
  }
  Widget* MakeColumn(const Options& options = DefaultOptions()) {
    return Materialize(WidgetType::COLUMN, options);
  }
  Widget* MakeStack(const Options& options = DefaultOptions()) {
    return Materialize(WidgetType::STACK, options);
  }
  Widget* MakeFloat(const Options& options = DefaultOptions()) {
    return Materialize(WidgetType::FLOAT, options);
  }
  Widget* MakeSimpleText(CharFmt fmt, const std::vector<std::string>& text);

  typedef std::function<void(DeviceContext* device)> DrawCall;
  void Draw(DrawCall draw) { draw_ = draw; }

  bool Focus() const;
  bool InFocusTree() const { return in_focus_tree_; }
  uint32_t CharPressed();
  bool Chord(absl::string_view chord);

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

  void EnterFrame(Widget* parent, const Options& options);
  void FinalizeConstraints(Renderer* renderer);
  void UpdateAnimations(Renderer* renderer);
  bool EndFrame();

  uint64_t GenUID(const Options& options);
  Widget* Materialize(WidgetType type, const Options& options);

  void ApplyJustify(rhea::simplex_solver* solver, Justify justify,
                    const ChildVector& widgets, rhea::variable(Widget::*start),
                    rhea::variable(Widget::*end),
                    rhea::variable(Widget::*perp_start),
                    rhea::variable(Widget::*perp_end));

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
  bool in_focus_tree_ = false;
};

class Device {
 public:
  struct Extents {
    float win_height;
    float win_width;
    float chr_height;
    float chr_width;
  };

  virtual void Paint(Renderer* renderer, const Widget& widget) = 0;
  virtual Extents GetExtents() = 0;
  virtual void ClipboardPut(const std::string& s) = 0;
  virtual std::string ClipboardGet() = 0;
};

enum CaretFlags {
  CARET_PRIMARY = 1,
  CARET_BLINKING = 2,
};

class DeviceContext {
 public:
  virtual int width() const = 0;
  virtual int height() const = 0;

  virtual void PutCaret(float x, float y, unsigned flags, Color color) = 0;
  virtual void Fill(float left, float top, float right, float bottom,
                    Color color) = 0;
  struct TextElem {
    uint32_t ch;
    Color color;
    Highlight highlight;
  };
  virtual void PutText(float x, float y, const TextElem* text,
                       size_t length) = 0;
  void PutText(float x, float y, const char* text, size_t length, Color color,
               Highlight highlight) {
    std::vector<TextElem> te;
    for (size_t i = 0; i < length; i++) {
      te.emplace_back(
          TextElem{static_cast<uint32_t>(text[i]), color, highlight});
    }
    PutText(x, y, te.data(), te.size());
  }
};

class Renderer : public Widget {
 public:
  explicit Renderer(Device* device)
      : Widget(this, WidgetType::STACK, 0), device_(device) {}

  void BeginFrame();
  absl::optional<absl::Time> FinishFrame();

  rhea::simplex_solver* solver() { return solver_.get(); }
  const Device::Extents& extents() const { return extents_; }
  void ClipboardPut(const std::string& s) { device_->ClipboardPut(s); }
  std::string ClipboardGet() { return device_->ClipboardGet(); }

  void AddKbEvent(absl::string_view event) {
    if (!kbev_.empty()) kbev_ += ' ';
    kbev_.append(event.data(), event.length());
  }

  void RefreshIn(absl::Duration dt) {
    auto when = frame_time_ + dt;
    if (next_frame_ && *next_frame_ < when) return;
    next_frame_ = when;
  }

  absl::Time frame_time() const { return frame_time_; }

 private:
  friend class Widget;

  Device* const device_;
  absl::optional<absl::Time> next_frame_;
  absl::Time frame_time_;
  Device::Extents extents_;
  uint64_t focus_widget_ = 0;
  uint64_t last_focus_widget_ = 0;
  std::unordered_map<uint64_t, std::unique_ptr<Widget>> widgets_;
  std::unique_ptr<rhea::simplex_solver> solver_{new rhea::simplex_solver};

  std::string kbev_;
  bool kbev_partial_match_ = false;
};
