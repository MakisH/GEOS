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
 * @file MultiFluidUpdate.hpp
 */
#ifndef GEOSX_PHYSICSSOLVERS_FLUIDFLOW_KERNELS_MULTIFLUIDUPDATE_HPP_
#define GEOSX_PHYSICSSOLVERS_FLUIDFLOW_KERNELS_MULTIFLUIDUPDATE_HPP_

#include "common/DataTypes.hpp"
#include "common/DataLayouts.hpp"
#include "constitutive/fluid/multifluid/Layouts.hpp"

namespace geos
{
namespace constitutive
{
class MultiFluidBase;
}

class MultiFluidUpdate
{
public:
/**
 * @brief Update properties for a single point
 * @details This single point update is rather expensive but it is only used in serial for
 *          initialisation within 1D tables.
 * @param fluid the fluid to use
 * @param cell the index of the element to update
 * @param node the index of the quadrature point
 * @param pressure the current pressure
 * @param temperature the current temperature
 * @param composition the current composition
 */
  static void update( constitutive::MultiFluidBase & fluid,
                      localIndex const index,
                      integer const node,
                      real64 const & pressure,
                      real64 const & temperature,
                      arraySlice1d< real64 const, compflow::USD_COMP - 1 > const & composition );

/**
 * @brief Update all the fluid properties
 * @param fluid the fluid to use
 * @param size the size of the data
 * @param pressure the current pressure
 * @param temperature the current temperature
 * @param composition the current composition
 */
  static void update( constitutive::MultiFluidBase & fluid,
                      localIndex const size,
                      arrayView1d< real64 const > const & pressure,
                      arrayView1d< real64 const > const & temperature,
                      arrayView2d< real64 const, compflow::USD_COMP > const & composition );

/**
 * @brief Update fluid properties for a selected set of points
 * @param fluid the fluid to use
 * @param targetSet the set of indices for the selected points
 * @param pressure the current pressure
 * @param temperature the current temperature
 * @param composition the current composition
 */
  static void update( constitutive::MultiFluidBase & fluid,
                      SortedArrayView< localIndex const > const & targetSet,
                      arrayView1d< real64 const > const & pressure,
                      arrayView1d< real64 const > const & temperature,
                      arrayView2d< real64 const, compflow::USD_COMP > const & composition );

private:
  template< typename FLUID_TYPE >
  struct Updater
  {
    static void update( typename FLUID_TYPE::KernelWrapper const & fluidWrapper,
                        localIndex const index,
                        integer const node,
                        real64 const & pressure,
                        real64 const & temperature,
                        arraySlice1d< real64 const, compflow::USD_COMP - 1 > const & composition );

    static void update( typename FLUID_TYPE::KernelWrapper const & fluidWrapper,
                        localIndex const size,
                        arrayView1d< real64 const > const & pressure,
                        arrayView1d< real64 const > const & temperature,
                        arrayView2d< real64 const, compflow::USD_COMP > const & composition );

    static void update( typename FLUID_TYPE::KernelWrapper const & fluidWrapper,
                        SortedArrayView< localIndex const > const & targetSet,
                        arrayView1d< real64 const > const & pressure,
                        arrayView1d< real64 const > const & temperature,
                        arrayView2d< real64 const, compflow::USD_COMP > const & composition );
  };
};

} //namespace geos

#endif //GEOSX_PHYSICSSOLVERS_FLUIDFLOW_KERNELS_MULTIFLUIDUPDATE_HPP_
