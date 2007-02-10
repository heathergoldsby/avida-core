/*
 *  cCPUTestInfo.h
 *  Avida
 *
 *  Called "cpu_test_info.hh" prior to 11/29/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1999-2003 California Institute of Technology.
 *
 */

#ifndef cCPUTestInfo_h
#define cCPUTestInfo_h

#ifndef nHardware_h
#include "nHardware.h"
#endif
#ifndef cString_h
#include "cString.h"
#endif
#ifndef tArray_h
#include "tArray.h"
#endif

class cHardwareTracer;
class cOrganism;
class cPhenotype;
class cString;

class cCPUTestInfo
{
  friend class cTestCPU;
private:
  // Inputs...
  const int generation_tests; // Maximum depth in generations to test
  bool trace_execution;       // Should we trace this CPU?
  bool trace_task_order;      // Should we keep track of ordering of tasks?
  bool use_random_inputs;     // Should we give the organism random inputs?
  cHardwareTracer* m_tracer;

  // Outputs...
  bool is_viable;         // Is this organism colony forming?
  int max_depth;          // Deepest tests went...
  int depth_found;        // Depth actually found (often same as max_depth)
  int max_cycle;          // Longest cycle found.
  int cycle_to;           // Cycle path of the last genotype.

  tArray<cOrganism*> org_array;


  cCPUTestInfo(const cCPUTestInfo&); // @not_implemented
  cCPUTestInfo& operator=(const cCPUTestInfo&); // @not_implemented

public:
  cCPUTestInfo(int max_tests=nHardware::TEST_CPU_GENERATIONS);
  ~cCPUTestInfo();

  void Clear();
 
  // Input Setup
  void TraceTaskOrder(bool _trace=true) { trace_task_order = _trace; }
  void UseRandomInputs(bool _rand=true) { use_random_inputs = _rand; }
  void SetTraceExecution(cHardwareTracer *tracer = NULL);

  // Input Accessors
  int GetGenerationTests() const { return generation_tests; }
  bool GetTraceTaskOrder() const { return trace_task_order; }
  bool GetUseRandomInputs() const { return use_random_inputs; }
  bool GetTraceExecution() const { return trace_execution; }
  cHardwareTracer *GetTracer() { return m_tracer; }


  // Output Accessors
  bool IsViable() const { return is_viable; }
  int GetMaxDepth() const { return max_depth; }
  int GetDepthFound() const { return depth_found; }
  int GetMaxCycle() const { return max_cycle; }
  int GetCycleTo() const { return cycle_to; }

  // Genotype Stats...
  inline cOrganism* GetTestOrganism(int level = 0);
  cPhenotype& GetTestPhenotype(int level = 0);
  inline cOrganism* GetColonyOrganism();

  // And just because these are so commonly used...
  double GetGenotypeFitness();
  double GetColonyFitness();
};


#ifdef ENABLE_UNIT_TESTS
namespace nCPUTestInfo {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  


inline cOrganism* cCPUTestInfo::GetTestOrganism(int level)
{
  assert(org_array[level] != NULL);
  return org_array[level];
}

inline cOrganism* cCPUTestInfo::GetColonyOrganism()
{
  const int depth_used = (depth_found == -1) ? 0 : depth_found;
  assert(org_array[depth_used] != NULL);
  return org_array[depth_used];
}

#endif
