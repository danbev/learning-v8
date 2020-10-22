#include <iostream>
#include <memory>

// This example tries to mimic how the destructors currently work for V8
// BackingStore's. Running this example will produce a Address Sanitizer
// error as described in notes/backing_store_issue.md.
//
// $ g++ -g -fsanitize=address -o backing-store-org backing-store-original.cc
class BaseStore { 
  public: 
    BaseStore() {
      std::cout << "Constructing BaseStore " << this << '\n'; 
    }
    ~BaseStore() { std::cout << "~BaseStore " << this << '\n'; }      
}; 

class InternalStore: public BaseStore { 
  public: 
    InternalStore() { std::cout << "Constructing InternalStore " << this << '\n'; } 
    ~InternalStore() { std::cout << "~InternalStore " << this << '\n'; } 
  private:
    char c[100];
}; 

class PublicStore: public BaseStore {
  public: 
    ~PublicStore() { 
      std::cout << "~PublicStore " << this << '\n';
      InternalStore* i = reinterpret_cast<InternalStore*>(this);
      i->~InternalStore();
    } 
  private:
    PublicStore() { std::cout << "Constructing PublicStore " << this << '\n'; } 
}; 

int main(void) 
{ 
  std::unique_ptr<BaseStore> base = std::unique_ptr<BaseStore>(new InternalStore());
  { 
    std::unique_ptr<PublicStore> p_store = std::unique_ptr<PublicStore>(static_cast<PublicStore*>(base.release()));

    std::shared_ptr<BaseStore> base_store = std::move(p_store);
    {
      std::shared_ptr<InternalStore> i_store = std::static_pointer_cast<InternalStore>(base_store);
      std::cout << "inside i_store scope...use_count: " << base_store.use_count() << '\n';
      // count is 1 so the underlying object will not be deleted.
    }
    std::cout << "after i_store...use_count: " << base_store.use_count() << '\n';
    // When the this scope ends, base_store's use_count  will be checked and it
    // will be 0 and hence deleted. Static/early binding is in use here so
    // ~PublicStore will be called. ~BaseStore's destructor will be called
    // twice in this case, once by the call to i->~InternalStore(), and the
    // after ~PublicStore as completed.
  }

  return 0; 
} 
