/*
 *  cScriptObject.h
 *  Avida
 *
 *  Created by David on 2/2/06.
 *  Copyright 2006 Michigan State University. All rights reserved.
 *
 */

#ifndef cScriptObject_h
#define cScriptObject_h

class cScriptObject
{
private:
  
public:
    cScriptObject() { ; }
};


#ifdef ENABLE_UNIT_TESTS
namespace nScriptObject {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
