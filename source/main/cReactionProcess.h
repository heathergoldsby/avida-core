//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2004 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#ifndef REACTION_PROCESS_HH
#define REACTION_PROCESS_HH
#ifndef STRING_HH
#include "cString.h"
#endif

#include <iostream>

using namespace std;

class cResource;
class cReactionProcess {
private:
  cResource * resource;  // Resource consumed.
  double value;          // Efficiency.
  int type;              // Method of applying bonuses.
  double max_number;     // Max quantity of resource usable.
  double min_number;     // Minimum quantity of resource needed (otherwise 0)
  double max_fraction;   // Max fraction of avaiable resources useable.
  cResource * product;   // Output resource.
  double conversion;     // Conversion factor.
  bool lethal;			 // Lethality of reaction
  int inst_id;           // Instruction to be triggered if reaction successful.
  cString match_string;	 // Bit string to match if this is a match string reaction

  // Resource detection
  cResource * detect;    // Resource Measured
  double detection_threshold; // Min quantity of resource to register present
  double detection_error; // Var of Detection Event (as % of resource present)
public:
  cReactionProcess();
  ~cReactionProcess();

  cResource * GetResource() const { return resource; }
  double GetValue() const { return value; }
  int GetType() const { return type; }
  double GetMaxNumber() const { return max_number; }
  double GetMinNumber() const { return min_number; }
  double GetMaxFraction() const { return max_fraction; }
  cResource * GetProduct() const { return product; }
  double GetConversion() const { return conversion; }
  int GetInstID() const { return inst_id; }
  bool GetLethal() const { return lethal; }
  cResource * GetDetect() const { return detect; }
  double GetDetectionThreshold() const { return detection_threshold; }
  double GetDetectionError() const { return detection_error; }
  cString GetMatchString() const { return match_string; }

  void SetResource(cResource * _in) { resource = _in; }
  void SetValue(double _in) { value = _in; }
  void SetType(int _in) { type = _in; }
  void SetMaxNumber(double _in) { max_number = _in; }
  void SetMinNumber(double _in) { min_number = _in; }
  void SetMaxFraction(double _in) { max_fraction = _in; }
  void SetProduct(cResource * _in) { product = _in; }
  void SetConversion(double _in) { conversion = _in; }
  void SetInstID(int _in) { inst_id = _in; }
  void SetLethal(int _in) { lethal = _in; }
  void SetDetect(cResource * _in) { detect = _in; }
  void SetDetectionThreshold(double _in) { detection_threshold = _in; }
  void SetDetectionError(double _in) { detection_error = _in; }
  void SetMatchString(cString _in) { match_string = _in; }
};

#endif
