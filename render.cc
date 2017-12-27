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
#include "render.h"
#include <city.h>
#include "log.h"

Widget* Widget::MakeSimpleText(CharFmt fmt,
                               const std::vector<std::string>& text) {
  auto c = MakeContent();
  int lines = 0;
  int cols = 0;
  for (auto& l : text) {
    lines++;
    cols = std::max(cols, (int)l.length());
  }
  int cw = renderer_->extents().chr_width;
  int ch = renderer_->extents().chr_height;
  renderer_->solver()->add_constraints(
      {c->width() >= cols * cw, c->height() >= lines * ch});
  c->Draw([=](DeviceContext* ctx) {
    int line = 0;
    for (auto& l : text) {
      for (int c = 0; c < l.length(); c++) {
        ctx->PutChar(line * ch, c * cw, l[c], fmt);
      }
    }
  });
  return c;
}

void Widget::EnterFrame(Widget* parent) {
  parent_ = parent;
  draw_ = DrawCall();
  auto* solver = renderer_->solver_.get();
  solver->add_constraints({bottom_ >= top_, right_ >= left_});
  nextid_ = 257 * id_ + 1;
  if (parent_) {
    parent_->children_.push_back(this);
  }
}

void Widget::ApplyJustify(rhea::simplex_solver* solver, Justify justify,
                          const ChildVector& widgets,
                          rhea::variable(Widget::*start),
                          rhea::variable(Widget::*end)) {
  if (widgets.empty()) return;
  switch (justify) {
    case Justify::FILL:
      for (size_t i = 1; i < widgets.size(); i++) {
        solver->add_constraints(
            {(widgets[i]->*start) == (widgets[i - 1]->*end)});
      }
      solver->add_constraints(
          {(widgets[0]->*start) == 0,
           (widgets.back()->*end) == (this->*end) - (this->*start)});
      break;
    case Justify::START:
      for (size_t i = 1; i < widgets.size(); i++) {
        solver->add_constraints(
            {(widgets[i]->*start) == (widgets[i - 1]->*end)});
      }
      solver->add_constraints(
          {(widgets[0]->*start) == 0, (widgets.back()->*end) + container_pad_ ==
                                          (this->*end) - (this->*start)});
      break;
    case Justify::END:
      for (size_t i = 1; i < widgets.size(); i++) {
        solver->add_constraints(
            {(widgets[i]->*start) == (widgets[i - 1]->*end)});
      }
      solver->add_constraints(
          {(widgets[0]->*start) == container_pad_,
           (widgets.back()->*end) == (this->*end) - (this->*start)});
      break;
    case Justify::CENTER:
      for (size_t i = 1; i < widgets.size(); i++) {
        solver->add_constraints(
            {(widgets[i]->*start) == (widgets[i - 1]->*end)});
      }
      solver->add_constraints({(widgets[0]->*start) == container_pad_,
                               (widgets.back()->*end) + container_pad_ ==
                                   (this->*end) - (this->*start)});
      break;
    case Justify::SPACE_BETWEEN:
      for (size_t i = 1; i < widgets.size(); i++) {
        solver->add_constraints(
            {(widgets[i]->*start) == (widgets[i - 1]->*end) + container_pad_});
      }
      solver->add_constraints(
          {(widgets[0]->*start) == 0,
           (widgets.back()->*end) == (this->*end) - (this->*start)});
      break;
    case Justify::SPACE_AROUND:
      for (size_t i = 1; i < widgets.size(); i++) {
        solver->add_constraints({(widgets[i]->*start) ==
                                 (widgets[i - 1]->*end) + 2 * container_pad_});
      }
      solver->add_constraints({(widgets[0]->*start) == container_pad_,
                               (widgets.back()->*end) + container_pad_ ==
                                   (this->*end) - (this->*start)});
      break;
    case Justify::SPACE_EVENLY:
      for (size_t i = 1; i < widgets.size(); i++) {
        solver->add_constraints(
            {(widgets[i]->*start) == (widgets[i - 1]->*end) + container_pad_});
      }
      solver->add_constraints({(widgets[0]->*start) == container_pad_,
                               (widgets.back()->*end) + container_pad_ ==
                                   (this->*end) - (this->*start)});
      break;
  }
}

void Widget::FinalizeConstraints(Renderer* renderer) {
  auto* solver = renderer->solver_.get();
  switch (type_) {
    case WidgetType::CONTENT:
    case WidgetType::FLOAT:
      break;
    case WidgetType::ROW:
      for (size_t i = 0; i < children_.size(); i++) {
        solver->add_constraints(
            {children_[i]->bottom() - children_[i]->top() == bottom() - top()});
      }
      ApplyJustify(solver, justify_, children_, &Widget::left_,
                   &Widget::right_);
      break;
    case WidgetType::COLUMN:
      for (size_t i = 0; i < children_.size(); i++) {
        solver->add_constraints(
            {children_[i]->right() - children_[i]->left() == right() - left()});
      }
      ApplyJustify(solver, justify_, children_, &Widget::top_,
                   &Widget::bottom_);
      break;
    case WidgetType::STACK:
      for (size_t i = 0; i < children_.size(); i++) {
        solver->add_constraints(
            {children_[i]->right() - children_[i]->left() == right() - left(),
             children_[i]->bottom() - children_[i]->top() == bottom() - top()});
      }
      break;
  }
}

