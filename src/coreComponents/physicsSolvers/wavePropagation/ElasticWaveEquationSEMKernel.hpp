/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file ElasticWaveEquationSEMKernel.hpp
 */

#ifndef GEOS_PHYSICSSOLVERS_WAVEPROPAGATION_ELASTICWAVEEQUATIONSEMKERNEL_HPP_
#define GEOS_PHYSICSSOLVERS_WAVEPROPAGATION_ELASTICWAVEEQUATIONSEMKERNEL_HPP_

#include "finiteElement/kernelInterface/KernelBase.hpp"
#include "WaveSolverUtils.hpp"
#include "WaveSolverBaseFields.hpp"



namespace geos
{

using namespace fields;

/// Namespace to contain the elastic wave kernels.
namespace elasticWaveEquationSEMKernels
{

struct PrecomputeSourceAndReceiverKernel
{

  /**
   * @brief Launches the precomputation of the source and receiver terms
   * @tparam EXEC_POLICY execution policy
   * @tparam FE_TYPE finite element type
   * @param[in] size the number of cells in the subRegion
   * @param[in] numFacesPerElem number of face on an element
   * @param[in] X coordinates of the nodes
   * @param[in] elemGhostRank array containing the ghost rank
   * @param[in] elemsToNodes map from element to nodes
   * @param[in] elemsToFaces map from element to faces
   * @param[in] elemCenter coordinates of the element centers
   * @param[in] faceNormal array containing the normal of all faces
   * @param[in] faceCenter array containing the center of all faces
   * @param[in] sourceCoordinates coordinates of the source terms
   * @param[in] receiverCoordinates coordinates of the receiver terms
   * @param[in] dt time-step
   * @param[in] timeSourceFrequency Peak frequency of the source
   * @param[in] rickerOrder Order of the Ricker wavelet
   * @param[out] sourceIsAccessible flag indicating whether the source is accessible or not
   * @param[out] sourceNodeIds indices of the nodes of the element where the source is located
   * @param[out] sourceConstantsx constant part of the source terms in x-direction
   * @param[out] sourceConstantsy constant part of the source terms in y-direction
   * @param[out] sourceConstantsz constant part of the source terms in z-direction
   * @param[out] receiverIsLocal flag indicating whether the receiver is local or not
   * @param[out] receiverNodeIds indices of the nodes of the element where the receiver is located
   * @param[out] receiverNodeConstants constant part of the receiver term
   * @param[out] sourceValue array containing the value of the time dependent source (Ricker for e.g)
   */
  template< typename EXEC_POLICY, typename FE_TYPE >
  static void
  launch( localIndex const size,
          localIndex const numFacesPerElem,
          arrayView2d< WaveSolverBase::wsCoordType const, nodes::REFERENCE_POSITION_USD > const X,
          arrayView1d< integer const > const elemGhostRank,
          arrayView2d< localIndex const, cells::NODE_MAP_USD > const & elemsToNodes,
          arrayView2d< localIndex const > const elemsToFaces,
          arrayView2d< real64 const > const & elemCenter,
          arrayView2d< real64 const > const faceNormal,
          arrayView2d< real64 const > const faceCenter,
          arrayView2d< real64 const > const sourceCoordinates,
          arrayView1d< localIndex > const sourceIsAccessible,
          arrayView2d< localIndex > const sourceNodeIds,
          arrayView2d< real64 > const sourceConstantsx,
          arrayView2d< real64 > const sourceConstantsy,
          arrayView2d< real64 > const sourceConstantsz,
          arrayView2d< real64 const > const receiverCoordinates,
          arrayView1d< localIndex > const receiverIsLocal,
          arrayView2d< localIndex > const receiverNodeIds,
          arrayView2d< real64 > const receiverConstants,
          arrayView2d< real32 > const sourceValue,
          real64 const dt,
          real32 const timeSourceFrequency,
          localIndex const rickerOrder,
          R1Tensor const sourceForce,
          R2SymTensor const sourceMoment )
  {

    forAll< EXEC_POLICY >( size, [=] GEOS_HOST_DEVICE ( localIndex const k )
    {

      constexpr localIndex numNodesPerElem = FE_TYPE::numNodes;

      real64 const center[3] = { elemCenter[k][0],
                                 elemCenter[k][1],
                                 elemCenter[k][2] };

      // Step 1: locate the sources, and precompute the source term

      /// loop over all the source that haven't been found yet
      for( localIndex isrc = 0; isrc < sourceCoordinates.size( 0 ); ++isrc )
      {
        if( sourceIsAccessible[isrc] == 0 )
        {
          real64 const coords[3] = { sourceCoordinates[isrc][0],
                                     sourceCoordinates[isrc][1],
                                     sourceCoordinates[isrc][2] };

          real64 xLocal[numNodesPerElem][3];

          for( localIndex a=0; a< numNodesPerElem; ++a )
          {
            for( localIndex i=0; i<3; ++i )
            {
              xLocal[a][i] = X( elemsToNodes( k, a ), i );
            }
          }


          bool const sourceFound =
            WaveSolverUtils::locateSourceElement( numFacesPerElem,
                                                  center,
                                                  faceNormal,
                                                  faceCenter,
                                                  elemsToFaces[k],
                                                  coords );

          if( sourceFound )
          {
            real64 coordsOnRefElem[3]{};


            WaveSolverUtils::computeCoordinatesOnReferenceElement< FE_TYPE >( coords,
                                                                              elemsToNodes[k],
                                                                              X,
                                                                              coordsOnRefElem );
            sourceIsAccessible[isrc] = 1;

            real64 N[FE_TYPE::numNodes];
            real64 gradN[FE_TYPE::numNodes][3];
            FE_TYPE::calcN( coordsOnRefElem, N );
            FE_TYPE::calcGradN( coordsOnRefElem, xLocal, gradN );
            R2SymTensor moment = sourceMoment;
            for( localIndex q=0; q< numNodesPerElem; ++q )
            {
              real64 inc[3] = { 0, 0, 0 };
              sourceNodeIds[isrc][q] = elemsToNodes[k][q];
              inc[0] += sourceForce[0] * N[q];
              inc[1] += sourceForce[1] * N[q];
              inc[2] += sourceForce[2] * N[q];

              LvArray::tensorOps::Ri_add_symAijBj< 3 >( inc, moment.data, gradN[q] );
              sourceConstantsx[isrc][q] += inc[0];
              sourceConstantsy[isrc][q] += inc[1];
              sourceConstantsz[isrc][q] += inc[2];
            }

            for( localIndex cycle = 0; cycle < sourceValue.size( 0 ); ++cycle )
            {
              real64 const time = cycle*dt;
              sourceValue[cycle][isrc] = WaveSolverUtils::evaluateRicker( time, timeSourceFrequency, rickerOrder );
            }

          }
        }
      } // end loop over all sources


      // Step 2: locate the receivers, and precompute the receiver term

      /// loop over all the receivers that haven't been found yet
      for( localIndex ircv = 0; ircv < receiverCoordinates.size( 0 ); ++ircv )
      {
        if( receiverIsLocal[ircv] == 0 )
        {
          real64 const coords[3] = { receiverCoordinates[ircv][0],
                                     receiverCoordinates[ircv][1],
                                     receiverCoordinates[ircv][2] };

          real64 coordsOnRefElem[3]{};
          bool const receiverFound =
            WaveSolverUtils::locateSourceElement( numFacesPerElem,
                                                  center,
                                                  faceNormal,
                                                  faceCenter,
                                                  elemsToFaces[k],
                                                  coords );

          if( receiverFound && elemGhostRank[k] < 0 )
          {
            WaveSolverUtils::computeCoordinatesOnReferenceElement< FE_TYPE >( coords,
                                                                              elemsToNodes[k],
                                                                              X,
                                                                              coordsOnRefElem );

            receiverIsLocal[ircv] = 1;

            real64 Ntest[numNodesPerElem];

            FE_TYPE::calcN( coordsOnRefElem, Ntest );

            for( localIndex a = 0; a < numNodesPerElem; ++a )
            {
              receiverNodeIds[ircv][a] = elemsToNodes[k][a];
              receiverConstants[ircv][a] = Ntest[a];
            }
          }
        }
      } // end loop over receivers
    } );

  }
};

template< typename FE_TYPE >
struct MassMatrixKernel
{

