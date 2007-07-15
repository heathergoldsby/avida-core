/*
 *  cOrganism.h
 *  Avida
 *
 *  Called "organism.hh" prior to 12/5/05.
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
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

#ifndef cOrganism_h
#define cOrganism_h

#include <iostream>

#ifndef cCPUMemory_h
#include "cCPUMemory.h"
#endif
#ifndef sCPUStats_h
#include "sCPUStats.h"
#endif
#ifndef cGenome_h
#include "cGenome.h"
#endif
#ifndef cLocalMutations_h
#include "cLocalMutations.h"
#endif
#ifndef cMutationRates_h
#include "cMutationRates.h"
#endif
#ifndef cPhenotype_h
#include "cPhenotype.h"
#endif
#ifndef cOrgInterface_h
#include "cOrgInterface.h"
#endif
#ifndef cOrgSeqMessage_h
#include "cOrgSeqMessage.h"
#endif
#ifndef cOrgSourceMessage_h
#include "cOrgSourceMessage.h"
#endif
#ifndef tArray_h
#include "tArray.h"
#endif
#ifndef tBuffer_h
#include "tBuffer.h"
#endif
#ifndef tList_h
#include "tList.h"
#endif
#ifndef tSmartArray_h
#include "tSmartArray.h"
#endif
#ifndef _C_UMLMODEL_H_
#include "cUMLModel.h"
#endif
#ifndef _C_UMLSTATEDIAGRAM_H_
#include "cUMLStateDiagram.h"
#endif


class cAvidaContext;
class cCodeLabel;
class cEnvironment;
class cGenotype;
class cHardwareBase;
class cInjectGenotype;
class cLineage;
class cOrgSinkMessage;
class cSaleItem;


class cOrganism
{
protected:
  cWorld* m_world;
  cHardwareBase* m_hardware;              // The actual machinery running this organism.
  cGenotype* m_genotype;                  // Information about organisms with this genome.
  cPhenotype m_phenotype;                 // Descriptive attributes of organism.
  const cGenome m_initial_genome;         // Initial genome; can never be changed!
  tArray<cInjectGenotype*> m_parasites;   // List of all parasites associated with this organism.
  cMutationRates m_mut_rates;             // Rate of all possible mutations.
  cLocalMutations m_mut_info;             // Info about possible mutations;
  cOrgInterface* m_interface;             // Interface back to the population.
  int m_id;                               // unique id for each org, is just the number it was born
  int m_lineage_label;                    // a lineages tag; inherited unchanged in offspring
  cLineage* m_lineage;                    // A lineage descriptor... (different from label)
	int cclade_id;				                  // @MRR Coalescence clade information (set in cPopulation)
 
	// Other stats
  cCPUMemory m_child_genome; // Child genome, while under construction.
  sCPUStats m_cpu_stats;     // Info for statistics

  // Input and Output with the environment
  int m_input_pointer;
  tBuffer<int> m_input_buf;
  tBuffer<int> m_output_buf;
  tBuffer<int> m_received_messages;
  tList<tListNode<cSaleItem> > m_sold_items;

  // Communication
  int m_sent_value;         // What number is this org sending?
  bool m_sent_active;       // Has this org sent a number?
  int m_test_receive_pos;   // In a test CPU, what message to receive next?

  int m_max_executed;      // Max number of instruction executed before death.  
  bool m_is_running;       // Does this organism have the CPU?
  bool m_is_sleeping;      // Is this organisms sleeping?
    
  class cNetSupport
  {
  public:
    tList<cOrgSinkMessage> pending;
    tSmartArray<cOrgSinkMessage*> received;
    tSmartArray<cOrgSourceMessage> sent;
    tSmartArray<cOrgSeqMessage> seq; 
    int last_seq;
    
    cNetSupport() : last_seq(0) { ; }
    ~cNetSupport();
  };
  cNetSupport* m_net;

  // UML
  int m_state_diag;			// Index of the state diagram that the organism is currently
							// manipulating
  int m_orig_state_index;
  int m_dest_state_index;
/*  int m_trans_label_index;
  int m_trigger_index;
  int m_guard_index;
  int m_action_index;*/	
  cUMLModel m_model;		
  std::string m_parent_xmi; 
  std::map<std::string, float> m_parent_bonus;  

  cOrganism(); // @not_implemented
  cOrganism(const cOrganism&); // @not_implemented
  cOrganism& operator=(const cOrganism&); // @not_implemented
  
