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
#include "theme.h"
#include <functional>
#include <unordered_map>
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "default_theme.h"
#include "log.h"
#include "plist.h"
#include "read.h"

#include <iostream>

Theme::Theme(const std::string& filename) { Load(Read(filename)); }

Theme::Theme(default_type) { Load(default_theme); }

static Highlight Merge(Highlight a, Highlight b) {
  if (a == Highlight::UNSET) return b;
  return a;
}

static int Merge(int a, int b) {
  if (a < 0) return b;
  return a;
}

template <class T>
static absl::optional<T> Merge(absl::optional<T> a, absl::optional<T> b) {
  if (!a) return b;
  return a;
}

static uint8_t blend(uint8_t a, uint8_t b, uint8_t alpha) {
  return (alpha * a + (255 - alpha) * b) / 255;
}

static absl::optional<Color> Merge(absl::optional<Color> a,
                                   absl::optional<Color> b) {
  if (!a)
    return b;
  else if (!b)
    return a;
  return Color{blend(a->r, b->r, a->a), blend(a->g, b->g, a->a),
               blend(a->b, b->b, a->a), 255};
}

CharFmt Theme::ThemeToken(Tag token, uint32_t flags) {
  auto key = std::make_pair(token, flags);
  auto it = theme_cache_.find(key);
  if (it != theme_cache_.end()) return it->second;

  Log() << "Theme: " << absl::StrJoin(token, ":") << " flags=" << flags;

  Setting composite;
  for (auto sit = settings_.crbegin(); sit != settings_.crend(); ++sit) {
    bool matches = false;
    for (const auto& sel : sit->scopes) {
      if (SelectorMatches(sel, token)) {
        matches = true;
        break;
      }
    }
    if (!matches) continue;
    composite.font_style = Merge(composite.font_style, sit->font_style);
    composite.bracket_contents_options = Merge(
        composite.bracket_contents_options, sit->bracket_contents_options);
    composite.brackets_options =
        Merge(composite.brackets_options, sit->brackets_options);
    composite.tags_options = Merge(composite.tags_options, sit->tags_options);
    composite.shadow_width = Merge(composite.shadow_width, sit->shadow_width);
    composite.foreground = Merge(composite.foreground, sit->foreground);
    composite.background = Merge(composite.background, sit->background);
    composite.invisibles = Merge(composite.invisibles, sit->invisibles);
    composite.caret = Merge(composite.caret, sit->caret);
    composite.line_highlight =
        Merge(composite.line_highlight, sit->line_highlight);
    composite.bracket_contents_foreground =
        Merge(composite.bracket_contents_foreground,
              sit->bracket_contents_foreground);
    composite.brackets_foreground =
        Merge(composite.brackets_foreground, sit->brackets_foreground);
    composite.tags_foreground =
        Merge(composite.tags_foreground, sit->tags_foreground);
    composite.find_highlight =
        Merge(composite.find_highlight, sit->find_highlight);
    composite.find_highlight_foreground = Merge(
        composite.find_highlight_foreground, sit->find_highlight_foreground);
    composite.gutter = Merge(composite.gutter, sit->gutter);
    composite.gutter_foreground =
        Merge(composite.gutter_foreground, sit->gutter_foreground);
    composite.selection = Merge(composite.selection, sit->selection);
    composite.selection_border =
        Merge(composite.selection_border, sit->selection_border);
    composite.inactive_selection =
        Merge(composite.inactive_selection, sit->inactive_selection);
    composite.guide = Merge(composite.guide, sit->guide);
    composite.active_guide = Merge(composite.active_guide, sit->active_guide);
    composite.stack_guide = Merge(composite.stack_guide, sit->stack_guide);
    composite.highlight = Merge(composite.highlight, sit->highlight);
    composite.highlight_foreground =
        Merge(composite.highlight_foreground, sit->highlight_foreground);
    composite.shadow = Merge(composite.shadow, sit->shadow);
  }

  OptColor foreground = composite.foreground;
  OptColor background = composite.background;
  Highlight highlight = composite.font_style;

  if (flags & HIGHLIGHT_LINE) {
    background = Merge(composite.line_highlight, background);
  }
  if (flags & SELECTED) {
    background = Merge(composite.selection, background);
  }
  if (flags & CARET) {
    foreground = Merge(composite.caret, foreground);
  }

  CharFmt result{foreground ? *foreground : Color{255, 255, 255, 255},
                 background ? *background : Color{0, 0, 0, 255},
                 highlight != Highlight::UNSET ? highlight : Highlight::NONE};
  theme_cache_.insert(std::make_pair(key, result));
  return result;
}

