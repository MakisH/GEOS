// Copyright (c) 2018, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-746361. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the GEOSX Simulation Framework.

//
// GEOSX is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

/**
 * @file SinglePhaseFlow_TPFA.cpp
 */

#include "SinglePhaseFlow_TPFA.hpp"

#include <vector>
#include <math.h>


#include "codingUtilities/Utilities.hpp"
#include "common/DataTypes.hpp"
#include "common/TimingMacros.hpp"
#include "constitutive/ConstitutiveManager.hpp"
#include "dataRepository/ManagedGroup.hpp"
#include "finiteElement/FiniteElementManager.hpp"
#include "finiteElement/FiniteElementSpaceManager.hpp"
#include "finiteElement/ElementLibrary/FiniteElement.h"
#include "managers/DomainPartition.hpp"
#include "mesh/MeshForLoopInterface.hpp"
#include "meshUtilities/ComputationalGeometry.hpp"
#include "MPI_Communications/CommunicationTools.hpp"
#include "physicsSolvers/BoundaryConditions/BoundaryConditionManager.hpp"

/**
 * @namespace the geosx namespace that encapsulates the majority of the code
 */
namespace geosx
{

using namespace dataRepository;
using namespace constitutive;
using namespace systemSolverInterface;


SinglePhaseFlow_TPFA::SinglePhaseFlow_TPFA( const std::string& name,
                                            ManagedGroup * const parent ):
  SolverBase( name, parent )
{
  this->RegisterGroup<SystemSolverParameters>( groupKeys.systemSolverParameters.Key() );
  m_linearSystem.SetBlockID( EpetraBlockSystem::BlockIDs::fluidPressureBlock, this->getName() );
}


void SinglePhaseFlow_TPFA::FillDocumentationNode(  )
{
  cxx_utilities::DocumentationNode * const docNode = this->getDocumentationNode();
  SolverBase::FillDocumentationNode();

  docNode->setName(this->CatalogName());
  docNode->setSchemaType("Node");
  docNode->setShortDescription("An example solid mechanics solver");


  docNode->AllocateChildNode( viewKeys.functionalSpace.Key(),
                              viewKeys.functionalSpace.Key(),
                              -1,
                              "string",
                              "string",
                              "name of field variable",
                              "name of field variable",
                              "Pressure",
                              "",
                              0,
                              1,
                              0 );
}

void SinglePhaseFlow_TPFA::FillOtherDocumentationNodes( dataRepository::ManagedGroup * const rootGroup )
{
  DomainPartition * domain  = rootGroup->GetGroup<DomainPartition>(keys::domain);

  for( auto & mesh : domain->getMeshBodies()->GetSubGroups() )
  {
    MeshLevel * meshLevel = ManagedGroup::group_cast<MeshBody*>(mesh.second)->getMeshLevel(0);

    {
      FaceManager * const faceManager = meshLevel->getFaceManager();
      cxx_utilities::DocumentationNode * const docNode = faceManager->getDocumentationNode();

      docNode->AllocateChildNode( viewKeyStruct::faceAreaString,
                                  viewKeyStruct::faceAreaString,
                                  -1,
                                  "real64_array",
                                  "real64_array",
                                  "",
                                  "",
                                  "",
                                  faceManager->getName(),
                                  1,
                                  0,
                                  0 );

      docNode->AllocateChildNode( viewKeyStruct::faceCenterString,
                                  viewKeyStruct::faceCenterString,
                                  -1,
                                  "r1_array",
                                  "r1_array",
                                  "",
                                  "",
                                  "",
                                  faceManager->getName(),
                                  1,
                                  0,
                                  0 );
    }



    ElementRegionManager * const elemManager = meshLevel->getElemManager();

    elemManager->forCellBlocks( [&]( CellBlockSubRegion * const cellBlock ) -> void
      {
        cxx_utilities::DocumentationNode * const docNode = cellBlock->getDocumentationNode();
        docNode->AllocateChildNode( viewKeys.trilinosIndex.Key(),
                                    viewKeys.trilinosIndex.Key(),
                                    -1,
                                    "localIndex_array",
                                    "localIndex_array",
                                    "",
                                    "",
                                    "",
                                    elemManager->getName(),
                                    1,
                                    0,
                                    0 );

        docNode->AllocateChildNode( viewKeyStruct::deltaFluidPressureString,
                                    viewKeyStruct::deltaFluidPressureString,
                                    -1,
                                    "real64_array",
                                    "real64_array",
                                    "",
                                    "",
                                    "",
                                    elemManager->getName(),
                                    1,
                                    0,
                                    0 );

        docNode->AllocateChildNode( viewKeyStruct::fluidPressureString,
                                    viewKeyStruct::fluidPressureString,
                                    -1,
                                    "real64_array",
                                    "real64_array",
                                    "",
                                    "",
                                    "",
                                    elemManager->getName(),
                                    1,
                                    0,
                                    0 );

        docNode->AllocateChildNode( viewKeyStruct::volumeString,
                                    viewKeyStruct::volumeString,
                                    -1,
                                    "real64_array",
                                    "real64_array",
                                    "",
                                    "",
                                    "",
                                    elemManager->getName(),
                                    1,
                                    0,
                                    0 );

        docNode->AllocateChildNode( viewKeyStruct::deltaVolumeString,
                                    viewKeyStruct::deltaVolumeString,
                                    -1,
                                    "real64_array",
                                    "real64_array",
                                    "",
                                    "",
                                    "",
                                    elemManager->getName(),
                                    1,
                                    0,
                                    0 );

        docNode->AllocateChildNode( viewKeyStruct::porosityString,
                                    viewKeyStruct::porosityString,
                                    -1,
                                    "real64_array",
                                    "real64_array",
                                    "",
                                    "",
                                    "",
                                    elemManager->getName(),
                                    1,
                                    0,
                                    0 );
        docNode->AllocateChildNode( viewKeyStruct::deltaPorosityString,
                                    viewKeyStruct::deltaPorosityString,
                                    -1,
                                    "real64_array",
                                    "real64_array",
                                    "",
                                    "",
                                    "",
                                    elemManager->getName(),
                                    1,
                                    0,
                                    0 );

        docNode->AllocateChildNode( viewKeyStruct::permeabilityString,
                                    viewKeyStruct::permeabilityString,
                                    -1,
                                    "real64_array",
                                    "real64_array",
                                    "",
                                    "",
                                    "",
                                    elemManager->getName(),
                                    1,
                                    0,
                                    0 );

      });
  }
}


//void SinglePhaseFlow_TPFA::ReadXML_PostProcess()
//{
//
//}

void SinglePhaseFlow_TPFA::InitializeFinalLeaf( ManagedGroup * const problemManager )
{
  DomainPartition * domain = problemManager->GetGroup<DomainPartition>(keys::domain);

  // Call function to fill geometry parameters for use forming system
  MakeGeometryParameters( domain );
}

void SinglePhaseFlow_TPFA::TimeStep( real64 const& time_n,
                                     real64 const& dt,
                                     const int cycleNumber,
                                     ManagedGroup * domain )
{
  this->TimeStepImplicit( time_n, dt, cycleNumber, domain->group_cast<DomainPartition*>() );
}


/**
 * This function currently applies Dirichlet boundary conditions on the elements/zones as they
 * hold the DOF. Futher work will need to be done to apply a Dirichlet bc to the connectors (faces)
 */
void SinglePhaseFlow_TPFA::ApplyDirichletBC_implicit( ManagedGroup * object,
                                                      real64 const time,
                                                      EpetraBlockSystem & blockSystem )
{
  BoundaryConditionManager * bcManager = BoundaryConditionManager::get();
  ElementRegionManager * elemManager = object->group_cast<ElementRegionManager *>();

  // loop through cell block sub-regions
  elemManager->forCellBlocks([&]( CellBlockSubRegion * subRegion ) -> void
    {
      bcManager->ApplyBoundaryCondition( time,
                                         subRegion,
                                         viewKeyStruct::fluidPressureString,
                                         [&]( BoundaryConditionBase const * const bc,
                                              lSet const & set ) -> void
      {
        bc->ApplyDirichletBounaryConditionDefaultMethod<0>( set,
                                                            time,
                                                            subRegion,
                                                            viewKeyStruct::fluidPressureString,
                                                            viewKeys.trilinosIndex.Key(),
                                                            1,
                                                            &m_linearSystem,
                                                            EpetraBlockSystem::BlockIDs::fluidPressureBlock );

      });
    });
}


void SinglePhaseFlow_TPFA::TimeStepImplicitSetup( real64 const& time_n,
                                                  real64 const& dt,
                                                  DomainPartition * const domain )
{}

void SinglePhaseFlow_TPFA::TimeStepImplicitComplete( real64 const & time_n,
                                                     real64 const & dt,
                                                     DomainPartition * const domain)
{}


real64 SinglePhaseFlow_TPFA::TimeStepImplicit( real64 const & time_n,
                                               real64 const & dt,
                                               integer const cycleNumber,
                                               DomainPartition * const domain )
{
  real64 dt_return = dt;

  TimeStepImplicitSetup( time_n, dt, domain );

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);


