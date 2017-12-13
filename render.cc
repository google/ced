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

void Widget::EnterFrame(Renderer* renderer) {
  assert(!new_);
  new_.emplace();
  const Renderer::Scope& scope = renderer->stack_.back();
  new_->parent = scope.container;
}

bool Widget::EndFrame() {
  prev_.swap(new_);
  new_.reset();
  return true;
}

uint64_t Renderer::GenUID(OptionalStableID id) {
  if (!id) {
    return stack_.back().nextid++;
  } else {
    if (stack_.size() >= 2) {
      return CityHash64WithSeed(id->data(), id->length(),
                                stack_[stack_.size() - 2].nextid);
    } else {
      return CityHash64(id->data(), id->length());
    }
  }
}

Widget& Renderer::Materialize(WidgetType type, OptionalStableID id,
                              uint64_t* puid) {
  auto uid = GenUID(id);
  if (puid) *puid = uid;
  auto it = widgets_.find(uid);
  Widget* w;
  if (it != widgets_.end()) {
    assert(it->second.type() == type);
    w = &it->second;
  } else {
    w = &widgets_.emplace(uid, type).first->second;
  }
  w->EnterFrame(this);
  return *w;
}

void Renderer::FinishFrame() {
  // collect all widgets
  std::vector<Widget*> widgets;
  widgets.reserve(widgets_.size());
  for (auto& w : widgets_) {
    widgets.push_back(&w.second);
  }

  // signal frame termination
  for (auto it = widgets_.begin(); it != widgets_.end();) {
    if (it->second.EndFrame()) {
      ++it;
    } else {
      it = widgets_.erase(it);
    }
  }
}