static std::string GetName(const plist::Dict* d) {
  auto* name_node = d->Get("name");
  if (!name_node) return "<<unnamed>>";
  auto* str = name_node->AsString();
  if (!str) return "<<name_not_string>>";
  return *str;
}

std::vector<Theme::Selector> Theme::ParseScopes(const plist::Dict* d) {
  auto scopes_node = d->Get("scope");
  if (!scopes_node) return {Selector()};
  auto* str = scopes_node->AsString();
  if (!str) throw std::runtime_error("scope not a string");
  std::vector<Selector> r;
  for (auto alt : absl::StrSplit(*str, ',')) {
    Selector s;
    for (auto sel_row : absl::StrSplit(alt, ' ')) {
      if (sel_row.empty()) continue;
      s.emplace_back(sel_row.data(), sel_row.length());
    }
    r.emplace_back(std::move(s));
  }
  return r;
}

void Theme::Load(const std::string& src) {
  static const std::unordered_map<
      std::string, std::function<void(const std::string&, Setting*)>>
      load_setting = {
          {"phantomCss", LoadIgnored},
          {"shadowWidth", LoadInt<&Setting::shadow_width>()},
          {"fontStyle", LoadHighlight<&Setting::font_style>()},
          {"bracketContentsOptions",
           LoadHighlight<&Setting::bracket_contents_options>()},
          {"bracketsOptions", LoadHighlight<&Setting::brackets_options>()},
          {"tagsOptions", LoadHighlight<&Setting::tags_options>()},
          {"foreground", LoadColor<&Setting::foreground>()},
          {"background", LoadColor<&Setting::background>()},
          {"invisibles", LoadColor<&Setting::invisibles>()},
          {"caret", LoadColor<&Setting::caret>()},
          {"lineHighlight", LoadColor<&Setting::line_highlight>()},
          {"bracketContentsForeground",
           LoadColor<&Setting::bracket_contents_foreground>()},
          {"bracketsForeground", LoadColor<&Setting::brackets_foreground>()},
          {"tagsForeground", LoadColor<&Setting::tags_foreground>()},
          {"findHighlight", LoadColor<&Setting::find_highlight>()},
          {"findHighlightForeground",
           LoadColor<&Setting::find_highlight_foreground>()},
          {"gutter", LoadColor<&Setting::gutter>()},
          {"gutterForeground", LoadColor<&Setting::gutter_foreground>()},
          {"selection", LoadColor<&Setting::selection>()},
          {"selectionBorder", LoadColor<&Setting::selection_border>()},
          {"inactiveSelection", LoadColor<&Setting::inactive_selection>()},
          {"guide", LoadColor<&Setting::guide>()},
          {"activeGuide", LoadColor<&Setting::active_guide>()},
          {"stackGuide", LoadColor<&Setting::stack_guide>()},
          {"highlight", LoadColor<&Setting::highlight>()},
          {"highlightForeground", LoadColor<&Setting::highlight_foreground>()},
          {"shadow", LoadColor<&Setting::shadow>()},
      };

  auto root_node = plist::Parse(src);
  if (!root_node) throw std::runtime_error("Failed to parse theme");
  auto root = root_node->AsDict();
  if (!root) throw std::runtime_error("Root not a dict");
  auto settings_node = root->Get("settings");
  if (!settings_node) throw std::runtime_error("No settings node");
  auto settings = settings_node->AsArray();
  if (!settings) throw std::runtime_error("Settings not an array");
  for (const auto& top : *settings) {
    Setting s;
    auto top_dict = top->AsDict();
    if (!top_dict) throw std::runtime_error("Setting not a dict");
    auto name = GetName(top_dict);
    try {
      s.scopes = ParseScopes(top_dict);
      auto setting = top_dict->Get("settings");
      if (!setting) throw std::runtime_error("No settings");
      auto setting_dict = setting->AsDict();
      if (!setting_dict) throw std::runtime_error("Setting not a dict");
      for (const auto& kv : *setting_dict) {
        try {
          auto load_it = load_setting.find(kv.first);
          if (load_it == load_setting.end()) {
            throw std::runtime_error("Dont know how to load setting");
          }
          auto value = kv.second->AsString();
          if (!value) throw std::runtime_error("Value is not a string");
          load_it->second(*value, &s);
        } catch (std::exception& e) {
          throw std::runtime_error("Parsing '" + kv.first + "': " + e.what());
        }
      }
    } catch (std::exception& e) {
      throw std::runtime_error("Parsing " + name + ": " + e.what());
    }
    settings_.push_back(s);
  }
}
