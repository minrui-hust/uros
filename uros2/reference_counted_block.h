///\file

/******************************************************************************
Reference counted block for uROS.
Stores reference count and pointer to separately allocated object.
Supports flexible alignment and inheritance hierarchies.
******************************************************************************/

#ifndef UROS_REFERENCE_COUNTED_BLOCK_INCLUDED
#define UROS_REFERENCE_COUNTED_BLOCK_INCLUDED

#include "etl/platform.h"
#include "etl/nullptr.h"
#include "etl/atomic.h"

namespace uros
{
  // Forward declaration
  class reference_counted_block_pool;

  //***************************************************************************
  /// Reference counted block with pointer to user block.
  /// The user block is stored in the same memory allocation with proper alignment.
  /// This design supports polymorphic types and varying alignment requirements.
  /// The pool manages size and alignment, block only stores pointers.
  //***************************************************************************
  class reference_counted_block
  {
  public:

    //*************************************************************************
    /// Constructor
    /// \param owner_ The pool that owns this block
    /// \param block_ptr_ Pointer to the user's block memory
    //*************************************************************************
    reference_counted_block(uros::reference_counted_block_pool* owner_,
                           void* block_ptr_)
      : owner(owner_)
      , counter(0)
      , block_ptr(block_ptr_)
    {
    }

    //*************************************************************************
    /// Destructor
    //*************************************************************************
    ~reference_counted_block()
    {
    }

    //*************************************************************************
    /// Set the reference count.
    //*************************************************************************
    void set_reference_count(int value)
    {
      counter = value;
    }

    //*************************************************************************
    /// Increment the reference count.
    //*************************************************************************
    void increment_reference_count()
    {
      ++counter;
    }

    //*************************************************************************
    /// Decrement the reference count.
    /// \return The reference count after decrementing.
    //*************************************************************************
    ETL_NODISCARD int decrement_reference_count()
    {
      return int(--counter);
    }

    //*************************************************************************
    /// Get the current reference count.
    //*************************************************************************
    ETL_NODISCARD int get_reference_count() const
    {
      return int(counter);
    }

    //*************************************************************************
    /// Get the owner pool.
    //*************************************************************************
    ETL_NODISCARD uros::reference_counted_block_pool* get_owner() const
    {
      return owner;
    }

    //*************************************************************************
    /// Get pointer to the user's block memory.
    //*************************************************************************
    ETL_NODISCARD void* get_block_ptr()
    {
      return block_ptr;
    }

    //*************************************************************************
    /// Get const pointer to the user's block memory.
    //*************************************************************************
    ETL_NODISCARD const void* get_block_ptr() const
    {
      return block_ptr;
    }

  private:

    uros::reference_counted_block_pool* owner;  ///< The pool that owns this block.
    etl::atomic_int counter;                    ///< The atomic reference counter.
    void* block_ptr;                            ///< Pointer to the user's block memory.

    // Disable copy and assignment
    reference_counted_block(const reference_counted_block&) ETL_DELETE;
    reference_counted_block& operator=(const reference_counted_block&) ETL_DELETE;
  };
}

#endif
