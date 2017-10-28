#include "woot.h"
#include "gtest/gtest.h"

namespace woot {
namespace testing {

TEST(String, NoOp) { String<char> s; }

TEST(String, MutateThenRender) {
  String<char> s;
  SitePtr site = Site::Make();
  auto r = s.Insert(site, 'a', s.begin());
  EXPECT_EQ(s.Render(), "");
  EXPECT_EQ(r.str.Render(), "a");
  s = r.str;
  r = s.Insert(site, 'b', r.id);
  EXPECT_EQ(r.str.Render(), "ab");
  s = r.str;
  auto rr = s.Remove(r.id);
  EXPECT_EQ(rr.str.Render(), "a");
}

}  // namespace testing
}  // namespace woot
