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
 * @file Section.cpp
 */

#include "Section.hpp"
#include <algorithm>

namespace geos
{

Section::Section():
  m_rowMinWidth( 20 )
{}

void Section::setName( string_view title )
{
  m_sectionTitle = title;
}

void Section::addDescription( string const & description )
{
  m_descriptions.push_back( description );
}

void Section::setMinWidth( integer const & minWidth )
{
  m_rowMinWidth = minWidth;
}

void Section::computeMaxRowSize( string_view title,
                                 std::vector< string > const & rowsDescription )
{
  std::string const footerStr = "End : ";
  integer const titleLength = footerStr.length() + title.length() + m_marginBorder * 2 + m_nbSpecialChar * 2;

  m_rowLength = std::max( m_rowMinWidth, titleLength );

  if( rowsDescription.size() == 0 )
  {
    return;
  }

  auto it = std::max_element( rowsDescription.begin(),
                              rowsDescription.end(),
                              []( auto const & a, auto const & b ) {
    return a.size() < b.size();
  } );

  string const maxDescriptionSize = *it;

  integer const maxDescriptionLength = integer( maxDescriptionSize.length()) + m_marginBorder * 2 + m_nbSpecialChar * 2;

  m_rowLength = std::max( maxDescriptionLength, m_rowLength );
}

void Section::formatAndInsertDescriptions( std::string const & descriptionName,
                                           std::vector< string > const & descriptionValues )
{
  integer const descriptionLength = descriptionName.length() + m_embeddingName;
  m_descriptions.push_back( GEOS_FMT( "- {}: {}", descriptionName, descriptionValues[0] ) );
  for( size_t idxValue = 1; idxValue < descriptionValues.size(); idxValue++ )
  {
    m_descriptions.push_back( GEOS_FMT( "{:>{}}{}", " ",
                                        descriptionLength,
                                        descriptionValues[idxValue] ) );
  }
}

void Section::clear()
{
  m_descriptions.clear();
  m_sectionTitle.clear();
}

void Section::begin( std::ostream & oss )
{
  computeMaxRowSize( m_sectionTitle, m_descriptions );

  string const lineSection =  GEOS_FMT( "{:#>{}}\n", "", m_rowLength );
  integer const titleLength = m_rowLength - m_nbSpecialChar * 2;
  integer const descriptionLength = m_rowLength - m_nbSpecialChar * 2 - m_marginBorder;

  //build section title
  oss << '\n';
  oss << lineSection;
  oss << GEOS_FMT( "##{:^{}}##\n", m_sectionTitle, titleLength );

  //build section descriptions
  oss << lineSection;

  if( m_descriptions.empty())
  {
    oss << '\n';
    return;
  }

  for( string const & description : m_descriptions )
  {
    oss << GEOS_FMT( "##{:<{}}{:<{}}##", " ", m_marginBorder, description, descriptionLength );
    if( &description != &m_descriptions.back())
    {
      oss << '\n';
    }
    else
    {
      oss << "\n\n";
    }
  }
}

void Section::end( std::ostream & oss )
{
  std::string footerTitle = "End : " + m_sectionTitle;

  integer const titleLength = m_rowLength - m_nbSpecialChar * 2;
  string const lineSection = GEOS_FMT( "{:#^{}}\n", "", m_rowLength );

  oss << '\n';
  oss << GEOS_FMT( "##{:^{}}##\n", footerTitle, titleLength );
  oss << lineSection;
  oss << '\n';

  clear();
}
}
