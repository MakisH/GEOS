/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 TotalEnergies
 * Copyright (c) 2020-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

#include "VTKFaceBlockUtilities.hpp"

#include "mesh/generators/VTKUtilities.hpp"

#include "dataRepository/Group.hpp"

#include <vtkCell.h>
#include <vtkDataArray.h>
#include <vtkFieldData.h>

#include <vtkExtractEdges.h>
#include <vtkGeometryFilter.h>
#include <vtkPointData.h>
#include <vtkCellData.h>

#include <algorithm>

namespace geos
{

namespace internal
{

/**
 * @brief This classes builds the mapping from global (vtk) element indices to the indices local to the cell blocks.
 * Then the global element to global faces can be be also computed.
 */
class ElementToFace
{
public:
  /**
   * @brief Builds from the cell blocks.
   * @param[in] cellBlocks The cell blocks group.
   */
  explicit ElementToFace( dataRepository::Group const & cellBlocks )
  {
    localIndex const numCellBlocks = cellBlocks.numSubGroups();
    for( int c = 0; c < numCellBlocks; ++c )
    {
      CellBlock const & cb = cellBlocks.getGroup< CellBlock >( c );
      auto const & l2g = cb.localToGlobalMapConstView();

      std::map< globalIndex, localIndex > g2l;

      for( auto l = 0; l < l2g.size(); ++l )
      {
        globalIndex const & g = l2g[l];
        m_elementToCellBlock[g] = c;
        g2l[g] = l;
      }

		m_cbe[c] = g2l;
		m_cbf[c] = cb.getElemToFacesConstView();
    }
  }

  /**
   * @brief Given the global (vtk) id @p ei, returns the cell block index it belongs to.
   * @param[in] ei global cell index.
   * @return The cell block index.
   */
  int getCellBlockIndex( vtkIdType const & ei ) const
  {
    return m_elementToCellBlock.at( ei );
  }

  /**
   * @brief From global index @p ei, returns the local element index in its cell block.
   * @param ei The global vtk element index.
   * @return The integer.
   */
  localIndex getElementIndexInCellBlock( vtkIdType const & ei ) const
  {
    localIndex const & cbi = getCellBlockIndex( ei );
    return m_cbe.at( cbi ).at( ei );
  }

  /**
   * @brief Returns the faces indices touching the global (vtk) element id.
   * @param[in] ei global cell index.
   * @return A container for the face indices of element @p ei.
   */
  auto operator[]( vtkIdType const & ei ) const
  {
    localIndex const & cbi = getCellBlockIndex( ei );
    localIndex const & e = m_cbe.at( cbi ).at( ei );
    arrayView2d< localIndex const > const & e2f = m_cbf.at( cbi );
    return e2f[e];
  }

private:
  /// global element index to the local cell block index
  std::map< globalIndex, localIndex > m_elementToCellBlock;  // TODO use a vector.

  /// Cell block index to a mapping from global element index to the local (to the cell block?) element index.
  std::map< localIndex , std::map< globalIndex, localIndex > > m_cbe;

  /// Cell block index to a mapping from global element index to the faces indices.
  std::map< localIndex , arrayView2d< localIndex const > > m_cbf;
};

} // end of namespace internal


/**
 * @brief Convenience wrapper around the raw vtk information.
 */
class CollocatedNodes
{
public:
  CollocatedNodes( string const & faceBlockName,
                   vtkSmartPointer< vtkDataSet > faceMesh )
  {
    vtkIdTypeArray const * collocatedNodes = vtkIdTypeArray::FastDownCast( faceMesh->GetPointData()->GetArray( vtk::COLLOCATED_NODES.c_str() ) );
    {
      // Depending on the parallel split, the vtk face mesh may be empty on a rank.
      // In that case, vtk will not provide any field for the emtpy mesh.
      // Therefore, not finding the duplicated nodes field on a rank cannot be interpreted as a globally missing field.
      // Converting the address into an integer and exchanging it through the MPI ranks let us find out
      // if the field is globally missing or not.
      std::uintptr_t const address = MpiWrapper::max( reinterpret_cast<std::uintptr_t>(collocatedNodes) );
      if( address == 0 )
      {
        GEOS_ERROR_IF( collocatedNodes == nullptr, "Could not find valid field \"" << vtk::COLLOCATED_NODES << "\" for fracture \"" << faceBlockName << "\"." );
      }
    }

    if( collocatedNodes )
    {
      init( collocatedNodes );
    }
  }

