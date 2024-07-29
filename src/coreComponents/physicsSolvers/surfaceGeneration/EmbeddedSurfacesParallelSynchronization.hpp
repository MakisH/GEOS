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
 * @file EmebeddedSurfacesParallelSynchronization.hpp
 */

#ifndef GEOS_PHYSICSSOLVERS_SURFACEGENERATION_EMBEDDEDSURFACESPARALLELSYNCHRONIZATION_HPP_
#define GEOS_PHYSICSSOLVERS_SURFACEGENERATION_EMBEDDEDSURFACESPARALLELSYNCHRONIZATION_HPP_

#include "physicsSolvers/surfaceGeneration/EmbeddedSurfaceGenerator.hpp"

namespace geos
{
class MeshLevel;
class NeighborCommunicator;
struct ModifiedObjectLists;

namespace embeddedSurfacesParallelSynchronization
{

void sychronizeTopology( MeshLevel & mesh,
                         std::vector< NeighborCommunicator > & neighbors,
                         NewObjectLists & newObjects,
                         int const mpiCommOrder,
                         string const fractureRegionName );
}

} /* namespace geos */

#endif /* GEOS_PHYSICSSOLVERS_SURFACEGENERATION_EMBEDDEDSURFACESPARALLELSYNCHRONIZATION_HPP_ */