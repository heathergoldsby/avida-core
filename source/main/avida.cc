/*
 *  avida.cc
 *  Avida
 *
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
 *
 */

#include "avida.h"

#include "cString.h"
#include "defs.h"

#ifdef REVISION_SUPPORT
#include "revision.h"
#endif

#include <iostream>
#include <signal.h>
#include <stdio.h>

using namespace std;

cString getAvidaVersion()
{
  cString version("Avida ");
  version += VERSION;
#ifdef REVISION_SUPPORT
  version += " r";
  version += REVISION;
#endif
  version += " (";
  version += VERSION_TAG;
  version += ")";

#ifdef DEBUG
  version += " debug";
#endif
#if BREAKPOINTS
  version += " breakp";
#endif
#ifdef EXECUTION_ERRORS
  version += " exec_err";
#endif
#if INSTRUCTION_COSTS
  version += " inst_cost";
#endif
#if INSTRUCTION_COUNT
  version += " inst_cnt";
#endif
#if SMT_FULLY_ASSOCIATIVE
  version += " smt_fa";
#endif
#if CLASSIC_FULLY_ASSOCIATIVE
  version += " c_fa";
#endif
#if WRITE_PROTECTION
  version += " wp";
#endif
#ifdef ENABLE_UNIT_TESTS
  version += " ut";
#endif
#if USE_tMemTrack
  version += " memt";
#endif
  
  return version;
}

void printVersionBanner()
{
  // output copyright message
  cout << getAvidaVersion() << endl;
  cout << "----------------------------------------------------------------------" << endl;
  cout << "Copyright (C) 1999-2006 Michigan State University." << endl;
  cout << "Copyright (C) 1993-2005 California Institute of Technology." << endl << endl;
  
  cout << "Avida comes with ABSOLUTELY NO WARRANTY." << endl;
  cout << "This is free software, and you are welcome to redistribute it" << endl;
  cout << "under certain conditions. See file COPYING for details." << endl << endl;

  cout << "For more information, see: http://devolab.cse.msu.edu/software/avida/" << endl << endl;
}

void ExitAvida(int exit_code)
{
  signal(SIGINT, SIG_IGN);          // Ignore all future interupts.
  exit(exit_code);
}