  SetupSystem( domain, &m_linearSystem );

  Epetra_FECrsMatrix * matrix = m_linearSystem.GetMatrix( EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                                                          EpetraBlockSystem::BlockIDs::fluidPressureBlock );
  matrix->Scale(0.0);

  Epetra_FEVector * rhs = m_linearSystem.GetResidualVector( EpetraBlockSystem::BlockIDs::fluidPressureBlock );
  rhs->Scale(0.0);

  Epetra_FEVector * solution = m_linearSystem.GetSolutionVector( EpetraBlockSystem::BlockIDs::fluidPressureBlock );
  solution->Scale(0.0);

  Assemble( domain, &m_linearSystem, time_n+dt, dt );

  MeshLevel * const mesh = domain->getMeshBodies()->GetGroup<MeshBody>(0)->getMeshLevel(0);
  ManagedGroup * const nodeManager = mesh->getNodeManager();


  ApplyDirichletBC_implicit( mesh->getElemManager(),
                             time_n + dt,
                             m_linearSystem );

  if( verboseLevel() >= 2 )
  {
    matrix->Print(std::cout);
    rhs->Print(std::cout);
  }

  rhs->Scale(-1.0);

  if( verboseLevel() >= 1 )
  {
    std::cout<<"Solving system"<<std::endl;
  }



