//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2002 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#ifndef STATS_HH
#define STATS_HH

#include <assert.h>

//#include <fstream.h>
//#include <iostream.h>
// porting to gcc 3.1 -- k
#include <fstream>
#include <iostream>

#include "../tools/tArray.hh"

#include "../tools/data_manager.hh"
#include "../tools/datafile.hh"
#include "../tools/functions.hh"
#include "../tools/stat.hh"
#include "../tools/tools.hh"

#include "../defs.hh"

#define GEOMETRY_GLOBAL 0
#define GEOMETRY_GRID   1
#define GEOMETRY_TORUS  2

class cDataFile;
class cGenotype;
class sCPUStats;
class cGenome;

class cStats {
private:
  // Time scales...
  int current_update;
  int sub_update;
  double avida_time;

  // The data manager handles printing user-formated output files.
  tDataManager<cStats> data_manager;

  // Log files are recorded every time a specified event occurs.
  cDataFileManager data_file_manager;

  std::ofstream fp_creature_log;
  std::ofstream fp_genotype_log;
  std::ofstream fp_threshold_log;
  std::ofstream fp_species_log;
  std::ofstream fp_lineage_log;


  //// By Creature Sums ////  (Cleared and resummed by population each update)
  cDoubleSum sum_merit;
  cDoubleSum sum_mem_size;
  cDoubleSum sum_creature_age;
  cDoubleSum sum_generation;

  cDoubleSum sum_neutral_metric;
  cDoubleSum sum_lineage_label;

  cDoubleSum sum_copy_mut_rate;
  cDoubleSum sum_log_copy_mut_rate;

  cDoubleSum sum_div_mut_rate;
  cDoubleSum sum_log_div_mut_rate;

  //// By Genotype Sums ////  (Cleared and resummed by population each update)

  cDoubleSum sum_gestation;
  cDoubleSum sum_fitness;
  cDoubleSum sum_repro_rate;

  // calculates a running average over the actual replication rate
  // given by the number of births per update
  cRunningAverage rave_true_replication_rate;

  cDoubleSum sum_size;
  cDoubleSum sum_copy_size;
  cDoubleSum sum_exe_size;

  cDoubleSum sum_genotype_age;

  // breed/geneological stats
  cDoubleSum sum_abundance;
  cDoubleSum sum_genotype_depth;


  //// By Threshold Sums ////  (Cleared and resummed by population each update)

  cDoubleSum sum_threshold_age;


  //// By Species Sums ////  (Cleared and resummed by population each update)

  cDoubleSum sum_species_age;


  //// Sums cleard on output only ////
  cIntSum isum_parent_dist;
  cIntSum isum_parent_size;
  cIntSum isum_child_size;
  cIntSum isum_point_mut;
  cIntSum isum_copy_mut;
  cIntSum isum_insert_mut;
  cIntSum isum_point_mut_line;
  cIntSum isum_copy_mut_line;
  cIntSum isum_delete_mut;
  cIntSum isum_divide_mut;
  cIntSum isum_divide_insert_mut;
  cIntSum isum_divide_delete_mut;
  cIntSum isum_copied_size;
  cIntSum isum_executed_size;
  cIntSum isum_copies_exec;
  cDoubleSum dsum_copy_mut_by_copies_exec;
  cDoubleSum dsum_copied_size_by_copies_exec;
  cDoubleSum dsum_copy_mut_lines_by_copied_size;
  cDoubleSum dsum_copy_mut_lines_by_copy_mut;

  // Instruction Counts (DM)
  tArray<cIntSum> sum_exe_inst_array;


  // Calculated stats
  double entropy;
  double species_entropy;
  double energy;
  double dom_fidelity;
  double ave_fidelity;

  // For tracking of advantageous mutations
  double max_viable_fitness;

  // Dominant Genotype
  cGenotype * dom_genotype;
  double dom_merit;
  double dom_gestation;
  double dom_repro_rate;
  double dom_fitness;
  int dom_size;
  double dom_copied_size;
  double dom_exe_size;
  double max_fitness;
  double max_merit;
  int dom_genotype_id;
  cString dom_name;
  int dom_births;
  int dom_breed_true;
  int dom_breed_in;
  int dom_breed_out;
  int dom_abundance;
  int dom_gene_depth;
  cString dom_sequence;
  int coal_depth;

