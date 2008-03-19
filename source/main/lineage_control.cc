//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2000 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#include "lineage_control.hh"

#ifndef LINEAGE_HH
#include "lineage.hh"
#endif

#ifndef STATS_HH
#include "stats.hh"
#endif

#ifndef CONFIG_HH
#include "config.hh"
#endif

#ifndef GENEBANK_HH
#include "genebank.hh"
#endif

#ifndef CPU_HH
#include "../cpu/cpu.hh"
#endif

#ifndef LANDSCAPE_HH
#include "landscape.hh" // for macro LANDSCAPE_NEUTRAL_MAX
#endif

/////////////////////
//  cLineageControl
/////////////////////

cLineageControl::cLineageControl( cGenebank * genebank )
  : m_best_lineage(NULL), m_max_fitness_lineage(NULL),
    m_dominant_lineage(NULL), m_genebank( genebank )
{
}

cLineageControl::~cLineageControl()
{
}


cLineage * 
cLineageControl::AddLineage( double start_fitness, int parent_lin_id, int id, double lineage_stat1, double lineage_stat2 )
{
  cLineage * new_lineage = new cLineage( start_fitness, parent_lin_id, id, lineage_stat1, lineage_stat2 );

  // the best/ dominant lineage are automatically corrected
  // when a creature is added to this lineage
  m_lineage_list.push_back(new_lineage);

  cStats::AddLineage();

  return new_lineage;
}


void
cLineageControl::AddCreaturePrivate( cGenotype *genotype,
				     cLineage * lineage )
{
  assert( lineage != 0 );

  // add to the lineage
  lineage->AddCreature( genotype );

  // This would be nice, but the current Avida code doesn't allow for it.
  // Try to implement it in a new version...
  // update the cpu
  //  cpu->SetLineage( lineage );
  //  cpu->SetLineageLabel( lineage->GetID() );

  // test whether this makes the new lineage the best
  if ( !m_best_lineage ||
       lineage->GetAveFitness() > m_best_lineage->GetAveFitness() )
    m_best_lineage = lineage;

  // test whether this makes the new lineage the dominant
  if ( !m_dominant_lineage ||
       lineage->GetNumCreatures() > m_dominant_lineage->GetNumCreatures() )
    m_dominant_lineage = lineage;

  // test whether thiw makes the new lineage the one with the maximum fitness
  if ( !m_max_fitness_lineage ||
       lineage->GetMaxFitness() > m_max_fitness_lineage->GetMaxFitness() )
    m_max_fitness_lineage = lineage;
}

void cLineageControl::UpdateLineages()
{
  m_best_lineage = NULL;
  m_dominant_lineage = NULL;
  m_max_fitness_lineage = NULL;

  list<cLineage *>::iterator it = m_lineage_list.begin();
  list<cLineage *>::iterator del_it;

  while( it != m_lineage_list.end() ){
    bool del = false;

    // mark the lineage for removal if empty
    if ( (*it)->GetNumCreatures() == 0 ){
      del_it = it;
      del = true;
    }
    else { // otherwise it is a candidate for the best/ dominant/... lineage
      if ( !m_best_lineage ||
	   (*it)->GetAveFitness() > m_best_lineage->GetAveFitness() )
	m_best_lineage = (*it);

      if ( !m_dominant_lineage ||
	   (*it)->GetNumCreatures() > m_dominant_lineage->GetNumCreatures() )
	m_dominant_lineage = (*it);

      if ( !m_max_fitness_lineage ||
	   (*it)->GetMaxFitness() > m_max_fitness_lineage->GetMaxFitness() )
	m_max_fitness_lineage = (*it);

    }
    // proceed to the next lineage
    it++;

    // now do the removal if necessary
    if ( del ){
      delete (*del_it); // delete the lineage
      m_lineage_list.erase( del_it ); // and remove its reference
    }
  }

#ifdef DEBUG
  if ( !m_lineage_list.empty() ){
    assert( m_dominant_lineage != 0 );
    assert( m_best_lineage != 0 );
    assert( m_max_fitness_lineage != 0 );
  }
#endif

}


