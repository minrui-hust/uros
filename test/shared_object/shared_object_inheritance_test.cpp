///\file

/******************************************************************************
Test for shared_object inheritance and polymorphism support.
Tests that shared_object<Base> can hold shared_object<Derived>.
******************************************************************************/

#include "uros2/shared_object.h"
#include "uros2/object_pool.h"
#include "uros2/block_allocator.h"
#include <iostream>
#include <cassert>

// Base class
class Animal
{
public:
  Animal(int age_) : age(age_) {}
  virtual ~Animal() {}
  
  virtual const char* speak() const { return "Generic animal sound"; }
  
  int get_age() const { return age; }
  
protected:
  int age;
};

// Derived class
class Dog : public Animal
{
public:
  Dog(int age_, const char* name_) 
    : Animal(age_)
    , name(name_) 
  {}
  
  virtual const char* speak() const override { return "Woof!"; }
  
  const char* get_name() const { return name; }
  
private:
  const char* name;
};

// Another derived class
class Cat : public Animal
{
public:
  Cat(int age_, bool is_grumpy_) 
    : Animal(age_)
    , is_grumpy(is_grumpy_) 
  {}
  
  virtual const char* speak() const override { return is_grumpy ? "Hiss!" : "Meow~"; }
  
  bool get_grumpiness() const { return is_grumpy; }
  
private:
  bool is_grumpy;
};

int main()
{
  std::cout << "=== Testing shared_object inheritance support ===" << std::endl;
  
  // Create memory allocator and pool
  uros::memory_block_allocator<1024, 16> allocator;
  uros::object_pool pool(allocator);
  
  // Test 1: Create Dog and assign to Animal shared_object
  {
    std::cout << "\nTest 1: Dog -> Animal (copy constructor)" << std::endl;
    
    auto dog = uros::shared_object<Dog>::create(pool, 3, "Buddy");
    assert(dog.is_valid());
    assert(dog.use_count() == 1);
    std::cout << "  Dog created: " << dog->get_name() << ", age " << dog->get_age() << std::endl;
    std::cout << "  Dog says: " << dog->speak() << std::endl;
    
    // Convert Dog to Animal
    uros::shared_object<Animal> animal(dog);
    assert(animal.is_valid());
    assert(animal.use_count() == 2);
    assert(dog.use_count() == 2);
    std::cout << "  After conversion, dog ref count: " << dog.use_count() << std::endl;
    std::cout << "  Animal says: " << animal->speak() << " (polymorphic call)" << std::endl;
    std::cout << "  PASS" << std::endl;
  }
  
  // Test 2: Move Dog to Animal shared_object
  {
    std::cout << "\nTest 2: Dog -> Animal (move constructor)" << std::endl;
    
    auto dog = uros::shared_object<Dog>::create(pool, 5, "Max");
    std::cout << "  Dog created: " << dog->get_name() << ", age " << dog->get_age() << std::endl;
    
    // Move Dog to Animal
    uros::shared_object<Animal> animal(etl::move(dog));
    assert(animal.is_valid());
    assert(!dog.is_valid());
    assert(animal.use_count() == 1);
    std::cout << "  After move, dog is valid: " << dog.is_valid() << std::endl;
    std::cout << "  Animal ref count: " << animal.use_count() << std::endl;
    std::cout << "  Animal says: " << animal->speak() << std::endl;
    std::cout << "  PASS" << std::endl;
  }
  
  // Test 3: Copy assignment Cat to Animal
  {
    std::cout << "\nTest 3: Cat -> Animal (copy assignment)" << std::endl;
    
    auto cat = uros::shared_object<Cat>::create(pool, 2, false);
    std::cout << "  Cat created, age " << cat->get_age() << ", grumpy: " << cat->get_grumpiness() << std::endl;
    std::cout << "  Cat says: " << cat->speak() << std::endl;
    
    uros::shared_object<Animal> animal;
    animal = cat;
    
    assert(animal.is_valid());
    assert(animal.use_count() == 2);
    assert(cat.use_count() == 2);
    std::cout << "  After assignment, cat ref count: " << cat.use_count() << std::endl;
    std::cout << "  Animal says: " << animal->speak() << std::endl;
    std::cout << "  PASS" << std::endl;
  }
  
  // Test 4: Move assignment Cat to Animal
  {
    std::cout << "\nTest 4: Cat -> Animal (move assignment)" << std::endl;
    
    auto cat = uros::shared_object<Cat>::create(pool, 7, true);
    std::cout << "  Grumpy cat created, age " << cat->get_age() << std::endl;
    
    uros::shared_object<Animal> animal;
    animal = etl::move(cat);
    
    assert(animal.is_valid());
    assert(!cat.is_valid());
    assert(animal.use_count() == 1);
    std::cout << "  After move assignment, cat is valid: " << cat.is_valid() << std::endl;
    std::cout << "  Animal says: " << animal->speak() << std::endl;
    std::cout << "  PASS" << std::endl;
  }
  
  // Test 5: Polymorphic array
  {
    std::cout << "\nTest 5: Polymorphic array of Animals" << std::endl;
    
    uros::shared_object<Animal> animals[3];
    
    animals[0] = uros::shared_object<Dog>::create(pool, 4, "Rex");
    animals[1] = uros::shared_object<Cat>::create(pool, 2, false);
    animals[2] = uros::shared_object<Dog>::create(pool, 6, "Luna");
    
    for (int i = 0; i < 3; ++i)
    {
      std::cout << "  Animal[" << i << "] age " << animals[i]->get_age() 
                << " says: " << animals[i]->speak() << std::endl;
    }
    std::cout << "  PASS" << std::endl;
  }
  
  std::cout << "\n=== All tests passed! ===" << std::endl;
  
  return 0;
}