public:
  cOrganism(cWorld* world, cAvidaContext& ctx, const cGenome& in_genome);
  ~cOrganism();

  // --------  Accessor Methods  --------
  void SetGenotype(cGenotype* in_genotype) { m_genotype = in_genotype; }
  cGenotype* GetGenotype() const { return m_genotype; }
  const cPhenotype& GetPhenotype() const { return m_phenotype; }
  cPhenotype& GetPhenotype() { return m_phenotype; }

  const cGenome& GetGenome() const { return m_initial_genome; }
  
  const cMutationRates& MutationRates() const { return m_mut_rates; }
  cMutationRates& MutationRates() { return m_mut_rates; }
  const cLocalMutations& GetLocalMutations() const { return m_mut_info; }
  cLocalMutations& GetLocalMutations() { return m_mut_info; }
  
  const cOrgInterface& GetOrgInterface() const { assert(m_interface); return *m_interface; }
  cOrgInterface& GetOrgInterface() { assert(m_interface); return *m_interface; }
  void SetOrgInterface(cOrgInterface* interface);
  
  void SetLineageLabel(int in_label) { m_lineage_label = in_label; }
  int GetLineageLabel() const { return m_lineage_label; }  
  void SetLineage(cLineage* in_lineage) { m_lineage = in_lineage; }
  cLineage* GetLineage() const { return m_lineage; }
  
	void SetCCladeLabel( int in_label ) { cclade_id = in_label; };  //@MRR
	int  GetCCladeLabel() const { return cclade_id; }
	
	
  int GetMaxExecuted() const { return m_max_executed; }
  
  cCPUMemory& ChildGenome() { return m_child_genome; }
  sCPUStats& CPUStats() { return m_cpu_stats; }

  void SetRunning(bool in_running) { m_is_running = in_running; }
  bool IsRunning() { return m_is_running; }

  void SetSleeping(bool in_sleeping) { m_is_sleeping = in_sleeping; }
  bool IsSleeping() { return m_is_sleeping; }
  
  
  // --------  cOrgInterface Methods  --------
  cHardwareBase& GetHardware() { return *m_hardware; }
  cOrganism* GetNeighbor() { return m_interface->GetNeighbor(); }
  int GetNeighborhoodSize() { return m_interface->GetNumNeighbors(); }
  int GetFacing() { assert(m_interface); return m_interface->GetFacing(); }  // Returns the facing of this organism.
  void Rotate(int direction) { m_interface->Rotate(direction); }
  void DoBreakpoint() { m_interface->Breakpoint(); }
  int GetNextInput() { return m_interface->GetInputAt(m_input_pointer); }
  int GetNextInput(int& in_input_pointer) { return m_interface->GetInputAt(in_input_pointer); } //@JEB alternate for GX
  tBuffer<int>& GetInputBuf() { return m_input_buf; }
  tBuffer<int>& GetOutputBuf() { return m_output_buf; }
  void Die() { m_interface->Die(); }
  void Kaboom(int dist) { m_interface->Kaboom(dist);}
  void SpawnDeme() { m_interface->SpawnDeme(); }
  int GetCellID() { return m_interface->GetCellID(); }
  int GetDebugInfo() { return m_interface->Debug(); }
  int GetID() { return m_id; }
  bool GetSentActive() { return m_sent_active; }
  void SendValue(int value) { m_sent_active = true; m_sent_value = value; }
  int RetrieveSentValue() { m_sent_active = false; return m_sent_value; }
  int ReceiveValue();
  void SellValue(const int data, const int label, const int sell_price);
  int BuyValue(const int label, const int buy_price);
  tListNode<tListNode<cSaleItem> >* AddSoldItem(tListNode<cSaleItem>* node) { return m_sold_items.PushRear(node); }
  tList<tListNode<cSaleItem> >* GetSoldItems() { return &m_sold_items; }
  void UpdateMerit(double new_merit) { m_interface->UpdateMerit(new_merit); }
  

  // --------  Input and Output Methods  --------
  void DoInput(const int value);
  void DoInput(tBuffer<int>& input_buffer, tBuffer<int>& output_buffer, const int value);
  void DoOutput(cAvidaContext& ctx, const int value, const bool on_divide = false);
  void DoOutput(cAvidaContext& ctx, tBuffer<int>& input_buffer, 
                tBuffer<int>& output_buffer, const int value, const bool on_divide=false);
  void ClearInput() { m_input_buf.Clear(); }
  void AddOutput(int val) { m_output_buf.Add(val); }

  // --------  Divide Methods  --------
  bool Divide_CheckViable();
  bool ActivateDivide(cAvidaContext& ctx);
  
  
  // --------  Networking Support  --------
  void NetGet(cAvidaContext& ctx, int& value, int& seq);
  void NetSend(cAvidaContext& ctx, int value);
  cOrgSinkMessage* NetPop() { return m_net->pending.PopRear(); }
  bool NetReceive(int& value);
  bool NetValidate(cAvidaContext& ctx, int value);
  bool NetRemoteValidate(cAvidaContext& ctx, int value);
  int NetLast() { return m_net->last_seq; }
  void NetReset();

  
  // --------  Parasite Interactions  --------
  bool InjectParasite(const cCodeLabel& label, const cGenome& genome);
  bool InjectHost(const cCodeLabel& in_label, const cGenome& genome);
  void AddParasite(cInjectGenotype* cur) { m_parasites.Push(cur); }
  cInjectGenotype& GetParasite(int x) { return *m_parasites[x]; }
  int GetNumParasites() const { return m_parasites.GetSize(); }
  void ClearParasites();
		      

  // --------  Support Methods  --------
  double GetTestFitness(cAvidaContext& ctx);
  double CalcMeritRatio();
  
  void PrintStatus(std::ostream& fp, const cString& next_name);
  void PrintFinalStatus(std::ostream& fp, int time_used, int time_allocated) const;
  void Fault(int fault_loc, int fault_type, cString fault_desc="");


  // --------  Mutation Rate Convenience Methods  --------
  bool TestCopyMut(cAvidaContext& ctx) const { return m_mut_rates.TestCopyMut(ctx); }
  bool TestCopyIns(cAvidaContext& ctx) const { return m_mut_rates.TestCopyIns(ctx); }
  bool TestCopyDel(cAvidaContext& ctx) const { return m_mut_rates.TestCopyDel(ctx); }
  bool TestDivideMut(cAvidaContext& ctx) const { return m_mut_rates.TestDivideMut(ctx); }
  bool TestDivideIns(cAvidaContext& ctx) const { return m_mut_rates.TestDivideIns(ctx); }
  bool TestDivideDel(cAvidaContext& ctx) const { return m_mut_rates.TestDivideDel(ctx); }
  bool TestDivideSlip(cAvidaContext& ctx) const { return m_mut_rates.TestDivideSlip(ctx); } 
  bool TestParentMut(cAvidaContext& ctx) const { return m_mut_rates.TestParentMut(ctx); }
  
  double GetCopyMutProb() const { return m_mut_rates.GetCopyMutProb(); }
  void SetCopyMutProb(double _p) { return m_mut_rates.SetCopyMutProb(_p); }
  void SetDivMutProb(double _p) { return m_mut_rates.SetDivMutProb(_p); }

  double GetInsMutProb() const { return m_mut_rates.GetInsMutProb(); }
  double GetDelMutProb() const { return m_mut_rates.GetDelMutProb(); }
  double GetDivMutProb() const { return m_mut_rates.GetDivMutProb(); }
  double GetParentMutProb() const { return m_mut_rates.GetParentMutProb();}

  double GetInjectInsProb() const { return m_mut_rates.GetInjectInsProb(); }
  double GetInjectDelProb() const { return m_mut_rates.GetInjectDelProb(); }
  double GetInjectMutProb() const { return m_mut_rates.GetInjectMutProb(); }
  

  // --------  Configuration Convenience Methods  --------
  bool GetTestOnDivide() const;
  bool GetFailImplicit() const;

  bool GetRevertFatal() const;
  bool GetRevertNeg() const;
  bool GetRevertNeut() const;
  bool GetRevertPos() const;

  bool GetSterilizeFatal() const;
  bool GetSterilizeNeg() const;
  bool GetSterilizeNeut() const;
  bool GetSterilizePos() const;
  double GetNeutralMin() const;
  double GetNeutralMax() const;
  
 // UML
  void modelCheck(cAvidaContext& ctx);
  cUMLModel* getUMLModel();
