### String types
There are a number of different String types in V8 which are optimized for various situations.
If we look in src/objects/objects.h we can see the object hierarchy:
```
    Object
      SMI
      HeapObject    // superclass for every object instans allocated on the heap.
        ...
        Name
          String
            SeqString
              SeqOneByteString
              SeqTwoByteString
            SlicedString
            ConsString
            ThinString
            ExternalString
              ExternalOneByteString
              ExternalTwoByteString
            InternalizedString
              SeqInternalizedString
                SeqOneByteInternalizedString
                SeqTwoByteInternalizedString
              ConsInternalizedString
              ExternalInternalizedString
                ExternalOneByteInternalizedString
                ExternalTwoByteInternalizedString
```

Note that v8::String is declared in `include/v8.h`.

`Name` as can be seen extends HeapObject and anything that can be used as a
property name should extend Name.

Looking at the declaration in include/v8.h we find the following:
```c++
    int GetIdentityHash();
    static Name* Cast(Value* obj)
```

### Unicode
This section contains some notes about unicode which are good to keep in mind
while reading the code and understanding what some of the string related
functions do.

#### Abstract characters
An abstract character in unicode has a name like `LATIN SMALL LETTER A`.

#### Code point
A code point is a number associated with an abstract character, for example
U+0061 (U+<hex> where U stands for Unicode).

#### Planes
A plane is a range of 65536 continuous code points from U+n0000 to U+nFFFF.
There are 17 equals groups:
```
Plane 0: U+0000 -> U+FFFF           Basic Multilingual Plane (BMP)
Plane 1: U+10000 -> U+1FFFF         Supplementary Multilingual Plane
Plane 2: U+20000 -> U+2FFFF         Supplementary Ideographic Plane
Plane 3: U+30000 -> U+3FFFF
...
Plane 16: U+100000 -> U+10FFFF      Supplementary Private Use Area B.
```
BMP contains most of the characters that are used when programming which can
have 4 hex digits.

#### Code units
Not to be confused with code points which are just numbers used to look up
abstract characters in the format U+<hex>. The memory in a computer does not
deal with code points or abstract characters but instead it deals with code
units which is a bit sequence.
You can take an code point and put it through a function that translates
it into a code unit. This process is called character encoding. There are different
encodings and JavaScript uses UTF-16.

#### 16-bit Unicode Transformation Format (UTF-16)
Code points from BPM are transformed/encoded into single code unit of 16-bits
Code points from the other planes are transformed/encoded into two code units
each of size 16-bits.

```
Abstract Character        Code point       Code unit
LATIN SMALL LETTER A      U+0061           0x0061
LATIN SMALL LETTER B      U+0062           0x0062
...
```

#### Surrogate pairs
To represent the Swedish character 'å' we need to code units each of 16-bits:
```console
$ node -p "console.log('\u0061\u030A')"
å
```
This pair for code units represent a single abstract character. The first is
called the high-surrogate code unit, and the second the log-surrogate code unit.

In the case or 'å' the second code unit is called a combinging mark as it modifies
the character preceeding it.


#### String
A String extends `Name` and has a length and content. The content can be made
up of 1 or 2 byte characters.
Looking at the declaration in include/v8.h we find the following:
```c++
    enum Encoding {
      UNKNOWN_ENCODING = 0x1,
      TWO_BYTE_ENCODING = 0x0,
      ONE_BYTE_ENCODING = 0x8
    };

    int Length() const;
    int Uft8Length const;
    bool IsOneByte() const;
```
Example usages can be found in [test/string_test.cc](./test/string_test.cc).
Looking at the functions I've seen one that returns the actual bytes
from the String. You can get at the in utf8 format using:
```c++
    String::Utf8Value print_value(joined);
    std::cout << *print_value << '\n';
```
So that is the only string class in include/v8.h, but there are a lot more
implementations that we've seen above. There are used for various cases, for
example for indexing, concatenation, and slicing).

