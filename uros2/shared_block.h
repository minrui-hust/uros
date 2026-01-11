///\file

/******************************************************************************
Smart pointer for reference counted blocks in uROS.
Generic block management without assuming object types.
******************************************************************************/

#ifndef UROS_SHARED_BLOCK_INCLUDED
#define UROS_SHARED_BLOCK_INCLUDED

#include "reference_counted_block.h"
#include "reference_counted_block_pool.h"
#include "etl/nullptr.h"
#include "etl/utility.h"

namespace uros
{
  //***************************************************************************
  /// Smart pointer wrapper for reference counted blocks.
  /// Manages the lifetime of blocks allocated from reference_counted_block_pool.
  //***************************************************************************
  class shared_block
  {
  public:

    //*************************************************************************
    /// Default constructor - creates an empty shared block
    //*************************************************************************
    shared_block()
      : p_block(ETL_NULLPTR)
    {
    }

    //*************************************************************************
    /// Create a shared block with specified size and alignment
    /// \param pool The pool to allocate from
    /// \param size Size of the user block in bytes
    /// \param alignment Alignment requirement of the user block
    /// \return A shared_block managing the allocated memory
    //*************************************************************************
    static shared_block create(reference_counted_block_pool& pool, size_t size, size_t alignment)
    {
      shared_block block;
      block.p_block = pool.allocate(size, alignment);
      
      if (block.p_block != ETL_NULLPTR)
      {
        block.p_block->set_reference_count(1);
      }
      
      return block;
    }

    //*************************************************************************
    /// Copy constructor - increments reference count
    //*************************************************************************
    shared_block(const shared_block& other)
      : p_block(other.p_block)
    {
      if (p_block != ETL_NULLPTR)
      {
        p_block->increment_reference_count();
      }
    }

    //*************************************************************************
    /// Move constructor - transfers ownership
    //*************************************************************************
    shared_block(shared_block&& other) ETL_NOEXCEPT
      : p_block(other.p_block)
    {
      other.p_block = ETL_NULLPTR;
    }

    //*************************************************************************
    /// Destructor - decrements reference count and releases if zero
    //*************************************************************************
    ~shared_block()
    {
      if ((p_block != ETL_NULLPTR) &&
          (p_block->decrement_reference_count() == 0))
      {
        release();
      }
    }

    //*************************************************************************
    /// Copy assignment operator
    //*************************************************************************
    shared_block& operator=(const shared_block& other)
    {
      if (&other != this)
      {
        if ((p_block != ETL_NULLPTR) &&
            (p_block->decrement_reference_count() == 0))
        {
          release();
        }

        p_block = other.p_block;
        
        if (p_block != ETL_NULLPTR)
        {
          p_block->increment_reference_count();
        }
      }

      return *this;
    }

    //*************************************************************************
    /// Move assignment operator
    //*************************************************************************
    shared_block& operator=(shared_block&& other) ETL_NOEXCEPT
    {
      if (&other != this)
      {
        if ((p_block != ETL_NULLPTR) &&
            (p_block->decrement_reference_count() == 0))
        {
          release();
        }

        p_block = other.p_block;
        other.p_block = ETL_NULLPTR;
      }

      return *this;
    }

    //*************************************************************************
    /// Get pointer to the user's block
    //*************************************************************************
    ETL_NODISCARD void* get()
    {
      return (p_block != ETL_NULLPTR) ? p_block->get_block_ptr() : ETL_NULLPTR;
    }

    //*************************************************************************
    /// Get const pointer to the user's block
    //*************************************************************************
    ETL_NODISCARD const void* get() const
    {
      return (p_block != ETL_NULLPTR) ? p_block->get_block_ptr() : ETL_NULLPTR;
    }

    //*************************************************************************
    /// Bool conversion operator
    //*************************************************************************
    ETL_NODISCARD operator bool() const
    {
      return (p_block != ETL_NULLPTR);
    }

    //*************************************************************************
    /// Get the reference count
    //*************************************************************************
    ETL_NODISCARD int use_count() const
    {
      return (p_block != ETL_NULLPTR) ? p_block->get_reference_count() : 0;
    }

    //*************************************************************************
    /// Check if the block is valid (not null)
    //*************************************************************************
    ETL_NODISCARD bool is_valid() const
    {
      return (p_block != ETL_NULLPTR);
    }

    //*************************************************************************
    /// Reset to empty
    //*************************************************************************
    void reset()
    {
      if ((p_block != ETL_NULLPTR) &&
          (p_block->decrement_reference_count() == 0))
      {
        release();
      }
      
      p_block = ETL_NULLPTR;
    }

  private:

    //*************************************************************************
    /// Release the block back to the pool
    //*************************************************************************
    void release()
    {
      if (p_block != ETL_NULLPTR)
      {
        reference_counted_block_pool* owner = p_block->get_owner();
        if (owner != ETL_NULLPTR)
        {
          owner->release(p_block);
        }
        p_block = ETL_NULLPTR;
      }
    }

    reference_counted_block* p_block; ///< Pointer to the reference counted block.
  };
}

#endif
