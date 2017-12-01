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
#include "annotated_string.h"
#include "log.h"

std::atomic<uint16_t> Site::id_gen_{1};

AnnotatedString::AnnotatedString() {
  chars_ = chars_
               .Add(Begin(),
                    CharInfo{false, 0, End(), End(), End(), End(), AVL<ID>()})
               .Add(End(), CharInfo{false, 1, Begin(), Begin(), Begin(),
                                    Begin(), AVL<ID>()});
  line_breaks_ = line_breaks_.Add(Begin(), LineBreak{End(), End()})
                     .Add(End(), LineBreak{Begin(), Begin()});
}

ID AnnotatedString::MakeRawInsert(CommandSet* commands, Site* site,
                                  absl::string_view chars, ID after,
                                  ID before) {
  auto ids = site->GenerateIDBlock(chars.length());
  auto cmd = commands->add_commands();
  cmd->set_id(ids.first.id);
  auto ins = cmd->mutable_insert();
  ins->set_after(after.id);
  ins->set_before(before.id);
  ins->set_characters(chars.data(), chars.length());
  return ids.second;
}

void AnnotatedString::MakeDelete(CommandSet* commands, ID id) {
  auto cmd = commands->add_commands();
  cmd->set_id(id.id);
  cmd->mutable_delete_();
}

void AnnotatedString::MakeDelMark(CommandSet* commands, ID id) {
  auto cmd = commands->add_commands();
  cmd->set_id(id.id);
  cmd->mutable_del_mark();
}

void AnnotatedString::MakeDelDecl(CommandSet* commands, ID id) {
  auto cmd = commands->add_commands();
  cmd->set_id(id.id);
  cmd->mutable_del_decl();
}

ID AnnotatedString::MakeDecl(CommandSet* commands, Site* site,
                             const Attribute& attribute) {
  auto cmd = commands->add_commands();
  ID id = site->GenerateID();
  cmd->set_id(id.id);
  *cmd->mutable_decl() = attribute;
  return id;
}

ID AnnotatedString::MakeMark(CommandSet* commands, Site* site,
                             const Annotation& annotation) {
  auto cmd = commands->add_commands();
  ID id = site->GenerateID();
  cmd->set_id(id.id);
  *cmd->mutable_mark() = annotation;
  return id;
}

void AnnotatedString::MakeDeleteAttributesBySite(CommandSet* commands,
                                                 const Site& site) {
  annotations_by_type_.ForEach(
      [&](Attribute::DataCase dc, AVL<ID, Annotation> by_type) {
        by_type.ForEach([&](ID id, const Annotation& an) {
          if (site.CreatedID(id) || site.CreatedID(an.attribute())) {
            MakeDelMark(commands, id);
          }
        });
      });
  attributes_.ForEach([&](ID id, Attribute::DataCase dc) {
    if (site.CreatedID(id)) {
      MakeDelDecl(commands, id);
    }
  });
}

AnnotatedString AnnotatedString::Integrate(const CommandSet& commands) const {
  AnnotatedString s = *this;
  for (const auto& cmd : commands.commands()) {
    s.Integrate(cmd);
  }
  return s;
}

void AnnotatedString::Integrate(const Command& cmd) {
  // Log() << "INTEGRATE: " << cmd.DebugString();
  switch (cmd.command_case()) {
    case Command::kInsert:
      IntegrateInsert(cmd.id(), cmd.insert());
      break;
    case Command::kDelete:
      IntegrateDelChar(cmd.id());
      break;
    case Command::kDecl:
      IntegrateDecl(cmd.id(), cmd.decl());
      break;
    case Command::kDelDecl:
      IntegrateDelDecl(cmd.id());
      break;
    case Command::kMark:
      IntegrateMark(cmd.id(), cmd.mark());
      break;
    case Command::kDelMark:
      IntegrateDelMark(cmd.id());
      break;
    default:
      throw std::runtime_error("String integration failed");
  }
}

void AnnotatedString::IntegrateInsert(ID id, const InsertCommand& cmd) {
  if (chars_.Lookup(id)) return;
  ID after = cmd.after();
  ID before = cmd.before();
  for (auto c : cmd.characters()) {
    IntegrateInsertChar(id, c, after, before);
    after = id;
    id.clock++;
  }
}