  int num_births;
  int num_deaths;
  int num_breed_in;
  int num_breed_true;
  int num_breed_true_creatures;
  int num_creatures;
  int num_genotypes;
  int num_threshold;
  int num_species;
  int num_thresh_species;
  int num_lineages;
  int num_executed;
  int num_parasites;
  int num_no_birth_creatures;
  int num_single_thread_creatures;
  int num_multi_thread_creatures;
  int num_modified;

  int num_genotypes_last;

  int tot_organisms;
  int tot_genotypes;
  int tot_threshold;
  int tot_species;
  int tot_thresh_species;
  int tot_lineages;
  int tot_executed;
  
  tArray<int> genotype_map;

  tArray<int> task_cur_count;
  tArray<int> task_last_count;
  tArray<int> task_exe_count;

  tArray<double> reaction_count;
  tArray<double> resource_count;
  tArray<int> resource_geometry;
  tArray< tArray<double> > spatial_res_count;

  tArray<cString> task_names;
  tArray<cString> inst_names;
  tArray<cString> reaction_names;
  tArray<cString> resource_names;

public:
  cStats();
  ~cStats();

  void SetupPrintDatabase();

  void ProcessUpdate();


  // GLOBAL STATS

  inline void SetCurrentUpdate(int new_update) {
    current_update = new_update;
    sub_update = 0;
  }

  inline void IncCurrentUpdate() {
    current_update++;
    sub_update = 0;
  }

  void IncSubUpdate() { sub_update++; }

  bool OK() { return true; }  // @CAO FIX!!!!

  void FlushFP(); // Flush all the files

  // Accessors...
  int GetUpdate() const { return current_update; }
  int GetSubUpdate() const { return sub_update; }
  double GetGeneration() const { return SumGeneration().Average(); }

  cGenotype * GetDomGenotype() const { return dom_genotype; }
  double GetDomMerit() const { return dom_merit; }
  double GetDomGestation() const { return dom_gestation; }
  double GetDomReproRate() const { return dom_repro_rate; }
  double GetDomFitness() const { return dom_fitness; }
  double GetDomCopySize() const { return dom_copied_size; }
  double GetDomExeSize() const { return dom_exe_size; }

  int GetDomSize() const { return dom_size; }
  int GetDomID() const { return dom_genotype_id; }
  const cString & GetDomName() const { return dom_name; }
  int GetDomBirths() const { return dom_births; }
  int GetDomBreedTrue() const { return dom_breed_true; }
  int GetDomBreedIn() const { return dom_breed_in; }
  int GetDomBreedOut() const { return dom_breed_out; }
  int GetDomAbundance() const { return dom_abundance; }
  int GetDomGeneDepth() const { return dom_gene_depth; }
  const cString & GetDomSequence() const { return dom_sequence; }


  // Settings...
  void SetDomGenotype(cGenotype * in_gen) { dom_genotype = in_gen; }
  void SetDomMerit(double in_merit) { dom_merit = in_merit; }
  void SetDomGestation(double in_gest) { dom_gestation = in_gest; }
  void SetDomReproRate(double in_rate) { dom_repro_rate = in_rate; }
  void SetDomFitness(double in_fit) { dom_fitness = in_fit; }
  void SetDomCopiedSize(double in_size) { dom_copied_size = in_size; }
  void SetDomExeSize(double in_size) { dom_exe_size = in_size; }

  void SetDomSize(int in_size) { dom_size = in_size; }
  void SetDomID(int in_id) { dom_genotype_id = in_id; }
  void SetDomName(const cString & in_name) { dom_name = in_name; }
  void SetDomBirths(int in_births) { dom_births = in_births; }
  void SetDomBreedTrue(int in_bt) { dom_breed_true = in_bt; }
  void SetDomBreedIn(int in_bi) { dom_breed_in = in_bi; }
  void SetDomBreedOut(int in_bo) { dom_breed_out = in_bo; }
  void SetDomAbundance(int in_abund) { dom_abundance = in_abund; }
  void SetDomGeneDepth(int in_depth) { dom_gene_depth = in_depth; }
  void SetDomSequence(const cString & in_seq) { dom_sequence = in_seq; }

  void SetGenoMapElement(int i, int in_geno) { genotype_map[i] = in_geno; }
  // Generic data
  void SetCoalescentGenotypeDepth(int in_depth) {coal_depth = in_depth;}

