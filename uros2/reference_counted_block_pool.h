///\file

/******************************************************************************
Block pool for reference counted blocks in uROS.
Manages allocation of reference_counted_block and object memory separately.
******************************************************************************/

#ifndef UROS_REFERENCE_COUNTED_BLOCK_POOL_INCLUDED
#define UROS_REFERENCE_COUNTED_BLOCK_POOL_INCLUDED

#include "reference_counted_block.h"
#include "platform.h"
#include "etl/imemory_block_allocator.h"
#include "etl/alignment.h"
#include "etl/nullptr.h"
#include "etl/exception.h"
#include "etl/error_handler.h"
#include "etl/utility.h"
#include <new>

namespace uros
{
  //***************************************************************************
  /// Exception for block pool allocation failure
  //***************************************************************************
  class block_pool_allocation_failure : public etl::exception
  {
  public:
    block_pool_allocation_failure(string_type file_name_, numeric_type line_number_)
      : etl::exception(ETL_ERROR_TEXT("block_pool:allocation_failure", "uros:bpool:A"), file_name_, line_number_)
    {
    }
  };

  //***************************************************************************
  /// Block pool for reference counted blocks
  /// Only allocates blocks with specified size and alignment
  //***************************************************************************
  class reference_counted_block_pool
  {
  public:

    //*************************************************************************
    /// Constructor
    /// \param allocator_ Allocator for reference_counted_block structures
    //*************************************************************************
    reference_counted_block_pool(etl::imemory_block_allocator& allocator_)
      : allocator(allocator_)
    {
    }

    //*************************************************************************
    /// Destructor
    //*************************************************************************
    ~reference_counted_block_pool()
    {
    }

    //*************************************************************************
    /// Allocate a reference counted block with user block
    /// \param user_block_size Size of the user's block in bytes
    /// \param user_block_alignment Alignment requirement of the user's block
    /// \return Pointer to reference_counted_block, or nullptr on failure
    //*************************************************************************
    reference_counted_block* allocate(size_t user_block_size, size_t user_block_alignment)
    {
      // Align user block offset after reference_counted_block
      size_t offset = (sizeof(reference_counted_block) + user_block_alignment - 1) & ~(user_block_alignment - 1);
      size_t total_size = offset + user_block_size;
      size_t alignment = (etl::alignment_of<reference_counted_block>::value > user_block_alignment) 
                       ? etl::alignment_of<reference_counted_block>::value : user_block_alignment;
      
      lock();
      void* memory = allocator.allocate(total_size, alignment);
      unlock();
      
      if (memory == ETL_NULLPTR)
      {
        ETL_ASSERT(false, ETL_ERROR(block_pool_allocation_failure));
        return ETL_NULLPTR;
      }

      return new (memory) reference_counted_block(this, static_cast<char*>(memory) + offset);
    }

    //*************************************************************************
    /// Release a reference counted block and its user block
    /// \param rcblock Pointer to the reference_counted_block to release
    //*************************************************************************
    void release(reference_counted_block* rcblock)
    {
      if (rcblock == ETL_NULLPTR) return;

      rcblock->~reference_counted_block();
      
      lock();
      allocator.release(rcblock);
      unlock();
    }

  private:

    //*************************************************************************
    /// Lock for thread safety using OAL SchedulerLock
    //*************************************************************************
    void lock()
    {
      SchedulerLock::Lock();
    }

    //*************************************************************************
    /// Unlock for thread safety using OAL SchedulerLock
    //*************************************************************************
    void unlock()
    {
      SchedulerLock::Unlock();
    }

    etl::imemory_block_allocator& allocator;  ///< Allocator for reference_counted_block structures

    // Prevent copying
    reference_counted_block_pool(const reference_counted_block_pool&) ETL_DELETE;
    reference_counted_block_pool& operator=(const reference_counted_block_pool&) ETL_DELETE;
  };
}

#endif
