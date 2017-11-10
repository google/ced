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
#include "absl/strings/str_split.h"
#include "default_theme.h"
#include "plist.h"
#include "read.h"

#include <iostream>

Theme::Theme(const std::string& filename) { Load(Read(filename)); }

Theme::Theme(default_type) { Load(default_theme); }

static std::string GetName(const plist::Dict* d) {
  auto* name_node = d->Get("name");
  if (!name_node) return "<<unnamed>>";
  auto* str = name_node->AsString();
  if (!str) return "<<name_not_string>>";
  return *str;
}

static std::vector<Selector> ParseScopes(const plist::Dict* d) {
  auto scopes_node = d->Get("scope");
  if (!scopes_node) return {};
  auto* str = scopes_node->AsString();
  if (!str) throw std::runtime_error("scope not a string");
  std::vector<Selector> r;
  for (auto alt : absl::StrSplit(*str, ',')) {
    Selector s;
    for (auto sel_row : absl::StrSplit(alt, ' ')) {
      if (sel_row.empty()) continue;
      s = s.Push(std::string(sel_row.data(), sel_row.length()));
    }
    r.push_back(s);
  }
  return r;
}

void Theme::Load(const std::string& src) {
  static const std::unordered_map<
      std::string, std::function<void(const std::string&, Setting*)>>
      load_setting = {
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
