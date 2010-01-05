/*
 *  cHardwareExperimental.cc
 *  Avida
 *
 *  Created by David on 2/10/07 based on cHardwareCPU.cc
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
 *  Copyright 1999-2003 California Institute of Technology.
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


#include "cHardwareExperimental.h"

#include "cAvidaContext.h"
#include "cCPUTestInfo.h"
#include "functions.h"
#include "cGenomeUtil.h"
#include "cGenotype.h"
#include "cHardwareManager.h"
#include "cHardwareTracer.h"
#include "cInstSet.h"
#include "cMutation.h"
#include "cMutationLib.h"
#include "nMutation.h"
#include "cOrganism.h"
#include "cPhenotype.h"
#include "cStringUtil.h"
#include "cTestCPU.h"
#include "cWorldDriver.h"
#include "cWorld.h"
#include "tInstLibEntry.h"

#include <climits>
#include <fstream>

using namespace std;


tInstLib<cHardwareExperimental::tMethod>* cHardwareExperimental::s_inst_slib = cHardwareExperimental::initInstLib();

tInstLib<cHardwareExperimental::tMethod>* cHardwareExperimental::initInstLib(void)
{
  struct cNOPEntry {
    cString name;
    int nop_mod;
    cNOPEntry(const cString &name, int nop_mod)
      : name(name), nop_mod(nop_mod) {}
  };
  static const cNOPEntry s_n_array[] = {
    cNOPEntry("nop-A", REG_AX),
    cNOPEntry("nop-B", REG_BX),
    cNOPEntry("nop-C", REG_CX),
    cNOPEntry("nop-D", REG_DX),
    cNOPEntry("nop-E", REG_EX),
    cNOPEntry("nop-F", REG_FX)
  };
  
  static const tInstLibEntry<tMethod> s_f_array[] = {
    /*
     Note: all entries of cNOPEntryCPU s_n_array must have corresponding
     in the same order in tInstLibEntry<tMethod> s_f_array, and these entries must
     be the first elements of s_f_array.
     */
    tInstLibEntry<tMethod>("nop-A", &cHardwareExperimental::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation; modifies other instructions"),
    tInstLibEntry<tMethod>("nop-B", &cHardwareExperimental::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation; modifies other instructions"),
    tInstLibEntry<tMethod>("nop-C", &cHardwareExperimental::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation; modifies other instructions"),
    tInstLibEntry<tMethod>("nop-D", &cHardwareExperimental::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation; modifies other instructions"),
    
    tInstLibEntry<tMethod>("NULL", &cHardwareExperimental::Inst_Nop, 0, "True no-operation instruction: does nothing"),
    tInstLibEntry<tMethod>("nop-X", &cHardwareExperimental::Inst_Nop, 0, "True no-operation instruction: does nothing"),

    tInstLibEntry<tMethod>("if-n-equ", &cHardwareExperimental::Inst_IfNEqu, nInstFlag::DEFAULT, "Execute next instruction if ?BX?!=?CX?, else skip it"),
    tInstLibEntry<tMethod>("if-less", &cHardwareExperimental::Inst_IfLess, nInstFlag::DEFAULT, "Execute next instruction if ?BX? < ?CX?, else skip it"),
    
    tInstLibEntry<tMethod>("label", &cHardwareExperimental::Inst_Label, (nInstFlag::DEFAULT | nInstFlag::LABEL)),
    
    tInstLibEntry<tMethod>("pop", &cHardwareExperimental::Inst_Pop, nInstFlag::DEFAULT, "Remove top number from stack and place into ?BX?"),
    tInstLibEntry<tMethod>("push", &cHardwareExperimental::Inst_Push, nInstFlag::DEFAULT, "Copy number from ?BX? and place it into the stack"),
    tInstLibEntry<tMethod>("swap-stk", &cHardwareExperimental::Inst_SwitchStack, nInstFlag::DEFAULT, "Toggle which stack is currently being used"),
    tInstLibEntry<tMethod>("swap", &cHardwareExperimental::Inst_Swap, nInstFlag::DEFAULT, "Swap the contents of ?BX? with ?CX?"),
    
    tInstLibEntry<tMethod>("shift-r", &cHardwareExperimental::Inst_ShiftR, nInstFlag::DEFAULT, "Shift bits in ?BX? right by one (divide by two)"),
    tInstLibEntry<tMethod>("shift-l", &cHardwareExperimental::Inst_ShiftL, nInstFlag::DEFAULT, "Shift bits in ?BX? left by one (multiply by two)"),
    tInstLibEntry<tMethod>("inc", &cHardwareExperimental::Inst_Inc, nInstFlag::DEFAULT, "Increment ?BX? by one"),
    tInstLibEntry<tMethod>("dec", &cHardwareExperimental::Inst_Dec, nInstFlag::DEFAULT, "Decrement ?BX? by one"),

    tInstLibEntry<tMethod>("add", &cHardwareExperimental::Inst_Add, nInstFlag::DEFAULT, "Add BX to CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("sub", &cHardwareExperimental::Inst_Sub, nInstFlag::DEFAULT, "Subtract CX from BX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("mult", &cHardwareExperimental::Inst_Mult, 0, "Multiple BX by CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("div", &cHardwareExperimental::Inst_Div, 0, "Divide BX by CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("mod", &cHardwareExperimental::Inst_Mod),
    tInstLibEntry<tMethod>("nand", &cHardwareExperimental::Inst_Nand, nInstFlag::DEFAULT, "Nand BX by CX and place the result in ?BX?"),
    
    tInstLibEntry<tMethod>("IO", &cHardwareExperimental::Inst_TaskIO, nInstFlag::DEFAULT, "Output ?BX?, and input new number back into ?BX?"),
    
    // Head-based instructions
    tInstLibEntry<tMethod>("h-alloc", &cHardwareExperimental::Inst_HeadAlloc, nInstFlag::DEFAULT, "Allocate maximum allowed space"),
    tInstLibEntry<tMethod>("h-divide", &cHardwareExperimental::Inst_HeadDivide, nInstFlag::DEFAULT, "Divide code between read and write heads."),
    tInstLibEntry<tMethod>("h-read", &cHardwareExperimental::Inst_HeadRead),
    tInstLibEntry<tMethod>("h-write", &cHardwareExperimental::Inst_HeadWrite),
    tInstLibEntry<tMethod>("h-copy", &cHardwareExperimental::Inst_HeadCopy, nInstFlag::DEFAULT, "Copy from read-head to write-head; advance both"),
    tInstLibEntry<tMethod>("h-search", &cHardwareExperimental::Inst_HeadSearch, nInstFlag::DEFAULT, "Find complement template and make with flow head"),
    tInstLibEntry<tMethod>("mov-head", &cHardwareExperimental::Inst_MoveHead, nInstFlag::DEFAULT, "Move head ?IP? to the flow head"),
    tInstLibEntry<tMethod>("jmp-head", &cHardwareExperimental::Inst_JumpHead, nInstFlag::DEFAULT, "Move head ?Flow? by amount in ?CX? register"),
    tInstLibEntry<tMethod>("get-head", &cHardwareExperimental::Inst_GetHead, nInstFlag::DEFAULT, "Copy the position of the ?IP? head into ?CX?"),
    tInstLibEntry<tMethod>("if-label", &cHardwareExperimental::Inst_IfLabel, nInstFlag::DEFAULT, "Execute next if we copied complement of attached label"),
    tInstLibEntry<tMethod>("set-flow", &cHardwareExperimental::Inst_SetFlow, nInstFlag::DEFAULT, "Set flow-head to position in ?CX?"),
    tInstLibEntry<tMethod>("goto", &cHardwareExperimental::Inst_Goto, nInstFlag::DEFAULT, "Move IP to labeled position matching the label that follows")
  };
  
  const int n_size = sizeof(s_n_array)/sizeof(cNOPEntry);
  
  static cString n_names[n_size];
  static int nop_mods[n_size];
  for (int i = 0; i < n_size && i < NUM_REGISTERS; i++) {
    n_names[i] = s_n_array[i].name;
    nop_mods[i] = s_n_array[i].nop_mod;
  }
  
  const int f_size = sizeof(s_f_array)/sizeof(tInstLibEntry<tMethod>);
  static tMethod functions[f_size];
  for (int i = 0; i < f_size; i++) functions[i] = s_f_array[i].GetFunction();

	const cInstruction error(255);
	const cInstruction def(0);
  const cInstruction null_inst(4);
  
  return new tInstLib<tMethod>(f_size, s_f_array, n_names, nop_mods, functions, error, def, null_inst);
}

cHardwareExperimental::cHardwareExperimental(cWorld* world, cOrganism* in_organism, cInstSet* in_m_inst_set, bool inherited_instset)
: cHardwareBase(world, in_organism, in_m_inst_set, inherited_instset)
{
  /* FIXME:  reorganize storage of m_functions.  -- kgn */
  m_functions = s_inst_slib->GetFunctions();
  /**/
  m_memory = in_organism->GetGenome();  // Initialize memory...
  Reset();                            // Setup the rest of the hardware...
}


cHardwareExperimental::cHardwareExperimental(const cHardwareExperimental &hardware_cpu)
: cHardwareBase(hardware_cpu.m_world, hardware_cpu.organism, hardware_cpu.m_inst_set)
, m_functions(hardware_cpu.m_functions)
, m_memory(hardware_cpu.m_memory)
, m_global_stack(hardware_cpu.m_global_stack)
, m_threads(hardware_cpu.m_threads)
, m_thread_id_chart(hardware_cpu.m_thread_id_chart)
, m_cur_thread(hardware_cpu.m_cur_thread)
, m_mal_active(hardware_cpu.m_mal_active)
, m_advance_ip(hardware_cpu.m_advance_ip)
, m_executedmatchstrings(hardware_cpu.m_executedmatchstrings)
{
#if INSTRUCTION_COSTS
  inst_cost = hardware_cpu.inst_cost;
  inst_ft_cost = hardware_cpu.inst_ft_cost;
  inst_energy_cost = hardware_cpu.inst_energy_cost;
  m_has_costs = hardware_cpu.m_has_costs;
  m_has_ft_costs = hardware_cpu.m_has_ft_costs;
  m_has_energy_costs = hardware_cpu.m_has_energy_costs;
#endif
}


void cHardwareExperimental::Reset()
{
  m_global_stack.Clear();
  
  // We want to reset to have a single thread.
  m_threads.Resize(1);
  
  // Reset that single thread.
  m_threads[0].Reset(this, 0);
  m_thread_id_chart = 1; // Mark only the first thread as taken...
  m_cur_thread = 0;
  
  m_mal_active = false;
  m_executedmatchstrings = false;
  
#if INSTRUCTION_COSTS
  // instruction cost arrays
  const int num_inst_cost = m_inst_set->GetSize();
  inst_cost.Resize(num_inst_cost);
  inst_ft_cost.Resize(num_inst_cost);
  inst_energy_cost.Resize(num_inst_cost);
  m_has_costs = false;
  m_has_ft_costs = false;
  m_has_energy_costs = false;
  
  for (int i = 0; i < num_inst_cost; i++) {
    inst_cost[i] = m_inst_set->GetCost(cInstruction(i));
    if (!m_has_costs && inst_cost[i]) m_has_costs = true;
    
    inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
    if (!m_has_ft_costs && inst_ft_cost[i]) m_has_ft_costs = true;

    inst_energy_cost[i] = m_inst_set->GetEnergyCost(cInstruction(i));    
    if(!m_has_energy_costs && inst_energy_cost[i]) m_has_energy_costs = true;
  }
#endif 
}

void cHardwareExperimental::cLocalThread::operator=(const cLocalThread& in_thread)
{
  m_id = in_thread.m_id;
  for (int i = 0; i < NUM_REGISTERS; i++) reg[i] = in_thread.reg[i];
  for (int i = 0; i < NUM_HEADS; i++) heads[i] = in_thread.heads[i];
  stack = in_thread.stack;
}

void cHardwareExperimental::cLocalThread::Reset(cHardwareBase* in_hardware, int in_id)
{
  m_id = in_id;
  
  for (int i = 0; i < NUM_REGISTERS; i++) reg[i] = 0;
  for (int i = 0; i < NUM_HEADS; i++) heads[i].Reset(in_hardware);
  
  stack.Clear();
  cur_stack = 0;
  cur_head = nHardware::HEAD_IP;
  
  reading = false;
  read_label.Clear();
  next_label.Clear();
}



// This function processes the very next command in the genome, and is made
// to be as optimized as possible.  This is the heart of avida.

void cHardwareExperimental::SingleProcess(cAvidaContext& ctx)
{
  // Mark this organism as running...
  organism->SetRunning(true);
  
  cPhenotype & phenotype = organism->GetPhenotype();
  phenotype.IncCPUCyclesUsed();
  if (!m_world->GetConfig().NO_CPU_CYCLE_TIME.Get()) phenotype.IncTimeUsed();

  const int num_threads = GetNumThreads();
  
  // If we have threads turned on and we executed each thread in a single
  // timestep, adjust the number of instructions executed accordingly.
  const int num_inst_exec = (m_world->GetConfig().THREAD_SLICING_METHOD.Get() == 1) ?
num_threads : 1;
  
  for (int i = 0; i < num_inst_exec; i++) {
    // Setup the hardware for the next instruction to be executed.
    ThreadNext();
    m_advance_ip = true;
    IP().Adjust();
    
#if BREAKPOINTS
    if (IP().FlagBreakpoint()) {
      organism->DoBreakpoint();
    }
#endif
    
    // Print the status of this CPU at each step...
    if (m_tracer != NULL) m_tracer->TraceHardware(*this);
    
    // Find the instruction to be executed
    const cInstruction& cur_inst = IP().GetInst();
    
    // Test if costs have been paid and it is okay to execute this now...
    bool exec = SingleProcess_PayCosts(ctx, cur_inst);

    // Now execute the instruction...
    if (exec == true) {
      // NOTE: This call based on the cur_inst must occur prior to instruction
      //       execution, because this instruction reference may be invalid after
      //       certain classes of instructions (namely divide instructions) @DMB
      const int addl_time_cost = m_inst_set->GetAddlTimeCost(cur_inst);

      // Prob of exec (moved from SingleProcess_PayCosts so that we advance IP after a fail)
      if ( m_inst_set->GetProbFail(cur_inst) > 0.0 ) 
      {
        exec = !( ctx.GetRandom().P(m_inst_set->GetProbFail(cur_inst)) );
      }
      
      if (exec == true) SingleProcess_ExecuteInst(ctx, cur_inst);
      
      // Some instruction (such as jump) may turn m_advance_ip off.  Usually
      // we now want to move to the next instruction in the memory.
      if (m_advance_ip == true) IP().Advance();
      
      // Pay the additional death_cost of the instruction now
      phenotype.IncTimeUsed(addl_time_cost);
    } // if exec
    
  } // Previous was executed once for each thread...
  
  // Kill creatures who have reached their max num of instructions executed
  const int max_executed = organism->GetMaxExecuted();
  if ((max_executed > 0 && phenotype.GetTimeUsed() >= max_executed)
      || phenotype.GetToDie() == true) {
    organism->Die();
  }
  
  organism->SetRunning(false);
  CheckImplicitRepro(ctx);
}

// This method will handle the actuall execution of an instruction
// within single process, once that function has been finalized.
bool cHardwareExperimental::SingleProcess_ExecuteInst(cAvidaContext& ctx, const cInstruction& cur_inst) 
{
  // Copy Instruction locally to handle stochastic effects
  cInstruction actual_inst = cur_inst;
  
#ifdef EXECUTION_ERRORS
  // If there is an execution error, execute a random instruction.
  if (organism->TestExeErr()) actual_inst = m_inst_set->GetRandomInst(ctx);
#endif /* EXECUTION_ERRORS */
  
  // Get a pointer to the corrisponding method...
  int inst_idx = m_inst_set->GetLibFunctionIndex(actual_inst);
  
  // Mark the instruction as executed
  IP().SetFlagExecuted();
	
  
#if INSTRUCTION_COUNT
  // instruction execution count incremeneted
  organism->GetPhenotype().IncCurInstCount(actual_inst.GetOp());
#endif
	
  // And execute it.
  const bool exec_success = (this->*(m_functions[inst_idx]))(ctx);
	
#if INSTRUCTION_COUNT
  // decremenet if the instruction was not executed successfully
  if (exec_success == false) {
    organism->GetPhenotype().DecCurInstCount(actual_inst.GetOp());
  }
#endif	
  
  return exec_success;
}


void cHardwareExperimental::ProcessBonusInst(cAvidaContext& ctx, const cInstruction& inst)
{
  // Mark this organism as running...
  bool prev_run_state = organism->IsRunning();
  organism->SetRunning(true);
  
  if (m_tracer != NULL) m_tracer->TraceHardware(*this, true);
  
  SingleProcess_ExecuteInst(ctx, inst);
  
  organism->SetRunning(prev_run_state);
}


bool cHardwareExperimental::OK()
{
  bool result = true;
  
  if (!m_memory.OK()) result = false;
  
  for (int i = 0; i < GetNumThreads(); i++) {
    if (m_threads[i].stack.OK() == false) result = false;
    if (m_threads[i].next_label.OK() == false) result = false;
  }
  
  return result;
}

void cHardwareExperimental::PrintStatus(ostream& fp)
{
  fp << organism->GetPhenotype().GetCPUCyclesUsed() << " ";
  fp << "IP:" << IP().GetPosition() << "    ";
  
  for (int i = 0; i < NUM_REGISTERS; i++) {
    fp << static_cast<char>('A' + i) << "X:" << GetRegister(i) << " ";
    fp << setbase(16) << "[0x" << GetRegister(i) << "]  " << setbase(10);
  }
  
  // Add some extra information if additional time costs are used for instructions,
  // leave this out if there are no differences to keep it cleaner
  if ( organism->GetPhenotype().GetTimeUsed() != organism->GetPhenotype().GetCPUCyclesUsed() )
  {
    fp << "  EnergyUsed:" << organism->GetPhenotype().GetTimeUsed();
  }
  fp << endl;
  
  fp << "  R-Head:" << GetHead(nHardware::HEAD_READ).GetPosition() << " "
    << "W-Head:" << GetHead(nHardware::HEAD_WRITE).GetPosition()  << " "
    << "F-Head:" << GetHead(nHardware::HEAD_FLOW).GetPosition()   << "  "
    << "RL:" << GetReadLabel().AsString() << "   "
    << endl;
    
  int number_of_stacks = GetNumStacks();
  for (int stack_id = 0; stack_id < number_of_stacks; stack_id++) {
    fp << ((m_threads[m_cur_thread].cur_stack == stack_id) ? '*' : ' ') << " Stack " << stack_id << ":" << setbase(16) << setfill('0');
    for (int i = 0; i < nHardware::STACK_SIZE; i++) fp << " Ox" << setw(8) << GetStack(i, stack_id, 0);
    fp << setfill(' ') << setbase(10) << endl;
  }
  
  fp << "  Mem (" << GetMemory().GetSize() << "):"
		  << "  " << GetMemory().AsString()
		  << endl;
  fp.flush();
}





cHeadCPU cHardwareExperimental::FindLabelStart(bool mark_executed)
{
  cHeadCPU& ip = IP();
  const cCodeLabel& search_label = GetLabel();
  
  // Make sure the label is of size > 0.
  if (search_label.GetSize() == 0) return ip;

  cCPUMemory& memory = ip.GetMemory();
  int pos = 0;
  
  while (pos < memory.GetSize()) {
    if (m_inst_set->IsLabel(memory[pos])) { // starting label found
      pos++;
      
      // Check for direct matched label pattern, can be substring of 'label'ed target
      // - must match all NOPs in search_label
      // - extra NOPs in 'label'ed target are ignored
      int size_matched = 0;
      while (size_matched < search_label.GetSize() && pos < memory.GetSize()) {
        if (!m_inst_set->IsNop(memory[pos]) || search_label[size_matched] != m_inst_set->GetNopMod(memory[pos])) break;
        size_matched++;
        pos++;
      }
      
      // Check that the label matches and has examined the full sequence of nops following the 'label' instruction
      if (size_matched == search_label.GetSize()) {
        // Return Head pointed at last NOP of label sequence
        if (mark_executed) {
          size_matched++; // Increment size matched so that it includes the label instruction
          const int start = pos - size_matched;
          const int max = m_world->GetConfig().MAX_LABEL_EXE_SIZE.Get() + 1; // Max label + 1 for the label instruction itself
          for (int i = 0; i < size_matched && i < max; i++) memory.SetFlagExecuted(start + i);
        }
        return cHeadCPU(this, pos - 1, ip.GetMemSpace());
      }
      
      continue; 
    }
    pos++;
  }
         
  // Return start point if not found
  return ip;
}


cHeadCPU cHardwareExperimental::FindLabelForward(bool mark_executed)
{
  cHeadCPU& ip = IP();
  const cCodeLabel& search_label = GetLabel();
  
  // Make sure the label is of size > 0.
  if (search_label.GetSize() == 0) return ip;
  
  cHeadCPU pos(ip);
  pos++;
  
  while (pos.GetPosition() != ip.GetPosition()) {
    if (m_inst_set->IsLabel(pos.GetInst())) { // starting label found
      const int label_start = pos.GetPosition();
      pos++;
      
      // Check for direct matched label pattern, can be substring of 'label'ed target
      // - must match all NOPs in search_label
      // - extra NOPs in 'label'ed target are ignored
      int size_matched = 0;
      while (size_matched < search_label.GetSize() && pos.GetPosition() != ip.GetPosition()) {
        if (!m_inst_set->IsNop(pos.GetInst()) || search_label[size_matched] != m_inst_set->GetNopMod(pos.GetInst())) break;
        size_matched++;
        pos++;
      }
      
      // Check that the label matches and has examined the full sequence of nops following the 'label' instruction
      if (size_matched == search_label.GetSize()) {
        pos--;
        const int found_pos = pos.GetPosition();

        if (mark_executed) {
          pos.Set(label_start);
          const int max = m_world->GetConfig().MAX_LABEL_EXE_SIZE.Get() + 1; // Max label + 1 for the label instruction itself
          for (int i = 0; i < max; i++) pos.SetFlagExecuted();          
        }

        // Return Head pointed at last NOP of label sequence
        return cHeadCPU(this, found_pos, ip.GetMemSpace());
      }
      
      continue; 
    }
    pos++;
  }
  
  // Return start point if not found
  return ip;
}


bool cHardwareExperimental::InjectHost(const cCodeLabel & in_label, const cGenome & injection)
{
  // Make sure the genome will be below max size after injection.
  
  const int new_size = injection.GetSize() + GetMemory().GetSize();
  if (new_size > MAX_CREATURE_SIZE) return false; // (inject fails)
  
  const int inject_line = FindLabelFull(in_label).GetPosition();
  
  // Abort if no compliment is found.
  if (inject_line == -1) return false; // (inject fails)
  
  // Inject the code!
  InjectCode(injection, inject_line+1);
  
  return true; // (inject succeeds!)
}

void cHardwareExperimental::InjectCode(const cGenome & inject_code, const int line_num)
{
  assert(line_num >= 0);
  assert(line_num <= m_memory.GetSize());
  assert(m_memory.GetSize() + inject_code.GetSize() < MAX_CREATURE_SIZE);
  
  // Inject the new code.
  const int inject_size = inject_code.GetSize();
  m_memory.Insert(line_num, inject_code);
  
  // Set instruction flags on the injected code
  for (int i = line_num; i < line_num + inject_size; i++) {
    m_memory.SetFlagInjected(i);
  }
  organism->GetPhenotype().IsModified() = true;
  
  // Adjust all of the heads to take into account the new mem size.  
  for (int i = 0; i < NUM_HEADS; i++) {    
    if (GetHead(i).GetPosition() > line_num) GetHead(i).Jump(inject_size);
  }
}


void cHardwareExperimental::ReadInst(const int in_inst)
{
  if (m_inst_set->IsLabel(cInstruction(in_inst))) {
    GetReadLabel().Clear();
    ReadingLabel() = true;
  } else if (ReadingLabel() && m_inst_set->IsNop(cInstruction(in_inst))) {
    GetReadLabel().AddNop(in_inst);
  } else {
    GetReadLabel().Clear();
    ReadingLabel() = false;
  }
}


void cHardwareExperimental::AdjustHeads()
{
  for (int i = 0; i < GetNumThreads(); i++) {
    for (int j = 0; j < NUM_HEADS; j++) {
      m_threads[i].heads[j].Adjust();
    }
  }
}



// This function looks at the current position in the info of a creature,
// and sets the next_label to be the sequence of nops which follows.  The
// instruction pointer is left on the last line of the label found.

void cHardwareExperimental::ReadLabel(int max_size)
{
  int count = 0;
  cHeadCPU * inst_ptr = &( IP() );
  
  GetLabel().Clear();
  
  while (m_inst_set->IsNop(inst_ptr->GetNextInst()) &&
         (count < max_size)) {
    count++;
    inst_ptr->Advance();
    GetLabel().AddNop(m_inst_set->GetNopMod(inst_ptr->GetInst()));
    
    // If this is the first line of the template, mark it executed.
    if (GetLabel().GetSize() <=	m_world->GetConfig().MAX_LABEL_EXE_SIZE.Get()) {
      inst_ptr->SetFlagExecuted();
    }
  }
}


bool cHardwareExperimental::ForkThread()
{
  const int num_threads = GetNumThreads();
  if (num_threads == m_world->GetConfig().MAX_CPU_THREADS.Get()) return false;
  
  // Make room for the new thread.
  m_threads.Resize(num_threads + 1);
  
  // Initialize the new thread to the same values as the current one.
  m_threads[num_threads] = m_threads[m_cur_thread];
  
  // Find the first free bit in m_thread_id_chart to determine the new
  // thread id.
  int new_id = 0;
  while ( (m_thread_id_chart >> new_id) & 1 == 1) new_id++;
  m_threads[num_threads].SetID(new_id);
  m_thread_id_chart |= (1 << new_id);
  
  return true;
}


bool cHardwareExperimental::KillThread()
{
  // Make sure that there is always at least one thread...
  if (GetNumThreads() == 1) return false;
  
  // Note the current thread and set the current back one.
  const int kill_thread = m_cur_thread;
  ThreadPrev();
  
  // Turn off this bit in the m_thread_id_chart...
  m_thread_id_chart ^= 1 << m_threads[kill_thread].GetID();
  
  // Copy the last thread into the kill position
  const int last_thread = GetNumThreads() - 1;
  if (last_thread != kill_thread) {
    m_threads[kill_thread] = m_threads[last_thread];
  }
  
  // Kill the thread!
  m_threads.Resize(GetNumThreads() - 1);
  
  if (m_cur_thread > kill_thread) m_cur_thread--;
  
  return true;
}

////////////////////////////
//  Instruction Helpers...
////////////////////////////

inline int cHardwareExperimental::FindModifiedRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_register = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  }
  return default_register;
}

inline int cHardwareExperimental::FindModifiedNextRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_register = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  } else {
    default_register = (default_register + 1) % NUM_REGISTERS;
  }
  return default_register;
}

