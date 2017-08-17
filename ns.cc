#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "libplatform/libplatform.h"
//#include "v8-ns.h"
#include "v8.h"

using v8::Undefined;
using v8::True;
using v8::Null;

int main(int argc, char* argv[]) {
  printf("%s\n", (*True ? "true": "false"));
  printf("%s\n", (*Null ? "true": "false"));
  printf("%s\n", (*Undefined));
  return 0;
}
