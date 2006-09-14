/*
 *  cMutation.h
 *  Avida
 *
 *  Called "mutation.hh" prior to 12/5/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
 *
 */

#ifndef cMutation_h
#define cMutation_h

#ifndef cString_h
#include "cString.h"
#endif

class cMutation
{
private:
  cString name;
  int id;
  int trigger;
  int scope;
  int type;
  double rate;


  cMutation(); // @not_implemented
  
public:
  cMutation(const cString& _name, int _id, int _trigger, int _scope, int _type, double _rate)
    : name(_name), id(_id), trigger(_trigger), scope(_scope), type(_type), rate(_rate)
  {
  }
  ~cMutation() { ; }

  const cString & GetName() const { return name; }
  int GetID() const { return id; }
  int GetTrigger() const { return trigger; }
  int GetScope() const { return scope; }
  int GetType() const { return type; }
  double GetRate() const { return rate; }

  /*
  added to satisfy Boost.Python; the semantics are fairly useless --
  equality of two references means that they refer to the same object.
  */
  bool operator==(const cMutation &in) const { return &in == this; }
};


#ifdef ENABLE_UNIT_TESTS
namespace nMutation {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif

#endif
