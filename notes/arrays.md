### Arrays
The declaration of Array can be found in `include/v8.h` and extends Object. It
has a `Length()` method and functions to create new instances but apart
from that it only has the methods inherited from Object.

Objects have properties that map to values, and Arrays have indecies that map
to elements. V8 stores these differently to be able to optimize certain operations.

An example can be found in [arrays_test.cc](../test/arrays_test.cc).

### Holey Array
Is an array which contains a hole in it, which could happen if the array was
created to be of a certain size but not given any values upon creation, or if
an entry has been deleted from the array. When there is a hole V8 has to look
up the prototype chain to see if the value might exist there. This takes time
and there are optimizations that can be done if V8 knows that there are no holes.

Even if a holey array contains the value being looked for V8 has to perform a
bounds check, followed by `hasOwnProperty` call. 

### Packed Array
Are arrays which don't contain any holes. This means that V8 does not need to
look up the prototype chain for a value as the value being looked for is either
in the array or not, so V8 can do a bounds check on the index being looked for
and return the value there. 

### ElementsKinds
`src/objects/elements-kind.h'