  this->m_linearSolverWrapper.SolveSingleBlockSystem( &m_linearSystem,
                                                      getSystemSolverParameters(),
                                                      systemSolverInterface::EpetraBlockSystem::BlockIDs::fluidPressureBlock );

  if( verboseLevel() >= 2 )
  {
    solution->Print(std::cout);
  }

  ApplySystemSolution( &m_linearSystem, 1.0, 0, mesh->getElemManager() );

  TimeStepImplicitComplete( time_n, dt,  domain );

  return dt_return;
}


void SinglePhaseFlow_TPFA::SetNumRowsAndTrilinosIndices( MeshLevel * const meshLevel,
                                                         localIndex & numLocalRows,
                                                         localIndex & numGlobalRows,
                                                         localIndex_array& localIndices,
                                                         localIndex offset )
{

  ElementRegionManager * const elementRegionManager = meshLevel->getElemManager();
  ElementRegionManager::ElementViewAccessor<localIndex_array>
  trilinosIndex = elementRegionManager->
                  ConstructViewAccessor<localIndex_array>( viewKeys.trilinosIndex.Key(),
                                                           string() );

  ElementRegionManager::ElementViewAccessor< integer_array >
  ghostRank = elementRegionManager->
              ConstructViewAccessor<integer_array>( ObjectManagerBase::viewKeyStruct::ghostRankString,
                                                    string() );

  int numMpiProcesses;
  MPI_Comm_size( MPI_COMM_WORLD, &numMpiProcesses );

  int thisMpiProcess = 0;
  MPI_Comm_rank( MPI_COMM_WORLD, &thisMpiProcess );

  int intNumLocalRows = integer_conversion<int>(numLocalRows);
  std::vector<int> gather(numMpiProcesses);

  m_linearSolverWrapper.m_epetraComm.GatherAll( &intNumLocalRows,
                                                &gather.front(),
                                                1 );
  numLocalRows = intNumLocalRows;

  localIndex firstLocalRow = 0;
  numGlobalRows = 0;

  for( integer p=0 ; p<numMpiProcesses ; ++p)
  {
    numGlobalRows += gather[p];
    if(p<thisMpiProcess)
      firstLocalRow += gather[p];
  }

  // create trilinos dof indexing
  for( localIndex er=0 ; er<ghostRank.size() ; ++er )
  {
    for( localIndex esr=0 ; esr<ghostRank[er].size() ; ++esr )
    {
      trilinosIndex[er][esr] = -1;
    }
  }

  integer localCount = 0;
  for( localIndex er=0 ; er<ghostRank.size() ; ++er )
  {
    for( localIndex esr=0 ; esr<ghostRank[er].size() ; ++esr )
    {
      for( localIndex k=0 ; k<ghostRank[er][esr].get().size() ; ++k )
      {
        if( ghostRank[er][esr][k] < 0 )
        {
          trilinosIndex[er][esr][k] = firstLocalRow+localCount+offset;
          ++localCount;
        }
        else
        {
          trilinosIndex[er][esr][k] = -1;
        }
      }
    }
  }

  assert(localCount == numLocalRows );

//  partition.SynchronizeFields(m_syncedFields, CommRegistry::lagrangeSolver02);

}