inline int cHardwareExperimental::FindModifiedPreviousRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_register = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  } else {
    default_register = (default_register + NUM_REGISTERS - 1) % NUM_REGISTERS;
  }
  return default_register;
}


inline int cHardwareExperimental::FindModifiedHead(int default_head)
{
  assert(default_head < NUM_HEADS); // Head ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_head = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  }
  return default_head;
}


inline int cHardwareExperimental::FindNextRegister(int base_reg)
{
  return (base_reg + 1) % NUM_REGISTERS;
}


bool cHardwareExperimental::Allocate_Necro(const int new_size)
{
  GetMemory().ResizeOld(new_size);
  return true;
}

bool cHardwareExperimental::Allocate_Random(cAvidaContext& ctx, const int old_size, const int new_size)
{
  GetMemory().Resize(new_size);
  
  for (int i = old_size; i < new_size; i++) {
    GetMemory()[i] = m_inst_set->GetRandomInst(ctx);
  }
  return true;
}

bool cHardwareExperimental::Allocate_Default(const int new_size)
{
  GetMemory().Resize(new_size);
  
  // New space already defaults to default instruction...
  
  return true;
}

bool cHardwareExperimental::Allocate_Main(cAvidaContext& ctx, const int allocated_size)
{
  // must do divide before second allocate & must allocate positive amount...
  if (m_world->GetConfig().REQUIRE_ALLOCATE.Get() && m_mal_active == true) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR, "Allocate already active");
    return false;
  }
  if (allocated_size < 1) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Allocate of %d too small", allocated_size));
    return false;
  }
  
  const int old_size = GetMemory().GetSize();
  const int new_size = old_size + allocated_size;
  
  // Make sure that the new size is in range.
  if (new_size > MAX_CREATURE_SIZE  ||  new_size < MIN_CREATURE_SIZE) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Invalid post-allocate size (%d)",
                               new_size));
    return false;
  }
  
  const int max_alloc_size = (int) (old_size * m_world->GetConfig().CHILD_SIZE_RANGE.Get());
  if (allocated_size > max_alloc_size) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Allocate too large (%d > %d)",
                               allocated_size, max_alloc_size));
    return false;
  }
  
  const int max_old_size =
    (int) (allocated_size * m_world->GetConfig().CHILD_SIZE_RANGE.Get());
  if (old_size > max_old_size) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Allocate too small (%d > %d)",
                               old_size, max_old_size));
    return false;
  }
  
  switch (m_world->GetConfig().ALLOC_METHOD.Get()) {
    case ALLOC_METHOD_NECRO:
      // Only break if this succeeds -- otherwise just do random.
      if (Allocate_Necro(new_size) == true) break;
    case ALLOC_METHOD_RANDOM:
      Allocate_Random(ctx, old_size, new_size);
      break;
    case ALLOC_METHOD_DEFAULT:
      Allocate_Default(new_size);
      break;
  }
  
  m_mal_active = true;
  
  return true;
}

