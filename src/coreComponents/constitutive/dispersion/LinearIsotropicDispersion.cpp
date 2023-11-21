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
 * @file LinearIsotropicDispersion.cpp
 */

#include "LinearIsotropicDispersion.hpp"
#include "constitutive/dispersion/DispersionFields.hpp"

namespace geos
{

using namespace dataRepository;

namespace constitutive
{

LinearIsotropicDispersion::LinearIsotropicDispersion( string const & name, Group * const parent ):
  DispersionBase( name, parent )
{
  registerWrapper( viewKeyStruct::longitudinalDispersivityString(), &m_longitudinalDispersivity ).
    setInputFlag( InputFlags::REQUIRED ).
    setRestartFlags( RestartFlags::NO_WRITE ).
    setDescription( "Longitudinal dispersivity [m]" );

  registerField( fields::dispersion::phaseVelocityNorm{}, &m_phaseVelocityNorm );
}

std::unique_ptr< ConstitutiveBase >
LinearIsotropicDispersion::deliverClone( string const & name,
                                         Group * const parent ) const
{
  return DispersionBase::deliverClone( name, parent );
}

void LinearIsotropicDispersion::allocateConstitutiveData( dataRepository::Group & parent,
                                                          const geos::localIndex numConstitutivePointsPerParentIndex )
{
  DispersionBase::allocateConstitutiveData( parent, numConstitutivePointsPerParentIndex );
  m_phaseVelocityNorm.resize( parent.size(), 1, numFluidPhases());
  for( int ei = 0; ei < parent.size(); ++ei )
  {
    for( int q = 0; q < 1; ++q )
    {
      for( int ip = 0; ip < m_dispersivity.size( 2 ); ++ip )
      {
        m_dispersivity[ei][q][0] = m_longitudinalDispersivity;
        m_dispersivity[ei][q][1] = m_longitudinalDispersivity;
        m_dispersivity[ei][q][2] = m_longitudinalDispersivity;
      }
    }

  }
}

void LinearIsotropicDispersion::postProcessInput()
{
  GEOS_THROW_IF( m_longitudinalDispersivity < 0,
                 GEOS_FMT( "{}: longitudinal dispersivity must be positive",
                           getFullName() ),
                 InputError );
}

void LinearIsotropicDispersion::initializeVelocityState( arrayView4d< real64 const > const & initialVelocity, arrayView3d< real64 const > const & phaseDensity ) const
{
  saveConvergedVelocityState( initialVelocity, phaseDensity );
}

void LinearIsotropicDispersion::saveConvergedVelocityState( arrayView4d< real64 const > const & convergedVelocity, arrayView3d< real64 const > const & phaseDensity ) const
{
  // note that the update function is called here, and not in the solver, because total velocity is treated explicitly
  KernelWrapper dispersionWrapper = createKernelWrapper();

  forAll< parallelDevicePolicy<> >( dispersionWrapper.numElems(), [=] GEOS_HOST_DEVICE ( localIndex const k )
  {
    for( localIndex q = 0; q < dispersionWrapper.numGauss(); ++q )
    {
      dispersionWrapper.update( k, q, convergedVelocity[k][q], phaseDensity[k][q] );
    }
  } );
}


REGISTER_CATALOG_ENTRY( ConstitutiveBase, LinearIsotropicDispersion, string const &, Group * const )

} // namespace constitutive

} // namespace geos