void SinglePhaseFlow_TPFA :: SetupSystem ( DomainPartition * const domain,
                                           EpetraBlockSystem * const blockSystem )
{
  MeshLevel * const mesh = domain->getMeshBodies()->GetGroup<MeshBody>(0)->getMeshLevel(0);
  ElementRegionManager * const elementRegionManager = mesh->getElemManager();

  localIndex numGhostRows  = 0;
  localIndex numLocalRows  = 0;
  localIndex numGlobalRows = 0;

  elementRegionManager->forCellBlocks( [&]( CellBlockSubRegion * const subRegion )
    {
      localIndex subRegionGhosts = subRegion->GetNumberOfGhosts();
      numGhostRows += subRegionGhosts;
      numLocalRows += subRegion->size() - subRegionGhosts;
    });


  localIndex_array displacementIndices;
  SetNumRowsAndTrilinosIndices( mesh,
                                numLocalRows,
                                numGlobalRows,
                                displacementIndices,
                                0 );

  std::map<string, array<string> > fieldNames;
  fieldNames["node"].push_back(viewKeys.trilinosIndex.Key());

  CommunicationTools::SynchronizeFields(fieldNames,
                                        mesh,
                                        domain->getReference< array<NeighborCommunicator> >( domain->viewKeys.neighbors ) );


  // create epetra map


  Epetra_Map * const rowMap = blockSystem->SetRowMap( EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                                                      std::make_unique<Epetra_Map>( static_cast<int>(m_dim*numGlobalRows),
                                                                                    static_cast<int>(m_dim*numLocalRows),
                                                                                    0,
                                                                                    m_linearSolverWrapper.m_epetraComm ) );

  Epetra_FECrsGraph * const sparsity = blockSystem->SetSparsity( EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                                                                 EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                                                                 std::make_unique<Epetra_FECrsGraph>(Copy,*rowMap,0) );



  SetSparsityPattern( domain, sparsity );

  sparsity->GlobalAssemble();
  sparsity->OptimizeStorage();

  blockSystem->SetMatrix( EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                          EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                          std::make_unique<Epetra_FECrsMatrix>(Copy,*sparsity) );

  blockSystem->SetSolutionVector( EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                                  std::make_unique<Epetra_FEVector>(*rowMap) );

  blockSystem->SetResidualVector( EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                                  std::make_unique<Epetra_FEVector>(*rowMap) );

}