int cHardwareExperimental::GetCopiedSize(const int parent_size, const int child_size)
{
  int copied_size = 0;
  const cCPUMemory& memory = GetMemory();
  for (int i = parent_size; i < parent_size + child_size; i++) {
    if (memory.FlagCopied(i)) copied_size++;
  }
  return copied_size;
}  


bool cHardwareExperimental::Divide_Main(cAvidaContext& ctx, const int div_point,
                               const int extra_lines, double mut_multiplier)
{
  const int child_size = GetMemory().GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
  
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  cGenome & child_genome = organism->ChildGenome();
  child_genome = cGenomeUtil::Crop(m_memory, div_point, div_point+child_size);
  
  // Cut off everything in this memory past the divide point.
  GetMemory().Resize(div_point);
  
  // Handle Divide Mutations...
  Divide_DoMutations(ctx, mut_multiplier);
  
  // Many tests will require us to run the offspring through a test CPU;
  // this is, for example, to see if mutations need to be reverted or if
  // lineages need to be updated.
  Divide_TestFitnessMeasures(ctx);
  
#if INSTRUCTION_COSTS
  // reset first time instruction costs
  for (int i = 0; i < inst_ft_cost.GetSize(); i++) {
    inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
  }
#endif
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child
  bool parent_alive = organism->ActivateDivide(ctx);

  // Do more work if the parent lives through the birth of the offspring
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset();
  }
  
  return true;
}



