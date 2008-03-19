//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2001 California Institute of Technology             //
//                                                                          // 
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#include <fstream>

#include "symbol_util.hh"

#include "../main/genotype.hh"
#include "../main/organism.hh"
#include "../main/population_cell.hh"
#include "../main/species.hh"
#include "../main/config.hh"
#include "../cpu/hardware_base.hh"
#include "../cpu/hardware_4stack.hh"
#include "../cpu/hardware_cpu.hh"

using namespace std;


char cSymbolUtil::GetBasicSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
  const cOrganism & organism = *(cell.GetOrganism());
  return organism.GetGenotype()->GetSymbol();
}

char cSymbolUtil::GetSpeciesSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
  const cOrganism & organism = *(cell.GetOrganism());

  cSpecies * cur_species = organism.GetGenotype()->GetSpecies();
  if (cur_species == NULL) return '.';    // no species
  return cur_species->GetSymbol();        // symbol!
}

char cSymbolUtil::GetModifiedSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
  const cOrganism & organism = *(cell.GetOrganism());

  const bool modifier = organism.GetPhenotype().IsModifier();
  const bool modified = organism.GetPhenotype().IsModified();

  // 'I' = Injector     'H' = Host (Injected into)
  // 'B' = Both         '-' = Neither

  if (modifier == true && modified == true)  return 'B';
  if (modifier == true) return 'I'-6;
  if (modified == true) return 'H'-6;
  return '-';
}

char cSymbolUtil::GetResourceSymbol(const cPopulationCell & cell)
{
  // @CAO Not Implemented yet...
  if (cell.IsOccupied() == false) return ' ';
  return '.';
}

char cSymbolUtil::GetAgeSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
  const cOrganism & organism = *(cell.GetOrganism());

  const int age = organism.GetPhenotype().GetAge();
  if (age < 0) return '-';
  if (age < 10) return (char) ('0' + age);
  if (age < 20) return 'X';
  if (age < 80) return 'L';
  if (age < 200) return 'C';

  return '+';
}

char cSymbolUtil::GetBreedSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
  const cOrganism & organism = *(cell.GetOrganism());

  if (organism.GetPhenotype().ParentTrue() == true) return '*';
  return '-';
}

char cSymbolUtil::GetParasiteSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
  const cOrganism & organism = *(cell.GetOrganism());

  if (organism.GetPhenotype().IsParasite() == true) return '*';
  return '-';
}

char cSymbolUtil::GetMutSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
  const cOrganism & organism = *(cell.GetOrganism());

  if (organism.GetPhenotype().IsMutated() == true) return '*';
  return '-';
}

char cSymbolUtil::GetThreadSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
//  const cOrganism & organism = *(cell.GetOrganism());

//    int thread_count = organism->GetHardware()->GetNumThreads();
//    if (thread_count < 0) return '-';
//    if (thread_count < 10) return (char) ('0' + thread_count);
//    if (thread_count < 20) return 'X';
//    if (thread_count < 80) return 'L';
//    if (thread_count < 200) return 'C';
  //const cHardwareBase * hardware; 
  int num_threads;
  if(cConfig::GetHardwareType() == HARDWARE_TYPE_CPU_ORIGINAL)
    {
      //const cHardwareCPU & hard_cpu= (cHardwareCPU &) cell.GetOrganism()->GetHardware();
      //num_threads = hard_cpu.GetNumThreads();
      num_threads = ((cHardwareCPU &) cell.GetOrganism()->GetHardware()).GetNumThreads();
      return (char) ('0' + num_threads);
    }
  else
    {
      //const cHardware4Stack & hard_4stack= (cHardware4Stack &) cell.GetOrganism()->GetHardware();
      //num_threads = hard_4stack.GetNumThreads();
      num_threads = ((cHardware4Stack &) cell.GetOrganism()->GetHardware()).GetNumThreads();
      return (char) ('0' + num_threads);
    }
  //return '+';
}

char cSymbolUtil::GetLineageSymbol(const cPopulationCell & cell)
{
  if (cell.IsOccupied() == false) return ' ';
  const cOrganism & organism = *(cell.GetOrganism());

  return 'A' + (organism.GetLineageLabel() % 12);
}

