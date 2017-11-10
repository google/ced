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
#include "plist.h"
#include <string.h>
#include "src/pugixml.hpp"
namespace plist {

namespace {

NodePtr ParseNode(pugi::xml_node n);

NodePtr ParseDict(pugi::xml_node n) {
  enum State { KEY, VALUE };
  State state = KEY;
  std::unique_ptr<Dict> d(new Dict());
  std::string key;
  NodePtr node;
  for (auto c : n.children()) {
    switch (state) {
      case KEY:
        if (strcmp(c.name(), "key") != 0) return nullptr;
        key = c.child_value();
        state = VALUE;
        break;
      case VALUE:
        node = ParseNode(c);
        if (!node) return nullptr;
        d->AddNode(key, std::move(node));
        state = KEY;
        break;
    }
  }
  return NodePtr(d.release());
}

std::unique_ptr<Array> ParseArrayAsArray(pugi::xml_node n) {
  std::unique_ptr<Array> a(new Array());
  for (auto c : n.children()) {
    NodePtr node = ParseNode(c);
    if (!node) return nullptr;
    a->AddNode(std::move(node));
  }
  return a;
}

NodePtr ParseArray(pugi::xml_node n) {
  return NodePtr(ParseArrayAsArray(n).release());
}

NodePtr ParseString(pugi::xml_node n) {
  return NodePtr(new String(n.child_value()));
}

NodePtr ParseNode(pugi::xml_node n) {
  if (0 == strcmp(n.name(), "dict")) {
    return ParseDict(n);
  } else if (0 == strcmp(n.name(), "array")) {
    return ParseArray(n);
  } else if (0 == strcmp(n.name(), "string")) {
    return ParseString(n);
  } else {
    return nullptr;
  }
}

}  // namespace

NodePtr Parse(const std::string& src) {
  pugi::xml_document doc;
  auto parse_result = doc.load_buffer(
      src.data(), src.length(), pugi::parse_default | pugi::parse_trim_pcdata);

  if (!parse_result) {
    return nullptr;
  }

  auto top = ParseArrayAsArray(doc.child("plist"));
  if (top->mutable_nodes()->size() != 1) return nullptr;
  return std::move(top->mutable_nodes()->at(0));
}

}  // namespace plist