//////////////////////////
// And the instructions...
//////////////////////////

bool cHardwareExperimental::Inst_IfNEqu(cAvidaContext& ctx)     // Execute next if bx != ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindModifiedNextRegister(op1);
  if (GetRegister(op1) == GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareExperimental::Inst_IfLess(cAvidaContext& ctx)       // Execute next if ?bx? < ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindModifiedNextRegister(op1);
  if (GetRegister(op1) >=  GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareExperimental::Inst_Label(cAvidaContext& ctx)
{
  ReadLabel();
  return true;
};


bool cHardwareExperimental::Inst_Pop(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = StackPop();
  return true;
}

bool cHardwareExperimental::Inst_Push(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  StackPush(GetRegister(reg_used));
  return true;
}


bool cHardwareExperimental::Inst_SwitchStack(cAvidaContext& ctx) { SwitchStack(); return true;}

bool cHardwareExperimental::Inst_Swap(cAvidaContext& ctx)
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindModifiedNextRegister(op1);
  nFunctions::Swap(GetRegister(op1), GetRegister(op2));
  return true;
}

bool cHardwareExperimental::Inst_ShiftR(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) >>= 1;
  return true;
}

bool cHardwareExperimental::Inst_ShiftL(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) <<= 1;
  return true;
}


bool cHardwareExperimental::Inst_Inc(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) += 1;
  return true;
}

