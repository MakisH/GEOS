/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 TotalEnergies
 * Copyright (c) 2019-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file BlockVectorView.hpp
 */

#ifndef GEOSX_LINEARALGEBRA_UTILITIES_BLOCKVECTORVIEW_HPP_
#define GEOSX_LINEARALGEBRA_UTILITIES_BLOCKVECTORVIEW_HPP_

#include "common/common.hpp"
#include "linearAlgebra/utilities/AsyncRequest.hpp"
#include "common/MpiWrapper.hpp"
#include "common/GEOS_RAJA_Interface.hpp"

namespace geosx
{

/**
 * @brief Abstract view of a block vector.
 * @tparam VECTOR type of sub-vectors
 *
 * This class does not deal with constructing or storing sub-vectors, only provides high-level access functions.
 * See derived classes BlockVector and BlockVectorWrapper for ways to construct a block vector.
 */
template< typename VECTOR >
class BlockVectorView
{
public:

  /// Alias for sub-vector type
  using Vector = VECTOR;

  /// @name Constructors
  ///@{

  /**
   * @brief Deleted copy assignment.
   * @param rhs the block vector to copy
   * @return reference to @p this object
   */
  BlockVectorView & operator=( BlockVectorView const & rhs ) = delete;

  /**
   * @brief Deleted move assignment.
   * @param rhs the block vector to move from
   * @return reference to @p this object
   */
  BlockVectorView & operator=( BlockVectorView && rhs ) noexcept = delete;

  /**
   * @brief Destructor.
   */
  virtual ~BlockVectorView() = default;

  ///@}

  /// @name Setters
  ///@{

  /**
   * @brief Update vector <tt>y</tt> as <tt>y</tt> = <tt>x</tt>.
   * @param src the vector to copy
   */
  void copy( BlockVectorView const & src );

  ///@}

  /// @name Linear Algebra Operations
  ///@{

  /**
   * @brief Scale the block vector with <tt>factor</tt>.
   * @param factor multiplication factor
   */
  void scale( real64 const factor );

  /**
   * @brief Set the vector to zero
   */
  void zero();

  /**
   * @brief Set vector elements to a constant value.
   * @param value value to set vector elements to
   */
  void set( real64 const value );

  /**
   * @brief Set vector elements to random entries.
   * @param seed the random seed to use
   */
  void rand( unsigned const seed );

  /**
   * @brief Dot product.
   * @param x the block vector to compute product with
   * @return the dot product
   */
  real64 dot( BlockVectorView const & x ) const;

  /**
   * @brief Nonblocking dot product with the vector vec.
   * @param x the block vector to compute product with
   * @param request the MPI_Request to wait on for the dot product completion
   * @return the dot product
   * @note Each call to iDot must be paired with a call to MpiWrapper::wait( &request, ... )
   */
   AsyncRequest< real64 > iDot( BlockVectorView const & x ) const;


   template< typename ... VECS >
   AsyncRequest< std::array< real64, sizeof...( VECS ) > > iDot2( VECS const & ... vecs ) const;

  /**
   * @brief 2-norm of the block vector.
   * @return 2-norm of the block vector
   */
  real64 norm2() const;

  /**
   * @brief Inf-norm of the block vector.
   * @return inf-norm of the block vector
   */
  real64 normInf() const;

  /**
   * @brief Update vector <tt>y</tt> as <tt>y = alpha*x + y</tt>.
   * @note The naming convention follows the logic of the BLAS library.
   * @param alpha scaling factor for added vector
   * @param x     vector to add
   */
  void axpy( real64 const alpha,
             BlockVectorView const & x );

  /**
   * @brief Update vector <tt>y</tt> as <tt>y = alpha*x + beta*y</tt>.
   * @note The naming convention follows the logic of the BLAS library.
   *
   * @param alpha scaling factor for added vector
   * @param x     vector to add
   * @param beta  scaling factor for self vector
   */
  void axpby( real64 const alpha,
              BlockVectorView const & x,
              real64 const beta );

