/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2016-2024 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2024 Total, S.A
 * Copyright (c) 2018-2024 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2024 Chevron
 * Copyright (c) 2019-     GEOS/GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file denseLASolvers.hpp
 */
#ifndef GEOS_DENSELINEARALGEBRA_DENSELASOLVERS_HPP_
#define GEOS_DENSELINEARALGEBRA_DENSELASOLVERS_HPP_

#include "common/DataTypes.hpp"
#include "denseLinearAlgebra/common/layouts.hpp"

#include <complex>

namespace geos
{

namespace denseLinearAlgebra
{

namespace internal
{
/**
 * @brief Solves a 2x2 linear system A * x = b.
 *
 * This function solves a linear system of the form A * x = b, where A is a 2x2 matrix,
 * b is a 2x1 vector, and x is the solution vector. The function checks the sizes
 * of the inputs to ensure they conform to the expected dimensions. It also checks that
 * the determinant of matrix A is not near zero to avoid solving a singular system.
 *
 * @tparam MATRIX_TYPE The type of the matrix A. Must support indexing with `A[i][j]`.
 * @tparam RHS_TYPE The type of the right-hand side vector b. Must support indexing with `b[i]`.
 * @tparam SOL_TYPE The type of the solution vector x. Must support indexing with `x[i]`.
 *
 * @param[in] A The 2x2 matrix representing the system of equations. Must have size 2x2.
 * @param[in] b The 2-element vector representing the right-hand side of the equation.
 * @param[out] x The 2-element vector that will store the solution to the system.
 */
template< typename MATRIX_TYPE,
          typename RHS_TYPE,
          typename SOL_TYPE >
GEOS_HOST_DEVICE
inline
void solveTwoByTwoSystem( MATRIX_TYPE const & A, RHS_TYPE const & b, SOL_TYPE && x )
{
  LvArray::tensorOps::internal::checkSizes< 2, 2 >( A );
  LvArray::tensorOps::internal::checkSizes< 2 >( b );
  LvArray::tensorOps::internal::checkSizes< 2 >( x );

  real64 const detA = A[0][0] * A[1][1] - A[0][1] * A[1][0];

  GEOS_ERROR_IF_LT_MSG( LvArray::math::abs( detA ), LvArray::NumericLimits< real64 >::epsilon; , "Singular system." );

  x[0] = (A[1][1] * b[0] - A[0][1] * b[1] ) / detA;
  x[1] = (A[0][0] * b[1] - A[1][0] * b[0] ) / detA;
}

/**
 * @brief Solves a 3x3 linear system A * x = b.
 *
 * This function solves a linear system of the form A * x = b, where A is a 3x3 matrix,
 * b is a 3x1 vector, and x is the solution vector. The function checks the sizes
 * of the inputs to ensure they conform to the expected dimensions. It also checks that
 * the determinant of matrix A is not near zero to avoid solving a singular system.
 *
 * @tparam MATRIX_TYPE The type of the matrix A. Must support indexing with `A[i][j]`.
 * @tparam RHS_TYPE The type of the right-hand side vector b. Must support indexing with `b[i]`.
 * @tparam SOL_TYPE The type of the solution vector x. Must support indexing with `x[i]`.
 *
 * @param[in] A The 3x3 matrix representing the system of equations. Must have size 3x3.
 * @param[in] b The 3-element vector representing the right-hand side of the equation.
 * @param[out] x The 3-element vector that will store the solution to the system.
 */
template< typename MATRIX_TYPE,
          typename RHS_TYPE,
          typename SOL_TYPE >
GEOS_HOST_DEVICE
inline
void solveThreeByThreeSystem( MATRIX_TYPE const & A, RHS_TYPE const & b, SOL_TYPE && x )
{
  LvArray::tensorOps::internal::checkSizes< 3, 3 >( A );
  LvArray::tensorOps::internal::checkSizes< 3 >( b );
  LvArray::tensorOps::internal::checkSizes< 3 >( x );

  real64 const detA = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
                      A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
                      A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

  GEOS_ERROR_IF_LT_MSG( LvArray::math::abs( detA ), LvArray::NumericLimits< real64 >::epsilon; , "Singular system." );

  real64 const detX0 = b[0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
                       b[1] * (A[0][1] * A[2][2] - A[0][2] * A[2][1]) +
                       b[2] * (A[0][1] * A[1][2] - A[0][2] * A[1][1]);

  real64 const detX1 = A[0][0] * (b[1] * A[2][2] - b[2] * A[2][1]) -
                       A[0][1] * (b[0] * A[2][2] - b[2] * A[2][0]) +
                       A[0][2] * (b[0] * A[1][2] - b[1] * A[1][0]);

  real64 const detX2 = A[0][0] * (A[1][1] * b[2] - A[1][2] * b[1]) -
                       A[0][1] * (A[1][0] * b[2] - A[1][2] * b[0]) +
                       A[0][2] * (A[1][0] * b[1] - A[1][1] * b[0]);

  x[0] = detX0 / detA;
  x[1] = detX1 / detA;
  x[2] = detX2 / detA;
}

/**
 * @brief Solves a linear system where the matrix is upper triangular using back substitution.
 *
 * This function solves the linear system `Ax = b`, where `A` is an upper triangular matrix, using
 * back substitution. The solution `x` is computed and stored in the provided output vector.
 *
 * @tparam N The size of the square matrix `A`.
 * @tparam MATRIX_TYPE The type of the matrix `A`.
 * @tparam RHS_TYPE The type of the right-hand side vector `b`.
 * @tparam SOL_TYPE The type of the solution vector `x`.
 * @param[in] A The upper triangular matrix representing the coefficients of the system.
 * @param[in,out] b The right-hand side vector. It is used to compute the solution, but the values are not preserved.
 * @param[out] x The solution vector. The result of solving the system `Ax = b` using back substitution.
 */
template< std::ptrdiff_t N,
          typename MATRIX_TYPE,
          typename RHS_TYPE,
          typename SOL_TYPE >
void solveUpperTriangularSystem( MATRIX_TYPE const & A, RHS_TYPE & b, SOL_TYPE && x )
{
  for( std::ptrdiff_t i = N - 1; i >= 0; --i )
  {
    real64 sum = b[i];
    for( std::ptrdiff_t j = i + 1; j < N; ++j )
    {
      sum -= A[i][j] * x[j];
    }
    x[i] = sum / A[i][i];
  }
}

/**
 * @brief Solves a linear system using Gaussian elimination.
 *
 * This function performs Gaussian elimination on the given matrix `A` and right-hand side vector `b`.
 * It transforms the matrix `A` into an upper triangular matrix and then solves for the solution `x`
 * using back substitution.
 *
 * @tparam N The size of the square matrix `A`.
 * @tparam MATRIX_TYPE The type of the matrix `A`.
 * @tparam RHS_TYPE The type of the right-hand side vector `b`.
 * @tparam SOL_TYPE The type of the solution vector `x`.
 * @param[in,out] A The matrix to be transformed into an upper triangular matrix. Modified in place.
 * @param[in,out] b The right-hand side vector. Modified in place to reflect the transformed system.
 * @param[out] x The solution vector. The result of solving the system `Ax = b`.
 */
template< std::ptrdiff_t N,
          typename MATRIX_TYPE,
          typename RHS_TYPE,
          typename SOL_TYPE >
GEOS_HOST_DEVICE
inline
void solveGaussianElimination( MATRIX_TYPE & A, RHS_TYPE & b, SOL_TYPE && x )
{
  static_assert( N > 0, "N must be greater than 0." );
  internal::checkSizes< N, N >( matrix );
  internal::checkSizes< N >( b );
  internal::checkSizes< N >( x );


  // Step 1: Transform  into an upper triangular matrix

  // 1.a. Find the pivot
  for( std::ptrdiff_t i = 0; i < N; ++i )
  {
    std::ptrdiff_t max_row = i;
    for( std::ptrdiff_t k = i + 1; k < N; ++k )
    {
      if( std::abs( A[k][i] ) > std::abs( A[max_row][i] ))
      {
        max_row = k;
      }
    }

    // 1.b. Swap rows
    for( std::ptrdiff_t k = i; k < N; ++k )
    {
      std::swap( A[i][k], A[max_row][k] );
    }
    std::swap( b[i], b[max_row] );

    GEOS_ERROR_IF_LT_MSG( LvArray::math::abs( A[i][i] ), LvArray::NumericLimits< real64 >::epsilon, "Singular matrix." );

    // 1.c Eliminate entries below the pivot
    for( std::ptrdiff_t k = i + 1; k < N; ++k )
    {
      real64 const scaling = A[k][i] / A[i][i];
      for( std::ptrdiff_t j = i; j < N; ++j )
      {
        A[k][j] -= scaling * A[i][j];
      }
      b[k] -= scaling * b[i];
    }
  }

  // Step 2: Backward substitution
  solveUpperTriangularSystem< N >( A, b, std::forward< N >( x ) )
}

}; // internal namespace

/**
 * @brief Solves a linear system using the most appropriate method based on the size of the system.
 *
 * This function determines the appropriate method for solving a linear system `Ax = b` based on
 * the size of the matrix `A`. For 2x2 and 3x3 systems, specialized solvers are used. For larger systems,
 * Gaussian elimination is employed. The matrix and the rhs are modified by the function.
 *
 * @tparam N The size of the square matrix `A`.
 * @tparam MATRIX_TYPE The type of the matrix `A`.
 * @tparam RHS_TYPE The type of the right-hand side vector `b`.
 * @tparam SOL_TYPE The type of the solution vector `x`.
 * @param[in] A The constant matrix representing the coefficients of the system.
 * @param[in] b The constant right-hand side vector.
 * @param[out] x The solution vector. The result of solving the system `Ax = b`.
 */
template< std::ptrdiff_t N,
          typename MATRIX_TYPE,
          typename RHS_TYPE,
          typename SOL_TYPE,
          bool MODIFY_MATRIX = true >
GEOS_HOST_DEVICE
inline
void solve( MATRIX_TYPE & A, RHS_TYPE & b, SOL_TYPE && x )
{
  static_assert( N > 0, "N must be greater than 0." );
  static_assert( N < 10, "N cannot be larger than 9" );
  internal::checkSizes< N, N >( A );
  internal::checkSizes< N >( b );
  internal::checkSizes< N >( x );

  if constexpr ( N == 2 )
  {
    internal::solveTwoByTwoSystem( A, b, std::forward< SOL_TYPE >( x ) );
  }
  else if constexpr ( N == 3 )
  {
    internal::solveThreeByThreeSystem( A, b, std::forward< SOL_TYPE >( x ) );
  }
  else
  {
    if constexpr ( MODIFY_MATRIX )
    {
      internal::solveGaussianElimination< N >( A, b, std::forward< SOL_TYPE >( x ) );
    }
    else
    {
      real64[N][N] A_copy{};
      real64[N] b_copy{};

      for( std::ptrdiff_t i=0; i < N; ++j )
      {
        b_copy[i] = b[i];
        for( std::ptrdiff_t j=0; j < N; ++j )
        {
          A_copy[i][j] = A[i][j];
        }
      }
      internal::solveGaussianElimination< N >( A_copy, b_copy, std::forward< SOL_TYPE >( x ) );
    }
  }
}

};

};


#endif /*GEOS_DENSELINEARALGEBRA_DENSELASOLVERS_HPP_*/