bool cHardwareExperimental::Inst_Dec(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) -= 1;
  return true;
}


bool cHardwareExperimental::Inst_Add(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedRegister(dst);
  const int op2 = FindModifiedNextRegister(op1);
  GetRegister(dst) = GetRegister(op1) + GetRegister(op2);
  return true;
}

bool cHardwareExperimental::Inst_Sub(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedRegister(dst);
  const int op2 = FindModifiedNextRegister(op1);
  GetRegister(dst) = GetRegister(op1) - GetRegister(op2);
  return true;
}

bool cHardwareExperimental::Inst_Mult(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedRegister(dst);
  const int op2 = FindModifiedNextRegister(op1);
  GetRegister(dst) = GetRegister(op1) * GetRegister(op2);
  return true;
}

bool cHardwareExperimental::Inst_Div(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedRegister(dst);
  const int op2 = FindModifiedNextRegister(op1);
  if (GetRegister(op2) != 0) {
    if (0-INT_MAX > GetRegister(op1) && GetRegister(op2) == -1)
      organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "div: Float exception");
    else
      GetRegister(dst) = GetRegister(op1) / GetRegister(op2);
  } else {
    organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "div: dividing by 0");
    return false;
  }
  return true;
}

bool cHardwareExperimental::Inst_Mod(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedRegister(dst);
  const int op2 = FindModifiedNextRegister(op1);
  if (GetRegister(op2) != 0) {
    GetRegister(dst) = GetRegister(op1) % GetRegister(op2);
  } else {
    organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "mod: modding by 0");
    return false;
  }
  return true;
}


