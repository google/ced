#include "woot.h"
#include "gtest/gtest.h"

TEST(String, NoOp) { String s; }

TEST(String, MutateThenRender) {
  String s;
  Site site;
  auto r = s.Insert(&site, 'a', String::Begin());
  EXPECT_EQ(s.Render(), "");
  EXPECT_EQ(r.str.Render(), "a");
  s = r.str;
  r = s.Insert(&site, 'b', r.command->id());
  EXPECT_EQ(r.str.Render(), "ab");
  s = r.str;
  auto rr = s.Remove(r.command->id());
  EXPECT_EQ(rr.str.Render(), "a");
}
