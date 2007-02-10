/*
 *  cAnalyzeCommandDefBase.h
 *  Avida
 *
 *  Called "analyze_command_def_base.hh" prior to 12/2/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
 *
 */

#ifndef cAnalyzeCommandDefBase_h
#define cAnalyzeCommandDefBase_h

#ifndef cString_h
#include "cString.h"
#endif

class cAnalyze;
class cAnalyzeCommand;

class cAnalyzeCommandDefBase {
protected:
  cString name;
public:
  cAnalyzeCommandDefBase(const cString& _name) : name(_name) { ; }
  virtual ~cAnalyzeCommandDefBase() { ; }

  virtual void Run(cAnalyze* analyze, const cString & args, cAnalyzeCommand& command) const = 0;
  virtual bool IsFlowCommand() { return false; }

  const cString& GetName() const { return name; }
};


#ifdef ENABLE_UNIT_TESTS
namespace nAnalyzeCommandDefBase {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
