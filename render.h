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

#include <city.h>
#include <functional>
#include <ostream>
#include <unordered_map>
#include <vector>
#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

class Renderer;

enum class WidgetType {
  TEXT,
  BUTTON,
  CONTAINER,
};

class Widget {
 public:
  template <class T>
  T as() {
    T r;
    GetValue(&r);
    return r;
  }

  // set label to label
  Widget& Labelled(absl::string_view& label) {
    new_->label = std::string(label.data(), label.length());
    return *this;
  }
  // reparent to widget
  Widget& ContainedBy(Widget& widget) {
    new_->parent = &widget;
    return *this;
  }
  // float above all widgets
  Widget& Floats() {
    new_->parent = nullptr;
    return *this;
  }

  WidgetType type() const { return type_; }
  Widget(WidgetType type) : type_(type) {}

 private:
  friend class Renderer;

  void EnterFrame(Renderer* renderer);
  bool EndFrame();

  struct State {
    std::string label;
    Widget* parent = nullptr;
  };

  const WidgetType type_;
  absl::optional<State> new_;
  absl::optional<State> prev_;
};

class Renderer {
 public:
  typedef absl::optional<absl::string_view> OptionalStableID;

  Widget& Text(OptionalStableID id = OptionalStableID()) {
    return Materialize(WidgetType::TEXT, id, nullptr);
  }
  Widget& Button(OptionalStableID id = OptionalStableID()) {
    return Materialize(WidgetType::BUTTON, id, nullptr);
  }

  // Use class Container
  Widget& BeginContainer(OptionalStableID id = OptionalStableID()) {
    uint64_t uid;
    Widget& w = Materialize(WidgetType::CONTAINER, id, &uid);
    stack_.push_back({&w, uid * 257});
    return w;
  }
  void EndContainer(Widget& w) {
    assert(stack_.back().container == &w);
    stack_.pop_back();
  }

  void FinishFrame();

 private:
  friend class Widget;

  typedef uint64_t WidgetID;

  struct Scope {
    Widget* container;
    uint64_t nextid;
  };
  absl::InlinedVector<Scope, 4> stack_ = {{nullptr, 1}};

  WidgetID GenUID(OptionalStableID id);
  Widget& Materialize(WidgetType type, OptionalStableID id, uint64_t* uid);

  std::unordered_map<WidgetID, Widget> widgets_;
};

class Container {
 public:
  explicit Container(Renderer& renderer, Renderer::OptionalStableID id =
                                             Renderer::OptionalStableID())
      : renderer_(renderer), widget_(renderer.BeginContainer(id)) {}
  ~Container() { renderer_.EndContainer(widget_); }

  Widget& widget() { return widget_; }
  const Widget& widget() const { return widget_; }

 private:
  Renderer& renderer_;
  Widget& widget_;
};
