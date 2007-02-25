/*
 *  cWorld.cc
 *  Avida
 *
 *  Created by David on 10/18/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *
 *
 */

#include "cWorld.h"

#include "avida.h"
#include "cAnalyze.h"
#include "cClassificationManager.h"
#include "cEnvironment.h"
#include "cEventList.h"
#include "cHardwareManager.h"
#include "cPopulation.h"
#include "cStats.h"
#include "cTestCPU.h"
#include "cTools.h"
#include "cFallbackWorldDriver.h"


cWorld::~cWorld()
{
  // m_actlib is not owned by cWorld, DO NOT DELETE
  delete m_analyze;
  delete m_pop;
  delete m_class_mgr;
  delete m_env;
  delete m_event_list;
  delete m_hw_mgr;
  delete m_stats;

  m_data_mgr->FlushAll();
  delete m_data_mgr;

  delete m_conf;

  // cleanup driver object, if needed
  if (m_own_driver) delete m_driver;
}

void cWorld::Setup()
{
  m_own_driver = true;
  m_driver = new cFallbackWorldDriver();
  
  // Setup Random Number Generator
  const int rand_seed = m_conf->RANDOM_SEED.Get();
  cout << "Random Seed: " << rand_seed;
  m_rng.ResetSeed(rand_seed);
  if (rand_seed != m_rng.GetSeed()) cout << " -> " << m_rng.GetSeed();
  cout << endl;
  
  m_actlib = cDriverManager::GetActionLibrary();
  
  m_data_mgr = new cDataFileManager(m_conf->DATA_DIR.Get(), (m_conf->VERBOSITY.Get() > VERBOSE_ON));
  if (m_conf->VERBOSITY.Get() > VERBOSE_NORMAL)
    cout << "Data Directory: " << m_data_mgr->GetTargetDir() << endl;
  
  m_class_mgr = new cClassificationManager(this);
  m_env = new cEnvironment(this);
  
  // Initialize the default environment...
  if (m_env->Load(m_conf->ENVIRONMENT_FILE.Get()) == false) {
    cerr << "Unable to load environment... aborting!" << endl;
    ExitAvida(-1);
  }

  m_hw_mgr = new cHardwareManager(this);
  m_stats = new cStats(this);
  m_pop = new cPopulation(this);

  // Setup Event List
  m_event_list = new cEventList(this);
  m_event_list->LoadEventFile(m_conf->EVENT_FILE.Get());
    
  const bool revert_fatal = m_conf->REVERT_FATAL.Get() > 0.0;
  const bool revert_neg = m_conf->REVERT_DETRIMENTAL.Get() > 0.0;
  const bool revert_neut = m_conf->REVERT_NEUTRAL.Get() > 0.0;
  const bool revert_pos = m_conf->REVERT_BENEFICIAL.Get() > 0.0;
  const bool fail_implicit = m_conf->FAIL_IMPLICIT.Get() > 0;
  m_test_on_div = (revert_fatal || revert_neg || revert_neut || revert_pos || fail_implicit);

  const bool sterilize_fatal = m_conf->STERILIZE_FATAL.Get() > 0.0;
  const bool sterilize_neg = m_conf->STERILIZE_DETRIMENTAL.Get() > 0.0;
  const bool sterilize_neut = m_conf->STERILIZE_NEUTRAL.Get() > 0.0;
  const bool sterilize_pos = m_conf->STERILIZE_BENEFICIAL.Get() > 0.0;
  m_test_sterilize = (sterilize_fatal || sterilize_neg || sterilize_neut || sterilize_pos);
}

cAnalyze& cWorld::GetAnalyze()
{
  if (m_analyze == NULL) m_analyze = new cAnalyze(this);
  return *m_analyze;
}

void cWorld::GetEvents(cAvidaContext& ctx)
{  
  if (m_pop->GetSyncEvents() == true) {
    m_event_list->Sync();
    m_pop->SetSyncEvents(false);
  }
  m_event_list->Process(ctx);
}

int cWorld::GetNumInstructions()
{
  return m_hw_mgr->GetInstSet().GetSize();
}

int cWorld::GetNumTasks()
{
  return m_env->GetTaskLib().GetSize(); 
}

int cWorld::GetNumReactions()
{
  return m_env->GetReactionLib().GetSize();
}

int cWorld::GetNumResources()
{
  return m_env->GetResourceLib().GetSize();
}

void cWorld::SetDriver(cWorldDriver* driver, bool take_ownership)
{
  // cleanup current driver, if needed
  if (m_own_driver) delete m_driver;
  
  // store new driver information
  m_driver = driver;
  m_own_driver = take_ownership;
}
