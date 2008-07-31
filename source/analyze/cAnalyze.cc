/*
 *  cAnalyze.cc
 *  Avida
 *
 *  Called "analyze.cc" prior to 12/1/05.
 *  Copyright 1999-2008 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
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

#include "cAnalyze.h"

#include <fstream>
#include <sstream>
#include <string>
#include <queue>
#include <stack>

#include "cActionLibrary.h"
#include "cAnalyzeCommand.h"
#include "cAnalyzeCommandAction.h"
#include "cAnalyzeCommandDef.h"
#include "cAnalyzeCommandDefBase.h"
#include "cAnalyzeFlowCommand.h"
#include "cAnalyzeFlowCommandDef.h"
#include "cAnalyzeFunction.h"
#include "cAnalyzeGenotype.h"
#include "cAnalyzeTreeStats_CumulativeStemminess.h"
#include "cAnalyzeTreeStats_Gamma.h"
#include "tAnalyzeJob.h"
#include "cAvidaContext.h"
#include "cDataFile.h"
#include "cEnvironment.h"
#include "cFitnessMatrix.h"
#include "cGenomeUtil.h"
#include "cHardwareBase.h"
#include "cHardwareManager.h"
#include "cHardwareStatusPrinter.h"
#include "cHelpManager.h"
#include "cInitFile.h"
#include "cInstSet.h"
#include "cLandscape.h"
#include "cPhenotype.h"
#include "cPhenPlastGenotype.h"
#include "cPlasticPhenotype.h"
#include "cProbSchedule.h"
#include "cReaction.h"
#include "cReactionProcess.h"
#include "cSchedule.h"
#include "cSpecies.h"
#include "cStringIterator.h"
#include "tArgDataEntry.h"
#include "tDataEntry.h"
#include "tDataEntryCommand.h"
#include "tMatrix.h"
#include "cTestCPU.h"
#include "cCPUTestInfo.h"
#include "cResource.h"
#include "tHashTable.h"
#include "cWorld.h"
#include "cWorldDriver.h"

#include "defs.h"

#include <cerrno>
extern "C" {
#include <sys/stat.h>
}

using namespace std;

cAnalyze::cAnalyze(cWorld* world)
: cur_batch(0)
/*
 FIXME : refactor "temporary_next_id". @kgn
 - Added as a quick way to provide unique serial ids, per organism, in COMPETE
 command. @kgn
 */
, temporary_next_id(0)
, temporary_next_update(0)
, variables(26)
, local_variables(26)
, arg_variables(26)
, exit_on_error(true)
, m_world(world)
, inst_set(world->GetHardwareManager().GetInstSet())
, m_ctx(world->GetDefaultContext())
, m_jobqueue(world)
, interactive_depth(0)
{
  random.ResetSeed(m_world->GetConfig().RANDOM_SEED.Get());
  if (m_world->GetDriver().IsInteractive()) exit_on_error = false;
  
  for (int i = 0; i < MAX_BATCHES; i++) {
    batch[i].Name().Set("Batch%d", i);
  }
  
  resources.clear();
}



cAnalyze::~cAnalyze()
{
  while (genotype_data_list.GetSize()) delete genotype_data_list.Pop();
  while (command_list.GetSize()) delete command_list.Pop();
  while (function_list.GetSize()) delete function_list.Pop();
}


void cAnalyze::RunFile(cString filename)
{
  bool saved_analyze = m_ctx.GetAnalyzeMode();
  m_ctx.SetAnalyzeMode();
  
  cInitFile analyze_file(filename);
  if (!analyze_file.WasOpened()) {
    tConstListIterator<cString> err_it(analyze_file.GetErrors());
    const cString* errstr = NULL;
    while ((errstr = err_it.Next())) cerr << "Error: " << *errstr << endl;
    cerr << "Warning: Cannot load file: \"" << filename << "\"." << endl
    << "...creating it..." << endl;
    ofstream fp(filename);
    fp << "################################################################################################" << endl
      << "# This file is used to setup avida when it is in analysis-only mode, which can be triggered by"   << endl
      << "# running \"avida -a\"."                                                                          << endl
      << "# "                                                                                               << endl
      << "# Please see the documentation in doc/analyze_mode.html for information on how to use analyze"    << endl
      << "# mode, or the file doc/analyze_samples.html for guidelines on writing programs."                 << endl
      << "################################################################################################" << endl
      << endl; 
    fp.close();
    //if (exit_on_error) exit(1);
  }
  else {
    LoadCommandList(analyze_file, command_list);
    ProcessCommands(command_list);
  }
  
  if (!saved_analyze) m_ctx.ClearAnalyzeMode();
}

//////////////// Loading methods...

void cAnalyze::LoadOrganism(cString cur_string)
{
  // LOAD_ORGANISM command...
  
  cString filename = cur_string.PopWord();
  
  // Output information about loading file.
  cout << "Loading: " << filename << '\n';
  
  // Setup the genome...
  cGenome genome( cGenomeUtil::LoadGenome(filename, inst_set) );
  
  // Construct the new genotype..
  cAnalyzeGenotype * genotype = new cAnalyzeGenotype(m_world, genome, inst_set);
  
  // Determine the organism's original name -- strip off directory...
  while (filename.Find('/') != -1) filename.Pop('/');
  while (filename.Find('\\') != -1) filename.Pop('\\');
  filename.Replace(".gen", "");  // Remove the .gen from the filename.
  genotype->SetName(filename);
  
  // And save it in the current batch.
  batch[cur_batch].List().PushRear(genotype);
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::LoadBasicDump(cString cur_string)
{
  // LOAD_BASE_DUMP
  
  cString filename = cur_string.PopWord();
  
  cout << "Loading: " << filename << '\n';
  
  cInitFile input_file(filename);
  if (!input_file.WasOpened()) {
    tConstListIterator<cString> err_it(input_file.GetErrors());
    const cString* errstr = NULL;
    while ((errstr = err_it.Next())) cerr << "Error: " << *errstr << endl;
    cerr << "Error: Cannot load file: \"" << filename << "\"." << endl;
    if (exit_on_error) exit(1);
  }
  
  // Setup the genome...
  
  for (int line_id = 0; line_id < input_file.GetNumLines(); line_id++) {
    cString cur_line = input_file.GetLine(line_id);
    
    // Setup the genotype for this line...
    cAnalyzeGenotype * genotype = new cAnalyzeGenotype(m_world, cur_line.PopWord(), inst_set);
    int num_cpus = cur_line.PopWord().AsInt();
    int id_num = cur_line.PopWord().AsInt();
    cString name = cStringUtil::Stringf("org-%d", id_num);
    
    genotype->SetNumCPUs(num_cpus);
    genotype->SetID(id_num);
    genotype->SetName(name);
    
    // Add this genotype to the proper batch.
    batch[cur_batch].List().PushRear(genotype);
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::LoadDetailDump(cString cur_string)
{
  cerr << "Warning: Use of LOAD_DETAIL_DUMP is deprecated.  Use \"LOAD\" instead." << endl;
  // LOAD_DETAIL_DUMP
  
  cString filename = cur_string.PopWord();
  
  cout << "Loading: " << filename << '\n';
  
  cInitFile input_file(filename);
  if (!input_file.WasOpened()) {
    tConstListIterator<cString> err_it(input_file.GetErrors());
    const cString* errstr = NULL;
    while ((errstr = err_it.Next())) cerr << "Error: " << *errstr << endl;
    cerr << "Error: Cannot load file: \"" << filename << "\"." << endl;
    if (exit_on_error) exit(1);
  }
  
  // Setup the genome...
  
  for (int line_id = 0; line_id < input_file.GetNumLines(); line_id++) {
    cString cur_line = input_file.GetLine(line_id);
    
    // Setup the genotype for this line...
    
    int id_num      = cur_line.PopWord().AsInt();
    int parent_id   = cur_line.PopWord().AsInt();
    int parent_dist = cur_line.PopWord().AsInt();
    int num_cpus    = cur_line.PopWord().AsInt();
    int total_cpus  = cur_line.PopWord().AsInt();
    int length      = cur_line.PopWord().AsInt();
    double merit    = cur_line.PopWord().AsDouble();
    int gest_time   = cur_line.PopWord().AsInt();
    double fitness  = cur_line.PopWord().AsDouble();
    int update_born = cur_line.PopWord().AsInt();
    int update_dead = cur_line.PopWord().AsInt();
    int depth       = cur_line.PopWord().AsInt();
    cString name = cStringUtil::Stringf("org-%d", id_num);
    
    cAnalyzeGenotype * genotype = new cAnalyzeGenotype(m_world, cur_line.PopWord(), inst_set);
    
    genotype->SetID(id_num);
    genotype->SetParentID(parent_id);
    genotype->SetParentDist(parent_dist);
    genotype->SetNumCPUs(num_cpus);
    genotype->SetTotalCPUs(total_cpus);
    genotype->SetLength(length);
    genotype->SetMerit(merit);
    genotype->SetGestTime(gest_time);
    genotype->SetFitness(fitness);
    genotype->SetUpdateBorn(update_born);
    genotype->SetUpdateDead(update_dead);
    genotype->SetDepth(depth);
    genotype->SetName(name);
    
    // Add this genotype to the proper batch.
    batch[cur_batch].List().PushRear(genotype);
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::LoadMultiDetail(cString cur_string)
{
  // LOAD_MULTI_DETAIL
  
  int start_UD = cur_string.PopWord().AsInt();
  int step_UD = cur_string.PopWord().AsInt();
  int stop_UD = cur_string.PopWord().AsInt();
  cString data_directory = PopDirectory(cur_string, "./");
  cur_batch = cur_string.PopWord().AsInt();
  
  if (step_UD == 0) {
    cerr << "Error: LOAD_MULTI_DETAIL must have a non-zero value for step."
    << endl;
    return;
  }
  
  int num_steps = (stop_UD - start_UD) / step_UD + 1;
  if (num_steps > 1000) {
    cerr << "Warning: Loading over 1000 files in LOAD_MULTI_DETAIL!"
    << endl;
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Loading in " << num_steps
    << " detail files from update " << start_UD
    << " to update " << stop_UD
    << endl;
  } else {
    cout << "Running LOAD_MULTI_DETAIL" << endl;
  }
  
  for (int cur_UD = start_UD; cur_UD <= stop_UD; cur_UD += step_UD) {
    cString filename = cStringUtil::Stringf("detail_pop.%d", cur_UD);
    
    cout << "Loading '" << filename
      << "' into batch " << cur_batch
      << endl;
    
    cInitFile input_file(filename);
    if (!input_file.WasOpened()) {
      tConstListIterator<cString> err_it(input_file.GetErrors());
      const cString* errstr = NULL;
      while ((errstr = err_it.Next())) cerr << "Error: " << *errstr << endl;
      cerr << "Error: Cannot load file: \"" << filename << "\"." << endl;
      if (exit_on_error) exit(1);
    }
    
    // Setup the genome...
    
    for (int line_id = 0; line_id < input_file.GetNumLines(); line_id++) {
      cString cur_line = input_file.GetLine(line_id);
      
      // Setup the genotype for this line...
      
      int id_num      = cur_line.PopWord().AsInt();
      int parent_id   = cur_line.PopWord().AsInt();
      int parent_dist = cur_line.PopWord().AsInt();
      int num_cpus    = cur_line.PopWord().AsInt();
      int total_cpus  = cur_line.PopWord().AsInt();
      int length      = cur_line.PopWord().AsInt();
      double merit    = cur_line.PopWord().AsDouble();
      int gest_time   = cur_line.PopWord().AsInt();
      double fitness  = cur_line.PopWord().AsDouble();
      int update_born = cur_line.PopWord().AsInt();
      int update_dead = cur_line.PopWord().AsInt();
      int depth       = cur_line.PopWord().AsInt();
      cString name = cStringUtil::Stringf("org-%d", id_num);
      
      cAnalyzeGenotype * genotype = new cAnalyzeGenotype(m_world, cur_line.PopWord(), inst_set);
      
      genotype->SetID(id_num);
      genotype->SetParentID(parent_id);
      genotype->SetParentDist(parent_dist);
      genotype->SetNumCPUs(num_cpus);
      genotype->SetTotalCPUs(total_cpus);
      genotype->SetLength(length);
      genotype->SetMerit(merit);
      genotype->SetGestTime(gest_time);
      genotype->SetFitness(fitness);
      genotype->SetUpdateBorn(update_born);
      genotype->SetUpdateDead(update_dead);
      genotype->SetDepth(depth);
      genotype->SetName(name);
      
      // Add this genotype to the proper batch.
      batch[cur_batch].List().PushRear(genotype);
    }
    
    // Adjust the flags on this batch
    batch[cur_batch].SetLineage(false);
    batch[cur_batch].SetAligned(false);
    
    cur_batch++;
  }
}

void cAnalyze::LoadSequence(cString cur_string)
{
  // LOAD_SEQUENCE
  
  static int sequence_count = 1;
  cString sequence = cur_string.PopWord();
  cString seq_name = cur_string.PopWord();
  
  cout << "Loading: " << sequence << endl;
  
  // Setup the genotype...
  cAnalyzeGenotype * genotype = new cAnalyzeGenotype(m_world, sequence, inst_set);
  
  genotype->SetNumCPUs(1);      // Initialize to a single organism.
  if (seq_name == "") {
    seq_name = cStringUtil::Stringf("org-Seq%d", sequence_count);
  }
  genotype->SetName(seq_name);
  sequence_count++;
  
  // Add this genotype to the proper batch.
  batch[cur_batch].List().PushRear(genotype);
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::LoadDominant(cString cur_string)
{
  (void) cur_string;
  cerr << "Warning: \"LOAD_DOMINANT\" not implemented yet!"<<endl;
}

// Clears the current time oriented list of resources and loads in a new one
// from a file specified by the user, or resource.dat by default.
void cAnalyze::LoadResources(cString cur_string)
{
  resources.clear();
  
  int words = cur_string.CountNumWords();
  
  cString filename = "resource.dat";
  if(words >= 1) {
    filename = cur_string.PopWord();
  }
  if(words >= 2) {
    m_resource_time_spent_offset = cur_string.PopWord().AsInt();
  }
  
  cout << "Loading Resources from: " << filename << endl;
  
  // Process the resource.dat, currently assuming this is the only possible
  // input file
  ifstream resourceFile(filename, ios::in);
  if ( !resourceFile.good() ) {
    m_world->GetDriver().RaiseException("Failed to load resource file.");
    return;
  }
      
  // Read in each line of the resource file and process in
  char line[4096];
  while(!resourceFile.eof()) {
    resourceFile.getline(line, 4095);
    
    // Get rid of the whitespace at the beginning of the stream, then 
    // see if the line begins with a #, if so move on to the next line.
    stringstream ss;
    ss << line; ss >> ws; 
    if( (ss.peek() == '#') || (!ss.good()) ) { continue; }
    
    // Read the update number from the stream
    int update;
    ss >> update; assert(ss.good());
    
    // Read in the concentration values for the current update and put them
    // into a temporary vector.
    double x;
    vector<double> tempValues;
    // Loop until the stream is no longer valid, which means either all the
    // data has been processed or a non-numeric was encountered.  Currently
    // assuming a non-numeric is a comment denoting the rest of the line as
    // not informational.
    while(true) {
      ss >> ws; ss >> x;
      tempValues.push_back(x);
      if(!ss.good()) { ss.clear(); break; }
    }
    // Can't have no resources, so assert
    if(tempValues.empty()) { assert(0); }
    // add the update to the list of updates.  Also assuming the values
    // in the file are already sorted.  If this turns out not to be the
    // case you will need to sort on resources[i].first
    resources.push_back(make_pair(update, tempValues));
  }
  resourceFile.close();

  return;
}

double cAnalyze::AnalyzeEntropy(cAnalyzeGenotype * genotype, double mu) 
{
  double entropy = 0.0;
  
  // If the fitness is 0, the entropy is the length of genotype ...
  genotype->Recalculate(m_ctx);
  if (genotype->GetFitness() == 0) {
    return genotype->GetLength();
  }
  
  // Calculate the stats for the genotype we're working with ...
  const int num_insts = inst_set.GetSize();
  const int num_lines = genotype->GetLength();
  const cGenome & base_genome = genotype->GetGenome();
  cGenome mod_genome(base_genome);
  double base_fitness = genotype->GetFitness();
  
  // Loop through all the lines of code, testing all mutations...
  tArray<double> test_fitness(num_insts);
  tArray<double> prob(num_insts);
  for (int line_no = 0; line_no < num_lines; line_no ++) {
    int cur_inst = base_genome.GetOp(line_no);
    
    // Test fitness of each mutant.
    for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) {
      mod_genome.SetOp(line_no, mod_inst);
      cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
      test_genotype.Recalculate(m_ctx);
      // Ajust fitness ...
      if (test_genotype.GetFitness() <= base_fitness) {
        test_fitness[mod_inst] = test_genotype.GetFitness();
      } else {
        test_fitness[mod_inst] = base_fitness;
      }
    }
    
    // Calculate probabilities at mut-sel balance
    double w_bar = 1;
    
    // Normalize fitness values
    double maxFitness = 0.0;
    for(int i=0; i<num_insts; i++) {
      if(test_fitness[i] > maxFitness) {
        maxFitness = test_fitness[i];
      }
    }
    
    
    for(int i=0; i<num_insts; i++) {
      test_fitness[i] /= maxFitness;
    }
    
    while(1) {
      double sum = 0.0;
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst ++) {
        prob[mod_inst] = (mu * w_bar) / 
        ((double)num_insts * 
         (w_bar + test_fitness[mod_inst] * mu - test_fitness[mod_inst]));
        sum = sum + prob[mod_inst];
      }
      if ((sum-1.0)*(sum-1.0) <= 0.0001) 
        break;
      else
        w_bar = w_bar - 0.000001;
    }
    
    // Calculate entropy ...
    double this_entropy = 0.0;
    for (int i = 0; i < num_insts; i ++) {
      this_entropy += prob[i] * log((double) 1.0/prob[i]) / log ((double) num_insts);
    }
    entropy += this_entropy;
    
    // Reset the mod_genome back to the original sequence.
    mod_genome.SetOp(line_no, cur_inst);
  }
  return entropy;
}

//@ MRR @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
tMatrix< double > cAnalyze::AnalyzeEntropyPairs(cAnalyzeGenotype * genotype, double mu) 
{
  
  double entropy = 0.0;
  
  genotype->Recalculate(m_ctx);
  
  // Calculate the stats for the genotype we're working with ...
  const int num_insts = inst_set.GetSize();
  const int num_lines = genotype->GetLength();
  const cGenome & base_genome = genotype->GetGenome();
  cGenome mod_genome(base_genome);
  double base_fitness = genotype->GetFitness();
  
  cout << num_lines << endl;
  tMatrix< double > pairwiseEntropy(num_lines, num_lines);
  for (int i=0; i<num_lines; i++)
    for (int j=-0; j<num_lines; j++)
      pairwiseEntropy[i][j] = 0.0;
  
  cout << pairwiseEntropy.GetNumRows() << endl;
  
  // If the fitness is 0, return empty matrix
  
  if (genotype->GetFitness() == 0) {
    return pairwiseEntropy;
  }
  
  
  tMatrix< double >  test_fitness(num_insts,num_insts);
  tMatrix< double >  prob(num_insts,num_insts);
  
  //Pairwise mutations; the diagonal of the matrix will be the information
  //stored by that site alone
  for (int line_1 = 0; line_1 < num_lines; line_1++){ 
    for (int line_2 = line_1; line_2 < num_lines; line_2++) { 
      
      cerr << "[ " << line_1 << ", " << line_2 << " ]" << endl;
      
      int cur_inst_1 = base_genome.GetOp(line_1); 
      int cur_inst_2 = base_genome.GetOp(line_2);
      
      // Test fitness of each mutant.
      for (int mod_inst_1 = 0; mod_inst_1 < num_insts; mod_inst_1++){
        for (int mod_inst_2 = 0; mod_inst_2 < num_insts; mod_inst_2++) {
          mod_genome.SetOp(line_1, mod_inst_1);
          mod_genome.SetOp(line_2, mod_inst_2);
          cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
          test_genotype.Recalculate(m_ctx);
          // Adjust fitness ...
          if (test_genotype.GetFitness() <= base_fitness) {
            test_fitness[mod_inst_1][mod_inst_2] = test_genotype.GetFitness();
          } else {
            test_fitness[mod_inst_1][mod_inst_2] = base_fitness;
          }
        }
      }
      
      // Calculate probabilities at mut-sel balance
      double w_bar = 1;
      
      // Normalize fitness values
      double maxFitness = 0.0;
      for(int i=0; i<num_insts; i++) {
        for (int j = 0; j < num_insts; j++){
          if(test_fitness[i][j] > maxFitness) {
            maxFitness = test_fitness[i][j];
          }
        }
      }
      
      
      for(int i=0; i<num_insts; i++) {
        for (int j=0; j<num_insts; j++){
          test_fitness[i][j] /= maxFitness;
        }
      }
      
      
      while(1) {
        double sum = 0.0;
        for (int mod_inst_1 = 0; mod_inst_1 < num_insts; mod_inst_1++) {
          for (int mod_inst_2 = 0; mod_inst_2 < num_insts; mod_inst_2++){
            
            prob[mod_inst_1][mod_inst_2] = 
            (mu * w_bar) / 
            ((double)num_insts * 
             (w_bar + test_fitness[mod_inst_1][mod_inst_2] 
              * mu  - test_fitness[mod_inst_1][mod_inst_2]));
            sum = sum + prob[mod_inst_1][mod_inst_2];
          }
        }
        if ((sum-1.0)*(sum-1.0) <= 0.0001) 
          break;
        else
          w_bar = w_bar - 0.000001;
      }
      
      // Calculate entropy ...
      double this_entropy = 0.0;
      for (int i = 0; i < num_insts; i++){
        for (int j = 0; j < num_insts; j++) {
          this_entropy += prob[i][j] * 
          log((double) 1.0/prob[i][j]) / log ((double) num_insts);
        }
      }
      entropy += this_entropy;
      pairwiseEntropy[line_1][line_2] = this_entropy;
      
      // Reset the mod_genome back to the original sequence.
      mod_genome.SetOp(line_1, cur_inst_1);
      mod_genome.SetOp(line_2, cur_inst_2);
      
    }  
  }  //End Loops
  return pairwiseEntropy;
}




double cAnalyze::AnalyzeEntropyGivenParent(cAnalyzeGenotype * genotype,
                                           cAnalyzeGenotype * parent, double mut_rate)
{
  double entropy = 0.0;
  
  // Calculate the stats for the genotype we're working with ...
  genotype->Recalculate(m_ctx);
  const int num_insts = inst_set.GetSize();
  const int num_lines = genotype->GetLength();
  const cGenome & base_genome = genotype->GetGenome();
  const cGenome & parent_genome = parent->GetGenome();
  cGenome mod_genome(base_genome);
  
  // Loop through all the lines of code, testing all mutations ...
  tArray<double> test_fitness(num_insts);
  tArray<double> prob(num_insts);
  for (int line_no = 0; line_no < num_lines; line_no ++) {
    int cur_inst = base_genome.GetOp(line_no);
    int parent_inst = parent_genome.GetOp(line_no);
    
    // Test fitness of each mutant.
    for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) {
      mod_genome.SetOp(line_no, mod_inst);
      cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
      test_genotype.Recalculate(m_ctx);
      test_fitness[mod_inst] = test_genotype.GetFitness();
    }
    
    
    // Calculate probabilities at mut-sel balance
    double w_bar = 1;
    
    // Normalize fitness values, assert if they are all zero
    double maxFitness = 0.0;
    for(int i=0; i<num_insts; i++) {
      if ( i == parent_inst) { continue; }
      if (test_fitness[i] > maxFitness) {
        maxFitness = test_fitness[i];
      }
    }
    
    if(maxFitness > 0) {
      for(int i = 0; i < num_insts; i ++) {
        if (i == parent_inst) { continue; }
        test_fitness[i] /= maxFitness;
      }
    } else {
      // every other inst is equally likely to be mutated to
      for (int i = 0; i < num_insts; i ++) {
        if (i == parent_inst) { continue; }
        test_fitness[i] = 1;
      }
    }
    
    double double_num_insts = num_insts * 1.0;
    while(1) {
      double sum = 0.0;
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst ++) {
        if (mod_inst == parent_inst) { continue; }
        prob[mod_inst] = (mut_rate * w_bar) / 
        (double_num_insts-2) / 
        (w_bar + test_fitness[mod_inst] * mut_rate * (double_num_insts-1) / (double_num_insts - 2) 
         - test_fitness[mod_inst]);
        sum = sum + prob[mod_inst];
      }
      if ((sum-1.0)*(sum-1.0) <= 0.0001) 
        break;
      else
        w_bar = w_bar - 0.000001;
    }
    
    // Calculate entropy ...
    double this_entropy = 0.0;
    this_entropy -= (1.0 - mut_rate) * log(1.0 - mut_rate) / log(static_cast<double>(num_insts));
    for (int i = 0; i < num_insts; i ++) {
      if (i == parent_inst) { continue; }
      prob[i] = prob[i] * mut_rate;
      this_entropy += prob[i] * log(static_cast<double>(1.0/prob[i])) / log (static_cast<double>(num_insts));
    }
    entropy += this_entropy;
    
    // Reset the mod_genome back to the base_genome.
    mod_genome.SetOp(line_no, cur_inst);
  }
  return entropy;
}

double cAnalyze::IncreasedInfo(cAnalyzeGenotype * genotype1,
                               cAnalyzeGenotype * genotype2,
                               double mu) 
{
  double increased_info = 0.0;
  
  // Calculate the stats for the genotypes we're working with ...
  if ( genotype1->GetLength() != genotype2->GetLength() ) {
    cerr << "Error: Two genotypes don't have same length.(cAnalyze::IncreasedInfo)" << endl;
    if (exit_on_error) exit(1);
  }
  
  genotype1->Recalculate(m_ctx);
  if (genotype1->GetFitness() == 0) {
    return 0.0;
  }
  
  const int num_insts = inst_set.GetSize();
  const int num_lines = genotype1->GetLength();
  const cGenome & genotype1_base_genome = genotype1->GetGenome();
  cGenome genotype1_mod_genome(genotype1_base_genome);
  double genotype1_base_fitness = genotype1->GetFitness();
  vector<double> genotype1_info(num_lines, 0.0);
  
  // Loop through all the lines of code, calculate genotype1 information
  tArray<double> test_fitness(num_insts);
  tArray<double> prob(num_insts);
  for (int line_no = 0; line_no < num_lines; line_no ++) {
    int cur_inst = genotype1_base_genome.GetOp(line_no);
    
    // Test fitness of each mutant.
    for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) {
      genotype1_mod_genome.SetOp(line_no, mod_inst);
      cAnalyzeGenotype test_genotype(m_world, genotype1_mod_genome, inst_set);
      test_genotype.Recalculate(m_ctx);
      // Ajust fitness ...
      if (test_genotype.GetFitness() <= genotype1_base_fitness) {
        test_fitness[mod_inst] = test_genotype.GetFitness();
      } else {
        test_fitness[mod_inst] = genotype1_base_fitness;
      }
    }
    
    // Calculate probabilities at mut-sel balance
    double w_bar = 1;
    
    // Normalize fitness values
    double maxFitness = 0.0;
    for(int i=0; i<num_insts; i++) {
      if(test_fitness[i] > maxFitness) {
        maxFitness = test_fitness[i];
      }
    }
    
    for(int i=0; i<num_insts; i++) {
      test_fitness[i] /= maxFitness;
    }
    
    while(1) {
      double sum = 0.0;
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst ++) {
        prob[mod_inst] = (mu * w_bar) / 
        ((double)num_insts * 
         (w_bar + test_fitness[mod_inst] * mu - test_fitness[mod_inst]));
        sum = sum + prob[mod_inst];
      }
      if ((sum-1.0)*(sum-1.0) <= 0.0001) 
        break;
      else
        w_bar = w_bar - 0.000001;
    }
    
    // Calculate entropy ...
    double this_entropy = 0.0;
    for (int i = 0; i < num_insts; i ++) {
      this_entropy += prob[i] * log((double) 1.0/prob[i]) / log ((double) num_insts);
    }
    genotype1_info[line_no] = 1 - this_entropy;
    
    // Reset the mod_genome back to the original sequence.
    genotype1_mod_genome.SetOp(line_no, cur_inst);
  }
  
  genotype2->Recalculate(m_ctx);
  if (genotype2->GetFitness() == 0) {
    for (int line_no = 0; line_no < num_lines; ++ line_no) {
      increased_info += genotype1_info[line_no];
    }
    return increased_info;
  }
  
  const cGenome & genotype2_base_genome = genotype2->GetGenome();
  cGenome genotype2_mod_genome(genotype2_base_genome);
  double genotype2_base_fitness = genotype2->GetFitness();
  
  // Loop through all the lines of code, calculate increased information
  for (int line_no = 0; line_no < num_lines; line_no ++) {
    int cur_inst = genotype2_base_genome.GetOp(line_no);
    
    // Test fitness of each mutant.
    for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) {
      genotype2_mod_genome.SetOp(line_no, mod_inst);
      cAnalyzeGenotype test_genotype(m_world, genotype2_mod_genome, inst_set);
      test_genotype.Recalculate(m_ctx);
      // Ajust fitness ...
      if (test_genotype.GetFitness() <= genotype2_base_fitness) {
        test_fitness[mod_inst] = test_genotype.GetFitness();
      } else {
        test_fitness[mod_inst] = genotype2_base_fitness;
      }
    }
    
    // Calculate probabilities at mut-sel balance
    double w_bar = 1;
    
    // Normalize fitness values, assert if they are all zero
    double maxFitness = 0.0;
    for(int i=0; i<num_insts; i++) {
      if(test_fitness[i] > maxFitness) {
        maxFitness = test_fitness[i];
      }
    }
    
    for(int i=0; i<num_insts; i++) {
      test_fitness[i] /= maxFitness;
    }
    
    while(1) {
      double sum = 0.0;
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst ++) {
        prob[mod_inst] = (mu * w_bar) / 
        ((double)num_insts * 
         (w_bar + test_fitness[mod_inst] * mu - test_fitness[mod_inst]));
        sum = sum + prob[mod_inst];
      }
      if ((sum-1.0)*(sum-1.0) <= 0.0001) 
        break;
      else
        w_bar = w_bar - 0.000001;
    }
    
    // Calculate entropy ...
    double this_entropy = 0.0;
    for (int i = 0; i < num_insts; i ++) {
      this_entropy += prob[i] * log((double) 1.0/prob[i]) / log ((double) num_insts);
    }
    
    // Compare information 
    if (genotype1_info[line_no] > 1 - this_entropy) {
      increased_info += genotype1_info[line_no] - (1 - this_entropy);
    } // else increasing is 0, do nothing
    
    // Reset the mod_genome back to the original sequence.
    genotype2_mod_genome.SetOp(line_no, cur_inst);
  }
  
  
  return increased_info;
}

void cAnalyze::LoadFile(cString cur_string)
{
  // LOAD
  
  cString filename = cur_string.PopWord();
  
  cout << "Loading: " << filename << endl;
  
  cInitFile input_file(filename);
  if (!input_file.WasOpened()) {
    tConstListIterator<cString> err_it(input_file.GetErrors());
    const cString* errstr = NULL;
    while ((errstr = err_it.Next())) cerr << "Error: " << *errstr << endl;
    cerr << "Error: Cannot load file: \"" << filename << "\"." << endl;
    if (exit_on_error) exit(1);
  }
  
  const cString filetype = input_file.GetFiletype();
  if (filetype != "population_data" &&  // Depricated
      filetype != "genotype_data") {
    cerr << "Error: Cannot load files of type \"" << filetype << "\"." << endl;
    if (exit_on_error) exit(1);
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Loading file of type: " << filetype << endl;
  }
  
  
  // Construct a linked list of data types that can be loaded...
  tList< tDataEntryCommand<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > output_it(output_list);
  LoadGenotypeDataList(input_file.GetFormat(), output_list);
  bool id_inc = input_file.GetFormat().HasString("id");
  
  // Setup the genome...
  cGenome default_genome(1);
  int load_count = 0;
  
  for (int line_id = 0; line_id < input_file.GetNumLines(); line_id++) {
    cString cur_line = input_file.GetLine(line_id);
    
    cAnalyzeGenotype* genotype = new cAnalyzeGenotype(m_world, default_genome, inst_set);
    
    output_it.Reset();
    tDataEntryCommand<cAnalyzeGenotype>* data_command = NULL;
    while ((data_command = output_it.Next()) != NULL) {
      data_command->SetTarget(genotype);
      //        genotype->SetSpecialArgs(data_command->GetArgs());
      data_command->SetValue(cur_line.PopWord());
    }
    
    // Give this genotype a name.  Base it on the ID if possible.
    if (id_inc == false) {
      cString name = cStringUtil::Stringf("org-%d", load_count++);
      genotype->SetName(name);
    }
    else {
      cString name = cStringUtil::Stringf("org-%d", genotype->GetID());
      genotype->SetName(name);
    }
    
    // Add this genotype to the proper batch.
    batch[cur_batch].List().PushRear(genotype);
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}


//////////////// Reduction....

void cAnalyze::CommandFilter(cString cur_string)
{
  // First three arguments are: setting, relation, comparison
  // Fourth argument is optional batch.
  
  const int num_args = cur_string.CountNumWords();
  cString stat_name = cur_string.PopWord();
  cString relation = cur_string.PopWord();
  cString test_value = cur_string.PopWord();
  
  // Get the dynamic command to look up the stat we need.
  tDataEntryCommand<cAnalyzeGenotype> * stat_command = GetGenotypeDataCommand(stat_name);
  
  
  // Check for various possible errors before moving on...
  bool error_found = false;
  if (num_args < 3 || num_args > 4) {
    cerr << "Error: Incorrect argument count." << endl;
    error_found = true;
  }
  
  if (stat_command == NULL) {
    cerr << "Error: Unknown stat '" << stat_name << "'" << endl;
    error_found = true;
  }
  
  // Check relationship types.  rel_ok[0] = less_ok; rel_ok[1] = same_ok; rel_ok[2] = gtr_ok
  tArray<bool> rel_ok(3, false);
  if (relation == "==")      {                    rel_ok[1] = true;                    }
  else if (relation == "!=") { rel_ok[0] = true;                     rel_ok[2] = true; }
  else if (relation == "<")  { rel_ok[0] = true;                                       }
  else if (relation == ">")  {                                       rel_ok[2] = true; }
  else if (relation == "<=") { rel_ok[0] = true;  rel_ok[1] = true;                    }
  else if (relation == ">=") {                    rel_ok[1] = true;  rel_ok[2] = true; }
  else {
    cerr << "Error: Unknown relation '" << relation << "'" << endl;
    error_found = true;
  }
  
  if (error_found == true) {
    cerr << "Format: FILTER [stat] [relation] [value] [batch=current]" << endl;
    cerr << "Example: FILTER fitness >= 10.0" << endl;
    if (exit_on_error) exit(1);
    if (stat_command != NULL) delete stat_command;
    return;
  }
  
  
  // If we made it this far, we're going ahead with the command...
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Filtering batch " << cur_batch << " to genotypes where "
    << stat_name << " " << relation << " " << test_value << endl;
  }
  
  
  // Loop through the genotypes and remove the entries that don't match.
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * cur_genotype = NULL;
  while ((cur_genotype = batch_it.Next()) != NULL) {
    const cFlexVar value = stat_command->GetValue(cur_genotype);
    int compare = 1 + CompareFlexStat(value, test_value);
    
    // Check if we should eliminate this genotype...
    if (rel_ok[compare] == false) {
      delete batch_it.Remove();
    }
  }
  delete stat_command;
  
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::FindGenotype(cString cur_string)
{
  // If no arguments are passed in, just find max num_cpus.
  if (cur_string.GetSize() == 0) cur_string = "num_cpus";
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Reducing batch " << cur_batch << " to genotypes: ";
  }
  
  tListPlus<cAnalyzeGenotype> & gen_list = batch[cur_batch].List();
  tListPlus<cAnalyzeGenotype> found_list;
  while (cur_string.CountNumWords() > 0) {
    cString gen_desc(cur_string.PopWord());
    if (m_world->GetVerbosity() >= VERBOSE_ON) cout << gen_desc << " ";
    
    // Determine by lin_type which genotype were are tracking...
    cAnalyzeGenotype * found_gen = PopGenotype(gen_desc, cur_batch);
    
    if (found_gen == NULL) {
      cerr << "  Warning: genotype not found!" << endl;
      continue;
    }
    
    // Save this genotype...
    found_list.Push(found_gen);
  }
  cout << endl;
  
  // Delete all genotypes other than the ones found!
  while (gen_list.GetSize() > 0) delete gen_list.Pop();
  
  // And fill it back in with the good stuff.
  while (found_list.GetSize() > 0) gen_list.Push(found_list.Pop());
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::FindOrganism(cString cur_string)
{
  // At least one argument is rquired.
  if (cur_string.GetSize() == 0) {
    cerr << "Error: At least one argument is required in FIND_ORGANISM." << endl;
    cerr << " (perhaps you want FIND_GENOTYPE?)" << endl;
    return;
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Reducing batch " << cur_batch << " to organisms: " << endl;
  }
  
  tListPlus<cAnalyzeGenotype> & gen_list = batch[cur_batch].List();
  tListPlus<cAnalyzeGenotype> found_list;
  
  tArray<int> new_counts(gen_list.GetSize());
  new_counts.SetAll(0);
  
  while (cur_string.CountNumWords() > 0) {
    cString org_desc(cur_string.PopWord());
    if (m_world->GetVerbosity() >= VERBOSE_ON) cout << org_desc << " ";
    
    // Determine by org_desc which genotype were are tracking...
    if (org_desc == "random") {
      bool found = false; 
      int num_orgs = gen_list.Count(&cAnalyzeGenotype::GetNumCPUs);
      while (found != true) {
      	int org_chosen = random.GetUInt(num_orgs);
      	cAnalyzeGenotype * found_genotype = 
          gen_list.FindSummedValue(org_chosen, &cAnalyzeGenotype::GetNumCPUs);
        if ( found_genotype->GetNumCPUs() != 0 && found_genotype->GetViable()) {
          found_genotype->SetNumCPUs(found_genotype->GetNumCPUs()-1);
          new_counts[gen_list.FindPosPtr(found_genotype)] +=1;
          cout << "Found genotype " << gen_list.FindPosPtr(found_genotype) << endl;
          found = true;
        }
      }
    }
    
    // pick a random organisms, with replacement!
    if (org_desc == "randomWR") {
      bool found = false;
      int num_orgs = gen_list.Count(&cAnalyzeGenotype::GetNumCPUs);
      while (found != true) {
        int org_chosen = random.GetUInt(num_orgs);
        cAnalyzeGenotype * found_genotype =
          gen_list.FindSummedValue(org_chosen, &cAnalyzeGenotype::GetNumCPUs);
        if ( found_genotype->GetNumCPUs() != 0 && found_genotype->GetViable()) {
          new_counts[gen_list.FindPosPtr(found_genotype)] +=1;
          cout << "Found genotype " << gen_list.FindPosPtr(found_genotype) << endl;
          found = true;
        }
      }
    }
  }
  
  int pos_count = 0;
  cAnalyzeGenotype * genotype = NULL;
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  
  while ((genotype = batch_it.Next()) != NULL) {
    genotype->SetNumCPUs(new_counts[pos_count]);
    if (genotype->GetNumCPUs() == 0) batch_it.Remove();
    else cout << "Genotype " << pos_count << " has " << new_counts[pos_count] << " organisms." << endl;
    pos_count++;
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::FindLineage(cString cur_string)
{
  cString lin_type = "num_cpus";
  if (cur_string.CountNumWords() > 0) lin_type = cur_string.PopWord();
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Reducing batch " << cur_batch
    << " to " << lin_type << " lineage " << endl;
  } else cout << "Performing lineage scan..." << endl;
  
  
  // Determine by lin_type which genotype we are tracking...
  cAnalyzeGenotype * found_gen = PopGenotype(lin_type, cur_batch);
  
  if (found_gen == NULL) {
    cerr << "  Warning: Genotype " << lin_type
    << " not found.  Lineage scan aborted." << endl;
    return;
  }
  
  // Otherwise, trace back through the id numbers to mark all of those
  // in the ancestral lineage...
  
  // Construct a list of genotypes found...
  
  tListPlus<cAnalyzeGenotype> found_list;
  found_list.Push(found_gen);
  int next_id = found_gen->GetParentID();
  bool found = true;
  while (found == true) {
    found = false;
    
    tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
    while ((found_gen = batch_it.Next()) != NULL) {
      if (found_gen->GetID() == next_id) {
        batch_it.Remove();
        found_list.Push(found_gen);
        next_id = found_gen->GetParentID();
        found = true;
        break;
      }
    }
  }
  
  // We now have all of the genotypes in this lineage, delete everything
  // else.
  
  const int total_removed = batch[cur_batch].List().GetSize();
  while (batch[cur_batch].List().GetSize() > 0) {
    delete batch[cur_batch].List().Pop();
  }
  
  // And fill it back in with the good stuff.
  int total_kept = found_list.GetSize();
  while (found_list.GetSize() > 0) {
    batch[cur_batch].List().PushRear(found_list.Pop());
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  Lineage has " << total_kept << " genotypes; "
    << total_removed << " were removed." << endl;
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(true);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::FindSexLineage(cString cur_string)
{
  
  // detemine the method for construicting a lineage 
  // by defauly, find the lineage of the final dominant
  cString lin_type = "num_cpus";
  if (cur_string.CountNumWords() > 0) lin_type = cur_string.PopWord();
  
  // parent_method determins which of the two parental lineages to use
  // "rec_region_size" : 
  //		"mother" (dominant parent) is the parent contributing
  // 		more to the offspring genome (default)
  // "genome_size" : 
  //		"mother" (dominant parent) is the longer parent 
  cString parent_method = "rec_region_size";
  if (cur_string.CountNumWords() > 0) parent_method = cur_string.PopWord();
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Reducing batch " << cur_batch
    << " to " << lin_type << " sexual lineage " 
    << " using " << parent_method << " criteria." << endl;
  } else cout << "Performing sexual lineage scan..." << endl;
  
  
  // Determine by lin_type which genotype we are tracking...
  cAnalyzeGenotype * found_gen = PopGenotype(lin_type, cur_batch);
  
  cAnalyzeGenotype * found_dad;
  cAnalyzeGenotype * found_mom;
  cAnalyzeGenotype * found_temp;
  
  if (found_gen == NULL) {
    cerr << "  Warning: Genotype " << lin_type
    << " not found.  Sexual lineage scan aborted." << endl;
    return;
  }
  
  // Otherwise, trace back through the id numbers to mark all of those
  // in the ancestral lineage...
  
  // Construct a list of genotypes found...
  
  tListPlus<cAnalyzeGenotype> found_list;
  found_list.Push(found_gen);
  int next_id1 = found_gen->GetParentID();
  int next_id2 = found_gen->GetParent2ID();
  
  bool found_m = true;
  bool found_d = true;
  
  while (found_m == true & found_d == true) {
    
    //cout << "Searching for mom=" << next_id1
    //	 << " and dad=" << next_id2 << endl;
    found_m = false;
    found_d = false;
    
    // Look for the father first....
    tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
    batch_it.Reset();
    while ((found_dad = batch_it.Next()) != NULL) {
      // Check if the father is found...
      if (found_dad->GetID() == next_id2) {
        //cout << "Found dad!" << endl;
        batch_it.Remove();
        found_list.Push(found_dad);
        found_d = true; 
        break;
      }
    }
    
    // dad may have already been found, check the find list!
    if (found_d == false) {
      tListIterator<cAnalyzeGenotype> found_it(found_list);
      while ((found_dad = found_it.Next()) != NULL) {
        if (found_dad->GetID() == next_id2) {
          //cout << "Found dad in found list!" << endl;
          found_d = true;
          break;
        }
      }
    } 
    
    // Next, look for the mother...
    batch_it.Reset();
    while ((found_mom = batch_it.Next()) != NULL) {
      if (found_mom->GetID() == next_id1) {
        //cout << "Found mom!" << endl;
        batch_it.Remove();
        found_list.Push(found_mom);
        // if finding lineages by parental length, may have to swap 
        if (parent_method == "genome_size" & 
            found_mom->GetLength() < found_dad->GetLength()) { 
          //cout << "Swapping parents!" << endl;
          found_temp = found_mom; 
          found_mom = found_dad; 
          found_dad = found_temp; 
        } 	
        next_id1 = found_mom->GetParentID();
        next_id2 = found_mom->GetParent2ID();
        found_m = true;
        break;
      }
    }
    
    // If the mother was not found, it may already have been placed in the
    // found list as a father... 
    if (found_m == false) {
      tListIterator<cAnalyzeGenotype> found_it(found_list);
      while ((found_mom = found_it.Next()) != NULL) {
        if (found_mom->GetID() == next_id1) {
          //cout << "Found mom as dad!" << endl;
          // Don't move to found list, since its already there, but update
          // to the next ids.
          // if finding lineages by parental length, may have to swap 
          if (parent_method == "genome_size" & 
              found_mom->GetLength() < found_dad->GetLength()) {
            //cout << "Swapping parents!" << endl;
            found_temp = found_mom; 
            found_mom = found_dad;  
            found_dad = found_temp;
          }
          next_id1 = found_mom->GetParentID();
          next_id2 = found_mom->GetParent2ID();
          found_m = true;
          break;
        }
      }
    }    
  }
  
  // We now have all of the genotypes in this lineage, delete everything
  // else.
  
  const int total_removed = batch[cur_batch].List().GetSize();
  while (batch[cur_batch].List().GetSize() > 0) {
    delete batch[cur_batch].List().Pop();
  }
  
  // And fill it back in with the good stuff.
  int total_kept = found_list.GetSize();
  while (found_list.GetSize() > 0) {
    batch[cur_batch].List().PushRear(found_list.Pop());
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  Sexual lineage has " << total_kept << " genotypes; "
    << total_removed << " were removed." << endl;
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::FindClade(cString cur_string)
{
  if (cur_string.GetSize() == 0) {
    cerr << "  Warning: No clade specified for FIND_CLADE.  Aborting." << endl;
    return;
  }
  
  cString clade_type( cur_string.PopWord() );
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Reducing batch " << cur_batch
    << " to clade " << clade_type << "." << endl;
  } else cout << "Performing clade scan..." << endl;
  
  
  // Determine by clade_type which genotype we are tracking...
  cAnalyzeGenotype * found_gen = PopGenotype(clade_type, cur_batch);
  
  if (found_gen == NULL) {
    cerr << "  Warning: Ancestral genotype " << clade_type
    << " not found.  Clade scan aborted." << endl;
    return;
  }
  
  // Do this the brute force way... scan for one step at a time.
  
  // Construct a list of genotypes found...
  
  tListPlus<cAnalyzeGenotype> found_list; // Found and finished.
  tListPlus<cAnalyzeGenotype> scan_list;  // Found, but need to scan for children.
  scan_list.Push(found_gen);
  
  // Keep going as long as there is something in the scan list...
  while (scan_list.GetSize() > 0) {
    // Move the next genotype from the scan list to the found_list.
    found_gen = scan_list.Pop();
    int parent_id = found_gen->GetID();
    found_list.Push(found_gen);
    
    // Seach for all of the children of this genotype...
    tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
    while ((found_gen = batch_it.Next()) != NULL) {
      // If we found a child, place it into the scan list.
      if (found_gen->GetParentID() == parent_id) {
        batch_it.Remove();
        scan_list.Push(found_gen);
      }
    }
  }
  
  // We now have all of the genotypes in this clade, delete everything else.
  
  const int total_removed = batch[cur_batch].List().GetSize();
  while (batch[cur_batch].List().GetSize() > 0) {
    delete batch[cur_batch].List().Pop();
  }
  
  // And fill it back in with the good stuff.
  int total_kept = found_list.GetSize();
  while (found_list.GetSize() > 0) {
    batch[cur_batch].List().PushRear(found_list.Pop());
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  Clade has " << total_kept << " genotypes; "
    << total_removed << " were removed." << endl;
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::SampleOrganisms(cString cur_string)
{
  double fraction = cur_string.PopWord().AsDouble();
  int init_genotypes = batch[cur_batch].List().GetSize();
  
  double test_viable = 0; 
  if (cur_string.GetSize() > 0) {
    test_viable = cur_string.PopWord().AsDouble();
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Sampling " << fraction << " organisms from batch "
    << cur_batch << "." << endl;
  }
  else cout << "Sampling Organisms..." << endl;
  
  cAnalyzeGenotype * genotype = NULL;
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  
  // Loop through all genotypes to perform a census
  int org_count = 0;
  while ((genotype = batch_it.Next()) != NULL) {
    // If we require viables, reduce all non-viables to zero organisms.
    if (test_viable == 1  &&  genotype->GetViable() == 0) { 
      genotype->SetNumCPUs(0);
    }
    
    // Count the number of organisms in this genotype.
    org_count += genotype->GetNumCPUs();
  } 
  
  // Create an array to store pointers to the genotypes and fill it in
  // while temporarily resetting all of the organism counts to zero.
  tArray<cAnalyzeGenotype *> org_array(org_count);
  int cur_org = 0;
  batch_it.Reset();
  while ((genotype = batch_it.Next()) != NULL) {
    for (int i = 0; i < genotype->GetNumCPUs(); i++) {
      org_array[cur_org] = genotype;
      cur_org++;
    }
    genotype->SetNumCPUs(0);
  }
  
  assert(cur_org == org_count);
  
  // Determine how many organisms we want to keep.
  int new_org_count = (int) fraction;
  if (fraction < 1.0) new_org_count = (int) (fraction * (double) org_count);
  if (new_org_count > org_count) {
    cerr << "Warning: Trying to sample " << new_org_count
    << "organisms from a population of " << org_count
    << endl;
    new_org_count = org_count;
  }
  
  // Now pick those that we are keeping.
  tArray<int> keep_ids(new_org_count);
  random.Choose(org_count, keep_ids);
  
  // And increment the org counts for the associated genotypes.
  for (int i = 0; i < new_org_count; i++) {
    genotype = org_array[ keep_ids[i] ];
    genotype->SetNumCPUs(genotype->GetNumCPUs() + 1);
  }
  
  
  // Delete all genotypes with no remaining organisms...
  batch_it.Reset();
  while ((genotype = batch_it.Next()) != NULL) {
    if (genotype->GetNumCPUs() == 0) {
      batch_it.Remove();
      delete genotype;
    }
  }
  
  int num_genotypes = batch[cur_batch].List().GetSize();
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  Removed " << org_count - new_org_count
    << " organisms (" << init_genotypes - num_genotypes
    << " genotypes); " << new_org_count
    << " orgs (" << num_genotypes << " gens) remaining."
    << endl;
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}


void cAnalyze::SampleGenotypes(cString cur_string)
{
  double fraction = cur_string.PopWord().AsDouble();
  int init_genotypes = batch[cur_batch].List().GetSize();
  
  double test_viable = 0;
  if (cur_string.GetSize() > 0) {
    test_viable = cur_string.PopWord().AsDouble();
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Sampling " << fraction << " genotypes from batch "
    << cur_batch << "." << endl;
  }
  else cout << "Sampling Genotypes..." << endl;
  
  double frac_remove = 1.0 - fraction;
  
  cAnalyzeGenotype * genotype = NULL;
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  while ((genotype = batch_it.Next()) != NULL) {
    if (random.P(frac_remove) || ((genotype->GetViable())==0 && test_viable==1) ) {
      batch_it.Remove();
      delete genotype;
    }
  }
  
  int num_genotypes = batch[cur_batch].List().GetSize();
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  Removed " << init_genotypes - num_genotypes
    << " genotypes; " << num_genotypes << " remaining."
    << endl;
  }
  
  // Adjust the flags on this batch
  batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::KeepTopGenotypes(cString cur_string)
{
  const int num_kept = cur_string.PopWord().AsInt();
  const int num_genotypes = batch[cur_batch].List().GetSize();
  const int num_removed = num_genotypes - num_kept;
  
  for (int i = 0; i < num_removed; i++) {
    delete batch[cur_batch].List().PopRear();
  }
  
  // Adjust the flags on this batch
  // batch[cur_batch].SetLineage(false); // Should not destroy a lineage...
  batch[cur_batch].SetAligned(false);
}

void cAnalyze::TruncateLineage(cString cur_string)
{
  cString type("task");
  int arg_i = -1;
  if (cur_string.GetSize()) type = cur_string.PopWord();
  if (type == "task") {
    if (cur_string.GetSize()) arg_i = cur_string.PopWord().AsInt();
    const int env_size = m_world->GetEnvironment().GetNumTasks();
    if (arg_i < 0 || arg_i >= env_size) arg_i = env_size - 1;
  }
  cString lin_type("num_cpus");
  if (cur_string.GetSize()) lin_type = cur_string.PopWord();
  FindLineage(lin_type);
  BatchRecalculate("");
  
  if (type == "task") {
    if (m_world->GetVerbosity() >= VERBOSE_ON)
      cout << "Truncating batch " << cur_batch << " based on task " << arg_i << " emergence..." << endl;
    else 
      cout << "Truncating lineage..." << endl;
    
    bool found = false;
    tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
    cAnalyzeGenotype* genotype = NULL;
    
    while ((genotype = batch_it.Next())) {
      if (found) {
        batch_it.Remove();
        delete genotype;
        continue;
      }
      if (genotype->GetTaskCount(arg_i)) found = true;
    }
  }  
}


//////////////// Output Commands...

void cAnalyze::CommandPrint(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing batch " << cur_batch << endl;
  else cout << "Printing organisms..." << endl;
  
  cString directory = PopDirectory(cur_string, "archive/");
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype* genotype = NULL;
  cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();
  while ((genotype = batch_it.Next()) != NULL) {
    cString filename(directory);
    
    if (cur_string.GetSize() > 0) {
      filename += cur_string.PopWord();
    }
    else { 
      filename += genotype->GetName();
      filename += ".gen";
    }
    
    testcpu->PrintGenome(m_ctx, genotype->GetGenome(), filename);
    if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing: " << filename << endl;
  }
  delete testcpu;
}

void cAnalyze::CommandTrace(cString cur_string)
{
  cString msg;
  tArray<int> manual_inputs;
  
  // Process our arguments; manual inputs must be the last arguments

  cString directory      = PopDirectory(cur_string.PopWord(), cString("archive/"));           // #1
  int use_resources      = (cur_string.GetSize()) ? cur_string.PopWord().AsInt() : 0;         // #2
  int update             = (cur_string.GetSize()) ? cur_string.PopWord().AsInt() : -1;        // #3
  bool use_random_inputs = (cur_string.GetSize()) ? cur_string.PopWord().AsInt() == 1: false; // #4
  bool use_manual_inputs = false;                                                             // #5+
  
  //Manual inputs will override random input request
  if (cur_string.CountNumWords() > 0){
    if (cur_string.CountNumWords() == m_world->GetEnvironment().GetInputSize()){
      manual_inputs.Resize(m_world->GetEnvironment().GetInputSize());
      use_random_inputs = false;
      use_manual_inputs = true;
      for (int k = 0; cur_string.GetSize(); k++)
        manual_inputs[k] = cur_string.PopWord().AsInt();
    } else if (m_world->GetVerbosity() >= VERBOSE_ON){
      msg.Set("Invalid number of environment inputs requested for recalculation: %d specified, %d required.", 
              cur_string.CountNumWords(), m_world->GetEnvironment().GetInputSize());
      m_world->GetDriver().NotifyWarning(msg);
    }
  }
  
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) 
    msg.Set("Tracing batch %d", cur_batch);
  else 
    msg.Set("Tracing organisms.");
  m_world->GetDriver().NotifyComment(msg);
  
  cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();  
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    cString filename = directory + genotype->GetName() + cString(".trace");
    
    if (genotype->GetGenome().GetSize() == 0)
      break;
    
    // Build the hardware status printer for tracing.
    ofstream& trace_fp = m_world->GetDataFileOFStream(filename);
    cHardwareStatusPrinter trace_printer(trace_fp);
    
    // Build the test info for printing.
    cCPUTestInfo test_info;
    test_info.SetTraceExecution(&trace_printer);
    if (use_manual_inputs)
      test_info.UseManualInputs(manual_inputs);
    else
      test_info.UseRandomInputs(use_random_inputs); 
    test_info.SetResourceOptions(use_resources, &resources, update, m_resource_time_spent_offset);

    if (m_world->GetVerbosity() >= VERBOSE_ON){
      msg = cString("Tracing ") + filename;
      m_world->GetDriver().NotifyComment(msg);
    }
    
    testcpu->TestGenome(m_ctx, test_info, genotype->GetGenome());
    
    m_world->GetDataFileManager().Remove(filename);
  }
  
  delete testcpu;
}


void cAnalyze::CommandPrintTasks(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing tasks in batch " << cur_batch << endl;
  else cout << "Printing tasks..." << endl;
  
  // Load in the variables...
  cString filename("tasks.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // Loop through all of the genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    fp << genotype->GetID() << " ";
    genotype->PrintTasks(fp);
    fp << endl;
  }
}

void cAnalyze::CommandPrintTasksQuality(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing task qualities in batch " << cur_batch << endl;
  else cout << "Printing task qualities..." << endl;
  
  // Load in the variables...
  cString filename("tasksquality.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // Loop through all of the genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    fp << genotype->GetID() << " ";
    genotype->PrintTasksQuality(fp);
    fp << endl;
  }
}

void cAnalyze::CommandDetail(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Detailing batch " << cur_batch << endl;
  else cout << "Detailing..." << endl;
  
  // Load in the variables...
  cString filename("detail.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  // Construct a linked list of details needed...
  tList< tDataEntryCommand<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > output_it(output_list);
  LoadGenotypeDataList(cur_string, output_list);
  
  // Determine the file type...
  int file_type = FILE_TYPE_TEXT;
  cString file_extension(filename);
  while (file_extension.Find('.') != -1) file_extension.Pop('.');
  if (file_extension == "html") file_type = FILE_TYPE_HTML;
  
  // Setup the file...
  if (filename == "cout") {
    CommandDetail_Header(cout, file_type, output_it);
    CommandDetail_Body(cout, file_type, output_it);
  } else {
    ofstream& fp = m_world->GetDataFileOFStream(filename);
    CommandDetail_Header(fp, file_type, output_it);
    CommandDetail_Body(fp, file_type, output_it);
		m_world->GetDataFileManager().Remove(filename);
	}
  
  // And clean up...
  while (output_list.GetSize() != 0) delete output_list.Pop();
}


void cAnalyze::CommandDetailTimeline(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Detailing batch "
    << cur_batch << " based on time" << endl;
  else cout << "Detailing..." << endl;
  
  // Load in the variables...
  cString filename("detail_timeline.dat");
  int time_step = 100;
  int max_time = 100000;
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  if (cur_string.GetSize() != 0) time_step = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() != 0) max_time = cur_string.PopWord().AsInt();
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  Time step = " << time_step << endl
    << "  Max time = " << max_time << endl;
  }
  
  // Construct a linked list of details needed...
  tList< tDataEntryCommand<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > output_it(output_list);
  LoadGenotypeDataList(cur_string, output_list);
  
  // Determine the file type...
  int file_type = FILE_TYPE_TEXT;
  cString file_extension(filename);
  while (file_extension.Find('.') != -1) file_extension.Pop('.');
  if (file_extension == "html") file_type = FILE_TYPE_HTML;
  
  // Setup the file...
  if (filename == "cout") {
    CommandDetail_Header(cout, file_type, output_it, time_step);
    CommandDetail_Body(cout, file_type, output_it, time_step, max_time);
  } else {
    ofstream& fp = m_world->GetDataFileOFStream(filename);
    CommandDetail_Header(fp, file_type, output_it, time_step);
    CommandDetail_Body(fp, file_type, output_it, time_step, max_time);
  }
  
  // And clean up...
  while (output_list.GetSize() != 0) delete output_list.Pop();
}


void cAnalyze::CommandDetail_Header(ostream& fp, int format_type,
                                    tListIterator< tDataEntryCommand<cAnalyzeGenotype> > & output_it,
                                    int time_step)
{
  // Write out the header on the file
  if (format_type == FILE_TYPE_TEXT) {
    fp << "#filetype genotype_data" << endl;
    fp << "#format ";
    if (time_step > 0) fp << "update ";
    while (output_it.Next() != NULL) {
      const cString & entry_name = output_it.Get()->GetName();
      fp << entry_name << " ";
    }
    fp << endl << endl;
    
    // Give the more human-readable legend.
    fp << "# Legend:" << endl;
    int count = 0;
    if (time_step > 0) fp << "# " << ++count << ": Update" << endl;
    while (output_it.Next() != NULL) {
      const cString & entry_desc = output_it.Get()->GetDesc();
      fp << "# " << ++count << ": " << entry_desc << endl;
    }
    fp << endl;
  } else { // if (format_type == FILE_TYPE_HTML) {
    fp << "<html>" << endl
    << "<body bgcolor=\"#FFFFFF\"" << endl
    << " text=\"#000000\"" << endl
    << " link=\"#0000AA\"" << endl
    << " alink=\"#0000FF\"" << endl
    << " vlink=\"#000044\">" << endl
    << endl
    << "<h1 align=center>Run " << batch[cur_batch].Name() << endl
    << endl
    << "<center>" << endl
    << "<table border=1 cellpadding=2><tr>" << endl;
    
    if (time_step > 0) fp << "<th bgcolor=\"#AAAAFF\">Update ";
    while (output_it.Next() != NULL) {
      const cString & entry_desc = output_it.Get()->GetDesc();
      fp << "<th bgcolor=\"#AAAAFF\">" << entry_desc << " ";
    }
    fp << "</tr>" << endl;
    
  }
  
  }


void cAnalyze::CommandDetail_Body(ostream& fp, int format_type,
                                  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > & output_it,
                                  int time_step, int max_time)
{
  // Loop through all of the genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * cur_genotype = batch_it.Next();
  cAnalyzeGenotype * next_genotype = batch_it.Next();
  cAnalyzeGenotype * prev_genotype = NULL;
  
  int cur_time = 0;
  while (cur_genotype != NULL && cur_time <= max_time) {
    if (m_world->GetVerbosity() >= VERBOSE_DETAILS) {
      cout << "Detailing genotype " << cur_genotype->GetID()
      << " at depth " << cur_genotype->GetDepth()
      << endl;
    }
    output_it.Reset();
    if (format_type == FILE_TYPE_HTML) {
      fp << "<tr>";
      if (time_step > 0) fp << "<td>" << cur_time << " ";
    }
    else if (time_step > 0) {  // TEXT file, printing times...
      fp << cur_time << " ";
    }
    
    tDataEntryCommand<cAnalyzeGenotype> * data_command = NULL;
    while ((data_command = output_it.Next()) != NULL) {
      cur_genotype->SetSpecialArgs(data_command->GetArgs());
      data_command->SetTarget(cur_genotype);
      cFlexVar cur_value = data_command->GetValue(cur_genotype);
      if (format_type == FILE_TYPE_HTML) {
        int compare = 0;
        if (prev_genotype) {
          prev_genotype->SetSpecialArgs(data_command->GetArgs());
          cFlexVar prev_value = data_command->GetValue(prev_genotype);
          int compare_type = data_command->GetCompareType();
          compare = CompareFlexStat(cur_value, prev_value, compare_type);
        }
        HTMLPrintStat(cur_value, fp, compare, data_command->GetHtmlCellFlags(), data_command->GetNull());
      }
      else {  // if (format_type == FILE_TYPE_TEXT) {
        fp << data_command->GetValue() << " ";
      }
      }
    if (format_type == FILE_TYPE_HTML) fp << "</tr>";
    fp << endl;
    
    cur_time += time_step;
    if (time_step > 0) {
      while (next_genotype && next_genotype->GetUpdateBorn() < cur_time) {
        prev_genotype = cur_genotype;
        cur_genotype = next_genotype;
        next_genotype = batch_it.Next();
      }
    }
    else {
      // Always moveon if we're not basing this on time, or if we've run out of genotypes.
      prev_genotype = cur_genotype;
      cur_genotype = next_genotype;
      next_genotype = batch_it.Next();
    }
    
    }
  
  // If in HTML mode, we need to end the file...
  if (format_type == FILE_TYPE_HTML) {
    fp << "</table>" << endl
    << "</center>" << endl;
  }
  }

void cAnalyze::CommandDetailAverage_Body(ostream& fp, int nucoutputs,
                                         tListIterator< tDataEntryCommand<cAnalyzeGenotype> > & output_it)
{
  // Loop through all of the genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * cur_genotype = batch_it.Next();
  cAnalyzeGenotype * next_genotype = batch_it.Next();
  cAnalyzeGenotype * prev_genotype = NULL;
  
  tArray<cDoubleSum> output_counts(nucoutputs);
  for (int i = 0; i < nucoutputs; i++) { output_counts[i].Clear();} 
  int count; 
  while (cur_genotype != NULL) { 
    count = 0; 
    output_it.Reset();
    tDataEntryCommand<cAnalyzeGenotype> * data_command = NULL;
    while ((data_command = output_it.Next()) != NULL) {
      data_command->SetTarget(cur_genotype);
      cur_genotype->SetSpecialArgs(data_command->GetArgs());
      for (int j = 0; j < cur_genotype->GetNumCPUs(); j++) { 
        output_counts[count].Add( data_command->GetValue().AsDouble() );
      } 	
      count++;
    }
    
    prev_genotype = cur_genotype;
    cur_genotype = next_genotype;
    next_genotype = batch_it.Next();
  }
  fp << batch[cur_batch].Name() << " "; 
  for (int i = 0; i < nucoutputs; i++) {  
    fp << output_counts[i].Average() << " ";
  } 
  fp << endl;   
}

void cAnalyze::CommandDetailAverage(cString cur_string) 
{ 
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Average detailing batch " << cur_batch << endl;
  else cout << "Detailing..." << endl;
  
  // Load in the variables...
  cString filename("detail.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  // Construct a linked list of details needed...
  tList< tDataEntryCommand<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > output_it(output_list);
  LoadGenotypeDataList(cur_string, output_list);
  
  // check if file is already in use.
  bool file_active = m_world->GetDataFileManager().IsOpen(filename);
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // if it's a new file print out the header
  if (file_active == false) {
    CommandDetail_Header(fp, FILE_TYPE_TEXT, output_it);
  } 
  CommandDetailAverage_Body(fp, cur_string.CountNumWords(), output_it);
  
  while (output_list.GetSize() != 0) delete output_list.Pop();
  
} 

void cAnalyze::CommandDetailBatches(cString cur_string)
{
  // Load in the variables...
  cString keyword("num_cpus");
  cString filename("detail_batch.dat");
  if (cur_string.GetSize() != 0) keyword = cur_string.PopWord();
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Detailing batches for " << keyword << endl;
  else cout << "Detailing Batches..." << endl;
  
  // Scan the functions list for the keyword we need...
  SetupGenotypeDataList();
  tListIterator< tDataEntryBase<cAnalyzeGenotype> >
    output_it(genotype_data_list);
  
  // Divide up the keyword into its acrual entry and its arguments...
  cString cur_args = keyword;
  cString cur_entry = cur_args.Pop(':');
  
  // Find its associated command...
  tDataEntryCommand<cAnalyzeGenotype> * cur_command = NULL;
  bool found = false;
  while (output_it.Next() != NULL) {
    if (output_it.Get()->GetName() == cur_entry) {
      cur_command = new tDataEntryCommand<cAnalyzeGenotype>
      (output_it.Get(), cur_args);
      found = true;
      break;
    }
  }
  if (found == false) {
    cout << "Error: Unknown data type: " << cur_entry << endl;
    return;
  }
  
  
  // Determine the file type...
  int file_type = FILE_TYPE_TEXT;
  cString file_extension(filename);
  while (file_extension.Find('.') != -1) file_extension.Pop('.');
  if (file_extension == "html") file_type = FILE_TYPE_HTML;
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // Write out the header on the file
  if (file_type == FILE_TYPE_TEXT) {
    fp << "#filetype batch_data" << endl
    << "#format batch_id " << keyword << endl
    << endl;
    
    // Give the more human-readable legend.
    fp << "# Legend:" << endl
      << "#  Column 1 = Batch ID" << endl
      << "#  Remaining entries: " << cur_command->GetDesc() << endl
      << endl;
    
  } else { // if (file_type == FILE_TYPE_HTML) {
    fp << "<html>" << endl
    << "<body bgcolor=\"#FFFFFF\"" << endl
    << " text=\"#000000\"" << endl
    << " link=\"#0000AA\"" << endl
    << " alink=\"#0000FF\"" << endl
    << " vlink=\"#000044\">" << endl
    << endl
    << "<h1 align=center> Distribution of " << cur_command->GetDesc()
    << endl << endl
    << "<center>" << endl
    << "<table border=1 cellpadding=2>" << endl
    << "<tr><th bgcolor=\"#AAAAFF\">" << cur_command->GetDesc() << "</tr>"
    << endl;
  }
  
  
  // Loop through all of the batches...
  for (int i = 0; i < MAX_BATCHES; i++) {
    if (batch[i].List().GetSize() == 0) continue;
    
    if (file_type == FILE_TYPE_HTML) fp << "<tr><td>";
    fp << i << " ";
    
    tListIterator<cAnalyzeGenotype> batch_it(batch[i].List());
    cAnalyzeGenotype * genotype = NULL;
    while ((genotype = batch_it.Next()) != NULL) {
      output_it.Reset();
      if (file_type == FILE_TYPE_HTML) fp << "<td>";
      
      cur_command->SetTarget(genotype);
      genotype->SetSpecialArgs(cur_command->GetArgs());
      if (file_type == FILE_TYPE_HTML) {
        HTMLPrintStat(cur_command->GetValue(), fp, 0, cur_command->GetHtmlCellFlags(), cur_command->GetNull());
      }
      else {  // if (file_type == FILE_TYPE_TEXT) {
        fp << cur_command->GetValue() << " ";
      }
      }
    if (file_type == FILE_TYPE_HTML) fp << "</tr>";
    fp << endl;
    }
  
  // If in HTML mode, we need to end the file...
  if (file_type == FILE_TYPE_HTML) {
    fp << "</table>" << endl
    << "</center>" << endl;
  }
  
  delete cur_command;
  }



void cAnalyze::CommandDetailIndex(cString cur_string)
{
  cout << "Creating a Detail Index..." << endl;
  
  // A filename and min and max batches must be included.
  if (cur_string.CountNumWords() < 3) {
    cerr << "Error: must include filename, and min and max batch numbers." << endl;
    if (exit_on_error) exit(1);
  }
  
  // Load in the variables...
  cString filename(cur_string.PopWord());
  int min_batch = cur_string.PopWord().AsInt();
  int max_batch = cur_string.PopWord().AsInt();
  
  if (max_batch < min_batch) {
    cerr << "Error: min_batch=" << min_batch
    << ", max_batch=" << max_batch << "  (incorrect order?)" << endl;
    if (exit_on_error) exit(1);
  }
  
  // Construct a linked list of details needed...
  tList< tDataEntryBase<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryBase<cAnalyzeGenotype> > output_it(output_list);
  
  // For the moment, just put everything into the output list.
  SetupGenotypeDataList();
  
  // If no args were given, load all of the stats.
  if (cur_string.CountNumWords() == 0) {
    tListIterator< tDataEntryBase<cAnalyzeGenotype> >
    genotype_data_it(genotype_data_list);
    while (genotype_data_it.Next() != NULL) {
      output_list.PushRear(genotype_data_it.Get());
    }
  }
  // Otherwise, load only those listed.
  else {
    while (cur_string.GetSize() != 0) {
      // Setup the next entry
      cString cur_entry = cur_string.PopWord();
      bool found_entry = false;
      
      // Scan the genotype data list for the current entry
      tListIterator< tDataEntryBase<cAnalyzeGenotype> >
        genotype_data_it(genotype_data_list);
      
      while (genotype_data_it.Next() != NULL) {
        if (genotype_data_it.Get()->GetName() == cur_entry) {
          output_list.PushRear(genotype_data_it.Get());
          found_entry = true;
          break;
        }
      }
      
      // If the entry was not found, give a warning.
      if (found_entry == false) {
        int best_match = 1000;
        cString best_entry;
        
        genotype_data_it.Reset();
        while (genotype_data_it.Next() != NULL) {
          const cString & test_str = genotype_data_it.Get()->GetName();
          const int test_dist = cStringUtil::EditDistance(test_str, cur_entry);
          if (test_dist < best_match) {
            best_match = test_dist;
            best_entry = test_str;
          }
        }	
        
        cerr << "Warning: Format entry \"" << cur_entry
          << "\" not found.  Best match is \""
          << best_entry << "\"." << endl;
      }
      
    }
  }
  
  // Setup the file...
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // Determine the file type...
  int file_type = FILE_TYPE_TEXT;
  while (filename.Find('.') != -1) filename.Pop('.'); // Grab only extension
  if (filename == "html") file_type = FILE_TYPE_HTML;
  
  // Write out the header on the file
  if (file_type == FILE_TYPE_TEXT) {
    fp << "#filetype genotype_data" << endl;
    fp << "#format ";
    while (output_it.Next() != NULL) {
      const cString & entry_name = output_it.Get()->GetName();
      fp << entry_name << " ";
    }
    fp << endl << endl;
    
    // Give the more human-readable legend.
    fp << "# Legend:" << endl;
    fp << "# 1: Batch Name" << endl;
    int count = 1;
    while (output_it.Next() != NULL) {
      const cString & entry_desc = output_it.Get()->GetDesc();
      fp << "# " << ++count << ": " << entry_desc << endl;
    }
    fp << endl;
  } else { // if (file_type == FILE_TYPE_HTML) {
    fp << "<html>" << endl
    << "<body bgcolor=\"#FFFFFF\"" << endl
    << " text=\"#000000\"" << endl
    << " link=\"#0000AA\"" << endl
    << " alink=\"#0000FF\"" << endl
    << " vlink=\"#000044\">" << endl
    << endl
    << "<h1 align=center>Batch Index" << endl
    << endl
    << "<center>" << endl
    << "<table border=1 cellpadding=2><tr>" << endl;
    
    fp << "<th bgcolor=\"#AAAAFF\">Batch ";
    while (output_it.Next() != NULL) {
      const cString & entry_desc = output_it.Get()->GetDesc();
      fp << "<th bgcolor=\"#AAAAFF\">" << entry_desc << " ";
    }
    fp << "</tr>" << endl;
    
  }
  
  // Loop through all of the batchs...
  for (int batch_id = min_batch; batch_id <= max_batch; batch_id++) {
    cAnalyzeGenotype * genotype = batch[batch_id].List().GetFirst();
    if (genotype == NULL) continue;
    output_it.Reset();
    tDataEntryBase<cAnalyzeGenotype> * data_entry = NULL;
    const cString & batch_name = batch[batch_id].Name();
    if (file_type == FILE_TYPE_HTML) {
      fp << "<tr><th><a href=lineage." << batch_name << ".html>"
      << batch_name << "</a> ";
    } else {
      fp << batch_name << " ";
    }
    
    while ((data_entry = output_it.Next()) != NULL) {
      data_entry->SetTarget(genotype);
      if (file_type == FILE_TYPE_HTML) {
        fp << "<td align=center><a href=\""
        << data_entry->GetName() << "." << batch_name << ".png\">"
        << data_entry->Get(genotype) << "</a> ";
      } else {  // if (file_type == FILE_TYPE_TEXT) {
        fp << data_entry->Get(genotype) << " ";
      }
      }
    if (file_type == FILE_TYPE_HTML) fp << "</tr>";
    fp << endl;
    }
  
  // If in HTML mode, we need to end the file...
  if (file_type == FILE_TYPE_HTML) {
    fp << "</table>" << endl
    << "</center>" << endl;
  }
  }

void cAnalyze::CommandHistogram(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Histogram batch " << cur_batch << endl;
  else cout << "Histograming..." << endl;
  
  // Load in the variables...
  cString filename("histogram.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  // Construct a linked list of details needed...
  tList< tDataEntryCommand<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > output_it(output_list);
  LoadGenotypeDataList(cur_string, output_list);
  
  // Determine the file type...
  int file_type = FILE_TYPE_TEXT;
  cString file_extension(filename);
  while (file_extension.Find('.') != -1) file_extension.Pop('.');
  if (file_extension == "html") file_type = FILE_TYPE_HTML;
  
  // Setup the file...
  if (filename == "cout") {
    CommandHistogram_Header(cout, file_type, output_it);
    CommandHistogram_Body(cout, file_type, output_it);
  } else {
    ofstream& fp = m_world->GetDataFileOFStream(filename);
    CommandHistogram_Header(fp, file_type, output_it);
    CommandHistogram_Body(fp, file_type, output_it);
  }
  
  // And clean up...
  while (output_list.GetSize() != 0) delete output_list.Pop();
}

void cAnalyze::CommandHistogram_Header(ostream& fp, int format_type,
                                       tListIterator< tDataEntryCommand<cAnalyzeGenotype> > & output_it)
{
  // Write out the header on the file
  if (format_type == FILE_TYPE_TEXT) {
    fp << "#filetype histogram_data" << endl;
    fp << "#format ";
    while (output_it.Next() != NULL) {
      const cString & entry_name = output_it.Get()->GetName();
      fp << entry_name << " ";
    }
    fp << endl << endl;
    
    // Give the more human-readable legend.
    fp << "# Histograms:" << endl;
    int count = 0;
    while (output_it.Next() != NULL) {
      const cString & entry_desc = output_it.Get()->GetDesc();
      fp << "# " << ++count << ": " << entry_desc << endl;
    }
    fp << endl;
  } else { // if (format_type == FILE_TYPE_HTML) {
    fp << "<html>" << endl
    << "<body bgcolor=\"#FFFFFF\"" << endl
    << " text=\"#000000\"" << endl
    << " link=\"#0000AA\"" << endl
    << " alink=\"#0000FF\"" << endl
    << " vlink=\"#000044\">" << endl
    << endl
    << "<h1 align=center>Histograms for " << batch[cur_batch].Name()
    << "</h1>" << endl
    << endl
    << "<center>" << endl
    << "<table border=1 cellpadding=2><tr>" << endl;
    
    while (output_it.Next() != NULL) {
      const cString & entry_desc = output_it.Get()->GetDesc();
      const cString & entry_name = output_it.Get()->GetName();
      fp << "<tr><th bgcolor=\"#AAAAFF\"><a href=\"#"
        << entry_name << "\">"
        << entry_desc << "</a></tr>";
    }
    fp << "</tr></table>" << endl;    
  }
  }


void cAnalyze::CommandHistogram_Body(ostream& fp, int format_type,
                                     tListIterator< tDataEntryCommand<cAnalyzeGenotype> > & output_it)
{
  output_it.Reset();
  tDataEntryCommand<cAnalyzeGenotype> * data_command = NULL;
  
  while ((data_command = output_it.Next()) != NULL) {
    if (format_type == FILE_TYPE_TEXT) {
      fp << "# --- " << data_command->GetDesc() << " ---" << endl;
    } else {
      fp << "<table cellpadding=3>" << endl
      << "<tr><th colspan=3><a name=\"" << data_command->GetName() << "\">"
      << data_command->GetDesc() << "</th></tr>" << endl;
    }
    
    tDictionary<int> count_dict;
    
    // Loop through all genotypes in this batch to collect the info we need.
    tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
    cAnalyzeGenotype * cur_genotype;
    while ((cur_genotype = batch_it.Next()) != NULL) {
      data_command->SetTarget(cur_genotype);
      const cString cur_name(data_command->GetValue().AsString());
      int count = 0;
      count_dict.Find(cur_name, count);
      count += cur_genotype->GetNumCPUs();
      count_dict.SetValue(cur_name, count);
    }
    
    tList<cString> name_list;
    tList<int> count_list;
    count_dict.AsLists(name_list, count_list);
    
    // Figure out the maximum count and the maximum widths...
    int max_count = 0;
    int max_name_width = 0;
    int max_count_width = 0;
    tListIterator<int> count_it(count_list);
    tListIterator<cString> name_it(name_list);
    while (count_it.Next() != NULL) {
      const cString cur_name( *(name_it.Next()) );
      const int cur_count = *(count_it.Get());
      const int name_width = cur_name.GetSize();
      const int count_width = cStringUtil::Stringf("%d", cur_count).GetSize();
      if (cur_count > max_count) max_count = cur_count;
      if (name_width > max_name_width) max_name_width = name_width;
      if (count_width > max_count_width) max_count_width = count_width;
    }
    
    // Do some final calculations now that we know the maximums...
    const int max_stars = 75 - max_name_width - max_count_width;
    
    // Now print everything out...
    count_it.Reset();
    name_it.Reset();
    while (count_it.Next() != NULL) {
      const cString cur_name( *(name_it.Next()) );
      const int cur_count = *(count_it.Get());
      if (cur_count == 0) continue;
      int num_stars = (cur_count * max_stars) / max_count;
      
      if (format_type == FILE_TYPE_TEXT) {
        fp << setw(max_name_width) << cur_name << "  " 
        << setw(max_count_width) << cur_count << "  ";
        for (int i = 0; i < num_stars; i++) { fp << '#'; }
        fp << endl;
      } else { // FILE_TYPE_HTML
        fp << "<tr><td>" << cur_name
        << "<td>" << cur_count
        << "<td>";
        for (int i = 0; i < num_stars; i++) { fp << '#'; }
        fp << "</tr>" << endl;
      }
    }
    
    if (format_type == FILE_TYPE_TEXT) {
      // Skip a line between histograms...
      fp << endl;
    } else {
      fp << "</table><br><br>" << endl << endl;;
    }
  }
  
  // If in HTML mode, we need to end the file...
  if (format_type == FILE_TYPE_HTML) {
    fp << "</table>" << endl
    << "</center>" << endl;
  }
}


///// Population Analysis Commands ////

void cAnalyze::CommandPrintPhenotypes(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing phenotypes in batch "
    << cur_batch << endl;
  else cout << "Printing phenotypes..." << endl;
  
  // Load in the variables...
  cString filename("phenotype.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();

  cString flag("");
  bool print_ttc = false;
  bool print_ttpc = false;
  while (cur_string.GetSize() != 0) {
  	flag = cur_string.PopWord();
  	if (flag == "total_task_count") print_ttc = true;
  	else if (flag == "total_task_performance_count") print_ttpc = true;
  }

  // Make sure we have at least one genotype...
  if (batch[cur_batch].List().GetSize() == 0) return;
  
  // Setup the phenotype categories...
  const int num_tasks = batch[cur_batch].List().GetFirst()->GetNumTasks();
  const int num_phenotypes = 1 << (num_tasks + 1);
  tArray<int> phenotype_counts(num_phenotypes);
  tArray<int> genotype_counts(num_phenotypes);
  tArray<double> total_length(num_phenotypes);
  tArray<double> total_gest(num_phenotypes);
  tArray<int> total_task_count(num_phenotypes);
  tArray<int> total_task_performance_count(num_phenotypes);
  
  phenotype_counts.SetAll(0);
  genotype_counts.SetAll(0);
  total_length.SetAll(0.0);
  total_gest.SetAll(0.0);
  total_task_count.SetAll(0);
  total_task_performance_count.SetAll(0);
  
  // Loop through all of the genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    int phen_id = 0;
    if (genotype->GetViable() == true) phen_id++;
    for (int i = 0; i < num_tasks; i++) {
      if (genotype->GetTaskCount(i) > 0)  phen_id += 1 << (i+1);
    }
    phenotype_counts[phen_id] += genotype->GetNumCPUs();
    genotype_counts[phen_id]++;
    total_length[phen_id] += genotype->GetNumCPUs() * genotype->GetLength();
    total_gest[phen_id] += genotype->GetNumCPUs() * genotype->GetGestTime();
    for (int i = 0; i < num_tasks; i++) {
    		total_task_count[phen_id] += ((genotype->GetTaskCount(i) > 0) ? 1 : 0);
    		total_task_performance_count[phen_id] += genotype->GetTaskCount(i);
    }
  }
  
  // Print out the results...
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  fp << "# 1: Number of organisms of this phenotype" << endl
    << "# 2: Number of genotypes of this phenotye" << endl
    << "# 3: Average Genome Length" << endl
    << "# 4: Average Gestation Time" << endl
    << "# 5: Viability of Phenotype" << endl;
  if (print_ttc && print_ttpc) {
  	fp << "# 6: Total # of different tasks performed by this phenotype" << endl
    	<< "# 7: Average # of tasks performed by this phenotype" << endl
    	<< "# 8+: Tasks performed in this phenotype" << endl;
  }
  else if (print_ttc) {
  	fp << "# 6: Total # of different tasks performed by this phenotype" << endl
    	<< "# 7+: Tasks performed in this phenotype" << endl;
  }
  else if (print_ttpc) {
  	fp << "# 6: Total # of tasks performed by this phenotype" << endl
  	  << "# 7+: Tasks performed in this phenotype" << endl;
  }
  else { fp << "# 6+: Tasks performed in this phenotype" << endl; }
  fp << endl;
  
  // @CAO Lets do this inefficiently for the moment, but print out the
  // phenotypes in order.
  
  while (true) {
    // Find the next phenotype to print...
    int max_count = phenotype_counts[0];
    int max_position = 0;
    for (int i = 0; i < num_phenotypes; i++) {
      if (phenotype_counts[i] > max_count) {
        max_count = phenotype_counts[i];
        max_position = i;
      }
    }
    
    if (max_count == 0) break; // we're done!
    
    fp << phenotype_counts[max_position] << " "
      << genotype_counts[max_position] << " "
      << total_length[max_position] / phenotype_counts[max_position] << " "
      << total_gest[max_position] / phenotype_counts[max_position] << " "
      << (max_position & 1) << "  ";
    if (print_ttc) { fp << total_task_count[max_position] / genotype_counts[max_position] << "  "; }
    if (print_ttpc) { 
    	fp << total_task_performance_count[max_position] / genotype_counts[max_position] << "  "; 
    }
    for (int i = 1; i <= num_tasks; i++) {
      if ((max_position >> i) & 1 > 0) fp << "1 ";
      else fp << "0 ";
    }
    fp << endl;
    phenotype_counts[max_position] = 0;
  }
  
  m_world->GetDataFileManager().Remove(filename);
}


// Print various diversity metrics from the current batch of genotypes...
void cAnalyze::CommandPrintDiversity(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing diversity data for batch "
    << cur_batch << endl;
  else cout << "Printing diversity data..." << endl;
  
  // Load in the variables...
  cString filename("diversity.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  // Make sure we have at least one genotype...
  if (batch[cur_batch].List().GetSize() == 0) return;
  
  // Setup the task categories...
  const int num_tasks = batch[cur_batch].List().GetFirst()->GetNumTasks();
  tArray<int> task_count(num_tasks);
  tArray<int> task_gen_count(num_tasks);
  tArray<double> task_gen_dist(num_tasks);
  tArray<double> task_site_entropy(num_tasks);
  task_count.SetAll(0);
  task_gen_count.SetAll(0);
  
  // We must determine the average hamming distance between genotypes in
  // this batch that perform each task.  Levenstein distance would be ideal,
  // but takes a while, so we'll do it this way first.  For the calculations,
  // we need to know home many times each instruction appears at each
  // position for each genotype collection that performs a particular task.
  const int num_insts = inst_set.GetSize();
  const int max_length = BatchUtil_GetMaxLength();
  tMatrix<int> inst_freq(max_length, num_insts+1);
  
  for (int task_id = 0; task_id < num_tasks; task_id++) {
    inst_freq.SetAll(0);
    
    // Loop through all genotypes, singling out those that do current task...
    tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
    cAnalyzeGenotype * genotype = NULL;
    while ((genotype = batch_it.Next()) != NULL) {
      if (genotype->GetTaskCount(task_id) == 0) continue;
      
      const cGenome & genome = genotype->GetGenome();
      const int num_cpus = genotype->GetNumCPUs();
      task_count[task_id] += num_cpus;
      task_gen_count[task_id]++;
      for (int i = 0; i < genotype->GetLength(); i++) {
        inst_freq( i, genome.GetOp(i) ) += num_cpus;
      }
      for (int i = genotype->GetLength(); i < max_length; i++) {
        inst_freq(i, num_insts) += num_cpus; // Entry for "past genome end"
      }
    }
    
    // Analyze the data for this entry...
    const int cur_count = task_count[task_id];
    const int total_pairs = cur_count * (cur_count - 1) / 2;
    int total_dist = 0;
    double total_ent = 0;
    for (int pos = 0; pos < max_length; pos++) {
      // Calculate distance component...
      for (int inst1 = 0; inst1 < num_insts; inst1++) {
        if (inst_freq(pos, inst1) == 0) continue;
        for (int inst2 = inst1+1; inst2 <= num_insts; inst2++) {
          total_dist += inst_freq(pos, inst1) * inst_freq(pos, inst2);
        }
      }
      
      // Calculate entropy component...
      for (int i = 0; i <= num_insts; i++) {
        const int cur_freq = inst_freq(pos, i);
        if (cur_freq == 0) continue;
        const double p = ((double) cur_freq) / (double) cur_count;
        total_ent -= p * log(p);
      }
    }
    
    task_gen_dist[task_id] = ((double) total_dist) / (double) total_pairs;
    task_site_entropy[task_id] = total_ent;
  }
  
  // Print out the results...
  cDataFile & df = m_world->GetDataFile(filename);
  
  for (int i = 0; i < num_tasks; i++) {
    df.Write(i,                    "# 1: Task ID");
    df.Write(task_count[i],        "# 2: Number of organisms performing task");
    df.Write(task_gen_count[i],    "# 3: Number of genotypes performing task");
    df.Write(task_gen_dist[i],     "# 4: Average distance between genotypes performing task");
    df.Write(task_site_entropy[i], "# 5: Total per-site entropy of genotypes performing task");
    df.Endl();
  }
}


void cAnalyze::PhyloCommunityComplexity(cString cur_string)
{
  
  /////////////////////////////////////////////////////////////////////////
  // Calculate the mutual information between all genotypes and environment
  /////////////////////////////////////////////////////////////////////////
  
  cout << "Analyze biocomplexity of current population about environment ...\n";
  
  // Get the number of genotypes that are gonna be analyzed.
  int max_genotypes = cur_string.PopWord().AsInt();
  
  // Get update
  int update = cur_string.PopWord().AsInt();
  
  // Get the directory  
  cString directory = PopDirectory(cur_string, "community_cpx/");
  
  // Get the file name that saves the result 
  cString filename = cur_string.PopWord();
  if (filename.IsEmpty()) {
    filename = "community.complexity.dat";
  }
  
  filename.Set("%s%s", static_cast<const char*>(directory), static_cast<const char*>(filename));
  ofstream& cpx_fp = m_world->GetDataFileOFStream(filename);
  
  cpx_fp << "# Legend:" << endl;
  cpx_fp << "# 1: Genotype ID" << endl;
  cpx_fp << "# 2: Entropy given Known Genotypes" << endl;
  cpx_fp << "# 3: Entropy given Both Known Genotypes and Env" << endl;
  cpx_fp << "# 4: New Information about Environment" << endl;
  cpx_fp << "# 5: Total Complexity" << endl;
  cpx_fp << endl;
  
  /////////////////////////////////////////////////////////////////////////////////
  // Loop through all genotypes in all batches and build id vs. genotype map
  
  map<int, cAnalyzeGenotype *> genotype_database;
  for (int i = 0; i < MAX_BATCHES; ++ i) {
    tListIterator<cAnalyzeGenotype> batch_it(batch[i].List());
    cAnalyzeGenotype * genotype = NULL;
    while ((genotype = batch_it.Next()) != NULL) {
      genotype_database.insert(make_pair(genotype->GetID(), genotype));
    }
  }
  
  ////////////////////////////////////////////////
  // Check if all the genotypes having same length
  
  int length_genome = 0;
  if (genotype_database.size() > 0) {
    length_genome = genotype_database.begin()->second->GetLength();
  }
  map<int, cAnalyzeGenotype*>::iterator gen_iterator = genotype_database.begin();
  for (; gen_iterator != genotype_database.end(); ++ gen_iterator) {
    if (gen_iterator->second->GetLength() != length_genome) {
      cerr << "Genotype " << gen_iterator->first << " has different genome length." << endl;
      if (exit_on_error) exit(1);
    }
  }
  
  ///////////////////////
  // Create Test Info 
  // No choice of use_resources for this analyze command...
  cCPUTestInfo test_info;
  test_info.SetResourceOptions(RES_CONSTANT, &resources, update, m_resource_time_spent_offset);
  
  
  ///////////////////////////////////////////////////////////////////////
  // Choose the first n most abundant genotypes and put them in community
  
  vector<cAnalyzeGenotype *> community;
  cAnalyzeGenotype * genotype = NULL;
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  
  while (((genotype = batch_it.Next()) != NULL) && (community.size() < static_cast<unsigned int>(max_genotypes))) {
    community.push_back(genotype);
  }
  
  ///////////////////////////
  // Measure hamming distance
  
  int size_community = community.size();
  if (size_community == 0) {
    cerr << "There is no genotype in this community." << endl;
    if (exit_on_error) exit(1);
  }
  typedef pair<int,int> gen_pair;
  map<gen_pair, int> hamming_dist;
  
  for (int i = 0; i< size_community; ++ i) {
    for (int j = i+1; j < size_community; ++ j) {
      int dist = cGenomeUtil::FindHammingDistance(community[i]->GetGenome(),
                                                  community[j]->GetGenome());
      int id1 = community[i]->GetID();
      int id2 = community[j]->GetID();
      
      hamming_dist.insert(make_pair(gen_pair(id1, id2), dist));
      hamming_dist.insert(make_pair(gen_pair(id2, id1), dist));
    }
  }
  
  //////////////////////////////////
  // Get Most Recent Common Ancestor
  
  map<gen_pair, cAnalyzeGenotype *> mrca;
  map<gen_pair, int> raw_dist;
  for (int i = 0; i< size_community; ++ i) {
    for (int j = i+1; j < size_community; ++ j) {
      
      cAnalyzeGenotype * lineage1_genotype = community[i];
      cAnalyzeGenotype * lineage2_genotype = community[j];
      int total_dist = 0;
      
      while (lineage1_genotype->GetID() != lineage2_genotype->GetID()) {
        if (lineage1_genotype->GetID() > lineage2_genotype->GetID()) {
          int parent_id = lineage1_genotype->GetParentID();
          cAnalyzeGenotype * parent = genotype_database.find(parent_id)->second;
          
          total_dist += cGenomeUtil::FindHammingDistance(lineage1_genotype->GetGenome(),
                                                         parent->GetGenome());
          lineage1_genotype = parent;
        } else {
          int parent_id = lineage2_genotype->GetParentID();
          cAnalyzeGenotype * parent = genotype_database.find(parent_id)->second;
          total_dist += cGenomeUtil::FindHammingDistance(lineage2_genotype->GetGenome(),
                                                         parent->GetGenome());
          
          lineage2_genotype = parent;
        }
      }
      
      int id1 = community[i]->GetID();
      int id2 = community[j]->GetID();
      mrca.insert(make_pair(gen_pair(id1, id2), lineage1_genotype));
      mrca.insert(make_pair(gen_pair(id2, id1), lineage1_genotype));
      raw_dist.insert(make_pair(gen_pair(id1, id2), total_dist));
      raw_dist.insert(make_pair(gen_pair(id2, id1), total_dist));
    }
  }
  
  /*
   ////////////////////////////////////////////////////////////////////////////////////////////
   // Sort the genotype that is next genotype is the most closest one to all previous genotypes
   
   vector<cAnalyzeGenotype *> sorted_community;
   vector<cAnalyzeGenotype *> left_genotypes = community;
   
   // Put the first genotype in left to sorted.
   sorted_community.push_back(*left_genotypes.begin());
   left_genotypes.erase(left_genotypes.begin());
   
   while (left_genotypes.size() > 0) {
     int min_total_hamming = size_community * length_genome;
     int index_next;
     
     for (int next = 0; next < left_genotypes.size(); ++ next) {
       int total_hamming = 0;
       int id1 = left_genotypes[next]->GetID();
       
       for (int given = 0; given < sorted_community.size(); ++ given) {
         int id2 = sorted_community[given]->GetID();
         total_hamming += hamming_dist.find(gen_pair(id1, id2))->second;
       }
       
       if (total_hamming < min_total_hamming) {
         min_total_hamming = total_hamming;
         index_next = next;
       }
     }
     
     sorted_community.push_back(left_genotypes[index_next]);
     left_genotypes.erase(left_genotypes.begin() + index_next);
   }
   
   */
  
  vector<cAnalyzeGenotype *> sorted_community = community;
  /////////////////////////////////////////////
  // Loop through genotypes in sorted community
  
  double complexity = 0.0;
  vector<cAnalyzeGenotype *> given_genotypes;
  int num_insts = inst_set.GetSize();
  
  for (int i = 0; i < size_community; ++ i) {
    genotype = sorted_community[i];
    
    // Skip the dead organisms
    genotype->Recalculate(m_ctx, &test_info);
    if (genotype->GetFitness() == 0) {
      continue;
    }
    
    vector<double> one_line_prob(num_insts, 0.0);
    vector< vector<double> > prob(length_genome, one_line_prob);
    
    cout << endl << genotype->GetID() << endl;
    
    /*if (given_genotypes.size() >= 2) {
      
      ///////////////////////////////////////////////////////////////////
      // Look for two given genotypes that has minimun depth dist with it
      
      cAnalyzeGenotype * min_depth_gen = given_genotypes[0];
    cAnalyzeGenotype * tmrca = mrca.find(gen_pair(genotype->GetID(), 
                                                  given_genotypes[0]->GetID()))->second;
    int min_depth_dist = genotype->GetDepth() + given_genotypes[0]->GetDepth() - 2 * tmrca->GetDepth();
    
    cAnalyzeGenotype * second_min_gen = given_genotypes[1];
    tmrca = mrca.find(gen_pair(genotype->GetID(), given_genotypes[1]->GetID()))->second;
    int second_min_depth = genotype->GetDepth() + given_genotypes[1]->GetDepth() - 2 * tmrca->GetDepth();
    
    for (int i = 2; i < given_genotypes.size(); ++ i) {
      cAnalyzeGenotype * given_genotype = given_genotypes[i];
      cAnalyzeGenotype * tmrca = mrca.find(gen_pair(genotype->GetID(),
                                                    given_genotype->GetID()))->second;
      int dist = genotype->GetDepth() + given_genotype->GetDepth() - 2 * tmrca->GetDepth();
      
      if (dist < min_depth_dist) {
        second_min_depth = min_depth_dist;
        second_min_gen = min_depth_gen;
        min_depth_dist = dist;
        min_depth_gen = given_genotype;
      } else if (dist >= min_depth_dist && dist < second_min_depth) {
        second_min_depth = dist;
        second_min_gen = given_genotype;
      }
    }
    
    const cGenome & given_genome1 = min_depth_gen->GetGenome();
    const cGenome & given_genome2 = second_min_gen->GetGenome();
    for (int line = 0; line < length_genome; ++ line) {
      int given_inst = given_genome1[line].GetOp();
      prob[line][given_inst] += pow(1 - 1.0/length_genome, min_depth_dist);
      given_inst = given_genome2[line].GetOp();
      prob[line][given_inst] += pow(1 - 1.0/length_genome, min_depth_dist);
    }
    
    cpx_fp << genotype->GetID() << " " << min_depth_dist << " " << second_min_depth 
	     << " " << raw_dist.find(gen_pair(genotype->GetID(), min_depth_gen->GetID()))->second
	     << " " << raw_dist.find(gen_pair(genotype->GetID(), second_min_gen->GetID()))->second
	     << " ";
    
    
    } else  if (given_genotypes.size() == 1) {
      //////////////////////////////////////////////////////
      // Calculate the probability of each inst at each line
      cAnalyzeGenotype * tmrca = mrca.find(gen_pair(genotype->GetID(), 
                                                    given_genotypes[0]->GetID()))->second;
      int dist = genotype->GetDepth() + given_genotypes[0]->GetDepth() - 2 * tmrca->GetDepth();
      const cGenome & given_genome = given_genotypes[0]->GetGenome();
      
      for (int line = 0; line < length_genome; ++ line) {
        int given_inst = given_genome[line].GetOp();
        prob[line][given_inst] += pow(1 - 1.0/length_genome, dist);
      }
      
      cpx_fp << genotype->GetID() << " " << dist << " " 
        << raw_dist.find(gen_pair(genotype->GetID(), given_genotypes[0]->GetID()))->second << " ";
    } else {
      cpx_fp << genotype->GetID() << " ";
    }*/
    
    if (given_genotypes.size() >= 1) {
      //////////////////////////////////////////////////
      // Look for a genotype that is closest to this one
      
      cAnalyzeGenotype * min_depth_gen = given_genotypes[0];
      cAnalyzeGenotype * tmrca = mrca.find(gen_pair(genotype->GetID(), 
                                                    given_genotypes[0]->GetID()))->second;
      int min_depth_dist = genotype->GetDepth() + given_genotypes[0]->GetDepth() - 2 * tmrca->GetDepth();
      
      for (unsigned int i = 1; i < given_genotypes.size() ; ++ i) {
        cAnalyzeGenotype * given_genotype = given_genotypes[i];
        cAnalyzeGenotype * tmrca = mrca.find(gen_pair(genotype->GetID(),
                                                      given_genotype->GetID()))->second;
        int dist = genotype->GetDepth() + given_genotype->GetDepth() - 2 * tmrca->GetDepth();
        
        if (dist < min_depth_dist) {
          min_depth_dist = dist;
          min_depth_gen = given_genotype;
        }
      }
      
      const cGenome & given_genome = min_depth_gen->GetGenome();
      const cGenome & base_genome = genotype->GetGenome();
      cGenome mod_genome(base_genome);
      for (int line = 0; line < length_genome; ++ line) {
        int given_inst = given_genome.GetOp(line);
        mod_genome = base_genome;
        mod_genome.SetOp(line, given_inst);
        cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
        test_genotype.Recalculate(m_ctx, &test_info);
        
        // Only when given inst make the genotype alive
        if (test_genotype.GetFitness() > 0) {
          prob[line][given_inst] += pow(1 - 1.0/length_genome, min_depth_dist);
        }
      }
      
      cpx_fp << genotype->GetID() << " " << min_depth_dist << " " 
        << raw_dist.find(gen_pair(genotype->GetID(), min_depth_gen->GetID()))->second << " "
        << hamming_dist.find(gen_pair(genotype->GetID(), min_depth_gen->GetID()))->second << "   ";
    } else {
      cpx_fp << genotype->GetID() << " ";
    }
    
    ///////////////////////////////////////////////////////////////////
    // Point mutation at all lines of code to look for neutral mutation
    // and the mutations that can make organism alive
    cout << "Test point mutation." << endl;
    vector<bool> one_line_neutral(num_insts, false);
    vector< vector<bool> > neutral_mut(length_genome, one_line_neutral);
    vector< vector<bool> > alive_mut(length_genome, one_line_neutral);
    
    genotype->Recalculate(m_ctx, &test_info);
    double base_fitness = genotype->GetFitness();
    cout << base_fitness << endl;
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);
    
    for (int line = 0; line < length_genome; ++ line) {
      int cur_inst = base_genome.GetOp(line);
      
      for (int mod_inst = 0; mod_inst < num_insts; ++ mod_inst) {
        mod_genome.SetOp(line, mod_inst);
        cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
        test_genotype.Recalculate(m_ctx, &test_info);
        if (test_genotype.GetFitness() >= base_fitness) {
          neutral_mut[line][mod_inst] = true;
        } 
        if (test_genotype.GetFitness() > 0) {
          alive_mut[line][mod_inst] = true;
        }
      }
      
      mod_genome.SetOp(line, cur_inst);
    }
    
    /////////////////////////////////////////
    // Normalize the probability at each line
    vector< vector<double> > prob_before_env(length_genome, one_line_prob);
    
    for (int line = 0; line < length_genome; ++ line) {
      double cur_total_prob = 0.0;
      int num_alive = 0;
      for (int inst = 0; inst < num_insts; ++ inst) {
        if (alive_mut[line][inst] == true) {
          cur_total_prob += prob[line][inst];
          num_alive ++;
        }
      }
      if (cur_total_prob > 1) {
        cout << "Total probability at " << line << " is greater than 0." << endl;
        if (exit_on_error) exit(1);
      }
      double left_prob = 1 - cur_total_prob;
      
      for (int inst = 0; inst < num_insts; ++ inst) {
        if (alive_mut[line][inst] == true) {
          prob_before_env[line][inst] = prob[line][inst] + left_prob / num_alive;
        } else {
          prob_before_env[line][inst] = 0;
        }	
      }
      
    }
    
    /////////////////////////////////
    // Calculate entropy of each line  
    vector<double> entropy(length_genome, 0.0);
    for (int line = 0; line < length_genome; ++ line) {
      double sum = 0;
      for (int inst = 0; inst < num_insts; ++ inst) {
        sum += prob_before_env[line][inst];
        if (prob_before_env[line][inst] > 0) {
          entropy[line] -= prob_before_env[line][inst] * log(prob_before_env[line][inst]) / log(num_insts*1.0);
        }
      }
      if (sum > 1.001 || sum < 0.999) {
        cout << "Sum of probability is not 1 at line " << line << endl;
        if (exit_on_error) exit(1);
      }
    }
    
    
    /////////////////////////////////////////////////////
    // Redistribute the probability of insts at each line
    vector< vector<double> > prob_given_env(length_genome, one_line_prob);
    
    for (int line = 0; line < length_genome; ++ line) {
      double total_prob = 0.0;
      int num_neutral = 0;
      for (int inst = 0; inst < num_insts; ++ inst) {
        if (neutral_mut[line][inst] == true) {
          num_neutral ++;
          total_prob += prob[line][inst];
        }
      }
      
      double left = 1 - total_prob;
      
      for (int inst = 0; inst < num_insts; ++ inst) {
        if (neutral_mut[line][inst] == true) {
          prob_given_env[line][inst] = prob[line][inst] + left / num_neutral;
        } else {
          prob_given_env[line][inst] = 0.0;
        }
      }
      
    }
    
    ////////////////////////////////////////////////
    // Calculate the entropy given environment
    
    vector<double> entropy_given_env(length_genome, 0.0);
    for (int line = 0; line < length_genome; ++ line) {
      double sum = 0;
      for (int inst = 0; inst < num_insts; ++ inst) {
        sum += prob_given_env[line][inst];
        if (prob_given_env[line][inst] > 0) {
          entropy_given_env[line] -= prob_given_env[line][inst] * log(prob_given_env[line][inst]) / 
          log(num_insts*1.0);
        }
      }
      if (sum > 1.001 || sum < 0.999) {
        cout << "Sum of probability is not 1 at line " << line << " " << sum << endl;
        if (exit_on_error) exit(1);
      }
    }
    
    
    ///////////////////////////////////////////////////////////////////////////
    // Calculate the information between genotype and env given other genotypes
    double information = 0.0;
    double entropy_before = 0.0;
    double entropy_after = 0.0;
    for (int line = 0; line < length_genome; ++ line) {
      entropy_before += entropy[line];
      entropy_after += entropy_given_env[line];
      
      if (entropy[line] >= entropy_given_env[line]) {
        information += entropy[line] - entropy_given_env[line];	
      } else {    // Negative information is because given condition is not related with this genotype  ...
        
        // Count the number of insts that can make genotype alive
        int num_inst_alive = 0;
        for (int inst = 0; inst < num_insts; ++ inst) {
          if (alive_mut[line][inst] == true) {
            num_inst_alive ++;
          }
        }
        
        double entropy_before = - log(1.0/num_inst_alive) / log(num_insts*1.0);
        information += entropy_before - entropy_given_env[line];
        if (information < 0) {
          cout << "Negative information at site " << line << endl;
          if (exit_on_error) exit(1);
        }
      }
      
    }
    complexity += information;
    
    cpx_fp << entropy_before << " " << entropy_after << " "
      << information << " " << complexity << "   ";
    
    genotype->PrintTasks(cpx_fp, 0, -1);
    cpx_fp << endl; 
    
    /////////////////////////////////////////////////////////////
    // This genotype becomes the given condition of next genotype
    
    given_genotypes.push_back(genotype);
    
  }
    
  m_world->GetDataFileManager().Remove(filename);
  return;
}


// Calculate various stats for trees in population.
void cAnalyze::CommandPrintTreeStats(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing tree stats for batch "
    << cur_batch << endl;
  else cout << "Printing tree stats..." << endl;
  
  // Load in the variables...
  cString filename("tree_stats.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();

  ofstream& fp = m_world->GetDataFileOFStream(filename);

  fp << "# Legend:" << endl;
  fp << "# 1: Average cumulative stemminess" << endl;
  fp << endl;
  
  cAnalyzeTreeStats_CumulativeStemminess agts(m_world);
  agts.AnalyzeBatchTree(batch[cur_batch].List());

  fp << agts.AverageStemminess();
  fp << endl;
}


// Calculate cumulative stemmines for trees in population.
void cAnalyze::CommandPrintCumulativeStemminess(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing cumulative stemmines for batch "
    << cur_batch << endl;
  else cout << "Printing cumulative stemmines..." << endl;
  
  // Load in the variables...
  cString filename("cumulative_stemminess.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  fp << "# Legend:" << endl;
  fp << "# 1: Average cumulative stemminess" << endl;
  fp << endl;
  
  cAnalyzeTreeStats_CumulativeStemminess agts(m_world);
  agts.AnalyzeBatchTree(batch[cur_batch].List());
  
  fp << agts.AverageStemminess();
  fp << endl;
}



// Calculate Pybus-Harvey gamma statistic for trees in population.
void cAnalyze::CommandPrintGamma(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Printing Pybus-Harvey gamma statistic for batch "
    << cur_batch << endl;
  else cout << "Printing Pybus-Harvey gamma statistic..." << endl;
  
  // Load in the variables...
  int end_time = (cur_string.GetSize()) ? cur_string.PopWord().AsInt() : -1;         // #1
  if (end_time < 0) {
    cout << "Error: end_time (argument 1) must be specified as nonzero." << endl;
    return;
  }
  
  cString filename("gamma.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();

  cString lineage_thru_time_fname("");
  if (cur_string.GetSize() != 0) lineage_thru_time_fname = cur_string.PopWord();

  /*
  I've hardwired the option 'furcation_time_convention' to '1'.

  'furcation_time_convention' refers to the time at which a 'speciation' event
  occurs (I'm not sure 'speciation' is the right word for this). If a parent
  genotype produces two distinct surviving lineages, then the time of
  speciation could be:
  - 1: The parent genotype's birth time
  - 2: The elder child genotype's birth time
  - 3: The younger child genotype's birth time

  @kgn
  */
  // int furcation_time_convention = (cur_string.GetSize()) ? cur_string.PopWord().AsInt() : 1;
  int furcation_time_convention = 1;
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  fp << "# Legend:" << endl;
  fp << "# 1: Pybus-Harvey gamma statistic" << endl;
  fp << endl;
  
  cAnalyzeTreeStats_Gamma atsg(m_world);
  atsg.AnalyzeBatch(batch[cur_batch].List(), end_time, furcation_time_convention);
  
  fp << atsg.Gamma();
  fp << endl;

  if(lineage_thru_time_fname != ""){
    ofstream& ltt_fp = m_world->GetDataFileOFStream(lineage_thru_time_fname);

    ltt_fp << "# Legend:" << endl;
    ltt_fp << "# 1: num_lineages" << endl;
    ltt_fp << "# 2: furcation_time" << endl;
    ltt_fp << endl;

    int size = atsg.FurcationTimes().GetSize();
    for(int i = 0; i < size; i++){
      ltt_fp << i+2 << " " << atsg.FurcationTimes()[i] <<  endl;
    }
  }
}


void cAnalyze::AnalyzeCommunityComplexity(cString cur_string)
{
  /////////////////////////////////////////////////////////////////////
  // Calculate the mutual information between community and environment
  /////////////////////////////////////////////////////////////////////
  
  cout << "Analyze community complexity of current population about environment with Charles method ...\n";
  
  // Get the number of genotypes that are gonna be analyzed.
  int max_genotypes = cur_string.PopWord().AsInt(); // If it is 0, we sample 
                                                    //two genotypes for each task.
  
  // Get update
  int update = cur_string.PopWord().AsInt();
  
  // Get the directory  
  cString dir = cur_string.PopWord();
  cString defaultDir = "community_cpx/";
  cString directory = PopDirectory(dir, defaultDir);
  
  // Get the file name that saves the result 
  cString filename = cur_string.PopWord();
  if (filename.IsEmpty()) {
    filename = "community.complexity.dat";
  }
  
  filename.Set("%s%s", static_cast<const char*>(directory), static_cast<const char*>(filename));
  ofstream& cpx_fp = m_world->GetDataFileOFStream(filename);
  
  cpx_fp << "# Legend:" << endl;
  cpx_fp << "# 1: Genotype ID" << endl;
  cpx_fp << "# 2: Entropy given Known Genotypes" << endl;
  cpx_fp << "# 3: Entropy given Both Known Genotypes and Env" << endl;
  cpx_fp << "# 4: New Information about Environment" << endl;
  cpx_fp << "# 5: Total Complexity" << endl;
  cpx_fp << "# 6: Hamming Distance to Closest Given Genotype" << endl;
  cpx_fp << "# 7: Total Hamming Distance to Closest Neighbor" << endl;
  cpx_fp << "# 8: Number of Organisms" << endl;
  cpx_fp << "# 9: Total Number of Organisms" << endl;
  cpx_fp << "# 10 - : Tasks Implemented" << endl;
  cpx_fp << endl;
  
  ///////////////////////
  // Backup test CPU data
  cCPUTestInfo test_info;
  // No choice of use_resources for this analyze command...
  test_info.SetResourceOptions(RES_CONSTANT, &resources, update, m_resource_time_spent_offset);
  
  vector<cAnalyzeGenotype *> community;
  cAnalyzeGenotype * genotype = NULL;
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  
  
  if (max_genotypes > 0) {
    
    ///////////////////////////////////////////////////////////////////////
    // Choose the first n most abundant genotypes and put them in community
    
    while (((genotype = batch_it.Next()) != NULL) && (community.size() < static_cast<unsigned int>(max_genotypes))) {
      community.push_back(genotype);
    }
  } else if (max_genotypes == 0) {
    
    /////////////////////////////////////
    // Choose two genotypes for each task
    
    genotype = batch_it.Next();
    if (genotype == NULL) {
      m_world->GetDataFileManager().Remove(filename);
      return;
    }
    genotype->Recalculate(m_ctx, &test_info);
    int num_tasks = genotype->GetNumTasks();
    vector< vector<cAnalyzeGenotype *> > genotype_class(num_tasks);
    do {
      for (int task_id = 0; task_id < num_tasks; ++ task_id) {
        int count = genotype->GetTaskCount(task_id);
        if (count > 0) {
          genotype_class[task_id].push_back(genotype);
        }
      }
    } while ((genotype = batch_it.Next()) != NULL);
    
    cRandom random;
    for (int task_id = 0; task_id < num_tasks; ++ task_id) {
      int num_genotype = genotype_class[task_id].size();
      if (num_genotype > 0) {
        int index = random.GetUInt(num_genotype);
        community.push_back(genotype_class[task_id][index]);
        index = random.GetUInt(num_genotype);
        community.push_back(genotype_class[task_id][index]);
      } else {
        // Pick up a class that is not empty
        int class_id = random.GetUInt(num_tasks);
        while (genotype_class[class_id].size() == 0) {
          class_id ++;
          if (class_id >= num_tasks) {
            class_id = 0;
          }
        }
        int num_genotype = genotype_class[class_id].size();
        int index = random.GetUInt(num_genotype);
        community.push_back(genotype_class[task_id][index]);
        index = random.GetUInt(num_genotype);
        community.push_back(genotype_class[task_id][index]);
      }
    }
    
  }
  
  ////////////////////////////////////////////////////
  // Test point mutation of each genotype in community
  
  int num_insts = inst_set.GetSize();
  map<int, tMatrix<double> > point_mut; 
  int size_community = community.size();
  int length_genome = 0;
  if (size_community > 1) {
    length_genome = community[0]->GetLength();
  }
  
  for (int i = 0; i < size_community; ++ i) {
    genotype = community[i];
    
    ///////////////////////////////////////////////////////////////////
    // Point mutation at all lines of code to look for neutral mutation
    cout << "Test point mutation for genotype " << genotype->GetID() << endl;
    tMatrix<double> prob(length_genome, num_insts);
    
    genotype->Recalculate(m_ctx, &test_info);
    double base_fitness = genotype->GetFitness();
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);
    
    for (int line = 0; line < length_genome; ++ line) {
      int cur_inst = base_genome.GetOp(line);
      int num_neutral = 0;
      
      for (int mod_inst = 0; mod_inst < num_insts; ++ mod_inst) {
        mod_genome.SetOp(line, mod_inst);
        cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
        test_genotype.Recalculate(m_ctx, &test_info);
        if (test_genotype.GetFitness() >= base_fitness) {
          prob[line][mod_inst] = 1.0;
          num_neutral ++;
        } else {
          prob[line][mod_inst] = 0.0;
        }
      }
      
      for (int mod_inst = 0; mod_inst < num_insts; ++ mod_inst) {
        prob[line][mod_inst] /= num_neutral;
      }
      
      
      mod_genome.SetOp(line, cur_inst);
    }
    
    point_mut.insert(make_pair(genotype->GetID(), prob));
  }
  
  //////////////////////////////////////
  // Loop through genotypes in community
  
  double complexity = 0.0;
  int total_dist = 0;
  int total_cpus = 0;
  vector<cAnalyzeGenotype *> given_genotypes;
  
  ////////////////////////////////////////
  // New information in the first gentoype
  genotype = community[0];
  double oo_initial_entropy = length_genome;
  double oo_conditional_entropy = 0.0;
  tMatrix<double> this_prob = point_mut.find(genotype->GetID())->second;
  
  for (int line = 0; line < length_genome; ++ line) {
    double oneline_entropy = 0.0;
    for (int inst = 0; inst < num_insts; ++ inst) {
      if (this_prob[line][inst] > 0) {
        oneline_entropy -= this_prob[line][inst] * (log(this_prob[line][inst]) /
                                                    log(1.0*num_insts));
      }
    }
    oo_conditional_entropy += oneline_entropy;
  }
  
  double new_info = oo_initial_entropy - oo_conditional_entropy;
  complexity += new_info;
  given_genotypes.push_back(genotype);
  
  cpx_fp << genotype->GetID() << " " 
    << oo_initial_entropy << " " 
    << oo_conditional_entropy << " "
    << new_info << " " 
    << complexity << "   "
    << "0 0" << "   ";
  int num_cpus = genotype->GetNumCPUs();
  total_cpus += num_cpus;
  cpx_fp << num_cpus << " " << total_cpus << "   ";
  genotype->Recalculate(m_ctx, &test_info);
  genotype->PrintTasks(cpx_fp, 0, -1);
  cpx_fp << endl;
  
  
  //////////////////////////////////////////////////////
  // New information in other genotypes in community ...
  for (int i = 1; i < size_community; ++ i) {
    genotype = community[i];
    if (genotype->GetLength() != length_genome) {
      cerr << "Genotypes in the community do not same genome length.\n";
      if (exit_on_error) exit(1);
    }
    
    // Skip the dead organisms
    genotype->Recalculate(m_ctx, &test_info);
    cout << genotype->GetID() << " " << genotype->GetFitness() << endl;
    if (genotype->GetFitness() == 0) {
      continue;
    }
    
    double min_new_info = length_genome; 
    double oo_initial_entropy = 0.0;
    double oo_conditional_entropy = 0.0;
    cAnalyzeGenotype* used_genotype = NULL;
    tMatrix<double> this_prob = point_mut.find(genotype->GetID())->second;
    
    // For any given genotype, calculate the new information in genotype
    for (unsigned int j = 0; j < given_genotypes.size(); ++ j) {
      
      tMatrix<double> given_prob = point_mut.find(given_genotypes[j]->GetID())->second;
      double new_info = 0.0;
      double total_initial_entropy = 0.0;
      double total_conditional_entropy = 0.0;
      
      for (int line = 0; line < length_genome; ++ line) {
        
        // H(genotype|known_genotype)    
        double prob_overlap = 0;
        for (int inst = 0; inst < num_insts; ++ inst) {
          if (this_prob[line][inst] < given_prob[line][inst]) {
            prob_overlap += this_prob[line][inst];
          } else {
            prob_overlap += given_prob[line][inst];
          }
        }
        
        double given_site_entropy = 0.0;
        for (int inst = 0; inst < num_insts; ++ inst) {
          if (given_prob[line][inst] > 0) {
            given_site_entropy -= given_prob[line][inst] * (log(given_prob[line][inst]) /
                                                            log(1.0*num_insts));
          }
        }
        
        
        double entropy_overlap = 0.0;
        if (prob_overlap > 0 &&  (1 - prob_overlap > 0)) {
          entropy_overlap = (- prob_overlap * log(prob_overlap) 
                             - (1-prob_overlap) * log(1 - prob_overlap)) / log(1.0*num_insts);
        } else {
          entropy_overlap = 0; 
        }
        
        double initial_entropy = prob_overlap * given_site_entropy 
          + (1 - prob_overlap) * 1 + entropy_overlap;
        total_initial_entropy += initial_entropy;
        
        // H(genotype|E, known_genotype) = H(genotype|Env)
        double conditional_entropy = 0.0;
        for (int inst = 0; inst < num_insts; ++ inst) {
          if (this_prob[line][inst] > 0) {
            conditional_entropy -= this_prob[line][inst] * (log(this_prob[line][inst]) / 
                                                            log(1.0*num_insts));
          }
        }
        total_conditional_entropy += conditional_entropy;
        
        if (conditional_entropy > initial_entropy + 0.00001) {
          cerr << "Negative Information.\n";
          cout << line << endl;
          for (int inst = 0; inst < num_insts; ++ inst) {
            cout << this_prob[line][inst] << " ";
          }
          cout << endl;
          for (int inst = 0; inst < num_insts; ++ inst) {
            cout << given_prob[line][inst] << " ";
          }
          cout << endl;
          
          if (exit_on_error) exit(1);
        }
        
        new_info += initial_entropy - conditional_entropy;
      }
      
      if (new_info < min_new_info) {
        min_new_info = new_info;
        oo_initial_entropy = total_initial_entropy;
        oo_conditional_entropy = total_conditional_entropy;
        used_genotype = given_genotypes[j];
        cout << "        " << "New closest genotype " << used_genotype->GetID() 
          << " " << new_info << endl;;
      }
      
    }
    complexity += min_new_info;
    cpx_fp << genotype->GetID() << " " 
      << oo_initial_entropy << " "
      << oo_conditional_entropy << " "
      << min_new_info << " " << complexity << "   ";
    
    int hamm_dist = cGenomeUtil::FindHammingDistance(genotype->GetGenome(),
                                                     used_genotype->GetGenome());
    total_dist += hamm_dist;
    cpx_fp << hamm_dist << " " << total_dist << "   ";
    
    int num_cpus = genotype->GetNumCPUs();
    total_cpus += num_cpus;
    cpx_fp << num_cpus << " " << total_cpus << "   ";
    
    
    genotype->PrintTasks(cpx_fp, 0, -1);
    cpx_fp << endl;
    given_genotypes.push_back(genotype);
  }
  
  m_world->GetDataFileManager().Remove(filename);
  return;
}

/* prints grid with what the fitness of an org in each range box would be given the resource levels
	at given update (10000 by default) SLG*/
void cAnalyze::CommandPrintResourceFitnessMap(cString cur_string)
{
  cout << "creating resource fitness map...\n";
  // at what update do we want to use the resource concentrations from?
  int update = 10000;
  if (cur_string.GetSize() != 0) update = cur_string.PopWord().AsInt();
  // what file to write data to
  cString filename("resourcefitmap.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  ofstream& fp = m_world->GetDataFileOFStream(filename);

  int f1=-1, f2=-1, rangecount[2]={0,0}, threshcount[2]={0,0};
  double f1Max = 0.0, f1Min = 0.0, f2Max = 0.0, f2Min = 0.0;
  
  // first need to find out how many thresh and range resources there are on each function
  // NOTE! this only works for 2-obj. problems right now!
  for (int i=0; i<m_world->GetEnvironment().GetReactionLib().GetSize(); i++)
  {
	  cReaction* react = m_world->GetEnvironment().GetReactionLib().GetReaction(i);
	  int fun = react->GetTask()->GetArguments().GetInt(0);
	  double thresh = react->GetTask()->GetArguments().GetDouble(3);
	  double threshMax = react->GetTask()->GetArguments().GetDouble(4);
	  if (i==0)
	  {
		  f1 = fun;
		  f1Max = react->GetTask()->GetArguments().GetDouble(1);
		  f1Min = react->GetTask()->GetArguments().GetDouble(2);
	  }
	  
	     if (fun==f1 && threshMax>0)
			 rangecount[0]++;
		 else if (fun==f1 && thresh>=0)
			 threshcount[0]++;
		 else if (fun!=f1 && threshcount[1]==0 && rangecount[1]==0)
		 {
			 f2=fun;
			 f2Max = react->GetTask()->GetArguments().GetDouble(1);
			 f2Min = react->GetTask()->GetArguments().GetDouble(2);
		 }
		 if (fun==f2 && threshMax>0)
			 rangecount[1]++;
		 else if (fun==f2 && thresh>=0)
			 threshcount[1]++;
	  
  }
  int fsize[2];
  fsize[0] = rangecount[0];
  if (threshcount[0]>fsize[0])
	  fsize[0]=threshcount[0];
  fsize[1]=rangecount[1];
  if (threshcount[1]>fsize[1])
	  fsize[1]=threshcount[1];

  cout << "f1 size: " << fsize[0] << "  f2 size: " << fsize[1] << endl;
  double stepsize[2];
  stepsize[0] = (f1Max-f1Min)/fsize[0];
  stepsize[1] = (f2Max-f2Min)/fsize[1];
  
  // this is our grid where we are going to calculate the fitness of an org in each box
  // given current resource contributions
  tArray< tArray<double> > fitnesses(fsize[0]+1);
  for (int i=0; i<fitnesses.GetSize(); i++)
	  fitnesses[i].Resize(fsize[1]+1,1);
  
   // figure out what index into resources that we loaded goes with update we want
  int index=-1;
  for (unsigned int i = 0; i < resources.size(); i++)
  {
	  if (resources.at(i).first == update)
	  {
		  index=i;
		  i=resources.size();
	  }
  }
  if (index<0) cout << "error, never found desired update in resource array!\n";

  else cout << "creating map using resources at update: " << update << endl;
   
  for (int i=0; i<m_world->GetEnvironment().GetResourceLib().GetSize(); i++)
  {
  	  // first have to find reaction that matches this resource, so compare names
	  cString name = m_world->GetEnvironment().GetResourceLib().GetResource(i)->GetName();
	  cReaction* react = NULL;
	  for (int j=0; j<m_world->GetEnvironment().GetReactionLib().GetSize(); j++)
	  {
		  if (m_world->GetEnvironment().GetReactionLib().GetReaction(j)->GetProcesses().GetPos(0)->GetResource()->GetName() == name)
		  {
			  react = m_world->GetEnvironment().GetReactionLib().GetReaction(j);
			  j = m_world->GetEnvironment().GetReactionLib().GetSize();
		  }
	  }
	  if (react==NULL)
	    continue;
	  // now have proper reaction, pull all the data need from the reaction
	  double frac = react->GetProcesses().GetPos(0)->GetMaxFraction(); 
	  double max = react->GetProcesses().GetPos(0)->GetMaxNumber();
	  double min = react->GetProcesses().GetPos(0)->GetMinNumber();
	  double value = react->GetValue();
	  int fun = react->GetTask()->GetArguments().GetInt(0);
	  if (fun==f1)
		  fun=0;
	  else if (fun==f2)
		  fun=1;
	  else cout << "function is neither f1 or f2! doh!\n";
	  double thresh = react->GetTask()->GetArguments().GetDouble(3);
	  double threshMax = react->GetTask()->GetArguments().GetDouble(4);
	  //double maxFx = react->GetTask()->GetArguments().GetDouble(1);
	  //double minFx = react->GetTask()->GetArguments().GetDouble(2);

	  // and pull the concentration of this resource from resource object loaded from resource.dat
	  double concentration = resources.at(index).second.at(i);

	  // calculate the merit based on this resource concentration, fraction, and value
	  double mer = concentration * frac * value;
	  if (mer > max)
		  mer=max;
	  else if (mer < min)
		  mer=0;
	  double threshMaxAdjusted, threshAdjusted;
	  // if this is a range reaction, need to update one entire row or column in fitnesses array
	  if (threshMax>0)
	  {			
		  for (int k=0; k<fsize[fun]; k++)
		  {
			  // function f1
			  if (fun==0)
			  {
				  threshMaxAdjusted = threshMax*(f1Max-f1Min) + f1Min;
				  threshAdjusted = thresh*(f1Max-f1Min) + f1Min;
				  double pos = stepsize[0]*k+f1Min+stepsize[0]/2.0;
				  if (threshAdjusted <= pos && threshMaxAdjusted >= pos)
				  {
					  for (int z=0; z<fsize[1]+1; z++)
						  fitnesses[k+1][z] *= pow(2,mer);
				  // actually solutions right at min possible get range above them too
					  if (k==0)
						  for (int z=0; z<fsize[1]+1; z++)
							  fitnesses[0][z] *= pow(2,mer);
				  }
			  }
			  // function f2
			  else 
			  {
				  threshMaxAdjusted = threshMax*(f2Max-f2Min) + f2Min;
				  threshAdjusted = thresh*(f2Max-f2Min) + f2Min;
				  double pos = stepsize[1]*k+f1Min+stepsize[1]/2.0;
				  if (threshAdjusted <= pos && threshMaxAdjusted >= pos)
				  {
					  for (int z=0; z<fsize[0]+1; z++)
						  fitnesses[z][k+1] *= pow(2,mer);
				  // actually solutions right at min possible get range above them too
					  if (k==0)
						  for (int z=0; z<fsize[0]+1; z++)
							  fitnesses[z][0] *= pow(2,mer);
				  }
			  }
		  }
	  }
	  // threshold reaction, need to update all rows or columns above given threshold
	  else if (thresh>=0)
	  {
		  for (int k=0; k<fsize[fun]+1; k++)
		  {
			  // function f1
			  if (fun==0)
			  {
			      threshAdjusted = thresh*(f1Max-f1Min) + f1Min;
			      double pos = stepsize[0]*k+f1Min-stepsize[0]/2.0;
			      if (threshAdjusted >= pos)
				{
				  for (int z=0; z<fsize[1]+1; z++)
				    {
				      fitnesses[k][z] *= pow(2,mer);
				    }
				}
				 
			  }
			  // function f2
			  else 
			  {				  
			    threshAdjusted = thresh*(f2Max-f2Min) + f2Min;
			    double pos = stepsize[1]*k+f1Min-stepsize[1]/2.0;
			    if (threshAdjusted >= pos)
			      {
				for (int z=0; z<fsize[0]+1; z++)
				  fitnesses[z][k] *= pow(2,mer);
			      } 
			  }
		  }
	  }
	  
	  }
   
  for (int i=fitnesses[0].GetSize()-1; i>=0; i--)
  {
    for (int j=0; j<fitnesses.GetSize(); j++)
	fp << fitnesses[j][i] << " ";
    fp << endl;
  }
}


//@ MRR @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void cAnalyze::CommandPairwiseEntropy(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON)
    cout << "Finding pairwise entropy on batch " << cur_batch << endl;
  else
    cout << "Finding pairwise entropy..." << endl;
  
  cout << "@MRR-> This command is being tested!" << endl;
  
  cString directory = PopDirectory(cur_string, "pairwise_data/");
  if (m_world->GetVerbosity() >= VERBOSE_ON)
    cout << "\tUsing directory: " << directory << endl;
  double mu = cur_string.PopWord().AsDouble();
  if (m_world->GetVerbosity() >= VERBOSE_ON)
    cout << "\tUsing mu=" << mu << endl;
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = batch_it.Next();
  
  cout << genotype->GetName() << endl;
  
  while(genotype != NULL)
  { 
    cString genName = genotype->GetName();
    
    if (m_world->GetVerbosity() >= VERBOSE_ON)
      cout << "\t...on genotype " << genName << endl;
    
    cString filename;
    filename.Set("%spairdata.%s.dat", static_cast<const char*>(directory),
                 static_cast<const char*>(genName));
    
    // @DMB -- ofstream& fp = m_world->GetDataFileOFStream(filename);
    
    if (m_world->GetVerbosity() >= VERBOSE_ON)
      cout << "\t\t...with filename:  " << filename << endl;
    
    cout << "# Pairwise Entropy Information" << endl;
    
    tMatrix<double> pairdata = AnalyzeEntropyPairs(genotype, mu);
    
    cout << pairdata.GetNumRows() << endl;
    
    for (int i=0;  i < pairdata.GetNumRows(); i++){
      for (int j=0; j < pairdata.GetNumCols(); j++)
        cout << pairdata[i][j] << " ";
      cout << endl;
    }
    m_world->GetDataFileManager().Remove(filename);
    genotype = batch_it.Next();
  }
}





// This command will take the current batch and analyze how well organisms
// cross-over with each other, both across the population and between mates.

void cAnalyze::AnalyzeMateSelection(cString cur_string)
{
  int sample_size = 10000;
  if (cur_string.GetSize() != 0) sample_size = cur_string.PopWord().AsInt();
  cString filename("none");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  double min_swap_frac = 0.0;
  if (cur_string.GetSize() != 0) min_swap_frac=cur_string.PopWord().AsDouble();
  double max_swap_frac = 1.0 - min_swap_frac;
  
  cout << "Analyzing Mate Selection... " << endl;
  
  // Do some quick tests before moving on...
  if (min_swap_frac < 0.0 || min_swap_frac >= 0.5) {
    cerr << "ERROR: Minimum swap fraction out of range [0.0, 0.5)." << endl;
  }
  
  // Next, we create an array that contains pointers to all of the organisms
  // in this batch.  Note that we want to select genotypes based on their
  // abundance, so they will have one entry in the array per organism.  Note
  // that we only consider viable genotypes.
  
  // Start by counting the total number of organisms (and do other such
  // data collection...
  tHashTable<int, int> mate_id_counts;
  
  int org_count = 0;
  int gen_count = 0;
  cAnalyzeGenotype * genotype = NULL;
  tListIterator<cAnalyzeGenotype> list_it(batch[cur_batch].List());
  while ((genotype = list_it.Next()) != NULL) {
    if (genotype->GetViable() == false || genotype->GetNumCPUs() == 0) {
      continue;
    }
    gen_count++;
    org_count += genotype->GetNumCPUs();
    
    // Keep track of how many organisms have each mate id...
    int mate_id = genotype->GetMateID();
    int count = 0;
    mate_id_counts.Find(mate_id, count);
    count += genotype->GetNumCPUs();
    mate_id_counts.SetValue(mate_id, count);
  }
  
  // Create an array of the correct size.
  tArray<cAnalyzeGenotype *> genotype_array(org_count);
  
  // And insert all of the organisms into the array.
  int cur_pos = 0;
  while ((genotype = list_it.Next()) != NULL) {
    if (genotype->GetViable() == false) continue;
    int cur_count = genotype->GetNumCPUs();
    for (int i = 0; i < cur_count; i++) {
      genotype_array[cur_pos++] = genotype;
    }
  }
  
  
  // Setup some variables to collect statistics.
  int total_matches_tested = 0;
  int fail_count = 0;
  int match_fail_count = 0;
  
  // Create a Test CPU
  cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();
  
  // Loop through all of the tests, picking random organisms each time and
  // performing a random cross test.
  cAnalyzeGenotype * genotype2 = NULL;
  for (int test_id = 0; test_id < sample_size; test_id++) {
    genotype = genotype_array[ m_world->GetRandom().GetUInt(org_count) ];
    genotype2 = genotype_array[ m_world->GetRandom().GetUInt(org_count) ];
    
    // Stop immediately if we're comparing a genotype to itself.
    if (genotype == genotype2) {
      total_matches_tested++;
      continue;
    }
    
    // Setup the random parameters for this test.
    cCPUMemory test_genome0 = genotype->GetGenome(); 
    cCPUMemory test_genome1 = genotype2->GetGenome(); 
    
    double start_frac = -1.0;
    double end_frac = -1.0;
    double swap_frac = -1.0;
    while (swap_frac < min_swap_frac || swap_frac > max_swap_frac) {
      start_frac = m_world->GetRandom().GetDouble();
      end_frac = m_world->GetRandom().GetDouble();
      if (start_frac > end_frac) nFunctions::Swap(start_frac, end_frac);
      swap_frac = end_frac - start_frac;
    }
    
    int start0 = (int) (start_frac * (double) test_genome0.GetSize());
    int end0   = (int) (end_frac * (double) test_genome0.GetSize());
    int size0 = end0 - start0;
    
    int start1 = (int) (start_frac * (double) test_genome1.GetSize());
    int end1   = (int) (end_frac * (double) test_genome1.GetSize());
    int size1 = end1 - start1;
    
    int new_size0 = test_genome0.GetSize() - size0 + size1;   
    int new_size1 = test_genome1.GetSize() - size1 + size0;
    
    // Setup some statistics for this particular test.
    bool same_mate_id = ( genotype->GetMateID() == genotype2->GetMateID() );
    if (same_mate_id == true) total_matches_tested++;
    
    // Don't Crossover if offspring will be illegal!!!
    if (new_size0 < MIN_CREATURE_SIZE || new_size0 > MAX_CREATURE_SIZE || 
        new_size1 < MIN_CREATURE_SIZE || new_size1 > MAX_CREATURE_SIZE) { 
      fail_count++; 
      if (same_mate_id == true) match_fail_count++;
      continue; 
    } 
    
    // Do the replacement...  We're only going to test genome0, so we only
    // need to modify that one.
    cGenome cross1 = cGenomeUtil::Crop(test_genome1, start1, end1);
    test_genome0.Replace(start0, size0, cross1);
    
    // Do the test.
    cCPUTestInfo test_info;
    
    // Run each side, and determine viability...
    testcpu->TestGenome(m_ctx, test_info, test_genome0);
    if( test_info.IsViable() == false ) {
      fail_count++;
      if (same_mate_id == true) match_fail_count++;
    }
  }
  delete testcpu;
  
  // Do some calculations on the sizes of the mate groups...
  const int num_mate_groups = mate_id_counts.GetSize();
  
  // Collect lists on all of the mate groups for the calculations...
  tList<int> key_list;
  tList<int> count_list;
  mate_id_counts.AsLists(key_list, count_list);
  tListIterator<int> count_it(count_list);
  
  int max_group_size = 0;
  double mate_id_entropy = 0.0;
  while (count_it.Next() != NULL) {
    int cur_count = *(count_it.Get());
    double cur_frac = ((double) cur_count) / ((double) org_count);
    if (cur_count > max_group_size) max_group_size = cur_count;
    mate_id_entropy -= cur_frac * log(cur_frac);
  }
  
  // Calculate the final answer
  double fail_frac = (double) fail_count / (double) sample_size;
  double match_fail_frac =
    (double) match_fail_count / (double) total_matches_tested;
  cout << "  ave fraction failed = " << fail_frac << endl
    << "  ave matches failed = " << match_fail_frac << endl
    << "  total mate matches = " <<  total_matches_tested
    << " / " << sample_size<< endl;
  
  if (filename == "none") return;
  
  cDataFile & df = m_world->GetDataFile(filename);
  df.WriteComment( "Mate selection information" );
  df.WriteTimeStamp();  
  
  df.Write(fail_frac,       "Average fraction failed");
  df.Write(match_fail_frac, "Average fraction of mate matches failed");
  df.Write(sample_size, "Total number of crossovers tested");
  df.Write(total_matches_tested, "Number of crossovers with matching mate IDs");
  df.Write(gen_count, "Number of genotypes in test batch");
  df.Write(org_count, "Number of organisms in test batch");
  df.Write(num_mate_groups, "Number of distinct mate IDs");
  df.Write(max_group_size, "Size of the largest distinct mate ID group");
  df.Write(mate_id_entropy, "Diversity of mate IDs (entropy)");
  df.Endl();
}


void cAnalyze::AnalyzeComplexityDelta(cString cur_string)
{
  // This command will examine the current population, and sample mutations
  // to see what the distribution of complexity changes is.  Only genotypes
  // with a certain abundance (default=3) will be tested to make sure that
  // the organism didn't already have hidden complexity due to a downward
  // step.
  cout << "Testing complexity delta." << endl;
  
  cString filename = "complexity_delta.dat";
  int num_tests = 10;
  double copy_mut_prob = m_world->GetConfig().COPY_MUT_PROB.Get();
  double ins_mut_prob = m_world->GetConfig().DIVIDE_INS_PROB.Get();
  double del_mut_prob = m_world->GetConfig().DIVIDE_DEL_PROB.Get();
  int count_threshold = 3;
  
  if (cur_string.GetSize() > 0) filename = cur_string.PopWord();
  if (cur_string.GetSize() > 0) num_tests = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() > 0) copy_mut_prob = cur_string.PopWord().AsDouble();
  if (cur_string.GetSize() > 0) ins_mut_prob = cur_string.PopWord().AsDouble();
  if (cur_string.GetSize() > 0) del_mut_prob = cur_string.PopWord().AsDouble();
  if (cur_string.GetSize() > 0) count_threshold = cur_string.PopWord().AsInt();
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "...using:"
    << " filename='" << filename << "'"
    << " num_tests=" << num_tests
    << " copy_mut_prob=" << copy_mut_prob
    << " ins_mut_prob=" << ins_mut_prob
    << " del_mut_prob=" << del_mut_prob
    << " count_threshold=" << count_threshold
    << endl;
  }
  
  // Create an array of all of the genotypes above threshold.
  cAnalyzeGenotype * genotype = NULL;
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  
  // Loop through all genotypes to perform a census
  int org_count = 0;
  while ((genotype = batch_it.Next()) != NULL) {
    // Only count genotypes above threshold
    if (genotype->GetNumCPUs() >= count_threshold) { 
      org_count += genotype->GetNumCPUs();
    }
  } 
  
  // Create an array to store pointers to the genotypes and fill it in.
  tArray<cAnalyzeGenotype *> org_array(org_count);
  int cur_org = 0;
  batch_it.Reset();
  while ((genotype = batch_it.Next()) != NULL) {
    // Ignore genotypes below threshold.
    if (genotype->GetNumCPUs() < count_threshold) continue;
    
    // Insert the remaining genotypes into the array.
    for (int i = 0; i < genotype->GetNumCPUs(); i++) {
      org_array[cur_org] = genotype;
      cur_org++;
    }
  }
  
  // Open up the file and prepare it for output.
  cDataFile & df = m_world->GetDataFile(filename);
  df.WriteComment( "An analyze of expected complexity changes between parent and offspring" );
  df.WriteTimeStamp();  
  
  // Next check the appropriate number of organisms, perform mutations, and
  // store the results.
  for (int cur_test = 0; cur_test < num_tests; cur_test++) {
    // Pick the genotype to test.
    int test_org_id = m_world->GetRandom().GetInt(org_count);
    genotype = org_array[test_org_id];
    
    // Create a copy of the genome.
    cCPUMemory mod_genome = genotype->GetGenome();
    
    if (copy_mut_prob == 0.0 &&
        ins_mut_prob == 0.0 &&
        del_mut_prob == 0.0) {
      cerr << "ERROR: All mutation rates are zero!  No complexity delta analysis possible." << endl;
      return;
    }
    
    // Perform the per-site mutations -- we are going to keep looping until
    // we trigger at least one mutation.
    int num_mutations = 0;
    int ins_line = -1;
    int del_line = -1;
    while (num_mutations == 0) {
      if (copy_mut_prob > 0.0) {
        for (int i = 0; i < mod_genome.GetSize(); i++) {
          if (m_world->GetRandom().P(copy_mut_prob)) {
            mod_genome.SetInstruction(i, inst_set.GetRandomInst(m_ctx));
            num_mutations++;
          }
        }
      }
      
      // Perform an Insertion if it has one.
      if (m_world->GetRandom().P(ins_mut_prob)) {
        ins_line = m_world->GetRandom().GetInt(mod_genome.GetSize() + 1);
        mod_genome.Insert(ins_line, inst_set.GetRandomInst(m_ctx));
        num_mutations++;
      }
      
      // Perform a Deletion if it has one.
      if (m_world->GetRandom().P(del_mut_prob)) {
        del_line = m_world->GetRandom().GetInt(mod_genome.GetSize());
        mod_genome.Remove(del_line);
        num_mutations++;
      }
    }
    
    // Collect basic state before and after the mutations...
    genotype->Recalculate(m_ctx);
    double start_complexity = genotype->GetKO_Complexity();
    double start_fitness = genotype->GetFitness();
    int start_length = genotype->GetLength();
    int start_gest = genotype->GetGestTime();
    const tArray<int>& start_task_counts = genotype->GetTaskCounts();
    const tArray< tArray<int> >& start_KO_task_counts = genotype->GetKO_TaskCounts();
    
    cAnalyzeGenotype new_genotype(m_world, mod_genome, inst_set);
    new_genotype.Recalculate(m_ctx);
    double end_complexity = new_genotype.GetKO_Complexity();
    double end_fitness = new_genotype.GetFitness();
    int end_length = new_genotype.GetLength();
    int end_gest = new_genotype.GetGestTime();
    const tArray<int> & end_task_counts = new_genotype.GetTaskCounts();
    const tArray< tArray<int> >& end_KO_task_counts = new_genotype.GetKO_TaskCounts();
    
    // Calculate the complexities....
    double complexity_change = end_complexity - start_complexity;
    
    // Loop through each line and determine if each line contributes to
    int total_info_new = 0;    // Site didn't encode info, but now does.
    int total_info_shift = 0;  // Shift in which tasks this site codes for.
    int total_info_pshift = 0; // Partial, but not total shift of tasks.
    int total_info_share = 0;  // Site codes for more tasks than before.
    int total_info_lost = 0;   // Site list all tasks it encoded for.
    int total_info_plost = 0;  // Site reduced tasks it encodes for.
    int total_info_kept = 0;   // Site still codes for sames tasks as before
    int total_info_lack = 0;   // Site never codes for any tasks.
    
    const int num_tasks = start_task_counts.GetSize();
    tArray<int> mut_effects(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
      mut_effects[i] = end_task_counts[i] - start_task_counts[i];
    }
    
    int end_line = 0;
    for (int start_line = 0; start_line < start_length; start_line++) {
      if (start_line == del_line) {
        // This line was deleted in the end.  Skip it, but don't increment
        // the end_line
        continue;
      }
      if (start_line == ins_line) {
        // This position had an insertion.  Deal with it and then skip it.
        end_line++;
        
        // No "continue" here.  With the updated end_line we can move on.
      }
      
      // If we made it this far, the start_line and end_line should be aligned.
      int info_maintained_count = 0;
      int info_gained_count = 0;
      int info_lost_count = 0;
      
      for (int cur_task = 0; cur_task < num_tasks; cur_task++) {
        // At the organism level, the mutation may have caused four options
        // for this task  (A) Was never present, (B) Was present and still is,
        // (C) Was not present, but is now, or (D) Was present, but was lost.
        
        // Case A:
        if (start_task_counts[cur_task]==0 && end_task_counts[cur_task]==0) {
          // This task was never done.  Keep looping.
          continue;
        }
        
        // Case B:
        if (start_task_counts[cur_task] == end_task_counts[cur_task]) {
          // The task hasn't changed.  Has its encoding?
          bool KO_start = true;
          bool KO_end = true;
          if (start_KO_task_counts[start_line][cur_task]  ==
              start_task_counts[cur_task]) {
            // start_count is unchanged by knocking out this line.
            KO_start = false;
          }
          if (end_KO_task_counts[end_line][cur_task]  ==
              end_task_counts[cur_task]) {
            // end_count is unchanged by knocking out this line.
            KO_end = false;
          }
          
          if (KO_start == true && KO_end == true) info_maintained_count++;
          if (KO_start == true && KO_end == false) info_lost_count++;
          if (KO_start == false && KO_end == true) info_gained_count++;
          continue;
        }
        
        // Case C:
        if (start_task_counts[cur_task] < end_task_counts[cur_task]) {
          // Task was GAINED...  Is this site important?
          if (end_KO_task_counts[end_line][cur_task]  <
              end_task_counts[cur_task]) {
            info_gained_count++;
          }
          continue;
        }
        
        // Case D:
        if (start_task_counts[cur_task] > end_task_counts[cur_task]) {
          // The task was LOST...  Was this site important?
          if (start_KO_task_counts[start_line][cur_task]  <
              start_task_counts[cur_task]) {
            info_lost_count++;
          }
          continue;
        }
      }
      
      // We now have counts and know how often this site was responsible for
      // a task gain, a task loss, or a task being maintained.
      
      bool has_keep = info_maintained_count > 0;
      bool has_loss = info_lost_count > 0;
      bool has_gain = info_gained_count > 0;      
      
      if      ( !has_loss  &&  !has_gain  &&  !has_keep ) total_info_lack++;
      else if ( !has_loss  &&  !has_gain  &&   has_keep ) total_info_kept++;
      else if ( !has_loss  &&   has_gain  &&  !has_keep ) total_info_new++;
      else if ( !has_loss  &&   has_gain  &&   has_keep ) total_info_share++;
      else if (  has_loss  &&  !has_gain  &&  !has_keep ) total_info_lost++;
      else if (  has_loss  &&  !has_gain  &&   has_keep ) total_info_plost++;
      else if (  has_loss  &&   has_gain  &&  !has_keep ) total_info_shift++;
      else if (  has_loss  &&   has_gain  &&   has_keep ) total_info_pshift++;
      
      end_line++;
    }
    
    
    // Output the results.
    df.Write(num_mutations, "Number of mutational differences between original organism and mutant.");
    df.Write(complexity_change, "Complexity difference between original organism and mutant.");
    df.Write(start_complexity, "Total complexity of initial organism.");
    df.Write(end_complexity, "Total complexity of mutant.");
    
    // Broken down complexity info
    df.Write(total_info_lack, "Num sites with no info at all.");
    df.Write(total_info_kept, "Num sites with info, but no change.");
    df.Write(total_info_new, "Num sites with new info (prev. none).");
    df.Write(total_info_share, "Num sites with newly shared info.");
    df.Write(total_info_lost, "Num sites with lost info.");
    df.Write(total_info_plost, "Num sites with parital lost info.");
    df.Write(total_info_shift, "Num sites with shift in info.");
    df.Write(total_info_pshift, "Num sites with partial shift in info.");
    
    // Start and End task counts...
    for (int i = 0; i < start_task_counts.GetSize(); i++) {
      df.Write(start_task_counts[i], cStringUtil::Stringf("Start task %d", i));
    }
    
    for (int i = 0; i < end_task_counts.GetSize(); i++) {
      df.Write(end_task_counts[i], cStringUtil::Stringf("End task %d", i));
    }
    
    df.Write(start_fitness, "Fitness of initial organism.");
    df.Write(end_fitness, "Fitness of mutant.");
    df.Write(start_length, "Length of initial organism.");
    df.Write(end_length, "Length of mutant.");
    df.Write(start_gest, "Gestation Time of initial organism.");
    df.Write(end_gest, "Gestation Time of mutant.");
    df.Write(genotype->GetID(), "ID of initial genotype.");
    df.Endl();
  }
}

void cAnalyze::AnalyzeKnockouts(cString cur_string)
{
  cout << "Analyzing the effects of knockouts..." << endl;
  
  cString filename = "knockouts.dat";
  if (cur_string.GetSize() > 0) filename = cur_string.PopWord();
  
  int max_knockouts = 1;
  if (cur_string.GetSize() > 0) max_knockouts = cur_string.PopWord().AsInt();
  
  // Open up the data file...
  cDataFile & df = m_world->GetDataFile(filename);
  df.WriteComment( "Analysis of knockouts in genomes" );
  df.WriteTimeStamp();  
  
  
  // Setup a NULL instruction in a special inst set.
  cInstSet ko_inst_set(inst_set);
  const cInstruction null_inst = ko_inst_set.ActivateNullInst();
  
  // Loop through all of the genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "  Knockout: " << genotype->GetName() << endl;
    
    // Calculate the stats for the genotype we're working with...
    genotype->Recalculate(m_ctx);
    const double base_fitness = genotype->GetFitness();
    
    const int max_line = genotype->GetLength();
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);
    
    // Loop through all the lines of code, testing the removal of each.
    // -2=lethal, -1=detrimental, 0=neutral, 1=beneficial
    int dead_count = 0;
    int neg_count = 0;
    int neut_count = 0;
    int pos_count = 0;
    tArray<int> ko_effect(max_line);
    for (int line_num = 0; line_num < max_line; line_num++) {
      // Save a copy of the current instruction and replace it with "NULL"
      int cur_inst = base_genome.GetOp(line_num);
      mod_genome.SetInstruction(line_num, null_inst);
      cAnalyzeGenotype ko_genotype(m_world, mod_genome, ko_inst_set);
      ko_genotype.Recalculate(m_ctx);
      
      double ko_fitness = ko_genotype.GetFitness();
      if (ko_fitness == 0.0) {
        dead_count++;
        ko_effect[line_num] = -2;
      } else if (ko_fitness < base_fitness) {
        neg_count++;
        ko_effect[line_num] = -1;
      } else if (ko_fitness == base_fitness) {
        neut_count++;
        ko_effect[line_num] = 0;
      } else if (ko_fitness > base_fitness) {
        pos_count++;
        ko_effect[line_num] = 1;
      } else {
        cerr << "ERROR: illegal state in AnalyzeKnockouts()" << endl;
      }
      
      // Reset the mod_genome back to the original sequence.
      mod_genome.SetOp(line_num, cur_inst);
    }
    
    tArray<int> ko_pair_effect(ko_effect);
    if (max_knockouts > 1) {
      for (int line1 = 0; line1 < max_line; line1++) {
      	for (int line2 = line1+1; line2 < max_line; line2++) {
          int cur_inst1 = base_genome.GetOp(line1);
          int cur_inst2 = base_genome.GetOp(line2);
          mod_genome.SetInstruction(line1, null_inst);
          mod_genome.SetInstruction(line2, null_inst);
          cAnalyzeGenotype ko_genotype(m_world, mod_genome, ko_inst_set);
          ko_genotype.Recalculate(m_ctx);
          
          double ko_fitness = ko_genotype.GetFitness();
          
          // If both individual knockouts are both harmful, but in combination
          // they are neutral or even beneficial, they should not count as 
          // information.
          if (ko_fitness >= base_fitness &&
              ko_effect[line1] < 0 && ko_effect[line2] < 0) {
            ko_pair_effect[line1] = 0;
            ko_pair_effect[line2] = 0;
          }
          
          // If the individual knockouts are both neutral (or beneficial?),
          // but in combination they are harmful, they are likely redundant
          // to each other.  For now, count them both as information.
          if (ko_fitness < base_fitness &&
              ko_effect[line1] >= 0 && ko_effect[line2] >= 0) {
            ko_pair_effect[line1] = -1;
            ko_pair_effect[line2] = -1;
          }	
          
          // Reset the mod_genome back to the original sequence.
          mod_genome.SetOp(line1, cur_inst1);
          mod_genome.SetOp(line2, cur_inst2);
        }
      }
    }    
    
    int pair_dead_count = 0;
    int pair_neg_count = 0;
    int pair_neut_count = 0;
    int pair_pos_count = 0;
    for (int i = 0; i < max_line; i++) {
      if (ko_pair_effect[i] == -2) pair_dead_count++;
      else if (ko_pair_effect[i] == -1) pair_neg_count++;
      else if (ko_pair_effect[i] == 0) pair_neut_count++;
      else if (ko_pair_effect[i] == 1) pair_pos_count++;
    }
    
    // Output data...
    df.Write(genotype->GetID(), "Genotype ID");
    df.Write(dead_count, "Count of lethal knockouts");
    df.Write(neg_count,  "Count of detrimental knockouts");
    df.Write(neut_count, "Count of neutral knockouts");
    df.Write(pos_count,  "Count of beneficial knockouts");
    df.Write(pair_dead_count, "Count of lethal knockouts after paired knockout tests.");
    df.Write(pair_neg_count,  "Count of detrimental knockouts after paired knockout tests.");
    df.Write(pair_neut_count, "Count of neutral knockouts after paired knockout tests.");
    df.Write(pair_pos_count,  "Count of beneficial knockouts after paired knockout tests.");
    df.Endl();
  }
}


void cAnalyze::CommandFitnessMatrix(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Calculating fitness matrix for batch " << cur_batch << endl;
  else cout << "Calculating fitness matrix..." << endl;
  
  cout << "Warning: considering only first genotype of the batch!" << endl;
  
  // Load in the variables...
  int depth_limit = 4;
  if (cur_string.GetSize() != 0) depth_limit = cur_string.PopWord().AsInt();
  
  double fitness_threshold_ratio = .9;
  if (cur_string.GetSize() != 0) fitness_threshold_ratio = cur_string.PopWord().AsDouble();
  
  int ham_thresh  = 1;
  if (cur_string.GetSize() != 0) ham_thresh = cur_string.PopWord().AsInt();
  
  double error_rate_min = 0.005;
  if (cur_string.GetSize() != 0) error_rate_min = cur_string.PopWord().AsDouble();
  
  double error_rate_max = 0.05;
  if (cur_string.GetSize() != 0) error_rate_max = cur_string.PopWord().AsDouble();
  
  double error_rate_step = 0.005;
  if (cur_string.GetSize() != 0) error_rate_step = cur_string.PopWord().AsDouble();
  
  double vect_fmax = 1.1;
  if (cur_string.GetSize() != 0) vect_fmax = cur_string.PopWord().AsDouble();
  
  double vect_fstep = .1;
  if (cur_string.GetSize() != 0) vect_fstep = cur_string.PopWord().AsDouble();
  
  int diag_iters = 200;
  if (cur_string.GetSize() != 0) diag_iters = cur_string.PopWord().AsInt();
  
  int write_ham_vector = 0;
  if (cur_string.GetSize() != 0) write_ham_vector = cur_string.PopWord().AsInt();
  
  int write_full_vector = 0;
  if (cur_string.GetSize() != 0) write_full_vector = cur_string.PopWord().AsInt();
  
  // Consider only the first genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = batch_it.Next();
  
  cFitnessMatrix matrix(m_world, genotype->GetGenome(), &inst_set);
  
  matrix.CalcFitnessMatrix(depth_limit, fitness_threshold_ratio, ham_thresh, error_rate_min,
                           error_rate_max, error_rate_step, vect_fmax, vect_fstep, diag_iters,
                           write_ham_vector, write_full_vector );
}


void cAnalyze::CommandMapTasks(cString cur_string)
{
  cString msg;  //Use if to construct any messages to send to driver
  
  m_world->GetDriver().NotifyComment("Constructing genotype-phenotype maps");
  
  // Load in the variables / default them
  cString directory         = PopDirectory(cur_string.PopWord(), "phenotype/");
  int     print_mode        = 0;   // 0=Normal, 1=Boolean results
  int     file_type         = FILE_TYPE_TEXT;
  bool    use_manual_inputs = false;  // Should we use manual inputs?
  
  // HTML special flags...
  bool link_maps = false;  // Should muliple maps be linked together?
  bool link_insts = false; // Should links be made to instruction descs?
  
  // Collect any other format information needed...
  tList< tDataEntryCommand<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > output_it(output_list);
  tArray<int> manual_inputs;
  
  cStringList arg_list(cur_string);
  
  msg.Set("Found %d args.", arg_list.GetSize());
  m_world->GetDriver().NotifyComment(msg);
  
  int use_resources = 0;
  
  // Check for some command specific variables, removing them from the list if found.
  if (arg_list.PopString("0") != "")                 print_mode = 0;
  if (arg_list.PopString("1") != "")                 print_mode = 1;
  if (arg_list.PopString("text") != "")              file_type = FILE_TYPE_TEXT;
  if (arg_list.PopString("html") != "")              file_type = FILE_TYPE_HTML;
  if (arg_list.PopString("link_maps") != "")         link_maps = true;
  if (arg_list.PopString("link_insts") != "")        link_insts = true;
  if (arg_list.PopString("use_resources=2") != "")   use_resources = 2;
  if (arg_list.HasString("use_manual_inputs"))       use_manual_inputs = true;
  
  if (use_manual_inputs){
    int pos = arg_list.LocateString("use_manual_inputs");
    arg_list.PopString("use_manual_inputs");
    manual_inputs.Resize(m_world->GetEnvironment().GetInputSize());
    if (arg_list.GetSize() >= pos + m_world->GetEnvironment().GetInputSize() - 1)
      for (int k = 0; k < m_world->GetEnvironment().GetInputSize(); k++)
        manual_inputs[k] = arg_list.PopLine(pos).AsInt();  
    else
      m_world->GetDriver().RaiseFatalException(1, "CommandMapTask: Invalid use of use_manual_inputs");
  }
  
  msg.Set("There are %d column args.", arg_list.GetSize());
  m_world->GetDriver().NotifyComment(msg);
  
  LoadGenotypeDataList(arg_list, output_list);
  
  m_world->GetDriver().NotifyComment("Args are loaded.");
  
  const int num_cols = output_list.GetSize();
  
  
  // Give some information in verbose mode.
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  outputing as ";
    if (print_mode == 1) cout << "boolean ";
    if (file_type == FILE_TYPE_TEXT) {
      cout << "text files." << endl;
    } else { // if (file_type == FILE_TYPE_HTML) {
      cout << "HTML files";
      if (link_maps == true) cout << "; linking files together";
      if (link_maps == true) cout << "; linking inst names to descs";
      cout << "." << endl;
    }
    cout << "  Format: ";
    
    output_it.Reset();
    while (output_it.Next() != NULL) {
      cout << output_it.Get()->GetName() << " ";
    }
    cout << endl;
  }
  
  
  ///////////////////////////////////////////////////////
  // Loop through all of the genotypes in this batch...
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "  Mapping " << genotype->GetName() << endl;
    
    // Construct this filename...
    cString filename;
    if (file_type == FILE_TYPE_TEXT) {
      filename.Set("%stasksites.%s.dat", static_cast<const char*>(directory), static_cast<const char*>(genotype->GetName()));
    } else {   //  if (file_type == FILE_TYPE_HTML) {
      filename.Set("%stasksites.%s.html", static_cast<const char*>(directory), static_cast<const char*>(genotype->GetName()));
    }
    ofstream& fp = m_world->GetDataFileOFStream(filename);
    
    // Construct linked filenames...
    cString next_file("");
    cString prev_file("");
    if (link_maps == true) {
      // Check the next genotype on the list...
      if (batch_it.Next() != NULL) {
        next_file.Set("tasksites.%s.html", static_cast<const char*>(batch_it.Get()->GetName()));
      }
      batch_it.Prev();  // Put the list back where it was...
      
      // Check the previous genotype on the list...
      if (batch_it.Prev() != NULL) {
        prev_file.Set("tasksites.%s.html", static_cast<const char*>(batch_it.Get()->GetName()));
      }
      batch_it.Next();  // Put the list back where it was...
    }
    
    // Calculate the stats for the genotype we're working with...
    cCPUTestInfo test_info;
    if (use_manual_inputs)
      test_info.UseManualInputs(manual_inputs);
    test_info.SetResourceOptions(use_resources, &resources);
    genotype->Recalculate(m_ctx, &test_info);
    
    // Headers...
    if (file_type == FILE_TYPE_TEXT) {
      fp << "-1 "  << batch[cur_batch].Name() << " "
      << genotype->GetID() << " ";
      
      tDataEntryCommand<cAnalyzeGenotype> * data_command = NULL;
      while ((data_command = output_it.Next()) != NULL) {
        data_command->SetTarget(genotype);
        fp << data_command->GetValue() << " ";
      }
      fp << endl;
      
    } else { // if (file_type == FILE_TYPE_HTML) {
             // Mark file as html
      fp << "<html>" << endl;
      
      // Setup any javascript macros needed...
      fp << "<head>" << endl;
      if (link_insts == true) {
        fp << "<script language=\"javascript\">" << endl
        << "function Inst(inst_name)" << endl
        << "{" << endl
        << "var filename = \"help.\" + inst_name + \".html\";" << endl
        << "newwin = window.open(filename, 'Instruction', "
        << "'toolbar=0,status=0,location=0,directories=0,menubar=0,"
        << "scrollbars=1,height=150,width=300');" << endl
        << "newwin.focus();" << endl
        << "}" << endl
        << "</script>" << endl;
      }
      fp << "</head>" << endl;
      
      // Setup the body...
      fp << "<body bgcolor=\"#FFFFFF\"" << endl
        << " text=\"#000000\"" << endl
        << " link=\"#0000AA\"" << endl
        << " alink=\"#0000FF\"" << endl
        << " vlink=\"#000044\">" << endl
        << endl
        << "<h1 align=center>Run " << batch[cur_batch].Name()
        << ", ID " << genotype->GetID() << "</h1>" << endl
        << "<center>" << endl
        << endl;
      
      // Links?
      fp << "<table width=90%><tr><td align=left>";
      if (prev_file != "") fp << "<a href=\"" << prev_file << "\">Prev</a>";
      else fp << "&nbsp;";
      fp << "<td align=right>";
      if (next_file != "") fp << "<a href=\"" << next_file << "\">Next</a>";
      else fp << "&nbsp;";
      fp << "</tr></table>" << endl;
      
      // The table
      fp << "<table border=1 cellpadding=2>" << endl;
      
      // The headings...///
      fp << "<tr><td colspan=3> ";
      output_it.Reset();
      while (output_it.Next() != NULL) {
        fp << "<th>" << output_it.Get()->GetDesc() << " ";
      }
      fp << "</tr>" << endl;
      
      // The base creature...
      fp << "<tr><th colspan=3>Base Creature";
      tDataEntryCommand<cAnalyzeGenotype> * data_command = NULL;
      cAnalyzeGenotype null_genotype(m_world, "a", inst_set);
      while ((data_command = output_it.Next()) != NULL) {
        data_command->SetTarget(genotype);
        genotype->SetSpecialArgs(data_command->GetArgs());
        const cFlexVar cur_value = data_command->GetValue();
        const cFlexVar null_value = data_command->GetValue(&null_genotype);
        int compare = CompareFlexStat(cur_value, null_value, data_command->GetCompareType()); 
        if (compare > 0) {
          fp << "<th bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_POS.Get() << "\">";
        }
        else  fp << "<th bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_LETHAL.Get() << "\">";
        
        if (data_command->HasArg("blank") == true) fp << "&nbsp;" << " ";
        else fp << cur_value << " ";
      }
      fp << "</tr>" << endl;
    }
    
    
    const int max_line = genotype->GetLength();
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);
    
    // Keep track of the number of failues/successes for attributes...
    int * col_pass_count = new int[num_cols];
    int * col_fail_count = new int[num_cols];
    for (int i = 0; i < num_cols; i++) {
      col_pass_count[i] = 0;
      col_fail_count[i] = 0;
    }
    
    cInstSet map_inst_set(inst_set);
    const cInstruction null_inst = map_inst_set.ActivateNullInst();
    
    // Loop through all the lines of code, testing the removal of each.
    for (int line_num = 0; line_num < max_line; line_num++) {
      int cur_inst = base_genome.GetOp(line_num);
      char cur_symbol = base_genome.GetSymbol(line_num);
      
      mod_genome.SetInstruction(line_num, null_inst);
      cAnalyzeGenotype test_genotype(m_world, mod_genome, map_inst_set);
      test_genotype.Recalculate(m_ctx, &test_info);
      
      if (file_type == FILE_TYPE_HTML) fp << "<tr><td align=right>";
      fp << (line_num + 1) << " ";
      if (file_type == FILE_TYPE_HTML) fp << "<td align=center>";
      fp << cur_symbol << " ";
      if (file_type == FILE_TYPE_HTML) fp << "<td align=center>";
      if (link_insts == true) {
        fp << "<a href=\"javascript:Inst('"
        << map_inst_set.GetName(cur_inst)
        << "')\">";
      }
      fp << map_inst_set.GetName(cur_inst) << " ";
      if (link_insts == true) fp << "</a>";
      
      
      // Print the individual columns...
      output_it.Reset();
      tDataEntryCommand<cAnalyzeGenotype> * data_command = NULL;
      int cur_col = 0;
      while ((data_command = output_it.Next()) != NULL) {
        data_command->SetTarget(&test_genotype);
        test_genotype.SetSpecialArgs(data_command->GetArgs());
        const cFlexVar test_value = data_command->GetValue();
        int compare = CompareFlexStat(test_value, data_command->GetValue(genotype), data_command->GetCompareType());
        
        // BUG! Either of the next two conditional print commands can
        // cause landscaping to be triggered in a context that causes a crash, 
        // notably, if you don't provide any column parameters to MapTasks.. @JEB
        if (file_type == FILE_TYPE_HTML) {
          HTMLPrintStat(test_value, fp, compare, data_command->GetHtmlCellFlags(), data_command->GetNull(),
                        !(data_command->HasArg("blank")));
        } 
        else fp << test_value << " ";
        
        if (compare == -2) col_fail_count[cur_col]++;
        else if (compare == 2) col_pass_count[cur_col]++;
        cur_col++;
      }
      if (file_type == FILE_TYPE_HTML) fp << "</tr>";
      fp << endl;
      
      // Reset the mod_genome back to the original sequence.
      mod_genome.SetOp(line_num, cur_inst);
    }
    
    
    // Construct the final line of the table with all totals...
    if (file_type == FILE_TYPE_HTML) {
      fp << "<tr><th colspan=3>Totals";
      
      for (int i = 0; i < num_cols; i++) {
        if (col_pass_count[i] > 0) {
          fp << "<th bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_POS.Get() << "\">" << col_pass_count[i];
        }
        else if (col_fail_count[i] > 0) {
          fp << "<th bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_LETHAL.Get() << "\">" << col_fail_count[i];
        }
        else fp << "<th>0";
      }
      fp << "</tr>" << endl;
      
      // And close everything up...
      fp << "</table>" << endl
        << "</center>" << endl;
    }
    
    delete [] col_pass_count;
    delete [] col_fail_count;
    m_world->GetDataFileManager().Remove(filename);  // Close the data file object
    }
    }

void cAnalyze::CommandAverageModularity(cString cur_string)
{
  cout << "Average Modularity calculations" << endl;
  
  // Load in the variables...
  cString filename = cur_string.PopWord();
  //    cString filename = "average.dat";
  
  int print_mode = 0;   // 0=Normal, 1=Boolean results
  
  // Collect any other format information needed...
  tList< tDataEntryCommand<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > output_it(output_list);
  
  cStringList arg_list(cur_string);
  
  cout << "Found " << arg_list.GetSize() << " args." << endl;
  
  // Check for some command specific variables.
  if (arg_list.PopString("0") != "") print_mode = 0;
  if (arg_list.PopString("1") != "") print_mode = 1;
  
  cout << "There are " << arg_list.GetSize() << " column args." << endl;
  
  LoadGenotypeDataList(arg_list, output_list);
  
  cout << "Args are loaded." << endl;
  
  const int num_cols = output_list.GetSize();
  
  // Give some information in verbose mode.
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  outputing as ";
    if (print_mode == 1) cout << "boolean ";
    cout << "text files." << endl;
    cout << "  Format: ";
    
    output_it.Reset();
    while (output_it.Next() != NULL) {
      cout << output_it.Get()->GetName() << " ";
    }
    cout << endl;
  }
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // printing the headers
  // not done by default since many dumps may be analyzed at the same time
  // and results would be put in the same file
  if (arg_list.GetSize()==0) {
    // Headers
    fp << "# Avida analyze modularity data" << endl;
    fp << "# 1: organism length" << endl;
    fp << "# 2: number of tasks done" << endl;
    fp << "# 3: number of sites used in tasks" << endl;
    fp << "# 4: proportion of sites used in tasks" << endl;
    fp << "# 5: average number of tasks done per site" << endl;
    fp << "# 6: average number sites per task done" << endl;
    fp << "# 7: average number tasks per site per task" << endl;
    fp << "# 8: average proportion of the non-overlaping region of a task" << endl;
    fp << "# 9-17: average StDev in positions used for task 1-9" << endl;
    fp << "# 18-26: average number of sites necessary for each of the tasks" << endl;
    fp << "# 27-36: number of sites involved in 0-9 tasks" << endl;
    fp << "# 37-45: average task length (distance from first to last inst used)" << endl;
    fp << endl;
    return;
  }        
  
  // initialize various variables used in calculations
  
  int num_orgs = 0;		// number of organisms in the dump
  
  double  av_length = 0; 	// average organism length
  double  av_task = 0; 	// average # of tasks done
  double  av_inst = 0; 	// average # instructions used in tasks
  double  av_inst_len = 0; 	// proportion of sites used for tasks
  double  av_site_task = 0; 	// average number of sites per task
  double  av_task_site = 0;   // average number of tasks per site
  double  av_t_s_norm = 0;	// average number of tasks per site per task
  double  av_task_overlap = 0; // average overlap between tasks
  
  // average StDev in positions used for a task
  tArray<double> std_task_position(num_cols);
  std_task_position.SetAll(0.0);
  
  // # of organisms actually doing a task
  tArray<double> org_task(num_cols);
  org_task.SetAll(0.0);
  
  // av. # of sites necessary for each of the tasks
  tArray<double> av_num_inst(num_cols);
  av_num_inst.SetAll(0.0);
  
  // number of sites involved in 0-9 tasks 
  tArray<double> av_inst_task(num_cols+1);
  av_inst_task.SetAll(0.0);
  
  // av. # task length (distance from first to last site used)
  tArray<double> av_task_length(num_cols);
  av_task_length.SetAll(0.0);
  
  
  ///////////////////////////////////////////////////////
  // Loop through all of the genotypes in this batch...
  ///////////////////////////////////////////////////////
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  
  // would like to test oly the viable ones, but they can be non-viable
  // and still reproduce and do tasks
  // while ((genotype = batch_it.Next()) != NULL && genotype->GetViable()) {
  while ((genotype = batch_it.Next()) != NULL) {
    
    int num_cpus = genotype->GetNumCPUs();
    
    if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "  Mapping " << genotype->GetName() << endl;
    
    // Calculate the stats for the genotype we're working with...
    genotype->Recalculate(m_ctx);
    
    // Check if the organism does any tasks. 
    int does_tasks = 0;
    for (int i = 0; i < num_cols; i++) {
      if (genotype->GetTaskCount(i) > 0)  does_tasks = 1;
    }
    
    // Don't calculate the modularity if the organism doesn't reproduce
    // i.e. if the fitness is 0 
    if (genotype->GetFitness() != 0 && does_tasks != 0) {
      num_orgs = num_orgs + num_cpus;
      
      const int max_line = genotype->GetLength();
      const cGenome & base_genome = genotype->GetGenome();
      cGenome mod_genome(base_genome);
      
      // Create and initialize the modularity matrix
      tMatrix<int> mod_matrix(num_cols, max_line);
      mod_matrix.SetAll(0);
      
      // Create and initialize the task overalp matrix
      tMatrix<int> task_overlap(num_cols, num_cols);
      task_overlap.SetAll(0);
      
      // Create an initialize the counters for modularity
      tArray<int> num_task(max_line); // number of tasks instruction is used in
      tArray<int> num_inst(num_cols); // number of instructions involved in a task
      tArray<int> sum(num_cols); 	    // helps with StDev calculations
      tArray<int> sumsq(num_cols);    // helps with StDev calculations
      tArray<int> inst_task(num_cols+1); // # of inst's involved in 0,1,2,3... tasks
      tArray<int> task_length(num_cols);    // ditance between first and last inst involved in a task
      
      num_task.SetAll(0);
      num_inst.SetAll(0);
      sum.SetAll(0);
      sumsq.SetAll(0);
      inst_task.SetAll(0);
      task_length.SetAll(0);
      
      int total_task = 0;        // total number of tasks done
      int total_inst = 0;        // total number of instructions involved in tasks
      int total_all = 0;         // sum of mod_matrix
      double sum_task_overlap = 0;// task overlap for for this geneome
        
        cInstSet map_inst_set(inst_set);
        const cInstruction null_inst = map_inst_set.ActivateNullInst();
        
        // Loop through all the lines of code, testing the removal of each.
        for (int line_num = 0; line_num < max_line; line_num++) {
          int cur_inst = base_genome.GetOp(line_num);
          
          mod_genome.SetInstruction(line_num, null_inst);
          cAnalyzeGenotype test_genotype(m_world, mod_genome, map_inst_set);
          cAnalyzeGenotype old_genotype(m_world, base_genome, map_inst_set);
          test_genotype.Recalculate(m_ctx);
          
          // Print the individual columns...
          output_it.Reset();
          tDataEntryCommand<cAnalyzeGenotype> * data_command = NULL;
          int cur_col = 0;
          while ((data_command = output_it.Next()) != NULL) {
            data_command->SetTarget(&test_genotype);
            test_genotype.SetSpecialArgs(data_command->GetArgs());
            const cFlexVar test_value = data_command->GetValue();
            
            // This is done so that under 'binary' option it marks
            // the task as being influenced by the mutation iff
            // it is completely knocked out, not just decreased
            genotype->SetSpecialArgs(data_command->GetArgs());
            
            int compare_type = data_command->GetCompareType();
            int compare = CompareFlexStat(test_value, data_command->GetValue(genotype), compare_type);
            
            // If knocking out an instruction stops the expression of a
            // particular task, mark that in the modularity matrix
            // and add it to two counts
            // Only do the checking if the test_genotype replicate, i.e.
            // if it's fitness is not zeros
            
            if (compare < 0  && test_genotype.GetFitness() != 0) {
              mod_matrix(cur_col,line_num) = 1;
              num_inst[cur_col]++;
              num_task[line_num]++;
            }
            cur_col++;
          }
          
          // Reset the mod_genome back to the original sequence.
          mod_genome.SetOp(line_num, cur_inst);
        } // end of genotype-phenotype mapping for a single organism
        
        for (int i = 0; i < num_cols; i++) {if (num_inst[i] != 0) total_task++;}
        for (int i = 0; i < max_line; i++) {if (num_task[i] != 0) total_inst++;}
        for (int i = 0; i < num_cols; i++) {total_all = total_all + num_inst[i];}
        
        // Add the values to the av_ variables, used for calculating the average
        // in order to weigh them by abundance, multiply everything by num_cpus
        
        av_length = av_length + max_line*num_cpus;
        av_task = av_task + total_task*num_cpus;
        av_inst = av_inst + total_inst*num_cpus;
        av_inst_len = av_inst_len + (double) total_inst*num_cpus/max_line;
        
        if (total_task !=0)  av_site_task = av_site_task + num_cpus * (double) total_all/total_task; 
        if (total_inst !=0)  av_task_site = av_task_site + num_cpus * (double) total_all/total_inst; 
        if (total_inst !=0 && total_task !=0) {
          av_t_s_norm = av_t_s_norm + num_cpus * (double) total_all/(total_inst*total_task); 
        }
        
        for (int i = 0; i < num_cols; i++) { 
          if (num_inst[i] > 0) {
            av_num_inst[i] = av_num_inst[i] + num_inst[i] * num_cpus;
            org_task[i] = org_task[i] + num_cpus;   // count how many are actually doing the task
          }
        }	
        
        // calculate average task overlap
        // first construct num_task x num_task matrix with number of sites overlapping
        for (int i = 0; i < max_line; i++) {
          for (int j = 0; j < num_cols; j++) {
            for (int k = j; k < num_cols; k++) {
              if (mod_matrix(j,i)>0 && mod_matrix(k,i)>0) {
                task_overlap(j,k)++;
                if (j!=k) task_overlap(k,j)++;
              }               
            }
          }
        }
        
        // go though the task_overlap matrix, add and average everything up. 
        if (total_task > 1) {
          for (int i = 0; i < num_cols; i++) {
            double overlap_per_task = 0;                 
            for (int j = 0; j < num_cols; j++) {
              if (i!=j) {overlap_per_task = overlap_per_task + task_overlap(i,j);}
            }
            if (task_overlap(i,i) !=0){
              sum_task_overlap = sum_task_overlap + overlap_per_task / (task_overlap(i,i) * (total_task-1));   
            }
          }
        }
        
        // now, divide that by number of tasks done and add to the grand sum, weigthed by num_cpus 
        if (total_task!=0) {
          av_task_overlap = av_task_overlap + num_cpus * (double) sum_task_overlap/total_task ;
        }
        // calculate the first/last postion of a task, the task "spread"
        // starting from the top look for the fist command that matters for a task
        
        for (int i = 0; i < num_cols; i++) { 
          int j = 0; 
          while (j < max_line) {
            if (mod_matrix(i,j) > 0 && task_length[i] == 0 ) {
              task_length[i] = j;
              break;
            }
            j++;
          }
        }
        
        // starting frm the bottom look for the last command that matters for a task
        // and substract it from the first to get the task length
        // add one in order to account for both the beginning and the end instruction
        for (int i = 0; i < num_cols; i++) { 
          int j = max_line - 1; 
          while (j > -1) {
            if (mod_matrix(i,j) > 0) {
              task_length[i] = j - task_length[i] + 1;
              break;
            }
            j--;
          }
        }
        // add the task lengths to the average for the batch
        // weigthed by the number of cpus for that genotype 
        for (int i = 0; i < num_cols; i++) { 
          av_task_length[i] = av_task_length[i] +  num_cpus * task_length[i];
        }
        
        // calculate the Standard Deviation in the mean position of the task
        for (int i = 0; i < num_cols; i++) { 
          for (int j = 0; j < max_line; j++) { 
            if (mod_matrix(i,j)>0) sum[i] = sum[i] + j;
          }		
        }
        
        double temp = 0;
        for (int i = 0; i < num_cols; i++) {
          if (num_inst[i]>1) { 
            double av_sum = sum[i]/num_inst[i];
            for (int j = 0; j < max_line; j++) {
              if (mod_matrix(i,j)>0) temp = (av_sum - j)*(av_sum - j);
            }
            std_task_position[i] = std_task_position[i] + sqrt(temp/(num_inst[i]-1))*num_cpus;
          } 
        } 
        
        for (int i = 0; i < max_line; i++) { inst_task[num_task[i]]++ ;}
        for (int i = 0; i < num_cols+1; i++) { av_inst_task[i] = av_inst_task[i] + inst_task[i] * num_cpus;}
        
    }
  }  // this is the end of the loop going though all the organisms
  
  // make sure there are some organisms doing task in this batch
  // if not, return all zeros
  
  if (num_orgs != 0) { 
    fp << (double) av_length/num_orgs  << " ";  	// 1: average length
    fp << (double) av_task/num_orgs << " ";		// 2: av. number of tasks done
    fp << (double) av_inst/num_orgs << " ";		// 3: av. number of sites used for tasks
    fp << (double) av_inst_len/num_orgs << " ";		// 4: proportion of sites used for tasks
    fp << (double) av_task_site/num_orgs << " ";	// 5: av. number of tasks per site
    fp << (double) av_site_task/num_orgs << " ";	// 6: av. number of sites per task
    fp << (double) av_t_s_norm/num_orgs << " ";		// 7: av. number of tasks per site per task
    fp << (double) 1 - av_task_overlap/num_orgs << " ";        // 8: av. proportion of a task that DOESN'T overlap
    for (int i = 0; i < num_cols; i++) {
      if (org_task[i] > 0) fp << std_task_position[i]/org_task[i]  << " ";
      else fp << 0 << " ";
    }
    for (int i = 0; i < num_cols; i++) {
      if (org_task[i] > 0) fp << (double) av_num_inst[i]/org_task[i]  << " ";
      else fp << 0 << " ";
    }
    for (int i = 0; i < num_cols+1; i++) { fp << (double) av_inst_task[i]/num_orgs  << " ";}
    for (int i = 0; i < num_cols; i++) { fp << (double) av_task_length[i]/num_orgs  << " ";}
    fp << endl;
  }
  
  else {
    for (int i = 0; i < 8+4*num_cols+1; i++) {fp << "0 ";}
    fp << endl;
  }
  }


void cAnalyze::CommandAnalyzeModularity(cString cur_string)
{
  cString filename("analyze_modularity.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  cDataFile & df = m_world->GetDataFile(filename);
  df.WriteComment( "Modularity Analysis" );
  df.WriteTimeStamp();
  
  // Determine which phenotypic traits we're working with
  tList< tDataEntryCommand<cAnalyzeGenotype> > output_list;
  tListIterator< tDataEntryCommand<cAnalyzeGenotype> > output_it(output_list);
  cStringList arg_list(cur_string);
  LoadGenotypeDataList(arg_list, output_list);
  const int num_traits = output_list.GetSize();
  
  // Setup the map_inst_set with the NULL instruction
  cInstSet map_inst_set(inst_set);
  const cInstruction null_inst = map_inst_set.ActivateNullInst();
  
  
  // Loop through all genotypes in this batch.
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    const int base_length = genotype->GetLength();
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);
    genotype->Recalculate(m_ctx);
    
    tMatrix<bool> task_matrix(num_traits, base_length);
    tArray<int> num_inst(num_traits);  // Number of instructions for each task
    tArray<int> num_task(base_length); // Number of traits at each locus
    task_matrix.SetAll(false);
    num_inst.SetAll(0);
    num_task.SetAll(0);
    
    // Loop through all lines in this genome
    for (int line_num = 0; line_num < base_length; line_num++) {
      int cur_inst = base_genome.GetOp(line_num);
      
      // Determine what happens to this genotype when this line is knocked out
      mod_genome.SetInstruction(line_num, null_inst);
      cAnalyzeGenotype test_genotype(m_world, mod_genome, map_inst_set);
      test_genotype.Recalculate(m_ctx);
      
      // Loop through the individual traits
      output_it.Reset();
      tDataEntryCommand<cAnalyzeGenotype> * data_command = NULL;
      int cur_trait = 0;
      while ((data_command = output_it.Next()) != NULL) {
        data_command->SetTarget(&test_genotype);
        test_genotype.SetSpecialArgs(data_command->GetArgs());
        const cFlexVar test_value = data_command->GetValue();
        
        int compare_type = data_command->GetCompareType();
        int compare = CompareFlexStat(test_value, data_command->GetValue(genotype), compare_type);
        
        // If knocking out the instruction turns off this trait, mark it in
        // the modularity matrix.  Only check if the test_genotype replicates,
        // i.e. if its fitness is not zeros
        if (compare < 0  && test_genotype.GetFitness() != 0) {
          task_matrix(cur_trait, line_num) = true;
          num_inst[cur_trait]++;
          num_task[line_num]++;
          // cout << line_num << " : true" << endl;
        } else {
          // cout << line_num << " : false" << endl;
        }
        cur_trait++;
      }
      
      // Reset the mod_genome back to the original sequence.
      mod_genome.SetOp(line_num, cur_inst);
    } // end of genotype-phenotype mapping for a single organism
    
    
    // --== PHYSICAL MODULARITY ==--
    
    double ave_dist = 0.0;  // Average distance between sites in traits.
    
    // Loop through each task to calculate its physical modularity
    int trait_count = 0; // Count active traits...
    int site_count = 0;  // Count total sites for all traits...
    for (int cur_trait = 0; cur_trait < num_traits; cur_trait++) {
      //       cout << "Trait " << cur_trait << ", coded for by "
      //         << num_inst[cur_trait] << " instructions." << endl;
      
      // Ignore traits not coded for in this genome...
      if (num_inst[cur_trait] == 0) continue;
      
      // Keep track of how many traits we're examining...
      trait_count++;
      
      double trait_dist = 0.0;  // Total distance between sites in this trait.
      int num_samples = 0;      // Count samples we take for this trait.
      
      // Compare all pairs of positions.
      for (int pos1 = 0; pos1 < base_length; pos1++) {
        if (task_matrix(cur_trait, pos1) == false) continue;
        site_count++;
        for (int pos2 = pos1+1; pos2 < base_length; pos2++) {
          if (task_matrix(cur_trait, pos2) == false) continue;
          
          // We'll only make it this far if both positions code for the trait.
          num_samples++;
          
          // Calculate the distance...
          int cur_dist = pos2 - pos1;
          
          // Remember to consider that the genome is circular.
          if (2*cur_dist > base_length) cur_dist = base_length - cur_dist;
          
          //        cout << "Pos " << pos1 << " and " << pos2 << "; distance="
          //             << cur_dist << endl;
          
          // And add it into the total for this trait.
          trait_dist += cur_dist;
        }
      }
      
      // Assert that we found the correct number of samples.
      //assert(num_samples = num_inst(cur_trait) * (num_inst(cur_trait)-1) / 2);
      
      // Now that we have all of the distances for this trait, divide by the
      // number of samples and add it to the average.
      ave_dist += trait_dist / num_samples;
    }
    
    
    // Now that we've summed up all of the average distances for this
    // genotype, calculate the physical modularity.
    
    double PM = 1.0 - (ave_dist / (double) (base_length * trait_count));
    double ave_sites = ((double) site_count) / (double) trait_count;
    
    // Write the restults to file...
    df.Write(PM,          "Physical Modularity");
    df.Write(trait_count, "Number of traits used in calculation");
    df.Write(ave_sites,   "Average num sites associated with traits");
    df.Write(base_length, "Genome length");
    df.Write(ave_dist,    "Average Distance between trait sites");
    df.Endl();
  }
  
  // @CAO CONTINUE HERE
}


void cAnalyze::CommandMapMutations(cString cur_string)
{
  cout << "Constructing genome mutations maps..." << endl;
  
  // Load in the variables...
  cString directory = PopDirectory(cur_string, "mutations/");
  int file_type = FILE_TYPE_TEXT;
  
  cStringList arg_list(cur_string);
  
  // Check for some command specific variables.
  if (arg_list.PopString("text") != "") file_type = FILE_TYPE_TEXT;
  if (arg_list.PopString("html") != "") file_type = FILE_TYPE_HTML;
  
  // Give some information in verbose mode.
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  outputing as ";
    if (file_type == FILE_TYPE_TEXT) cout << "text files." << endl;
    else cout << "HTML files." << endl;
  }
  
  
  ///////////////////////////////////////////////////////
  // Loop through all of the genotypes in this batch...
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "  Creating mutation map for " << genotype->GetName() << endl;
    }
    
    // Construct this filename...
    cString filename;
    if (file_type == FILE_TYPE_TEXT) {
      filename.Set("%smut_map.%s.dat", static_cast<const char*>(directory), static_cast<const char*>(genotype->GetName()));
    } else {   //  if (file_type == FILE_TYPE_HTML) {
      filename.Set("%smut_map.%s.html", static_cast<const char*>(directory), static_cast<const char*>(genotype->GetName()));
    }
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "  Using filename \"" << filename << "\"" << endl;
    }
    ofstream& fp = m_world->GetDataFileOFStream(filename);
    
    // Calculate the stats for the genotype we're working with...
    genotype->Recalculate(m_ctx);
    const double base_fitness = genotype->GetFitness();
    const int num_insts = inst_set.GetSize();
    
    // Headers...
    if (file_type == FILE_TYPE_TEXT) {
      fp << "# 1: Genome instruction ID (pre-mutation)" << endl;
      for (int i = 0; i < num_insts; i++) {
        fp << "# " << i+1 <<": Fit if mutated to '"
        << inst_set.GetName(i) << "'" << endl;
      }
      fp << "# " << num_insts + 2 << ": Knockout" << endl;
      fp << "# " << num_insts + 3 << ": Fraction Lethal" << endl;
      fp << "# " << num_insts + 4 << ": Fraction Detremental" << endl;
      fp << "# " << num_insts + 5 << ": Fraction Neutral" << endl;
      fp << "# " << num_insts + 6 << ": Fraction Beneficial" << endl;
      fp << "# " << num_insts + 7 << ": Average Fitness" << endl;
      fp << "# " << num_insts + 8 << ": Expected Entropy" << endl;
      fp << "# " << num_insts + 9 << ": Original Instruction Name" << endl;
      fp << endl;
      
    } else { // if (file_type == FILE_TYPE_HTML) {
             // Mark file as html
      fp << "<html>" << endl;
      
      // Setup the body...
      fp << "<body bgcolor=\"#FFFFFF\"" << endl
        << " text=\"#000000\"" << endl
        << " link=\"#0000AA\"" << endl
        << " alink=\"#0000FF\"" << endl
        << " vlink=\"#000044\">" << endl
        << endl
        << "<h1 align=center>Mutation Map for Run " << batch[cur_batch].Name()
        << ", ID " << genotype->GetID() << "</h1>" << endl
        << "<center>" << endl
        << endl;
      
      // The main chart...
      fp << "<table border=1 cellpadding=2>" << endl;
      
      // The headings...///
      fp << "<tr><th>Genome ";
      for (int i = 0; i < num_insts; i++) {
        fp << "<th>" << inst_set.GetName(i) << " ";
      }
      fp << "<th>Knockout ";
      fp << "<th>Frac. Lethal ";
      fp << "<th>Frac. Detremental ";
      fp << "<th>Frac. Neutral ";
      fp << "<th>Frac. Beneficial ";
      fp << "<th>Ave. Fitness ";
      fp << "<th>Expected Entropy ";
      fp << "</tr>" << endl << endl;
    }
    
    const int max_line = genotype->GetLength();
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);
    
    // Keep track of the number of mutations in each category...
    int total_dead = 0, total_neg = 0, total_neut = 0, total_pos = 0;
    double total_fitness = 0.0;
    tArray<double> col_fitness(num_insts + 1);
    col_fitness.SetAll(0.0);
    
    // Build an empty instruction into the an instruction library.
    cInstSet map_inst_set(inst_set);
    const cInstruction null_inst = map_inst_set.ActivateNullInst();
    
    cString color_string;  // For coloring cells...
    
    // Loop through all the lines of code, testing all mutations...
    for (int line_num = 0; line_num < max_line; line_num++) {
      int cur_inst = base_genome.GetOp(line_num);
      char cur_symbol = base_genome.GetSymbol(line_num);
      int row_dead = 0, row_neg = 0, row_neut = 0, row_pos = 0;
      double row_fitness = 0.0;
      
      // Column 1... the original instruction in the geneome.
      if (file_type == FILE_TYPE_HTML) {
        fp << "<tr><td align=right>" << inst_set.GetName(cur_inst)
        << " (" << cur_symbol << ") ";
      } else {
        fp << cur_inst << " ";
      }
      
      // Columns 2 to D+1 (the possible mutations)
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) 
      {
        if (mod_inst == cur_inst) {
          if (file_type == FILE_TYPE_HTML) {
            color_string = "#FFFFFF";
            fp << "<th bgcolor=\"" << color_string << "\">";
          }
        }
        else {
          mod_genome.SetOp(line_num, mod_inst);
          cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
          test_genotype.Recalculate(m_ctx);
          const double test_fitness = test_genotype.GetFitness() / base_fitness;
          row_fitness += test_fitness;
          total_fitness += test_fitness;
          col_fitness[mod_inst] += test_fitness;
          
          // Categorize this mutation...
          if (test_fitness == 1.0) {           // Neutral Mutation...
            row_neut++;
            total_neut++;
            if (file_type == FILE_TYPE_HTML) color_string = m_world->GetConfig().COLOR_MUT_NEUT.Get();
          } else if (test_fitness == 0.0) {    // Lethal Mutation...
            row_dead++;
            total_dead++;
            if (file_type == FILE_TYPE_HTML) color_string = m_world->GetConfig().COLOR_MUT_LETHAL.Get();
          } else if (test_fitness < 1.0) {     // Detrimental Mutation...
            row_neg++;
            total_neg++;
            if (file_type == FILE_TYPE_HTML) color_string = m_world->GetConfig().COLOR_MUT_NEG.Get();
          } else {                             // Beneficial Mutation...
            row_pos++;
            total_pos++;
            if (file_type == FILE_TYPE_HTML) color_string = m_world->GetConfig().COLOR_MUT_POS.Get();
          }
          
          // Write out this cell...
          if (file_type == FILE_TYPE_HTML) {
            fp << "<th bgcolor=\"" << color_string << "\">";
          }
          fp << test_fitness << " ";
        }
      }
      
      // Column: Knockout
      mod_genome.SetInstruction(line_num, null_inst);
      cAnalyzeGenotype test_genotype(m_world, mod_genome, map_inst_set);
      test_genotype.Recalculate(m_ctx);
      const double test_fitness = test_genotype.GetFitness() / base_fitness;
      col_fitness[num_insts] += test_fitness;
      
      // Categorize this mutation if its in HTML mode (color only)...
      if (file_type == FILE_TYPE_HTML) {
        if (test_fitness == 1.0) color_string =  m_world->GetConfig().COLOR_MUT_NEUT.Get();
        else if (test_fitness == 0.0) color_string = m_world->GetConfig().COLOR_MUT_LETHAL.Get();
        else if (test_fitness < 1.0) color_string = m_world->GetConfig().COLOR_MUT_NEG.Get();
        else color_string = m_world->GetConfig().COLOR_MUT_POS.Get();
        
        fp << "<th bgcolor=\"" << color_string << "\">";
      }
      
      fp << test_fitness << " ";
      
      // Fraction Columns...
      if (file_type == FILE_TYPE_HTML) fp << "<th bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_LETHAL.Get() << "\">";
      fp << (double) row_dead / (double) (num_insts-1) << " ";
      
      if (file_type == FILE_TYPE_HTML) fp << "<th bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_NEG.Get() << "\">";
      fp << (double) row_neg / (double) (num_insts-1) << " ";
      
      if (file_type == FILE_TYPE_HTML) fp << "<th bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_NEUT.Get() << "\">";
      fp << (double) row_neut / (double) (num_insts-1) << " ";
      
      if (file_type == FILE_TYPE_HTML) fp << "<th bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_POS.Get() << "\">";
      fp << (double) row_pos / (double) (num_insts-1) << " ";
      
      
      // Column: Average Fitness
      if (file_type == FILE_TYPE_HTML) fp << "<th>";
      fp << row_fitness / (double) (num_insts-1) << " ";
      
      // Column: Expected Entropy  @CAO Implement!
      if (file_type == FILE_TYPE_HTML) fp << "<th>";
      fp << 0.0 << " ";
      
      // End this row...
      if (file_type == FILE_TYPE_HTML) fp << "</tr>";
      fp << endl;
      
      // Reset the mod_genome back to the original sequence.
      mod_genome.SetOp(line_num, cur_inst);
    }
    
    
    // Construct the final line of the table with all totals...
    if (file_type == FILE_TYPE_HTML) {
      fp << "<tr><th>Totals";
      
      // Instructions + Knockout
      for (int i = 0; i <= num_insts; i++) {
        fp << "<th>" << col_fitness[i] / max_line << " ";
      }
      
      int total_tests = max_line * (num_insts-1);
      fp << "<th>" << (double) total_dead / (double) total_tests << " ";
      fp << "<th>" << (double) total_neg / (double) total_tests << " ";
      fp << "<th>" << (double) total_neut / (double) total_tests << " ";
      fp << "<th>" << (double) total_pos / (double) total_tests << " ";
      fp << "<th>" << total_fitness / (double) total_tests << " ";
      fp << "<th>" << 0.0 << " ";
      
      
      // And close everything up...
      fp << "</table>" << endl
        << "</center>" << endl;
    }
    
    }
    }


void cAnalyze::CommandMapDepth(cString cur_string)
{
  cout << "Constructing depth map..." << endl;
  
  cString filename("depth_map.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();  
  
  int min_batch = 0;
  int max_batch = cur_batch - 1;
  
  if (cur_string.GetSize() != 0) min_batch = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() != 0) max_batch = cur_string.PopWord().AsInt();
  
  // First, scan all of the batches to find the maximum depth.
  int max_depth = -1;
  cAnalyzeGenotype * genotype;
  for (int i = min_batch; i <= max_batch; i++) {
    tListIterator<cAnalyzeGenotype> list_it(batch[i].List());
    while ((genotype = list_it.Next()) != NULL) {
      if (genotype->GetDepth() > max_depth) max_depth = genotype->GetDepth();
    }
  }
  
  cout << "max_depth = " << max_depth << endl;
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  cout << "Output to " << filename << endl;
  tArray<int> depth_array(max_depth+1);
  for (cur_batch = min_batch; cur_batch <= max_batch; cur_batch++) {
    depth_array.SetAll(0);
    tListIterator<cAnalyzeGenotype> list_it(batch[cur_batch].List());
    while ((genotype = list_it.Next()) != NULL) {
      const int cur_depth = genotype->GetDepth();
      const int cur_count = genotype->GetNumCPUs();
      depth_array[cur_depth] += cur_count;
    }
    
    for (int i = 0; i <= max_depth; i++) {
      fp << depth_array[i] << " ";
    }
    fp << endl;
  }
}

void cAnalyze::CommandHamming(cString cur_string)
{
  cString filename("hamming.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  int batch1 = PopBatch(cur_string.PopWord());
  int batch2 = PopBatch(cur_string.PopWord());
  
  // We want batch2 to be the larger one for efficiency...
  if (batch[batch1].List().GetSize() > batch[batch2].List().GetSize()) {
    int tmp = batch1;  batch1 = batch2;  batch2 = tmp;
  }
  
  if (m_world->GetVerbosity() <= VERBOSE_NORMAL) {
    cout << "Calculating Hamming Distance... ";
    cout.flush();
  } else {
    cout << "Calculating Hamming Distance between batches "
    << batch1 << " and " << batch2 << endl;
    cout.flush();
  }
  
  // Setup some variables;
  cAnalyzeGenotype * genotype1 = NULL;
  cAnalyzeGenotype * genotype2 = NULL;
  int total_dist = 0;
  int total_count = 0;
  
  tListIterator<cAnalyzeGenotype> list1_it(batch[batch1].List());
  tListIterator<cAnalyzeGenotype> list2_it(batch[batch2].List());
  
  while ((genotype1 = list1_it.Next()) != NULL) {
    list2_it.Reset();
    while ((genotype2 = list2_it.Next()) != NULL) {
      // Determine the counts...
      const int count1 = genotype1->GetNumCPUs();
      const int count2 = genotype2->GetNumCPUs();
      const int num_pairs = (genotype1 == genotype2) ?
        ((count1 - 1) * (count2 - 1)) : (count1 * count2);
      if (num_pairs == 0) continue;
      
      // And do the tests...
      const int dist =
        cGenomeUtil::FindHammingDistance(genotype1->GetGenome(),
                                         genotype2->GetGenome());
      total_dist += dist * num_pairs;
      total_count += num_pairs;
    }
  }
  
  
  // Calculate the final answer
  double ave_dist = (double) total_dist / (double) total_count;
  cout << " ave distance = " << ave_dist << endl;
  
  cDataFile & df = m_world->GetDataFile(filename);
  
  df.WriteComment( "Hamming distance information" );
  df.WriteTimeStamp();  
  
  df.Write(batch[batch1].Name(), "Name of First Batch");
  df.Write(batch[batch2].Name(), "Name of Second Batch");
  df.Write(ave_dist,             "Average Hamming Distance");
  df.Write(total_count,          "Total Pairs Test");
  df.Endl();
}

void cAnalyze::CommandLevenstein(cString cur_string)
{
  cString filename("lev.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  int batch1 = PopBatch(cur_string.PopWord());
  int batch2 = PopBatch(cur_string.PopWord());
  
  // We want batch2 to be the larger one for efficiency...
  if (batch[batch1].List().GetSize() > batch[batch2].List().GetSize()) {
    int tmp = batch1;  batch1 = batch2;  batch2 = tmp;
  }
  
  if (m_world->GetVerbosity() <= VERBOSE_NORMAL) {
    cout << "Calculating Levenstein Distance... ";
    cout.flush();
  } else {
    cout << "Calculating Levenstein Distance between batch "
    << batch1 << " and " << batch2 << endl;
    cout.flush();
  }
  
  // Setup some variables;
  cAnalyzeGenotype * genotype1 = NULL;
  cAnalyzeGenotype * genotype2 = NULL;
  int total_dist = 0;
  int total_count = 0;
  
  tListIterator<cAnalyzeGenotype> list1_it(batch[batch1].List());
  tListIterator<cAnalyzeGenotype> list2_it(batch[batch2].List());
  
  // Loop through all of the genotypes in each batch...
  while ((genotype1 = list1_it.Next()) != NULL) {
    list2_it.Reset();
    while ((genotype2 = list2_it.Next()) != NULL) {
      // Determine the counts...
      const int count1 = genotype1->GetNumCPUs();
      const int count2 = genotype2->GetNumCPUs();
      const int num_pairs = (genotype1 == genotype2) ?
        ((count1 - 1) * (count2 - 1)) : (count1 * count2);
      if (num_pairs == 0) continue;
      
      // And do the tests...
      const int dist = cGenomeUtil::FindEditDistance(genotype1->GetGenome(),
                                                     genotype2->GetGenome());
      total_dist += dist * num_pairs;
      total_count += num_pairs;
    }
  }
  
  // Calculate the final answer
  double ave_dist = (double) total_dist / (double) total_count;
  cout << " ave distance = " << ave_dist << endl;
  
  cDataFile & df = m_world->GetDataFile(filename);
  
  df.WriteComment( "Levenstein distance information" );
  df.WriteTimeStamp();  
  
  df.Write(batch[batch1].Name(), "Name of First Batch");
  df.Write(batch[batch2].Name(), "Name of Second Batch");
  df.Write(ave_dist,             "Average Levenstein Distance");
  df.Write(total_count,          "Total Pairs Test");
  df.Endl();
}

void cAnalyze::CommandSpecies(cString cur_string)
{
  cString filename("species.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  int batch1 = PopBatch(cur_string.PopWord());
  int batch2 = PopBatch(cur_string.PopWord());
  int num_compare = PopBatch(cur_string.PopWord());
  
  // We want batch2 to be the larger one for efficiency...
  if (batch[batch1].List().GetSize() > batch[batch2].List().GetSize()) {
    int tmp = batch1;  batch1 = batch2;  batch2 = tmp;
  }
  
  if (m_world->GetVerbosity() <= VERBOSE_NORMAL) cout << "Calculating Species Distance... " << endl;
  else cout << "Calculating Species Distance between batch "
    << batch1 << " and " << batch2 << endl;
  
  // Setup some variables;
  cAnalyzeGenotype * genotype1 = NULL;
  cAnalyzeGenotype * genotype2 = NULL;
  int total_fail = 0;
  int total_count = 0;
  
  cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();
  
  tListIterator<cAnalyzeGenotype> list1_it(batch[batch1].List());
  tListIterator<cAnalyzeGenotype> list2_it(batch[batch2].List());
  
  // Loop through all of the genotypes in each batch...
  while ((genotype1 = list1_it.Next()) != NULL) {
    list2_it.Reset();
    while ((genotype2 = list2_it.Next()) != NULL) {
      // Determine the counts...
      const int count1 = genotype1->GetNumCPUs();
      const int count2 = genotype2->GetNumCPUs();
      int num_pairs = count1 * count2; 
      int fail_count = 0;
      bool cross1_viable = true;
      bool cross2_viable = true;
      
      
      if (genotype1 == genotype2) {
        total_count += num_pairs * 2 * num_compare;
      }
      else {	
        assert(num_compare!=0);
        // And do the tests...
        for (int iter=1; iter < num_compare; iter++) {
          cCPUMemory test_genome0 = genotype1->GetGenome(); 
          cCPUMemory test_genome1 = genotype2->GetGenome(); 
          
          double start_frac = m_world->GetRandom().GetDouble();
          double end_frac = m_world->GetRandom().GetDouble();
          if (start_frac > end_frac) nFunctions::Swap(start_frac, end_frac);
          
          int start0 = (int) (start_frac * (double) test_genome0.GetSize());
          int end0   = (int) (end_frac * (double) test_genome0.GetSize());
          int start1 = (int) (start_frac * (double) test_genome1.GetSize());
          int end1   = (int) (end_frac * (double) test_genome1.GetSize());
          assert( start0 >= 0  &&  start0 < test_genome0.GetSize() );
          assert( end0   >= 0  &&  end0   < test_genome0.GetSize() );
          assert( start1 >= 0  &&  start1 < test_genome1.GetSize() );
          assert( end1   >= 0  &&  end1   < test_genome1.GetSize() );
          
          // Calculate size of sections crossing over...    
          int size0 = end0 - start0;
          int size1 = end1 - start1;
          
          int new_size0 = test_genome0.GetSize() - size0 + size1;   
          int new_size1 = test_genome1.GetSize() - size1 + size0;
          
          // Don't Crossover if offspring will be illegal!!!
          if (new_size0 < MIN_CREATURE_SIZE || new_size0 > MAX_CREATURE_SIZE || 
              new_size1 < MIN_CREATURE_SIZE || new_size1 > MAX_CREATURE_SIZE) { 
            fail_count +=2; 
            break; 
          } 
          
          // Swap the components
          cGenome cross0 = cGenomeUtil::Crop(test_genome0, start0, end0);
          cGenome cross1 = cGenomeUtil::Crop(test_genome1, start1, end1);
          test_genome0.Replace(start0, size0, cross1);
          test_genome1.Replace(start1, size1, cross0);
          
          // Run each side, and determine viability...
          cCPUTestInfo test_info;
          testcpu->TestGenome(m_ctx, test_info, test_genome0);
          cross1_viable = test_info.IsViable();
          
          testcpu->TestGenome(m_ctx, test_info, test_genome1);
          cross2_viable = test_info.IsViable();
          
          if (cross1_viable == false) fail_count++;   
          if (cross2_viable == false) fail_count++;
        }
        
        total_fail += fail_count * num_pairs;
        total_count += num_pairs * 2 * num_compare;
      }
    }
  }
  
  delete testcpu;
  
  // Calculate the final answer
  double ave_dist = (double) total_fail / (double) total_count;
  cout << "  ave distance = " << ave_dist
    << " in " << total_count
    << " tests." << endl; 
  
  cDataFile & df = m_world->GetDataFile(filename);
  
  df.WriteComment( "Species information" );
  df.WriteTimeStamp();  
  
  df.Write(batch[batch1].Name(), "Name of First Batch");
  df.Write(batch[batch2].Name(), "Name of Second Batch");
  df.Write(ave_dist,             "Average Species Distance");
  df.Write(total_count,          "Total Recombinants tested");
  df.Endl();
}

void cAnalyze::CommandRecombine(cString cur_string)
{
  int batch1 = PopBatch(cur_string.PopWord());
  int batch2 = PopBatch(cur_string.PopWord());
  int batch3 = PopBatch(cur_string.PopWord());
  int num_compare = PopBatch(cur_string.PopWord());
  
  // We want batch2 to be the larger one for efficiency...
  if (batch[batch1].List().GetSize() > batch[batch2].List().GetSize()) {
    int tmp = batch1;  batch1 = batch2;  batch2 = tmp;
  }
  
  if (m_world->GetVerbosity() <= VERBOSE_NORMAL) cout << "Creating recombinants...  " << endl;
  else cout << "Creating recombinants between batch "
    << batch1 << " and " << batch2 << endl;
  
  // Setup some variables;
  cAnalyzeGenotype * genotype1 = NULL;
  cAnalyzeGenotype * genotype2 = NULL;
  
  tListIterator<cAnalyzeGenotype> list1_it(batch[batch1].List());
  tListIterator<cAnalyzeGenotype> list2_it(batch[batch2].List());
  
  // Loop through all of the genotypes in each batch...
  while ((genotype1 = list1_it.Next()) != NULL) {
    list2_it.Reset();
    while ((genotype2 = list2_it.Next()) != NULL) {
      // Determine the counts...
      int fail_count = 0;
      
      
      assert(num_compare!=0);
      // And do the tests...
      for (int iter=1; iter < num_compare; iter++) {
        cCPUMemory test_genome0 = genotype1->GetGenome(); 
        cCPUMemory test_genome1 = genotype2->GetGenome(); 
        
        double start_frac = m_world->GetRandom().GetDouble();
        double end_frac = m_world->GetRandom().GetDouble();
        if (start_frac > end_frac) nFunctions::Swap(start_frac, end_frac);
        
        int start0 = (int) (start_frac * (double) test_genome0.GetSize());
        int end0   = (int) (end_frac * (double) test_genome0.GetSize());
        int start1 = (int) (start_frac * (double) test_genome1.GetSize());
        int end1   = (int) (end_frac * (double) test_genome1.GetSize());
        assert( start0 >= 0  &&  start0 < test_genome0.GetSize() );
        assert( end0   >= 0  &&  end0   < test_genome0.GetSize() );
        assert( start1 >= 0  &&  start1 < test_genome1.GetSize() );
        assert( end1   >= 0  &&  end1   < test_genome1.GetSize() );
        
        // Calculate size of sections crossing over...    
        int size0 = end0 - start0;
        int size1 = end1 - start1;
        
        int new_size0 = test_genome0.GetSize() - size0 + size1;   
        int new_size1 = test_genome1.GetSize() - size1 + size0;
        
        // Don't Crossover if offspring will be illegal!!!
        if (new_size0 < MIN_CREATURE_SIZE || new_size0 > MAX_CREATURE_SIZE || 
            new_size1 < MIN_CREATURE_SIZE || new_size1 > MAX_CREATURE_SIZE) { 
          fail_count +=2; 
          break; 
        } 
        
        if (size0 > 0 && size1 > 0) {
          cGenome cross0 = cGenomeUtil::Crop(test_genome0, start0, end0);
          cGenome cross1 = cGenomeUtil::Crop(test_genome1, start1, end1);
          test_genome0.Replace(start0, size0, cross1);
          test_genome1.Replace(start1, size1, cross0);
        }
        else if (size0 > 0) {
          cGenome cross0 = cGenomeUtil::Crop(test_genome0, start0, end0);
          test_genome1.Replace(start1, size1, cross0);
        }
        else if (size1 > 0) {
          cGenome cross1 = cGenomeUtil::Crop(test_genome1, start1, end1);
          test_genome0.Replace(start0, size0, cross1);
        }
        
        cAnalyzeGenotype * new_genotype0 = new cAnalyzeGenotype(m_world, test_genome0, inst_set); 
        cAnalyzeGenotype * new_genotype1 = new cAnalyzeGenotype(m_world, test_genome1, inst_set); 
        new_genotype0->SetNumCPUs(1); 
        new_genotype1->SetNumCPUs(1); 
        new_genotype0->SetID(0);
        new_genotype1->SetID(0);
        new_genotype0->SetName("noname");
        new_genotype1->SetName("noname");
        
        batch[batch3].List().PushRear(new_genotype0);
        batch[batch3].List().PushRear(new_genotype1); 
        
        //batch[batch3].List().PushRear(new cAnalyzeGenotype(test_genome0, inst_set));
        //batch[batch3].List().PushRear(new cAnalyzeGenotype(test_genome1, inst_set));
        
      }
    }
  }
}

void cAnalyze::CommandAlign(cString cur_string)
{
  // Align does not need any args yet.
  (void) cur_string;
  
  cout << "Aligning sequences..." << endl;
  
  if (batch[cur_batch].IsLineage() == false && m_world->GetVerbosity() >= VERBOSE_ON) {
    cerr << "  Warning: sequences may not be a consecutive lineage."
    << endl;
  }
  
  // Create an array of all the sequences we need to align.
  tListPlus<cAnalyzeGenotype> & glist = batch[cur_batch].List();
  tListIterator<cAnalyzeGenotype> batch_it(glist);
  const int num_sequences = glist.GetSize();
  cString * sequences = new cString[num_sequences];
  
  // Move through each sequence an update it.
  batch_it.Reset();
  cString diff_info;
  for (int i = 0; i < num_sequences; i++) {
    sequences[i] = batch_it.Next()->GetGenome().AsString();
    if (i == 0) continue;
    // Track of the number of insertions and deletions to shift properly.
    int num_ins = 0;
    int num_del = 0;
    
    // Compare each string to the previous.
    cStringUtil::EditDistance(sequences[i], sequences[i-1], diff_info, '_');
    while (diff_info.GetSize() != 0) {
      cString cur_mut = diff_info.Pop(',');
      const char mut_type = cur_mut[0];
      cur_mut.ClipFront(1); cur_mut.ClipEnd(1);
      int position = cur_mut.AsInt();
      
      // Nothing to do with Mutations
      if (mut_type == 'M') continue;
      
      // Handle insertions...
      if (mut_type == 'I') {
        // Loop back and insert an '_' into all previous sequences.
        for (int j = 0; j < i; j++) {
          sequences[j].Insert('_', position + num_del);
        }
        num_ins++;
      }
      
      // Handle Deletions...
      else if (mut_type == 'D') {
        // Insert '_' into the current sequence at the point of deletions.
        sequences[i].Insert("_", position + num_ins);
        num_del++;
      }
      
    }
  }
  
  batch_it.Reset();
  for (int i = 0; i < num_sequences; i++) {
    batch_it.Next()->SetAlignedSequence(sequences[i]);
  }
  
  // Cleanup
  delete [] sequences;
  
  // Adjust the flags on this batch
  // batch[cur_batch].SetLineage(false);
  batch[cur_batch].SetAligned(true);
}

// Now this command do not consider changing environment 
// and only work for lineage and fixed-length runs.
void cAnalyze::AnalyzeNewInfo(cString cur_string)
{
  cout << "Analyze new information in child about environment ..." << endl;
  
  // Load in the variables
  int words = cur_string.CountNumWords();
  if (words < 1) {
    cout << "This command requires mutation rate, skipping." << endl;
    return;
  }
  
  // Get the mutation rate ...
  double mu = cur_string.PopWord().AsDouble();
  
  // Create the directory using the string given as the second argument
  cString dir = cur_string.PopWord();
  cString defaultDir = "newinfo/";
  cString directory = PopDirectory(dir, defaultDir);
  
  ///////////////////////////////////////////////////////
  // Loop through all of the genotypes in this batch...
  
  if (batch[cur_batch].IsLineage() != true) {
    cout << "This command requires the lineage in the batch, skipping.\n";
    return;
  }
  
  cString newinfo_fn;
  newinfo_fn.Set("%s%s.newinfo.dat", static_cast<const char*>(directory), "lineage");
  ofstream& newinfo_fp = m_world->GetDataFileOFStream(newinfo_fn);
  
  newinfo_fp << "# Legend:" << endl;
  newinfo_fp << "# 1:Child Genotype ID" << endl;
  newinfo_fp << "# 2:Parent Genotype ID" << endl;
  newinfo_fp << "# 3:Information of Child about Environment I(C:E)" << endl;
  newinfo_fp << "# 4:Information of Parent about Environment I(P:E)" << endl;
  newinfo_fp << "# 5:I(C:E)-I(P:E)" << endl;
  newinfo_fp << "# 6:Information Gained in Child" << endl;
  newinfo_fp << "# 7:Information Decreased in Child" << endl;
  newinfo_fp << "# 8:Net Increasing of Information in Child" << endl;
  newinfo_fp << endl; 
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * parent_genotype = batch_it.Next();
  if (parent_genotype == NULL) {
    m_world->GetDataFileManager().Remove(newinfo_fn);
    return;
  }
  cAnalyzeGenotype * child_genotype = NULL;
  double I_P_E; // Information of parent about environment
  double H_P_E = AnalyzeEntropy(parent_genotype, mu);
  I_P_E = parent_genotype->GetLength() - H_P_E;
  
  while ((child_genotype = batch_it.Next()) != NULL) {
    
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "Analyze new information for " << child_genotype->GetName() << endl;
    }
    
    // Information of parent about its environment should not be zero.
    if (I_P_E == 0) {
      cerr << "Error: Information between parent and its enviroment is zero."
      << "(cAnalyze::AnalyzeNewInfo)" << endl;
      if (exit_on_error) exit(1);
    }
    
    double H_C_E = AnalyzeEntropy(child_genotype, mu);
    double I_C_E = child_genotype->GetLength() - H_C_E;
    double net_gain = I_C_E - I_P_E;
    
    // Increased information in child compared to parent
    double child_increased_info = IncreasedInfo(child_genotype, parent_genotype, mu);
    
    // Lost information in child compared to parent
    double child_lost_info = IncreasedInfo(parent_genotype, child_genotype, mu);
    
    // Write information to file ...
    newinfo_fp << child_genotype->GetID() << " ";
    newinfo_fp << parent_genotype->GetID() << " ";
    newinfo_fp << I_C_E << " ";
    newinfo_fp << I_P_E << " ";
    newinfo_fp << net_gain << " ";
    newinfo_fp << child_increased_info << " ";
    newinfo_fp << child_lost_info << " ";
    newinfo_fp << child_increased_info - child_lost_info << endl;
    
    parent_genotype = child_genotype;
    I_P_E = I_C_E;
  }
  
  m_world->GetDataFileManager().Remove(newinfo_fn);
  return;
}



void cAnalyze::WriteClone(cString cur_string)
{
  // Load in the variables...
  cString filename("clone.dat");
  int num_cells = -1;
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  if (cur_string.GetSize() != 0) num_cells = cur_string.PopWord().AsInt();
  
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // Start up again at update zero...
  fp << "0 ";
  
  // Setup the archive sizes of lists to all be zero.
  fp << MAX_CREATURE_SIZE << " ";
  for (int i = 0; i < MAX_CREATURE_SIZE; i++) {
    fp << "0 ";
  }
  
  // Save the individual genotypes
  fp << batch[cur_batch].List().GetSize() << " ";
  
  int org_count = 0;
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    org_count += genotype->GetNumCPUs();
    const int length = genotype->GetLength();
    const cGenome & genome = genotype->GetGenome();
    
    fp << genotype->GetID() << " "
      << length << " ";
    
    for (int i = 0; i < length; i++) {
      fp << genome.GetOp(i) << " ";
    }
  }
  
  // Write out the current state of the grid.
  
  if (num_cells == 0) num_cells = org_count;
  fp << num_cells << " ";
  
  batch_it.Reset();
  while ((genotype = batch_it.Next()) != NULL) {
    for (int i = 0; i < genotype->GetNumCPUs(); i++) {
      fp << genotype->GetID() << " ";
    }
  }
  
  // Fill out the remainder of the grid with -1
  for (int i = org_count; i < num_cells; i++) {
    fp << "-1 ";
  }
}


void cAnalyze::WriteInjectEvents(cString cur_string)
{
  // Load in the variables...
  cString filename("events_inj.cfg");
  int start_cell = 0;
  int lineage = 0;
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  if (cur_string.GetSize() != 0) start_cell = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() != 0) lineage = cur_string.PopWord().AsInt();
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  int org_count = 0;
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    const int cur_count = genotype->GetNumCPUs();
    org_count += cur_count;
    const cGenome & genome = genotype->GetGenome();
    
    fp << "u 0 inject_sequence "
      << genome.AsString() << " "
      << start_cell << " "
      << start_cell + cur_count << " "
      << genotype->GetMerit() << " "
      << lineage << " "
      << endl;
    start_cell += cur_count;
  }
}


void cAnalyze::WriteCompetition(cString cur_string)
{
  cout << "Writing Competition events..." << endl;
  
  // Load in the variables...
  int join_UD = 0;
  double start_merit = 50000;
  cString filename("events_comp.cfg");
  int batch_A = cur_batch - 1;
  int batch_B = cur_batch;
  int grid_side = -1;
  int lineage = 0;
  
  // Make sure we have reasonable default batches.
  if (cur_batch == 0) { batch_A = 0; batch_B = 1; }
  
  if (cur_string.GetSize() != 0) join_UD = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() != 0) start_merit = cur_string.PopWord().AsDouble();
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  if (cur_string.GetSize() != 0) batch_A = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() != 0) batch_B = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() != 0) grid_side = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() != 0) lineage = cur_string.PopWord().AsInt();
  
  // Check inputs...
  if (join_UD < 0) join_UD = 0;
  if (batch_A < 0 || batch_B < 0) {
    cerr << "Error: Batch IDs must be positive!" << endl;
    return;
  }
  
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // Count the number of organisms in each batch...
  cAnalyzeGenotype * genotype = NULL;
  
  int org_count_A = 0;
  tListIterator<cAnalyzeGenotype> batchA_it(batch[batch_A].List());
  while ((genotype = batchA_it.Next()) != NULL) {
    org_count_A += genotype->GetNumCPUs();
  }
  
  int org_count_B = 0;
  tListIterator<cAnalyzeGenotype> batchB_it(batch[batch_B].List());
  while ((genotype = batchB_it.Next()) != NULL) {
    org_count_B += genotype->GetNumCPUs();
  }
  
  int max_count = Max(org_count_A, org_count_B);
  if (max_count > 10000) {
    cout << "Warning: more than 10,000 organisms in sub-population!" << endl;
  }
  
  if (grid_side <= 0) {
    for (grid_side = 5; grid_side < 100; grid_side += 5) {
      if (grid_side * grid_side >= max_count) break;
    }
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "...assuming population size "
      << grid_side << "x" << grid_side << "." << endl;
    }
  }
  
  
  int pop_size = grid_side * grid_side;
  
  int inject_pos = 0;
  while ((genotype = batchA_it.Next()) != NULL) {
    const int cur_count = genotype->GetNumCPUs();
    const cGenome & genome = genotype->GetGenome();
    double cur_merit = start_merit;
    if (cur_merit < 0) cur_merit = genotype->GetMerit();
    fp << "u 0 inject_sequence "
      << genome.AsString() << " "
      << inject_pos << " "
      << inject_pos + cur_count << " "
      << cur_merit << " "
      << lineage << " "
      << endl;
    inject_pos += cur_count;
  }
  
  inject_pos = pop_size;
  while ((genotype = batchB_it.Next()) != NULL) {
    const int cur_count = genotype->GetNumCPUs();
    const cGenome & genome = genotype->GetGenome();
    double cur_merit = start_merit;
    if (cur_merit < 0) cur_merit = genotype->GetMerit();
    fp << "u 0 inject_sequence "
      << genome.AsString() << " "
      << inject_pos << " "
      << inject_pos + cur_count << " "
      << cur_merit << " "
      << lineage+1 << " "
      << endl;
    inject_pos += cur_count;
  }
  
  fp << "u 0 sever_grid_row" << grid_side << endl;
  fp << "u " << join_UD << " join_grid_row " << grid_side << endl;
}


// Analyze the mutations along an aligned lineage.

void cAnalyze::AnalyzeMuts(cString cur_string)
{
  cout << "Analyzing Mutations" << endl;
  
  // Make sure we have everything we need.
  if (batch[cur_batch].IsAligned() == false) {
    cout << "  Error: sequences not aligned." << endl;
    return;
  }
  
  // Setup variables...
  cString filename("analyze_muts.dat");
  bool all_combos = false;
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  if (cur_string.GetSize() != 0) all_combos = cur_string.PopWord().AsInt();
  
  tListPlus<cAnalyzeGenotype> & gen_list = batch[cur_batch].List();
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  
  const int num_sequences = gen_list.GetSize();
  const int sequence_length =
    gen_list.GetFirst()->GetAlignedSequence().GetSize();
  cString * sequences = new cString[num_sequences];
  int * mut_count = new int[sequence_length];
  for (int i = 0; i < sequence_length; i++) mut_count[i] = 0;
  
  // Load in the sequences
  batch_it.Reset();
  int count = 0;
  while (batch_it.Next() != NULL) {
    sequences[count] = batch_it.Get()->GetAlignedSequence();
    count++;
  }
  
  // Count the number of changes at each site...
  for (int i = 1; i < num_sequences; i++) {       // For each pair...
    cString & seq1 = sequences[i-1];
    cString & seq2 = sequences[i];
    for (int j = 0; j < sequence_length; j++) {   // For each site...
      if (seq1[j] != seq2[j]) mut_count[j]++;
    }
  }
  
  // Grab the two strings we're actively going to be working with.
  cString & first_seq = sequences[0];
  cString & last_seq = sequences[num_sequences - 1];
  
  // Print out the header...
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  fp << "# " << sequences[0] << endl;
  fp << "# " << sequences[num_sequences - 1] << endl;
  fp << "# ";
  for (int i = 0; i < sequence_length; i++) {
    if (mut_count[i] == 0) fp << " ";
    else if (mut_count[i] > 9) fp << "+";
    else fp << mut_count[i];
  }
  fp << endl;
  fp << "# ";
  for (int i = 0; i < sequence_length; i++) {
    if (first_seq[i] == last_seq[i]) fp << " ";
    else fp << "^";
  }
  fp << endl << endl;
  
  // Count the number of diffs between the two strings we're interested in.
  const int total_diffs = cStringUtil::Distance(first_seq, last_seq);
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "  " << total_diffs << " mutations being tested." << endl;
  
  // Locate each difference.
  int * mut_positions = new int[total_diffs];
  int cur_mut = 0;
  for (int i = 0; i < first_seq.GetSize(); i++) {
    if (first_seq[i] != last_seq[i]) {
      mut_positions[cur_mut] = i;
      cur_mut++;
    }
  }
  
  // The number of mutations we need to deal with will tell us how much
  // we can attempt to do. (@CAO should be able to overide defaults)
  bool scan_combos = true;  // Scan all possible combos of mutations?
  bool detail_muts = true;  // Collect detailed info on all mutations?
  bool print_all = true;    // Print everything we collect without digestion?
  if (total_diffs > 30) scan_combos = false;
  if (total_diffs > 20) detail_muts = false;
  if (total_diffs > 10) print_all = false;
  
  // Start moving through the difference combinations...
  if (scan_combos) {
    const int total_combos = 1 << total_diffs;
    cout << "  Scanning through " << total_combos << " combos." << endl;
    
    double * total_fitness = new double[total_diffs + 1];
    double * total_sqr_fitness = new double[total_diffs + 1];
    double * max_fitness = new double[total_diffs + 1];
    cString * max_sequence = new cString[total_diffs + 1];
    int * test_count = new int[total_diffs + 1];
    for (int i = 0; i <= total_diffs; i++) {
      total_fitness[i] = 0.0;
      total_sqr_fitness[i] = 0.0;
      max_fitness[i] = 0.0;
      test_count[i] = 0;
    }
    
    cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();
    
    // Loop through all of the combos...
    const int combo_step = total_combos / 79;
    for (int combo_id = 0; combo_id < total_combos; combo_id++) {
      if (combo_id % combo_step == 0) {
        cout << '.';
        cout.flush();
      }
      // Start at the first sequence and add needed changes...
      cString test_sequence = first_seq;
      int diff_count = 0;
      for (int mut_id = 0; mut_id < total_diffs; mut_id++) {
        if ((combo_id >> mut_id) & 1) {
          const int cur_pos = mut_positions[mut_id];
          test_sequence[cur_pos] = static_cast<const char*>(last_seq)[cur_pos];
          diff_count++;
        }
      }
      
      // Determine the fitness of the current sequence...
      cGenome test_genome(test_sequence);
      cCPUTestInfo test_info;
      testcpu->TestGenome(m_ctx, test_info, test_genome);
      const double fitness = test_info.GetGenotypeFitness();
      
      //cAnalyzeGenotype test_genotype(test_sequence);
      //test_genotype.Recalculate(m_ctx, testcpu);
      //const double fitness = test_genotype.GetFitness();
      
      total_fitness[diff_count] += fitness;
      total_sqr_fitness[diff_count] += fitness * fitness;
      if (fitness > max_fitness[diff_count]) {
        max_fitness[diff_count] = fitness;
        max_sequence[diff_count] = test_sequence;
        //  	cout << endl
        //  	     << max_sequence[diff_count] << " "
        //  	     << test_info.GetGenotypeMerit() << " "
        //  	     << fitness << " "
        //  	     << combo_id << endl;
      }
      test_count[diff_count]++;
    }
    
    // Output the results...
    
    for (int i = 0; i <= total_diffs; i++) {
      cAnalyzeGenotype max_genotype(m_world, max_sequence[i], inst_set);
      max_genotype.Recalculate(m_ctx);
      fp << i                                         << " "  //  1
        << test_count[i]                             << " "  //  2
        << total_fitness[i] / (double) test_count[i] << " "  //  3
        << max_fitness[i]                            << " "  //  4
        << max_genotype.GetMerit()                   << " "  //  5
        << max_genotype.GetGestTime()                << " "  //  6
        << max_genotype.GetLength()                  << " "  //  7
        << max_genotype.GetCopyLength()              << " "  //  8
        << max_genotype.GetExeLength()               << " "; //  9
      max_genotype.PrintTasks(fp, 3,12);
      fp << max_sequence[i] << endl;
    }
    
    // Cleanup
    delete [] total_fitness;
    delete [] total_sqr_fitness;
    delete [] max_fitness;
    delete [] max_sequence;
    delete [] test_count;
    
    delete testcpu;
  }
  // If we can't scan through all combos, give wanring.
  else {
    cerr << "  Warning: too many mutations (" << total_diffs
    << ") to scan through combos." << endl;
  }
  
  
  // Cleanup...
  delete [] sequences;
  delete [] mut_count;
  delete [] mut_positions;
}


// Analyze the frequency that each instruction appears in the batch, and
// make note of those that appear more or less often than expected.

void cAnalyze::AnalyzeInstructions(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Analyzing Instructions in batch " << cur_batch << endl;
  }
  else cout << "Analyzing Instructions..." << endl;
  
  // Load in the variables...
  cString filename("inst_analyze.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  const int num_insts = inst_set.GetSize();
  
  // Setup the file...
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // Determine the file type...
  int file_type = FILE_TYPE_TEXT;
  while (filename.Find('.') != -1) filename.Pop('.');
  if (filename == "html") file_type = FILE_TYPE_HTML;
  
  // If we're in HTML mode, setup the header...
  if (file_type == FILE_TYPE_HTML) {
    // Document header...
    fp << "<html>" << endl
    << "<body bgcolor=\"#FFFFFF\"" << endl
    << " text=\"#000000\"" << endl
    << " link=\"#0000AA\"" << endl
    << " alink=\"#0000FF\"" << endl
    << " vlink=\"#000044\">" << endl
    << endl
    << "<h1 align=center>Instruction Chart: "
    << batch[cur_batch].Name() << endl
    << "<br><br>" << endl
    << endl;
    
    // Instruction key...
    const int num_cols = 6;
    const int num_rows = ((num_insts - 1) / num_cols) + 1;
    fp << "<table border=2 cellpadding=3>" << endl
      << "<tr bgcolor=\"#AAAAFF\"><th colspan=6>Instruction Set Legend</tr>"
      << endl;
    for (int i = 0; i < num_rows; i++) {
      fp << "<tr>";
      for (int j = 0; j < num_cols; j++) {
        const int inst_id = i + j * num_rows;
        if (inst_id < num_insts) {
          cInstruction cur_inst(inst_id);
          fp << "<td><b>" << cur_inst.GetSymbol() << "</b> : "
            << inst_set.GetName(inst_id) << " ";
        }
        else {
          fp << "<td>&nbsp; ";
        }
      }
      fp << "</tr>" << endl;
    }
    fp << "</table>" << endl
      << "<br><br><br>" << endl;
    
    // Main table header...
    fp << "<center>" << endl
      << "<table border=1 cellpadding=2>" << endl
      << "<tr><th bgcolor=\"#AAAAFF\">Run # <th bgcolor=\"#AAAAFF\">Length"
      << endl;
    for (int i = 0; i < num_insts; i++) {
      cInstruction cur_inst(i);
      fp << "<th bgcolor=\"#AAAAFF\">" << cur_inst.GetSymbol() << " ";
    }
    fp << "</tr>" << endl;
  }
  else { // if (file_type == FILE_TYPE_TEXT) {
    fp << "#RUN_NAME  LENGTH  ";
    for (int i = 0; i < num_insts; i++) {
      cInstruction cur_inst(i);
      fp << cur_inst.GetSymbol() << ":" << inst_set.GetName(i) << " ";
    }
    fp << endl;
  }
  
  // Figure out how often we expect each instruction to appear...
  const double exp_freq = 1.0 / (double) num_insts;
  const double min_freq = exp_freq * 0.5;
  const double max_freq = exp_freq * 2.0;
  
  double total_length = 0.0;
  tArray<double> total_freq(num_insts);
  for (int i = 0; i < num_insts; i++) total_freq[i] = 0.0;
  
  // Loop through all of the genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    // Setup for counting...
    tArray<int> inst_bin(num_insts);
    for (int i = 0; i < num_insts; i++) inst_bin[i] = 0;
    
    // Count it up!
    const int genome_size = genotype->GetLength();
    for (int i = 0; i < genome_size; i++) {
      const int inst_id = genotype->GetGenome().GetOp(i);
      inst_bin[inst_id]++;
    }
    
    // Print it out...
    if (file_type == FILE_TYPE_HTML) fp << "<tr><th>";
    fp << genotype->GetName() << " ";
    if (file_type == FILE_TYPE_HTML) fp << "<td align=center>";
    total_length += genome_size;
    fp << genome_size << " ";
    for (int i = 0; i < num_insts; i++) {
      const double inst_freq = ((double) inst_bin[i]) / (double) genome_size;
      total_freq[i] += inst_freq;
      if (file_type == FILE_TYPE_HTML) {
        if (inst_freq == 0.0) fp << "<td bgcolor=\"FFAAAA\">";
        else if (inst_freq < min_freq) fp << "<td bgcolor=\"FFFFAA\">";
        else if (inst_freq < max_freq) fp << "<td bgcolor=\"AAAAFF\">";
        else fp << "<td bgcolor=\"AAFFAA\">";
      }
      fp << cStringUtil::Stringf("%04.3f", inst_freq) << " ";
    }
    if (file_type == FILE_TYPE_HTML) fp << "</tr>";
    fp << endl;
  }
  
  if (file_type == FILE_TYPE_HTML) {
    int num_genomes = batch[cur_batch].List().GetSize();
    fp << "<tr><th>Average <th>" << total_length / num_genomes << " ";
    for (int i = 0; i < num_insts; i++) {
      double inst_freq = total_freq[i] / num_genomes;
      if (inst_freq == 0.0) fp << "<td bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_LETHAL.Get() << "\">";
      else if (inst_freq < min_freq) fp << "<td bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_NEG.Get() << "\">";
      else if (inst_freq < max_freq) fp << "<td bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_NEUT.Get() << "\">";
      else fp << "<td bgcolor=\"#" << m_world->GetConfig().COLOR_MUT_POS.Get() << "\">";
      fp << cStringUtil::Stringf("%04.3f", inst_freq) << " ";
    }
    fp << "</tr>" << endl
      << "</table></center>" << endl;
  }
  }

void cAnalyze::AnalyzeInstPop(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Analyzing Instructions in batch " << cur_batch << endl;
  }
  else cout << "Analyzeing Instructions..." << endl;
  
  // Load in the variables...
  cString filename("inst_analyze.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  const int num_insts = inst_set.GetSize();
  
  // Setup the file...
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  for (int i = 0; i < num_insts; i++) {
    cInstruction cur_inst(i);
    fp << cur_inst.GetSymbol() << ":" << inst_set.GetName(i) << " ";
  }
  fp << endl;
  
  double total_length = 0.0;
  tArray<double> total_freq(num_insts);
  for (int i = 0; i < num_insts; i++) total_freq[i] = 0.0;
  int num_orgs = 0; 
  
  // Loop through all of the genotypes in this batch...
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    
    num_orgs++; 
    
    // Setup for counting...
    tArray<int> inst_bin(num_insts);
    for (int i = 0; i < num_insts; i++) inst_bin[i] = 0;
    
    // Count it up!
    const int genome_size = genotype->GetLength();
    for (int i = 0; i < genome_size; i++) {
      const int inst_id = genotype->GetGenome().GetOp(i);
      inst_bin[inst_id]++;
    }
    total_length += genome_size;
    for (int i = 0; i < num_insts; i++) {
      const double inst_freq = ((double) inst_bin[i]) / (double) genome_size;
      total_freq[i] += inst_freq;
    }
  }
  // Print it out...
  //    fp << total_length/num_orgs  << " ";
  for (int i = 0; i < num_insts; i++) {
    fp << cStringUtil::Stringf("%04.3f", total_freq[i]/num_orgs) << " ";
  }
  fp << endl;
  
}

void cAnalyze::AnalyzeBranching(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Analyzing branching patterns in batch " << cur_batch << endl;
  }
  else cout << "Analyzeing Branches..." << endl;
  
  // Load in the variables...
  cString filename("branch_analyze.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  // Setup the file...
  //ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  // UNFINISHED!
  // const int num_insts = inst_set.GetSize();
}

void cAnalyze::AnalyzeMutationTraceback(cString cur_string)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Analyzing mutation traceback in batch " << cur_batch << endl;
  }
  else cout << "Analyzing mutation traceback..." << endl;
  
  // This works best on lineages, so warn if we don't have one.
  if (batch[cur_batch].IsLineage() == false && m_world->GetVerbosity() >= VERBOSE_ON) {
    cerr << "  Warning: trying to traceback mutations outside of lineage."
    << endl;
  }
  
  if (batch[cur_batch].List().GetSize() == 0) {
    cerr << "Error: Trying to traceback mutations with no genotypes in batch."
    << endl;
    return;
  }
  
  // Make sure all genotypes are the same length.
  int size = -1;
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    if (size == -1) size = genotype->GetLength();
    if (size != genotype->GetLength()) {
      cerr << "  Error: Trying to traceback mutations in genotypes of differing lengths." << endl;
      cerr << "  Aborting." << endl;
      return;
    }
  }
  
  // Setup variables...
  cString filename("analyze_traceback.dat");
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  
  // Setup a genome to store the previous values before mutations.
  tArray<int> prev_inst(size);
  prev_inst.SetAll(-1);  // -1 indicates never changed.
  
  // Open the output file...
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();
  
  // Loop through all of the genotypes again, testing mutation reversions.
  cAnalyzeGenotype * prev_genotype = batch_it.Next();
  while ((genotype = batch_it.Next()) != NULL) {
    continue;
    // Check to see if any sites have changed...
    for (int i = 0; i < size; i++) {
      if (genotype->GetGenome().GetInstruction(i) != prev_genotype->GetGenome().GetInstruction(i)) {
        prev_inst[i] = prev_genotype->GetGenome().GetOp(i);
      }
    }
    
    // Next, determine the fraction of mutations that are currently adaptive.
    int num_beneficial = 0;
    int num_neutral = 0;
    int num_detrimental = 0;
    int num_static = 0;      // Sites that were never mutated.
    
    cGenome test_genome = genotype->GetGenome();
    cCPUTestInfo test_info;
    testcpu->TestGenome(m_ctx, test_info, test_genome);
    const double base_fitness = test_info.GetGenotypeFitness();
    
    for (int i = 0; i < size; i++) {
      if (prev_inst[i] == -1) num_static++;
      else {
        test_genome.SetOp(i, prev_inst[i]);
        testcpu->TestGenome(m_ctx, test_info, test_genome);
        const double cur_fitness = test_info.GetGenotypeFitness();
        if (cur_fitness > base_fitness) num_detrimental++;
        else if (cur_fitness < base_fitness) num_beneficial++;
        else num_neutral++;
        test_genome.SetInstruction(i, genotype->GetGenome().GetInstruction(i));
      }      
    }
    
    fp << genotype->GetDepth() << " "
      << num_beneficial << " "
      << num_neutral << " "
      << num_detrimental << " "
      << num_static << " "
      << endl;
    
    prev_genotype = genotype;
  }
  
  delete testcpu;
}

void cAnalyze::AnalyzeComplexity(cString cur_string)
{
  cout << "Analyzing genome complexity..." << endl;
  
  // Load in the variables...
  // This command requires at least on arguement
  int words = cur_string.CountNumWords();
  if(words < 1) {
    cout << "Error: AnalyzeComplexity has no parameters, skipping." << endl;
    return;
  }
  
  // Get the mutation rate arguement
  double mut_rate = cur_string.PopWord().AsDouble();
  
  // Create the directory using the string given as the second arguement
  cString dir = cur_string.PopWord();
  cString defaultDirectory = "complexity/";
  cString directory = PopDirectory(dir, defaultDirectory);
  
  // Default for usage of resources is false
  int useResources = 0;
  // resource usage flag is an optional arguement, but is always the 3rd arg
  if(words >= 3) {
    useResources = cur_string.PopWord().AsInt();
    // All non-zero values are considered false (Handled by testcpu->InitResources)
  }
  
  // Batch frequency begins with the first organism, but then skips that 
  // amount ahead in the batch.  It defaults to 1, so that default analyzes
  // all the organisms in the batch.  It is always the 4th arg.
  int batchFrequency = 1;
  if(words == 4) {
    batchFrequency = cur_string.PopWord().AsInt();
    if(batchFrequency <= 0) {
      batchFrequency = 1;
    }
  }
  
  cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();
  
  ///////////////////////////////////////////////////////
  // Loop through all of the genotypes in this batch...
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  
  cString lineage_filename;
  if (batch[cur_batch].IsLineage()) {
    lineage_filename.Set("%s%s.complexity.dat", static_cast<const char*>(directory), "lineage");
  } else {
    lineage_filename.Set("%s%s.complexity.dat", static_cast<const char*>(directory), "nonlineage");
  }
  ofstream& lineage_fp = m_world->GetDataFileOFStream(lineage_filename);
  
  while ((genotype = batch_it.Next()) != NULL) {
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "  Analyzing complexity for " << genotype->GetName() << endl;
    }
    
    // Construct this filename...
    cString filename;
    filename.Set("%s%s.complexity.dat", static_cast<const char*>(directory), static_cast<const char*>(genotype->GetName()));
    ofstream& fp = m_world->GetDataFileOFStream(filename);
    
    lineage_fp << genotype->GetID() << " ";
    
    int updateBorn = -1;
    updateBorn = genotype->GetUpdateBorn();
    cCPUTestInfo test_info;
    test_info.SetResourceOptions(useResources, &resources, updateBorn, m_resource_time_spent_offset);
    
    // Calculate the stats for the genotype we're working with ...
    genotype->Recalculate(m_ctx, &test_info);
    cout << genotype->GetFitness() << endl;
    const int num_insts = inst_set.GetSize();
    const int max_line = genotype->GetLength();
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);
    
    // Loop through all the lines of code, testing all mutations...
    tArray<double> test_fitness(num_insts);
    tArray<double> prob(num_insts);
    for (int line_num = 0; line_num < max_line; line_num++) {
      int cur_inst = base_genome.GetOp(line_num);
      //char cur_symbol = base_genome[line_num].GetSymbol();
      
      // Column 1 ... the original instruction in the genome.
      fp << cur_inst << " ";
      
      // Test fitness of each mutant.
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) {
        mod_genome.SetOp(line_num, mod_inst);
        cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
        test_genotype.Recalculate(m_ctx);
        test_fitness[mod_inst] = test_genotype.GetFitness();
      }
      
      // Ajust fitness
      double cur_inst_fitness = test_fitness[cur_inst];
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) {
        if (test_fitness[mod_inst] > cur_inst_fitness)
          test_fitness[mod_inst] = cur_inst_fitness;
        test_fitness[mod_inst] = test_fitness[mod_inst] / cur_inst_fitness;
      }
      
      // Calculate probabilities at mut-sel balance
      double w_bar = 1;
      
      // Normalize fitness values, assert if they are all zero
      double maxFitness = 0.0;
      for(int i=0; i<num_insts; i++) {
        if(test_fitness[i] > maxFitness) {
          maxFitness = test_fitness[i];
        }
      }
      
      if(maxFitness > 0) {
        for(int i=0; i<num_insts; i++) {
          test_fitness[i] /= maxFitness;
        }
      } else {
        fp << "All zero fitness, ERROR." << endl;
        continue;
      }
      
      while(1) {
        double sum = 0.0;
        for (int mod_inst = 0; mod_inst < num_insts; mod_inst ++) {
          prob[mod_inst] = (mut_rate * w_bar) /
          ((double)num_insts * (w_bar + test_fitness[mod_inst] * mut_rate - test_fitness[mod_inst]));
          sum = sum + prob[mod_inst];
        }
        if ((sum-1.0)*(sum-1.0) <= 0.0001) 
          break;
        else
          w_bar = w_bar - 0.000001;
      }
      // Write probability
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst ++) {
        fp << prob[mod_inst] << " ";
      }
      
      // Calculate complexity
      double entropy = 0;
      for (int i = 0; i < num_insts; i ++) {
        entropy += prob[i] * log((double) 1.0/prob[i]) / log ((double) num_insts);
      }
      double complexity = 1 - entropy;
      fp << complexity << endl;
      
      lineage_fp << complexity << " ";
      
      // Reset the mod_genome back to the original sequence.
      mod_genome.SetOp(line_num, cur_inst);
    }
    
    m_world->GetDataFileManager().Remove(filename);
    
    lineage_fp << endl;
    
    // Always grabs the first one
    // Skip i-1 times, so that the beginning of the loop will grab the ith one
    // where i is the batchFrequency
    for(int count=0; genotype != NULL && count < batchFrequency - 1; count++) {
      genotype = batch_it.Next();
      if(genotype != NULL && m_world->GetVerbosity() >= VERBOSE_ON) {
        cout << "Skipping: " << genotype->GetName() << endl;
      }
    }
    if(genotype == NULL) { break; }
  }
  
  m_world->GetDataFileManager().Remove(lineage_filename);
  
  delete testcpu;
}

void cAnalyze::AnalyzeFitnessLandscapeTwoSites(cString cur_string)
{
  cout << "Fitness for all instruction combinations at two sites..." << endl;
  
  /*
   * Arguments:
   * 1) directory (default: 'fitness_landscape_two_sites/'
   * 2) useResources (default: 0 -- no)
   * 3) batchFrequency (default: 1 -- all genotypes in batch)
   * 
   */
  
  // number of arguments provided  
  int words = cur_string.CountNumWords();
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  Number of arguments passed: " << words << endl;
  }
  
  //
  // argument 1 -- directory
  //
  cString dir = cur_string.PopWord();
  cString defaultDirectory = "fitness_landscape_two_sites/";
  cString directory = PopDirectory(dir, defaultDirectory);
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  - Analysis results to directory: " << directory << endl;
  }

  //
  // argument 2 -- use resources?
  //
  // Default for usage of resources is false
  int useResources = 0;
  if(words >= 2) {
    useResources = cur_string.PopWord().AsInt();
    // All non-zero values are considered false (Handled by testcpu->InitResources)
  }
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  - Use resorces set to: " << useResources << " (0=false, true other int)" << endl;
  }
  
  //
  // argument 3 -- batch frequncy
  //   - default batchFrequency=1 (every organism analyzed)
  //
  int batchFrequency = 1;
  if(words >= 3) {
    batchFrequency = cur_string.PopWord().AsInt();
    if(batchFrequency <= 0) {
      batchFrequency = 1;
    }
  }
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  - Batch frequency set to: " << batchFrequency << endl;
  }

  // test cpu
  //cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();
  
  // get current batch
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
    
  // analyze each genotype in the batch
  while ((genotype = batch_it.Next()) != NULL) {
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "  Analyzing complexity for " << genotype->GetName() << endl;
    }
    
    int updateBorn = -1;
    updateBorn = genotype->GetUpdateBorn();
    cCPUTestInfo test_info;
    test_info.SetResourceOptions(useResources, &resources, updateBorn, m_resource_time_spent_offset);
    
    // Calculate the stats for the genotype we're working with ...
    genotype->Recalculate(m_ctx, &test_info);
    const int num_insts = inst_set.GetSize();
    const int max_line = genotype->GetLength();
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);

    // run throught sites in genome
    for (int site1 = 0; site1 < max_line; site1++) {
      for (int site2 = site1+1; site2 < max_line; site2++) {
        
        // Construct filename for this site combination
        cString fl_filename;
        fl_filename.Set("%s%s_FitLand_sites-%d_and_%d.dat", static_cast<const char*>(directory), static_cast<const char*>(genotype->GetName()), site1, site2);
        cDataFile & fit_land_fp = m_world->GetDataFile(fl_filename);
        fit_land_fp.WriteComment( "Two-site fitness landscape, all possible instructions" );
        fit_land_fp.WriteComment( cStringUtil::Stringf("Site 1: %d Site 2: %d", site1, site2) );
        fit_land_fp.WriteComment( "Rows #- instruction, site 1" );
        fit_land_fp.WriteComment( "Columns #- instruction, site 2" );
        fit_land_fp.WriteTimeStamp();

        // get current instructions at site 1 and site 2
        int curr_inst1 = base_genome.GetOp(site1);
        int curr_inst2 = base_genome.GetOp(site2);
      
        // get current fitness
        //double curr_fitness = genotype->GetFitness();
        
        // run through all possible instruction combinations
        // at two sites
        for (int mod_inst1 = 0; mod_inst1 < num_insts; mod_inst1++) {
          for (int mod_inst2 = 0; mod_inst2 < num_insts; mod_inst2++) {
            // modify mod_genome at two sites
            mod_genome.SetOp(site1, mod_inst1);
            mod_genome.SetOp(site2, mod_inst2);
            // analyze mod_genome
            cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
            test_genotype.Recalculate(m_ctx);
            double mod_fitness = test_genotype.GetFitness();
             
            // write to file
            fit_land_fp.Write(mod_fitness, cStringUtil::Stringf("Instruction, site 2: %d ", mod_inst2));
          }
          fit_land_fp.Endl();
        }   
        // Reset the mod_genome back to the original sequence.
        mod_genome.SetOp(site1, curr_inst1);
        mod_genome.SetOp(site2, curr_inst2);
        
        // close file
        m_world->GetDataFileManager().Remove(fl_filename);
      }
    }
  }  
}

void cAnalyze::AnalyzeComplexityTwoSites(cString cur_string)
{
  cout << "Analyzing genome complexity (one and two sites)..." << endl;
  
  /*
   * Arguments:
   * 1) mutation rate (default: 0.0 - selection only)
   * 2) directory for results (default: 'complexity_two_sites/'
   * 3) use resources ? -- 0 or 1 (default: 0)
   * 4) batch frequency (default: 1 - all genotypes in batch)
   *    -- how many genotypes to skip in batch
   * 5) convergence accuracy (default: 1.e-10)
   * 
   */
  
  // number of arguments provided  
  int words = cur_string.CountNumWords();
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  Number of arguments passed: " << words << endl;
  }
  
  //
  // argument 1 -- mutation rate 
  //
  double mut_rate = 0.0075;
  if(words < 1) {
    // no mutation rate provided
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "  - No mutation rate passed, using default mu = " << mut_rate << endl;
    }
  } else {
    // mutation rate provided
    mut_rate = cur_string.PopWord().AsDouble();
    if (mut_rate < 0.0) {
      // can't have mutation rate below zero
      mut_rate = 0.0; 
    }
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "  - Mutation rate passed, using mu = " << mut_rate << endl;
    }  
  }
  
  //
  // argument 2 -- directory
  //
  cString dir = cur_string.PopWord();
  cString defaultDirectory = "complexity_two_sites/";
  cString directory = PopDirectory(dir, defaultDirectory);
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  - Analysis results to directory: " << directory << endl;
  }
  
  //
  // argument 3 -- use resources?
  //
  // Default for usage of resources is false
  int useResources = 0;
  if(words >= 3) {
    useResources = cur_string.PopWord().AsInt();
    // All non-zero values are considered false (Handled by testcpu->InitResources)
  }
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  - Use resorces set to: " << useResources << " (0=false, true other int)" << endl;
  }
  
  //
  // argument 4 -- batch frequncy
  //   - default batchFrequency=1 (every organism analyzed)
  //
  int batchFrequency = 1;
  if(words >= 4) {
    batchFrequency = cur_string.PopWord().AsInt();
    if(batchFrequency <= 0) {
      batchFrequency = 1;
    }
  }
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  - Batch frequency set to: " << batchFrequency << endl;
  }
  
  //
  // argument 5 -- convergence accuracy for mutation-selection balance
  //
  double converg_accuracy = 1.e-10;
  if(words >= 5) {
    converg_accuracy = cur_string.PopWord().AsDouble();
  }
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "  - Convergence accuracy: " << converg_accuracy << endl;
  }

  // test cpu
  cTestCPU* testcpu = m_world->GetHardwareManager().CreateTestCPU();
  
  // get current batch
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  
  // create file for batch summary
  cString summary_filename;
  summary_filename.Set("%scomplexity_batch_summary.dat", static_cast<const char*>(directory));
  cDataFile & summary_fp = m_world->GetDataFile(summary_filename);
  summary_fp.WriteComment( "One, Two Site Entropy/Complexity Analysis" );
  summary_fp.WriteTimeStamp();  
  
  // analyze each genotype in the batch
  while ((genotype = batch_it.Next()) != NULL) {
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "  Analyzing complexity for " << genotype->GetName() << endl;
    }
    // entropy and complexity for whole genome
    // in both mers and bits
    // >> single site approximation
    double genome_ss_entropy_mers = 0.0;
    double genome_ss_entropy_bits = 0.0;
    double genome_ss_complexity_mers = 0.0;
    double genome_ss_complexity_bits = 0.0;
    // >> two site approximation
    double genome_ds_mut_info_mers = 0.0;
    double genome_ds_mut_info_bits = 0.0;
    double genome_ds_complexity_mers = 0.0;
    double genome_ds_complexity_bits = 0.0;
    
    // Construct filename
    cString filename_2s;
    filename_2s.Set("%s%s.twosite.complexity.dat", static_cast<const char*>(directory), static_cast<const char*>(genotype->GetName()));
    cDataFile & fp_2s = m_world->GetDataFile(filename_2s);
    fp_2s.WriteComment( "One, Two Site Entropy/Complexity Analysis" );
    fp_2s.WriteComment( "NOTE: mutual information = (col 6 + col 8) - (col 9)" );
    fp_2s.WriteComment( "NOTE: possible negative mutual information-- is this real? " );
    fp_2s.WriteTimeStamp();
        
    int updateBorn = -1;
    updateBorn = genotype->GetUpdateBorn();
    cCPUTestInfo test_info;
    test_info.SetResourceOptions(useResources, &resources, updateBorn, m_resource_time_spent_offset);
    
    // Calculate the stats for the genotype we're working with ...
    genotype->Recalculate(m_ctx, &test_info);
    const int num_insts = inst_set.GetSize();
    const int max_line = genotype->GetLength();
    const cGenome & base_genome = genotype->GetGenome();
    cGenome mod_genome(base_genome);
    
    /*
     * 
     *  ONE SITE CALCULATIONS
     * 
     */
     
    // single site entropies for use with
    // two site calculations (below)
    tArray<double> entropy_ss_mers(max_line);
    tArray<double> entropy_ss_bits(max_line);
    // used in single site calculations
    tArray<double> test_fitness(num_insts);
    tArray<double> prob(num_insts);
    tArray<double> prob_next(num_insts);
    
    // run through lines in genome
    for (int line_num = 0; line_num < max_line; line_num++) {
      // get the current instruction at this line/site
      int cur_inst = base_genome.GetOp(line_num);
      
      // recalculate fitness of each mutant.
      for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) {
        mod_genome.SetOp(line_num, mod_inst);
        cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
        test_genotype.Recalculate(m_ctx);
        test_fitness[mod_inst] = test_genotype.GetFitness();
      }
      
      // Adjust fitness
      // - set all fitness values greater than current instruction
      // equal to current instruction fitness
      // - make the rest of the fitness values relative to 
      // the current instruction fitness
      double cur_inst_fitness = test_fitness[cur_inst];
      // test that current fitness greater than zero
      // if NOT, all fitnesses will be set to zero
      if (cur_inst_fitness > 0.0) {
        for (int mod_inst = 0; mod_inst < num_insts; mod_inst++) {
          if (test_fitness[mod_inst] > cur_inst_fitness)
            test_fitness[mod_inst] = cur_inst_fitness;
          test_fitness[mod_inst] /= cur_inst_fitness;
        }
      } else {
        cout << "Fitness of this genotype is ZERO--no information." << endl;
        continue;
      }
           
      // initialize prob for
      // mutation-selection balance
      double fitness_total = 0.0;
      for (int i = 0; i < num_insts; i ++ ) {
        fitness_total += test_fitness[i];
      }
      for (int i = 0; i < num_insts; i ++ ) {
        prob[i] = test_fitness[i]/fitness_total;
        prob_next[i] = 0.0;
      }
      
      double check_sum = 0.0;
      while(1) {
        check_sum = 0.0;
        double delta_prob = 0.0;
        //double delta_prob_ex = 0.0;
        for (int mod_inst = 0; mod_inst < num_insts; mod_inst ++) {  
          // calculate the average fitness
          double w_avg = 0.0;
          for (int i = 0; i < num_insts; i++) {
            w_avg += prob[i]*test_fitness[i];
          }
          if (mut_rate != 0.0) {
            // run mutation-selection equation
            prob_next[mod_inst] = ((1.0-mut_rate)*test_fitness[mod_inst]*prob[mod_inst])/(w_avg);
            prob_next[mod_inst] += mut_rate/((double)num_insts);
          } else {
            // run selection equation
            prob_next[mod_inst] = (test_fitness[mod_inst]*prob[mod_inst])/(w_avg);
          }
          // increment change in probs
          delta_prob += (prob_next[mod_inst]-prob[mod_inst])*(prob_next[mod_inst]-prob[mod_inst]);
          //delta_prob_ex += (prob_next[mod_inst]-prob[mod_inst]);
        }
        // transfer t+1 to t for next iteration
        for (int i = 0; i < num_insts; i++) {
          prob[i]=prob_next[i];
          check_sum += prob[i]; 
        }
        
        // test for convergence
        if (delta_prob < converg_accuracy)
          break;
      }
      
      // Calculate complexity and entropy in bits and mers
      double entropy_mers = 0;
      double entropy_bits = 0;
      for (int i = 0; i < num_insts; i ++) {
        // watch for prob[i] == 0
        // --> 0.0 log(0.0) = 0.0
        if (prob[i] != 0.0) { 
          entropy_mers += prob[i] * log((double) 1.0/prob[i]) / log ((double) num_insts);
          entropy_bits += prob[i] * log((double) 1.0/prob[i]) / log ((double) 2.0);
        }
      }
      double complexity_mers = 1 - entropy_mers;
      double complexity_bits = (log ((double) num_insts) / log ((double) 2.0)) - entropy_bits;
            
      // update entropy and complexity values
      // with this site's values
      genome_ss_entropy_mers += entropy_mers;
      genome_ss_entropy_bits += entropy_bits;
      genome_ss_complexity_mers += complexity_mers;
      genome_ss_complexity_bits += complexity_bits;
      
      // save entropy for this line/site number
      entropy_ss_mers[line_num] = entropy_mers;
      entropy_ss_bits[line_num] = entropy_bits;
      
      // Reset the mod_genome back to the original sequence.
      mod_genome.SetOp(line_num, cur_inst);
    }
    
    /*
     * 
     *  TWO SITE CALCULATIONS
     * 
     */
    
    // Loop through all the lines of code, 
    // testing all TWO SITE mutations...
    tMatrix<double> test_fitness_2s(num_insts,num_insts);
    tArray<double> prob_1s_i(num_insts);
    tArray<double> prob_1s_j(num_insts);
    tMatrix<double> prob_2s(num_insts,num_insts);
    tMatrix<double> prob_next_2s(num_insts,num_insts);
    
    // run through lines in genome
    // - only consider lin_num2 > lin_num1 so that we don't consider
    // Mut Info [1][45] and Mut Info [45][1]
    for (int line_num1 = 0; line_num1 < max_line; line_num1++) {
      for (int line_num2 = line_num1+1; line_num2 < max_line; line_num2++) {
        // debug
        //cout << "line #1, #2: " << line_num1 << ", " << line_num2 << endl; 
        
        // get current instructions at site 1 and site 2
        int cur_inst1 = base_genome.GetOp(line_num1);
        int cur_inst2 = base_genome.GetOp(line_num2);
      
        // get current fitness
        double cur_inst_fitness_2s = genotype->GetFitness();
        
        // initialize running fitness total
        double fitness_total_2s = 0.0;
        
        // test that current fitness is greater than zero
        if (cur_inst_fitness_2s > 0.0) {
          // current fitness greater than zero
          // run through all possible instructions
          for (int mod_inst1 = 0; mod_inst1 < num_insts; mod_inst1++) {
            for (int mod_inst2 = 0; mod_inst2 < num_insts; mod_inst2++) {
              // modify mod_genome at two sites
              mod_genome.SetOp(line_num1, mod_inst1);
              mod_genome.SetOp(line_num2, mod_inst2);
              // analyze mod_genome
              cAnalyzeGenotype test_genotype(m_world, mod_genome, inst_set);
              test_genotype.Recalculate(m_ctx);
              test_fitness_2s[mod_inst1][mod_inst2] = test_genotype.GetFitness();
              
              // if modified fitness is greater than current fitness
              //  - set equal to current fitness
              if (test_fitness_2s[mod_inst1][mod_inst2] > cur_inst_fitness_2s)
                test_fitness_2s[mod_inst1][mod_inst2] = cur_inst_fitness_2s;
              
              // in all cases, scale fitness relative to current fitness
              test_fitness_2s[mod_inst1][mod_inst2] /= cur_inst_fitness_2s;
              
              // update fitness total
              fitness_total_2s += test_fitness_2s[mod_inst1][mod_inst2];
            }
          }
        } else {
          // current fitness is not greater than zero--skip
          cout << "Fitness of this genotype is ZERO--no information." << endl;
          continue;
        }
        
        // initialize probabilities
        for (int i = 0; i < num_insts; i++ ) {
          // single site probabilities
          // to be built from two site probabilities
          prob_1s_i[i] = 0.0;
          prob_1s_j[i] = 0.0;
          for (int j = 0; j < num_insts; j++ ) {
            // intitialize two site probability with
            // relative fitness
            prob_2s[i][j] = test_fitness_2s[i][j]/fitness_total_2s;
            prob_next_2s[i][j] = 0.0;
          }
        }
      
        double check_sum_2s = 0.0;
        while(1) {
          check_sum_2s = 0.0;
          double delta_prob_2s = 0.0;
          //double delta_prob_ex = 0.0;
          for (int mod_inst1 = 0; mod_inst1 < num_insts; mod_inst1 ++) {
            for (int mod_inst2 = 0; mod_inst2 < num_insts; mod_inst2 ++) {  
              // calculate the average fitness
              double w_avg_2s = 0.0;
              for (int i = 0; i < num_insts; i++) {
                for (int j = 0; j < num_insts; j++) {
                  w_avg_2s += prob_2s[i][j]*test_fitness_2s[i][j];
                }
              }
              if (mut_rate != 0.0) {
                // run mutation-selection equation
                // -term 1
                prob_next_2s[mod_inst1][mod_inst2] = ((1.0-mut_rate)*(1.0-mut_rate)*test_fitness_2s[mod_inst1][mod_inst2]*prob_2s[mod_inst1][mod_inst2])/(w_avg_2s);
                // -term 2
                double sum_term2 = 0.0;
                for (int i = 0; i < num_insts; i++) {
                  sum_term2 += (test_fitness_2s[i][mod_inst2]*prob_2s[i][mod_inst2])/(w_avg_2s);
                }
                prob_next_2s[mod_inst1][mod_inst2] += (((mut_rate*(1.0-mut_rate))/((double)num_insts)))*sum_term2;
                // -term 3
                double sum_term3 = 0.0;
                for (int j = 0; j < num_insts; j++) {
                  sum_term3 += (test_fitness_2s[mod_inst1][j]*prob_2s[mod_inst1][j])/(w_avg_2s);
                }
                prob_next_2s[mod_inst1][mod_inst2] += (((mut_rate*(1.0-mut_rate))/((double)num_insts)))*sum_term3;
                // -term 4
                prob_next_2s[mod_inst1][mod_inst2] += (mut_rate/((double)num_insts))*(mut_rate/((double)num_insts));
              } else {
                // run selection equation
                prob_next_2s[mod_inst1][mod_inst2] = (test_fitness_2s[mod_inst1][mod_inst2]*prob_2s[mod_inst1][mod_inst2])/(w_avg_2s);
                                
              }
              // increment change in probs
              delta_prob_2s += (prob_next_2s[mod_inst1][mod_inst2]-prob_2s[mod_inst1][mod_inst2])*(prob_next_2s[mod_inst1][mod_inst2]-prob_2s[mod_inst1][mod_inst2]);
              //delta_prob_ex += (prob_next[mod_inst]-prob[mod_inst]);
            }
          }
          // transfer probabilities at time t+1 
          // to t for next iteration
          for (int i = 0; i < num_insts; i++) {
            for (int j = 0; j < num_insts; j++) {
              prob_2s[i][j]=prob_next_2s[i][j];
              check_sum_2s += prob_2s[i][j];
            } 
          }
        
          // test for convergence
          if (delta_prob_2s < converg_accuracy)
            break;
        }

        // get single site probabilites from 
        // two site probabilities
        // site i (first site)
        double check_prob_sum_site_1 = 0.0;
        double check_prob_sum_site_2 = 0.0;
        for (int i = 0; i < num_insts; i++) {
          for (int j = 0; j < num_insts; j++) {
            prob_1s_i[i] += prob_2s[i][j];
          }
          check_prob_sum_site_1 += prob_1s_i[i]; 
        }
        // site j (second site)
        for (int j = 0; j < num_insts; j++) {
          for (int i = 0; i < num_insts; i++) {
            prob_1s_j[j] += prob_2s[i][j];
          }
          check_prob_sum_site_2 += prob_1s_j[j];
        }

        // Calculate one site and two versions of 
        // complexity and entropy in bits and mers
        //-mers
        double entropy_ss_site1_mers = 0.0;
        double entropy_ss_site2_mers = 0.0;
        double entropy_ds_mers = 0.0;
        //-bits
        double entropy_ss_site1_bits = 0.0;
        double entropy_ss_site2_bits = 0.0;
        double entropy_ds_bits = 0.0;
        
        // single site entropies
        for (int i = 0; i < num_insts; i ++) {
          // watch for zero probabilities
          if (prob_1s_i[i] != 0.0) {
            // mers
            entropy_ss_site1_mers += prob_1s_i[i] * log((double) 1.0/prob_1s_i[i]) / log ((double) num_insts);
            // bits
            entropy_ss_site1_bits += prob_1s_i[i] * log((double) 1.0/prob_1s_i[i]) / log ((double) 2.0);
          }
          if (prob_1s_j[i] != 0.0) {
            // mers
            entropy_ss_site2_mers += prob_1s_j[i] * log((double) 1.0/prob_1s_j[i]) / log ((double) num_insts);
            // bits
            entropy_ss_site2_bits += prob_1s_j[i] * log((double) 1.0/prob_1s_j[i]) / log ((double) 2.0);
          }
        }
        
        // two site joint entropies
        for (int i = 0; i < num_insts; i ++) {
          for (int j = 0; j < num_insts; j ++) {
            // watch for zero probabilities
            if (prob_2s[i][j] != 0.0) {
              // two site entropy in mers
              entropy_ds_mers += prob_2s[i][j] * log((double) 1.0/prob_2s[i][j]) / log ((double) num_insts);
              // two site entropy in bitss
              entropy_ds_bits += prob_2s[i][j] * log((double) 1.0/prob_2s[i][j]) / log ((double) 2.0);
            }
          }
        }
        
        // calculate the mutual information
        // - add single site entropies
        // - subtract two site joint entropy
        // units: mers
        double mutual_information_mers = entropy_ss_site1_mers + entropy_ss_site2_mers;
        mutual_information_mers -= entropy_ds_mers;
        
        // units: bits
        double mutual_information_bits = entropy_ss_site1_bits + entropy_ss_site2_bits;
        mutual_information_bits -= entropy_ds_bits;
        
        // two site, only update mutatual informtion total
        genome_ds_mut_info_mers += mutual_information_mers;
        genome_ds_mut_info_bits += mutual_information_bits;
        
        // write output to file
        fp_2s.Write(line_num1,                    "Site 1 in genome");
        fp_2s.Write(line_num2,                    "Site 2 in genome");
        fp_2s.Write(cur_inst1,                    "Current Instruction, Site 1");
        fp_2s.Write(cur_inst2,                    "Current Instruction, Site 2");
        fp_2s.Write(entropy_ss_mers[line_num1],   "Entropy (MERS), Site 1 -- single site mut-sel balance");
        fp_2s.Write(entropy_ss_site1_mers,        "Entropy (MERS), Site 1 -- TWO site mut-sel balance");
        fp_2s.Write(entropy_ss_mers[line_num2],   "Entropy (MERS), Site 2 -- single site mut-sel balance");
        fp_2s.Write(entropy_ss_site2_mers,        "Entropy (MERS), Site 2 -- TWO site mut-sel balance");
        fp_2s.Write(entropy_ds_mers,              "Joint Entropy (MERS), Site 1 & 2 -- TWO site mut-sel balance");
        fp_2s.Write(mutual_information_mers,      "Mutual Information (MERS), Site 1 & 2 -- TWO site mut-sel balance");
        fp_2s.Endl();
                    
        // Reset the mod_genome back to the original sequence.
        mod_genome.SetOp(line_num1, cur_inst1);
        mod_genome.SetOp(line_num2, cur_inst2);
        
      }// end line 2
    }// end line 1
    
    // cleanup file for this genome
    m_world->GetDataFileManager().Remove(filename_2s);
    
    // calculate the two site complexity
    // (2 site complexity) = (1 site complexity) + (total 2 site mutual info)
    genome_ds_complexity_mers = genome_ss_complexity_mers + genome_ds_mut_info_mers;
    genome_ds_complexity_bits = genome_ss_complexity_bits + genome_ds_mut_info_bits;
        
    summary_fp.Write(genotype->GetID(),           "Genotype ID");
    summary_fp.Write(genotype->GetFitness(),      "Genotype Fitness");
    summary_fp.Write(genome_ss_entropy_mers,      "Entropy (single-site) MERS");
    summary_fp.Write(genome_ss_complexity_mers,   "Complexity (single-site) MERS");
    summary_fp.Write(genome_ds_mut_info_mers,     "Mutual Information MERS");
    summary_fp.Write(genome_ds_complexity_mers,   "Complexity (two-site) MERS");
    summary_fp.Write(genome_ss_entropy_bits,      "Entropy (single-site) BITS");
    summary_fp.Write(genome_ss_complexity_bits,   "Complexity (single-site) BITS");
    summary_fp.Write(genome_ds_mut_info_bits,     "Mutual Information BITS");
    summary_fp.Write(genome_ds_complexity_bits,   "Complexity (two-site) BITS");
    summary_fp.Endl();
        
    // Always grabs the first one
    // Skip i-1 times, so that the beginning of the loop will grab the ith one
    // where i is the batchFrequency
    for(int count=0; genotype != NULL && count < batchFrequency - 1; count++) {
      genotype = batch_it.Next();
      if(genotype != NULL && m_world->GetVerbosity() >= VERBOSE_ON) {
        cout << "Skipping: " << genotype->GetName() << endl;
      }
    }
    if(genotype == NULL) { break; }
  }
  
  m_world->GetDataFileManager().Remove(summary_filename);
  
  delete testcpu;
}

void cAnalyze::AnalyzePopComplexity(cString cur_string)
{
  cout << "Analyzing population complexity ..." << endl;
  
  // Load in the variables...
  cString directory = PopDirectory(cur_string, "pop_complexity/");
  cString file = cur_string;
  
  // Construct filename...
  cString filename;
  filename.Set("%spop%s.complexity.dat", static_cast<const char*>(directory), static_cast<const char*>(file));
  ofstream& fp = m_world->GetDataFileOFStream(filename);
  
  //////////////////////////////////////////////////////////
  // Loop through all of the genotypes in this batch ...
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  
  
  genotype = batch_it.Next();
  
  
  if (genotype == NULL) return;
  int seq_length = genotype->GetLength();
  const int num_insts = inst_set.GetSize();
  tMatrix<int> inst_stat(seq_length, num_insts);
  
  // Initializing inst_stat ...
  for (int line_num = 0; line_num < seq_length; line_num ++) 
    for (int inst_num = 0; inst_num < num_insts; inst_num ++) 
      inst_stat(line_num, inst_num) = 0;
  
  int num_cpus = 0;
  int actural_samples = 0;
  while (genotype != NULL) {
    num_cpus = genotype->GetNumCPUs();
    const cGenome & base_genome = genotype->GetGenome();
    for (int i=0; i<num_cpus; i++) {   // Stat on every organism with same genotype.
                                       //if (flag_array[organism_index] == 0) {
                                       //organism_index++;
                                       //continue;
                                       //}
      for (int line_num = 0; line_num < seq_length; line_num ++) {
        int cur_inst = base_genome.GetOp(line_num);
        inst_stat(line_num, cur_inst) ++;
      }
      //organism_index++;
      actural_samples++;
    }
    genotype = batch_it.Next();
  }

// Calculate complexity
for (int line_num = 0; line_num < seq_length; line_num ++) {
  double entropy = 0.0;
  for (int inst_num = 0; inst_num < num_insts; inst_num ++) {
    if (inst_stat(line_num, inst_num) == 0) continue;
    float prob = (float) (inst_stat(line_num, inst_num)) / (float) (actural_samples);
    entropy += prob * log((double) 1.0/prob) / log((double) num_insts);
  }
  double complexity = 1 - entropy;
  fp << complexity << " ";
}
fp << endl;

m_world->GetDataFileManager().Remove(filename);
return;
}



/* MRR
 * August 2007
 * This function will go through the lineage, align the genotypes, and
 * preform mutation reversion a specified number of descendents ahead
 * assuming they keep within a certain alignment distance (specified as well).
 * The output will give fitness information for the mutation-reverted genotypes
 * as described below.
*/
void cAnalyze::MutationRevert(cString cur_string)
{
  
  //This function takes in three parameters, all defaulted:
  cString filename("XXX.dat");   //The name of the output file
  int      max_dist      = -1;    //The maximum edit distance allowed in the search
  int	   max_depth     = 5;     //The maximum depth forward one wishes to search
  
  if (cur_string.GetSize() != 0) filename = cur_string.PopWord();
  if (cur_string.GetSize() != 0) max_dist = cur_string.PopWord().AsInt();
  if (cur_string.GetSize() != 0) max_depth = cur_string.PopWord().AsInt();
  
	//Warning notifications
  if (!batch[cur_batch].IsLineage())
  {
		cout << "Error: This command requires a lineage.  Skipping." << endl;
		return;
  }
  
	
	//Request a file
	ofstream& FOT = m_world->GetDataFileOFStream(filename);
	/*
   FOT output per line
   ID
   FITNESS
   BIRTH
   DISTANCE
   PID
   P_FITNESS
   P_BIRTH
			@ea depth past
   CHILDX_ID
   CHILDX_BIRTH
   CHILDX_FITNESS
   CHILDX_DISTANCE
   CHILDX_FITNESS_SANS_MUT
   */
	
	
  //Align the batch... we're going to keep the fitnesses intact from the runs
	CommandAlign("");
  
	//Our edit distance is already stored in the historical dump.
	
	//Test hardware
	cCPUTestInfo test_info;
	test_info.UseRandomInputs(true); 
  
	tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype* parent_genotype = batch_it.Next();
	cAnalyzeGenotype* other_genotype  = NULL;
	cAnalyzeGenotype* genotype        = NULL;
	
  while( (genotype = batch_it.Next()) != NULL && parent_genotype != NULL)
  {
		if (true)
		{
			FOT << genotype->GetID()			<< " "
      << genotype->GetFitness()		<< " "
      << genotype->GetUpdateBorn() << " "
      << genotype->GetParentDist() << " "
      << parent_genotype->GetID()				<< " "
      << parent_genotype->GetFitness()		<< " "
      << parent_genotype->GetUpdateBorn()	<< " ";
      
			int cum_dist = 0;
			cString str_parent = parent_genotype->GetSequence();
			cString str_other  = "";
			cString str_align_parent = parent_genotype->GetAlignedSequence();
			cString str_align_other  = genotype->GetAlignedSequence();
			cString reversion  = ""; //Reversion mask
			
			//Find what changes to revert
			for (int k = 0; k < str_align_parent.GetSize(); k++)
			{
				char p = str_align_parent[k];
				char c = str_align_other[k];
				if (p == c)
					reversion += " ";	//Nothing
				else if (p == '_' && c != '_')
					reversion += "+";	//Insertion
				else if (p != '_' && c == '_')
					reversion += "-";  //Deletion
				else
					reversion += p;			//Point Mutation
			}
			
			tListIterator<cAnalyzeGenotype> next_it(batch_it);
			for (int i = 0; i < max_depth; i++)
			{
				if ( (other_genotype = next_it.Next()) != NULL && 
             (cum_dist <= max_dist || max_dist == -1) )
				{
					cum_dist += other_genotype->GetParentDist();
					if (cum_dist > max_dist && max_dist != -1)
						break;
					str_other = other_genotype->GetSequence();
					str_align_other = other_genotype->GetAlignedSequence();
					
					//Revert "background" to parental form
					cString reverted = "";
					for (int k = 0; k < reversion.GetSize(); k++)
					{
						if (reversion[k] == '+')       continue;  //Insertion, so skip
						else if (reversion[k] == '-')  reverted += str_align_parent[k]; //Add del
						else if (reversion[k] != ' ')       reverted += reversion[k];        //Revert mut
						else if (str_align_other[k] != '_') reverted += str_align_other[k];  //Keep current
					}
					
					cAnalyzeGenotype new_genotype(m_world, reverted, inst_set);  //Get likely fitness
					new_genotype.Recalculate(m_ctx, &test_info, NULL, 50);
					
          FOT << other_genotype->GetID()			<< " "
            << other_genotype->GetFitness()		<< " "
            << other_genotype->GetUpdateBorn() << " "
            << cum_dist                        << " "
            << new_genotype.GetFitness()       << " ";
				}
				else
				{
					FOT << -1 << " "
          << -1 << " "
          << -1 << " "
          << -1 << " "
          << -1 << " ";
				}
			}
			FOT << endl;
		}
		parent_genotype = genotype;
  }
	
  return;
}

void cAnalyze::EnvironmentSetup(cString cur_string)
{
  cout << "Running environment command: " << endl << "  " << cur_string << endl;  
  m_world->GetEnvironment().LoadLine(cur_string);
}


void cAnalyze::CommandHelpfile(cString cur_string)
{
  cout << "Printing helpfiles in: " << cur_string << endl;
  
  cHelpManager help_control;
  if (m_world->GetVerbosity() >= VERBOSE_ON) help_control.SetVerbose();
  while (cur_string.GetSize() > 0) {
    help_control.LoadFile(cur_string.PopWord());
  }
  
  help_control.PrintHTML();
}




//////////////// Control...

void cAnalyze::VarSet(cString cur_string)
{
  cString var = cur_string.PopWord();
  
  if (cur_string.GetSize() == 0) {
    cerr << "Error: No variable provided in SET command" << endl;
    return;
  }
  
  cString& cur_variable = GetVariable(var);
  cur_variable = cur_string.PopWord();
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Setting " << var << " to " << cur_variable << endl;
  }
}

void cAnalyze::ConfigGet(cString cur_string)
{
  cString cvar = cur_string.PopWord();
  cString var = cur_string.PopWord();
  
  if (cvar.GetSize() == 0 || var.GetSize() == 0) {
    cerr << "Error: Missing variable in CONFIG_GET command" << endl;
    return;
  }
  
  cString& cur_variable = GetVariable(var);
  
  // Get Config Variable
  if (!m_world->GetConfig().Get(cvar, cur_variable)) {
    cerr << "Error: Configuration Variable '" << var << "' was not found." << endl;
    return;
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON)
    cout << "Setting variable " << var << " to " << cur_variable << endl;
}

void cAnalyze::ConfigSet(cString cur_string)
{
  cString cvar = cur_string.PopWord();
  
  if (cvar.GetSize() == 0) {
    cerr << "Error: No variable provided in CONFIG_SET command" << endl;
    return;
  }
  
  // Get Config Variable
  cString val = cur_string.PopWord();
  if (!m_world->GetConfig().Set(cvar, val)) {
    cerr << "Error: Configuration Variable '" << cvar << "' was not found." << endl;
    return;
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON)
    cout << "Setting configuration variable " << cvar << " to " << val << endl;
}


void cAnalyze::BatchSet(cString cur_string)
{
  int next_batch = 0;
  if (cur_string.CountNumWords() > 0) {
    next_batch = cur_string.PopWord().AsInt();
  }
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Setting current batch to " << next_batch << endl;
  if (next_batch >= MAX_BATCHES) {
    cerr << "  Error: max batches is " << MAX_BATCHES << endl;
    if (exit_on_error) exit(1);
  } else {
    cur_batch = next_batch;
  }
}

void cAnalyze::BatchName(cString cur_string)
{
  if (cur_string.CountNumWords() == 0) {
    if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "  Warning: No name given in NAME_BATCH!" << endl;
    return;
  }
  
  batch[cur_batch].Name() = cur_string.PopWord();
}

void cAnalyze::BatchTag(cString cur_string)
{
  if (cur_string.CountNumWords() == 0) {
    if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "  Warning: No tag given in TAG_BATCH!" << endl;
    return;
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Tagging batch " << cur_batch
    << " with tag '" << cur_string << "'" << endl;
  }
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    genotype->SetTag(cur_string);
  }
  
}

void cAnalyze::BatchPurge(cString cur_string)
{
  int batch_id = cur_batch;
  if (cur_string.CountNumWords() > 0) batch_id = cur_string.PopWord().AsInt();
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Purging batch " << batch_id << endl;
  
  while (batch[batch_id].List().GetSize() > 0) {
    delete batch[batch_id].List().Pop();
  }
  
  batch[batch_id].SetLineage(false);
  batch[batch_id].SetAligned(false);
}

void cAnalyze::BatchDuplicate(cString cur_string)
{
  if (cur_string.GetSize() == 0) {
    cerr << "Duplicate Error: Must include from ID!" << endl;
    if (exit_on_error) exit(1);
  }
  int batch_from = cur_string.PopWord().AsInt();
  
  int batch_to = cur_batch;
  if (cur_string.GetSize() > 0) batch_to = cur_string.PopWord().AsInt();
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Duplicating from batch " << batch_from << " to batch " << batch_to << "." << endl;
  }
  
  tListIterator<cAnalyzeGenotype> batch_from_it(batch[batch_from].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_from_it.Next()) != NULL) {
    cAnalyzeGenotype * new_genotype = new cAnalyzeGenotype(*genotype);
    batch[batch_to].List().PushRear(new_genotype);
  }
  
  batch[batch_to].SetLineage(false);
  batch[batch_to].SetAligned(false);
}

void cAnalyze::BatchRecalculate(cString cur_string)
{
  tArray<int> manual_inputs;  // Used only if manual inputs are specified
  cString msg;                // Holds any information we may want to send the driver to display
  
  int use_resources      = (cur_string.GetSize()) ? cur_string.PopWord().AsInt() : 0;
  int update             = (cur_string.GetSize()) ? cur_string.PopWord().AsInt() : -1;
  bool use_random_inputs = (cur_string.GetSize()) ? cur_string.PopWord().AsInt() == 1: false;
  bool use_manual_inputs = false;
  
  //Manual inputs will override random input request and must be the last arguments.
  if (cur_string.CountNumWords() > 0){
    if (cur_string.CountNumWords() == m_world->GetEnvironment().GetInputSize()){
      manual_inputs.Resize(m_world->GetEnvironment().GetInputSize());
      use_random_inputs = false;
      use_manual_inputs = true;
      for (int k = 0; cur_string.GetSize(); k++)
        manual_inputs[k] = cur_string.PopWord().AsInt();
    } else if (m_world->GetVerbosity() >= VERBOSE_ON){
      msg.Set("Invalid number of environment inputs requested for recalculation: %d specified, %d required.", 
              cur_string.CountNumWords(), m_world->GetEnvironment().GetInputSize());
      m_world->GetDriver().NotifyWarning(msg);
    }
  }
  
  cCPUTestInfo test_info;
  if (use_manual_inputs)
    test_info.UseManualInputs(manual_inputs);
  else
    test_info.UseRandomInputs(use_random_inputs); 
  test_info.SetResourceOptions(use_resources, &resources, update, m_resource_time_spent_offset);

  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    msg.Set("Running batch %d through test CPUs...", cur_batch);
    m_world->GetDriver().NotifyComment(msg);
  } else{ 
    msg.Set("Running through test CPUs...");
    m_world->GetDriver().NotifyComment(msg);
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON && batch[cur_batch].IsLineage() == false) {
    msg.Set("Batch may not be a lineage; parent and ancestor distances may not be correct"); 
    m_world->GetDriver().NotifyWarning(msg);
  }
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  cAnalyzeGenotype * last_genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {    
    // If the previous genotype was the parent of this one, pass in a pointer
    // to it for improved recalculate (such as distance to parent, etc.)
    if (last_genotype != NULL && genotype->GetParentID() == last_genotype->GetID()) {
      genotype->Recalculate(m_ctx, &test_info, last_genotype);
    } else {
      genotype->Recalculate(m_ctx, &test_info);
    }
    last_genotype = genotype;
  }
    
  return;
}


void cAnalyze::BatchRecalculateWithArgs(cString cur_string)
{
  // RECALC <use_resources> <random_inputs> <manual_inputs in.1 in.2 in.3> <update N> <num_trials X>

  tArray<int> manual_inputs;  // Used only if manual inputs are specified
  cString msg;                // Holds any information we may want to send the driver to display
  
  // Defaults
  bool use_resources     = false;
  int  update            = -1;
  bool use_random_inputs = false;
  bool use_manual_inputs = false;
  int  num_trials        = 1;
  
  // Handle our recalculate arguments
  // Really, we should have a generalized tokenizer handle this
  cStringList args(cur_string);
  int pos = -1;
  if (args.PopString("use_resources") != "")      use_resources     = true;
  if (args.PopString("use_random_inputs") != "")  use_random_inputs = true;
  if ( (pos = args.LocateString("use_manual_inputs") ) != -1){
    use_manual_inputs = true;
    args.PopString("use_manual_inputs");
    int num = m_world->GetEnvironment().GetInputSize();
    manual_inputs.Resize(num);
    if (args.GetSize() >= pos + num - 2) 
      for (int k = 0; k < num; k++)
        manual_inputs[k] = args.PopLine(pos).AsInt();  
    else
      m_world->GetDriver().RaiseFatalException(1, "RecalculateWithArgs: Invalid use of use_manual_inputs");
  }
  if ( (pos = args.LocateString("update")) != -1 ){
    args.PopString("update");
    if (args.GetSize() >= pos - 1){
      update = args.PopLine(pos).AsInt();
    } else
       m_world->GetDriver().RaiseFatalException(1, "RecalculateWithArgs: Invalid use of update (did you specify a value?)");
  }
  if ( (pos = args.LocateString("num_trials")) != -1){
    args.PopString("num_trials");
    if (args.GetSize() >= pos - 1)
      num_trials = args.PopLine(pos).AsInt();
    else
      m_world->GetDriver().RaiseFatalException(1, "RecalculateWithArgs: Invalid use of num_trials (did you specify a value?)");
  }
  
  if (use_manual_inputs)
    use_random_inputs = false;
  
  cCPUTestInfo test_info;
  if (use_manual_inputs)
    test_info.UseManualInputs(manual_inputs);
  else
    test_info.UseRandomInputs(use_random_inputs); 
  test_info.SetResourceOptions(use_resources, &resources, update, m_resource_time_spent_offset);
  
  // Notifications
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    msg.Set("Running batch %d through test CPUs...", cur_batch);
    m_world->GetDriver().NotifyComment(msg);
  } else{ 
    msg.Set("Running through test CPUs...");
    m_world->GetDriver().NotifyComment(msg);
  }
  if (m_world->GetVerbosity() >= VERBOSE_ON && batch[cur_batch].IsLineage() == false) {
    msg.Set("Batch may not be a lineage; parent and ancestor distances may not be correct"); 
    m_world->GetDriver().NotifyWarning(msg);
  }
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  cAnalyzeGenotype * last_genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {    
    // If the previous genotype was the parent of this one, pass in a pointer
    // to it for improved recalculate (such as distance to parent, etc.)
    if (last_genotype != NULL && genotype->GetParentID() == last_genotype->GetID()) {
      genotype->Recalculate(m_ctx, &test_info, last_genotype, num_trials);
    } else {
      genotype->Recalculate(m_ctx, &test_info, NULL, num_trials);
    }
    last_genotype = genotype;
  }
  
  return;
}


void cAnalyze::BatchRename(cString cur_string)
{
  if (m_world->GetVerbosity() <= VERBOSE_NORMAL) cout << "Renaming organisms..." << endl;
  else cout << "Renaming organisms in batch " << cur_batch << endl;
  
  // If a number is given with rename, start at that number...
  
  int id_num = cur_string.PopWord().AsInt();
  tListIterator<cAnalyzeGenotype> batch_it(batch[cur_batch].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    cString name = cStringUtil::Stringf("org-%d", id_num);
    genotype->SetID(id_num);
    genotype->SetName(name);
    id_num++;
  }
}

void cAnalyze::PrintStatus(cString cur_string)
{
  // No Args needed...
  (void) cur_string;
  
  cout << "Status Report:" << endl;
  for (int i = 0; i < MAX_BATCHES; i++) {
    if (i == cur_batch || batch[i].List().GetSize() > 0) {
      cout << "  Batch " << i << " -- "
      << batch[i].List().GetSize() << " genotypes.";
      if (i == cur_batch) cout << "  <current>";
      if (batch[i].IsLineage() == true) cout << "  <lineage>";
      if (batch[i].IsAligned() == true) cout << "  <aligned>";
      
      cout << endl;
    }
  }
}

void cAnalyze::PrintDebug(cString cur_string)
{
  cout << "::: " << cur_string << '\n';
}

void cAnalyze::PrintTestInfo(cString cur_string)
{
  cFlexVar var1(1), var2(2.0), var3('3'), var4("four");
  cFlexVar var5(9), var6(9.0), var7('9'), var8("9");
  
  tArray<cFlexVar> vars(10);
  vars[0] = "Testing";
  vars[1] = 1;
  vars[2] = 2.0;
  vars[3] = '3';
  vars[4] = "four";
  vars[5] = 9;
  vars[6] = 9.0;
  vars[7] = '9';
  vars[8] = "9";
  
  cout << "AsString:  ";
  for (int i = 0; i < 10; i++) cout << i << ":" << vars[i].AsString() << " ";
  cout << endl;
  
  cout << "AsInt:  ";
  for (int i = 0; i < 10; i++) cout << i << ":" << vars[i].AsInt() << " ";
  cout << endl;
  
  for (int i = 0; i < 10; i++) {
    for (int j = i+1; j < 10; j++) {
      cout << "     vars[" << i << "] <= vars[" << j << "] ?  " << (vars[i] <= vars[j]);
      cout << "     vars[" << j << "] <= vars[" << i << "] ?  " << (vars[j] <= vars[i]);
      cout << endl;
    }
  }
  
}

void cAnalyze::IncludeFile(cString cur_string)
{
  while (cur_string.GetSize() > 0) {
    cString filename = cur_string.PopWord();
    
    cInitFile include_file(filename);
    
    tList<cAnalyzeCommand> include_list;
    LoadCommandList(include_file, include_list);
    ProcessCommands(include_list);
  }
}

void cAnalyze::CommandSystem(cString cur_string)
{
  if (cur_string.GetSize() == 0) {
    cerr << "Error: Keyword \"system\" must be followed by command to run." << endl;
    if (exit_on_error) exit(1);
  }
  
  cout << "Running System Command: " << cur_string << endl;
  
  system(cur_string);
}

void cAnalyze::CommandInteractive(cString cur_string)
{
  // No Args needed...
  (void) cur_string;
  
  RunInteractive();
}


/*
 FIXME@kgn
 Must categorize COMPETE command.
 */
/* Arguments to COMPETE: */
/*
 batch_size : size of target batch
 from_id
 to_id=current
 initial_next_id=-1
 */
void cAnalyze::BatchCompete(cString cur_string)
{
  if (cur_string.GetSize() == 0) {
    cerr << "Compete Error: Must include target batch size!" << endl;
    if (exit_on_error) exit(1);
  }
  int batch_size = cur_string.PopWord().AsInt();
  
  if (cur_string.GetSize() == 0) {
    cerr << "Compete Error: Must include from ID!" << endl;
    if (exit_on_error) exit(1);
  }
  int batch_from = cur_string.PopWord().AsInt();
  
  int batch_to = cur_batch;
  if (cur_string.GetSize() > 0) batch_to = cur_string.PopWord().AsInt();
  
  int initial_next_id = -1;
  if (cur_string.GetSize() > 0) {
    initial_next_id = cur_string.PopWord().AsInt();
  }
  if (0 <= initial_next_id) {
    SetTempNextID(initial_next_id);
  }
  
  int initial_next_update = -1;
  if (cur_string.GetSize() > 0) {
    initial_next_update = cur_string.PopWord().AsInt();
  }
  if (0 <= initial_next_update) {
    SetTempNextUpdate(initial_next_update);
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Compete " << batch_size << " organisms from batch " << batch_from << " to batch " << batch_to << ";" << endl;
    cout << "assigning new IDs starting with " << GetTempNextID() << "." << endl;
  }
  
  /* Get iterator into "from" batch. */ 
  tListIterator<cAnalyzeGenotype> batch_it(batch[batch_from].List());
  /* Get size of "from" batch. */
  const int parent_batch_size = batch[batch_from].List().GetSize();
  
  /* Create scheduler. */
  cSchedule* schedule = new cProbSchedule(
                                          parent_batch_size,
                                          m_world->GetRandom().GetInt(0x7FFFFFFF)
                                          );
  
  /* Initialize scheduler with fitness values per-organism. */
  tArray<cAnalyzeGenotype *> genotype_array(parent_batch_size);
  tArray<cCPUMemory> offspring_genome_array(parent_batch_size);
  tArray<cMerit> fitness_array(parent_batch_size);
  cAnalyzeGenotype * genotype = NULL;
  
  cCPUTestInfo test_info;
  
  /*
   FIXME@kgn
   This should be settable by an optional argument.
   */
  test_info.UseRandomInputs(true); 
  
  int array_pos = 0;
  while ((genotype = batch_it.Next()) != NULL) {
    genotype_array[array_pos] = genotype;
    genotype->Recalculate(m_world->GetDefaultContext(), &test_info, NULL);
    if(genotype->GetViable()){
      /*
       FIXME@kgn
       - HACK : multiplication by 1000 because merits less than 1 are truncated
       to zero.
       */
      fitness_array[array_pos] = genotype->GetFitness() * 1000.;
      /*
       FIXME@kgn
       - Need to note somewhere that we are using first descendent of the
       parent, if the parent is viable, so that genome of first descendent may
       differ from that of parent.
       */
      offspring_genome_array[array_pos] = test_info.GetTestOrganism(0)->ChildGenome();
    } else {
      fitness_array[array_pos] = 0.0;
    }
    schedule->Adjust(array_pos, fitness_array[array_pos]);
    array_pos++;
  }
  
  /* Use scheduler to sample organisms in "from" batch. */
  for(int i=0; i<batch_size; /* don't increment i yet */){
    /* Sample an organism. */
    array_pos = schedule->GetNextID();
    if(array_pos < 0){
      cout << "Warning: No organisms in origin batch have positive fitness, cannot sample to destination batch." << endl; 
      break;
    }
    genotype = genotype_array[array_pos];
    
    double copy_mut_prob = m_world->GetConfig().COPY_MUT_PROB.Get();
    double ins_mut_prob = m_world->GetConfig().DIVIDE_INS_PROB.Get();
    double del_mut_prob = m_world->GetConfig().DIVIDE_DEL_PROB.Get();
    int ins_line = -1;
    int del_line = -1;
    
    cCPUMemory child_genome = offspring_genome_array[array_pos];
    
    if (copy_mut_prob > 0.0) {
      for (int n = 0; n < child_genome.GetSize(); n++) {
        if (m_world->GetRandom().P(copy_mut_prob)) {
          child_genome.SetInstruction(n, inst_set.GetRandomInst(m_ctx));
        }
      }
    }
    
    /* Perform an Insertion if it has one. */
    if (m_world->GetRandom().P(ins_mut_prob)) {
      ins_line = m_world->GetRandom().GetInt(child_genome.GetSize() + 1);
      child_genome.Insert(ins_line, inst_set.GetRandomInst(m_ctx));
    }
    
    /* Perform a Deletion if it has one. */
    if (m_world->GetRandom().P(del_mut_prob)) {
      del_line = m_world->GetRandom().GetInt(child_genome.GetSize());
      child_genome.Remove(del_line);
    }
    
    /* Create (possibly mutated) offspring. */
    cAnalyzeGenotype * new_genotype = new cAnalyzeGenotype(
                                                           m_world,
                                                           child_genome,
                                                           inst_set
                                                           );
    
    int parent_id = genotype->GetID();
    int child_id = GetTempNextID();
    SetTempNextID(child_id + 1);
    cString child_name = cStringUtil::Stringf("org-%d", child_id);
    
    new_genotype->SetParentID(parent_id);
    new_genotype->SetID(child_id);
    new_genotype->SetName(child_name);
    new_genotype->SetUpdateBorn(GetTempNextUpdate());
    
    /* Place offspring in "to" batch. */
    batch[batch_to].List().PushRear(new_genotype);
    /* Increment and continue. */
    i++;
  }
  
  SetTempNextUpdate(GetTempNextUpdate() + 1);
  
  batch[batch_to].SetLineage(false);
  batch[batch_to].SetAligned(false);
  
  if(schedule){ delete schedule; schedule = 0; }
  
  return;
}


void cAnalyze::FunctionCreate(cString cur_string, tList<cAnalyzeCommand>& clist)
{
  int num_args = cur_string.CountNumWords();
  if (num_args < 1) {
    cerr << "Error: Must provide function name when creating function.";
    if (exit_on_error) exit(1);
  }
  
  cString fun_name = cur_string.PopWord();
  
  if (FindAnalyzeCommandDef(fun_name) != NULL) {
    cerr << "Error: Cannot create function '" << fun_name
    << "'; already exists." << endl;
    if (exit_on_error) exit(1);
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Creating function: " << fun_name << endl;
  
  // Create the new function...
  cAnalyzeFunction * new_function = new cAnalyzeFunction(fun_name);
  while (clist.GetSize() > 0) {
    new_function->GetCommandList()->PushRear(clist.Pop());
  }
  
  // Save the function on the new list...
  function_list.PushRear(new_function);
}

bool cAnalyze::FunctionRun(const cString & fun_name, cString args)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Running function: " << fun_name << endl;
    // << " with args: " << args << endl;
  }
  
  // Find the function we're about to run...
  cAnalyzeFunction * found_function = NULL;
  tListIterator<cAnalyzeFunction> function_it(function_list);
  while (function_it.Next() != NULL) {
    if (function_it.Get()->GetName() == fun_name) {
      found_function = function_it.Get();
      break;
    }
  }
  
  // If we were unable to find the command we're looking for, return false.
  if (found_function == NULL) return false;
  
  // Back up the local variables
  cString backup_arg_vars[10];
  cString backup_local_vars[26];
  for (int i = 0; i < 10; i++) backup_arg_vars[i] = arg_variables[i];
  for (int i = 0; i < 26; i++) backup_local_vars[i] = local_variables[i];
  
  // Set the arg variables to the passed-in args...
  arg_variables[0] = fun_name;
  for (int i = 1; i < 10; i++) arg_variables[i] = args.PopWord();
  for (int i = 0; i < 26; i++) local_variables[i] = "";
  
  ProcessCommands(*(found_function->GetCommandList()));
  
  // Restore the local variables
  for (int i = 0; i < 10; i++) arg_variables[i] = backup_arg_vars[i];
  for (int i = 0; i < 26; i++) local_variables[i] = backup_local_vars[i];
  
  return true;
}


int cAnalyze::BatchUtil_GetMaxLength(int batch_id)
{
  if (batch_id < 0) batch_id = cur_batch;
  
  int max_length = 0;
  
  tListIterator<cAnalyzeGenotype> batch_it(batch[batch_id].List());
  cAnalyzeGenotype * genotype = NULL;
  while ((genotype = batch_it.Next()) != NULL) {
    if (genotype->GetLength() > max_length) max_length = genotype->GetLength();
  }
  
  return max_length;
}


void cAnalyze::CommandForeach(cString cur_string,
                              tList<cAnalyzeCommand> & clist)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Initiating Foreach loop..." << endl;
  
  cString var = cur_string.PopWord();
  int num_args = cur_string.CountNumWords();
  
  cString & cur_variable = GetVariable(var);
  
  for (int i = 0; i < num_args; i++) {
    cur_variable = cur_string.PopWord();
    
    if (m_world->GetVerbosity() >= VERBOSE_ON) {
      cout << "Foreach: setting " << var << " to " << cur_variable << endl;
    }
    ProcessCommands(clist);
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Ending Foreach on " << var << endl;
  }
}


void cAnalyze::CommandForRange(cString cur_string,
                               tList<cAnalyzeCommand> & clist)
{
  if (m_world->GetVerbosity() >= VERBOSE_ON) cout << "Initiating FORRANGE loop..." << endl;
  
  int num_args = cur_string.CountNumWords();
  if (num_args < 3) {
    cerr << "  Error: Must give variable, min and max with FORRANGE!"
    << endl;
    if (exit_on_error) exit(1);
  }
  
  cString var = cur_string.PopWord();
  double min_val = cur_string.PopWord().AsDouble();
  double max_val = cur_string.PopWord().AsDouble();
  double step_val = 1.0;
  if (num_args >=4 ) step_val = cur_string.PopWord().AsDouble();
  
  cString & cur_variable = GetVariable(var);
  
  // Seperate out all ints from not all ints...
  if (min_val == (double) ((int) min_val) &&
      max_val == (double) ((int) max_val) &&
      step_val == (double) ((int) step_val)) {
    for (int i = (int) min_val; i <= (int) max_val; i += (int) step_val) {
      cur_variable.Set("%d", i);
      
      if (m_world->GetVerbosity() >= VERBOSE_ON) {
        cout << "FORRANGE: setting " << var << " to " << cur_variable << endl;
      }
      ProcessCommands(clist);
    }
  } else {
    for (double i = min_val; i <= max_val; i += step_val) {
      cur_variable.Set("%f", i);
      
      if (m_world->GetVerbosity() >= VERBOSE_ON) {
        cout << "FORRANGE: setting " << var << " to " << cur_variable << endl;
      }
      ProcessCommands(clist);
    }
  }
  
  if (m_world->GetVerbosity() >= VERBOSE_ON) {
    cout << "Ending FORRANGE on " << var << endl;
  }
}


///////////////////  Private Methods ///////////////////////////

cString cAnalyze::PopDirectory(cString in_string, const cString default_dir)
{
  // Determing the directory name
  cString directory(default_dir);
  if (in_string.GetSize() != 0) directory = in_string.PopWord();
  
  // Make sure the directory ends in a slash.  If not, add one.
  int last_pos = directory.GetSize() - 1;
  if (directory[last_pos] != '/' && directory[last_pos] != '\\') {
    directory += '/';
  }
  
  return directory;
}

int cAnalyze::PopBatch(const cString & in_string)
{
  int batch = cur_batch;
  if (in_string.GetSize() != 0 && in_string != "current") {
    batch = in_string.AsInt();
  }
  
  return batch;
}

cAnalyzeGenotype * cAnalyze::PopGenotype(cString gen_desc, int batch_id)
{
  if (batch_id == -1) batch_id = cur_batch;
  tListPlus<cAnalyzeGenotype> & gen_list = batch[batch_id].List();
  gen_desc.ToLower();
  
  cAnalyzeGenotype * found_gen = NULL;
  if (gen_desc == "num_cpus")
    found_gen = gen_list.PopIntMax(&cAnalyzeGenotype::GetNumCPUs);
  else if (gen_desc == "total_cpus")
    found_gen = gen_list.PopIntMax(&cAnalyzeGenotype::GetTotalCPUs);
  else if (gen_desc == "merit")
    found_gen = gen_list.PopDoubleMax(&cAnalyzeGenotype::GetMerit);
  else if (gen_desc == "fitness")
    found_gen = gen_list.PopDoubleMax(&cAnalyzeGenotype::GetFitness);
  else if (gen_desc.IsNumeric(0))
    found_gen = gen_list.PopIntValue(&cAnalyzeGenotype::GetID,
                                     gen_desc.AsInt());
  else if (gen_desc == "random") {
    int gen_pos = random.GetUInt(gen_list.GetSize());
    found_gen = gen_list.PopPos(gen_pos);
  }
  else {
    cout << "  Error: unknown type " << gen_desc << endl;
    if (exit_on_error) exit(1);
  }
  
  return found_gen;
}


cString& cAnalyze::GetVariable(const cString & var)
{
  if (var.GetSize() != 1 ||
      (var.IsLetter(0) == false && var.IsNumeric(0) == false)) {
    cerr << "Error: Illegal variable " << var << " being used." << endl;
    if (exit_on_error) exit(1);
  }
  
  if (var.IsLowerLetter(0) == true) {
    int var_id = (int) (var[0] - 'a');
    return variables[var_id];
  }
  else if (var.IsUpperLetter(0) == true) {
    int var_id = (int) (var[0] - 'A');
    return local_variables[var_id];
  }
  // Otherwise it must be a number...
  int var_id = (int) (var[0] - '0');
  return arg_variables[var_id];
}


int cAnalyze::LoadCommandList(cInitFile& init_file, tList<cAnalyzeCommand>& clist, int start_at)
{
  for (int i = start_at; i < init_file.GetNumLines(); i++) {
    cString cur_string = init_file.GetLine(i);
    cString command = cur_string.PopWord();
    
    cAnalyzeCommand* cur_command;
    cAnalyzeCommandDefBase* command_def = FindAnalyzeCommandDef(command);
    
    if (command == "END") {
      // We are done with this section of code; break out...
      return i;
    } else if (command_def != NULL && command_def->IsFlowCommand() == true) {
      // This code has a body to it... fill it out!
      cur_command = new cAnalyzeFlowCommand(command, cur_string);
      i = LoadCommandList(init_file, *(cur_command->GetCommandList()), i + 1); // Start processing at the next line
    } else {
      // This is a normal command...
      cur_command = new cAnalyzeCommand(command, cur_string);
    }
    
    clist.PushRear(cur_command);
  }
  
  return init_file.GetNumLines();
}

void cAnalyze::InteractiveLoadCommandList(tList<cAnalyzeCommand> & clist)
{
  interactive_depth++;
  char text_input[2048];
  while (true) {
    for (int i = 0; i <= interactive_depth; i++) {
      cout << ">>";
    }
    cout << " ";
    cout.flush();
    cin.getline(text_input, 2048);
    cString cur_input(text_input);
    cString command = cur_input.PopWord();
    
    cAnalyzeCommand * cur_command;
    cAnalyzeCommandDefBase * command_def = FindAnalyzeCommandDef(command);
    
    if (command == "END") {
      // We are done with this section of code; break out...
      break;
    }
    else if (command_def != NULL && command_def->IsFlowCommand() == true) {
      // This code has a body to it... fill it out!
      cur_command = new cAnalyzeFlowCommand(command, cur_input);
      InteractiveLoadCommandList(*(cur_command->GetCommandList()));
    }
    else {
      // This is a normal command...
      cur_command = new cAnalyzeCommand(command, cur_input);
    }
    
    clist.PushRear(cur_command);
  }
  interactive_depth--;
}

void cAnalyze::PreProcessArgs(cString & args)
{
  int pos = 0;
  int search_start = 0;
  while ((pos = args.Find('$', search_start)) != -1) {
    // Setup the variable name that was found...
    char varlet = args[pos+1];
    cString varname("$");
    varname += varlet;
    
    // Determine the variable and act on it.
    int varsize = 0;
    if (varlet == '$') {
      args.Clip(pos+1, 1);
      varsize = 1;
    }
    else if (varlet >= 'a' && varlet <= 'z') {
      int var_id = (int) (varlet - 'a');
      args.Replace(varname, variables[var_id], pos);
      varsize = variables[var_id].GetSize();
    }
    else if (varlet >= 'A' && varlet <= 'Z') {
      int var_id = (int) (varlet - 'A');
      args.Replace(varname, local_variables[var_id], pos);
      varsize = local_variables[var_id].GetSize();
    }
    else if (varlet >= '0' && varlet <= '9') {
      int var_id = (int) (varlet - '0');
      args.Replace(varname, arg_variables[var_id], pos);
      varsize = arg_variables[var_id].GetSize();
    }
    search_start = pos + varsize;
  }
}

void cAnalyze::ProcessCommands(tList<cAnalyzeCommand>& clist)
{
  // Process the command list...
  tListIterator<cAnalyzeCommand> command_it(clist);
  command_it.Reset();
  cAnalyzeCommand* cur_command = NULL;
  while ((cur_command = command_it.Next()) != NULL) {
    cString command = cur_command->GetCommand();
    cString args = cur_command->GetArgs();
    PreProcessArgs(args);
    
    cAnalyzeCommandDefBase* command_fun = FindAnalyzeCommandDef(command);
    
    if (command_fun != NULL) command_fun->Run(this, args, *cur_command);
    else if (!FunctionRun(command, args)) {
      cerr << "Error: Unknown analysis keyword '" << command << "'." << endl;
      if (exit_on_error) exit(1);
    }    
  }
}


// The following function will print a cell in a table with a background color based on a comparison
// with its parent (the result of which is passed in as the 'compare' argument).  The cell_flags argument
// includes any other information you want in the <td> tag; 'null_text' is the text you want to replace a
// zero with (sometime "none" or "N/A"); and 'print_text' is a bool asking if the text should be included at
// all, or just the background color.

void cAnalyze::HTMLPrintStat(const cFlexVar & value, std::ostream& fp, int compare,
                             const cString & cell_flags, const cString & null_text, bool print_text)
{
  fp << "<td " << cell_flags << " ";
  if (compare == COMPARE_RESULT_OFF) {
    fp << "bgcolor=\"#" << m_world->GetConfig().COLOR_NEG2.Get() << "\">";
    if (print_text == true) fp << null_text << " ";
    else fp << "&nbsp; ";
    return;
  }
  
  if (compare == COMPARE_RESULT_NEG)       fp << "bgcolor=\"#" << m_world->GetConfig().COLOR_NEG1.Get() << "\">";
  else if (compare == COMPARE_RESULT_SAME) fp << "bgcolor=\"#" << m_world->GetConfig().COLOR_SAME.Get() << "\">";
  else if (compare == COMPARE_RESULT_POS)  fp << "bgcolor=\"#" << m_world->GetConfig().COLOR_POS1.Get() << "\">";
  else if (compare == COMPARE_RESULT_ON)   fp << "bgcolor=\"#" << m_world->GetConfig().COLOR_POS2.Get() << "\">";
  else if (compare == COMPARE_RESULT_DIFF) fp << "bgcolor=\"#" << m_world->GetConfig().COLOR_DIFF.Get() << "\">";
  else {
    std::cerr << "Error! Illegal case in Compare:" << compare << std::endl;
    exit(0);
  }
  
  if (print_text == true) fp << value << " ";
  else fp << "&nbsp; ";
  
}

int cAnalyze::CompareFlexStat(const cFlexVar & org_stat, const cFlexVar & parent_stat, int compare_type)
{
  // If no comparisons need be done, return zero and stop here.
  if (compare_type == FLEX_COMPARE_NONE) {
    return COMPARE_RESULT_SAME;
  }
  
  // In all cases, if the stats are the same, we should return this and stop.
  if (org_stat == parent_stat) return COMPARE_RESULT_SAME;
  
  // If we made it this far and all we care about is if they differ, return that they do.
  if (compare_type == FLEX_COMPARE_DIFF) return COMPARE_RESULT_DIFF;
  
  // If zero is not special we can calculate our result.
  if (compare_type == FLEX_COMPARE_MAX) {     // Color higher values as beneficial, lower as harmful.
    if (org_stat > parent_stat) return COMPARE_RESULT_POS;
    return COMPARE_RESULT_NEG;
  }
  if (compare_type == FLEX_COMPARE_MIN) {     // Color lower values as beneficial, higher as harmful.
    if (org_stat > parent_stat) return COMPARE_RESULT_NEG;
    return COMPARE_RESULT_POS;
  }
  
  
  // If we made it this far, it means that zero has a special status.
  if (org_stat == 0) return COMPARE_RESULT_OFF;
  if (parent_stat == 0) return COMPARE_RESULT_ON;
  
  
  // No zeros are involved, so we can go back to basic checks...
  if (compare_type == FLEX_COMPARE_DIFF2) return COMPARE_RESULT_DIFF;
  
  if (compare_type == FLEX_COMPARE_MAX2) {     // Color higher values as beneficial, lower as harmful.
    if (org_stat > parent_stat) return COMPARE_RESULT_POS;
    return COMPARE_RESULT_NEG;
  }
  if (compare_type == FLEX_COMPARE_MIN2) {     // Color lower values as beneficial, higher as harmful.
    if (org_stat > parent_stat) return COMPARE_RESULT_NEG;
    return COMPARE_RESULT_POS;
  }
  
  assert(false);  // One of the other options should have been chosen.
  return 0;
}



// A basic macro to link a keyword to a description and Get and Set methods in cAnalyzeGenotype.
#define ADD_GDATA(TYPE, KEYWORD, DESC, GET, SET, COMP, NSTR, HSTR)                                         \
{                                                                                                          \
  cString nstr_str(#NSTR), hstr_str(#HSTR);                                                                \
    cString null_str = "0";                                                                                  \
      if (nstr_str != "0") null_str = NSTR;                                                                    \
        cString html_str = "align=center";                                                                       \
          if (hstr_str != "0") html_str = HSTR;                                                                    \
            \
            genotype_data_list.PushRear(new tDataEntry<cAnalyzeGenotype, TYPE>                                       \
                                        (KEYWORD, DESC, &cAnalyzeGenotype::GET, &cAnalyzeGenotype::SET, COMP, null_str, html_str)); \
}


void cAnalyze::SetupGenotypeDataList()
{
  if (genotype_data_list.GetSize() != 0) return; // List already setup.
  
  // To add a new keyword connected to a stat in cAnalyzeGenotype, you need to connect all of the pieces here.
  // The ADD_GDATA macro takes eight arguments:
  //  type              : The type of the variables being linked in.
  //  keyword           : The short word used to reference this variable from analyze mode.
  //  description       : A slightly fuller description of what this variable is; used in data legends.
  //  "get" accessor    : The accessor method to retrieve the value of this variable from cAnalyzeGenotype
  //  "set" accessor    : The method to set this variable in cAnalyzeGenotype (use SetNULL if none exists).
  //  comparison method : A method that will take two genotypes and compare this value bewtween them (or CompareNULL)
  //  null keyword      : A string to represent what should be printed if this stat is zero. (0 for default)
  //  html flags        : A string to be included in the <td> when stat is printed in HTML table (0 for "align=center")
  
  // As a reminder about the compare types:
  //   FLEX_COMPARE_NONE   = 0  -- No comparisons should be done at all.
  //   FLEX_COMPARE_DIFF   = 1  -- Only track if a stat has changed, don't worry about direction.
  //   FLEX_COMPARE_MAX    = 2  -- Color higher values as beneficial, lower as harmful.
  //   FLEX_COMPARE_MIN    = 3  -- Color lower values as beneficial, higher as harmful.
  //   FLEX_COMPARE_DIFF2  = 4  -- Same as FLEX_COMPARE_DIFF, but 0 indicates trait is off.
  //   FLEX_COMPARE_MAX2   = 5  -- Same as FLEX_COMPARE_MAX, and 0 indicates trait is off.
  //   FLEX_COMPARE_MIN2   = 6  -- Same as FLEX_COMPARE_MIN, BUT 0 still indicates off.
  
  ADD_GDATA(const cString&, "name", "Genotype Name",                 GetName,           SetName,       0, 0, 0);
  ADD_GDATA(bool,   "viable",       "Is Viable (0/1)",               GetViable,         SetViable,     5, 0, 0);
  ADD_GDATA(int,    "id",           "Genotype ID",                   GetID,             SetID,         0, 0, 0);
  ADD_GDATA(const cString &, "tag", "Genotype Tag",                  GetTag,            SetTag,        0, "(none)","");
  ADD_GDATA(int,    "parent_id",    "Parent ID",                     GetParentID,       SetParentID,   0, 0, 0);
  ADD_GDATA(int,    "parent2_id",   "Second Parent ID (sexual orgs)",GetParent2ID,      SetParent2ID,  0, 0, 0);
  ADD_GDATA(int,    "parent_dist",  "Parent Distance",               GetParentDist,     SetParentDist, 0, 0, 0);
  ADD_GDATA(int,    "ancestor_dist","Ancestor Distance",             GetAncestorDist,   SetAncestorDist, 0, 0, 0);
  ADD_GDATA(int,    "lineage",      "Unique Lineage Label",          GetLineageLabel,   SetLineageLabel, 0, 0, 0);
  ADD_GDATA(int,    "num_cpus",     "Number of CPUs",                GetNumCPUs,        SetNumCPUs,    0, 0, 0);
  ADD_GDATA(int,    "total_cpus",   "Total CPUs Ever",               GetTotalCPUs,      SetTotalCPUs,  0, 0, 0);
  ADD_GDATA(int,    "length",       "Genome Length",                 GetLength,         SetLength,     4, 0, 0);
  ADD_GDATA(int,    "copy_length",  "Copied Length",                 GetCopyLength,     SetCopyLength, 0, 0, 0);
  ADD_GDATA(int,    "exe_length",   "Executed Length",               GetExeLength,      SetExeLength,  0, 0, 0);
  ADD_GDATA(double, "merit",        "Merit",                         GetMerit,          SetMerit,      5, 0, 0);
  ADD_GDATA(double, "comp_merit",   "Computational Merit",           GetCompMerit,      SetNULL,       5, 0, 0);
  ADD_GDATA(double, "comp_merit_ratio", "Computational Merit Ratio", GetCompMeritRatio, SetNULL,       5, 0, 0);
  ADD_GDATA(int,    "gest_time",    "Gestation Time",                GetGestTime,       SetGestTime,   6, "Inf", 0);
  ADD_GDATA(double, "efficiency",   "Rep. Efficiency",               GetEfficiency,     SetNULL,       5, 0, 0);
  ADD_GDATA(double, "efficiency_ratio", "Rep. Efficiency Ratio",     GetEfficiencyRatio,SetNULL,       5, 0, 0);
  ADD_GDATA(double, "fitness",      "Fitness",                       GetFitness,        SetFitness,    5, 0, 0);
  ADD_GDATA(double, "div_type",     "Divide Type",                   GetDivType,        SetDivType,    0, 0, 0);
  ADD_GDATA(int,    "mate_id",      "Mate Selection ID Number",      GetMateID,         SetMateID,     0, 0, 0);
  ADD_GDATA(double, "fitness_ratio","Fitness Ratio",                 GetFitnessRatio,   SetNULL,       5, 0, 0);
  ADD_GDATA(int,    "update_born",  "Update Born",                   GetUpdateBorn,     SetUpdateBorn, 0, 0, 0);
  ADD_GDATA(int,    "update_dead",  "Update Dead",                   GetUpdateDead,     SetUpdateDead, 0, 0, 0);
  ADD_GDATA(int,    "depth",        "Tree Depth",                    GetDepth,          SetDepth,      0, 0, 0);
  ADD_GDATA(double, "frac_dead",    "Fraction Mutations Lethal",     GetFracDead,       SetNULL,       0, 0, 0);
  ADD_GDATA(double, "frac_neg",     "Fraction Mutations Detrimental",GetFracNeg,        SetNULL,       0, 0, 0);
  ADD_GDATA(double, "frac_neut",    "Fraction Mutations Neutral",    GetFracNeut,       SetNULL,       0, 0, 0);
  ADD_GDATA(double, "frac_pos",     "Fraction Mutations Beneficial", GetFracPos,        SetNULL,       0, 0, 0);
  ADD_GDATA(double, "complexity",   "Basic Complexity (beneficial muts are neutral)", GetComplexity, SetNULL, 0, 0, 0);
  ADD_GDATA(double, "land_fitness", "Average Lanscape Fitness",      GetLandscapeFitness, SetNULL,     0, 0, 0);
  
  ADD_GDATA(int,    "num_phen",           "Number of Plastic Phenotypes",          GetNumPhenotypes,          SetNULL, 0, 0, 0);
  ADD_GDATA(int,    "num_trials",         "Number of Recalculation Trials",        GetNumTrials,              SetNULL, 0, 0, 0);
  ADD_GDATA(double, "phen_entropy",       "Phenotpyic Entropy",                    GetPhenotypicEntropy,      SetNULL, 0, 0, 0);
  ADD_GDATA(double, "phen_max_fitness",   "Phen Plast Maximum Fitness",            GetMaximumFitness,         SetNULL, 0, 0, 0);
  ADD_GDATA(double, "phen_max_fit_freq",  "Phen Plast Maximum Fitness Frequency",  GetMaximumFitnessFrequency,SetNULL, 0, 0, 0);
  ADD_GDATA(double, "phen_min_fitness",   "Phen Plast Minimum Fitness",            GetMinimumFitness,         SetNULL, 0, 0, 0);
  ADD_GDATA(double, "phen_min_freq",      "Phen Plast Minimum Fitness Frequency",  GetMinimumFitnessFrequency,SetNULL, 0, 0, 0);
  ADD_GDATA(double, "phen_avg_fitness",   "Phen Plast Wtd Avg Fitness",            GetAverageFitness,         SetNULL, 0, 0, 0);
  ADD_GDATA(double, "phen_likely_freq",   "Freq of Most Likely Phenotype",         GetLikelyFrequency,        SetNULL, 0, 0, 0);
  ADD_GDATA(double, "phen_likely_fitness","Fitness of Most Likely Phenotype",      GetLikelyFitness,          SetNULL, 0, 0, 0);
  
  ADD_GDATA(const cString &, "parent_muts", "Mutations from Parent", GetParentMuts,   SetParentMuts, 0, "(none)", "");
  ADD_GDATA(const cString &, "task_order", "Task Performance Order", GetTaskOrder,    SetTaskOrder,  0, "(none)", "");
  ADD_GDATA(cString, "sequence",    "Genome Sequence",               GetSequence,     SetSequence,   0, "(N/A)", "");
  ADD_GDATA(const cString &, "alignment", "Aligned Sequence",        GetAlignedSequence, SetAlignedSequence, 0, "(N/A)", "");
  
  ADD_GDATA(cString, "executed_flags", "Executed Flags",             GetExecutedFlags, SetNULL, 0, "(N/A)", "");
  ADD_GDATA(cString, "alignment_executed_flags", "Alignment Executed Flags", GetAlignmentExecutedFlags, SetNULL, 0, "(N/A)", "");
  ADD_GDATA(cString, "task_list", "List of all tasks performed",     GetTaskList,     SetNULL, 0, "(N/A)", "");
  ADD_GDATA(cString, "link.tasksites", "Phenotype Map",              GetMapLink,      SetNULL, 0, 0,       0);
  ADD_GDATA(cString, "html.sequence",  "Genome Sequence",            GetHTMLSequence, SetNULL, 0, "(N/A)", "");
  
  // coarse-grained task stats
  ADD_GDATA(int, 		"total_task_count","# Different Tasks", 		GetTotalTaskCount, SetNULL, 1, 0, 0);
  ADD_GDATA(int, 		"total_task_performance_count", "Total Tasks Performed",	GetTotalTaskPerformanceCount, SetNULL, 1, 0, 0);
  
  const cEnvironment& environment = m_world->GetEnvironment();
  for (int i = 0; i < environment.GetNumTasks(); i++) {
    cString t_name, t_desc;
    t_name.Set("task.%d", i);
    t_desc = environment.GetTask(i).GetDesc();
    genotype_data_list.PushRear(new tArgDataEntry<cAnalyzeGenotype, int, int>
                                (t_name, t_desc, &cAnalyzeGenotype::GetTaskCount, i, 5));
  }
  
  for (int i = 0; i < environment.GetInputSize(); i++){
    cString t_name, t_desc;
    t_name.Set("env_input.%d", i);
    t_desc.Set("env_input.%d", i);
    genotype_data_list.PushRear(new tArgDataEntry<cAnalyzeGenotype, int, int>
                                (t_name, t_desc, &cAnalyzeGenotype::GetEnvInput, i, 0));
  }
  
  // The remaining values should actually go in a seperate list called
  // "population_data_list", but for the moment we're going to put them
  // here so that we only need to worry about a single system to load and
  // save genotype information.
  ADD_GDATA(int, "update",       "Update Output",                   GetUpdateDead, SetUpdateDead, 0, 0, 0);
  ADD_GDATA(int, "dom_num_cpus", "Number of Dominant Organisms",    GetNumCPUs,    SetNumCPUs,    0, 0, 0);
  ADD_GDATA(int, "dom_depth",    "Tree Depth of Dominant Genotype", GetDepth,      SetDepth,      0, 0, 0);
  ADD_GDATA(int, "dom_id",       "Dominant Genotype ID",            GetID,         SetID,         0, 0, 0);
  ADD_GDATA(cString, "dom_sequence", "Dominant Genotype Sequence",  GetSequence,   SetSequence,   0, "(N/A)", "");
}


// Find a data entry bassed on a keywrod.
tDataEntryCommand<cAnalyzeGenotype> * cAnalyze::GetGenotypeDataCommand(const cString & stat_entry) 
{
  // Make sure we have all of the possibilities loaded...
  SetupGenotypeDataList();
  
  // Get the name from the beginning of the entry; everything else is arguments.
  cString arg_list = stat_entry;
  cString stat_name = arg_list.Pop(':');
  
  // Create an iterator to scan the genotype data list for the current entry.
  tListIterator< tDataEntryBase<cAnalyzeGenotype> > genotype_data_it(genotype_data_list);
  
  while (genotype_data_it.Next() != (void *) NULL) {
    if (genotype_data_it.Get()->GetName() == stat_name) {
      return new tDataEntryCommand<cAnalyzeGenotype>(genotype_data_it.Get(), arg_list);
    }
  }
  
  return NULL;
}


// Pass in the arguments for a command and fill out the entries in list
// format....

void cAnalyze::LoadGenotypeDataList(cStringList arg_list,
                                    tList< tDataEntryCommand<cAnalyzeGenotype> > & output_list)
{
  // Make sure we have all of the possibilities loaded...
  SetupGenotypeDataList();
  
  // If no args were given, load all of the stats.
  if (arg_list.GetSize() == 0) {
    tListIterator< tDataEntryBase<cAnalyzeGenotype> >
    genotype_data_it(genotype_data_list);
    while (genotype_data_it.Next() != (void *) NULL) {
      tDataEntryCommand<cAnalyzeGenotype> * entry_command =
      new tDataEntryCommand<cAnalyzeGenotype>(genotype_data_it.Get());
      output_list.PushRear(entry_command);
    }
  }
  // Otherwise, load only those listed.
  else {
    while (arg_list.GetSize() != 0) {
      // Setup the next entry
      cString cur_args = arg_list.Pop();
      cString cur_entry = cur_args.Pop(':');
      bool found_entry = false;
      
      // Scan the genotype data list for the current entry
      tListIterator< tDataEntryBase<cAnalyzeGenotype> >
        genotype_data_it(genotype_data_list);
      
      while (genotype_data_it.Next() != (void *) NULL) {
        if (genotype_data_it.Get()->GetName() == cur_entry) {
          tDataEntryCommand<cAnalyzeGenotype> * entry_command =
          new tDataEntryCommand<cAnalyzeGenotype>
          (genotype_data_it.Get(), cur_args);
          output_list.PushRear(entry_command);
          found_entry = true;
          break;
        }
      }
      
      // If the entry was not found, give a warning.
      if (found_entry == false) {
        int best_match = 1000;
        cString best_entry;
        
        genotype_data_it.Reset();
        while (genotype_data_it.Next() != (void *) NULL) {
          const cString & test_str = genotype_data_it.Get()->GetName();
          const int test_dist = cStringUtil::EditDistance(test_str, cur_entry);
          if (test_dist < best_match) {
            best_match = test_dist;
            best_entry = test_str;
          }
        }	
        
        cerr << "Warning: Format entry \"" << cur_entry
          << "\" not found.  Best match is \""
          << best_entry << "\"." << endl;
      }
      
    }
  }
}





void cAnalyze::AddLibraryDef(const cString & name,
                             void (cAnalyze::*_fun)(cString))
{
  command_lib.PushRear(new cAnalyzeCommandDef(name, _fun));
}

void cAnalyze::AddLibraryDef(const cString & name,
                             void (cAnalyze::*_fun)(cString, tList<cAnalyzeCommand> &))
{
  command_lib.PushRear(new cAnalyzeFlowCommandDef(name, _fun));
}

void cAnalyze::SetupCommandDefLibrary()
{
  if (command_lib.GetSize() != 0) return; // Library already setup.
  
  AddLibraryDef("LOAD_ORGANISM", &cAnalyze::LoadOrganism);
  AddLibraryDef("LOAD_BASE_DUMP", &cAnalyze::LoadBasicDump);
  AddLibraryDef("LOAD_DETAIL_DUMP", &cAnalyze::LoadDetailDump);
  AddLibraryDef("LOAD_MULTI_DETAIL", &cAnalyze::LoadMultiDetail);
  AddLibraryDef("LOAD_SEQUENCE", &cAnalyze::LoadSequence);
  AddLibraryDef("LOAD_DOMINANT", &cAnalyze::LoadDominant);
  AddLibraryDef("LOAD_RESOURCES", &cAnalyze::LoadResources);
  AddLibraryDef("LOAD", &cAnalyze::LoadFile);
  
  // Reduction commands...
  AddLibraryDef("FILTER", &cAnalyze::CommandFilter);
  AddLibraryDef("FIND_GENOTYPE", &cAnalyze::FindGenotype);
  AddLibraryDef("FIND_ORGANISM", &cAnalyze::FindOrganism);
  AddLibraryDef("FIND_LINEAGE", &cAnalyze::FindLineage);
  AddLibraryDef("FIND_SEX_LINEAGE", &cAnalyze::FindSexLineage);
  AddLibraryDef("FIND_CLADE", &cAnalyze::FindClade);
  AddLibraryDef("SAMPLE_ORGANISMS", &cAnalyze::SampleOrganisms);
  AddLibraryDef("SAMPLE_GENOTYPES", &cAnalyze::SampleGenotypes);
  AddLibraryDef("KEEP_TOP", &cAnalyze::KeepTopGenotypes);
  AddLibraryDef("TRUNCATELINEAGE", &cAnalyze::TruncateLineage); // Depricate!
  AddLibraryDef("TRUNCATE_LINEAGE", &cAnalyze::TruncateLineage);
  
  // Direct output commands...
  AddLibraryDef("PRINT", &cAnalyze::CommandPrint);
  AddLibraryDef("TRACE", &cAnalyze::CommandTrace);
  AddLibraryDef("PRINT_TASKS", &cAnalyze::CommandPrintTasks);
  AddLibraryDef("PRINT_TASKS_QUALITY", &cAnalyze::CommandPrintTasksQuality);
  AddLibraryDef("DETAIL", &cAnalyze::CommandDetail);
  AddLibraryDef("DETAIL_TIMELINE", &cAnalyze::CommandDetailTimeline);
  AddLibraryDef("DETAIL_BATCHES", &cAnalyze::CommandDetailBatches);
  AddLibraryDef("DETAIL_AVERAGE", &cAnalyze::CommandDetailAverage);
  AddLibraryDef("DETAIL_INDEX", &cAnalyze::CommandDetailIndex);
  AddLibraryDef("HISTOGRAM", &cAnalyze::CommandHistogram);
  
  // Population analysis commands...
  AddLibraryDef("PRINT_PHENOTYPES", &cAnalyze::CommandPrintPhenotypes);
  AddLibraryDef("PRINT_DIVERSITY", &cAnalyze::CommandPrintDiversity);
  AddLibraryDef("PRINT_TREE_STATS", &cAnalyze::CommandPrintTreeStats);
  AddLibraryDef("PRINT_CUMULATIVE_STEMMINESS", &cAnalyze::CommandPrintCumulativeStemminess);
  AddLibraryDef("PRINT_GAMMA", &cAnalyze::CommandPrintGamma);
  AddLibraryDef("COMMUNITY_COMPLEXITY", &cAnalyze::AnalyzeCommunityComplexity);
  AddLibraryDef("PRINT_RESOURCE_FITNESS_MAP", &cAnalyze::CommandPrintResourceFitnessMap);
  
  // Individual organism analysis...
  AddLibraryDef("FITNESS_MATRIX", &cAnalyze::CommandFitnessMatrix);
  AddLibraryDef("MAP", &cAnalyze::CommandMapTasks);  // Deprecated...
  AddLibraryDef("MAP_TASKS", &cAnalyze::CommandMapTasks);
  AddLibraryDef("AVERAGE_MODULARITY", &cAnalyze::CommandAverageModularity);
  AddLibraryDef("MAP_MUTATIONS", &cAnalyze::CommandMapMutations);
  AddLibraryDef("ANALYZE_COMPLEXITY", &cAnalyze::AnalyzeComplexity);
  AddLibraryDef("ANALYZE_FITNESS_TWO_SITES", &cAnalyze::AnalyzeFitnessLandscapeTwoSites);
  AddLibraryDef("ANALYZE_COMPLEXITY_TWO_SITES", &cAnalyze::AnalyzeComplexityTwoSites);
  AddLibraryDef("ANALYZE_KNOCKOUTS", &cAnalyze::AnalyzeKnockouts);
  AddLibraryDef("ANALYZE_POP_COMPLEXITY", &cAnalyze::AnalyzePopComplexity);
  AddLibraryDef("MAP_DEPTH", &cAnalyze::CommandMapDepth);
  // (Untested) AddLibraryDef("PAIRWISE_ENTROPY", &cAnalyze::CommandPairwiseEntropy); 
  
  // Population comparison commands...
  AddLibraryDef("HAMMING", &cAnalyze::CommandHamming);
  AddLibraryDef("LEVENSTEIN", &cAnalyze::CommandLevenstein);
  AddLibraryDef("SPECIES", &cAnalyze::CommandSpecies);
  AddLibraryDef("RECOMBINE", &cAnalyze::CommandRecombine);
  
  // Lineage analysis commands...
  AddLibraryDef("ALIGN", &cAnalyze::CommandAlign);
  AddLibraryDef("ANALYZE_NEWINFO", &cAnalyze::AnalyzeNewInfo);
  AddLibraryDef("MUTATION_REVERT", &cAnalyze::MutationRevert);
  
  // Build input files for avida...
  AddLibraryDef("WRITE_CLONE", &cAnalyze::WriteClone);
  AddLibraryDef("WRITE_INJECT_EVENTS", &cAnalyze::WriteInjectEvents);
  AddLibraryDef("WRITE_COMPETITION", &cAnalyze::WriteCompetition);
  
  // Automated analysis
  AddLibraryDef("ANALYZE_MUTS", &cAnalyze::AnalyzeMuts);
  AddLibraryDef("ANALYZE_INSTRUCTIONS", &cAnalyze::AnalyzeInstructions);
  AddLibraryDef("ANALYZE_INST_POP", &cAnalyze::AnalyzeInstPop);
  AddLibraryDef("ANALYZE_BRANCHING", &cAnalyze::AnalyzeBranching);
  AddLibraryDef("ANALYZE_MUTATION_TRACEBACK",
                &cAnalyze::AnalyzeMutationTraceback);
  AddLibraryDef("ANALYZE_MATE_SELECTION", &cAnalyze::AnalyzeMateSelection);
  AddLibraryDef("ANALYZE_COMPLEXITY_DELTA", &cAnalyze::AnalyzeComplexityDelta);
  
  // Environment manipulation
  AddLibraryDef("ENVIRONMENT", &cAnalyze::EnvironmentSetup);
  
  // Documantation...
  AddLibraryDef("HELPFILE", &cAnalyze::CommandHelpfile);
  
  // Control commands...
  AddLibraryDef("SET", &cAnalyze::VarSet);
  AddLibraryDef("CONFIG_GET", &cAnalyze::ConfigGet);
  AddLibraryDef("CONFIG_SET", &cAnalyze::ConfigSet);
  AddLibraryDef("SET_BATCH", &cAnalyze::BatchSet);
  AddLibraryDef("NAME_BATCH", &cAnalyze::BatchName);
  AddLibraryDef("TAG_BATCH", &cAnalyze::BatchTag);
  AddLibraryDef("PURGE_BATCH", &cAnalyze::BatchPurge);
  AddLibraryDef("DUPLICATE", &cAnalyze::BatchDuplicate);
  AddLibraryDef("RECALCULATE", &cAnalyze::BatchRecalculate);
  AddLibraryDef("RECALC", &cAnalyze::BatchRecalculateWithArgs);
  AddLibraryDef("RENAME", &cAnalyze::BatchRename);
  AddLibraryDef("STATUS", &cAnalyze::PrintStatus);
  AddLibraryDef("ECHO", &cAnalyze::PrintDebug);
  AddLibraryDef("DEBUG", &cAnalyze::PrintDebug);
  AddLibraryDef("TEST", &cAnalyze::PrintTestInfo);
  AddLibraryDef("INCLUDE", &cAnalyze::IncludeFile);
  AddLibraryDef("RUN", &cAnalyze::IncludeFile);
  AddLibraryDef("SYSTEM", &cAnalyze::CommandSystem);
  AddLibraryDef("INTERACTIVE", &cAnalyze::CommandInteractive);
  
  // Functions...
  AddLibraryDef("FUNCTION", &cAnalyze::FunctionCreate);
  
  // Flow commands...
  AddLibraryDef("FOREACH", &cAnalyze::CommandForeach);
  AddLibraryDef("FORRANGE", &cAnalyze::CommandForRange);
  
  // Uncategorized commands...
  AddLibraryDef("COMPETE", &cAnalyze::BatchCompete);
}

cAnalyzeCommandDefBase* cAnalyze::FindAnalyzeCommandDef(const cString& name)
{
  SetupCommandDefLibrary();
  
  cString uppername(name);
  uppername.ToUpper();
  tListIterator<cAnalyzeCommandDefBase> lib_it(command_lib);
  while (lib_it.Next() != (void *) NULL) {
    if (lib_it.Get()->GetName() == uppername) break;
  }
  cAnalyzeCommandDefBase* command_def = lib_it.Get();
  
  if (command_def == NULL && m_world->GetActionLibrary().Supports(name)) {
    command_def = new cAnalyzeCommandAction(name, m_world);
    command_lib.PushRear(command_def);
  }
  
  return command_def;
}

void cAnalyze::RunInteractive()
{
  bool saved_analyze = m_ctx.GetAnalyzeMode();
  m_ctx.SetAnalyzeMode();
  
  cout << "Entering interactive mode..." << endl;
  
  char text_input[2048];
  while (true) {
    cout << ">> ";
    cout.flush();
    cin.getline(text_input, 2048);
    cString cur_input(text_input);
    cString command = cur_input.PopWord();
    
    cAnalyzeCommand* cur_command;
    cAnalyzeCommandDefBase* command_def = FindAnalyzeCommandDef(command);
    if (command == "") {
      // Don't worry about blank lines...
      continue;
    } else if (command == "END" || command == "QUIT" || command == "EXIT") {
      // We are done with interactive mode...
      break;
    } else if (command_def != NULL && command_def->IsFlowCommand() == true) {
      // This code has a body to it... fill it out!
      cur_command = new cAnalyzeFlowCommand(command, cur_input);
      InteractiveLoadCommandList(*(cur_command->GetCommandList()));
    } else {
      // This is a normal command...
      cur_command = new cAnalyzeCommand(command, cur_input);
    }
    
    cString args = cur_command->GetArgs();
    PreProcessArgs(args);
    
    cAnalyzeCommandDefBase* command_fun = FindAnalyzeCommandDef(command);
    
    // First check for built-in functions...
    if (command_fun != NULL) command_fun->Run(this, args, *cur_command);
    
    // Then for user defined functions
    else if (FunctionRun(command, args) == true) { }
    
    // Otherwise, give an error.
    else cerr << "Error: Unknown command '" << command << "'." << endl;
  }
  
  if (!saved_analyze) m_ctx.ClearAnalyzeMode();
}

bool cAnalyze::Send(const cString &text_input)
{
  cString cur_input(text_input);
  cString command = cur_input.PopWord();
  
  cAnalyzeCommand* cur_command = NULL;
  cAnalyzeCommandDefBase* command_def = FindAnalyzeCommandDef(command);
  if (command == "") {
    // Don't worry about blank lines...
    ;
  } else if (command_def != NULL && command_def->IsFlowCommand() == true) {
    // This code has a body to it... fill it out!
    cur_command = new cAnalyzeFlowCommand(command, cur_input);
    InteractiveLoadCommandList(*(cur_command->GetCommandList()));
  } else {
    // This is a normal command...
    cur_command = new cAnalyzeCommand(command, cur_input);
  }
  
  cString args = cur_command->GetArgs();
  PreProcessArgs(args);
  
  cAnalyzeCommandDefBase* command_fun = FindAnalyzeCommandDef(command);
  
  // First check for built-in functions...
  if (command_fun != NULL) command_fun->Run(this, args, *cur_command);
  
  // Then for user defined functions
  else if (FunctionRun(command, args) == true) { }
  
  // Otherwise, give an error.
  else {
    cerr << "Error: Unknown command '" << command << "'." << endl;
    return false;
  }
  
  return true;
}

bool cAnalyze::Send(const cStringList &list_input)
{
  bool did_succeed = true;
  cStringIterator list_it(list_input);
  while ( list_it.AtEnd() == false ) {
    list_it.Next();
    if( !Send(list_it.Get()) ) {
      did_succeed = false;
    }
  }
  return did_succeed;
}
