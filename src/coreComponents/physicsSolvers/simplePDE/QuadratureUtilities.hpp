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
 * @file QuadratureUtilities.hpp
 */

#ifndef GEOS_PHYSICSSOLVERS_SIMPLEPDE_QUADRATUREUTILITIES_HPP_
#define GEOS_PHYSICSSOLVERS_SIMPLEPDE_QUADRATUREUTILITIES_HPP_

#include "CellUtilities.hpp"

namespace geos
{

namespace QuadratureUtilities
{

/**
 * @brief Integration rule type
 */
enum class Rule : integer
{
  Gauss
};

struct Data
{
  real64 wq;
  real64 Xiq[3];
};

template< typename CELL_TYPE,
          Rule RULE,
          int N_INTEGRATION_POINTS >
struct Helper
{};

template<>
struct Helper< HexahedronCell,
               Rule::Gauss,
               8 >
{
  GEOS_HOST_DEVICE
  static Data getData( int q )
  {
    real64 const val = 0.5773502691896257645092;
    int const a = q & 1;
    int const b = ( q & 2 ) >> 1;
    int const c = ( q & 4 ) >> 2;

    Data data;
    data.wq = 1.0; // weight  
    data.Xiq[0] = ( 2 * a - 1 ) * val;
    data.Xiq[1] = ( 2 * b - 1 ) * val;
    data.Xiq[2] = ( 2 * c - 1 ) * val;
    return data;
  }
};
                  


// getQuadratureData< CELL_TYPE, INTEGRATION_RULE, INTEGRATION_ORDER >

template< typename CELL_TYPE,
          Rule RULE_TYPE,
          int N_INTEGRATION_POINTS >
GEOS_HOST_DEVICE
static Data getData( int quadraturePointIndex )
{
  return Helper< CELL_TYPE, RULE_TYPE, N_INTEGRATION_POINTS >::getData( quadraturePointIndex );
}

/// Declare strings associated with enumeration values.
ENUM_STRINGS( Rule,
              "Gauss" );

} // namespace QuadratureUtilities

} // namespace geos

#endif // GEOS_PHYSICSSOLVERS_SIMPLEPDE_QUADRATUREUTILITIES_HPP_