  MassMatrixKernel( FE_TYPE const & finiteElement )
    : m_finiteElement( finiteElement )
  {}

  /**
   * @brief Launches the precomputation of the mass matrices
   * @tparam EXEC_POLICY the execution policy
   * @tparam ATOMIC_POLICY the atomic policy
   * @param[in] size the number of cells in the subRegion
   * @param[in] numFacesPerElem number of faces per element
   * @param[in] X coordinates of the nodes
   * @param[in] elemsToNodes map from element to nodes
   * @param[in] density cell-wise density
   * @param[out] mass diagonal of the mass matrix
   */
  template< typename EXEC_POLICY, typename ATOMIC_POLICY >
  //std::enable_if_t< geos::is_sem_formulation< std::remove_cv_t< FE_TYPE_ > >::value, void >
  void
  launch( localIndex const size,
          arrayView2d< WaveSolverBase::wsCoordType const, nodes::REFERENCE_POSITION_USD > const X,
          arrayView2d< localIndex const, cells::NODE_MAP_USD > const elemsToNodes,
          arrayView1d< real32 const > const density,
          arrayView1d< real32 > const mass )

  {
    forAll< EXEC_POLICY >( size, [=] GEOS_HOST_DEVICE ( localIndex const k )
    {

      constexpr localIndex numNodesPerElem = FE_TYPE::numNodes;
      constexpr localIndex numQuadraturePointsPerElem = FE_TYPE::numQuadraturePoints;

      real64 xLocal[ numNodesPerElem ][ 3 ];
      for( localIndex a = 0; a < numNodesPerElem; ++a )
      {
        for( localIndex i = 0; i < 3; ++i )
        {
          xLocal[a][i] = X( elemsToNodes( k, a ), i );
        }
      }

      for( localIndex q = 0; q < numQuadraturePointsPerElem; ++q )
      {
        real32 const localIncrement = density[k] * m_finiteElement.computeMassTerm( q, xLocal );
        RAJA::atomicAdd< ATOMIC_POLICY >( &mass[elemsToNodes[k][q]], localIncrement );
      }
    } ); // end loop over element
  }

