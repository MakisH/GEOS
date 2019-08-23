/*
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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

/**
 * @file SolidMechanicsLagrangianFEM.hpp
 */

#ifndef SOLID_MECHANICS_LAGRANGIAN_FEM_HPP_
#define SOLID_MECHANICS_LAGRANGIAN_FEM_HPP_

#include "physicsSolvers/SolverBase.hpp"

#include "common/TimingMacros.hpp"
#include "mesh/MeshForLoopInterface.hpp"
#include "MPI_Communications/CommunicationTools.hpp"
#include "SolidMechanicsLagrangianFEMKernels.hpp"


namespace geosx
{
namespace dataRepository
{
class ManagedGroup;
}
class FieldSpecificationBase;
class FiniteElementBase;
class DomainPartition;

/**
 * @class SolidMechanicsLagrangianFEM
 *
 * This class implements a finite element solution to the equations of motion.
 */
class SolidMechanicsLagrangianFEM : public SolverBase
{
public:

  /**
   * Constructor
   * @param name The name of the solver instance
   * @param parent the parent group of the solver
   */
  SolidMechanicsLagrangianFEM( const std::string& name,
                               ManagedGroup * const parent );


  SolidMechanicsLagrangianFEM( SolidMechanicsLagrangianFEM const & ) = delete;
  SolidMechanicsLagrangianFEM( SolidMechanicsLagrangianFEM && ) = default ;

  SolidMechanicsLagrangianFEM& operator=( SolidMechanicsLagrangianFEM const & ) = delete;
  SolidMechanicsLagrangianFEM& operator=( SolidMechanicsLagrangianFEM && ) = delete ;

  /**
   * destructor
   */
  virtual ~SolidMechanicsLagrangianFEM() override;

  /**
   * @return The string that may be used to generate a new instance from the SolverBase::CatalogInterface::CatalogType
   */
  static string CatalogName() { return "SolidMechanics_LagrangianFEM"; }

  virtual void InitializePreSubGroups(ManagedGroup * const rootGroup) override;

  virtual void RegisterDataOnMesh( ManagedGroup * const MeshBody ) override final;

  void updateIntrinsicNodalData( DomainPartition * const domain );

  /**
   * @defgroup Solver Interface Functions
   *
   * These functions provide the primary interface that is required for derived classes
   */
  /**@{*/
  virtual
  real64 SolverStep( real64 const& time_n,
                             real64 const & dt,
                             integer const cycleNumber,
                             DomainPartition * domain ) override;

  virtual
  real64 ExplicitStep( real64 const& time_n,
                               real64 const & dt,
                               integer const cycleNumber,
                               DomainPartition * domain ) override;

  virtual void
  ImplicitStepSetup( real64 const & time_n,
                     real64 const & dt,
                     DomainPartition * const domain,
                     DofManager & dofManager,
                     ParallelMatrix & matrix,
                     ParallelVector & rhs,
                     ParallelVector & solution ) override;

  virtual void
  AssembleSystem( real64 const time,
                  real64 const dt,
                  DomainPartition * const domain,
                  DofManager const & dofManager,
                  ParallelMatrix & matrix,
                  ParallelVector & rhs ) override;

  virtual void
  SolveSystem( DofManager const & dofManager,
               ParallelMatrix & matrix,
               ParallelVector & rhs,
               ParallelVector & solution ) override;

  virtual void
  ApplySystemSolution( DofManager const & dofManager,
                       ParallelVector const & solution,
                       real64 const scalingFactor,
                       DomainPartition * const domain ) override;

  virtual void ApplyBoundaryConditions( real64 const time,
                                        real64 const dt,
                                        DomainPartition * const domain,
                                        DofManager const & dofManager,
                                        ParallelMatrix & matrix,
                                        ParallelVector & rhs ) override;

  virtual real64
  CalculateResidualNorm( DomainPartition const * const domain,
                         DofManager const & dofManager,
                         ParallelVector const & rhs ) override;

  virtual void ResetStateToBeginningOfStep( DomainPartition * const domain ) override;

  virtual void ImplicitStepComplete( real64 const & time,
                                     real64 const & dt,
                                     DomainPartition * const domain ) override;

  /**@}*/

