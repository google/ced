#include "woot.h"
#include "gtest/gtest.h"

namespace woot {
namespace testing {

TEST(String, NoOp) { String<char> s; }

TEST(String, InsertThenRender) {
  String<char> s;
  SitePtr site = Site::Make();
  auto r = s.Insert(site, 'a', s.begin());
  EXPECT_EQ(s.Render(), "");
  EXPECT_EQ(r.str.Render(), "a");
}

}  // namespace testing
}  // namespace woot