  /**
   * @brief For node @p i of the face block, returns all the duplicated global node indices in the main 3d mesh.
   * @param i the node in the face block (numbering is local to the face block).
   * @return The list of global node indices in the main 3d mesh.
   */
  std::vector< vtkIdType > const & operator[]( std::size_t i ) const
  {
    return m_collocatedNodes[i];
  }

  /**
   * @brief Number of duplicated nodes buckets.
   * Multiple nodes that are considered to be duplicated one of each other make one bucket.
   * @return The number of duplicated nodes buckets.
   */
  std::size_t size() const
  {
    return m_collocatedNodes.size();
  }

private:

  /**
   * @brief Populates the mapping.
   * @param collocatedNodes The pointer to the vtk data array. Guaranteed not to be null.
   * @details Converts the raw vtk data to STL containers.
   */
  void init( vtkIdTypeArray const * collocatedNodes )
  {
    vtkIdType const numTuples = collocatedNodes->GetNumberOfTuples();
    int const numComponents = collocatedNodes->GetNumberOfComponents();
    m_collocatedNodes.resize( numTuples );
    for( vtkIdType i = 0; i < numTuples; ++i )
    {
      m_collocatedNodes[i].reserve( numComponents );
    }

    for( vtkIdType i = 0; i < numTuples; ++i )
    {
      for( int j = 0; j < numComponents; ++j )
      {
        vtkIdType const tmp = collocatedNodes->GetTypedComponent( i, j );
        if( tmp > -1 )
        {
          m_collocatedNodes[i].emplace_back( tmp );
        }
      }
    }
  }