  /**
   * Function to setup/allocate the linear system blocks
   * @param domain The DomainPartition object.
   * @param blockSystem the block system object to that holds the blocks that will be constructed
   */
  void SetupSystem( DomainPartition * const domain,
                    DofManager & dofManager,
                    ParallelMatrix & matrix,
                    ParallelVector & rhs,
                    ParallelVector & solution );

  /**
   * Function to set the sparsity pattern
   * @param domain The DomainPartition object
   * @param sparsity Pointer to the the sparsity graph
   */
  void SetSparsityPattern( DomainPartition const * const domain,
                           ParallelMatrix * const matrix );

  /**
   * Function to set the global DOF numbers  for this solver.
   * @param domain The DomainPartition object
   * @param numLocalRows The number of local rows on this process
   * @param numGlobalRows The number of global rows in the system
   * @param localIndices The local indices associated with a local row??
   * @param offset The offset for the row/globalDOF
   */
  void SetNumRowsAndTrilinosIndices( ManagedGroup * const domain,
                                     localIndex & numLocalRows,
                                     globalIndex & numGlobalRows,
                                     localIndex offset ) const;

  /**
   * @brief Launch of the element processing kernel for explicit time integration.
   * @param NUM_NODES_PER_ELEM The number of nodes/dof per element.
   * @param NUM_QUADRATURE_POINTS The number of quadrature points per element.
   * @param constitutiveRelation A pointer to the constitutive relation that is being used.
   * @param elementList The list of elements to be processed
   * @param elemsToNodes The map from the elements to the nodes that form that element.
   * @param dNdX The derivatives of the shape functions wrt the reference configuration.
   * @param detJ The determinant of the transformation matrix (Jacobian) to the parent element.
   * @param u The nodal array of total displacements.
   * @param vel The nodal array of velocity.
   * @param acc The nodal array of force/acceleration.
   * @param meanStress The mean stress at each element quadrature point
   * @param devStress The deviator stress at each element quadrature point.
   * @param dt The timestep
   * @return The achieved timestep.
   */
  virtual real64
  ExplicitElementKernelLaunch( localIndex NUM_NODES_PER_ELEM,
                               localIndex NUM_QUADRATURE_POINTS,
                               constitutive::ConstitutiveBase * const constitutiveRelation,
                               set<localIndex> const & elementList,
                               arrayView2d<localIndex const> const & elemsToNodes,
                               arrayView3d< R1Tensor const> const & dNdX,
                               arrayView2d<real64 const> const & detJ,
                               arrayView1d<R1Tensor const> const & u,
                               arrayView1d<R1Tensor const> const & vel,
                               arrayView1d<R1Tensor> const & acc,
                               arrayView2d<real64> const & meanStress,
                               arrayView2d<R2SymTensor> const & devStress,
                               real64 const dt ) const
  {
    using ExplicitKernel = SolidMechanicsLagrangianFEMKernels::ExplicitKernel;
    return SolidMechanicsLagrangianFEMKernels::
           ElementKernelLaunchSelector<ExplicitKernel>( NUM_NODES_PER_ELEM,
                                                        NUM_QUADRATURE_POINTS,
                                                        constitutiveRelation,
                                                        elementList,
                                                        elemsToNodes,
                                                        dNdX,
                                                        detJ,
                                                        u,
                                                        vel,
                                                        acc,
                                                        meanStress,
                                                        devStress,
                                                        dt );
  }