  inline void SetNumGenotypes(int new_genotypes);
  inline void SetNumCreatures(int new_creatures);
  inline void SetNumThreshSpecies(int new_thresh_species);
  inline void SetBreedTrueCreatures(int in_num_breed_true_creatures);
  inline void SetNumParasites(int in_num_parasites);
  inline void SetNumNoBirthCreatures(int in_num_no_birth_creatures);
  inline void SetNumSingleThreadCreatures(int in_num_single_thread_creatures);
  inline void SetNumMultiThreadCreatures(int in_num_multi_thread_creatures);
  inline void SetNumModified(int in_num_modified);

  inline void SetMaxFitness(double in_max_fitness)
    { max_fitness = in_max_fitness; }
  inline void SetMaxMerit(double in_max_merit)
    { max_merit = in_max_merit; }
  inline void SetEntropy(double in_entropy)
    { entropy = in_entropy; }
  inline void SetSpeciesEntropy(double in_ent)
    { species_entropy = in_ent; }

  cDoubleSum & SumFitness()       { return sum_fitness; }
  cDoubleSum & SumGestation()     { return sum_gestation; }
  cDoubleSum & SumMerit()         { return sum_merit; }
  cDoubleSum & SumReproRate()     { return sum_repro_rate; }

  cDoubleSum & SumCreatureAge()   { return sum_creature_age; }
  cDoubleSum & SumGenotypeAge()   { return sum_genotype_age; }
  cDoubleSum & SumGeneration()    { return sum_generation; }
  cDoubleSum & SumAbundance()     { return sum_abundance; }
  cDoubleSum & SumGenotypeDepth() { return sum_genotype_depth; }
  cDoubleSum & SumThresholdAge()  { return sum_threshold_age; }
  cDoubleSum & SumSpeciesAge()    { return sum_species_age; }

  cDoubleSum & SumNeutralMetric() { return sum_neutral_metric; }
  cDoubleSum & SumLineageLabel()  { return sum_lineage_label; }
  cDoubleSum & SumCopyMutRate()   { return sum_copy_mut_rate; }
  cDoubleSum & SumLogCopyMutRate()   { return sum_log_copy_mut_rate; }
  cDoubleSum & SumDivMutRate()   { return sum_div_mut_rate; }
  cDoubleSum & SumLogDivMutRate()   { return sum_log_div_mut_rate; }

  cDoubleSum & SumSize()          { return sum_size; }
  cDoubleSum & SumCopySize()      { return sum_copy_size; }
  cDoubleSum & SumExeSize()       { return sum_exe_size; }
  cDoubleSum & SumMemSize()       { return sum_mem_size; }

  void ZeroInst();
  tArray<cIntSum> & SumExeInst() { return sum_exe_inst_array; }

  // And constant versions of the above...
  const cDoubleSum & SumFitness() const       { return sum_fitness; }
  const cDoubleSum & SumGestation() const     { return sum_gestation; }
  const cDoubleSum & SumMerit() const         { return sum_merit; }
  const cDoubleSum & SumReproRate() const     { return sum_repro_rate; }

  const cDoubleSum & SumCreatureAge() const   { return sum_creature_age; }
  const cDoubleSum & SumGenotypeAge() const   { return sum_genotype_age; }
  const cDoubleSum & SumGeneration() const    { return sum_generation; }
  const cDoubleSum & SumAbundance() const     { return sum_abundance; }
  const cDoubleSum & SumGenotypeDepth() const { return sum_genotype_depth; }
  const cDoubleSum & SumThresholdAge() const  { return sum_threshold_age; }
  const cDoubleSum & SumSpeciesAge() const    { return sum_species_age; }

  const cDoubleSum & SumNeutralMetric() const { return sum_neutral_metric; }
  const cDoubleSum & SumLineageLabel() const  { return sum_lineage_label; }
  const cDoubleSum & SumCopyMutRate() const   { return sum_copy_mut_rate; }
  const cDoubleSum & SumLogCopyMutRate() const{ return sum_log_copy_mut_rate; }
  const cDoubleSum & SumDivMutRate() const   { return sum_div_mut_rate; }
  const cDoubleSum & SumLogDivMutRate() const{ return sum_log_div_mut_rate; }

