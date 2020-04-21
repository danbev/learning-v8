#include <iostream>
#include "gtest/gtest.h"
#include "v8_test_fixture.h"
#include "v8.h"

using namespace v8;

class MaybeTest : public V8TestFixture {
};

TEST_F(MaybeTest, Maybe) {
  bool cond = true;
  Maybe<int> maybe = cond ? Just<int>(10) : Nothing<int>();
  EXPECT_TRUE(maybe.IsJust());
  EXPECT_FALSE(maybe.IsNothing());
  maybe.Check();

  int nr = maybe.ToChecked();
  EXPECT_EQ(nr, 10);
  EXPECT_EQ(maybe.FromJust(), 10);

  Maybe<int> nothing = Nothing<int>();
  int value = nothing.FromMaybe(22);
  EXPECT_EQ(value, 22);
}
