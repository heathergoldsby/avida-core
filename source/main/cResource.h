/*
 *  cResource.h
 *  Avida
 *
 *  Called "resource.hh" prior to 12/5/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
 *
 */

#ifndef cResource_h
#define cResource_h

#ifndef cString_h
#include "cString.h"
#endif

class cResource
{
private:
  cString name;
  int id;
  double initial;
  double inflow;
  double outflow;
  int geometry;
  int inflowX1;
  int inflowX2;
  int inflowY1;
  int inflowY2;
  int outflowX1;
  int outflowX2;
  int outflowY1;
  int outflowY2;
  double xdiffuse;
  double xgravity;
  double ydiffuse;
  double ygravity;
 
  
  cResource(); // @not_implemented
  
public:
  cResource(const cString& _name, int _id);
  ~cResource() { ; }

  const cString & GetName() const { return name; }
  int GetID() const { return id; }
  double GetInitial() const { return initial; }
  double GetInflow() const { return inflow; }
  double GetOutflow() const { return outflow; }
  int GetGeometry() const { return geometry; }
  int GetInflowX1() const { return inflowX1; }
  int GetInflowX2() const { return inflowX2; }
  int GetInflowY1() const { return inflowY1; }
  int GetInflowY2() const { return inflowY2; }
  int GetOutflowX1() const { return outflowX1; }
  int GetOutflowX2() const { return outflowX2; }
  int GetOutflowY1() const { return outflowY1; }
  int GetOutflowY2() const { return outflowY2; }
  double GetXDiffuse() const { return xdiffuse; }
  double GetXGravity() const { return xgravity; }
  double GetYDiffuse() const { return ydiffuse; }
  double GetYGravity() const { return ygravity; }


  void SetInitial(double _initial) { initial = _initial; }
  void SetInflow (double _inflow ) { inflow  = _inflow; }
  void SetOutflow(double _outflow) { outflow = _outflow; }
  bool SetGeometry(cString _geometry);
  void SetInflowX1(int _inflowX1) { inflowX1 = _inflowX1; }
  void SetInflowX2(int _inflowX2) { inflowX2 = _inflowX2; }
  void SetInflowY1(int _inflowY1) { inflowY1 = _inflowY1; }
  void SetInflowY2(int _inflowY2) { inflowY2 = _inflowY2; }
  void SetOutflowX1(int _outflowX1) { outflowX1 = _outflowX1; }
  void SetOutflowX2(int _outflowX2) { outflowX2 = _outflowX2; }
  void SetOutflowY1(int _outflowY1) { outflowY1 = _outflowY1; }
  void SetOutflowY2(int _outflowY2) { outflowY2 = _outflowY2; }
  void SetXDiffuse(double _xdiffuse) { xdiffuse = _xdiffuse; }
  void SetXGravity(double _xgravity) { xgravity = _xgravity; }
  void SetYDiffuse(double _ydiffuse) { ydiffuse = _ydiffuse; }
  void SetYGravity(double _ygravity) { ygravity = _ygravity; }
};


#ifdef ENABLE_UNIT_TESTS
namespace nResource {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
