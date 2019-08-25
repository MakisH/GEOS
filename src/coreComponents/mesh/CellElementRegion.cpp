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
 * @file CellElementRegion.cpp
 */

#include "CellElementRegion.hpp"

namespace geosx
{
using namespace dataRepository;

CellElementRegion::CellElementRegion( string const & name, ManagedGroup * const parent ):
  ElementRegionBase( name, parent )
{}

CellElementRegion::~CellElementRegion()
{}

REGISTER_CATALOG_ENTRY( ObjectManagerBase, CellElementRegion, std::string const &, ManagedGroup * const )

} /* namespace geosx */
