/*
*  cPhenPlastGenotype.h
*  Avida
*
*  Created by Matthew Rupp on 7/27/07.
*/


#ifndef cPhenPlastGenotype_h
#define cPhenPlastGenotype_h

#include <set>
#include <utility>

#ifndef functions_h
#include "functions.h"
#endif
#ifndef cCPUMemory_h
#include "cCPUMemory.h"
#endif
#ifndef cGenome_h
#include "cGenome.h"
#endif
#ifndef cString_h
#include "cString.h"
#endif
#ifndef cStringList_h
#include "cStringList.h"
#endif
#ifndef cStringUtil_h
#include "cStringUtil.h"
#endif
#ifndef tArray_h
#include "tArray.h"
#endif
#ifndef cPlasticPhenotype_h
#include "cPlasticPhenotype.h"
#endif
#ifndef cPhenotype_h
#include "cPhenotype.h"
#endif
#ifndef cHardwareManager_h
#include "cHardwareManager.h"
#endif
#ifndef cWorld_h
#include "cWorld.h"
#endif
#ifndef cWorldDriver_h
#include "cWorldDriver.h"
#endif

class cAvidaContext;
class cTestCPU;
class cWorld;

/**
 * This class examines a genotype for evidence of phenotypic plasticity. 
**/
 

class cPhenPlastGenotype
{
  private:

		typedef set<cPhenotype*, cPhenotype::lt_phenotype  > UniquePhenotypes;  //Actually, these are cPlatsicPhenotypes*
		cGenome m_genome;
		int m_num_trials;  
		UniquePhenotypes m_unique;
		cWorld* m_world;
		
		double m_min_fitness;
		double m_max_fitness;
		double m_avg_fitness;
		double m_likely_fitness;
			
		double m_min_merit;
		double m_max_merit;
		double m_avg_merit;
		double m_likely_merit;
		
		double m_min_gest_time;
		double m_max_gest_time;
		double m_avg_gest_time;
		double m_likely_gest_time;
		
		double m_phenotypic_entropy;
		double m_max_freq;
		double m_max_fit_freq;
		double m_min_fit_freq;
		
		void Process(cCPUTestInfo& test_info, cWorld* world, cAvidaContext& ctx);

  public:
      cPhenPlastGenotype(const cGenome& in_genome, int num_trials, cWorld* world, cAvidaContext& ctx);
    cPhenPlastGenotype(const cGenome& in_genome, int num_trails, cCPUTestInfo& test_info, cWorld* world, cAvidaContext& ctx);
    ~cPhenPlastGenotype();
    
    // Accessors
    int    GetNumPhenotypes() const     { return m_unique.size();  }
    int    GetNumTrials() const         { return m_num_trials;     }
  
    //   Fitness
    double GetMaximumFitness() const    { return m_max_fitness;    }
    double GetMinimumFitness() const    { return m_min_fitness;    }
    double GetAverageFitness() const    { return m_avg_fitness;    }
    double GetLikelyFitness()  const    { return m_likely_fitness; }
    
    //   Merit
    double GetMaximumMerit() const    { return m_max_merit;    }
    double GetMinimumMerit() const    { return m_min_merit;    }
    double GetAverageMerit() const    { return m_avg_merit;    }
    double GetLikelyMerit()  const    { return m_likely_merit; }
  
    //   Gestation Time
    double GetMaximumGestTime() const    { return m_max_gest_time;    }
    double GetMinimumGestTime() const    { return m_min_gest_time;    }
    double GetAverageGestTime() const    { return m_avg_gest_time;    }
    double GetLikelyGestTime()  const    { return m_likely_gest_time; }
  
    //  Frequency
    double GetPhenotypicEntropy() const { return m_phenotypic_entropy; }
    double GetMaximumFrequency() const  { return m_max_freq; }
    double GetMaximumFitnessFrequency() const {return m_max_fit_freq;}
    double GetMinimumFitnessFrequency() const {return m_min_fit_freq;}
    const cPlasticPhenotype* GetPlasticPhenotype(int num) const;
    const cPlasticPhenotype* GetMostLikelyPhenotype() const;
    const cPlasticPhenotype* GetHighestFitnessPhenotype() const;
    
    
};

#endif