  /// The finite element space/discretization object for the element type in the subRegion
  FE_TYPE const & m_finiteElement;

};

template< typename FE_TYPE >
struct DampingMatrixKernel
{

  DampingMatrixKernel( FE_TYPE const & finiteElement )
    : m_finiteElement( finiteElement )
  {}

  /**
   * @brief Launches the precomputation of the damping matrices
   * @tparam EXEC_POLICY the execution policy
   * @tparam ATOMIC_POLICY the atomic policy
   * @param[in] size the number of cells in the subRegion
   * @param[in] X coordinates of the nodes
   * @param[in] facesToElems map from faces to elements
   * @param[in] facesToNodes map from face to nodes
   * @param[in] facesDomainBoundaryIndicator flag equal to 1 if the face is on the boundary, and to 0 otherwise
   * @param[in] freeSurfaceFaceIndicator flag equal to 1 if the face is on the free surface, and to 0 otherwise
   * @param[in] velocity cell-wise velocity
   * @param[out] damping diagonal of the damping matrix
   */
  template< typename EXEC_POLICY, typename ATOMIC_POLICY >
  //std::enable_if_t< geos::is_sem_formulation< std::remove_cv_t< FE_TYPE_ > >::value, void >
  void
  launch( localIndex const size,
          arrayView2d< WaveSolverBase::wsCoordType const, nodes::REFERENCE_POSITION_USD > const X,
          arrayView2d< localIndex const > const facesToElems,
          ArrayOfArraysView< localIndex const > const facesToNodes,
          arrayView1d< integer const > const facesDomainBoundaryIndicator,
          arrayView1d< localIndex const > const freeSurfaceFaceIndicator,
          arrayView2d< real64 const > const faceNormal,
          arrayView1d< real32 const > const density,
          arrayView1d< real32 const > const Vp,
          arrayView1d< real32 const > const Vs,
          arrayView1d< real32 > const dampingx,
          arrayView1d< real32 > const dampingy,
          arrayView1d< real32 > const dampingz )
  {
    forAll< EXEC_POLICY >( size, [=] GEOS_HOST_DEVICE ( localIndex const f )
    {
      // face on the domain boundary and not on free surface
      if( facesDomainBoundaryIndicator[f] == 1 && freeSurfaceFaceIndicator[f] != 1 )
      {
        localIndex k = facesToElems( f, 0 );
        if( k < 0 )
        {
          k = facesToElems( f, 1 );
        }

        constexpr localIndex numNodesPerFace = FE_TYPE::numNodesPerFace;

        real64 xLocal[ numNodesPerFace ][ 3 ];
        for( localIndex a = 0; a < numNodesPerFace; ++a )
        {
          for( localIndex i = 0; i < 3; ++i )
          {
            xLocal[a][i] = X( facesToNodes( f, a ), i );
          }
        }

        for( localIndex q = 0; q < numNodesPerFace; ++q )
        {
          real32 const localIncrementx = density[k] * (Vp[k]*LvArray::math::abs( faceNormal[f][0] ) + Vs[k]*sqrt( faceNormal[f][1]*faceNormal[f][1] +
                                                                                                                  faceNormal[f][2]*faceNormal[f][2] ) )* m_finiteElement.computeDampingTerm(
            q,
            xLocal );
          real32 const localIncrementy = density[k] * (Vp[k]*LvArray::math::abs( faceNormal[f][1] ) + Vs[k]*sqrt( faceNormal[f][0]*faceNormal[f][0] +
                                                                                                                  faceNormal[f][2]*faceNormal[f][2] ) )* m_finiteElement.computeDampingTerm(
            q,
            xLocal );
          real32 const localIncrementz = density[k] * (Vp[k]*LvArray::math::abs( faceNormal[f][2] ) + Vs[k]*sqrt( faceNormal[f][0]*faceNormal[f][0] +
                                                                                                                  faceNormal[f][1]*faceNormal[f][1] ) )*  m_finiteElement.computeDampingTerm(
            q,
            xLocal );

          RAJA::atomicAdd< ATOMIC_POLICY >( &dampingx[facesToNodes[f][q]], localIncrementx );
          RAJA::atomicAdd< ATOMIC_POLICY >( &dampingy[facesToNodes[f][q]], localIncrementy );
          RAJA::atomicAdd< ATOMIC_POLICY >( &dampingz[facesToNodes[f][q]], localIncrementz );
        }
      }
    } ); // end loop over element
  }