bool cHardwareExperimental::Inst_Nand(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedRegister(dst);
  const int op2 = FindModifiedNextRegister(op1);
  GetRegister(dst) = ~(GetRegister(op1) & GetRegister(op2));
  return true;
}



bool cHardwareExperimental::Inst_HeadAlloc(cAvidaContext& ctx)   // Allocate maximal more
{
  const int dst = FindModifiedRegister(REG_AX);
  const int cur_size = GetMemory().GetSize();
  const int alloc_size = Min((int) (m_world->GetConfig().CHILD_SIZE_RANGE.Get() * cur_size),
                             MAX_CREATURE_SIZE - cur_size);
  if (Allocate_Main(ctx, alloc_size)) {
    GetRegister(dst) = cur_size;
    return true;
  } else return false;
}


bool cHardwareExperimental::Inst_TaskIO(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  // Do the "put" component
  const int value_out = GetRegister(reg_used);
  organism->DoOutput(ctx, value_out);  // Check for tasks completed.
  
  // Do the "get" component
  const int value_in = organism->GetNextInput();
  GetRegister(reg_used) = value_in;
  organism->DoInput(value_in);
  return true;
}


bool cHardwareExperimental::Inst_MoveHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  const int target = FindModifiedHead(nHardware::HEAD_FLOW);
  GetHead(head_used).Set(GetHead(target));
  if (head_used == nHardware::HEAD_IP) m_advance_ip = false;
  return true;
}