void AnnotatedString::IntegrateInsertChar(ID id, char c, ID after, ID before) {
  for (;;) {
    const CharInfo* caft = chars_.Lookup(after);
    const CharInfo* cbef = chars_.Lookup(before);
    assert(caft != nullptr);
    assert(cbef != nullptr);
    if (caft->next == before) {
      if (c == '\n') {
        auto prev_line_id = after;
        const CharInfo* plic = caft;
        while (prev_line_id != Begin() &&
               (!plic->visible || plic->chr != '\n')) {
          prev_line_id = plic->prev;
          plic = chars_.Lookup(prev_line_id);
        }
        auto prev_lb = line_breaks_.Lookup(prev_line_id);
        auto next_lb = line_breaks_.Lookup(prev_lb->next);
        line_breaks_ =
            line_breaks_.Add(prev_line_id, LineBreak{prev_lb->prev, id})
                .Add(id, LineBreak{prev_line_id, prev_lb->next})
                .Add(prev_lb->next, LineBreak{id, next_lb->next});
      }
      // Log() << "Woot " << after.id << " " << id.id << " " << before.id << "
      // '"
      //      << c << "'";
      chars_ = chars_
                   .Add(after,
                        CharInfo{caft->visible, caft->chr, id, caft->prev,
                                 caft->after, caft->before, caft->annotations})
                   .Add(id, CharInfo{true, c, before, after, after, before,
                                     AVL<ID>()})
                   .Add(before,
                        CharInfo{cbef->visible, cbef->chr, cbef->next, id,
                                 cbef->after, cbef->before, cbef->annotations});
      return;
    }
    typedef std::map<ID, const CharInfo*> LMap;
    LMap inL;
    std::vector<typename LMap::iterator> L;
    auto addToL = [&](ID id, const CharInfo* ci) {
      L.push_back(inL.emplace(id, ci).first);
    };
    addToL(after, caft);
    ID n = caft->next;
    do {
      const CharInfo* cn = chars_.Lookup(n);
      assert(cn != nullptr);
      addToL(n, cn);
      n = cn->next;
    } while (n != before);
    addToL(before, cbef);
    size_t i, j;
    for (i = 1, j = 1; i < L.size() - 1; i++) {
      auto it = L[i];
      auto ai = inL.find(it->second->after);
      if (ai == inL.end()) continue;
      auto bi = inL.find(it->second->before);
      if (bi == inL.end()) continue;
      L[j++] = L[i];
    }
    L[j++] = L[i];
    L.resize(j);
    for (i = 1; i < L.size() - 1 && L[i]->first < id; i++)
      ;
    // loop with new bounds
    after = L[i - 1]->first;
    before = L[i]->first;
  }
}

void AnnotatedString::IntegrateDelChar(ID id) {
  const CharInfo* cdel = chars_.Lookup(id);
  if (!cdel->visible) return;
  if (cdel->chr == '\n') {
    auto* self = line_breaks_.Lookup(id);
    auto* prev = line_breaks_.Lookup(self->prev);
    auto* next = line_breaks_.Lookup(self->next);
    line_breaks_ = line_breaks_.Remove(id)
                       .Add(self->prev, LineBreak{prev->prev, self->next})
                       .Add(self->next, LineBreak{self->prev, next->next});
  }
  Log() << "Del char " << id.id;
  chars_ = chars_.Add(id, CharInfo{false, cdel->chr, cdel->next, cdel->prev,
                                   cdel->after, cdel->before, AVL<ID>()});
}

void AnnotatedString::IntegrateDecl(ID id, const Attribute& decl) {
  if (graveyard_.Lookup(id)) return;
  attributes_ = attributes_.Add(id, decl.data_case());
  const auto* tattr = attributes_by_type_.Lookup(decl.data_case());
  attributes_by_type_ = attributes_by_type_.Add(
      decl.data_case(), (tattr ? *tattr : AVL<ID, Attribute>()).Add(id, decl));
}

void AnnotatedString::IntegrateDelDecl(ID id) {
  const auto* dc = attributes_.Lookup(id);
  if (!dc) return;
  attributes_by_type_ =
      attributes_by_type_.Add(*dc, attributes_by_type_.Lookup(*dc)->Remove(id));
  attributes_ = attributes_.Remove(id);
  graveyard_ = graveyard_.Add(id);
}