  /// For each node of the face block, lists all the duplicated nodes in the main 3d mesh.
  std::vector< std::vector< vtkIdType > > m_collocatedNodes;
};


/**
 * @brief Organize the duplicated nodes information as an LvArray ArrayOfArrys.
 * @param dn The duplicated nodes informations.
 * @return An iterable of arrays. Each array containing the global indices of the nodes which are duplicated of each others.
 */
ArrayOfArrays< globalIndex > buildCollocatedNodesMap( CollocatedNodes const & dn )
{
  ArrayOfArrays< globalIndex > result;

  std::vector< int > sizes( dn.size() );
  for( std::size_t i = 0; i < dn.size(); ++i )
  {
    sizes[i] = dn[i].size();
  }
  result.resizeFromCapacities< serialPolicy >( sizes.size(), sizes.data() );

  for( std::size_t i = 0; i < dn.size(); ++i )
  {
    for( globalIndex const & g: dn[i] )
    {
      result.emplaceBack( i, g );
    }
  }

  return result;
}


/**
 * @brief Build the mapping between the elements and their nodes.
 * @param mesh The mesh for which to compute the mapping.
 * @return The mapping.
 */
ArrayOfArrays< localIndex > build2dElemTo2dNodes( vtkSmartPointer< vtkDataSet > mesh )
{
  vtkIdType const numCells = mesh->GetNumberOfCells();
  std::vector< localIndex > sizes( numCells );
  for( auto i = 0; i < numCells; ++i )
  {
    sizes[i] = mesh->GetCell( i )->GetNumberOfPoints();
  }

  ArrayOfArrays< localIndex > result;
  result.resizeFromCapacities< geos::serialPolicy >( sizes.size(), sizes.data() );
  for( auto i = 0; i < numCells; ++i )
  {
    vtkIdList * const pointIds = mesh->GetCell( i )->GetPointIds();
    vtkIdType const numPoints = pointIds->GetNumberOfIds();
    for( int j = 0; j < numPoints; ++j )
    {
      result.emplaceBack( i, pointIds->GetId( j ) );
    }
  }

  return result;
}


/**
 * @brief Computes the bucket of all the collocated nodes for each 2d element.
 * @param elem2dTo2dNodes The 2d elements to its "2d" nodes mapping.
 * @param nodes2dToCollocatedNodes The "2d" nodes to their collocated nodes mapping.
 * @return The computed mapping. First dimension being the number of 2d elements.
 */
ArrayOfArrays< globalIndex > buildCollocatedNodesOf2dElemsMap( ArrayOfArrays< localIndex > const & elem2dTo2dNodes,
                                                               ArrayOfArrays< globalIndex > const & nodes2dToCollocatedNodes )
{
  ArrayOfArrays< globalIndex > result;
  result.resize( elem2dTo2dNodes.size() );
  for( localIndex e2d = 0; e2d < elem2dTo2dNodes.size(); ++e2d )
  {
    for( auto const & node: elem2dTo2dNodes[e2d] )
    {
      for( auto const collocatedNode: nodes2dToCollocatedNodes[node] )
      {
        result.emplaceBack( e2d, collocatedNode );
      }
    }
  }

  return result;
}


/**
 * @brief Some hash function for pairs of integers.
 */
struct pairHashComputer
{
  /**
   * @brief Computes the hash of a pair of integers.
   * @param p The input pair.
   * @return The hash.
   */
  size_t operator()( const std::pair< vtkIdType, vtkIdType > & p ) const
  {
    auto hash1 = std::hash< vtkIdType >{} ( p.first );
    auto hash2 = std::hash< vtkIdType >{} ( p.second );

    // If hash1 == hash2, their XOR is zero.
    return hash1 != hash2 ? hash1 ^ hash2: hash1;
  }
};


/**
 * @brief Compute the 2d faces (geometrical edges in 2d) to 2d elems (geometrical faces in 3d) mapping.
 * @param edges The edges of the fracture mesh.
 * @param faceMesh The fracture mesh.
 * @return The mapping
 */
ArrayOfArrays< localIndex > buildFace2dToElems2d( vtkPolyData * edges,
                                                  vtkSmartPointer< vtkDataSet > faceMesh )
{
  // Each edge is first associated to an hash and to an id.
  // Then we loop over all the edges of each cell and compute its hash to recover the associated id.
  std::unordered_map< std::pair< vtkIdType, vtkIdType >, int, pairHashComputer > face2dIds;
  for( int i = 0; i < edges->GetNumberOfCells(); ++i )
  {
    vtkCell * c = edges->GetCell( i );
    vtkIdList * edgePointIds = c->GetPointIds();
    GEOS_ASSERT( edgePointIds->GetNumberOfIds() == 2 );
    std::pair< vtkIdType, vtkIdType > const minMax = std::minmax( edgePointIds->GetId( 0 ), edgePointIds->GetId( 1 ) );
    face2dIds[minMax] = i;
  }

  ArrayOfArrays< localIndex > face2dToElems2d( LvArray::integerConversion< localIndex >( edges->GetNumberOfCells() ) );
  for( vtkIdType i = 0; i < faceMesh->GetNumberOfCells(); ++i )
  {
    vtkCell * c = faceMesh->GetCell( i );
    for( int j = 0; j < c->GetNumberOfEdges(); ++j )
    {
      vtkCell * e = c->GetEdge( j );
      vtkIdList * edgePointIds = e->GetPointIds();
      GEOS_ASSERT( edgePointIds->GetNumberOfIds() == 2 );
      std::pair< vtkIdType, vtkIdType > const minMax = std::minmax( edgePointIds->GetId( 0 ), edgePointIds->GetId( 1 ) );
      face2dToElems2d.emplaceBack( LvArray::integerConversion< localIndex >( face2dIds.at( minMax ) ), i );
    }
  }

  return face2dToElems2d;
}


/**
 * @brief For each 2d face (a segment in 3d), returns one single overlapping 3d edge (and not 2).
 * @param edges The edges as computed by vtk.
 * @param collocatedNodes The collocated nodes information.
 * @param nodeToEdges The node to edges mapping.
 * @return The 2d face to 3d edge mapping.
 *
 * @see FaceBlockABC::get2dFaceToEdge for more information.
 */
array1d< localIndex > buildFace2dToEdge( vtkSmartPointer< vtkDataSet > mesh,
                                         vtkPolyData * edges,
                                         CollocatedNodes const & collocatedNodes,
                                         ArrayOfArraysView< localIndex const > nodeToEdges )
{
  vtkIdTypeArray const * globalPtIds = vtkIdTypeArray::FastDownCast( mesh->GetPointData()->GetGlobalIds() );
  std::map< globalIndex , std::vector< localIndex > > n2e;
  for( auto i = 0; i < nodeToEdges.size(); ++i )
  {
    std::vector< localIndex > es;
    for( auto j = 0; j < nodeToEdges[i].size(); ++j )
    {
      es.push_back( nodeToEdges[i][j] );
    }
    n2e[globalPtIds->GetValue( i )] = es;
  }

  auto const comp = []( std::pair< vtkIdType, int > const & l, std::pair< vtkIdType, int > const & r ) { return l.second < r.second; };
  array1d< localIndex > face2dToEdge( edges->GetNumberOfCells() );
  // We loop over all the (duplicated) nodes of each edge.
  // Then, thanks to the node to edges mapping,
  // we can find all (ie 2) the edges that share (at max) 2 duplicated nodes.
  // Eventually we select one of these edges.
  for( int i = 0; i < edges->GetNumberOfCells(); ++i )
  {
    std::vector< vtkIdType > allDuplicatedNodesOfEdge;
    vtkCell * edge = edges->GetCell( i );
    for( int j = 0; j < edge->GetNumberOfPoints(); ++j )
    {
      for( auto const & d: collocatedNodes[ edge->GetPointId( j ) ] )
      {
        allDuplicatedNodesOfEdge.emplace_back( d );
      }
    }
    std::map< vtkIdType, int > edgeCount;
    for( auto const & d: allDuplicatedNodesOfEdge )
    {
      for( localIndex const & val: n2e[d] )  // TODO allDuplicatedNodesOfEdge contains global indices and nodeToEdges uses local indices as input key.
      {
        edgeCount[val]++;
      }
    }
    auto const res = std::max_element( edgeCount.cbegin(), edgeCount.cend(), comp );
    face2dToEdge[i] = LvArray::integerConversion< localIndex >( res->first );
  }

  return face2dToEdge;
}

/**
 * @brief Compute the 2d elements to 2d faces mapping (mainly by inverting @p face2dToElems2d).
 * @param num2dElements Number of (2d) elements in the fracture.
 * @param face2dToElems2d Which 2d faces touch which 2d elements.
 * @return The inverted mapping.
 */
ArrayOfArrays< localIndex > buildElem2dToFace2d( vtkIdType num2dElements,
                                                 ArrayOfArraysView< localIndex const > face2dToElems2d )
{
  // Inversion of the input mapping.
  ArrayOfArrays< localIndex > elem2dToFace2d( LvArray::integerConversion< localIndex >( num2dElements ) );
  for( localIndex face2dIndex = 0; face2dIndex < face2dToElems2d.size(); ++face2dIndex )
  {
    for( localIndex const & elem2dIndex: face2dToElems2d[face2dIndex] )
    {
      elem2dToFace2d.emplaceBack( elem2dIndex, face2dIndex );
    }
  }

  return elem2dToFace2d;
}

/**
 * @brief Builds the 2d element to edges mapping.
 * @param num2dElements Number of (2d) elements in the fracture.
 * @param face2dToEdge The 2d face to 3d edge mapping.
 * @param elem2dToFace2d The 2d element to 2d face mapping.
 * @return The computed mapping.
 *
 * @see FaceBlockABC::get2dElemToEdges for more information.
 */
ArrayOfArrays< localIndex > buildElem2dToEdges( vtkIdType num2dElements,
                                                arrayView1d< localIndex const > face2dToEdge,
                                                ArrayOfArraysView< localIndex const > elem2dToFace2d )
{
  ArrayOfArrays< localIndex > elem2dToEdges( LvArray::integerConversion< localIndex >( num2dElements ) );
  // Let's compose elem2dToFace2d with face2dToEdge to create elem2dToEdges.
  for( localIndex elemIndex = 0; elemIndex < elem2dToFace2d.size(); ++elemIndex )
  {
    for( auto const & face2dIndex: elem2dToFace2d[elemIndex] )
    {
      elem2dToEdges.emplaceBack( elemIndex, face2dToEdge[face2dIndex] );
    }
  }

  return elem2dToEdges;
}

/**
 * @brief Utility structure which connects 2d element to their 3d element and faces neighbors.
 */
struct Elem2dTo3dInfo
{
  ToCellRelation< ArrayOfArrays< localIndex > > elem2dToElem3d;
  ArrayOfArrays< localIndex > elem2dToFaces;