bool cHardwareExperimental::Inst_JumpHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  const int reg = FindModifiedRegister(REG_CX);
  GetHead(head_used).Jump(GetRegister(reg));
  if (head_used == nHardware::HEAD_IP) m_advance_ip = false;
  return true;
}

bool cHardwareExperimental::Inst_GetHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  const int reg = FindModifiedRegister(REG_CX);
  GetRegister(reg) = GetHead(head_used).GetPosition();
  return true;
}

bool cHardwareExperimental::Inst_IfLabel(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  if (GetLabel() != GetReadLabel())  IP().Advance();
  return true;
}

bool cHardwareExperimental::Inst_HeadDivide(cAvidaContext& ctx)
{
  AdjustHeads();
  const int divide_pos = GetHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  GetHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = GetMemory().GetSize();
  const int extra_lines = GetMemory().GetSize() - child_end;
  bool ret_val = Divide_Main(ctx, divide_pos, extra_lines, 1.0);
  // Re-adjust heads.
  AdjustHeads();
  return ret_val; 
}


bool cHardwareExperimental::Inst_HeadRead(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  
  const int head_id = FindModifiedHead(nHardware::HEAD_READ);
  GetHead(head_id).Adjust();
//  sCPUStats & cpu_stats = organism->CPUStats();
  
  // Mutations only occur on the read, for the moment.
  int read_inst = 0;
  if (organism->TestCopyMut(ctx)) {
    read_inst = m_inst_set->GetRandomInst(ctx).GetOp();
//    cpu_stats.mut_stats.copy_mut_count++;  // @CAO, hope this is good!
  } else {
    read_inst = GetHead(head_id).GetInst().GetOp();
  }
  GetRegister(dst) = read_inst;
  ReadInst(read_inst);
  
//  cpu_stats.mut_stats.copies_exec++;  // @CAO, this too..
  GetHead(head_id).Advance();
  return true;
}

