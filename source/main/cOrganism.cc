/*
 *  cOrganism.cc
 *  Avida
 *
 *  Called "organism.cc" prior to 12/5/05.
 *  Copyright 1999-2009 Michigan State University. All rights reserved.
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

#include "cOrganism.h"

#include "cAvidaContext.h"
#include "nHardware.h"
#include "cEnvironment.h"
#include "functions.h"
#include "cGenome.h"
#include "cGenomeUtil.h"
#include "cGenotype.h"
#include "cHardwareBase.h"
#include "cHardwareCPU.h"
#include "cHardwareManager.h"
#include "cInjectGenotype.h"
#include "cInstSet.h"
#include "cOrgSinkMessage.h"
#include "cPopulationCell.h"
#include "cPopulation.h"
#include "cStateGrid.h"
#include "cStringUtil.h"
#include "cTaskContext.h"
#include "cTools.h"
#include "cWorld.h"
#include "cStats.h"

#include <algorithm>
#include <iomanip>
#include <utility>

using namespace std;


cOrganism::cOrganism(cWorld* world, cAvidaContext& ctx, const cMetaGenome& genome)
  : m_world(world)
  , m_genotype(NULL)
  , m_phenotype(world)
  , m_initial_genome(genome)
  , m_mut_info(world->GetEnvironment().GetMutationLib(), genome.GetSize())
  , m_interface(NULL)
  , m_lineage_label(-1)
  , m_lineage(NULL)
  , m_input_pointer(0)
  , m_input_buf(world->GetEnvironment().GetInputSize())
  , m_output_buf(world->GetEnvironment().GetOutputSize())
  , m_received_messages(RECEIVED_MESSAGES_SIZE)
  , m_cur_sg(0)
  , m_sent_value(0)
  , m_sent_active(false)
  , m_test_receive_pos(0)
  , m_pher_drop(false)
  , frac_energy_donating(m_world->GetConfig().ENERGY_SHARING_PCT.Get())
  , m_max_executed(-1)
  , m_is_running(false)
  , m_is_sleeping(false)
  , m_is_dead(false)
  , killed_event(false)
  , m_net(NULL)
  , m_msg(0)
  , m_opinion(0)
{
  m_hardware = m_world->GetHardwareManager().Create(ctx, this, m_initial_genome);
  
  initialize(ctx);
}
cOrganism::cOrganism(cWorld* world, cAvidaContext& ctx, int hw_type, int inst_set_id, const cGenome& genome)
  : m_world(world)
  , m_genotype(NULL)
  , m_phenotype(world)
  , m_initial_genome(hw_type, inst_set_id, genome)
  , m_mut_info(world->GetEnvironment().GetMutationLib(), genome.GetSize())
  , m_interface(NULL)
  , m_lineage_label(-1)
  , m_lineage(NULL)
  , m_input_pointer(0)
  , m_input_buf(world->GetEnvironment().GetInputSize())
  , m_output_buf(world->GetEnvironment().GetOutputSize())
  , m_received_messages(RECEIVED_MESSAGES_SIZE)
  , m_cur_sg(0)
  , m_sent_value(0)
  , m_sent_active(false)
  , m_test_receive_pos(0)
  , m_pher_drop(false)
  , frac_energy_donating(m_world->GetConfig().ENERGY_SHARING_PCT.Get())
  , m_max_executed(-1)
  , m_is_running(false)
  , m_is_sleeping(false)
  , m_is_dead(false)
  , killed_event(false)
  , m_net(NULL)
  , m_msg(0)
  , m_opinion(0)
{
  m_hardware = m_world->GetHardwareManager().Create(ctx, this, m_initial_genome);


  initialize(ctx);
}

cOrganism::cOrganism(cWorld* world, cAvidaContext& ctx, const cMetaGenome& genome, cInstSet* inst_set)
  : m_world(world)
  , m_genotype(NULL)
  , m_phenotype(world)
  , m_initial_genome(genome)
  , m_mut_info(world->GetEnvironment().GetMutationLib(), genome.GetSize())
  , m_interface(NULL)
  , m_lineage_label(-1)
  , m_lineage(NULL)
  , m_input_pointer(0)
  , m_input_buf(world->GetEnvironment().GetInputSize())
  , m_output_buf(world->GetEnvironment().GetOutputSize())
  , m_received_messages(RECEIVED_MESSAGES_SIZE)
  , m_cur_sg(0)
  , m_sent_value(0)
  , m_sent_active(false)
  , m_test_receive_pos(0)
  , m_pher_drop(false)
  , frac_energy_donating(m_world->GetConfig().ENERGY_SHARING_PCT.Get())
  , m_max_executed(-1)
  , m_is_running(false)
  , m_is_sleeping(false)
  , m_is_dead(false)
  , killed_event(false)
  , m_net(NULL)
  , m_msg(0)
  , m_opinion(0)
{
  m_hardware = m_world->GetHardwareManager().Create(ctx, this, m_initial_genome, inst_set);
  
  initialize(ctx);
}


void cOrganism::initialize(cAvidaContext& ctx)
{
  m_phenotype.SetInstSetSize(m_hardware->GetInstSet().GetSize());
  
  if (m_world->GetConfig().DEATH_METHOD.Get() > DEATH_METHOD_OFF) {
    m_max_executed = m_world->GetConfig().AGE_LIMIT.Get();
    if (m_world->GetConfig().AGE_DEVIATION.Get() > 0.0) {
      m_max_executed += (int) (ctx.GetRandom().GetRandNormal() * m_world->GetConfig().AGE_DEVIATION.Get());
    }
    if (m_world->GetConfig().DEATH_METHOD.Get() == DEATH_METHOD_MULTIPLE) {
      m_max_executed *= m_initial_genome.GetSize();
    }
    
    // m_max_executed must be positive or an organism will not die!
    if (m_max_executed < 1) m_max_executed = 1;
  }
  
  if (m_world->GetConfig().NET_ENABLED.Get()) m_net = new cNetSupport();
  m_id = m_world->GetStats().GetTotCreatures();  
}



cOrganism::~cOrganism()
{
  assert(m_is_running == false);
  delete m_hardware;
  delete m_interface;
  if(m_net) delete m_net;
  if(m_msg) delete m_msg;
  if(m_opinion) delete m_opinion;
}

cOrganism::cNetSupport::~cNetSupport()
{
  while (pending.GetSize()) delete pending.Pop();
  for (int i = 0; i < received.GetSize(); i++) delete received[i];
}

void cOrganism::SetOrgInterface(cAvidaContext& ctx, cOrgInterface* interface)
{
  delete m_interface;
  m_interface = interface;
  
  HardwareReset(ctx);
}

const cStateGrid& cOrganism::GetStateGrid() const { return m_world->GetEnvironment().GetStateGrid(m_cur_sg); }

double cOrganism::GetRBinsTotal()
{
	double total = 0;
	for(int i = 0; i < m_phenotype.GetCurRBinsAvail().GetSize(); i++)
	{total += m_phenotype.GetCurRBinsAvail()[i];}
	
	return total;
}

void cOrganism::SetRBins(const tArray<double>& rbins_in) 
{ 
	m_phenotype.SetCurRBinsAvail(rbins_in);
}

void cOrganism::SetRBin(const int index, const double value) 
{ 
	m_phenotype.SetCurRBinAvail(index, value);
}

void cOrganism::AddToRBin(const int index, const double value) 
{ 
	m_phenotype.AddToCurRBinAvail(index, value);
	
	if(value > 0)
	{ m_phenotype.AddToCurRBinTotal(index, value); }
}  

double cOrganism::GetTestFitness(cAvidaContext& ctx)
{
  assert(m_interface);
  return m_genotype->GetTestFitness(ctx);
}
  
int cOrganism::ReceiveValue()
{
  assert(m_interface);
  const int out_value = m_interface->ReceiveValue();
  return out_value;
}

void cOrganism::SellValue(const int data, const int label, const int sell_price)
{
	if (m_sold_items.GetSize() < 10)
	{
		assert (m_interface);
		m_interface->SellValue(data, label, sell_price, m_id);
		m_world->GetStats().AddMarketItemSold();
	}
}

int cOrganism::BuyValue(const int label, const int buy_price)
{
	assert (m_interface);
	const int receive_value = m_interface->BuyValue(label, buy_price);
	if (receive_value != 0)
	{
		// put this value in storage place for recieved values
		m_received_messages.Add(receive_value);
		// update loss of buy_price to merit
		double cur_merit = GetPhenotype().GetMerit().GetDouble();
		cur_merit -= buy_price;
		UpdateMerit(cur_merit);
		m_world->GetStats().AddMarketItemBought();
	}
	return receive_value;
}


void cOrganism::DoInput(const int value)
{
  DoInput(m_input_buf, m_output_buf, value);
}


void cOrganism::DoInput(tBuffer<int>& input_buffer, tBuffer<int>& output_buffer, const int value)
{
  input_buffer.Add(value);
  m_phenotype.TestInput(input_buffer, output_buffer);
}


void cOrganism::DoOutput(cAvidaContext& ctx, const bool on_divide)
{
  if (m_net) m_net->valid = false;
  doOutput(ctx, m_input_buf, m_output_buf, on_divide);
}


void cOrganism::DoOutput(cAvidaContext& ctx, const int value)
{
  m_output_buf.Add(value);
  NetValidate(ctx, value);
  doOutput(ctx, m_input_buf, m_output_buf, false);
}


void cOrganism::DoOutput(cAvidaContext& ctx, tBuffer<int>& input_buffer, tBuffer<int>& output_buffer, const int value)
{
  output_buffer.Add(value);
  NetValidate(ctx, value);
  doOutput(ctx, input_buffer, output_buffer, false);
}


void cOrganism::doOutput(cAvidaContext& ctx, 
                         tBuffer<int>& input_buffer, 
                         tBuffer<int>& output_buffer,
                         const bool on_divide)
{
  const int deme_id = m_interface->GetDemeID();
  const tArray<double> & global_resource_count = m_interface->GetResources();
  const tArray<double> & deme_resource_count = m_interface->GetDemeResources(deme_id);
  const tArray< tArray<int> > & cell_id_lists = m_interface->GetCellIdLists();
  
  tList<tBuffer<int> > other_input_list;
  tList<tBuffer<int> > other_output_list;
  
  // If tasks require us to consider neighbor inputs, collect them...
  if (m_world->GetEnvironment().UseNeighborInput()) {
    const int num_neighbors = m_interface->GetNumNeighbors();
    for (int i = 0; i < num_neighbors; i++) {
      m_interface->Rotate();
      cOrganism * cur_neighbor = m_interface->GetNeighbor();
      if (cur_neighbor == NULL) continue;
      
      other_input_list.Push( &(cur_neighbor->m_input_buf) );
    }
  }
  
  // If tasks require us to consider neighbor outputs, collect them...
  if (m_world->GetEnvironment().UseNeighborOutput()) {
    const int num_neighbors = m_interface->GetNumNeighbors();
    for (int i = 0; i < num_neighbors; i++) {
      m_interface->Rotate();
      cOrganism * cur_neighbor = m_interface->GetNeighbor();
      if (cur_neighbor == NULL) continue;
      
      other_output_list.Push( &(cur_neighbor->m_output_buf) );
    }
  }
  
  // Do the testing of tasks performed...
  
  tArray<double> global_res_change(global_resource_count.GetSize());
  global_res_change.SetAll(0.0);
  tArray<double> deme_res_change(deme_resource_count.GetSize());
  deme_res_change.SetAll(0.0);
  tArray<int> insts_triggered;
  
  tBuffer<int>* received_messages_point = &m_received_messages;
  if (!m_world->GetConfig().SAVE_RECEIVED.Get()) received_messages_point = NULL;
  
  cTaskContext taskctx(this, input_buffer, output_buffer, other_input_list, other_output_list,
                       m_hardware->GetExtendedMemory(), on_divide, received_messages_point);
                       
  //combine global and deme resource counts
  tArray<double> globalAndDeme_resource_count = global_resource_count + deme_resource_count;
  tArray<double> globalAndDeme_res_change = global_res_change + deme_res_change;
  
  // set any resource amount to 0 if a cell cannot access this resource
  int cell_id=GetCellID();
  if (cell_id_lists.GetSize())
  {
	  for (int i=0; i<cell_id_lists.GetSize(); i++)
	  {
		  // if cell_id_lists have been set then we have to check if this cell is in the list
		  if (cell_id_lists[i].GetSize()) {
			  int j;
			  for (j=0; j<cell_id_lists[i].GetSize(); j++)
			  {
				  if (cell_id==cell_id_lists[i][j])
					  break;
			  }
			  if (j==cell_id_lists[i].GetSize())
				  globalAndDeme_resource_count[i]=0;
		  }
	  }
  }

  bool task_completed = m_phenotype.TestOutput(ctx, taskctx, globalAndDeme_resource_count, 
                                               m_phenotype.GetCurRBinsAvail(), globalAndDeme_res_change, 
                                               insts_triggered);
  
  // Handle merit increases that take the organism above it's current population merit
  if (m_world->GetConfig().MERIT_INC_APPLY_IMMEDIATE.Get()) {
    double cur_merit = m_phenotype.CalcCurrentMerit();
    if (m_phenotype.GetMerit().GetDouble() < cur_merit) m_interface->UpdateMerit(cur_merit);
  }
 
  //disassemble global and deme resource counts
  global_res_change = globalAndDeme_res_change.Subset(0, global_res_change.GetSize());
  deme_res_change = globalAndDeme_res_change.Subset(global_res_change.GetSize(), globalAndDeme_res_change.GetSize());
    
  if(m_world->GetConfig().ENERGY_ENABLED.Get() && m_world->GetConfig().APPLY_ENERGY_METHOD.Get() == 1 && task_completed) {
    m_phenotype.RefreshEnergy();
    m_phenotype.ApplyToEnergyStore();
    double newMerit = m_phenotype.ConvertEnergyToMerit(m_phenotype.GetStoredEnergy() * m_phenotype.GetEnergyUsageRatio());
    if(newMerit != -1) {
      m_interface->UpdateMerit(newMerit);
    }
  }
  m_interface->UpdateResources(global_res_change);

  //update deme resources
  m_interface->UpdateDemeResources(deme_res_change);  

  for (int i = 0; i < insts_triggered.GetSize(); i++) {
    const int cur_inst = insts_triggered[i];
    m_hardware->ProcessBonusInst(ctx, cInstruction(cur_inst));
  }
}

void cOrganism::NetGet(cAvidaContext& ctx, int& value, int& seq)
{
  assert(m_net);
  seq = m_net->seq.GetSize();
  m_net->seq.Resize(seq + 1);
  value = ctx.GetRandom().GetUInt(1 << 16);
  m_net->seq[seq].SetValue(value);
}

void cOrganism::NetSend(cAvidaContext& ctx, int value)
{
  assert(m_net);
  int index = -1;
  
  // Search for previously sent value
  for (int i = m_net->sent.GetSize() - 1; i >= 0; i--) {
    if (m_net->sent[i].GetValue() == value) {
      index = i;
      m_net->sent[i].SetSent();
      break;
    }
  }
  
  // If not found, add new message
  if (index == -1) {
    index = m_net->sent.GetSize();
    m_net->sent.Resize(index + 1);
    m_net->sent[index] = cOrgSourceMessage(value);
  }
  
  // Test if this message will be dropped
  const double drop_prob = m_world->GetConfig().NET_DROP_PROB.Get();
  if (drop_prob > 0.0 && ctx.GetRandom().P(drop_prob)) {
    m_net->sent[index].SetDropped();
    return;
  }
  
  // Test if this message will be corrupted
  int actual_value = value;
  const double mut_prob = m_world->GetConfig().NET_MUT_PROB.Get();
  if (mut_prob > 0.0 && ctx.GetRandom().P(mut_prob)) {
    switch (m_world->GetConfig().NET_MUT_TYPE.Get())
    {
      case 0: // Flip a single random bit
        actual_value ^= 1 << ctx.GetRandom().GetUInt(31);
        m_net->sent[index].SetCorrupted();
        break;
      case 1: // Flip the last bit
        actual_value ^= 1;
        m_net->sent[index].SetCorrupted();
        break;
      default:
        // invalid selection, no action
        break;
    }
  }
  
  assert(m_interface);
  cOrgSinkMessage* msg = new cOrgSinkMessage(m_interface->GetCellID(), value, actual_value);
  m_net->pending.Push(msg);
}

bool cOrganism::NetReceive(int& value)
{
  assert(m_net && m_interface);
  cOrgSinkMessage* msg = m_interface->NetReceive();
  if (msg == NULL) {
    value = 0;
    return false;
  }
  
  m_net->received.Push(msg);
  value = msg->GetActualValue();
  return true;
}

void cOrganism::NetValidate(cAvidaContext& ctx, int value)
{
  if (!m_net) return;

  m_net->valid = false;
  
  if (0xFFFF0000 & value) return;
  
  for (int i = 0; i < m_net->received.GetSize(); i++) {
    cOrgSinkMessage* msg = m_net->received[i];
    if (!msg->GetValidated() && (msg->GetOriginalValue() & 0xFFFF) == value) {
      msg->SetValidated();
      assert(m_interface);
      m_net->valid = m_interface->NetRemoteValidate(ctx, msg);
      break;
    }
  }
}

bool cOrganism::NetRemoteValidate(cAvidaContext& ctx, int value)
{
  assert(m_net);

  bool found = false;
  for (int i = m_net->last_seq; i < m_net->seq.GetSize(); i++) {
    cOrgSeqMessage& msg = m_net->seq[i];
    if (msg.GetValue() == value && !msg.GetReceived()) {
      m_net->seq[i].SetReceived();
      found = true;
      break;
    }
  }
  if (!found) return false;

  m_net->valid = false;
  int& completed = m_net->completed;
  completed = 0;
  while (m_net->last_seq < m_net->seq.GetSize() && m_net->seq[m_net->last_seq].GetReceived()) {
    completed++;
    m_net->last_seq++;
  }
  
  if (completed) {
    assert(m_interface);
    const tArray<double>& resource_count = m_interface->GetResources();
    
    tList<tBuffer<int> > other_input_list;
    tList<tBuffer<int> > other_output_list;
    
    // If tasks require us to consider neighbor inputs, collect them...
    if (m_world->GetEnvironment().UseNeighborInput()) {
      const int num_neighbors = m_interface->GetNumNeighbors();
      for (int i = 0; i < num_neighbors; i++) {
        m_interface->Rotate();
        cOrganism * cur_neighbor = m_interface->GetNeighbor();
        if (cur_neighbor == NULL) continue;
        
        other_input_list.Push( &(cur_neighbor->m_input_buf) );
      }
    }
    
    // If tasks require us to consider neighbor outputs, collect them...
    if (m_world->GetEnvironment().UseNeighborOutput()) {
      const int num_neighbors = m_interface->GetNumNeighbors();
      for (int i = 0; i < num_neighbors; i++) {
        m_interface->Rotate();
        cOrganism * cur_neighbor = m_interface->GetNeighbor();
        if (cur_neighbor == NULL) continue;
        
        other_output_list.Push( &(cur_neighbor->m_output_buf) );
      }
    }
        
    // Do the testing of tasks performed...
    m_output_buf.Add(value);
    tArray<double> res_change(resource_count.GetSize());
    tArray<int> insts_triggered;

    cTaskContext taskctx(this, m_input_buf, m_output_buf, other_input_list, other_output_list,
                         m_hardware->GetExtendedMemory());
    m_phenotype.TestOutput(ctx, taskctx, resource_count, m_phenotype.GetCurRBinsAvail(), res_change, insts_triggered);
    m_interface->UpdateResources(res_change);
    
    for (int i = 0; i < insts_triggered.GetSize(); i++) {
      const int cur_inst = insts_triggered[i];
      m_hardware->ProcessBonusInst(ctx, cInstruction(cur_inst) );
    }
}
  
  return true;
}

void cOrganism::HardwareReset(cAvidaContext& ctx)
{
  if (m_world->GetEnvironment().GetNumStateGrids() > 0 && m_interface) {
    // Select random state grid in the environment
    m_cur_sg = m_interface->GetStateGridID(ctx);
    
    const cStateGrid& sg = GetStateGrid();
    
    tArray<int> sg_state(3 + sg.GetNumStates(), 0);
    sg_state[0] = sg.GetInitialX();
    sg_state[1] = sg.GetInitialY();
    sg_state[2] = sg.GetInitialFacing();
    
    m_hardware->SetupExtendedMemory(sg_state);
  }

  if (m_net) {
    while (m_net->pending.GetSize()) delete m_net->pending.Pop();
    for (int i = 0; i < m_net->received.GetSize(); i++) delete m_net->received[i];
    m_net->received.Resize(0);
    m_net->sent.Resize(0);
    m_net->seq.Resize(0);
  }
}


bool cOrganism::InjectParasite(const cCodeLabel& label, const cGenome& injected_code)
{
  assert(m_interface);
  return m_interface->InjectParasite(this, label, injected_code);
}

bool cOrganism::InjectHost(const cCodeLabel& label, const cGenome& injected_code)
{
  return m_hardware->InjectHost(label, injected_code);
}

void cOrganism::ClearParasites()
{
  for (int i = 0; i < m_parasites.GetSize(); i++) m_parasites[i]->RemoveParasite();
  m_parasites.Resize(0);
}


double cOrganism::CalcMeritRatio()
{
  const double age = (double) m_phenotype.GetAge();
  const double merit = m_phenotype.GetMerit().GetDouble();
  return (merit > 0.0) ? (age / merit ) : age;
}


bool cOrganism::GetTestOnDivide() const { return m_interface->TestOnDivide(); }
int cOrganism::GetFailImplicit() const { return m_world->GetConfig().FAIL_IMPLICIT.Get(); }

bool cOrganism::GetRevertFatal() const { return m_world->GetConfig().REVERT_FATAL.Get(); }
bool cOrganism::GetRevertNeg() const { return m_world->GetConfig().REVERT_DETRIMENTAL.Get(); }
bool cOrganism::GetRevertNeut() const { return m_world->GetConfig().REVERT_NEUTRAL.Get(); }
bool cOrganism::GetRevertPos() const { return m_world->GetConfig().REVERT_BENEFICIAL.Get(); }

bool cOrganism::GetSterilizeFatal() const { return m_world->GetConfig().STERILIZE_FATAL.Get(); }
bool cOrganism::GetSterilizeNeg() const { return m_world->GetConfig().STERILIZE_DETRIMENTAL.Get(); }
bool cOrganism::GetSterilizeNeut() const { return m_world->GetConfig().STERILIZE_NEUTRAL.Get(); }
bool cOrganism::GetSterilizePos() const { return m_world->GetConfig().STERILIZE_BENEFICIAL.Get(); }
double cOrganism::GetNeutralMin() const { return m_world->GetConfig().NEUTRAL_MIN.Get(); }
double cOrganism::GetNeutralMax() const { return m_world->GetConfig().NEUTRAL_MAX.Get(); }


void cOrganism::PrintStatus(ostream& fp, const cString& next_name)
{
  fp << "---------------------------" << endl;
	fp << "U:" << m_world->GetStats().GetUpdate() << endl;
  m_hardware->PrintStatus(fp);
  m_phenotype.PrintStatus(fp);
  fp << endl;

  fp << setbase(16) << setfill('0');
  
  fp << "Input (env):";
  for (int i = 0; i < m_input_buf.GetCapacity(); i++) {
    int j = i; // temp holder, because GetInputAt self adjusts the input pointer
    fp << " 0x" << setw(8) << m_interface->GetInputAt(j);
  }
  fp << endl;
  
  fp << "Input (buf):";
  for (int i = 0; i < m_hardware->GetInputBuf().GetNumStored(); i++) fp << " 0x" << setw(8) << m_hardware->GetInputBuf()[i];
  fp << endl;

  fp << "Output:     ";
  for (int i = 0; i < m_hardware->GetOutputBuf().GetNumStored(); i++) fp << " 0x" << setw(8) << m_hardware->GetOutputBuf()[i];
  fp << endl;
  
	
	std::pair<int, int> pos = m_world->GetPopulation().GetDeme(GetOrgInterface().GetDemeID()).GetCellPosition(GetCellID());
	fp << "Location: (" << pos.first << "," << pos.second << ")\n";
	
	static const bool INTERRUPT_ENABLED = m_world->GetConfig().INTERRUPT_ENABLED.Get();
	if(INTERRUPT_ENABLED) {
		if(isInterrupted()) {
			cLocalThread* currentThread = static_cast<cHardwareCPU*>(m_hardware)->GetThread(m_hardware->GetCurThread());
			fp << "Interrupted: Saved IP " << (currentThread->pushedState.heads[0]).GetPosition() << endl;
		}
		else
			fp << "NOT Interrupted " <<endl;
	}
	fp << "Queued messages: "<< static_cast<int>(NumQueuedMessages()) << endl;
	
	
  fp << setfill(' ') << setbase(10);
    
  fp << "---------------------------" << endl;
  fp << "ABOUT TO EXECUTE: " << next_name << endl;
}

void cOrganism::PrintFinalStatus(ostream& fp, int time_used, int time_allocated) const
{
  fp << "---------------------------" << endl;
  m_phenotype.PrintStatus(fp);
  fp << endl;

  if (time_used == time_allocated) {
    fp << endl << "# TIMEOUT: No offspring produced." << endl;
  } else if (m_hardware->GetMemory().GetSize() == 0) {
    fp << endl << "# ORGANISM DEATH: No offspring produced." << endl;
  } else {
    fp << endl;
    fp << "# Final Memory: " << m_hardware->GetMemory().AsString() << endl;
    fp << "# Child Memory: " << m_offspring_genome.GetGenome().AsString() << endl;
  }
}


bool cOrganism::Divide_CheckViable()
{
  // Make sure required task (if any) has been performed...
  const int required_task = m_world->GetConfig().REQUIRED_TASK.Get();
  const int immunity_task = m_world->GetConfig().IMMUNITY_TASK.Get();

  if (required_task != -1 && m_phenotype.GetCurTaskCount()[required_task] == 0) { 
    if (immunity_task==-1 || m_phenotype.GetCurTaskCount()[immunity_task] == 0) {
      Fault(FAULT_LOC_DIVIDE, FAULT_TYPE_ERROR,
            cStringUtil::Stringf("Lacks required task (%d)", required_task));
      return false; //  (divide fails)
    } 
  }
  
  if (GetPhenotype().GetCurBonus() < m_world->GetConfig().REQUIRED_BONUS.Get()) return false;
  
  const int required_reaction = m_world->GetConfig().REQUIRED_REACTION.Get();
  if (required_reaction != -1 && m_phenotype.GetCurReactionCount()[required_reaction] == 0) {
    Fault(FAULT_LOC_DIVIDE, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Lacks required reaction (%d)", required_reaction));
    return false; //  (divide fails)
  }
  
  // Make sure the parent is fertile
  if ( m_phenotype.IsFertile() == false ) {
    Fault(FAULT_LOC_DIVIDE, FAULT_TYPE_ERROR, "Infertile organism");
    return false; //  (divide fails)
  }

  return true;  // Organism has no problem with divide...
}


// This gets called after a successful divide to deal with the child. 
// Returns true if parent lives through this process.

bool cOrganism::ActivateDivide(cAvidaContext& ctx)
{
  assert(m_interface);
  // Test tasks one last time before actually dividing, pass true so 
  // know that should only test "divide" tasks here
  DoOutput(ctx, true);

  // Activate the child!  (Keep Last: may kill this organism!)
  return m_interface->Divide(ctx, this, m_offspring_genome);
}


void cOrganism::Fault(int fault_loc, int fault_type, cString fault_desc)
{
  (void) fault_loc;
  (void) fault_type;
  (void) fault_desc;

#if FATAL_ERRORS
  if (fault_type == FAULT_TYPE_ERROR) {
    m_phenotype.IsFertile() = false;
  }
#endif

#if FATAL_WARNINGS
  if (fault_type == FAULT_TYPE_WARNING) {
    m_phenotype.IsFertile() = false;
  }
#endif

#if BREAKPOINTS
  m_phenotype.SetFault(fault_desc);
#endif

  m_phenotype.IncErrors();
}

void cOrganism::NewTrial()
{
  //More should be reset here... @JEB
  GetPhenotype().NewTrial();
  m_input_pointer = 0;
  m_input_buf.Clear();
  m_output_buf.Clear();
}


bool cOrganism::SendMessage(cAvidaContext& ctx, cOrgMessage& msg)
{
  assert(m_interface);
  InitMessaging();
	cDeme* deme = m_interface->GetDeme();
	if(deme == NULL)
		return true; // in test cpu
	deme->IncMessageSent();
  // If we're able to succesfully send the message...
  if(m_interface->SendMessage(msg)) {
    // If we're remembering messages
    if (m_world->GetConfig().ORGANISMS_REMEMBER_MESSAGES.Get()) {
      // save it...
      m_msg->sent.push_back(msg);
      // and set the receiver-pointer of this message to NULL.  We don't want to
      // walk this list later thinking that the receivers are still around.
      m_msg->sent.back().SetReceiver(0);
    }
    // stat-tracking...
    m_world->GetStats().SentMessage(msg);
		m_interface->GetDeme()->MessageSuccessfullySent();
    // check to see if we've performed any tasks...
    DoOutput(ctx);
    return true;
  }
	m_interface->GetDeme()->messageSendFailed();
  return false;
}


/*! Broadcast a message to all organisms out to the given depth. */
bool cOrganism::BroadcastMessage(cAvidaContext& ctx, cOrgMessage& msg, int depth) {
  assert(m_interface);
  InitMessaging();
	
	if(m_interface->BroadcastMessage(msg, depth)) {
		// If we're remembering messages
    if (m_world->GetConfig().ORGANISMS_REMEMBER_MESSAGES.Get()) {
      // save it...
      m_msg->sent.push_back(msg);
      // and set the receiver-pointer of this message to NULL.  We don't want to
      // walk this list later thinking that the receivers are still around.
      m_msg->sent.back().SetReceiver(0);
    }		
		// stat-tracking...  NOTE: this has receiver not specified, so may be a problem for predicates
    m_world->GetStats().SentMessage(msg);
    // check to see if we've performed any tasks...NOTE: this has receiver not specified, so may be a problem for tasks that care
    DoOutput(ctx);
    return true;
  }
	
	return false;
}


