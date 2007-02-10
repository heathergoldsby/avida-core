/*
 *  cMessageClass.h
 *  Avida
 *
 *  Called "message_class.hh" prior to 12/7/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology
 *
 */

#ifndef cMessageClass_h
#define cMessageClass_h

class cMessageDisplay;
class cMessageType;

class cMessageClass {
public:
  cMessageClass(
    const char* class_name,
    cMessageDisplay** msg_display,
    bool is_fatal,
    bool is_prefix,
    bool no_prefix
  );
public:
  void configure(cMessageType* message_type);
public:
  const char* const m_class_name;
  cMessageDisplay** m_msg_display;
  bool const m_is_fatal;
  bool const m_is_prefix;
  bool const m_no_prefix;
private:
  bool _configured;
};


#ifdef ENABLE_UNIT_TESTS
namespace nMessageClass {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

/*
Declaration of the five message classes.
*/
extern cMessageClass MCInfo;
extern cMessageClass MCDebug;
extern cMessageClass MCError;
extern cMessageClass MCFatal;
extern cMessageClass MCNoPrefix;

#endif