bool cHardwareExperimental::Inst_HeadWrite(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int head_id = FindModifiedHead(nHardware::HEAD_WRITE);
  cHeadCPU& active_head = GetHead(head_id);
  
  active_head.Adjust();
  
  int value = GetRegister(src);
  if (value < 0 || value >= m_inst_set->GetSize()) value = 0;
  
  active_head.SetInst(cInstruction(value));
  active_head.SetFlagCopied();
  
  // Advance the head after write...
  active_head++;
  return true;
}

bool cHardwareExperimental::Inst_HeadCopy(cAvidaContext& ctx)
{
  // For the moment, this cannot be nop-modified.
  cHeadCPU& read_head = GetHead(nHardware::HEAD_READ);
  cHeadCPU& write_head = GetHead(nHardware::HEAD_WRITE);
//  sCPUStats& cpu_stats = organism->CPUStats();
  
  read_head.Adjust();
  write_head.Adjust();
  
  // Do mutations.
  cInstruction read_inst = read_head.GetInst();
  ReadInst(read_inst.GetOp());
  if (organism->TestCopyMut(ctx)) {
    read_inst = m_inst_set->GetRandomInst(ctx);
//    cpu_stats.mut_stats.copy_mut_count++; 
    write_head.SetFlagMutated();
    write_head.SetFlagCopyMut();
  }
  
//  cpu_stats.mut_stats.copies_exec++;
  
  write_head.SetInst(read_inst);
  write_head.SetFlagCopied();  // Set the copied flag...
  
  read_head.Advance();
  write_head.Advance();
  return true;
}

bool cHardwareExperimental::Inst_HeadSearch(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  cHeadCPU found_pos = FindLabelStart(true);
  const int search_size = found_pos.GetPosition() - IP().GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  GetHead(nHardware::HEAD_FLOW).Set(found_pos);
  GetHead(nHardware::HEAD_FLOW).Advance();
  return true;
}

bool cHardwareExperimental::Inst_SetFlow(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  GetHead(nHardware::HEAD_FLOW).Set(GetRegister(reg_used));
  return true; 
}

bool cHardwareExperimental::Inst_Goto(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  cHeadCPU found_pos = FindLabelForward(true);
  IP().Set(found_pos);
  return true;
}