bool Widget::EndFrame() {
  children_.clear();
  return true;
}

uint64_t Widget::GenUID(OptionalStableID id) {
  if (!id) {
    return nextid_++;
  } else {
    return CityHash64WithSeed(id->data(), id->length(), id_);
  }
}

Widget* Widget::Materialize(WidgetType type, OptionalStableID id) {
  auto uid = GenUID(id);
  auto it = renderer_->widgets_.find(uid);
  Widget* w;
  if (it != renderer_->widgets_.end()) {
    assert(it->second->type() == type);
    w = it->second.get();
  } else {
    w = renderer_->widgets_
            .emplace(uid,
                     std::unique_ptr<Widget>(new Widget(renderer_, type, uid)))
            .first->second.get();
  }
  w->EnterFrame(this);
  return w;
}

double Widget::Animator::ScaleTime(absl::Time time) const {
  absl::Duration so_far = time - target_set;
  double r = absl::FDivDuration(so_far, blend_time);
  if (r < 0.0) return 0.0;
  if (r > 1.0) return 1.0;
  return r;
}

void Widget::Animator::DeclTarget(absl::Time time, double new_target) {
  if (fabs(new_target - target) < 0.5) return;
  if (!initialized) {
    initial_value = target = new_target;
    initial_velocity = 0;
    initialized = true;
    return;
  }
  const auto c = CalcCoefficients();
  const double t = ScaleTime(time);
  initial_value = c.a * t * t * t + c.b * t * t + c.c * t + c.d;
  initial_velocity = 3 * c.a * t * t + 2 * c.b * t + c.c;
  target_set = time;
  target = new_target;
}

double Widget::Animator::ValueAt(absl::Time time, Renderer* renderer) const {
  const double t = ScaleTime(time);
  if (t < 1.0) renderer->animating_ = true;
  const auto c = CalcCoefficients();
  return c.a * t * t * t + c.b * t * t + c.c * t + c.d;
}

Widget::Animator::Coefficients Widget::Animator::CalcCoefficients() const {
  // f(0) = x0
  // f'(0) = v0
  // f(1) = x1
  // f'(1) = 0
  // f(p)  = a*p^3 + b*p^2 + c*p + d
  // f'(p) = 3*a*p^2 + 2*b*p + c
  // so  f'(0) = c = v0
  // and f(0) = d = x0
  // so  f'(1) = 3*a + 2*b + v0 = 0
  //     => 2*b = -3*a - v0
  //     => b = (-3*a - v0) / 2
  // and f(1) = a + b + c + d = x1
  //     => a + (-3*a - v0) / 2 + v0 + x0 = x1
  //     => -a/2 + v0/2 + x0 = x1
  //     => a/2 = v0/2 + x0 - x1
  //     => a = v0 - 2*(x1 - x0)
  // so b = (-3*(v0 - 2*(x1-x0)) - v0) / 2
  //      = (-3*v0 + 6*(x1-x0) - v0) / 2
  //      = (-4*v0 + 6*(x1-x0)) / 2
  //      = 3*(x1-x0) - 2*v0
  return {initial_velocity - 2 * (target - initial_value),
          (3 * (target - initial_value) - 2 * initial_velocity),
          initial_velocity, initial_value};
}

void Widget::UpdateAnimations(Renderer* renderer) {
  double l = left_.value();
  double r = right_.value();
  double t = top_.value();
  double b = bottom_.value();
  ani_midx_.DeclTarget(renderer->frame_time_, (l + r) / 2.0);
  ani_midy_.DeclTarget(renderer->frame_time_, (t + b) / 2.0);
  ani_szx_.DeclTarget(renderer->frame_time_, r - l);
  ani_szy_.DeclTarget(renderer->frame_time_, b - t);
}

void Renderer::BeginFrame() {
  solver_.reset(new rhea::simplex_solver);
  solver_->set_autosolve(false);
  frame_time_ = absl::Now();
  animating_ = false;
  extents_ = device_->GetExtents();
  Log() << "extents: sw=" << extents_.win_width
        << " sh=" << extents_.win_height;
  solver_->add_constraints({
      right_ == left_ + extents_.win_width,
      bottom_ == top_ + extents_.win_height,
  });
}

absl::optional<absl::Time> Renderer::FinishFrame() {
  // collect all widgets
  std::vector<Widget*> widgets;
  widgets.reserve(widgets_.size());
  for (auto& w : widgets_) {
    widgets.push_back(w.second.get());
  }

  // finalize solver constraints
  this->FinalizeConstraints(this);
  for (auto w : widgets) {
    w->FinalizeConstraints(this);
  }

  solver_->solve();

  // update animations
  for (auto w : widgets) {
    w->UpdateAnimations(this);
  }

  // paint
  for (auto w : children()) {
    device_->Paint(this, *w);
  }

  // signal frame termination
  for (auto it = widgets_.begin(); it != widgets_.end();) {
    if (it->second->EndFrame()) {
      ++it;
    } else {
      it = widgets_.erase(it);
    }
  }

  solver_.reset();

  if (animating_) {
    return frame_time_ + absl::Milliseconds(16);
  } else {
    return absl::optional<absl::Time>();
  }
}
