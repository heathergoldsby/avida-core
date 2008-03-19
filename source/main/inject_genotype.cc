//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2003 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#include <fstream>

#include "inject_genotype.hh"

#include "../tools/tools.hh"
#include "../tools/merit.hh"

#include "stats.hh"
#include "config.hh"
#include "genome_util.hh"
#include "../cpu/hardware_method.hh"
#include "organism.hh"
#include "phenotype.hh"

#include "../cpu/test_cpu.hh"


using namespace std;

/////////////////////////
//  cInjectGenotype_BirthData
/////////////////////////

cInjectGenotype_BirthData::cInjectGenotype_BirthData(int in_update_born)
  : update_born(in_update_born)
  , parent_id(-1)
  , gene_depth(0)
  , update_deactivated(-1)
  , parent_genotype(NULL)
  , num_offspring_genotypes(0)
{
}

cInjectGenotype_BirthData::~cInjectGenotype_BirthData()
{
}


///////////////////////////
//  cInjectGenotype
///////////////////////////

cInjectGenotype::cInjectGenotype(int in_update_born, int in_id)
  : genome(1)
  , name("p001-no_name")
  , flag_threshold(false)
  , is_active(true)
      , can_reproduce(false)
  , defer_adjust(0)
  , symbol(0)
  , birth_data(in_update_born)
  , num_injected(0)
  , last_num_injected(0)
  , total_injected(0)
  , next(NULL)
  , prev(NULL)
{
  static int next_id = 1;
  
  if ( in_id >= 0 )
    next_id = in_id;
  
  id_num = next_id++;
}

cInjectGenotype::~cInjectGenotype()
{
  // Reset some of the variables to make sure program will crash if a deleted
  // cell is read!
  symbol = '!';

  num_injected = -1;
  total_injected = -1;

  next = NULL;
  prev = NULL;
}

bool cInjectGenotype::SaveClone(ofstream & fp)
{
  fp << id_num         << " ";
  fp << genome.GetSize() << " ";

  for (int i = 0; i < genome.GetSize(); i++) {
    fp << ((int) genome[i].GetOp()) << " ";
  }

  return true;
}

bool cInjectGenotype::LoadClone(ifstream & fp)
{
  int genome_size = 0;

  fp >> id_num;
  fp >> genome_size;

  genome = cGenome(genome_size);
  for (int i = 0; i < genome_size; i++) {
    cInstruction temp_inst;
    int inst_op;
    fp >> inst_op;
    temp_inst.SetOp((UCHAR) inst_op);
    genome[i] = temp_inst;
    // @CAO add something here to load arguments for instructions.
  }

  return true;
}

bool cInjectGenotype::OK()
{
  bool ret_value = true;

  // Check the components...

  if (!genome.OK()) ret_value = false;

  // And the statistics
  assert( id_num >= 0 && num_injected >= 0 && total_injected >= 0 );
  assert( birth_data.update_born >= -1);

  return ret_value;
};

void cInjectGenotype::SetParent(cInjectGenotype * parent)
{
  birth_data.parent_genotype = parent;

  // If we have a real parent genotype, collect other data about parent.
  if (parent == NULL) return;
  birth_data.parent_id = parent->GetID();
  birth_data.gene_depth = parent->GetDepth() + 1;
  parent->AddOffspringGenotype();
}

void cInjectGenotype::Mutate()  // Check each location to be mutated.
{
  int i;

  for (i = 0; i < genome.GetSize(); i++) {
      genome[i].SetOp(g_random.GetUInt(cConfig::GetNumInstructions()));
    }
  
}

void cInjectGenotype::UpdateReset()
{
  last_num_injected = num_injected;
  birth_data.birth_track.Next();
  birth_data.death_track.Next();
}

void cInjectGenotype::SetGenome(const cGenome & in_genome)
{
  genome = in_genome;

  name.Set("p%03d-no_name", genome.GetSize());
}

void cInjectGenotype::Deactivate(int update)
{
  is_active = false;
  birth_data.update_deactivated = update;
}

int cInjectGenotype::AddParasite()
{
  total_injected++;
  return num_injected++;
}

int cInjectGenotype::RemoveParasite()
{
  //birth_data.death_track.Inc();
  return num_injected--;
}