void cOrganism::ReceiveMessage(cOrgMessage& msg)
{
  InitMessaging();
  msg.SetReceiver(this);
  int msg_queue_size = m_world->GetConfig().MESSAGE_QUEUE_SIZE.Get();
  // are message queues unbounded?
  if (msg_queue_size >= 0) {
    // if the message queue size is zero, the incoming message is always dropped
    if (msg_queue_size == 0) {
      return;
    }

    // how many messages in the queue?
    int num_unretrieved_msgs = m_msg->received.size()-m_msg->retrieve_index;
    if (num_unretrieved_msgs == msg_queue_size) {
      // look up message queue behavior
      int bhvr = m_world->GetConfig().MESSAGE_QUEUE_BEHAVIOR_WHEN_FULL.Get();
      if (bhvr == 0) {
        // drop incoming message
        return;
      } else if (bhvr == 1 ) {
        // drop the oldest unretrieved message
        m_msg->received.erase(m_msg->received.begin()+m_msg->retrieve_index);
      } else {
        assert(false);
        cerr << "ERROR: MESSAGE_QUEUE_BEHAVIOR_WHEN_FULL was set to " << bhvr << "," << endl;
        cerr << "legal values are:" << endl;
        cerr << "\t0: drop incoming message if message queue is full (default)" << endl;
        cerr << "\t1: drop oldest unretrieved message if message queue is full" << endl;

        // TODO: is there a more gracefull way to fail?
        exit(1);
      }
    } // end if message queue is full
    m_msg->received.push_back(msg);
  } else {
    // unbounded message queues
    m_msg->received.push_back(msg);
  }
	
	static const bool INTERRUPT_ENABLED = m_world->GetConfig().INTERRUPT_ENABLED.Get();
	if(INTERRUPT_ENABLED) {
	cLocalThread* currentThread = static_cast<cHardwareCPU*>(m_hardware)->GetThread(m_hardware->GetCurThread());
		if(currentThread->isInterrupted() == false) {
			currentThread->interruptContextSwitch(cLocalThread::MSG_INTERRUPT);
		}
	}
	// else msg gets added to msg queue and will cause interrupt after current interrupt is processed
}