#### SeqString
Represents a sequence of characters which (the characters) are either one or two
bytes in length.

#### ConsString
These are string that are built using:

    const str = "one" + "two";

This would be represented as:
```
         +--------------+
         |              | 
   [str|one|two]     [one|...]   [two|...]
             |                       |
             +-----------------------+
```
So we can see that one and two in str are pointers to existing strings. 

### NewFromUt8Literal
This function was introduced in b097a8e5de7.

The normal way of creating a Local<String> would be to use:
```c++
  Local<String> str = String::NewFromUtf8(isolate_, "åäö").ToLocalChecked();    
```
Now, String::NewFromUtf8 looks like this:
```c++
MaybeLocal<String> String::NewFromUtf8(Isolate* isolate, const char* data,           
                                       NewStringType type, int length) {        
  NEW_STRING(isolate, String, NewFromUtf8, char, data, type, length);           
  return result;                                                                
}
```
The macro `NEW_STRING` (which can be found in src/api/api.cc) 
We can take a look at the expanded macro using:
```console
$ g++ -I./out/x64.release_gcc/gen -I./include -I. -E src/api/api.cc > output
```
```c++
MaybeLocal<String> String::NewFromUtf8(Isolate* isolate, const char* data,      
                                       NewStringType type, int length) {        
  MaybeLocal<String> result;
  if (length == 0) {
    result = String::Empty(isolate);
  } else if (length > i::String::kMaxLength) {
    result = MaybeLocal<String>();
  } else {
    i::Isolate* i_isolate = reinterpret_cast<internal::Isolate*>(isolate);
    i::VMState<v8::OTHER> __state__((i_isolate));;
    i::RuntimeCallTimerScope _runtime_timer( i_isolate, i::RuntimeCallCounterId::kAPI_String_NewFromUtf8);
    do {
      auto&& logger = (i_isolate)->logger();
      if (logger->is_logging())
        logger->ApiEntryCall("v8::" "String" "::" "NewFromUtf8");
    } while (false);
    if (length < 0)
      length = StringLength(data);
     i::Handle<i::String> handle_result = NewString(i_isolate->factory(), type, i::Vector<const char>(data, length)) .ToHandleChecked();
     result = Utils::ToLocal(handle_result);
  };
  return result;                                                                
}
```
There are a number of checks that are not required when we have a string literal,
and some that can be checked at compile time, like the max length. 
```c++
  template <int N>                                                              
  static V8_WARN_UNUSED_RESULT Local<String> NewFromUtf8Literal(                
      Isolate* isolate, const char (&literal)[N],                                    
      NewStringType type = NewStringType::kNormal) {                                 
    static_assert(N <= kMaxLength, "String is too long");                       
    return NewFromUtf8Literal(isolate, literal, type, N - 1);                        
  }                                                                                  
```
Notice the `static_assert` which is performed at compile time.
          

Local<String> String::NewFromUtf8Literal(Isolate* isolate, const char* literal, 
                                         NewStringType type, int length) {           
  DCHECK_LE(length, i::String::kMaxLength);
  i::Isolate* i_isolate = reinterpret_cast<internal::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  LOG_API(i_isolate, String, NewFromUtf8Literal);
  i::Handle<i::String> handle_result =
      NewString(i_isolate->factory(), type,
                i::Vector<const char>(literal, length))
          .ToHandleChecked();
  return Utils::ToLocal(handle_result);
}
```
We can see that the compiler instantiates this template as:
```console
$ nm -C test/string_test | grep NewFromUtf8Literal
                 U v8::String::NewFromUtf8Literal(v8::Isolate*, char const*, v8::NewStringType, int)
000000000041551c W v8::Local<v8::String> v8::String::NewFromUtf8Literal<10>(v8::Isolate*, char const (&) [10], v8::NewStringType)
```


#### ExternalString
These strings are located on the native heap. The ExternalString structure has a
pointer to this external location and the usual length field for all Strings.

