/*
 *  cPhenPlastGenotype.cpp
 *  Avida
 *
 *  Created by Matthew Rupp on 7/29/07.
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
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

#include "cPhenPlastGenotype.h"
#include <iostream>
#include <cmath>

cPhenPlastGenotype::cPhenPlastGenotype(const cGenome& in_genome, int num_trials, cWorld* world, cAvidaContext& ctx)
: m_genome(in_genome), m_num_trials(num_trials), m_world(world)
{
  cCPUTestInfo test_info;
  test_info.UseRandomInputs(true);
  Process(test_info, world, ctx);
}

cPhenPlastGenotype::cPhenPlastGenotype(const cGenome& in_genome, int num_trials, cCPUTestInfo& test_info, cWorld* world, cAvidaContext& ctx)
: m_genome(in_genome), m_num_trials(num_trials), m_world(world)
{
  // Override input mode if more than one recalculation requested
  if (num_trials > 1)  
    test_info.UseRandomInputs(true);
  Process(test_info, world, ctx);
}

cPhenPlastGenotype::~cPhenPlastGenotype()
{
  UniquePhenotypes::iterator it = m_unique.begin();
  while (it != m_unique.end()){
    delete *it;
    ++it;
  }
}

void cPhenPlastGenotype::Process(cCPUTestInfo& test_info, cWorld* world, cAvidaContext& ctx)
{
  if (m_num_trials > 1)
    test_info.UseRandomInputs(true);
  cTestCPU* test_cpu = m_world->GetHardwareManager().CreateTestCPU();
  for (int k = 0; k < m_num_trials; k++){
    test_cpu->TestGenome(ctx, test_info, m_genome);
    
    //Is this a new phenotype?
    UniquePhenotypes::iterator uit = m_unique.find(&test_info.GetTestPhenotype());
    if (uit == m_unique.end()){  // Yes, make a new entry for it
      cPlasticPhenotype* new_phen = new cPlasticPhenotype(test_info, m_num_trials);
      m_unique.insert( static_cast<cPhenotype*>(new_phen) );
    } else{   // No, add an observation to existing entry, make sure it is equivalent
      assert( static_cast<cPlasticPhenotype*>((*uit))->AddObservation(test_info) );
    }
  }
  // Update statistics
  UniquePhenotypes::iterator uit = m_unique.begin();
  m_max_fitness     =  -1.0;
  m_avg_fitness     =   0.0;
  m_likely_fitness  =  -1.0;
  m_max_freq        =   0.0;
  m_max_fit_freq    =   0.0;
  m_min_fit_freq    =   0.0;
  m_phenotypic_entropy = 0.0;
  m_min_fitness     = (*uit)->GetFitness();
  while(uit != m_unique.end()){
    cPlasticPhenotype* this_phen = static_cast<cPlasticPhenotype*>(*uit);
    double fit = this_phen->GetFitness();
    double freq = this_phen->GetFrequency();
    if (fit > m_max_fitness){
      m_max_fitness = fit;
      m_max_fit_freq = freq;
    }
    if (fit < m_min_fitness){
      m_min_fitness = fit;
      m_min_fit_freq = freq;
    }
    if (freq > m_max_freq){
      m_max_freq = freq;
      m_likely_fitness = fit;
    }
    m_avg_fitness += freq * fit;
    m_phenotypic_entropy -= freq * log(freq) / log(2);
    ++uit;
  }
  delete test_cpu;
}


const cPlasticPhenotype* cPhenPlastGenotype::GetPlasticPhenotype(int num) const
{
  assert(num >= 0 && num < (int) m_unique.size() && m_unique.size() > 0);
  UniquePhenotypes::const_iterator it = m_unique.begin();
  for (int k = 0; k < num; k++, it++);
  return static_cast<cPlasticPhenotype*>(*it);
}

const cPlasticPhenotype* cPhenPlastGenotype::GetMostLikelyPhenotype() const
{
  assert(m_unique.size() > 0);
  UniquePhenotypes::const_iterator it = m_unique.begin();
  UniquePhenotypes::const_iterator ret_it = it;
  for (int k = 0; k < (int) m_unique.size(); k++, it++)
    if ( static_cast<cPlasticPhenotype*>(*it)->GetFrequency() > 
         static_cast<cPlasticPhenotype*>(*ret_it)->GetFrequency() )
      ret_it = it;
  return static_cast<cPlasticPhenotype*>(*ret_it);
}

const cPlasticPhenotype* cPhenPlastGenotype::GetHighestFitnessPhenotype() const
{
  assert(m_unique.size() > 0);
  UniquePhenotypes::const_iterator it = m_unique.begin();
  UniquePhenotypes::const_iterator ret_it = it;
  for (int k = 0; k < (int) m_unique.size(); k++, it++)
    if ( static_cast<cPlasticPhenotype*>(*it)->GetFitness() > 
         static_cast<cPlasticPhenotype*>(*ret_it)->GetFitness() )
      ret_it = it;
  return static_cast<cPlasticPhenotype*>(*ret_it);
}