const cOrgMessage* cOrganism::RetrieveMessage()
{
  InitMessaging();

  assert(m_msg->retrieve_index <= m_msg->received.size());

  // Return null if no new messages have been received
  if (m_msg->retrieve_index == m_msg->received.size())
    return 0;

  if (m_world->GetConfig().ORGANISMS_REMEMBER_MESSAGES.Get()) {
    // Return the next unretrieved message and incrememt retrieve_index
    return &m_msg->received.at(m_msg->retrieve_index++);
  } else {
    // Not remembering messages, return the front of the message queue.
    // Notice that retrieve_index will always equal 0 if
    // ORGANISMS_REMEMBER_MESSAGES is false.
    const cOrgMessage* msg = &m_msg->received.front();
    m_msg->received.pop_front();
    return msg;
  }
}

int cOrganism::PeekRetrieveMessageType() {
  InitMessaging();
	
  assert(m_msg->retrieve_index <= m_msg->received.size());
	
  // Return null if no new messages have been received
  if (m_msg->retrieve_index == m_msg->received.size())
		assert(false); // no message
	
	const cOrgMessage* msg;
  if (m_world->GetConfig().ORGANISMS_REMEMBER_MESSAGES.Get()) {
    // Return the type of next unretrieved message
    msg = &m_msg->received.at(m_msg->retrieve_index);
  } else {
    // Not remembering messages, return the type from the front of the message queue.
    msg = &m_msg->received.front();
  }
	return msg->GetMessageType();
}

