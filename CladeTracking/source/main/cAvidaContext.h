/*
 *  cAvidaContext.h
 *  Avida
 *
 *  Created by David on 3/13/06.
 *  Copyright 1999-2008 Michigan State University. All rights reserved.
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
 *  along with this program; if not, write to the Free Software Foundation, 
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*! Class to to hold a random number generator.  Used to keep random
    number creation in a given context seperate from rest of the program
    helps with reapeatablity???? */

#ifndef cAvidaContext_h
#define cAvidaContext_h

#if USE_tMemTrack
# ifndef tMemTrack_h
#  include "tMemTrack.h"
# endif
#endif


class cRandom;

class cAvidaContext
{
#if USE_tMemTrack
  tMemTrack<cAvidaContext> mt;
#endif
private:
  cRandom* m_rng;
  bool m_analyze;
  
public:
  cAvidaContext(cRandom& rng) : m_rng(&rng), m_analyze(false) { ; }
  cAvidaContext(cRandom* rng) : m_rng(rng), m_analyze(false) { ; }
  ~cAvidaContext() { ; }
  
  void SetRandom(cRandom& rng) { m_rng = &rng; }  
  void SetRandom(cRandom* rng) { m_rng = rng; }  
  cRandom& GetRandom() { return *m_rng; }
  
  void SetAnalyzeMode() { m_analyze = true; }
  void ClearAnalyzeMode() { m_analyze = false; }
  bool GetAnalyzeMode() { return m_analyze; }
};

#endif
