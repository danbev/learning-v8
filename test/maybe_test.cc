#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"

TEST(MaybeLocal, maybeLocal) {
  v8::MaybeLocal<v8::Value> m;
  EXPECT_EQ(true, m.IsEmpty());
  // to produce an error:
  //m.ToLocalChecked();
}