void cOrganism::Move(cAvidaContext& ctx)
{
  assert(m_interface);
  DoOutput(ctx);
} //End cOrganism::Move()

bool cOrganism::BcastAlarmMSG(cAvidaContext& ctx, int jump_label, int bcast_range) {
  assert(m_interface);
  
  // If we're able to succesfully send an alarm...
  if(m_interface->BcastAlarm(jump_label, bcast_range)) {
    // check to see if we've performed any tasks...
    DoOutput(ctx);
    return true;
  }  
  return false;
}

void cOrganism::moveIPtoAlarmLabel(int jump_label) {
  // move IP to alarm_label
  m_hardware->Jump_To_Alarm_Label(jump_label);
}


/*! Called to set this organism's opinion, which remains valid until a new opinion
 is expressed.
 */
void cOrganism::SetOpinion(const Opinion& opinion)
{
  InitOpinions();
  m_opinion->opinion_list.push_back(std::make_pair(opinion, m_world->GetStats().GetUpdate()));
}


/*! Called when an organism receives a flash from a neighbor. */
void cOrganism::ReceiveFlash() {
  m_hardware->ReceiveFlash();
}


/*! Called by the "flash" instruction. */
void cOrganism::SendFlash(cAvidaContext& ctx) {
  assert(m_interface);
  
  // Check to see if we should lose the flash:
  if((m_world->GetConfig().SYNC_FLASH_LOSSRATE.Get() > 0.0) &&
     (m_world->GetRandom().P(m_world->GetConfig().SYNC_FLASH_LOSSRATE.Get()))) {
    return;
  }
  
  // Flash not lost; continue.
  m_interface->SendFlash();
  m_world->GetStats().SentFlash(*this);
  DoOutput(ctx);
}

