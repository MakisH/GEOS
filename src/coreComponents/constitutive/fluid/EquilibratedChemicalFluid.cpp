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
 * @file EquilibratedChemicalFluid.cpp
 */

#include "EquilibratedChemicalFluid.hpp"

namespace geosx
{

using namespace dataRepository;

namespace constitutive
{

static void interpolation( const string & name, const array1d< real64 > & values1, const array1d< real64 > & values2, const real64 & value1, real64 & value2 )
{

  if( value1 < values1[0] || value1 > values1[values1.size()-1] )
    GEOSX_ERROR( "variable: " + name + " is out of bound!" );

  localIndex idx = 0;

  for( localIndex i = 1; i < values1.size(); ++i )
    if( value1 <= values1[i] )
    {
      idx = i;
      break;
    }

  value2 = values2[idx-1] + (values2[idx] - values2[idx-1]) / (values1[idx] - values1[idx-1]) * (value1 - values1[idx-1]);


}



static DatabaseType stringToDatabaseType( string const & databaseType )
{
  if( databaseType == "EQ36" )
  {
    return DatabaseType::EQ36;
  }

  GEOSX_ERROR( "Database type not supported: " << databaseType );

  return DatabaseType::invalidType;
}

static ActivityCoefModel stringToActivityCoefModel( string const & activityCoefModel )
{
  if( activityCoefModel == "BDot" )
  {
    return ActivityCoefModel::BDot;
  }

  GEOSX_ERROR( "Activity coefficient model not supported: " << activityCoefModel );

  return ActivityCoefModel::invalidModel;
}

EquilibratedChemicalFluid::EquilibratedChemicalFluid( std::string const & name, Group * const parent ):
  ReactiveFluidBase( name, parent )
{

  registerWrapper( viewKeyStruct::databaseTypeString, &m_databaseTypeString )->
    setInputFlag( InputFlags::REQUIRED )->
    setDescription( "Thermodynamic database" );

  registerWrapper( viewKeyStruct::databaseFileString, &m_databaseFileName )->
    setInputFlag( InputFlags::REQUIRED )->
    setDescription( "Thermodynamic database file name" );

  registerWrapper( viewKeyStruct::activityCoefModelString, &m_activityCoefModelString )->
    setInputFlag( InputFlags::REQUIRED )->
    setDescription( "Activity coefficient model" );

  registerWrapper( viewKeyStruct::kineticReactionTypeString, &m_kineticReactionTypeString )->
    setInputFlag( InputFlags::OPTIONAL )->
    setDescription( "Kinetic reaction" );

  registerWrapper( viewKeyStruct::kineticReactionFileString, &m_kineticReactionFileName )->
    setInputFlag( InputFlags::OPTIONAL )->
    setDescription( "Kineitc reaction file name" );

  registerWrapper( viewKeyStruct::useActCoefString, &m_useActCoef )->
    setInputFlag( InputFlags::OPTIONAL )->
    setApplyDefaultValue( 1.0 )->
    setDescription( "Use activity coefficient " );


}

EquilibratedChemicalFluid::~EquilibratedChemicalFluid() = default;

void EquilibratedChemicalFluid::allocateConstitutiveData( dataRepository::Group * const parent,
                                                          localIndex const numConstitutivePointsPerParentIndex )
{
  ReactiveFluidBase::allocateConstitutiveData( parent, numConstitutivePointsPerParentIndex );

  m_databaseType   = stringToDatabaseType( m_databaseTypeString );
  m_activityCoefModel = stringToActivityCoefModel( m_activityCoefModelString );

  ReadDatabase();

  bool HplusNotFound = 1;
  bool H2OFound = 0;

  localIndex const NBasis = numBasisSpecies();

  m_isHplus.resize( NBasis );

  for( localIndex id = 0; id < NBasis; ++id )
  {

    if( m_basisSpeciesNames[id] == "H+" )
    {
      HplusNotFound = 0;
      m_isHplus[id] = 1;
    }
    else
    {
      m_isHplus[id] = 0;
    }

    if( m_basisSpeciesNames[id] == "H2O" )
      H2OFound = 1;

  }

  GEOSX_ERROR_IF( HplusNotFound, "ReactiveFluidBase: H+ is not specified in basisSpeciesNames" );

  GEOSX_ERROR_IF( H2OFound, "ReactiveFluidBase: H2O cannot be specified in basisSpeciesNames" );

  ResizeFields( parent->size() );

}

void EquilibratedChemicalFluid::PostProcessInput()
{
  ReactiveFluidBase::PostProcessInput();

}

void EquilibratedChemicalFluid::ResizeFields( localIndex size )
{

  localIndex const NBasis = numBasisSpecies();
  localIndex const NDependent = numDependentSpecies();
  localIndex const NReaction = numKineticReaction();

  m_concentrationAct.resize( size, NBasis );
  m_dependentConc.resize( size, NDependent + 1 );
  m_dDependentConc_dConc.resize( size, NDependent + 1, NBasis + 1 );

  m_kineticReactionRate.resize( size, NReaction );
  m_dKineticReactionRate_dConc.resize( size, NReaction, NBasis );

  m_kineticSpeciesReactionRate.resize( size, NBasis );
  m_dKineticSpeciesReactionRate_dConc.resize( size, NBasis, NBasis );

  m_totalConc.resize( size, NBasis );
  m_dTotalConc_dConc.resize( size, NBasis, NBasis );

}

void EquilibratedChemicalFluid::ReadDatabase()
{
  if( !m_thermoDatabase )
    m_thermoDatabase = ( ThermoDatabaseBase::CatalogInterface::Factory( m_databaseTypeString, m_databaseFileName, m_basisSpeciesNames, m_inputDependentSpeciesNames ) );

  const array1d< Species > & dependentSpecies = m_thermoDatabase->GetDependentSpecies();

  m_dependentSpeciesNames.clear();
  for( localIndex ic = 0; ic < dependentSpecies.size(); ++ic )
  {
    m_dependentSpeciesNames.emplace_back( dependentSpecies[ic].name );
  }

  localIndex const NBasis = numBasisSpecies();
  localIndex const NDependent = numDependentSpecies();

  m_stochMatrix.resize( NBasis, NDependent );

  m_stochMatrix.setValues< serialPolicy >( 0 );

  for( localIndex j = 0; j < dependentSpecies.size(); ++j )
  {

    real64 nu2 = dependentSpecies[j].stochs[0];

    bool isSolid = dependentSpecies[j].type == SpeciesType::Solid ? 1 : 0;
    bool isNonBasisGas = (dependentSpecies[j].name != "O2(g)" && dependentSpecies[j].type == SpeciesType::Gas) ? 1 : 0;

    for( localIndex i = 1; i < dependentSpecies[j].speciesIndices.size(); ++i )
    {

      localIndex ic = dependentSpecies[j].speciesIndices[i];

      if( ic >= NBasis )
        continue;

      real64 nu1 = dependentSpecies[j].stochs[i];

      if( isSolid || isNonBasisGas )
        m_stochMatrix[ic][j] = 0.0;
      else
        m_stochMatrix[ic][j] = nu1 / nu2;

    }

  }


  if( !m_kineticReactions )
    m_kineticReactions = ( KineticReactionsBase::CatalogInterface::Factory( m_kineticReactionTypeString, m_kineticReactionFileName, m_basisSpeciesNames ) );


}

void EquilibratedChemicalFluid::PointUpdate( real64 const & pressure, real64 const & temperature, arraySlice1d< real64 const > const & concentration, localIndex const k )
{

  Compute( pressure, temperature, concentration, m_dependentConc[k], m_dDependentConc_dConc[k], m_concentrationAct[k], m_thermoDatabase );

}

void EquilibratedChemicalFluid::ComputeLogActCoef( real64 const & pressure,
                                                   real64 const & temperature,
                                                   real64 const & ionicStrength,
                                                   array1d< real64 > & logActCoef1,
                                                   array1d< real64 > & dLogActCoef1,
                                                   array1d< real64 > & logActCoef2,
                                                   array1d< real64 > & dLogActCoef2 )
{
  GEOSX_UNUSED_VAR( pressure );
  localIndex const NBasis = numBasisSpecies();
  localIndex const NDependent = numDependentSpecies();

  const array1d< Species > & dependentSpecies = m_thermoDatabase->GetDependentSpecies();
  const array1d< Species > & basisSpecies = m_thermoDatabase->GetBasisSpecies();
  const array1d< localIndex > & basisSpeciesIndices = m_thermoDatabase->GetBasisSpeciesIndices();

  const ActCoefParameters & actCoefParameters = m_thermoDatabase->GetActCoefParameters();

  logActCoef1.resize( NBasis );
  dLogActCoef1.resize( NBasis );

  logActCoef2.resize( NDependent );
  dLogActCoef2.resize( NDependent );

  real64 DHA, DHB, Bdot, DHazero, charge;

  interpolation( "DHA", actCoefParameters.temperatures, actCoefParameters.DHAs, temperature, DHA );

  interpolation( "DHB", actCoefParameters.temperatures, actCoefParameters.DHBs, temperature, DHB );

  static real64 C=-1.0312;
  static real64 F=0.0012806;
  static real64 G=255.9;
  static real64 E=0.4445;
  static real64 H=-0.001606;
  real64 TK = temperature + 273.15;

  if( m_activityCoefModel == ActivityCoefModel::BDot )
  {

    interpolation( "Bdot", actCoefParameters.temperatures, actCoefParameters.BDots, temperature, Bdot );

    for( localIndex i = 0; i < NBasis; ++i )
    {
      localIndex ic = basisSpeciesIndices[i];

      DHazero = basisSpecies[ic].DHazero;
      charge = basisSpecies[ic].charge;

      logActCoef1[i] = Bdot * ionicStrength - DHA * charge * charge * sqrt( ionicStrength ) / (1.0 + DHazero * DHB * sqrt( ionicStrength ));

      dLogActCoef1[i] = Bdot - DHA * charge * charge *
                        (0.5 / sqrt( ionicStrength ) / (1.0 + DHazero * DHB * sqrt( ionicStrength )) - 0.5 * DHazero * DHB / (1.0 + DHazero * DHB * sqrt( ionicStrength )) /
                         (1.0 + DHazero * DHB * sqrt( ionicStrength )));

    }

    for( localIndex i = 0; i < dependentSpecies.size(); ++i )
    {
      DHazero = dependentSpecies[i].DHazero;
      charge = dependentSpecies[i].charge;

      if( dependentSpecies[i].name == "CO2(aq)" || dependentSpecies[i].name == "H2(aq)" || dependentSpecies[i].name == "O2(aq)" )
      {

        logActCoef2[i] = ((C + F * TK + G / TK) * ionicStrength -(E + H * TK) * (ionicStrength / (ionicStrength+1) )) / 2.303;

        dLogActCoef2[i] = ((C + F * TK + G / TK) - (E + H * TK) * (1.0 / (ionicStrength + 1) - ionicStrength / (ionicStrength + 1) / (ionicStrength + 1))) / 2.303;

      }
      else if( strstr( dependentSpecies[i].name.c_str(), "(aq)" ) || dependentSpecies[i].type == SpeciesType::Gas || dependentSpecies[i].type == SpeciesType::Solid )
      {

        logActCoef2[i] = 0;
        dLogActCoef2[i] = 0;


      }
      else
      {

        logActCoef2[i] = Bdot * ionicStrength - DHA * charge * charge * sqrt( ionicStrength ) / (1.0 + DHazero * DHB * sqrt( ionicStrength ));

        dLogActCoef2[i] = Bdot - DHA * charge * charge *
                          (0.5 / sqrt( ionicStrength ) / (1.0 + DHazero * DHB * sqrt( ionicStrength )) - 0.5 * DHazero * DHB / (1.0 + DHazero * DHB * sqrt( ionicStrength )) /
                           (1.0 + DHazero * DHB * sqrt( ionicStrength )));

      }

    }
  }
  else
    GEOSX_ERROR( "wrong activity coef model" );

}

void EquilibratedChemicalFluid::Compute( real64 const & pressure,
                                         real64 const & temperature,
                                         arraySlice1d< real64 const > const & concentration,
                                         arraySlice1d< real64 > const & dependentConc,
                                         arraySlice2d< real64 > const & dDependentConc_dConc,
                                         arraySlice1d< real64 > const & concentrationAct,
                                         ThermoDatabase & thermoDatabase )
{

  localIndex const NBasis = numBasisSpecies();
  localIndex const NDependent = numDependentSpecies();

  const array1d< Species > & dependentSpecies = thermoDatabase->GetDependentSpecies();

  const array1d< Species > & basisSpecies = m_thermoDatabase->GetBasisSpecies();
  const array1d< localIndex > & basisSpeciesIndices = m_thermoDatabase->GetBasisSpeciesIndices();

  const ActCoefParameters & actCoefParameters = m_thermoDatabase->GetActCoefParameters();

  static bool firstTime = 1;

  //compute d(logActCoef)/dI

  array1d< real64 > logActCoef1;
  array1d< real64 > dLogActCoef1;

  array1d< real64 > logActCoef2;
  array1d< real64 > dLogActCoef2;


  //get ionic strength

  dependentConc[NDependent] = 0;

  for( localIndex i = 0; i < NBasis; ++i )
  {
    localIndex id = basisSpeciesIndices[i];
    real64 charge = basisSpecies[id].charge;
    real64 conc = pow( 10.0, concentration[i] );

    dependentConc[NDependent] += 0.5 * charge * charge * conc;
  }

  for( localIndex i = 0; i < dependentSpecies.size(); ++i )
  {
    real64 charge = dependentSpecies[i].charge;
    real64 conc = pow( 10.0, dependentConc[i] );

    dependentConc[NDependent] += 0.5 * charge * charge * conc;

  }

  if( firstTime )
  {
    dependentConc[NDependent] = 1.0; // Initial guess, need to fix it later
    firstTime = 0;
  }

  ComputeLogActCoef( pressure, temperature, dependentConc[NDependent], logActCoef1, dLogActCoef1, logActCoef2, dLogActCoef2 );

  real64 logK;

  for( localIndex i = 0; i < NDependent + 1; ++i )
    for( localIndex j = 0; j < NBasis + 1; ++j )
      dDependentConc_dConc[i][j] = 0;

  for( localIndex i = 0; i < dependentSpecies.size(); ++i )
  {

    if( dependentSpecies[i].type == SpeciesType::Solid )
    {
      dependentConc[i] = 0.0;
      continue;
    }


    real64 nu1 = dependentSpecies[i].stochs[0];
    interpolation( "logK", actCoefParameters.temperatures, dependentSpecies[i].logKs, temperature, logK );

    dependentConc[i] = logK / nu1 - logActCoef2[i] * m_useActCoef;

    for( localIndex j = 1; j < dependentSpecies[i].speciesIndices.size(); ++j )
    {

      real64 nu2 = dependentSpecies[i].stochs[j];
      localIndex ic = dependentSpecies[i].speciesIndices[j];

      if( ic == NBasis )
      {
        dependentConc[i] -= nu2 / nu1 * m_logActH2O;
      }
      else if( ic == NBasis + 1 )
        dependentConc[i] -= nu2 / nu1 * m_logFO2g;
      else
      {
        dependentConc[i] -= nu2 / nu1 * (concentration[ic] + logActCoef1[ic] * m_useActCoef);
        dDependentConc_dConc[i][ic] -= nu2 / nu1;

      }


    }

  }

  for( localIndex i = 0; i < NBasis; ++i )
    concentrationAct[i] = concentration[i] + logActCoef1[i] * m_useActCoef;

}

void EquilibratedChemicalFluid::PointUpdateChemistry( real64 const & pressure,
                                                      real64 const & temperature,
                                                      arraySlice1d< real64 const > const & concentration,
                                                      arraySlice1d< real64 const > const & surfaceArea0,
                                                      arraySlice1d< real64 const > const & volumeFraction0,
                                                      arraySlice1d< real64 const > const & volumeFraction,
                                                      real64 const & porosity0,
                                                      real64 const & porosity, localIndex const k )
{

  Compute( pressure, temperature, concentration, m_dependentConc[k], m_dDependentConc_dConc[k], m_concentrationAct[k], m_thermoDatabase );

  ComputeChemistry( temperature, concentration, surfaceArea0, volumeFraction0, volumeFraction, porosity0, porosity, m_concentrationAct[k], m_dependentConc[k], m_dDependentConc_dConc[k],
                    m_totalConc[k], m_dTotalConc_dConc[k], m_kineticReactionRate[k], m_dKineticReactionRate_dConc[k], m_kineticSpeciesReactionRate[k], m_dKineticSpeciesReactionRate_dConc[k],
                    m_kineticReactions );

}

void EquilibratedChemicalFluid::PointUpdateKineticReactionRate( real64 const & temperature, arraySlice1d< real64 const > const & concentration, arraySlice1d< real64 const > const & surfaceArea0,
                                                                arraySlice1d< real64 const > const & volumeFraction0, arraySlice1d< real64 const > const & volumeFraction, real64 const & porosity0,
                                                                real64 const & porosity, localIndex const k )
{

  ComputeKineticReactionRate( temperature, concentration, surfaceArea0, volumeFraction0, volumeFraction, porosity0, porosity, m_kineticReactionRate[k], m_kineticReactions );

}

void EquilibratedChemicalFluid::ComputeKineticReactionRate( real64 const & temperature,
                                                            arraySlice1d< real64 const > const & concentration,
                                                            arraySlice1d< real64 const > const & surfaceArea0,
                                                            arraySlice1d< real64 const > const & volumeFraction0,
                                                            arraySlice1d< real64 const > const & volumeFraction,
                                                            real64 const & porosity0,
                                                            real64 const & porosity,
                                                            arraySlice1d< real64 > const & kineticReactionRate,
                                                            KineticReactions & kineticReactions )
{

  static const real64 RConst = 8.314;

  const array1d< KineticReaction > & kineticReactionArray = kineticReactions->GetKineticReactions();

  for( localIndex ir = 0; ir < kineticReactionArray.size(); ++ir )
  {

    const KineticReaction & kineticReaction = kineticReactionArray[ir];
    const array1d< localIndex > & basisSpeciesIndices = kineticReaction.basisSpeciesIndices;

    real64 SIndex = -kineticReaction.logK;

    for( localIndex ic = 0; ic < kineticReaction.stochs.size(); ++ic )
    {

      SIndex += kineticReaction.stochs[ic] * concentration[basisSpeciesIndices[ic] ];

    }

    real64 S = surfaceArea0[ir] * pow( volumeFraction[ir] / volumeFraction0[ir], 2.0/3.0 ) * pow( porosity / porosity0, 2.0/3.0 );

    real64 rateTemp = exp( -kineticReaction.E / RConst * (1.0 / (temperature + 273.15) - 1.0 / 298.15));

    real64 SS = (pow( 10.0, SIndex ) - 1.0);

    kineticReactionRate[ir] = S * kineticReaction.rateConst * rateTemp * SS;

  }

}


void EquilibratedChemicalFluid::ComputeChemistry( real64 const & temperature,
                                                  arraySlice1d< real64 const > const & concentration,
                                                  arraySlice1d< real64 const > const & surfaceArea0,
                                                  arraySlice1d< real64 const > const & volumeFraction0,
                                                  arraySlice1d< real64 const > const & volumeFraction,
                                                  real64 const & porosity0,
                                                  real64 const & porosity,
                                                  arraySlice1d< real64 const > const & concAct,
                                                  arraySlice1d< real64 const > const & dependentConc,
                                                  arraySlice2d< real64 const > const & dDependentConc_dConc,
                                                  arraySlice1d< real64 > const & totalConc,
                                                  arraySlice2d< real64 > const & dTotalConc_dConc,
                                                  arraySlice1d< real64 > const & kineticReactionRate,
                                                  arraySlice2d< real64 > const & dKineticReactionRate_dConc, arraySlice1d< real64 > const & kineticSpeciesReactionRate,
                                                  arraySlice2d< real64 > const & dKineticSpeciesReactionRate_dConc,
                                                  KineticReactions & kineticReactions )

{

  static const real64 RConst = 8.314;

  localIndex const NBasis = numBasisSpecies();
  localIndex const NDependent = numDependentSpecies();
  localIndex const NReaction = numKineticReaction();

  for( localIndex ic = 0; ic < NBasis; ++ic )
    for( localIndex jc = 0; jc < NBasis; ++jc )
      dTotalConc_dConc[ic][jc] = 0.0;

  for( localIndex ic = 0; ic < NBasis; ++ic )
  {

    real64 concBasis = pow( 10.0, concentration[ic] );

    totalConc[ic] = concBasis;

    dTotalConc_dConc[ic][ic] = log( 10.0 ) * concBasis;

    for( localIndex id = 0; id < NDependent; ++id )
    {
      real64 concDependent = pow( 10.0, dependentConc[id] );

      totalConc[ic] -= m_stochMatrix[ic][id] * concDependent;

      for( localIndex idc = 0; idc < NBasis; ++idc )
      {

        dTotalConc_dConc[ic][idc] -= m_stochMatrix[ic][id] * log( 10.0 ) * concDependent * dDependentConc_dConc[id][idc];

      }
    }
  }

  const array1d< KineticReaction > & kineticReactionArray = kineticReactions->GetKineticReactions();

  for( localIndex ir = 0; ir < NReaction; ++ir )
  {

    const KineticReaction & kineticReaction = kineticReactionArray[ir];
    const array1d< localIndex > & basisSpeciesIndices = kineticReaction.basisSpeciesIndices;

    real64 SIndex = -kineticReaction.logK;

    array1d< real64 > dSIndex( NBasis );
    for( localIndex ic = 0; ic < NBasis; ++ic )
      dSIndex[ic] = 0.0;

    for( localIndex ic = 0; ic < kineticReaction.stochs.size(); ++ic )
    {
      localIndex id = basisSpeciesIndices[ic];
      SIndex += kineticReaction.stochs[ic] * concAct[id];
      dSIndex[id] = kineticReaction.stochs[ic];

    }

    real64 S = surfaceArea0[ir] * pow( volumeFraction[ir] / volumeFraction0[ir], 2.0/3.0 ) * pow( porosity / porosity0, 2.0/3.0 );

    real64 rateTemp = exp( -kineticReaction.E / RConst * (1.0 / (temperature + 273.15) - 1.0 / 298.15));

    real64 SS = (pow( 10.0, SIndex ) - 1.0);

    kineticReactionRate[ir] = S * kineticReaction.rateConst * rateTemp * SS;

    for( localIndex ic = 0; ic < NBasis; ++ic )
    {

      dKineticReactionRate_dConc[ir][ic] = S * kineticReaction.rateConst * rateTemp * pow( 10.0, SIndex ) * log( 10.0 ) * dSIndex[ic];

    }

  }


  for( localIndex ic = 0; ic < NBasis; ++ic )
  {
    kineticSpeciesReactionRate[ic] = 0.0;

    for( localIndex id = 0; id < NBasis; ++id )
      dKineticSpeciesReactionRate_dConc[ic][id] = 0.0;
  }

  for( localIndex ir = 0; ir < NReaction; ++ir )
  {

    const KineticReaction & kineticReaction = kineticReactionArray[ir];
    const array1d< localIndex > & basisSpeciesIndices = kineticReaction.basisSpeciesIndices;

    for( localIndex i = 0; i < (kineticReaction.stochs).size(); ++i )
    {

      localIndex ic = basisSpeciesIndices[i];

      kineticSpeciesReactionRate[ic] += -(kineticReaction.stochs)[i] * kineticReactionRate[ir];

      for( localIndex id = 0; id < NBasis; ++id )
      {

        dKineticSpeciesReactionRate_dConc[ic][id] += -(kineticReaction.stochs)[i] * dKineticReactionRate_dConc[ir][id];

      }

    }

  }

}

REGISTER_CATALOG_ENTRY( ConstitutiveBase, EquilibratedChemicalFluid, std::string const &, Group * const )

} /* namespace constitutive */

} /* namespace geosx */
