//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2003 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#ifndef TASK_ENTRY_HH
#define TASK_ENTRY_HH

#ifndef STRING_HH
#include "cString.h"
#endif
#ifndef TASK_LIB_HH
#include "cTaskLib.h"
#endif

class cTaskEntry {
private:
  cString name;  // Short keyword for task
  cString desc;  // For more human-understandable output...
  int id;
  tTaskTest test_fun;
  cString info;  // extra info (like the string or whatever to match)
public:
  cTaskEntry(const cString & _name, const cString & _desc, int _id,
	     tTaskTest _test_fun, const cString & info);
  ~cTaskEntry();

  const cString & GetName()    const { return name; }
  const cString & GetDesc() const { return desc; }
  const int       GetID()      const { return id; }
  const tTaskTest GetTestFun() const { return test_fun; }
  const cString & GetInfo() const { return info; }
};

#endif