void SinglePhaseFlow_TPFA::SetSparsityPattern( DomainPartition const * const domain,
                                               Epetra_FECrsGraph * const sparsity )
{
  MeshLevel const * const meshLevel = domain->getMeshBodies()->GetGroup<MeshBody>(0)->getMeshLevel(0);
  ElementRegionManager const * const elementRegionManager = meshLevel->getElemManager();
  ElementRegionManager::ElementViewAccessor<localIndex_array const>
  trilinosIndex = elementRegionManager->
                  ConstructViewAccessor<localIndex_array>( viewKeys.trilinosIndex.Key(),
                                                           string() );

  ElementRegionManager::ElementViewAccessor< integer_array const >
  ghostRank = elementRegionManager->
              ConstructViewAccessor<integer_array>( ObjectManagerBase::viewKeyStruct::ghostRankString,
                                                    string() );


  FaceManager const * const faceManager = meshLevel->getFaceManager();
  Array2dT<localIndex> const & elementRegionList = faceManager->elementRegionList();
  Array2dT<localIndex> const & elementSubRegionList = faceManager->elementSubRegionList();
  Array2dT<localIndex> const & elementIndexList = faceManager->elementList();
  integer_array elementLocalDofIndex;
  elementLocalDofIndex.resize(2);

  localIndex numFaceConnectors = 0;
  m_faceConnectors.resize(faceManager->size());
  for( localIndex kf=0 ; kf<faceManager->size() ; ++kf )
  {
    if( ghostRank[elementRegionList[kf][0]][elementSubRegionList[kf][0]][elementIndexList[kf][0]] < 0 )
      if( elementRegionList[kf][0] >= 0 && elementRegionList[kf][1] >= 0 )
      {
        elementLocalDofIndex[0] = trilinosIndex[elementRegionList[kf][0]][elementSubRegionList[kf][0]][elementIndexList[kf][0]];
        elementLocalDofIndex[1] = trilinosIndex[elementRegionList[kf][1]][elementSubRegionList[kf][1]][elementIndexList[kf][1]];

        sparsity->InsertGlobalIndices( 2,
                                       elementLocalDofIndex.data(),
                                       2,
                                       elementLocalDofIndex.data());
        m_faceConnectors[numFaceConnectors] = kf;
        ++numFaceConnectors;
      }
  }


}



