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
 * @file MultiPhaseVolumeWeightedThermalConductivity.cpp
 */

#include "constitutive/thermalConductivity/MultiPhaseVolumeWeightedThermalConductivity.hpp"
#include "constitutive/thermalConductivity/MultiPhaseThermalConductivityFields.hpp"
#include "constitutive/thermalConductivity/ThermalConductivityFields.hpp"

namespace geos
{

using namespace dataRepository;

namespace constitutive
{

MultiPhaseVolumeWeightedThermalConductivity::MultiPhaseVolumeWeightedThermalConductivity( string const & name, Group * const parent ):
  MultiPhaseThermalConductivityBase( name, parent )
{
  registerWrapper( viewKeyStruct::rockThermalConductivityComponentsString(), &m_rockThermalConductivityComponents ).
    setInputFlag( InputFlags::REQUIRED ).
    setRestartFlags( RestartFlags::NO_WRITE ).
    setDescription( "xx, yy, and zz components of a diagonal rock thermal conductivity tensor [W/(m.K)]" );

  registerWrapper( viewKeyStruct::phaseThermalConductivityString(), &m_phaseThermalConductivity ).
    setInputFlag( InputFlags::REQUIRED ).
    setRestartFlags( RestartFlags::NO_WRITE ).
    setDescription( "Phase thermal conductivity [W/(m.K)]" );

  registerField( fields::thermalconductivity::rockThermalConductivity{}, &m_rockThermalConductivity );
}

std::unique_ptr< ConstitutiveBase >
MultiPhaseVolumeWeightedThermalConductivity::deliverClone( string const & name,
                                                           Group * const parent ) const
{
  return MultiPhaseThermalConductivityBase::deliverClone( name, parent );
}

void MultiPhaseVolumeWeightedThermalConductivity::allocateConstitutiveData( dataRepository::Group & parent,
                                                                            localIndex const numConstitutivePointsPerParentIndex )
{
  integer const numQuadraturePoints = LvArray::math::min( MultiPhaseThermalConductivityBase::maxNumQuadraturePoints, numConstitutivePointsPerParentIndex );

  m_rockThermalConductivity.resize( 0, numQuadraturePoints, 3 );

  MultiPhaseThermalConductivityBase::allocateConstitutiveData( parent, numQuadraturePoints );

  for( localIndex ei = 0; ei < parent.size(); ++ei )
  {
    for( localIndex q = 0; q < numQuadraturePoints; ++q )
    {
      m_rockThermalConductivity[ei][q][0] = m_rockThermalConductivityComponents[0];
      m_rockThermalConductivity[ei][q][1] = m_rockThermalConductivityComponents[1];
      m_rockThermalConductivity[ei][q][2] = m_rockThermalConductivityComponents[2];
    }
  }
}

void MultiPhaseVolumeWeightedThermalConductivity::postProcessInput()
{
  GEOS_THROW_IF( m_rockThermalConductivityComponents[0] <= 0 ||
                 m_rockThermalConductivityComponents[1] <= 0 ||
                 m_rockThermalConductivityComponents[2] <= 0,
                 GEOS_FMT( "{}: the components of the rock thermal conductivity tensor must be strictly positive",
                           getFullName() ),
                 InputError );

  for( integer ip = 0; ip < numFluidPhases(); ++ip )
  {
    GEOS_THROW_IF( m_phaseThermalConductivity[ip] <= 0,
                   GEOS_FMT( "{}: the phase thermal conductivity for phase {} must be strictly positive",
                             getFullName(), ip ),
                   InputError );
  }
}

void MultiPhaseVolumeWeightedThermalConductivity::initializeRockFluidState( arrayView2d< real64 const > const & initialPorosity,
                                                                            arrayView2d< real64 const, compflow::USD_PHASE > const & initialPhaseVolumeFraction ) const
{
  saveConvergedRockFluidState( initialPorosity, initialPhaseVolumeFraction );
}

void MultiPhaseVolumeWeightedThermalConductivity::saveConvergedRockFluidState( arrayView2d< real64 const > const & convergedPorosity,
                                                                               arrayView2d< real64 const, compflow::USD_PHASE > const & convergedPhaseVolumeFraction ) const
{
  // note that the update function is called here, and not in the solver, because porosity and phase volume fraction are treated explicitly

  KernelWrapper conductivityWrapper = createKernelWrapper();

  forAll< parallelDevicePolicy<> >( conductivityWrapper.numElems(), [=] GEOS_HOST_DEVICE ( localIndex const k )
  {
    for( localIndex q = 0; q < conductivityWrapper.numGauss(); ++q )
    {
      conductivityWrapper.update( k, q, convergedPorosity[k][q], convergedPhaseVolumeFraction[k] );
    }
  } );
}


REGISTER_CATALOG_ENTRY( ConstitutiveBase, MultiPhaseVolumeWeightedThermalConductivity, string const &, Group * const )

} // namespace constitutive

} // namespace geos
