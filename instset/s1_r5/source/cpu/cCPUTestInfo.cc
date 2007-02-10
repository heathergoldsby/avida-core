/*
 *  cCPUTestInfo.h
 *  Avida
 *
 *  Called "cpu_test_info.hh" prior to 11/29/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1999-2003 California Institute of Technology.
 *
 */

#include "cCPUTestInfo.h"

#include "cHardwareStatusPrinter.h"
#include "cOrganism.h"
#include "cPhenotype.h"

#include <assert.h>

cCPUTestInfo::cCPUTestInfo(int max_tests)
  : generation_tests(max_tests)  // These vars not reset on Clear()
  , trace_execution(false)
  , trace_task_order(false)
  , use_random_inputs(false)
  , m_tracer(NULL)
  , org_array(max_tests)
{
  org_array.SetAll(NULL);
  Clear();
}


cCPUTestInfo::~cCPUTestInfo()
{
  for (int i = 0; i < generation_tests; i++) {
    if (org_array[i] != NULL) delete org_array[i];
  }
}


void cCPUTestInfo::Clear()
{
  is_viable = false;
  max_depth = -1;
  depth_found = -1;
  max_cycle = 0;
  cycle_to = -1;

  for (int i = 0; i < generation_tests; i++) {
    if (org_array[i] == NULL) break;
    delete org_array[i];
    org_array[i] = NULL;
  }
}
 

void cCPUTestInfo::SetTraceExecution(cHardwareTracer *tracer)
{
  trace_execution = (tracer)?(true):(false);
  m_tracer = tracer;
}


double cCPUTestInfo::GetGenotypeFitness()
{
  if (org_array[0] != NULL) return org_array[0]->GetPhenotype().GetFitness();
  return 0.0;
}


double cCPUTestInfo::GetColonyFitness()
{
  if (IsViable()) return GetColonyOrganism()->GetPhenotype().GetFitness();
  return 0.0;
}

cPhenotype& cCPUTestInfo::GetTestPhenotype(int level)
{
  assert(org_array[level] != NULL);
  return org_array[level]->GetPhenotype();
}
