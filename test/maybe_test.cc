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

/*
 * I think the intention with a type Maybe<void> is that we don't really
 * care/want to have a value in the Maybe apart from that is is empty or
 * something. So instead of having a bool and setting it to true just
 * have void and return an empty. I think this signals the intent of a
 * function better as one might otherwise wonder what the value in the maybe
 * represents.
 */
Maybe<void> doit(int x) {
  if (x == -1) {
    return Nothing<void>();
  }
  return JustVoid();
}

TEST_F(MaybeTest, MaybeVoid) {
  Maybe<void> maybe = JustVoid();
  EXPECT_FALSE(maybe.IsNothing());

  Maybe<void> maybe_nothing = Nothing<void>();
  EXPECT_TRUE(maybe_nothing.IsNothing());

  EXPECT_TRUE(doit(-1).IsNothing());
  EXPECT_TRUE(doit(1).IsJust());
}
