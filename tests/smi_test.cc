#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include <bitset>

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
  int value = 2;
  int smiShiftTagSize = 1;
  int smiShiftSize = 0;
  int smi_shift_bits = smiShiftTagSize + smiShiftSize;
  int smiTag = 0;
  EXPECT_EQ("00000000000000000000000000000010", std::bitset<32>(value).to_string());
  // unintptr_t is being used because bitwise operations cannot be done on pointers
  // according to the standard. 
  uintptr_t tagged = (static_cast<uintptr_t>(value) << smi_shift_bits) | smiTag;
  EXPECT_EQ("00000000000000000000000000000100", std::bitset<32>(tagged).to_string());
  // so we are left shifting one position and then ORing with 0 keeping the int
  // intact. 

  v8::internal::Object* obj = reinterpret_cast<v8::internal::Object*>(tagged);
  // to get the int back we have to right shift since we left shifted before.
  int unwrapped = static_cast<int>(reinterpret_cast<intptr_t>(obj)) >> smi_shift_bits;
  std::cout << "unwrapped  = " << unwrapped << std::endl;
  EXPECT_EQ("00000000000000000000000000000010", std::bitset<32>(unwrapped).to_string());
}