  /**
   * @brief Launch of the element processing kernel for implicit time integration.
   * @tparam NUM_NODES_PER_ELEM The number of nodes/dof per element.
   * @tparam NUM_QUADRATURE_POINTS The number of quadrature points per element.
   * @tparam CONSTITUTIVE_TYPE the type of the constitutive relation that is being used.
   * @param constitutiveRelation A pointer to the constitutive relation that is being used.
   * @param numElems The number of elements the kernel will process.
   * @param dt The timestep.
   * @param dNdX The derivatives of the shape functions wrt the reference configuration.
   * @param detJ The determinant of the transformation matrix (Jacobian) to the parent element.
   * @param fe A pointer to the finite element class used in this kernel.
   * @param elemGhostRank An array containing the values of the owning ranks for ghost elements.
   * @param elemsToNodes The map from the elements to the nodes that form that element.
   * @param globalDofNumber The map from localIndex to the globalDOF number.
   * @param disp The array of total displacements.
   * @param uhat The array of incremental displacements (displacement for this step).
   * @param vtilde The array for the velocity predictor.
   * @param uhattilde The array for the incremental displacement predictor.
   * @param density The array containing the density
   * @param fluidPressure Array containing element fluid pressure at the beginning of the step.
   * @param deltaFluidPressure Array containing the change in element fluid pressure over this step.
   * @param biotCoefficient The biotCoefficient used to calculate effective stress.
   * @param tiOption The time integration option used for the integration.
   * @param stiffnessDamping The stiffness damping coefficient for the Newmark method assuming Rayleigh damping.
   * @param massDamping The mass damping coefficient for the Newmark method assuming Rayleigh damping.
   * @param newmarkBeta The value of \beta in the Newmark update.
   * @param newmarkGamma The value of \gamma in the Newmark update.
   * @param dofManager degree-of-freedom manager associated with the linear system
   * @param matrix sparse matrix containing the derivatives of the residual wrt displacement
   * @param rhs parallel vector containing the global residual
   * @return The maximum nodal force contribution from all elements.
   */
  virtual real64
  ImplicitElementKernelLaunch( localIndex NUM_NODES_PER_ELEM,
                               localIndex NUM_QUADRATURE_POINTS,
                               constitutive::ConstitutiveBase * const constitutiveRelation,
                               localIndex const numElems,
                               real64 const dt,
                               arrayView3d<R1Tensor const> const & dNdX,
                               arrayView2d<real64 const > const& detJ,
                               FiniteElementBase const * const fe,
                               arrayView1d< integer const > const & elemGhostRank,
                               arrayView2d< localIndex const > const & elemsToNodes,
                               arrayView1d< globalIndex const > const & globalDofNumber,
                               arrayView1d< R1Tensor const > const & disp,
                               arrayView1d< R1Tensor const > const & uhat,
                               arrayView1d< R1Tensor const > const & vtilde,
                               arrayView1d< R1Tensor const > const & uhattilde,
                               arrayView1d< real64 const > const & density,
                               arrayView1d< real64 const > const & fluidPressure,
                               arrayView1d< real64 const > const & deltaFluidPressure,
                               arrayView1d< real64 const > const & biotCoefficient,
                               timeIntegrationOption const tiOption,
                               real64 const stiffnessDamping,
                               real64 const massDamping,
                               real64 const newmarkBeta,
                               real64 const newmarkGamma,
                               DofManager const * const dofManager,
                               ParallelMatrix * const matrix,
                               ParallelVector * const rhs ) const
  {
    using ImplicitKernel = SolidMechanicsLagrangianFEMKernels::ImplicitKernel;
    return SolidMechanicsLagrangianFEMKernels::
           ElementKernelLaunchSelector<ImplicitKernel>( NUM_NODES_PER_ELEM,
                                                        NUM_QUADRATURE_POINTS,
                                                        constitutiveRelation,
                                                        numElems,
                                                        dt,
                                                        dNdX,
                                                        detJ,
                                                        fe,
                                                        elemGhostRank,
                                                        elemsToNodes,
                                                        globalDofNumber,
                                                        disp,
                                                        uhat,
                                                        vtilde,
                                                        uhattilde,
                                                        density,
                                                        fluidPressure,
                                                        deltaFluidPressure,
                                                        biotCoefficient,
                                                        tiOption,
                                                        stiffnessDamping,
                                                        massDamping,
                                                        newmarkBeta,
                                                        newmarkGamma,
                                                        dofManager,
                                                        matrix,
                                                        rhs );
  }

  /**
   * Applies displacement boundary conditions to the system for implicit time integration
   * @param time The time to use for any lookups associated with this BC
   * @param dofManager degree-of-freedom manager associated with the linear system
   * @param domain The DomainPartition.
   * @param matrix the system matrix
   * @param rhs the system right-hand side vector
   * @param solution the solution vector
   */
  void ApplyDisplacementBC_implicit( real64 const time,
                                     DofManager const & dofManager,
                                     DomainPartition & domain,
                                     ParallelMatrix & matrix,
                                     ParallelVector & rhs );


  void ApplyTractionBC( real64 const time,
                        DofManager const & dofManager,
                        DomainPartition * const domain,
                        ParallelVector & rhs );

  void ApplyChomboPressure( DofManager const & dofManager,
                            DomainPartition * const domain,
                            ParallelVector & rhs );