void AnnotatedString::IntegrateMark(ID id, const Annotation& annotation) {
  if (graveyard_.Lookup(id)) return;
  // Log() << "INTEGRATE_MARK: " << annotation.DebugString() << " into " <<
  // AsProto().DebugString();
  const auto* dc = attributes_.Lookup(annotation.attribute());
  assert(dc);
  annotations_ = annotations_.Add(id, *dc);
  const auto* tann = annotations_by_type_.Lookup(*dc);
  annotations_by_type_ = annotations_by_type_.Add(
      *dc, (tann ? *tann : AVL<ID, Annotation>()).Add(id, annotation));
  ID loc = annotation.begin();
  while (loc != annotation.end()) {
    const CharInfo* ci = chars_.Lookup(loc);
    // Log() << "Mark " << loc.id << " with " << id.id << " vis:" <<
    // ci->visible;
    assert(ci);
    auto next = ci->next;
    // Log() << "loc=" << loc.id << " next=" << next.id;
    assert(next != loc);
    if (IsMarkable(loc, ci)) {
      chars_ = chars_.Add(
          loc, CharInfo{ci->visible, ci->chr, ci->next, ci->prev, ci->after,
                        ci->before, ci->annotations.Add(id)});
    }
    loc = next;
  }
  // Log() << "GOT: " << AsProto().DebugString();
}

void AnnotatedString::IntegrateDelMark(ID id) {
  const auto* dc = annotations_.Lookup(id);
  if (!dc) return;
  const auto* bt = annotations_by_type_.Lookup(*dc);
  const auto* ann = bt->Lookup(id);
  ID loc = ann->begin();
  while (loc != ann->end()) {
    const CharInfo* ci = chars_.Lookup(loc);
    assert(ci);
    // Log() << "Unmark " << loc.id << " with " << id.id << " vis:" <<
    // ci->visible;
    auto next = ci->next;
    if (IsMarkable(loc, ci)) {
      chars_ = chars_.Add(
          loc, CharInfo{ci->visible, ci->chr, ci->next, ci->prev, ci->after,
                        ci->before, ci->annotations.Remove(id)});
    }
    loc = next;
  }
  annotations_by_type_ = annotations_by_type_.Add(*dc, bt->Remove(id));
  annotations_ = annotations_.Remove(id);
  graveyard_ = graveyard_.Add(id);
}

std::string AnnotatedString::Render(ID beg, ID end) const {
  MakeOrderedIDs(&beg, &end);
  std::string r;
  ID loc = beg;
  while (loc != end) {
    const CharInfo* ci = chars_.Lookup(loc);
    if (ci->visible) {
      r += ci->chr;
    }
    loc = ci->next;
  }
  return r;
}

AnnotatedStringMsg AnnotatedString::AsProto() const {
  AnnotatedStringMsg out;
  chars_.ForEach([&](ID id, const CharInfo& ci) {
    auto c = out.add_chars();
    c->set_id(id.id);
    c->set_visible(ci.visible);
    c->set_chr(ci.chr);
    c->set_next(ci.next.id);
    c->set_prev(ci.prev.id);
    c->set_after(ci.after.id);
    c->set_before(ci.before.id);
  });
  attributes_by_type_.ForEach(
      [&](Attribute::DataCase, AVL<ID, Attribute> attrs) {
        attrs.ForEach([&](ID id, const Attribute& attr) {
          auto a = out.add_attributes();
          a->set_id(id.id);
          *a->mutable_attr() = attr;
        });
      });
  annotations_by_type_.ForEach(
      [&](Attribute::DataCase, AVL<ID, Annotation> attrs) {
        attrs.ForEach([&](ID id, const Annotation& anno) {
          auto a = out.add_annotations();
          a->set_id(id.id);
          *a->mutable_anno() = anno;
        });
      });
  graveyard_.ForEach([&](ID id) { out.add_graveyard(id.id); });
  return out;
}

