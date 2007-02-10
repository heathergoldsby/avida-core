/*
 *  cHelpFullEntry.h
 *  Avida
 *
 *  Called "help_full_type.hh" prior to 12/7/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology
 *
 */

#ifndef cHelpFullEntry_h
#define cHelpFullEntry_h

#ifndef cHelpEntry_h
#include "cHelpEntry.h"
#endif
#ifndef cString_h
#include "cString.h"
#endif

class cHelpType;

class cHelpFullEntry : public cHelpEntry {
private:
  cHelpType* type;
  cString desc;
public:
  cHelpFullEntry(const cString & _name, cHelpType * _type, const cString _desc)
    : cHelpEntry(_name), type(_type), desc(_desc) { ; }
  cHelpFullEntry() : cHelpEntry(""), type(NULL) { ; }
  ~cHelpFullEntry() { ; }

  const cHelpFullEntry & operator=(const cHelpEntry & in_entry) {
    name = in_entry.GetName();
    type = in_entry.GetType();
    desc = in_entry.GetDesc();
    return *this;
  }

  const cString & GetKeyword() const { return name; }
  const cString & GetDesc() const { return desc; }
  cHelpType* GetType() const { return type; }

  bool IsAlias() const { return false; }
};


#ifdef ENABLE_UNIT_TESTS
namespace nHelpFullEntry {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
