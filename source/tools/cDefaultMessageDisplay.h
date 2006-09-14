/*
 *  cDefaultMessageDisplay.h
 *  Avida
 *
 *  Called "default_message_display.hh" prior to 12/7/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology
 *
 */

#ifndef cDefaultMessageDisplay_h
#define cDefaultMessageDisplay_h

#include <iostream>

#ifndef cMessageDisplay_h
#include "cMessageDisplay.h"
#endif

/*
This subclass of cMessageDisplay just prints to stdout and stderr.

cMessageDisplay displays the message "final_msg" to the user/programmer.
cMessageDisplay is meant to be subclassed, where
  out(string final_msg)
is reimplemented to display "final_msg" in the desired manner. 

Instances of the subclass may then be passed to
  setStandardOutDisplay(cMessageDisplay &)
and
  setStandardErrDisplay(cMessageDisplay &)
with expected results.
*/

class cString;

class cDefaultMessageDisplay : public cMessageDisplay
{
private:
  std::ostream* m_out;


  cDefaultMessageDisplay(); // @not_implemented
  cDefaultMessageDisplay(const cDefaultMessageDisplay&); // @not_implemented
  cDefaultMessageDisplay& operator=(const cDefaultMessageDisplay&); // @not_implemented

public:
  cDefaultMessageDisplay(std::ostream* stream) : m_out(stream) { ; }
  void out(cString& final_msg);
};


#ifdef ENABLE_UNIT_TESTS
namespace nDefaultMessageDisplay {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

extern cDefaultMessageDisplay s_info_msg_cout;
extern cDefaultMessageDisplay s_debug_msg_cerr;
extern cDefaultMessageDisplay s_error_msg_cerr;
extern cDefaultMessageDisplay s_fatal_msg_cerr;
extern cDefaultMessageDisplay s_noprefix_msg_cout;

#endif
