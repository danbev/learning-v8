#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"

/*
 * Small integer (SMI) example. These are tagged values.
 *
 * Most computers architectures have a concept of pointer alignment meaning
 * that a pointer to a particular data type must be a multiple of 8.
 * So a pointer can be 8, 16, 24 etc
 *
 * 8  =  1000
 * 16 = 10000
 * 24 = 11000
 *
 * Remember that we are talking about the value stored in a memory location, 
 * which in this case is interpreted as an address. It is this value that 
 * will be a multiple of 8 like described above. As seen above there will be
 * a three of the lowest order bits that will always be zero. This allows us to
 * use them for other purposes. This is done as using a pointer for everything
 * is not efficient. For example, using this for small integers 
 *
 */
TEST(Smi, IntToSmi) {
  v8::internal::Object* obj = v8::internal::IntToSmi<31>(2);
  EXPECT_EQ(2, *obj);
}
