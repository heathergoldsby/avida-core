/*
 *  tDataEntryBase.h
 *  Avida
 *
 *  Called "tDataEntryBase.hh" prior to 12/7/05.
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
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

#ifndef tDataEntryBase_h
#define tDataEntryBase_h

#include <iostream>
#include <sstream>

#ifndef cDataEntry_h
#include "cDataEntry.h"
#endif

#ifndef cFlexVar_h
#include "cFlexVar.h"
#endif

#if USE_tMemTrack
# ifndef tMemTrack_h
#  include "tMemTrack.h"
# endif
#endif

class cString;

template <class T> class tDataEntryBase : public cDataEntry {
#if USE_tMemTrack
  tMemTrack<tDataEntryBase<T> > mt;
#endif
protected:
  T * target;
public:
  tDataEntryBase(const cString & _name, const cString & _desc, int _compare_type,
		 const cString & _null="0",
		 const cString & _html_cell="align=center")
    : cDataEntry(_name, _desc, _compare_type, _null, _html_cell), target(NULL) { ; }
  
  void SetTarget(T * _target) { target = _target; }
  virtual bool Set(const cString & value) { (void) value; return false; }
  virtual cFlexVar Get() const = 0;
  virtual cFlexVar Get(const T * tmp_target) const = 0;
};

#endif