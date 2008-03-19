//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2003 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "hardware_method.hh"
#include "hardware_base.hh"

#include "../main/inst_set.hh"


using namespace std;



using namespace std;


int cHardwareBase::instance_count(0);

cHardwareBase::cHardwareBase(cOrganism * in_organism, cInstSet * in_inst_set)
  : organism(in_organism)
  , inst_set(in_inst_set)
  , viewer_lock(-1)
{
  assert(inst_set->OK());
  assert(organism != NULL);

  instance_count++;
}

cHardwareBase::~cHardwareBase()
{
  instance_count--;
}

void cHardwareBase::Recycle(cOrganism * new_organism, cInstSet * in_inst_set)
{
  assert(inst_set->OK());
  assert(new_organism != NULL);

  organism    = new_organism;
  inst_set    = in_inst_set;
  viewer_lock = -1;
}

bool cHardwareBase::Inst_Nop()          // Do Nothing.
{
  return true;
}


int cHardwareBase::GetNumInst()
{
  assert(inst_set != NULL);
  return inst_set->GetSize();
}


cInstruction cHardwareBase::GetRandomInst()
{
  assert(inst_set != NULL);
  return inst_set->GetRandomInst();
}