  void ApplyContactConstraint( DofManager const & dofManager,
                               DomainPartition & domain,
                               ParallelMatrix * const matrix,
                               ParallelVector * const rhs );

  void SetTimeIntegrationOption( string const & stringVal )
  {
    if( stringVal == "ExplicitDynamic" )
    {
      this->m_timeIntegrationOption = timeIntegrationOption::ExplicitDynamic;
    }
    else if( stringVal == "ImplicitDynamic" )
    {
      this->m_timeIntegrationOption = timeIntegrationOption::ImplicitDynamic;
    }
    else if ( stringVal == "QuasiStatic" )
    {
      this->m_timeIntegrationOption = timeIntegrationOption::QuasiStatic;
    }
    else
    {
      GEOS_ERROR("Invalid time integration option: " << stringVal);
    }
  }

  struct viewKeyStruct : SolverBase::viewKeyStruct
  {
    static constexpr auto vTildeString = "velocityTilde";
    static constexpr auto uhatTildeString = "uhatTilde";
    static constexpr auto cflFactorString = "cflFactor";
    static constexpr auto newmarkGammaString = "newmarkGamma";
    static constexpr auto newmarkBetaString = "newmarkBeta";
    static constexpr auto massDampingString = "massDamping";
    static constexpr auto stiffnessDampingString = "stiffnessDamping";
    static constexpr auto useVelocityEstimateForQSString = "useVelocityForQS";
    static constexpr auto timeIntegrationOptionStringString = "timeIntegrationOption";
    static constexpr auto timeIntegrationOptionString = "timeIntegrationOptionEnum";
    static constexpr auto maxNumResolvesString = "maxNumResolves";
    static constexpr auto strainTheoryString = "strainTheory";
    static constexpr auto solidMaterialNameString = "solidMaterialName";
    static constexpr auto solidMaterialFullIndexString = "solidMaterialFullIndex";
    static constexpr auto forceExternal = "externalForce";
    static constexpr auto contactRelationNameString = "contactRelationName";
    static constexpr auto contactForceString = "contactForce";

    dataRepository::ViewKey vTilde = { vTildeString };
    dataRepository::ViewKey uhatTilde = { uhatTildeString };
    dataRepository::ViewKey newmarkGamma = { newmarkGammaString };
    dataRepository::ViewKey newmarkBeta = { newmarkBetaString };
    dataRepository::ViewKey massDamping = { massDampingString };
    dataRepository::ViewKey stiffnessDamping = { stiffnessDampingString };
    dataRepository::ViewKey useVelocityEstimateForQS = { useVelocityEstimateForQSString };
    dataRepository::ViewKey globalDofNumber = { globalDofNumberString };
    dataRepository::ViewKey timeIntegrationOption = { timeIntegrationOptionString };
  } solidMechanicsViewKeys;

  struct groupKeyStruct
  {
    dataRepository::GroupKey systemSolverParameters = { "SystemSolverParameters" };
  } solidMechanicsGroupKeys;

protected:
  virtual void PostProcessInput() override final;

  virtual void InitializePostInitialConditions_PreSubGroups( dataRepository::ManagedGroup * const problemManager ) override final;

  real64 m_newmarkGamma;
  real64 m_newmarkBeta;
  real64 m_massDamping;
  real64 m_stiffnessDamping;
  string m_timeIntegrationOptionString;
  timeIntegrationOption m_timeIntegrationOption;
  integer m_useVelocityEstimateForQS;
  real64 m_maxForce = 0.0;
  integer m_maxNumResolves;
  integer m_strainTheory;
  string m_solidMaterialName;
  localIndex m_solidMaterialFullIndex;
  string m_contactRelationName;


  array1d< array1d < set<localIndex> > > m_elemsAttachedToSendOrReceiveNodes;
  array1d< array1d < set<localIndex> > > m_elemsNotAttachedToSendOrReceiveNodes;
  set<localIndex> m_sendOrReceiveNodes;
  set<localIndex> m_nonSendOrReceiveNodes;
  MPI_iCommData m_iComm;

  SolidMechanicsLagrangianFEM();

};

//**********************************************************************************************************************
//**********************************************************************************************************************
//**********************************************************************************************************************

} /* namespace geosx */

#endif /* SOLID_MECHANICS_LAGRANGIAN_FEM_HPP_ */
