#include <iostream>
#include <memory>

// This example contains a suggestion to how the destructors for V8's
// BackingStore's could be changed to address the Address Sanitizer
// error as described in notes/backing_store_issue.md.
//
// $ g++ -g -fsanitize=address -o backing-store-new backing-store-new.cc
class BaseStore { 
  public: 
    BaseStore() {
      std::cout << "Constructing BaseStore " << this << '\n'; 
    }
    virtual ~BaseStore() { std::cout << "~BaseStore " << this <<  '\n'; }      
}; 

class InternalStore: public BaseStore { 
  public: 
    InternalStore() { std::cout << "Constructing InternalStore " << this << '\n'; } 
    ~InternalStore() override {
      std::cout << "~InternalStore " << this << '\n';
    }
  private:
    char c[100];
}; 

class PublicStore: public BaseStore {
  public: 
    ~PublicStore() override {
      std::cout << "~PublicStore " << this << '\n';
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
      // count is 1 so the underlying object will not be deleted. When the
      // next scope ends, base_store's use_count  will be checked and it will be
      // 0 and hence deleted. In this case since BaseStore has a virtual
      // destructor and InternalStore implements it, InternalStore's destructor
      // will be called. Dynamic look up is in place as opposed to static/early
      // binding in the case of a non-virtual version.
    }
    std::cout << "after i_store...use_count: " << base_store.use_count() << '\n';
    // When this scope ends, base_store's use_count will be checked and it 
    // will be 0 and hence deleted. In this case since BaseStore has a virtual
    // destructor and InternalStore implements it, InternalStore's destructor
    // will be called. Dynamic look up is in place as opposed to static/early
    // binding in the case of a non-virtual version.
  }

  return 0; 
} 