  /**
   * @brief Update vector <tt>z</tt> as <tt>z = alpha*x + beta*y + gamma*z</tt>.
   * @note The naming convention follows the logic of the BLAS library.
   *
   * @param alpha scaling factor for first added vector
   * @param x first vector to add
   * @param beta scaling factor for second added vector
   * @param y second vector to add
   * @param gamma scaling factor for self vector
   */
  void axpbypcz( real64 const alpha,
                 BlockVectorView const & x,
                 real64 const beta,
                 BlockVectorView const & y,
                 real64 const gamma );
   
  ///@}

  /// @name Accessors
  ///@{

  /**
   * @brief Get block size.
   * @return The block size.
   */
  localIndex blockSize() const;

  /**
   * @brief Get global size.
   * @return The global size.
   */
  globalIndex globalSize() const;

  /**
   * @brief Get local size.
   * @return The local size.
   */
  localIndex localSize() const;

  /**
   * @brief Print the block vector.
   * @param os the stream to print to
   */
  void print( std::ostream & os = std::cout ) const;

  /**
   * @brief Get a reference to the vector corresponding to block @p blockRowIndex.
   * @param blockIndex index of the block to return
   * @return a reference to the sub-block
   */
  VECTOR const & block( localIndex const blockIndex ) const
  {
    GEOSX_LAI_ASSERT( m_vectors[blockIndex] != nullptr );
    return *m_vectors[blockIndex];
  }

  /**
   * @copydoc block(localIndex const) const
   */
  VECTOR & block( localIndex const blockIndex )
  {
    GEOSX_LAI_ASSERT( m_vectors[blockIndex] != nullptr );
    return *m_vectors[blockIndex];
  }

  /**
   * @copydoc block(localIndex const) const
   */
  VECTOR const & operator()( localIndex const blockIndex ) const
  {
    return block( blockIndex );
  }

  /**
   * @copydoc block(localIndex const) const
   */
  VECTOR & operator()( localIndex const blockIndex )
  {
    return block( blockIndex );
  }

  ///@}

protected:

  /**
   * @name Constructor/assignment Methods
   *
   * @note Protected to prevent users from creating/copying/moving base class objects.
   *       Derived classes should still use them to size/copy/move pointer array.
   */
  ///@{

  /**
   * @brief Create a vector of @p nBlocks blocks.
   * @param nBlocks number of blocks
   */
  explicit BlockVectorView( localIndex const nBlocks )
    : m_vectors( nBlocks )
  { }

  /**
   * @brief Copy constructor.
   */
  BlockVectorView( BlockVectorView const & ) = default;

  /**
   * @brief Move constructor.
   */
  BlockVectorView( BlockVectorView && ) = default;

  ///@}

  /**
   * @brief Resize to a new number of blocks.
   * @param size the new size
   */
  void resize( localIndex const size )
  {
    m_vectors.resize( size );
  }

  /**
   * @brief Set pointer to a vector.
   * @param i   index of vector
   * @param vec new pointer to vector
   */
  void setPointer( localIndex i, VECTOR * vec )
  {
    m_vectors( i ) = vec;
  }

private:

  /// Resizable array of pointers to GEOSX vectors.
  array1d< VECTOR * > m_vectors;
};

template< typename VECTOR >
void BlockVectorView< VECTOR >::copy( BlockVectorView const & src )
{
  GEOSX_LAI_ASSERT_EQ( blockSize(), src.blockSize() );
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    block( i ).copy( src.block( i ) );
  }
}

template< typename VECTOR >
void BlockVectorView< VECTOR >::scale( real64 const factor )
{
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    block( i ).scale( factor );
  }
}

template< typename VECTOR >
void BlockVectorView< VECTOR >::zero()
{
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    block( i ).zero();
  }
}

template< typename VECTOR >
void BlockVectorView< VECTOR >::set( real64 const value )
{
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    block( i ).set( value );
  }
}