//  cUMLModel* getParentUMLModel() { return m_parent_model; } 
  std::string getParentXMI() { return m_parent_xmi; }
  int getStateDiagramIndex() { return m_state_diag; } 
//  bool lastStateDiagram () { m_state_diag = getUMLModel()->getStateDiagramSize(); }
//  bool firstStateDiagram() { m_state_diag = 0; }  
  bool currTrans (int, int, int, int, int, int); 
  cUMLStateDiagram* getStateDiagram();
//  void printStats();
//  void setParentModel(cUMLModel* m) { m_parent_model = m; } 
  void setParentXMI(std::string s) { m_parent_xmi = s; } 
  void setParentBonus (std::map<std::string, float> v) { m_parent_bonus = v; }
  float getParentBonus (std::string s) { return m_parent_bonus[s]; }
  std::map<std::string, float> getParentBonus() { return m_parent_bonus; }
  

 
// The jump functions jump the index of the various vectors either forward (+ int) or backwards (- int)
/*
  bool absoluteJumpGuard(int);
  bool absoluteJumpAction(int);
  bool absoluteJumpTrigger(int);
  bool absoluteJumpTransitionLabel(int);
*/
  bool absoluteJumpOriginState(int);

  bool absoluteJumpDestinationState(int);
  bool absoluteJumpStateDiagram (int);
/*
  bool relativeJumpGuard(int amount);
  bool relativeJumpAction(int amount);
  bool relativeJumpTrigger(int amount);
  bool relativeJumpTransitionLabel(int amount);
*/
  bool relativeJumpOriginState(int amount);
  bool relativeJumpDestinationState(int amount);
  bool relativeJumpStateDiagram (int); 
 
  bool addTransitionTotal();
 
};


#ifdef ENABLE_UNIT_TESTS
namespace nOrganism {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif

