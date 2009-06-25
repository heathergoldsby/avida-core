/*
 *  cBirthEntry.h
 *  Avida
 *
 *  Created by David Bryson on 4/1/09.
 *  Copyright 2009 Michigan State University. All rights reserved.
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef cBirthEntry_h
#define cBirthEntry_h

#ifndef cMerit_h
#include "cMerit.h"
#endif
#ifndef cMetaGenome_h
#include "cMetaGenome.h"
#endif

class cGenotype;


class cBirthEntry
{
public:
  cMetaGenome genome;
  double energy4Offspring;
  cMerit merit;
  cGenotype* parent_genotype;
  int timestamp; // -1 if empty
  
  inline cBirthEntry() : parent_genotype(NULL), timestamp(-1) { ; }
};

#endif
