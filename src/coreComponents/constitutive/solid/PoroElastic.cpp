/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2019 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2019 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2019 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All right reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */


/**
 * @file PoroElastic.cpp
 */

#include "PoroElastic.hpp"

#include "LinearElasticAnisotropic.hpp"
#include "LinearElasticIsotropic.hpp"
#include "LinearViscoElasticAnisotropic.hpp"
#include "LinearViscoElasticIsotropic.hpp"
#include "BilinearElasticIsotropic.hpp"
#include "CycLiqCPSP.hpp"

namespace geosx
{

using namespace dataRepository;
namespace constitutive
{

template< typename BASE >
PoroElastic<BASE>::PoroElastic( string const & name, Group * const parent ):
  BASE( name, parent ),
  m_biotCoefficient(),
  m_poreVolumeMultiplier(),
  m_dPVMult_dPressure(),
  m_poreVolumeRelation()
{
  this->registerWrapper( viewKeyStruct::biotCoefficientString, &m_biotCoefficient, 0 )->
    setApplyDefaultValue(0)->
    setInputFlag(InputFlags::OPTIONAL)->
    setDescription("Biot's coefficient");

  this->registerWrapper( viewKeyStruct::poreVolumeMultiplierString, &m_poreVolumeMultiplier, 0 )->
    setApplyDefaultValue(-1)->
    setDescription("");

  this->registerWrapper( viewKeyStruct::dPVMult_dPresString, &m_dPVMult_dPressure, 0 )->
    setApplyDefaultValue(-1)->
    setDescription("");
}

template< typename BASE >
PoroElastic<BASE>::~PoroElastic()
{
}

template< typename BASE >
void PoroElastic<BASE>::PostProcessInput()
{
	BASE::PostProcessInput();

//   m_compressibility = 1 / m_defaultBulkModulus;
//
//  if (m_compressibility <= 0)
//  {
//    string const message = std::to_string( numConstantsSpecified ) + " Elastic Constants Specified. Must specify 2 constants!";
//    GEOSX_ERROR( message );
//  }
//  m_poreVolumeRelation.SetCoefficients( m_referencePressure, 1.0, m_compressibility );

}

template< typename BASE >
void PoroElastic<BASE>::DeliverClone( string const & name,
                                      Group * const parent,
                                      std::unique_ptr<ConstitutiveBase> & clone ) const
{
  if( !clone )
  {
    clone = std::make_unique<PoroElastic<BASE> >( name, parent );
  }
  BASE::DeliverClone( name, parent, clone );
  PoroElastic<BASE> * const newConstitutiveRelation = dynamic_cast<PoroElastic<BASE> *>(clone.get());

  newConstitutiveRelation->m_biotCoefficient      = m_biotCoefficient;
  newConstitutiveRelation->m_poreVolumeMultiplier = m_poreVolumeMultiplier;
  newConstitutiveRelation->m_dPVMult_dPressure    = m_dPVMult_dPressure;
  newConstitutiveRelation->m_poreVolumeRelation   = m_poreVolumeRelation;
}

template< typename BASE >
void PoroElastic<BASE>::AllocateConstitutiveData( dataRepository::Group * const parent,
                                                  localIndex const numConstitutivePointsPerParentIndex )
{
  BASE::AllocateConstitutiveData( parent, numConstitutivePointsPerParentIndex );

  m_poreVolumeMultiplier.resize( parent->size(), numConstitutivePointsPerParentIndex );
  m_dPVMult_dPressure.resize( parent->size(), numConstitutivePointsPerParentIndex );
  m_poreVolumeMultiplier = 1.0;

}

typedef PoroElastic<LinearElasticIsotropic> PoroLinearElasticIsotropic;
typedef PoroElastic<LinearElasticAnisotropic> PoroLinearElasticAnisotropic;
typedef PoroElastic<LinearViscoElasticIsotropic> PoroLinearViscoElasticIsotropic;
typedef PoroElastic<LinearViscoElasticAnisotropic> PoroLinearViscoElasticAnisotropic;
typedef PoroElastic<BilinearElasticIsotropic> PoroBilinearElasticIsotropic;
typedef PoroElastic<CycLiqCPSP> PoroCycLiqCPSP;

REGISTER_CATALOG_ENTRY( ConstitutiveBase, PoroLinearElasticIsotropic, string const &, Group * const )
REGISTER_CATALOG_ENTRY( ConstitutiveBase, PoroLinearElasticAnisotropic, string const &, Group * const )
REGISTER_CATALOG_ENTRY( ConstitutiveBase, PoroLinearViscoElasticIsotropic, string const &, Group * const )
REGISTER_CATALOG_ENTRY( ConstitutiveBase, PoroBilinearElasticIsotropic, string const &, Group * const )
REGISTER_CATALOG_ENTRY( ConstitutiveBase, PoroCycLiqCPSP, string const &, Group * const )

}
} /* namespace geosx */