real64 SinglePhaseFlow_TPFA::Assemble ( DomainPartition * const  domain,
                                        EpetraBlockSystem * const blockSystem,
                                        real64 const time_n,
                                        real64 const dt )
{
  MeshLevel * const mesh = domain->getMeshBodies()->GetGroup<MeshBody>(0)->getMeshLevel(0);
  ManagedGroup * const nodeManager = mesh->getNodeManager();
  ConstitutiveManager * const constitutiveManager = domain->GetGroup<ConstitutiveManager >(keys::ConstitutiveManager);
  ElementRegionManager * const elemManager = mesh->getElemManager();
  FaceManager const * const faceManager = mesh->getFaceManager();

  integer_array const & faceGhostRank = faceManager->getReference<integer_array>(ObjectManagerBase::viewKeyStruct::ghostRankString);



  Array2dT<localIndex> const & faceToElemRegionList     = faceManager->elementRegionList();
  Array2dT<localIndex> const & faceToElemSubRegionList  = faceManager->elementSubRegionList();
  Array2dT<localIndex> const & faceToElemList           = faceManager->elementList();


  Epetra_FECrsMatrix * const matrix = blockSystem->GetMatrix( EpetraBlockSystem::BlockIDs::fluidPressureBlock,
                                                              EpetraBlockSystem::BlockIDs::fluidPressureBlock );
  Epetra_FEVector * const rhs = blockSystem->GetResidualVector( EpetraBlockSystem::BlockIDs::fluidPressureBlock );
  Epetra_FEVector * const solution = blockSystem->GetSolutionVector( EpetraBlockSystem::BlockIDs::fluidPressureBlock );

  Epetra_IntSerialDenseVector elem_index(1);
  Epetra_SerialDenseVector elem_rhs(1);
  Epetra_SerialDenseMatrix elem_matrix(1, 1);

  constitutive::ViewAccessor<real64> fluidBulkModulus = constitutiveManager->GetParameterData< real64 >("BulkModulus");

  constitutive::ViewAccessor< array<real64> >
  rho = constitutiveManager->GetData< array<real64> >("fluidDensity");

  elemManager->forCellBlocks( [&]( CellBlockSubRegion * const activeCellBlock) -> void
    {

      auto const & constitutiveMap =
        activeCellBlock->getReference< std::pair< Array2dT<localIndex>,Array2dT<localIndex> > >(activeCellBlock->viewKeys().constitutiveMap);

      view_rtype_const<real64_array> volume = activeCellBlock->getData<real64_array>(viewKeyStruct::volumeString);
      view_rtype_const<real64_array> porosity = activeCellBlock->getData<real64_array>(viewKeyStruct::porosityString);

      view_rtype_const<real64_array> dP = activeCellBlock->getData<real64_array>(viewKeyStruct::deltaFluidPressureString);
      view_rtype_const<real64_array> dVolume = activeCellBlock->getData<real64_array>(viewKeyStruct::deltaVolumeString);
      view_rtype_const<real64_array> dPorosity = activeCellBlock->getData<real64_array>(viewKeyStruct::deltaPorosityString);

      view_rtype_const<localIndex_array> trilinos_index = activeCellBlock->getData<localIndex_array>(viewKeys.trilinosIndex);

      Array2dT<localIndex> const & consitutiveModelIndex      = constitutiveMap.first;
      Array2dT<localIndex> const & consitutiveModelArrayIndex = constitutiveMap.second;


      FOR_ELEMS_IN_SUBREGION( activeCellBlock, k )
      {
        localIndex const matIndex1 = consitutiveModelIndex[k][0];
        localIndex const matIndex2 = consitutiveModelArrayIndex[k][0];

        real64 dRho;
        real64 dRho_dP;
        ConstitutiveBase * const EOS = constitutiveManager->GetGroup<ConstitutiveBase>(matIndex1);
        EOS->EquationOfStateDensityUpdate( dP[k], matIndex2, dRho, dRho_dP );

        elem_matrix(0, 0) = 1.0 / fluidBulkModulus[matIndex1] * rho[matIndex1][matIndex2] * volume[k] * porosity[k];
        elem_index(0) = trilinos_index[k];
        matrix->SumIntoGlobalValues(elem_index, elem_matrix);

        elem_rhs(0) = ( (dPorosity[k] + porosity[k]) * (dRho + rho[matIndex1][matIndex2]) ) * dVolume[k]
                      + (dPorosity[k] * (dRho + rho[matIndex1][matIndex2]) + porosity[k] * dRho ) * volume[k];
        //
        rhs->SumIntoGlobalValues(elem_index, elem_rhs);
      } END_FOR
    });


  ElementRegionManager::ElementViewAccessor< real64_array >
  permeability = elemManager->
                 ConstructViewAccessor<real64_array>( viewKeyStruct::permeabilityString,
                                                      string() );

  ElementRegionManager::ElementViewAccessor< std::pair< Array2dT<localIndex>,Array2dT<localIndex> > >
  constitutiveMap = elemManager->
                    ConstructViewAccessor< std::pair< Array2dT<localIndex>,Array2dT<localIndex> > >( CellBlockSubRegion::viewKeyStruct::constitutiveMapString,
                                                                                                     string() );

  ElementRegionManager::ElementViewAccessor< real64_array >
  pressure = elemManager->
             ConstructViewAccessor<real64_array>( viewKeyStruct::fluidPressureString,
                                                  string() );


  ElementRegionManager::ElementViewAccessor< localIndex_array >
  trilinosIndex = elemManager->
                  ConstructViewAccessor<localIndex_array>( viewKeyStruct::trilinosIndexString,
                                                           string() );


  constitutive::ViewAccessor< real64 >
  fluidViscosity = constitutiveManager->GetData< real64 >("fluidViscocity");

  Epetra_IntSerialDenseVector face_index(2);
  Epetra_SerialDenseVector face_rhs(2);
  Epetra_SerialDenseMatrix face_matrix(2, 2);

  for( localIndex kf=0 ; kf<faceManager->size() ; ++kf )
  {
    if( faceGhostRank[kf] < 0 )
    {
      localIndex er1 = faceToElemRegionList[kf][0];
      localIndex er2 = faceToElemRegionList[kf][1];
      localIndex esr1 = faceToElemSubRegionList[kf][0];
      localIndex esr2 = faceToElemSubRegionList[kf][1];
      localIndex ei1 = faceToElemList[kf][0];
      localIndex ei2 = faceToElemList[kf][1];

      if( er1 >= 0 && er2 >= 0 )
      {
        localIndex const consitutiveModelIndex1      = constitutiveMap[er1][esr1].get().first[ei1][0];
        localIndex const consitutiveModelIndex2      = constitutiveMap[er2][esr2].get().first[ei2][0];
        localIndex const consitutiveModelArrayIndex1 = constitutiveMap[er1][esr1].get().second[ei1][0];
        localIndex const consitutiveModelArrayIndex2 = constitutiveMap[er2][esr2].get().second[ei2][0];

        real64 const rho1 = rho[consitutiveModelIndex1][consitutiveModelArrayIndex1];
        real64 const rho2 = rho[consitutiveModelIndex2][consitutiveModelArrayIndex2];

        real64 const face_weight = m_faceToElemLOverA[kf][0] / ( m_faceToElemLOverA[kf][0]
                                                                 + m_faceToElemLOverA[kf][1] );

        real64 perm = 1.0e-18;
        real64 const face_trans = 1.0 / ( m_faceToElemLOverA[kf][0] / perm
                                          + m_faceToElemLOverA[kf][1] / perm );
//      real64 const face_trans = 1.0 / ( m_faceToElemLOverA[kf][0] / (*(permeability[er1][esr1]))[ei1]
//                                      + m_faceToElemLOverA[kf][1] / (*(permeability[er2][esr2]))[ei2] );

        real64 const rhoav = face_weight * rho1
                             + (1.0 - face_weight) * rho2;

        real64 const dP = pressure[er1][esr1][ei1] - pressure[er2][esr2][ei2];
//      if (m_applyGravity) dP -= (*gdz)[kf] * rhoav;

        real64 const mu = 0.001;//*(fluidViscosity[consitutiveModelIndex1]);
        real64 const rhoTrans = rhoav * face_trans / mu * dt;

        real64 const poreCompress = 0.0;

        real64 const fluidCompressibility = 1.0 / fluidBulkModulus[consitutiveModelIndex1];

        real64 const dRdP1 = dP * face_weight * rho1 * ( fluidCompressibility + poreCompress ) * face_trans / mu * dt;

        real64 const dRdP2 = dP * (1.0 - face_weight) * rho2 * ( fluidCompressibility + poreCompress ) * face_trans / mu * dt;

        real64 dRgdP1 = 0.0;
        real64 dRgdP2 = 0.0;
//
//      dRgdP1 = (*gdz)[kf] * face_weight * (*m_densityPtr[reg0])[k0] * ( m_fluidRockProperty.m_compress +
// (*m_poreCompressPtr[reg0])[k0]);
//      dRgdP2 = (*gdz)[kf] * ( 1 - face_weight ) * (*m_densityPtr[reg1])[k1] * ( m_fluidRockProperty.m_compress +
// (*m_poreCompressPtr[reg1])[k1]);
//
        face_matrix(0, 0) = rhoTrans + dRdP1 - rhoTrans * dRgdP1;
        face_matrix(1, 1) = rhoTrans - dRdP2 + rhoTrans * dRgdP2;

        face_matrix(0, 1) = -rhoTrans + dRdP2 - rhoTrans * dRgdP2;
        face_matrix(1, 0) = -rhoTrans - dRdP1 + rhoTrans * dRgdP1;

        face_index[0] = trilinosIndex[er1][esr1][ei1];
        face_index[1] = trilinosIndex[er2][esr2][ei2];

        matrix->SumIntoGlobalValues(face_index, face_matrix);

        face_rhs(0) = rhoTrans * dP;
        face_rhs(1) = -rhoTrans * dP;
        rhs->SumIntoGlobalValues(face_index, face_rhs);
      }
    }
  }


  matrix->GlobalAssemble(true);
  rhs->GlobalAssemble();

  return 0;
}