  const cDoubleSum & SumSize() const          { return sum_size; }
  const cDoubleSum & SumCopySize() const      { return sum_copy_size; }
  const cDoubleSum & SumExeSize() const       { return sum_exe_size; }
  const cDoubleSum & SumMemSize() const       { return sum_mem_size; }

  void CalcEnergy();
  void CalcFidelity();

  void RecordBirth(int cell_id, int genotype_id, bool breed_true);
  void RecordDeath(int genotype_id, int num_divides, int age);
  void AddGenotype(int id_num);
  void RemoveGenotype(int id_num, int parent_id,
			     int parent_distance, int depth, int max_abundance,
			     int parasite_abundance, int age, int length);
  void AddThreshold(int id_num, const char * name,
				  int species_num=-1);
  void RemoveThreshold(int id_num);
  void AddSpecies(int id_num);
  void RemoveSpecies(int id_num, int parent_id,
			 int max_gen_abundance, int max_abundance, int age);
  void AddLineage();
  void RemoveLineage(int id_num, int parent_id, int update_born,
		     double generation_born, int total_CPUs,
		     int total_genotypes, double fitness, 
		     double lineage_stat1, double lineage_stat2 );
				
  void IncExecuted() { num_executed++; }

  void AddCurTask(int task_num) { task_cur_count[task_num]++; }
  void AddLastTask(int task_num) { task_last_count[task_num]++; }
  void IncTaskExeCount(int task_num) { task_exe_count[task_num]++; }
  void ZeroTasks();

  void SetReactions(const tArray<double> &_in) { reaction_count = _in; }
  void SetResources(const tArray<double> &_in) { resource_count = _in; }
  void SetResourcesGeometry(const tArray<int> &_in) { resource_geometry = _in;}
  void SetSpatialRes(const tArray< tArray<double> > &_in) { 
    spatial_res_count = _in;
  }

  void SetTaskName(int id, const cString & name) { task_names[id] = name; }
  void SetInstName(int id, const cString & name) {
    assert(id < inst_names.GetSize());
    inst_names[id] = name;
  }
  void SetReactionName(int id, const cString & name) { reaction_names[id] = name; }
  void SetResourceName(int id, const cString & name) { resource_names[id] = name; }

  // Information retrieval section...

  int GetNumBirths() const          { return num_births; }
  int GetNumDeaths() const          { return num_deaths; }
  int GetBreedIn() const            { return num_breed_in; }
  int GetBreedTrue() const          { return num_breed_true; }
  int GetBreedTrueCreatures() const { return num_breed_true_creatures; }
  int GetNumCreatures() const       { return num_creatures; }
  int GetNumGenotypes() const       { return num_genotypes; }
  int GetNumThreshold() const       { return num_threshold; }
  int GetNumSpecies() const         { return num_species; }
  int GetNumThreshSpecies() const   { return num_thresh_species; }
  int GetNumLineages() const        { return num_lineages; }
  int GetNumParasites() const       { return num_parasites; }
  int GetNumNoBirthCreatures() const{ return num_no_birth_creatures; }
  int GetNumSingleThreadCreatures() const { return num_single_thread_creatures; }
  int GetNumMultiThreadCreatures() const { return num_multi_thread_creatures; }
  int GetNumModified() const { return num_modified;}

  int GetTotCreatures() const       { return tot_organisms; }
  int GetTotGenotypes() const       { return tot_genotypes; }
  int GetTotThreshold() const       { return tot_threshold; }
  int GetTotSpecies() const         { return tot_species; }
  int GetTotThreshSpecies() const   { return tot_thresh_species; }
  int GetTotLineages() const        { return tot_lineages; }

  int GetTaskCurCount(int task_num) const { return task_cur_count[task_num]; }
  int GetTaskLastCount(int task_num) const {return task_last_count[task_num];}
  int GetTaskExeCount(int task_num) const { return task_exe_count[task_num]; }

  const tArray<double> & GetReactions() const { return reaction_count; }
  const tArray<double> & GetResources() const { return resource_count; }

  double GetAveReproRate() const  { return sum_repro_rate.Average(); }

  double GetAveMerit() const      { return sum_merit.Average(); }
  double GetAveCreatureAge() const{ return sum_creature_age.Average(); }
  double GetAveMemSize() const    { return sum_mem_size.Average(); }

