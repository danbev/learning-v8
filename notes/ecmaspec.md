## EcmaScript Spec
To further my understanding of the V8 code base I've found it helpful to
read the [spec](https://tc39.es/ecma262/). This can make it clear why functions
and fields are named in certain ways and also why they do certain things.

### [[something]]
Most often denotes an internal property named `something`.

### @@
Within the specification a well-known symbol is referred to by using a notation
of the form `@@name`, where `name` is one of the values:
```
Spec Name               [[Description]]
* @@asyncIterator       Symbol.asyncIterator
* @@hasInstance         Symbol.hasInstance
* @@isConcatSpreadable  Symbol.isConcatSpreadable
* @@iterator            Symbol.iterator
* @@match               Symbol.match
* @@matchAll            Symbol.matchAll
* @@replace             Symbol.replace
* @@search              Symbol.search
* @@speices             Symbol.species
* @@split		Symbol.split
* @@toPrimitive		Symbol.toPrimitive
* @@toStringTag         Symbol.toStringTag
```
So you might see something like [@@toPrimitive] anobject would have a function
keyed with Symbol.toPrimitive which can be used access that function.

### Record
A Record in the spec is like a struct in c where each member is called a field.

### Completion Record
Is a Record which is used as a return value and can have one of three possible
fields:
```
[[Type]] (normal, return, throw, break, or continue)
```
If the type is normal, return, or throw then the CompletionRecord can have a

```
[[Value]] which is what is returned/thrown.
```
```
[[Target]] If the type is break or continue it can optionally have a [[Target]].
```

For example:
```js
function something() {
  if (bla) {
    return CompletionRecord({type: "normal", value: "something"});
  } else {
    return CompletionRecord({type: "throw", value: "error"});
  }
}
const result = something();
```
So a function in the spec would return a CompletionRecord and the spec has to
describe what is done in each case.

### ReturnIfAbrupt
If it was an abrupt (throw) in the example above it return the value. Instead
of writing that as text the spec writers can use:
```
ReturnIfAbrupt(something())
```
can be written as:
```
? something()
```
ReturnIfAbrupt means if the value passed in is an abrupt completion then
that value is returned, else if the value is a Completion Record then set the
value to value.[[Value]].

```js
function something() {
  return CompletionRecord({type: "normal", value: "something"});
}
const result = something();
```
```
Let result be !something()
```
This means that something() will `never` return an abrupt completion.

Every abstract operation in the spec implicitely returns a completion record.

### Sets
You can find sets uses in the spec in places where you have something like
`<< one, two >>` which denotes a set with two elements.

### Builtin Objects
In the spec one can see object referred to as `%Array%` which referrs to builtin
objects. These are listed in [well-known-intrinsic-objects](https://tc39.es/ecma262/#sec-well-known-intrinsic-objects).

### Realms
All js code must be associated with a realm before it is evaluated which consists
of the builtin/intrinsic objects, a global environment ([link](https://tc39.es/ecma262/#sec-code-realms).
This could be a window/frame/tab in a browser, another example could be a web
worker or a service worker. Code in one realm might not be able to interact with
code in another. Like a symbol might only be available in one for example.
TODO: add more examples and clarify this.