  /// The finite element space/discretization object for the element type in the subRegion
  FE_TYPE const & m_finiteElement;

};

/**
 * @brief Implements kernels for solving the elastic wave equations
 *   explicit central FD method and SEM
 * @copydoc geos::finiteElement::KernelBase
 * @tparam SUBREGION_TYPE The type of subregion that the kernel will act on.
 *
 * ### ElasticWaveEquationSEMKernel Description
 * Implements the KernelBase interface functions required for solving
 * the acoustic wave equations using the
 * "finite element kernel application" functions such as
 * geos::finiteElement::RegionBasedKernelApplication.
 *
 * The number of degrees of freedom per support point for both
 * the test and trial spaces are specified as `1`.
 */


template< typename SUBREGION_TYPE,
          typename CONSTITUTIVE_TYPE,
          typename FE_TYPE >
class ExplicitElasticSEM : public finiteElement::KernelBase< SUBREGION_TYPE,
                                                             CONSTITUTIVE_TYPE,
                                                             FE_TYPE,
                                                             1,
                                                             1 >
{
public:

  /// Alias for the base class;
  using Base = finiteElement::KernelBase< SUBREGION_TYPE,
                                          CONSTITUTIVE_TYPE,
                                          FE_TYPE,
                                          1,
                                          1 >;

  /// Number of nodes per element...which is equal to the
  /// numTestSupportPointPerElem and numTrialSupportPointPerElem by definition.
  static constexpr int numNodesPerElem = Base::maxNumTestSupportPointsPerElem;

  using Base::numDofPerTestSupportPoint;
  using Base::numDofPerTrialSupportPoint;
  using Base::m_elemsToNodes;
  using Base::m_elemGhostRank;
  using Base::m_constitutiveUpdate;
  using Base::m_finiteElementSpace;

//*****************************************************************************
  /**
   * @brief Constructor
   * @copydoc geos::finiteElement::KernelBase::KernelBase
   * @param nodeManager Reference to the NodeManager object.
   * @param edgeManager Reference to the EdgeManager object.
   * @param faceManager Reference to the FaceManager object.
   * @param targetRegionIndex Index of the region the subregion belongs to.
   * @param dt The time interval for the step.
   */
  ExplicitElasticSEM( NodeManager & nodeManager,
                      EdgeManager const & edgeManager,
                      FaceManager const & faceManager,
                      localIndex const targetRegionIndex,
                      SUBREGION_TYPE const & elementSubRegion,
                      FE_TYPE const & finiteElementSpace,
                      CONSTITUTIVE_TYPE & inputConstitutiveType,
                      real64 const dt ):
    Base( elementSubRegion,
          finiteElementSpace,
          inputConstitutiveType ),
    m_X( nodeManager.getField< fields::referencePosition32 >() ),
    m_ux_n( nodeManager.getField< elasticSecondOrderSemFields::Displacementx_n >() ),
    m_uy_n( nodeManager.getField< elasticSecondOrderSemFields::Displacementy_n >() ),
    m_uz_n( nodeManager.getField< elasticSecondOrderSemFields::Displacementz_n >() ),
    m_stiffnessVectorx( nodeManager.getField< matricialFields::StiffnessVectorx >() ),
    m_stiffnessVectory( nodeManager.getField< matricialFields::StiffnessVectory >() ),
    m_stiffnessVectorz( nodeManager.getField< matricialFields::StiffnessVectorz >() ),
    m_density( elementSubRegion.template getField< geophysicalFields::Density >() ),
    m_Vp( elementSubRegion.template getField< geophysicalFields::Pwavespeed >() ),
    m_Vs( elementSubRegion.template getField< geophysicalFields::Swavespeed >() ),
    m_dt( dt )
  {
    GEOS_UNUSED_VAR( edgeManager );
    GEOS_UNUSED_VAR( faceManager );
    GEOS_UNUSED_VAR( targetRegionIndex );
  }



  //*****************************************************************************
  /**
   * @copydoc geos::finiteElement::KernelBase::StackVariables
   *
   * ### ExplicitElasticSEM Description
   * Adds a stack arrays for the nodal force, primary displacement variable, etc.
   */
  struct StackVariables : Base::StackVariables
  {
public:
    GEOS_HOST_DEVICE
    StackVariables():
      xLocal()
    {}
    /// C-array stack storage for element local the nodal positions.
    real64 xLocal[ numNodesPerElem ][ 3 ]{};
    real32 mu=0;
    real32 lambda=0;
  };
  //***************************************************************************


  /**
   * @copydoc geos::finiteElement::KernelBase::setup
   *
   * Copies the primary variable, and position into the local stack array.
   */
  GEOS_HOST_DEVICE
  inline
  void setup( localIndex const k,
              StackVariables & stack ) const
  {
    /// numDofPerTrialSupportPoint = 1
    for( localIndex a=0; a< numNodesPerElem; ++a )
    {
      localIndex const nodeIndex = m_elemsToNodes( k, a );
      for( int i=0; i< 3; ++i )
      {
        stack.xLocal[ a ][ i ] = m_X[ nodeIndex ][ i ];
      }
    }
    stack.mu = m_density[k] * m_Vs[k] * m_Vs[k];
    stack.lambda = m_density[k] *m_Vp[k] * m_Vp[k] - 2.0*stack.mu;
  }

  /**
   * @copydoc geos::finiteElement::KernelBase::quadraturePointKernel
   *
   * ### ExplicitElasticSEM Description
   * Calculates stiffness vector
   *
   */
  GEOS_HOST_DEVICE
  inline
  void quadraturePointKernel( localIndex const k,
                              localIndex const q,
                              StackVariables & stack ) const
  {

    m_finiteElementSpace.template computeFirstOrderStiffnessTerm( q, stack.xLocal, [&] ( int i, int j, real64 val, real64 J[3][3], int p, int r )
    {
      real32 const Rxx_ij = val*((stack.lambda+2.0*stack.mu)*J[p][0]*J[r][0]+stack.mu*(J[p][1]*J[r][1]+J[p][2]*J[r][2]));
      real32 const Ryy_ij = val*((stack.lambda+2.0*stack.mu)*J[p][1]*J[r][1]+stack.mu*(J[p][0]*J[r][0]+J[p][2]*J[r][2]));
      real32 const Rzz_ij = val*((stack.lambda+2.0*stack.mu)*J[p][2]*J[r][2]+stack.mu*(J[p][0]*J[r][0]+J[p][1]*J[r][1]));
      real32 const Rxy_ij = val*(stack.lambda*J[p][0]*J[r][1]+stack.mu*J[p][1]*J[r][0]);
      real32 const Ryx_ij = val*(stack.mu*J[p][0]*J[r][1]+stack.lambda*J[p][1]*J[r][0]);
      real32 const Rxz_ij = val*(stack.lambda*J[p][0]*J[r][2]+stack.mu*J[p][2]*J[r][0]);
      real32 const Rzx_ij = val*(stack.mu*J[p][0]*J[r][2]+stack.lambda*J[p][2]*J[r][0]);
      real32 const Ryz_ij = val*(stack.lambda*J[p][1]*J[r][2]+stack.mu*J[p][2]*J[r][1]);
      real32 const Rzy_ij = val*(stack.mu*J[p][1]*J[r][2]+stack.lambda*J[p][2]*J[r][1]);

      real32 const localIncrementx = (Rxx_ij * m_ux_n[m_elemsToNodes[k][j]] + Rxy_ij*m_uy_n[m_elemsToNodes[k][j]] + Rxz_ij*m_uz_n[m_elemsToNodes[k][j]]);
      real32 const localIncrementy = (Ryx_ij * m_ux_n[m_elemsToNodes[k][j]] + Ryy_ij*m_uy_n[m_elemsToNodes[k][j]] + Ryz_ij*m_uz_n[m_elemsToNodes[k][j]]);
      real32 const localIncrementz = (Rzx_ij * m_ux_n[m_elemsToNodes[k][j]] + Rzy_ij*m_uy_n[m_elemsToNodes[k][j]] + Rzz_ij*m_uz_n[m_elemsToNodes[k][j]]);

      RAJA::atomicAdd< parallelDeviceAtomic >( &m_stiffnessVectorx[m_elemsToNodes[k][i]], localIncrementx );
      RAJA::atomicAdd< parallelDeviceAtomic >( &m_stiffnessVectory[m_elemsToNodes[k][i]], localIncrementy );
      RAJA::atomicAdd< parallelDeviceAtomic >( &m_stiffnessVectorz[m_elemsToNodes[k][i]], localIncrementz );
    } );
  }


protected:
  /// The array containing the nodal position array.
  arrayView2d< WaveSolverBase::wsCoordType const, nodes::REFERENCE_POSITION_USD > const m_X;

  /// The array containing the nodal displacement array in x direction.
  arrayView1d< real32 > const m_ux_n;

  /// The array containing the nodal displacement array in y direction.
  arrayView1d< real32 > const m_uy_n;

  /// The array containing the nodal displacement array in z direction.
  arrayView1d< real32 > const m_uz_n;

  /// The array containing the product of the stiffness matrix and the nodal pressure.
  arrayView1d< real32 > const m_stiffnessVectorx;

  /// The array containing the product of the stiffness matrix and the nodal pressure.
  arrayView1d< real32 > const m_stiffnessVectory;

  /// The array containing the product of the stiffness matrix and the nodal pressure.
  arrayView1d< real32 > const m_stiffnessVectorz;

  /// The array containing the density of the medium
  arrayView1d< real32 const > const m_density;

  /// The array containing the P-wavespeed
  arrayView1d< real32 const > const m_Vp;

  /// The array containing the S-wavespeed
  arrayView1d< real32 const > const m_Vs;

  /// The time increment for this time integration step.
  real64 const m_dt;


};


/// The factory used to construct a ExplicitAcousticWaveEquation kernel.
using ExplicitElasticSEMFactory = finiteElement::KernelFactory< ExplicitElasticSEM,
                                                                real64 >;

} // namespace ElasticWaveEquationSEMKernels

} // namespace geos

#endif //GEOS_PHYSICSSOLVERS_WAVEPROPAGATION_ELASTICWAVEEQUATIONSEMKERNEL_HPP_