void SinglePhaseFlow_TPFA::ApplySystemSolution( EpetraBlockSystem const * const blockSystem,
                                                real64 const scalingFactor,
                                                localIndex const dofOffset,
                                                ManagedGroup * const objectManager )
{
  Epetra_Map const * const rowMap        = blockSystem->GetRowMap( EpetraBlockSystem::BlockIDs::fluidPressureBlock );
  Epetra_FEVector const * const solution = blockSystem->GetSolutionVector( EpetraBlockSystem::BlockIDs::fluidPressureBlock );

  int dummy;
  double* local_solution = nullptr;
  solution->ExtractView(&local_solution,&dummy);

  ElementRegionManager * const elementRegionManager = objectManager->group_cast<ElementRegionManager * const>();

  ElementRegionManager::ElementViewAccessor<localIndex_array>
  trilinosIndex = elementRegionManager->
                  ConstructViewAccessor<localIndex_array>( viewKeys.trilinosIndex.Key(),
                                                           string() );

  ElementRegionManager::ElementViewAccessor<real64_array>
  fieldVar = elementRegionManager->
             ConstructViewAccessor<real64_array>( viewKeyStruct::fluidPressureString,
                                                  string() );

  int count=0;
  for( localIndex er=0 ; er<trilinosIndex.size() ; ++er )
  {
    for( localIndex esr=0 ; esr<trilinosIndex[er].size() ; ++esr )
    {
      for( localIndex k=0 ; k<trilinosIndex[er][esr].get().size() ; ++k )
      {
        int const lid = rowMap->LID(integer_conversion<int>(trilinosIndex[er][esr][k]));
        fieldVar[er][esr][k] = local_solution[lid];
      }
    }
  }
}