////////////////////
// Interrupt Model//
////////////////////

// is the organism currently interrupted?
bool cOrganism::isInterrupted() {
	// retruns true if current thread is interrupted
	cLocalThread* currentThread = static_cast<cHardwareCPU*>(m_hardware)->GetThread(m_hardware->GetCurThread());
	return currentThread->isInterrupted();
}

// is the organism currently interrupted?
int cOrganism::getInterruptedMessageType() {
	cLocalThread* currentThread = static_cast<cHardwareCPU*>(m_hardware)->GetThread(m_hardware->GetCurThread());
	return currentThread->getInterruptMessageType();
}



cOrganism::Neighborhood cOrganism::GetNeighborhood() {
	Neighborhood neighbors;
	for(int i=0; i<GetNeighborhoodSize(); ++i, Rotate(1)) {
		if(IsNeighborCellOccupied()) {
			neighbors.insert(GetNeighbor()->GetID());
		}
	}	
	return neighbors;
}


void cOrganism::LoadNeighborhood() {
	InitNeighborhood();
	m_neighborhood->neighbors = GetNeighborhood();
	m_neighborhood->loaded = true;
}


bool cOrganism::HasNeighborhoodChanged() {
	InitNeighborhood();
	// Must have loaded the neighborhood first:
	if(!m_neighborhood->loaded) return false;
	
	// Ok, get the symmetric difference between the old neighborhood and the current neighborhood:
	Neighborhood symdiff;
	Neighborhood current = GetNeighborhood();	
	std::set_symmetric_difference(m_neighborhood->neighbors.begin(),
																m_neighborhood->neighbors.end(),
																current.begin(),
																current.end(),
																std::insert_iterator<Neighborhood>(symdiff, symdiff.begin()));
	
	// If the symmetric difference is empty, then nothing has changed -- return 
	return !symdiff.empty();
}
