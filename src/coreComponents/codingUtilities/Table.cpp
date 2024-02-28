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
 * @file Table.cpp
 */

#include "Table.hpp"

namespace geos
{

/**
 * @brief Build a value cell given an alignment and spaces from "|"
 *
 * @param alignment
 * @param value
 * @param spaces
 * @return A cell value
 */
string buildValueCell( Table::Alignment const alignment, string_view value, integer const spaces )
{
  switch( alignment )
  {
    case Table::right:   return GEOS_FMT( "{:>{}}", value, spaces );
    case Table::left:    return GEOS_FMT( "{:<{}}", value, spaces );
    case Table::middle:  return GEOS_FMT( "{:^{}}", value, spaces );
    default:             return GEOS_FMT( "{:<{}}", value, spaces );
  }
}

Table::Table( std::vector< string > const & headers )
{
  setMargin( MarginValue::medium );

  for( size_t idx = 0; idx< headers.size(); idx++ )
  {
    m_columns.push_back( {Table::ColumnParam{{headers[idx]}, Alignment::middle, true}, {}, ""} );
  }
}

Table::Table( std::vector< ColumnParam > const & columnParameter )
{
  setMargin( MarginValue::medium );

  for( size_t idx = 0; idx< columnParameter.size(); idx++ )
  {
    if( columnParameter[idx].enabled )
    {
      m_columns.push_back( {columnParameter[idx], {}, ""} );
    }

  }
}

void Table::addRowsFromVectors( std::vector< std::vector< string > > const & tableRows )
{
  for( size_t indexRow = 0; indexRow < tableRows.size(); indexRow++ )
  {
    std::vector< string > rowsValues;
    for( size_t indexValue = 0; indexValue < tableRows[indexRow].size(); indexValue++ )
    {
      string cellValue = GEOS_FMT( "{}", tableRows[indexRow][indexValue] );
      if( m_columns[indexValue].parameter.enabled )
      {
        rowsValues.push_back( cellValue );
      }
    }
    m_cellsRows.push_back( rowsValues );
  }
}

void Table::parseAndStoreHeaderSections( size_t & largestHeaderVectorSize,
                                         std::vector< std::vector< string > > & splitHeader )
{
  for( size_t columnParamIdx = 0; columnParamIdx< m_columns.size(); columnParamIdx++ )
  {
    std::vector< string > splitHeaderParts;
    std::istringstream ss( m_columns[columnParamIdx].parameter.headerName[0] );
    string subHeaderName;

    while( getline( ss, subHeaderName, '\n' ))
    {
      splitHeaderParts.push_back( subHeaderName );
    }

    size_t const cellSize = splitHeaderParts.size();
    largestHeaderVectorSize = std::max( largestHeaderVectorSize, cellSize );

    splitHeader.push_back( splitHeaderParts );
  }
}

void Table::adjustHeaderSizesAndStore( size_t largestHeaderVectorSize,
                                       std::vector< std::vector< string > > & splitHeader )
{
  for( size_t columnParamIdx = 0; columnParamIdx < m_columns.size(); columnParamIdx++ )
  {
    if( splitHeader[columnParamIdx].size() < largestHeaderVectorSize )
    {
      integer const whiteRowToAdd = largestHeaderVectorSize - splitHeader[columnParamIdx].size();
      splitHeader[columnParamIdx].insert( splitHeader[columnParamIdx].end(), whiteRowToAdd, " " );
    }
    m_columns[columnParamIdx].parameter.headerName = splitHeader[columnParamIdx];
  }
}

void Table::setTitle( string_view title_ )
{
  tableTitle = title_;
}

string_view Table::getTitle()
{
  return tableTitle;
}

void Table::setMargin( MarginValue marginType )
{
  borderMargin = marginType;
  columnMargin = integer( marginType ) * 2 + 1;
}

void Table::findAndSetMaxStringSize()
{
  string maxStringSize = "";
  for( size_t idxColumn  = 0; idxColumn <  m_columns.size(); idxColumn++ )
  {
    auto it = std::max_element( m_columns[idxColumn].parameter.headerName.begin(),
                                m_columns[idxColumn].parameter.headerName.end(),
                                []( const auto & a, const auto & b ) {
      return a.size() < b.size();
    } );

    maxStringSize = *it;

    for( size_t idxRow = 0; idxRow <  m_cellsRows.size(); idxRow++ )
    {
      string cell =  m_columns[idxColumn].columnValues[idxRow];
      if( maxStringSize.length() < cell.length())
      {
        maxStringSize = cell;
      }
    }
    m_columns[idxColumn].m_maxStringSize = maxStringSize;
  }
}

void Table::computeAndSetMaxStringSize( string::size_type sectionlineLength,
                                        string::size_type titleLineLength )
{
  integer extraLinesPerColumn;
  integer extraLines;
  integer newStringSize;

  extraLines = titleLineLength - sectionlineLength;
  extraLinesPerColumn = std::ceil( extraLines / m_columns.size() );

  for( std::size_t idxColumn = 0; idxColumn < m_columns.size(); ++idxColumn )
  {
    newStringSize = extraLinesPerColumn + m_columns[idxColumn].m_maxStringSize.size();
    if( idxColumn == m_columns.size() - 1 ||  m_columns.size() == 1 )
    {
      m_columns[idxColumn].m_maxStringSize = GEOS_FMT( "{:>{}}",
                                                       m_columns[idxColumn].m_maxStringSize,
                                                       newStringSize + columnMargin );
    }
    else
    {
      m_columns[idxColumn].m_maxStringSize = GEOS_FMT( "{:>{}}",
                                                       m_columns[idxColumn].m_maxStringSize,
                                                       newStringSize );
    }
  }
}

void Table::computeAndBuildSeparator( string & topSeparator, string & sectionSeparator )
{
  string::size_type sectionlineLength = 0;
  string::size_type titleLineLength = tableTitle.length() + ( marginTitle * 2 );
  integer nbSpaceBetweenColumn = ( ( m_columns.size() - 1 ) *  columnMargin ) + (borderMargin * 2);
  if( !tableTitle.empty())
  {
    tableTitle = GEOS_FMT( "{:^{}}", tableTitle, titleLineLength );
  }

  for( std::size_t i = 0; i < m_columns.size(); ++i )
  {
    sectionlineLength += m_columns[i].m_maxStringSize.length();
  }

  sectionlineLength += nbSpaceBetweenColumn;
  if( sectionlineLength < titleLineLength )
  {
    computeAndSetMaxStringSize( sectionlineLength, titleLineLength );
  }
  if( m_columns.size() == 1 )
  {
    sectionSeparator +=  GEOS_FMT( "+{:-<{}}+",
                                   "",
                                   ( m_columns[0].m_maxStringSize.length() + (borderMargin - 1) + columnMargin ));
  }
  else
  {
    for( std::size_t idxColumn = 0; idxColumn < m_columns.size(); ++idxColumn )
    {
      integer const cellSize = m_columns[idxColumn].m_maxStringSize.length();
      if( idxColumn == 0 )
      {
        sectionSeparator +=  GEOS_FMT( "+{:-<{}}", "", ( cellSize + borderMargin ));
      }
      else if( idxColumn == (m_columns.size() - 1))
      {
        sectionSeparator += GEOS_FMT( "{:-^{}}", "+", columnMargin );
        sectionSeparator += GEOS_FMT( "{:->{}}", "+", ( cellSize + borderMargin + 1 ) );
      }
      else
      {
        sectionSeparator += GEOS_FMT( "{:-^{}}", "+", columnMargin );
        sectionSeparator += GEOS_FMT( "{:->{}}", "", cellSize );
      }
    }
  }
  topSeparator = GEOS_FMT( "+{:-<{}}+", "", sectionSeparator.size() - 2 );// -2 for ++
}

void Table::buildTitleRow( string & titleRows, string_view topSeparator, string_view sectionSeparator )
{
  titleRows = GEOS_FMT( "\n{}\n|", topSeparator );
  titleRows +=  buildValueCell( Alignment::middle,
                                tableTitle,
                                (sectionSeparator.length() - 2) // -2 for ||
                                );
  titleRows += GEOS_FMT( "{}\n", "|" );
}

void Table::buildSectionRows( string_view sectionSeparator,
                              string & rows,
                              integer const nbRows,
                              Section const section )
{
  for( integer idxRow = 0; idxRow< nbRows; idxRow++ )
  {
    rows += GEOS_FMT( "{:<{}}", "|", 1 +  borderMargin );
    for( std::size_t idxColumn = 0; idxColumn < m_columns.size(); ++idxColumn )
    {
      string cell;

      if( section == Section::header )
      {
        cell = m_columns[idxColumn].parameter.headerName[idxRow];
      }
      else
      {
        cell = m_columns[idxColumn].columnValues[idxRow];
      }
      integer const cellSize = m_columns[idxColumn].m_maxStringSize.length();
      rows += buildValueCell( m_columns[idxColumn].parameter.alignment,
                              cell,
                              cellSize );

      if( idxColumn < m_columns.size() - 1 )
      {
        rows += GEOS_FMT( "{:^{}}", "|", columnMargin );
      }

    }
    if( m_columns.size() == 1 )
    {
      rows +=  GEOS_FMT( "{:>{}}\n", "|", columnMargin );
    }
    else
    {
      rows += GEOS_FMT( "{:>{}}\n", "|", borderMargin + 1 );
    }

  }
  if( nbRows != 0 )
  {
    rows += GEOS_FMT( "{}\n", sectionSeparator );
  }
}

void Table::fillColumnsValuesFromCellsRows()
{
  for( size_t idxRow = 0; idxRow < m_cellsRows.size(); idxRow++ )
  {
    for( size_t idxColumn = 0; idxColumn < m_columns.size(); idxColumn++ )
    {
      m_columns[idxColumn].columnValues.push_back( m_cellsRows[idxRow][idxColumn] );
    }
  }
}

void Table::draw( std::ostream & oss )
{
  string rows;
  string titleRows;
  string topSeparator;
  string sectionSeparator;

  std::vector< std::vector< string > > splitHeader;
  size_t largestHeaderVectorSize = 0;


  fillColumnsValuesFromCellsRows();
  parseAndStoreHeaderSections( largestHeaderVectorSize, splitHeader );
  adjustHeaderSizesAndStore( largestHeaderVectorSize, splitHeader );

  findAndSetMaxStringSize();
  computeAndBuildSeparator( topSeparator, sectionSeparator );

  if( !tableTitle.empty())
  {
    buildTitleRow( titleRows, topSeparator, sectionSeparator );
  }

  rows += GEOS_FMT( "{}\n", sectionSeparator );
  buildSectionRows( sectionSeparator, rows, largestHeaderVectorSize, Section::header );
  buildSectionRows( sectionSeparator, rows, m_cellsRows.size(), Section::values );

  string const tableOutput = titleRows + rows + '\n';

  oss << tableOutput;
}

}