  double GetAveNeutralMetric() const { return sum_neutral_metric.Average(); }
  double GetAveLineageLabel() const  { return sum_lineage_label.Average(); }
  double GetAveCopyMutRate() const   { return sum_copy_mut_rate.Average(); }
  double GetAveLogCopyMutRate() const{ return sum_log_copy_mut_rate.Average();}
  double GetAveDivMutRate() const   { return sum_div_mut_rate.Average(); }
  double GetAveLogDivMutRate() const{ return sum_log_div_mut_rate.Average();}

  double GetAveGestation() const { return sum_gestation.Average(); }
  double GetAveFitness() const   { return sum_fitness.Average(); }

  double GetAveGenotypeAge() const { return sum_genotype_age.Average();}

  double GetAveSize() const       { return sum_size.Average(); }
  double GetAveCopySize() const   { return sum_copy_size.Average(); }
  double GetAveExeSize() const    { return sum_exe_size.Average(); }

  double GetEntropy() const        { return entropy; }
  double GetSpeciesEntropy() const { return species_entropy; }
  double GetEnergy() const         { return energy; }
  double GetEvenness() const       { return entropy / Log(num_genotypes); }
  int GetCoalescentDepth() const   { return coal_depth; }

  double GetAveThresholdAge() const { return sum_threshold_age.Average(); }
  double GetAveSpeciesAge() const { return sum_species_age.Average(); }

  double GetMaxFitness() const { return max_fitness; }
  double GetMaxMerit() const { return max_merit; }

  // this value gets recorded when a creature with the particular
  // fitness value gets born. It will never change to a smaller value,
  // i.e., when the maximum fitness in the population drops, this value will
  // still stay up.
  double GetMaxViableFitness() const { return max_viable_fitness; }

  // Access to data_file_manager (so cPopulation can output files)
  std::ofstream & GetDataFileOFStream(const cString & fname){
    return data_file_manager.GetOFStream(fname); }
  cDataFile & GetDataFile(const cString & fname){
    return data_file_manager.Get(fname); }


  // User-defined datafile...
  void PrintDataFile(const cString & filename, const cString & format,
		     char sep=' ');

  // Public calls to output data files (for events)
  void PrintAverageData(const cString & filename);
  void PrintErrorData(const cString & filename);
  void PrintVarianceData(const cString & filename);
  void PrintDominantData(const cString & filename);
  void PrintStatsData(const cString & filename);
  void PrintCountData(const cString & filename);
  void PrintTotalsData(const cString & filename);
  void PrintTasksData(const cString & filename);
  void PrintTasksExeData(const cString & filename);
  void PrintReactionData(const cString & filename);
  void PrintResourceData(const cString & filename);
  void PrintSpatialResData(const cString & filename, int i);
  void PrintTimeData(const cString & filename);
  void PrintMutationData(const cString & filename);
  void PrintDivideMutData(const cString & filename);
  void PrintMutationRateData(const cString & filename);
  void PrintInstructionData(const cString & filename);
  void PrintGenotypeMap(const cString & filename);
};


inline void cStats::SetNumGenotypes(int new_genotypes)
{
  num_genotypes_last = num_genotypes;
  num_genotypes = new_genotypes;
}

inline void cStats::SetNumCreatures(int new_creatures)
{
  num_creatures = new_creatures;
}

inline void cStats::SetNumThreshSpecies(int new_thresh_species)
{
  num_thresh_species = new_thresh_species;
}

inline void cStats::SetBreedTrueCreatures(int in_num_breed_true_creatures)
{
  num_breed_true_creatures = in_num_breed_true_creatures;
}

inline void cStats::SetNumParasites(int in_num_parasites)
{
  num_parasites = in_num_parasites;
}

inline void cStats::SetNumNoBirthCreatures(int in_num_no_birth_creatures)
{
  num_no_birth_creatures = in_num_no_birth_creatures;
}

inline void cStats::SetNumSingleThreadCreatures(int in_num_single_thread_creatures)
{
  num_single_thread_creatures = in_num_single_thread_creatures;
}

inline void cStats::SetNumMultiThreadCreatures(int in_num_multi_thread_creatures)
{
  num_multi_thread_creatures = in_num_multi_thread_creatures;
}

inline void cStats::SetNumModified(int in_num_modified)
{
  num_modified = in_num_modified;
}


#endif
