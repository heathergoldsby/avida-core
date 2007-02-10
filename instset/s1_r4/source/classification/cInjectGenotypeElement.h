/*
 *  cInjectGenotypeElement.h
 *  Avida
 *
 *  Called "inject_genotype_element.hh" prior to 11/30/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
 *
 */

#ifndef cInjectGenotypeElement_h
#define cInjectGenotypeElement_h

#ifndef defs_h
#include "defs.h"
#endif

class cInjectGenotype;

class cInjectGenotypeElement {
private:
  cInjectGenotype* inject_genotype;
  cInjectGenotypeElement* next;
  cInjectGenotypeElement* prev;
  
  cInjectGenotypeElement(const cInjectGenotypeElement&); // @not_implemented
  cInjectGenotypeElement& operator=(const cInjectGenotypeElement&); // @not_implemented
public:
  cInjectGenotypeElement(cInjectGenotype* in_gen = NULL)
    : inject_genotype(in_gen), next(NULL), prev(NULL) { ; }
  ~cInjectGenotypeElement() { ; }

  cInjectGenotype* GetInjectGenotype() const { return inject_genotype; }
  cInjectGenotypeElement* GetNext() const { return next; }
  cInjectGenotypeElement* GetPrev() const { return prev; }

  void SetNext(cInjectGenotypeElement* in_next) { next = in_next; }
  void SetPrev(cInjectGenotypeElement* in_prev) { prev = in_prev; }
};


#ifdef ENABLE_UNIT_TESTS
namespace nInjectGenotypeElement {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif

#endif