template< typename VECTOR >
void BlockVectorView< VECTOR >::rand( unsigned const seed )
{
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    block( i ).rand( seed );
  }
}

template< typename VECTOR >
real64 BlockVectorView< VECTOR >::dot( BlockVectorView const & src ) const
{
  GEOSX_LAI_ASSERT_EQ( blockSize(), src.blockSize() );
  real64 accum = 0;
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    accum += block( i ).dot( src.block( i ) );
  }
  return accum;
}

template< typename VECTOR >
AsyncRequest< real64 > BlockVectorView< VECTOR >::iDot( BlockVectorView const & src ) const
{
  return AsyncRequest< real64 >( [](auto, auto){} );
}

template< typename VECTOR >
template< typename ... VECS > AsyncRequest< std::array< real64, sizeof...( VECS ) > > BlockVectorView<VECTOR>::iDot2( VECS const & ... vecs ) const
{
  return AsyncRequest< std::array< real64, sizeof...( VECS ) > >( [](auto,auto){} );
}

template< typename VECTOR >
real64 BlockVectorView< VECTOR >::norm2() const
{
  real64 accum = 0;
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    real64 const temp = block( i ).norm2();
    accum = accum + temp * temp;
  }
  return std::sqrt( accum );
}

template< typename VECTOR >
real64 BlockVectorView< VECTOR >::normInf() const
{
  real64 result = 0.0;
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    result = std::fmax( result, block( i ).normInf() );
  }
  return result;
}

template< typename VECTOR >
void BlockVectorView< VECTOR >::axpy( real64 const alpha,
                                      BlockVectorView const & x )
{
  GEOSX_LAI_ASSERT_EQ( blockSize(), x.blockSize() );
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    block( i ).axpy( alpha, x.block( i ) );
  }
}

template< typename VECTOR >
void BlockVectorView< VECTOR >::axpby( real64 const alpha,
                                       BlockVectorView const & x,
                                       real64 const beta )
{
  GEOSX_LAI_ASSERT_EQ( blockSize(), x.blockSize() );
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    block( i ).axpby( alpha, x.block( i ), beta );
  }
}

template< typename VECTOR >
void BlockVectorView< VECTOR >::axpbypcz( real64 const alpha,
                                          BlockVectorView const & x,
                                          real64 const beta,
                                          BlockVectorView const & y,
                                          real64 const gamma )
{
  GEOSX_LAI_ASSERT_EQ( blockSize(), x.blockSize() );
  GEOSX_LAI_ASSERT_EQ( blockSize(), y.blockSize() );
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    block( i ).axpbypcz( alpha, x.block( i ), beta, y.block( i ), gamma );
  }
}

template< typename VECTOR >
localIndex BlockVectorView< VECTOR >::blockSize() const
{
  return m_vectors.size();
}

template< typename VECTOR >
globalIndex BlockVectorView< VECTOR >::globalSize() const
{
  globalIndex size = 0;
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    size += block( i ).globalSize();
  }
  return size;
}

template< typename VECTOR >
localIndex BlockVectorView< VECTOR >::localSize() const
{
  localIndex size = 0;
  for( localIndex i = 0; i < blockSize(); i++ )
  {
    size += block( i ).localSize();
  }
  return size;
}

template< typename VECTOR >
void BlockVectorView< VECTOR >::print( std::ostream & os ) const
{
  for( localIndex i = 0; i < m_vectors.size(); i++ )
  {
    os << "Block " << i << " of " << blockSize() << ":" << std::endl;
    os << "=============" << std::endl;
    block( i ).print( os );
  }
}

/**
 * @brief Stream insertion operator
 * @param os the output stream
 * @param vec the vector to be printed
 * @return reference to the output stream
 */
template< typename VECTOR >
std::ostream & operator<<( std::ostream & os,
                           BlockVectorView< VECTOR > const & vec )
{
  vec.print( os );
  return os;
}

} // end geosx namespace


#endif /* GEOSX_LINEARALGEBRA_UTILITIES_BLOCKVECTORVIEW_HPP_ */
