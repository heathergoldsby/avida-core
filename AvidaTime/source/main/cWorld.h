/*
 *  cWorld.h
 *  Avida
 *
 *  Created by David on 10/18/05.
 *  Copyright 1999-2010 Michigan State University. All rights reserved.
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef cWorld_h
#define cWorld_h

#ifndef cAvidaConfig_h
#include "cAvidaConfig.h"
#endif
#ifndef cAvidaContext_h
#include "cAvidaContext.h"
#endif
#ifndef cDataFileManager_h
#include "cDataFileManager.h"
#endif
#ifndef cRandom_h
#include "cRandom.h"
#endif

#if USE_tMemTrack
# ifndef tMemTrack_h
#  include "tMemTrack.h"
# endif
#endif


#include <cassert>

class cAnalyze;
class cAnalyzeGenotype;
class cAvidaDriver;
class cClassificationManager;
class cEnvironment;
class cEventList;
class cHardwareManager;
class cOrganism;
class cPopulation;
class cMerit;
class cPopulationCell;
class cStats;
class cTestCPU;
class cWorldDriver;
template<class T> class tDataEntry;
template<class T> class tDictionary;

class cWorld
{
#if USE_tMemTrack
  tMemTrack<cWorld> mt;
#endif
protected:
  cAnalyze* m_analyze;
  cAvidaConfig* m_conf;
  cAvidaContext m_ctx;
  cClassificationManager* m_class_mgr;
  cDataFileManager* m_data_mgr;
  cEnvironment* m_env;
  cEventList* m_event_list;
  cHardwareManager* m_hw_mgr;
  cPopulation* m_pop;
  cStats* m_stats;
  cWorldDriver* m_driver;

  cRandom m_rng;
  
  bool m_test_on_div;     // flag derived from a collection of configuration settings
  bool m_test_sterilize;  // flag derived from a collection of configuration settings
  
  bool m_own_driver;      // specifies whether this world object should manage its driver object

  // Internal Methods
  void Setup();
  
  
  cWorld(const cWorld&); // @not_implemented
  cWorld& operator=(const cWorld&); // @not_implemented
  
public:
  cWorld(cAvidaConfig* cfg) : m_analyze(NULL), m_conf(cfg), m_ctx(this, m_rng) { Setup(); }
  virtual ~cWorld();
  
  void SetDriver(cWorldDriver* driver, bool take_ownership = false);
  
  // General Object Accessors
  cAnalyze& GetAnalyze();
  cAvidaConfig& GetConfig() { return *m_conf; }
  cAvidaContext& GetDefaultContext() { return m_ctx; }
  cClassificationManager& GetClassificationManager() { return *m_class_mgr; }
  cDataFileManager& GetDataFileManager() { return *m_data_mgr; }
  cEnvironment& GetEnvironment() { return *m_env; }
  cHardwareManager& GetHardwareManager() { return *m_hw_mgr; }
  cPopulation& GetPopulation() { return *m_pop; }
  cRandom& GetRandom() { return m_rng; }
  cStats& GetStats() { return *m_stats; }
  cWorldDriver& GetDriver() { return *m_driver; }
  
  // Access to Data File Manager
  std::ofstream& GetDataFileOFStream(const cString& fname) { return m_data_mgr->GetOFStream(fname); }
  cDataFile& GetDataFile(const cString& fname) { return m_data_mgr->Get(fname); }  

  // Config Dependent Modes
  bool GetTestOnDivide() const { return m_test_on_div; }
  bool GetTestSterilize() const { return m_test_sterilize; }
  
  // Convenience Accessors
  int GetNumInstructions();
  int GetNumResources();
  int GetNumResourceSpecs();
  inline int GetVerbosity() { return m_conf->VERBOSITY.Get(); }
  inline void SetVerbosity(int v) { m_conf->VERBOSITY.Set(v); }

  // @DMB - Inherited from cAvidaDriver heritage
  void GetEvents(cAvidaContext& ctx);
	
	cEventList* GetEventsList() { return m_event_list; }

	//! Migrate this organism to a different world (does nothing here; see cMultiProcessWorld).
	virtual void MigrateOrganism(cOrganism* org, const cPopulationCell& cell, const cMerit& merit, int lineage) { }

	//! Returns true if the given cell is on the boundary of the world, false otherwise.
	virtual bool IsWorldBoundary(const cPopulationCell& cell) { return false; }
	
	//! Process post-update events.
	virtual void ProcessPostUpdate(cAvidaContext& ctx) { }
	
	//! Returns true if this world allows early exits, e.g., when the population reaches 0.
	virtual bool AllowsEarlyExit() const { return true; }
	
	//! Calculate the size (in virtual CPU cycles) of the current update.
	virtual int CalculateUpdateSize();
};

#endif