AnnotatedString AnnotatedString::FromProto(const AnnotatedStringMsg& msg) {
  AnnotatedString out;
  for (const auto& chr : msg.chars()) {
    out.chars_ = out.chars_.Add(
        chr.id(), {chr.visible(), static_cast<char>(chr.chr()), chr.next(),
                   chr.prev(), chr.after(), chr.before(), AVL<ID>()});
  }
  //  line_breaks_ = line_breaks_.Add(Begin(), LineBreak{End(), End()})
  //                     .Add(End(), LineBreak{Begin(), Begin()});
  Iterator it(out, Begin());
  it.MoveNext();
  ID last_id = Begin();
  while (!it.is_end()) {
    if (it.value() == '\n') {
      auto last_lb = *out.line_breaks_.Lookup(last_id);
      auto next_lb = *out.line_breaks_.Lookup(last_lb.next);
      assert(next_lb.prev == last_id);
      out.line_breaks_ =
          out.line_breaks_.Add(last_id, LineBreak{last_lb.prev, it.id()})
              .Add(it.id(), LineBreak{last_id, last_lb.next})
              .Add(last_lb.next, LineBreak{it.id(), next_lb.next});
    }
    it.MoveNext();
  }
  for (const auto& attr : msg.attributes()) {
    out.IntegrateDecl(attr.id(), attr.attr());
  }
  for (const auto& anno : msg.annotations()) {
    out.IntegrateMark(anno.id(), anno.anno());
  }
  for (auto id : msg.graveyard()) {
    out.graveyard_ = out.graveyard_.Add(id);
  }
  return out;
}

int AnnotatedString::OrderIDs(ID a, ID b) const {
  // same id
  if (a == b) return 0;
  if (a == Begin()) return -1;
  if (a == End()) return 1;
  if (b == Begin()) return 1;
  if (b == End()) return -1;
  LineIterator line_a(*this, a);
  LineIterator line_b(*this, b);
  if (line_a.id() == line_b.id()) {
    // same line
    AllIterator it(*this, line_a.id());
    for (;;) {
      if (it.id() == a) return -1;
      if (it.id() == b) return 1;
      it.MoveNext();
    }
  }
  LineIterator bk = line_a;
  LineIterator fw = line_a;
  for (;;) {
    bk.MovePrev();
    fw.MoveNext();
    if (bk.id() == line_b.id()) return 1;
    if (fw.id() == line_b.id()) return -1;
    if (bk.is_begin()) return -1;
    if (fw.is_end()) return 1;
  }
}
ID AnnotationEditor::AttrID(const Attribute& attr) {
  // Log() << "AttrID: " << attr.DebugString();
  std::string ser;
  if (!attr.SerializeToString(&ser)) abort();
  auto it = new_attr2id_.find(ser);
  if (it != new_attr2id_.end()) return it->second;
  it = last_attr2id_.find(ser);
  if (it != last_attr2id_.end()) {
    ID id = it->second;
    new_attr2id_.insert(*it);
    last_attr2id_.erase(it);
    return id;
  }
  ID id = AnnotatedString::MakeDecl(commands_, site_, attr);
  new_attr2id_.emplace(std::make_pair(std::move(ser), id));
  return id;
}

ID AnnotationEditor::Mark(ID beg, ID end, ID attr) {
  Annotation a;
  a.set_begin(beg.id);
  a.set_end(end.id);
  a.set_attribute(attr.id);
  return Mark(a);
}

ID AnnotationEditor::Mark(const Annotation& ann) {
  // Log() << "Mark: " << ann.DebugString();
  std::string ser;
  if (!ann.SerializeToString(&ser)) abort();
  auto it = new_ann2id_.find(ser);
  if (it != new_ann2id_.end()) return it->second;
  it = last_ann2id_.find(ser);
  if (it != last_ann2id_.end()) {
    ID id = it->second;
    new_ann2id_.insert(*it);
    last_ann2id_.erase(it);
    return id;
  }
  ID id = AnnotatedString::MakeMark(commands_, site_, ann);
  new_ann2id_.emplace(std::move(ser), id);
  return id;
}

void AnnotationEditor::EndEdit() {
  for (const auto& a : last_ann2id_) {
    AnnotatedString::MakeDelMark(commands_, a.second);
  }
  for (const auto& a : last_attr2id_) {
    AnnotatedString::MakeDelDecl(commands_, a.second);
  }
  last_ann2id_.clear();
  last_attr2id_.clear();
  last_ann2id_.swap(new_ann2id_);
  last_attr2id_.swap(new_attr2id_);
  commands_ = nullptr;
}
