/*
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Copyright (c) 2018, Lawrence Livermore National Security, LLC.
 *
 * Produced at the Lawrence Livermore National Laboratory
 *
 * LLNL-CODE-746361
 *
 * All rights reserved. See COPYRIGHT for details.
 *
 * This file is part of the GEOSX Simulation Framework.
 *
 * GEOSX is a free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the
 * Free Software Foundation) version 2.1 dated February 1999.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/*
 * CGsolver.hpp
 *
 *  Created on: Sep 12, 2018
 *      Author: Matthias
 */

#ifndef SRC_CORECOMPONENTS_LINEARALGEBRAINTERFACE_SRC_CGSOLVER_HPP_
#define SRC_CORECOMPONENTS_LINEARALGEBRAINTERFACE_SRC_CGSOLVER_HPP_

#include "TrilinosInterface.hpp"
//#include "HypreInterface.hpp"

namespace geosx
{

/**
 * \class BlockLinearSolvers
 * \brief This class creates and provides basic support for block
 *        linear solvers (templated on the LA interface).
 */

template< typename LAI >
class CGsolver
{

  using ParallelMatrix = typename LAI::ParallelMatrix;
  using ParallelVector = typename LAI::ParallelVector;

public:

  //! @name Constructor/Destructor Methods
  //@{
  /**
   * @brief Empty matrix constructor.
   *
   * Create an empty block matrix.
   */
  CGsolver();

  /**
   * @brief Virtual destructor.
   */
  virtual ~CGsolver() = default;
  //@}

  void solve( ParallelMatrix const &A,
              ParallelVector &x,
              ParallelVector const &b,
              ParallelMatrix const &M );

  void solve( BlockMatrixView<LAI> const &A,
              BlockVectorView<LAI> &x,
              BlockVectorView<LAI> const &b,
              BlockMatrixView<LAI> const &M );

private:

};

// Empty constructor
template< typename LAI >
CGsolver<LAI>::CGsolver()
{}

template< typename LAI >
void CGsolver<LAI>::solve( typename LAI::ParallelMatrix const &A,
                           typename LAI::ParallelVector &x,
                           typename LAI::ParallelVector const &b,
                           typename LAI::ParallelMatrix const &M )

{

  // Get the global size
  typename LAI::laiGID N = x.globalSize();

  // Placeholder for the number of iterations
  typename LAI::laiGID numIt = 0;

  // Define vectors
  ParallelVector rk( x );

  // Compute initial rk
  A.residual( x, b, rk );

  // Preconditioning
  ParallelVector zk( x );
  M.multiply( rk, zk );

  // pk = zk
  ParallelVector pk( zk );
  ParallelVector Apk( zk );

  real64 alpha, beta;

  real64 convCheck;
  rk.norm2( convCheck );

  for( typename LAI::laiGID k = 0 ; k < N ; k++ )
  {
    // Compute rkT.rk
    rk.dot( zk, &alpha );

    // Compute Apk
    A.multiply( pk, Apk );

    // compute alpha
    real64 temp;
    pk.dot( Apk, &temp );
    alpha = alpha/temp;

    // Update x
    x.update( alpha, pk, 1.0 );

    // Update rk
    ParallelVector rkold( rk );
    ParallelVector zkold( zk );
    rk.update( -alpha, Apk, 1.0 );

    // Convergence check
    rk.norm2( convCheck );

    if( convCheck < 1e-8 )
    {
      numIt = k;
      break;
    }

    M.multiply( rk, zk );

    zk.dot( rk, &beta );
    zkold.dot( rkold, &temp );
    beta = beta/temp;

    // Update pk
    pk.update( 1.0, zk, beta );

    //std::cout << k << ", " << convCheck << std::endl;

  }

  // Get the MPI rank
  int rank;
  MPI_Comm_rank( MPI_COMM_WORLD, &rank );
  if ( rank == 1 )
    std::cout << "CG converged in " << numIt << " iterations." << std::endl;
  return;

}

template< typename LAI >
void CGsolver<LAI>::solve( BlockMatrixView<LAI> const &A,
                           BlockVectorView<LAI> &x,
                           BlockVectorView<LAI> const &b,
                           BlockMatrixView<LAI> const &M )

{

  // Get the global size
  typename LAI::laiGID N = x.globalSize();

  // Placeholder for the number of iterations
  typename LAI::laiGID numIt = 0;

  // Define vectors
  BlockVectorView<LAI> rk( x );

  // Compute initial rk
  A.residual( x, b, rk );

  // Preconditioning
  BlockVectorView<LAI> zk( x );
  M.multiply( rk, zk );

  // pk = zk
  BlockVectorView<LAI> pk( zk );
  BlockVectorView<LAI> Apk( zk );

  real64 alpha, beta;

  real64 convCheck;
  rk.norm2( convCheck );

  for( typename LAI::laiGID k = 0 ; k < N ; k++ )
  {
    // Compute rkT.rk
    rk.dot( zk, alpha );

    // Compute Apk
    A.multiply( pk, Apk );

    // compute alpha
    real64 temp;
    pk.dot( Apk, temp );
    alpha = alpha/temp;

    // Update x
    x.update( alpha, pk, 1.0 );

    // Update rk
    BlockVectorView<LAI> rkold( rk );
    BlockVectorView<LAI> zkold( zk );
    rk.update( -alpha, Apk, 1.0 );

    // Convergence check
    rk.norm2( convCheck );

    if( convCheck < 1e-8 )
    {
      numIt = k;
      break;
    }

    M.multiply( rk, zk );

    zk.dot( rk, beta );
    zkold.dot( rkold, temp );
    beta = beta/temp;

    // Update pk
    pk.update( 1.0, zk, beta );

    //std::cout << k << ", " << convCheck << std::endl;

  }

  // Get the MPI rank
  int rank;
  MPI_Comm_rank( MPI_COMM_WORLD, &rank );
  if ( rank == 1 )
    std::cout << "CG converged in " << numIt << " iterations." << std::endl;
  return;

}

} // namespace GEOSX

#endif /* SRC_EXTERNALCOMPONENTS_LINEARALGEBRAINTERFACE_SRC_CGSOLVER_HPP_ */
