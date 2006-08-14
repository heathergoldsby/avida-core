//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2003 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#ifndef POPULATION_HH
#define POPULATION_HH

#include <fstream>

#ifndef BIRTH_CHAMBER_HH
#include "cBirthChamber.h"
#endif
#ifndef POPULATION_INTERFACE_HH
#include "cPopulationInterface.h"
#endif
#ifndef RESOURCE_COUNT_HH
#include "cResourceCount.h"
#endif
#ifndef STATS_HH
#include "cStats.h"
#endif
#ifndef STRING_HH
#include "cString.h"
#endif
#ifndef TARRAY_HH
#include "tArray.h"
#endif
#ifndef TLIST_HH
#include "tList.h"
#endif

class cSchedule;
template <class T> class tArray; // aggregate

class cBirthChamber; // aggregate
class cChangeList;
class cEnvironment;
class cGenebank;
class cGenome;
class cGenotype;
class cInjectGenebank;
class cLineage;
class cLineageControl;
template <class T> class tList; // aggregate
class cOrganism;
class cPopulationInterface; // aggregate
class cPopulationCell;
class cResourceCount; // aggregate
class cStats; // aggregate
class cString; // aggregate

class cPopulation {
private:
  cPopulation(const cPopulation &); // not implemented
private:
  // Components...
  cSchedule * schedule;                // Handles allocation of CPU cycles
  tArray<cPopulationCell> cell_array;  // Local cells composing the population
  cResourceCount resource_count;       // Global resources available
  cBirthChamber birth_chamber;         // Global birth chamber.

  // Data Tracking...
  cStats stats;                      // Main statistics object...
  cGenebank * genebank;                // Tracks genotypes
  cInjectGenebank * inject_genebank;   // Tracks all injected code
  cLineageControl * lineage_control;   // Tracks Linages
  tList<cPopulationCell> reaper_queue; // Death order in some mass-action runs

  // Default organism setups...
  cEnvironment & environment;          // Physics & Chemestry description
  cPopulationInterface default_interface;  // Organism interface to population

  // Other data...
  int world_x;                         // Structured population width.
  int world_y;                         // Structured population
  int num_organisms;                   // Cell count with living organisms
  int num_demes;                       // Number of sub-groups of organisms
  int deme_size;                       // Number of organims in a deme.
  tArray<int> deme_birth_count;        // Track number of births in each deme.
 
  // Outside interactions...
  bool sync_events;   // Do we need to sync up the event list with population?

  ///////////////// Private Methods ////////////////////
  void BuildTimeSlicer(cChangeList * change_list); // Build the schedule object

  // Methods to place offspring in the population.
  cPopulationCell & PositionChild(cPopulationCell & parent_cell,
				  bool parent_ok=true);
  void PositionAge(cPopulationCell & parent_cell,
		   tList<cPopulationCell> & found_list, bool parent_ok);
  void PositionMerit(cPopulationCell & parent_cell,
		     tList<cPopulationCell> & found_list, bool parent_ok);
  void FindEmptyCell(tList<cPopulationCell> & cell_list,
		     tList<cPopulationCell> & found_list);

  // Update statistics collecting...
  void UpdateOrganismStats();
  void UpdateGenotypeStats();
  void UpdateSpeciesStats();
  void UpdateDominantStats();
  void UpdateDominantParaStats();

  /**
   * Attention: InjectGenotype does *not* add the genotype to the genebank.
   * It assumes thats where you got the genotype from.
   **/
  void InjectGenotype(int cell_id, cGenotype * genotype);
  void InjectGenome(int cell_id, const cGenome & genome, int lineage_label);
  void InjectClone(int cell_id, cOrganism & orig_org);

  void LineageSetupOrganism(cOrganism * organism, cLineage * lineage,
			    int lin_label, cGenotype * parent_genotype=NULL);

  // Must be called to activate *any* organism in the population.
  void ActivateOrganism(cOrganism * in_organism, cPopulationCell &target_cell);

public:
  cPopulation(const cPopulationInterface & in_interface,
	      cEnvironment & in_environment,
	      cChangeList * change_list = 0);
  ~cPopulation();

  // Extra Setup...
  bool SetupDemes();

  // Activate the offspring of an organism in the population
  bool ActivateOffspring(cGenome & child_genome, cOrganism & parent_organism);

  bool ActivateInject(cOrganism & parent, const cGenome & injected_code);
  bool ActivateInject(const int cell_id, const cGenome & injected_code);

  // Inject an organism from the outside world.
  void Inject(
    const cGenome & genome,
    int cell_id=-1,
    double merit=-1,
	  int lineage_label=0,
    double neutral_metric=0,
    int mem_space=0,
    int gestation_time=-1,
    double life_fitness=-1
  );

  // Deactivate an organism in the population (required for deactivations)
  void KillOrganism(cPopulationCell & in_cell);
  void Kaboom(cPopulationCell & in_cell);

  // Deme-related methods
  void CompeteDemes(int competition_type);
  void ResetDemes();
  void CopyDeme(int deme1_id, int deme2_id);
  void PrintDemeStats();

  // Process a single organism one instruction...
  int ScheduleOrganism();          // Determine next organism to be processed.
  void ProcessStep(double step_size);
  void ProcessStep(double step_size, int cell_id);

  // Calculate the statistics from the most recent update.
  void CalcUpdateStats();

  // Clear all but a subset of cells...
  void SerialTransfer( int transfer_size, bool ignore_deads );

  // Saving and loading...
  bool SaveClone(std::ofstream & fp);
  bool LoadClone(std::ifstream & fp);
  bool LoadDumpFile(cString filename, int update);
  bool SavePopulation(std::ofstream & fp);
  bool LoadPopulation(std::ifstream & fp);
  bool DumpMemorySummary(std::ofstream & fp);

  bool OK();

  int GetSize() { return cell_array.GetSize(); }
  int GetWorldX() { return world_x; }
  int GetWorldY() { return world_y; }
  int GetUpdate() { return stats.GetUpdate(); }
  double GetGeneration() { return stats.SumGeneration().Average(); }

  cPopulationCell & GetCell(int in_num);
  const tArray<double> & GetResources() const
    { return resource_count.GetResources(); }
  const tArray<double> & GetCellResources(int cell_id) const
    { return resource_count.GetCellResources(cell_id); }
  cBirthChamber & GetBirthChamber(int id) { (void) id; return birth_chamber; }

  void UpdateResources(const tArray<double> & res_change);
  void UpdateResource(int id, double change);
  void UpdateCellResources(const tArray<double> & res_change,
                           const int cell_id);
  void SetResource(int id, double new_level);
  double GetResource(int id) const {
    return resource_count.Get(id); }

  cStats & GetStats() { return stats; }
  cGenebank & GetGenebank() { return *genebank; }
  cInjectGenebank & GetInjectGenebank() { return *inject_genebank; }
  cLineageControl * GetLineageControl() { return lineage_control; }
  cEnvironment & GetEnvironment() { return environment; }
  int GetNumOrganisms() { return num_organisms; }

  bool GetSyncEvents() { return sync_events; }
  void SetSyncEvents(bool _in) { sync_events = _in; }
  void ParasiteDebug();
  void PrintPhenotypeData(const cString & filename);
  void PrintPhenotypeStatus(const cString & filename);

  bool UpdateMerit(int cell_id, double new_merit);

  void SetChangeList(cChangeList *change_list);
  cChangeList *GetChangeList();
};

#endif
