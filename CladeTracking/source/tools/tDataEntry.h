/*
 *  tDataEntry.h
 *  Avida
 *
 *  Called "tDataEntry.hh" prior to 12/7/05.
 *  Copyright 1999-2008 Michigan State University. All rights reserved.
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

#ifndef tDataEntry_h
#define tDataEntry_h

#include <iostream>

#ifndef cFlexVar_h
#include "cFlexVar.h"
#endif

#ifndef cString_h
#include "cString.h"
#endif

#ifndef cStringUtil_h
#include "cStringUtil.h"
#endif

#ifndef tDataEntryBase_h
#include "tDataEntryBase.h"
#endif

#if USE_tMemTrack
# ifndef tMemTrack_h
#  include "tMemTrack.h"
# endif
#endif

template <class T, class OUT> class tDataEntry : public tDataEntryBase<T> {
#if USE_tMemTrack
  tMemTrack<tDataEntry<T, OUT> > mt;
#endif
protected:
  OUT  (T::*DataGet)() const;
  void (T::*DataSet)(OUT);
  int  (T::*DataCompare)(T*) const;

//  int CmpNULL(T *) const { return 0; }
public:
  tDataEntry(const cString & _name, const cString & _desc,
	     OUT (T::*_funR)() const,
	     void (T::*_funS)(OUT _val) = NULL,
	     int _compare_type=0,
	     const cString & _null="0",
	     const cString & _html_cell="align=center")
    : tDataEntryBase<T>(_name, _desc, _compare_type, _null, _html_cell), DataGet(_funR),
      DataSet(_funS) {;}

  bool Set(const cString & value) {
    OUT new_value(0);
    if (DataSet == 0) return false;
    (this->target->*DataSet)( cStringUtil::Convert(value, new_value) );
    return true;
  }

  cFlexVar Get() const {
    assert(this->target != NULL);
    return cFlexVar( (this->target->*DataGet)() );
  }

  cFlexVar Get(const T * tmp_target) const {
    assert(tmp_target != NULL);
    return cFlexVar( (tmp_target->*DataGet)() );
  }
};

#endif