void SinglePhaseFlow_TPFA::MakeGeometryParameters( DomainPartition * const  domain )
{

  MeshLevel * const mesh = domain->getMeshBodies()->GetGroup<MeshBody>(0)->getMeshLevel(0);

  NodeManager * const nodeManager = mesh->getNodeManager();
  FaceManager * const faceManager = mesh->getFaceManager();
  ElementRegionManager * const elemManager = mesh->getElemManager();

  array<R1Tensor> faceCenter(faceManager->size());

//  rArray1d * gdz = faceManager.GetFieldDataPointer<realT>("gdz");
//  if (m_applyGravity) (*gdz) = 0.0;

  Array2dT<localIndex> const & elemRegionList     = faceManager->elementRegionList();
  Array2dT<localIndex> const & elemSubRegionList  = faceManager->elementSubRegionList();
  Array2dT<localIndex> const & elemList           = faceManager->elementList();
  r1_array const & X = nodeManager->referencePosition();

  array< array< array< R1Tensor > > > elemCenter;

  elemCenter.resize(elemManager->numRegions());
  for( localIndex regIndex=0 ; regIndex<elemManager->numRegions() ; ++regIndex )
  {
    ElementRegion * const elemRegion = elemManager->GetRegion(regIndex);
    elemCenter[regIndex].resize(elemRegion->numSubRegions());
    for( localIndex subRegIndex=0 ; subRegIndex<elemRegion->numSubRegions() ; ++subRegIndex )
    {
      CellBlockSubRegion * const subRegion = elemRegion->GetSubRegion(subRegIndex);
      elemCenter[regIndex][subRegIndex].resize(subRegion->size());

      lArray2d const &elemsToNodes = subRegion->nodeList();
      real64_array & volume        = subRegion->getReference<real64_array>( viewKeyStruct::volumeString );

      for( localIndex k=0 ; k<subRegion->size() ; ++k )
      {
        elemCenter[regIndex][subRegIndex][k] = subRegion->GetElementCenter( k, *nodeManager, true );

        volume[k] += 1;
      }
    }
  }

  // loop over faces
  real64_array & faceArea = faceManager->getReference<real64_array>("faceArea");
  array< array<localIndex> > const & faceToNodes = faceManager->nodeList();
  for (localIndex kf = 0 ; kf < faceManager->size() ; ++kf )
  {
    //TODO fix this
    faceArea[kf] = 1.0;

  }

  array<R1Tensor> faceConnectionVector( faceManager->size() );


  R1Tensor fCenter, fNormal;
  localIndex m, n;

  m_faceToElemLOverA.resize( faceManager->size(), 2);
  for(localIndex kf = 0 ; kf < faceManager->size() ; ++kf)
  {
    computationalGeometry::Centroid_3DPolygon( faceToNodes[kf],
                                               X,
                                               fCenter,
                                               fNormal );

    localIndex numElems = 2;

    for (localIndex k = 0 ; k < numElems ; ++k)
    {
      if( elemRegionList[kf][k] != -1 )
      {
        R1Tensor la = elemCenter[ elemRegionList[kf][k] ]
                      [ elemSubRegionList[kf][k] ]
                      [ elemList[kf][k] ];
        la -= fCenter;

        m_faceToElemLOverA( kf, k) = ( fabs(Dot(la, fNormal)) / faceArea[kf] );

        //        if (m_applyGravity)
        //        {
        //          R1Tensor dz(la);
        //          if (ele == 1) dz *= -1.0;
        //          (*gdz)[index] += Dot(dz, m_gravityVector);
        //        }
      }
    }
  }
}



REGISTER_CATALOG_ENTRY( SolverBase, SinglePhaseFlow_TPFA, std::string const &, ManagedGroup * const )
} /* namespace ANST */