  Elem2dTo3dInfo( ToCellRelation< ArrayOfArrays< localIndex > > && elem2dToElem3d_,
                  ArrayOfArrays< localIndex > && elem2dToFaces_ )
    : elem2dToElem3d( elem2dToElem3d_ ),
    elem2dToFaces( elem2dToFaces_ )
  { }
};

/**
 * @brief Computes the mappings that link the 2d elements to their 3d element and faces neighbors.
 * @param faceMesh The face mesh.
 * @param mesh The 3d mesh.
 * @param collocatedNodes The collocated nodes information.
 * @param faceToNodes The (3d) face to nodes mapping.
 * @param elemToFaces The element to faces information.
 * @return All the information gathered into a single instance.
 */
Elem2dTo3dInfo buildElem2dTo3dElemAndFaces( vtkSmartPointer< vtkDataSet > faceMesh,
                                            vtkSmartPointer< vtkDataSet > mesh,
                                            CollocatedNodes const & collocatedNodes,
                                            ArrayOfArraysView< localIndex const > faceToNodes,
                                            geos::internal::ElementToFace const & elemToFaces )
{
  // First, we'll only consider the boundary cells,
  // since only boundary cells can be involved in this kind of computations.
  const char * const boundaryPointsName = "boundary points";
  const char * const boundaryCellsName = "boundary cells";

  auto boundaryExtractor = vtkSmartPointer< vtkGeometryFilter >::New();
  boundaryExtractor->PassThroughPointIdsOn();
  boundaryExtractor->PassThroughCellIdsOn();
  boundaryExtractor->FastModeOff();
  boundaryExtractor->SetOriginalPointIdsName( boundaryPointsName );
  boundaryExtractor->SetOriginalCellIdsName( boundaryCellsName );
  boundaryExtractor->SetInputData( mesh );
  boundaryExtractor->Update();
  vtkPolyData * boundary = boundaryExtractor->GetOutput();

  vtkIdTypeArray const * boundaryPoints = vtkIdTypeArray::FastDownCast( boundary->GetPointData()->GetArray( boundaryPointsName ) );
  vtkIdTypeArray const * boundaryCells = vtkIdTypeArray::FastDownCast( boundary->GetCellData()->GetArray( boundaryCellsName ) );

  vtkIdTypeArray const * globalPtIds = vtkIdTypeArray::FastDownCast( mesh->GetPointData()->GetGlobalIds() );
  vtkIdTypeArray const * globalCellIds = vtkIdTypeArray::FastDownCast( mesh->GetCellData()->GetGlobalIds() );

  // Let's build the elem2d to elem3d mapping. We need to find the 3d elements!
  // First we compute the mapping from all the boundary nodes to the 3d elements that rely on those nodes.
  std::map< vtkIdType, std::vector< vtkIdType > > nodesToCellsFull;
  for( vtkIdType i = 0; i < boundary->GetNumberOfCells(); ++i )
  {
    vtkIdType const cellId = boundaryCells->GetValue( i );
    vtkIdList * pointIds = boundary->GetCell( i )->GetPointIds();
    for( int j = 0; j < pointIds->GetNumberOfIds(); ++j )
    {
      vtkIdType const pointId = boundaryPoints->GetValue( pointIds->GetId( j ) );
      nodesToCellsFull[globalPtIds->GetValue( pointId )].emplace_back( globalCellIds->GetValue( cellId ) );
    }
  }

  // Then we only keep the duplicated nodes. It's only for optimisation purpose.
  std::map< vtkIdType, std::set< vtkIdType > > nodesToCells;
  { // scope reduction
    std::set< vtkIdType > allDuplicatedNodes;
    for( std::size_t i = 0; i < collocatedNodes.size(); ++i )
    {
      std::vector< vtkIdType > const & ns = collocatedNodes[ i ];
      allDuplicatedNodes.insert( ns.cbegin(), ns.cend() );
    }

    for( vtkIdType const & n: allDuplicatedNodes )
    {
      auto const iter = nodesToCellsFull.find( n );
      if( iter != nodesToCellsFull.cend() )
      {
        std::vector< vtkIdType > const & tmp = iter->second;
        std::set< vtkIdType > const cells{ tmp.cbegin(), tmp.cend() };
        nodesToCells[n] = cells;
      }
    }
  }

  vtkIdType const num2dElements = faceMesh->GetNumberOfCells();

  ArrayOfArrays< localIndex > elem2dToElem3d( num2dElements, 2 );
  ArrayOfArrays< localIndex > elem2dToCellBlock( num2dElements, 2 );
  ArrayOfArrays< localIndex > elem2dToFaces( num2dElements, 2 );

  // Now we loop on all the 2d elements.
  for( int i = 0; i < num2dElements; ++i )
  {
    // We collect all the duplicated points that are involved for each 2d element.
    vtkIdList * pointIds = faceMesh->GetCell( i )->GetPointIds();
    std::size_t const elem2dNumPoints = pointIds->GetNumberOfIds();
    // All the duplicated points of the 2d element. Note that we lose the collocation of the duplicated nodes.
    std::set< vtkIdType > duplicatedPointOfElem2d;
    for( vtkIdType j = 0; j < pointIds->GetNumberOfIds(); ++j )
    {
      std::vector< vtkIdType > const & ns = collocatedNodes[ pointIds->GetId( j ) ];
      duplicatedPointOfElem2d.insert( ns.cbegin(), ns.cend() );
    }

    // Here, we collect all the 3d elements that are concerned by at least one of those duplicated elements.
    std::map< vtkIdType, std::set< vtkIdType > > elem3dToDuplicatedNodes;
    for( vtkIdType const & n: duplicatedPointOfElem2d )
    {
      auto const ncs = nodesToCells.find( n );
      if( ncs != nodesToCells.cend() )
      {
        for( vtkIdType const & c: ncs->second )
        {
          elem3dToDuplicatedNodes[c].insert( n );
        }
      }
    }
    // Last we extract which of those candidate 3d elements are the ones actually neighboring the 2d element.
    for( auto const & e2n: elem3dToDuplicatedNodes )
    {
      // If the face of the element 3d has the same number of nodes than the elem 2d, it should be a successful (the mesh is conformal).
      if( e2n.second.size() == elem2dNumPoints )
      {
        // Now we know that the element 3d has a face that touches the element 2d. Let's find which one.
        elem2dToElem3d.emplaceBack( i, elemToFaces.getElementIndexInCellBlock( e2n.first ) );
        // Computing the elem2dToFaces mapping.
        auto faces = elemToFaces[e2n.first];
        for( int j = 0; j < faces.size( 0 ); ++j )
        {
          localIndex const faceIndex = faces[j];
          auto nodes = faceToNodes[faceIndex];
          std::set< vtkIdType > globalNodes;
          for( auto const & n: nodes )
          {
            globalNodes.insert( globalPtIds->GetValue( n ) );
          }
          if( globalNodes == e2n.second )
          {
            elem2dToFaces.emplaceBack( i, faceIndex );
            elem2dToCellBlock.emplaceBack( i, elemToFaces.getCellBlockIndex( e2n.first ) );
            break;
          }
        }
      }
    }
  }

  auto cellRelation = ToCellRelation< ArrayOfArrays< localIndex > >( std::move( elem2dToCellBlock ), std::move( elem2dToElem3d ) );
  return Elem2dTo3dInfo( std::move( cellRelation ), std::move( elem2dToFaces ) );
}


/**
 * @brief Computes the 2d element to nodes mapping.
 * @param num2dElements Number of (2d) elements in the fracture.
 * @param faceToNodes The face to nodes mapping.
 * @param elem2dToFaces The 2d element to faces mapping.
 * @return The computed mapping.
 */
ArrayOfArrays< localIndex > buildElem2dToNodes( vtkIdType num2dElements,
                                                ArrayOfArraysView< localIndex const > faceToNodes,
                                                ArrayOfArraysView< localIndex const > elem2dToFaces )
{
  ArrayOfArrays< localIndex > elem2dToNodes( LvArray::integerConversion< localIndex >( num2dElements ) );
  for( localIndex elem2dIndex = 0; elem2dIndex < elem2dToFaces.size(); ++elem2dIndex )
  {
    for( localIndex const & faceIndex: elem2dToFaces[elem2dIndex] )
    {
      if( faceIndex < 0 )
      { continue; }
      std::set< localIndex > tmp;
      for( auto j = 0; j < faceToNodes[faceIndex].size(); ++j )
      {
        localIndex const & nodeIndex = faceToNodes[faceIndex][j];
        tmp.insert( nodeIndex );
      }
      for( localIndex const & nodeIndex: tmp )
      {
        elem2dToNodes.emplaceBack( elem2dIndex, nodeIndex );
      }
    }
  }

  return elem2dToNodes;
}


/**
 * @brief Computes the local to global mappings for the 2d elements of the face mesh.
 * @param faceMesh The face mesh for which to compute the mapping.
 * @param mesh The volumic mesh.
 * @return The mapping as an array.
 * @details The vtk global ids of the elements of the @p faceMesh are used,
 * but they are shifted by the maximum element global id of the @p mesh,
 * to avoid any collision.
 */
array1d< globalIndex > buildLocalToGlobal( vtkSmartPointer< vtkDataSet > faceMesh,
                                           vtkSmartPointer< vtkDataSet > mesh )
{
  array1d< globalIndex > l2g( faceMesh->GetNumberOfCells() );

  // In order to avoid any cell global id collision, we gather the max cell global id over all the ranks.
  // Then we use this maximum as on offset.
  // TODO This does not take into account multiple fractures.
  vtkIdType const maxLocalCellId = vtkIdTypeArray::FastDownCast( mesh->GetCellData()->GetGlobalIds() )->GetMaxId();
  vtkIdType const maxGlobalCellId = MpiWrapper::max( maxLocalCellId );
  vtkIdType const cellGlobalOffset = maxGlobalCellId + 1;

  vtkIdTypeArray const * globalIds = vtkIdTypeArray::FastDownCast( faceMesh->GetCellData()->GetGlobalIds() );
  for( auto i = 0; i < l2g.size(); ++i )
  {
    l2g[i] = globalIds->GetValue( i ) + cellGlobalOffset;
  }

  return l2g;
}


/**
 * @brief Convert the @p faceMesh into a @p FaceBlock and registers it into the @p cellBlockManager.
 * @param faceMesh The mesh representing the fracture.
 * @param mesh The volumic mesh (for neighboring and global indices purposes).
 * @param cellBlockManager The manager in which the resulting @p FaceBlock will be stored.
 */
void importFractureNetwork( string const & faceBlockName,
                            vtkSmartPointer< vtkDataSet > faceMesh,
                            vtkSmartPointer< vtkDataSet > mesh,
                            CellBlockManager & cellBlockManager )
{
  ArrayOfArrays< localIndex > const faceToNodes = cellBlockManager.getFaceToNodes();
  geos::internal::ElementToFace const elemToFaces( cellBlockManager.getCellBlocks() );
  ArrayOfArrays< localIndex > const nodeToEdges = cellBlockManager.getNodeToEdges();

  CollocatedNodes const collocatedNodes( faceBlockName, faceMesh );
  // Add the appropriate validations (only 2d cells...)

  auto edgesExtractor = vtkSmartPointer< vtkExtractEdges >::New();
  edgesExtractor->SetInputData( faceMesh );
  edgesExtractor->UseAllPointsOn();  // Important: we want to prevent any node renumbering.
  edgesExtractor->Update();
  vtkPolyData * edges = edgesExtractor->GetOutput();

  vtkIdType const num2dFaces = edges->GetNumberOfCells();
  vtkIdType const num2dElements = faceMesh->GetNumberOfCells();
  // Now let's build the elem2dTo* mappings.
  Elem2dTo3dInfo elem2dTo3d = buildElem2dTo3dElemAndFaces( faceMesh, mesh, collocatedNodes, faceToNodes.toViewConst(), elemToFaces );
  ArrayOfArrays< localIndex > elem2dToNodes = buildElem2dToNodes( num2dElements, faceToNodes.toViewConst(), elem2dTo3d.elem2dToFaces.toViewConst() );

  ArrayOfArrays< localIndex > face2dToElems2d = buildFace2dToElems2d( edges, faceMesh );
  array1d< localIndex > face2dToEdge = buildFace2dToEdge( mesh, edges, collocatedNodes, nodeToEdges.toViewConst() );
  ArrayOfArrays< localIndex > const elem2dToFace2d = buildElem2dToFace2d( num2dElements, face2dToElems2d.toViewConst() );
  ArrayOfArrays< localIndex > elem2DToEdges = buildElem2dToEdges( num2dElements, face2dToEdge.toViewConst(), elem2dToFace2d.toViewConst() );

  // Mappings are now computed. Just create the face block by value.
  FaceBlock & faceBlock = cellBlockManager.registerFaceBlock( faceBlockName );

  faceBlock.setNum2dElements( num2dElements );
  faceBlock.setNum2dFaces( num2dFaces );
  faceBlock.set2dElemToNodes( std::move( elem2dToNodes ) );
  faceBlock.set2dElemToEdges( std::move( elem2DToEdges ) );
  faceBlock.set2dFaceToEdge( std::move( face2dToEdge ) );
  faceBlock.set2dFaceTo2dElems( std::move( face2dToElems2d ) );

  faceBlock.set2dElemToFaces( std::move( elem2dTo3d.elem2dToFaces ) );
  faceBlock.set2dElemToElems( std::move( elem2dTo3d.elem2dToElem3d ) );

  faceBlock.setLocalToGlobalMap( buildLocalToGlobal( faceMesh, mesh ) );
  // TODO there are build* functions and compute* functions.
  ArrayOfArrays< globalIndex > cn = buildCollocatedNodesMap( collocatedNodes );
  ArrayOfArrays< globalIndex > cne = buildCollocatedNodesOf2dElemsMap( build2dElemTo2dNodes( faceMesh ), cn );

  faceBlock.setCollocatedNodes( std::move( cn ) );
  faceBlock.setCollocatedNodesOf2dElems( std::move( cne ) );
}

} // end of namespace