cLineage*
cLineageControl::AddCreature( cGenotype * child_genotype, cGenotype *parent_genotype, cLineage * parent_lineage, int parent_lin_id )
{
  // Collect any information we can about the parent.
  double parent_fitness = 0.0;
  //  int parent_lin_id = 0;

  // at this point, the cpu has still the lineage from the
  // parent
  //  cLineage * parent_lineage = cpu->GetLineage();

#ifdef DEBUG
  if (parent_lineage != NULL){
    assert( parent_lin_id == parent_lineage->GetID() );
  }
#endif

  if (parent_genotype != NULL) {
    assert( parent_genotype->GetNumCPUs() > 0 );
    parent_fitness = parent_genotype->GetTestFitness();
  }
  //cGenotype * child_genotype = cpu->GetActiveGenotype();
  double child_fitness = child_genotype->GetTestFitness();
  cLineage * child_lineage = parent_lineage;
  bool create_lineage = false;
  double lineage_stat1 = child_fitness;
  double lineage_stat2 = child_fitness;

  // if we don't have a child lineage, we are probably dealing
  // with manual assignement of the lineage label
  if ( child_lineage == NULL ){
    child_lineage = FindLineage( parent_lin_id );
    // lineage doesn't exist...
    if ( child_lineage == NULL ){
      // create it
      cout << "Creating new lineage 'by hand'!\nRequested lineage label: " << parent_lin_id;
      child_lineage = AddLineage(child_fitness, -1, parent_lin_id, 0, 0);
      cout << ", actual lineage label: " << child_lineage->GetID() << endl;

    }
  }
  // otherwise, check for conditions that cause the creation of a new lineage
  else {
    switch ( cConfig::GetLineageCreationMethod() ) {
    case 0: // manual creation only
      break;
    case 1: // new lineage whenever a parent has offspring of greater fitness
      if ( child_fitness > (parent_fitness*LANDSCAPE_NEUTRAL_MAX) ) {
	create_lineage = true;
	lineage_stat1 = parent_fitness * LANDSCAPE_NEUTRAL_MAX;
	lineage_stat2 = 0;
      }
      break;
    case 2: // new lineage whenever a new child exceeds the
      // currently highest fitness in the population
      if ( child_fitness > m_max_fitness_lineage->GetMaxFitness() ) {
	create_lineage = true;
	lineage_stat1 = m_max_fitness_lineage->GetMaxFitness();
	lineage_stat2 = 0;
      }
      break;
    case 3: // new lineage whenever a new child exceeds the
      // highest fitness, or when it is a child of the
      // of the dominant lineage and exceeds that highest fitness
      if ( child_fitness > m_max_fitness_lineage->GetMaxFitness() ||
	   ( parent_lineage == m_dominant_lineage
	     && child_fitness > m_dominant_lineage->GetMaxFitness() ) ) {
	create_lineage = true;
	lineage_stat1 = m_max_fitness_lineage->GetMaxFitness();
	lineage_stat2 = m_dominant_lineage->GetMaxFitness();
      }

      break;
    case 4: // new lineage whenever a new child exceeds the
      // fitness of the dominant creature (and the fitness of its own lineage)
      if ( child_fitness > m_genebank->GetBestGenotype()->GetTestFitness()
	   && child_fitness > parent_lineage->GetMaxFitness() ) {
	create_lineage = true;
	lineage_stat1=m_genebank->GetBestGenotype()->GetTestFitness();
	lineage_stat2=parent_lineage->GetMaxFitness();
      }
      break;
    case 5: // new lineage whenever a new child exceeds the
      // fitness of the dominant lineage (and the fitness of its own lineage)
      if ( child_fitness > m_dominant_lineage->GetMaxFitness()
	   && child_fitness > parent_lineage->GetMaxFitness() ) {
	create_lineage = true;
	lineage_stat1=m_dominant_lineage->GetMaxFitness();
	lineage_stat2=parent_lineage->GetMaxFitness();
      }
      break;
    case 6: // new lineage whenever a new child exceeds the
      // fitness of its own lineage
      if ( child_fitness > parent_lineage->GetMaxFitness() ) {
	create_lineage = true;
	lineage_stat1=parent_lineage->GetMaxFitness();
	lineage_stat2 = 0;
      }
      break;
    case 7: // new lineage whenever a new child exceeds the
      // maximum fitness ever attained by its parent lineage
      if (child_fitness > parent_lineage->GetMaxFitnessEver() ) {
	create_lineage = true;
	lineage_stat1 = parent_lineage->GetMaxFitnessEver();
	lineage_stat2 = 0;
      }
      break;
    }
  }
  if ( create_lineage ){
    child_lineage = AddLineage(child_fitness, parent_lin_id, -1, lineage_stat1, lineage_stat2);
    //    cout << "Lineage " << child_lineage->GetID() << " created."
    //	 << " Genotype: " << child_genotype->GetName()() << endl;


  }

  AddCreaturePrivate( child_genotype, child_lineage );

  return child_lineage;
}


void
cLineageControl::RemoveCreature( cBaseCPU *cpu )
{
  cLineage * cur_lineage = cpu->GetLineage();

  if (cur_lineage) {
    // remove the creature
    if ( cur_lineage->RemoveCreature( cpu->GetActiveGenotype() )
    	 || cur_lineage == m_dominant_lineage
	 || cur_lineage == m_best_lineage )
      // recalc the best/dominant lineage if necessary
      UpdateLineages();
    cpu->SetLineage( 0 );
    cpu->SetLineageLabel( -1 );
  }

}


cLineage *
cLineageControl::FindLineage( int lineage_id ) const
{
  list<cLineage *>::const_iterator it = m_lineage_list.begin();

  for ( ; it != m_lineage_list.end(); it++ )
    if ( (*it)->GetID() == lineage_id )
      break;

  if ( it == m_lineage_list.end() ){
    cout << "Lineage " << lineage_id << " not found." << endl;
    return 0;
  }
  else
    return ( *it );
}


