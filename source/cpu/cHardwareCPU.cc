/*
 *  cHardwareCPU.cc
 *  Avida
 *
 *  Called "hardware_cpu.cc" prior to 11/17/05.
 *  Copyright 1999-2009 Michigan State University. All rights reserved.
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


#include "cHardwareCPU.h"

#include "cAvidaContext.h"
#include "cCPUTestInfo.h"
#include "functions.h"
#include "cEnvironment.h"
#include "cGenomeUtil.h"
#include "cGenotype.h"
#include "cHardwareManager.h"
#include "cHardwareTracer.h"
#include "cInstSet.h"
#include "cMutation.h"
#include "cMutationLib.h"
#include "nMutation.h"
#include "cOrganism.h"
#include "cOrgMessage.h"
#include "cPhenotype.h"
#include "cPopulation.h"
#include "cPopulationCell.h"
#include "cReaction.h"
#include "cReactionLib.h"
#include "cReactionProcess.h"
#include "cResource.h"
#include "cStateGrid.h"
#include "cStringUtil.h"
#include "cTestCPU.h"
#include "cWorldDriver.h"
#include "cWorld.h"
#include "tInstLibEntry.h"

#include <climits>
#include <fstream>
#include <cmath>

using namespace std;


// Interrupt Handler code

/* interrupt handling - MSG arrives, add to waiting pool, and process interrupts until all have been processed
 
 ***an interrupt cannot be preempted***
 
 
 Processing interrupt
 Save current state
 Push interrupt arguments into registers, i.e. MSG contents are placed in BX & CX.  This step will consume a nop that follows interrupt header
 Jump 1 instruction passed msg-handler
 Process instructions until MSG_received_handler_END
 On MSG_received_handler_END, process next interrupt or restore previous state
 */

void cLocalThread::saveState() {
	assert(!interrupted);
	// save registers
	// save heads
	// save thread stack
	
	for(int i = 0; i < NUM_REGISTERS; i++) {
		pushedState.reg[i] = reg[i];
	}
	
	for(int i = 0; i < NUM_HEADS; i++) {
		pushedState.heads[i] = heads[i];
	}
	
	pushedState.stack = stack;
	pushedState.cur_stack = cur_stack;
	pushedState.cur_head = cur_head;
	pushedState.read_label = read_label;
	pushedState.next_label = next_label;
}

void cLocalThread::restoreState() {
	assert(!interrupted);
	// restore registers
	// restore heads
	// save thread stack
	
	for(int i = 0; i < NUM_REGISTERS; i++) {
		reg[i] = pushedState.reg[i];
	}
	
	for(int i = 0; i < NUM_HEADS; i++) {
		heads[i] = pushedState.heads[i];
	}
	
	stack = pushedState.stack;
	cur_stack = pushedState.cur_stack;
	cur_head = pushedState.cur_head;
	read_label = pushedState.read_label;
	next_label = pushedState.next_label;
}

// push interrupt arguments into registers, i.e. MSG contents are placed in BX & CX, nothing for movement
bool cLocalThread::initializeInterruptState(const cString& handlerHeadInstructionString) {
	//Jump all heads 1 instruction passed msg-handler
	cInstruction label_inst = hardware->GetInstSet().GetInst(handlerHeadInstructionString);
	
	cHeadCPU search_head(hardware->IP());
	int start_pos = search_head.GetPosition();
	search_head++;
	
	while (start_pos != search_head.GetPosition()) {
		if (search_head.GetInst() == label_inst) {  // found handlerHeadInstructionString
			search_head++;  // one instruction past instruction
			for (int i = 0; i < NUM_REGISTERS; i++) {
				hardware->GetRegister(i) = 0;
			}
			stack.Clear();
			cur_stack = 0;
			cur_head = nHardware::HEAD_IP;
			read_label.Clear();
			next_label.Clear();
			
			// set all heads to same spot
			for(int i = 0; i < NUM_HEADS; i++) {
				hardware->GetHead(i,m_id).Set(search_head.GetPosition());
			}
			return true;
		}
		search_head++;
	}
	return false;
}

void cLocalThread::interruptContextSwitch(int interruptType) {
	// note: movement interrupts cannot be blocked, just message interrupts
	// note: movements within an interrupt handler do not cause another interrupt (REALLY?)
	// note: interrupt handlers can be jumped into and out of
  // TODO: config arg to disallow jumping into and out of interrupt handler
	
	if(!interrupted && interruptType != cLocalThread::INTERRUPT_COMPLETE) { //normal -> interrupt
		//Save current state
		saveState();
		interrupted = true;
		
		switch (interruptType) {
			case cLocalThread::MSG_INTERRUPT:
				if(initializeInterruptState("msg-handler")) {
					hardware->IP().Retreat();
					hardware->Inst_RetrieveMessage(m_world->GetDefaultContext());
					hardware->IP().Advance();
				}
				break;
			case cLocalThread::MOVE_INTERRUPT:
				if(initializeInterruptState("moved_handler")) {
					; // perform movement interrupt initialization here
				}
				break;
			default:
				cerr <<  "Unknown intrerrupt type " << interruptType << "  Exitting.\n\n";
				exit(-1);
				break;
		}
	}
	else if(interrupted && interruptType == cLocalThread::INTERRUPT_COMPLETE) { // currently interrupted	
		if(hardware->GetOrganism()->NumQueuedMessages() > 0) { // more messages to process
			if(initializeInterruptState("msg-handler")) {
				hardware->IP().Retreat();
				hardware->Inst_RetrieveMessage(m_world->GetDefaultContext());
				hardware->IP().Advance();
			}
		} else { // interrupt -> normal
			interrupted = false;
			restoreState();
		}
	}
}

void cLocalThread::operator=(const cLocalThread& in_thread)
{
  m_id = in_thread.m_id;
  for (int i = 0; i < NUM_REGISTERS; i++) reg[i] = in_thread.reg[i];
  for (int i = 0; i < NUM_HEADS; i++) heads[i] = in_thread.heads[i];
  stack = in_thread.stack;
	// what about other stuff?
}

void cLocalThread::Reset(cWorld* world, cHardwareCPU* in_hardware, int in_id)
{
	m_world = world;
	hardware = in_hardware;
  m_id = in_id;
  
  for (int i = 0; i < NUM_REGISTERS; i++) {
		reg[i] = 0;
		pushedState.reg[i] = 0;
	}
  for (int i = 0; i < NUM_HEADS; i++) {
		heads[i].Reset(in_hardware);
		pushedState.heads[i].Reset(in_hardware);
  }
  stack.Clear();
	pushedState.stack.Clear();
  cur_stack = 0;
	pushedState.cur_stack = 0;
  cur_head = nHardware::HEAD_IP;
	pushedState.cur_head = nHardware::HEAD_IP;
  read_label.Clear();
	pushedState.read_label.Clear();
  next_label.Clear();
	pushedState.next_label.Clear();
  
	interrupted = false;
	
  // Promoter model
  m_promoter_inst_executed = 0;
}


tInstLib<cHardwareCPU::tMethod>* cHardwareCPU::s_inst_slib = cHardwareCPU::initInstLib();

tInstLib<cHardwareCPU::tMethod>* cHardwareCPU::initInstLib(void)
{
  struct cNOPEntryCPU {
    cString name;
    int nop_mod;
    cNOPEntryCPU(const cString &name, int nop_mod)
		: name(name), nop_mod(nop_mod) {}
  };
  static const cNOPEntryCPU s_n_array[] = {
    cNOPEntryCPU("nop-A", REG_AX),
    cNOPEntryCPU("nop-B", REG_BX),
    cNOPEntryCPU("nop-C", REG_CX)
  };
  
  static const tInstLibEntry<tMethod> s_f_array[] = {
    /*
     Note: all entries of cNOPEntryCPU s_n_array must have corresponding
     in the same order in tInstLibEntry<tMethod> s_f_array, and these entries must
     be the first elements of s_f_array.
     */
    tInstLibEntry<tMethod>("nop-A", &cHardwareCPU::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation instruction; modifies other instructions"),
    tInstLibEntry<tMethod>("nop-B", &cHardwareCPU::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation instruction; modifies other instructions"),
    tInstLibEntry<tMethod>("nop-C", &cHardwareCPU::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation instruction; modifies other instructions"),
    
    tInstLibEntry<tMethod>("nop-X", &cHardwareCPU::Inst_Nop, 0, "True no-operation instruction: does nothing"),
    tInstLibEntry<tMethod>("if-equ-0", &cHardwareCPU::Inst_If0, 0, "Execute next instruction if ?BX?==0, else skip it"),
    tInstLibEntry<tMethod>("if-not-0", &cHardwareCPU::Inst_IfNot0, 0, "Execute next instruction if ?BX?!=0, else skip it"),
    tInstLibEntry<tMethod>("if-n-equ", &cHardwareCPU::Inst_IfNEqu, nInstFlag::DEFAULT, "Execute next instruction if ?BX?!=?CX?, else skip it"),
    tInstLibEntry<tMethod>("if-equ", &cHardwareCPU::Inst_IfEqu, 0, "Execute next instruction if ?BX?==?CX?, else skip it"),
    tInstLibEntry<tMethod>("if-grt-0", &cHardwareCPU::Inst_IfGr0),
    tInstLibEntry<tMethod>("if-grt", &cHardwareCPU::Inst_IfGr),
    tInstLibEntry<tMethod>("if->=-0", &cHardwareCPU::Inst_IfGrEqu0),
    tInstLibEntry<tMethod>("if->=", &cHardwareCPU::Inst_IfGrEqu),
    tInstLibEntry<tMethod>("if-les-0", &cHardwareCPU::Inst_IfLess0),
    tInstLibEntry<tMethod>("if-less", &cHardwareCPU::Inst_IfLess, nInstFlag::DEFAULT, "Execute next instruction if ?BX? < ?CX?, else skip it"),
    tInstLibEntry<tMethod>("if-<=-0", &cHardwareCPU::Inst_IfLsEqu0),
    tInstLibEntry<tMethod>("if-<=", &cHardwareCPU::Inst_IfLsEqu),
    tInstLibEntry<tMethod>("if-A!=B", &cHardwareCPU::Inst_IfANotEqB),
    tInstLibEntry<tMethod>("if-B!=C", &cHardwareCPU::Inst_IfBNotEqC),
    tInstLibEntry<tMethod>("if-A!=C", &cHardwareCPU::Inst_IfANotEqC),
    tInstLibEntry<tMethod>("if-bit-1", &cHardwareCPU::Inst_IfBit1),
    
    tInstLibEntry<tMethod>("jump-f", &cHardwareCPU::Inst_JumpF),
    tInstLibEntry<tMethod>("jump-b", &cHardwareCPU::Inst_JumpB),
    tInstLibEntry<tMethod>("call", &cHardwareCPU::Inst_Call),
    tInstLibEntry<tMethod>("return", &cHardwareCPU::Inst_Return),
		
    tInstLibEntry<tMethod>("throw", &cHardwareCPU::Inst_Throw),
    tInstLibEntry<tMethod>("throwif=0", &cHardwareCPU::Inst_ThrowIf0),    
    tInstLibEntry<tMethod>("throwif!=0", &cHardwareCPU::Inst_ThrowIfNot0),
    tInstLibEntry<tMethod>("catch", &cHardwareCPU::Inst_Catch),
    
    tInstLibEntry<tMethod>("goto", &cHardwareCPU::Inst_Goto),
    tInstLibEntry<tMethod>("goto-if=0", &cHardwareCPU::Inst_GotoIf0),    
    tInstLibEntry<tMethod>("goto-if!=0", &cHardwareCPU::Inst_GotoIfNot0),
    tInstLibEntry<tMethod>("label", &cHardwareCPU::Inst_Label),
    
    tInstLibEntry<tMethod>("pop", &cHardwareCPU::Inst_Pop, nInstFlag::DEFAULT, "Remove top number from stack and place into ?BX?"),
    tInstLibEntry<tMethod>("push", &cHardwareCPU::Inst_Push, nInstFlag::DEFAULT, "Copy number from ?BX? and place it into the stack"),
    tInstLibEntry<tMethod>("swap-stk", &cHardwareCPU::Inst_SwitchStack, nInstFlag::DEFAULT, "Toggle which stack is currently being used"),
    tInstLibEntry<tMethod>("flip-stk", &cHardwareCPU::Inst_FlipStack),
    tInstLibEntry<tMethod>("swap", &cHardwareCPU::Inst_Swap, nInstFlag::DEFAULT, "Swap the contents of ?BX? with ?CX?"),
    tInstLibEntry<tMethod>("swap-AB", &cHardwareCPU::Inst_SwapAB),
    tInstLibEntry<tMethod>("swap-BC", &cHardwareCPU::Inst_SwapBC),
    tInstLibEntry<tMethod>("swap-AC", &cHardwareCPU::Inst_SwapAC),
    tInstLibEntry<tMethod>("copy-reg", &cHardwareCPU::Inst_CopyReg),
    tInstLibEntry<tMethod>("set_A=B", &cHardwareCPU::Inst_CopyRegAB),
    tInstLibEntry<tMethod>("set_A=C", &cHardwareCPU::Inst_CopyRegAC),
    tInstLibEntry<tMethod>("set_B=A", &cHardwareCPU::Inst_CopyRegBA),
    tInstLibEntry<tMethod>("set_B=C", &cHardwareCPU::Inst_CopyRegBC),
    tInstLibEntry<tMethod>("set_C=A", &cHardwareCPU::Inst_CopyRegCA),
    tInstLibEntry<tMethod>("set_C=B", &cHardwareCPU::Inst_CopyRegCB),
    tInstLibEntry<tMethod>("reset", &cHardwareCPU::Inst_Reset),
	//tInstLibEntry<tMethod>("load-val", &cHardwareCPU::Inst_LoadVal),
    
    tInstLibEntry<tMethod>("pop-A", &cHardwareCPU::Inst_PopA),
    tInstLibEntry<tMethod>("pop-B", &cHardwareCPU::Inst_PopB),
    tInstLibEntry<tMethod>("pop-C", &cHardwareCPU::Inst_PopC),
    tInstLibEntry<tMethod>("push-A", &cHardwareCPU::Inst_PushA),
    tInstLibEntry<tMethod>("push-B", &cHardwareCPU::Inst_PushB),
    tInstLibEntry<tMethod>("push-C", &cHardwareCPU::Inst_PushC),
    
    tInstLibEntry<tMethod>("shift-r", &cHardwareCPU::Inst_ShiftR, nInstFlag::DEFAULT, "Shift bits in ?BX? right by one (divide by two)"),
    tInstLibEntry<tMethod>("shift-l", &cHardwareCPU::Inst_ShiftL, nInstFlag::DEFAULT, "Shift bits in ?BX? left by one (multiply by two)"),
    tInstLibEntry<tMethod>("bit-1", &cHardwareCPU::Inst_Bit1),
    tInstLibEntry<tMethod>("set-num", &cHardwareCPU::Inst_SetNum),
    tInstLibEntry<tMethod>("val-grey", &cHardwareCPU::Inst_ValGrey),
    tInstLibEntry<tMethod>("val-dir", &cHardwareCPU::Inst_ValDir),
    tInstLibEntry<tMethod>("val-add-p", &cHardwareCPU::Inst_ValAddP),
    tInstLibEntry<tMethod>("val-fib", &cHardwareCPU::Inst_ValFib),
    tInstLibEntry<tMethod>("val-poly-c", &cHardwareCPU::Inst_ValPolyC),
    tInstLibEntry<tMethod>("inc", &cHardwareCPU::Inst_Inc, nInstFlag::DEFAULT, "Increment ?BX? by one"),
    tInstLibEntry<tMethod>("dec", &cHardwareCPU::Inst_Dec, nInstFlag::DEFAULT, "Decrement ?BX? by one"),
    tInstLibEntry<tMethod>("zero", &cHardwareCPU::Inst_Zero, 0, "Set ?BX? to zero"),
    tInstLibEntry<tMethod>("neg", &cHardwareCPU::Inst_Neg),
    tInstLibEntry<tMethod>("square", &cHardwareCPU::Inst_Square),
    tInstLibEntry<tMethod>("sqrt", &cHardwareCPU::Inst_Sqrt),
    tInstLibEntry<tMethod>("not", &cHardwareCPU::Inst_Not),
    
    tInstLibEntry<tMethod>("add", &cHardwareCPU::Inst_Add, nInstFlag::DEFAULT, "Add BX to CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("sub", &cHardwareCPU::Inst_Sub, nInstFlag::DEFAULT, "Subtract CX from BX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("mult", &cHardwareCPU::Inst_Mult, 0, "Multiple BX by CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("div", &cHardwareCPU::Inst_Div, 0, "Divide BX by CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("mod", &cHardwareCPU::Inst_Mod),
    tInstLibEntry<tMethod>("nand", &cHardwareCPU::Inst_Nand, nInstFlag::DEFAULT, "Nand BX by CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("nor", &cHardwareCPU::Inst_Nor),
    tInstLibEntry<tMethod>("and", &cHardwareCPU::Inst_And),
    tInstLibEntry<tMethod>("order", &cHardwareCPU::Inst_Order),
    tInstLibEntry<tMethod>("xor", &cHardwareCPU::Inst_Xor),
    
    tInstLibEntry<tMethod>("copy", &cHardwareCPU::Inst_Copy),
    tInstLibEntry<tMethod>("read", &cHardwareCPU::Inst_ReadInst),
    tInstLibEntry<tMethod>("write", &cHardwareCPU::Inst_WriteInst),
    tInstLibEntry<tMethod>("stk-read", &cHardwareCPU::Inst_StackReadInst),
    tInstLibEntry<tMethod>("stk-writ", &cHardwareCPU::Inst_StackWriteInst),
    
    tInstLibEntry<tMethod>("compare", &cHardwareCPU::Inst_Compare),
    tInstLibEntry<tMethod>("if-n-cpy", &cHardwareCPU::Inst_IfNCpy),
    tInstLibEntry<tMethod>("allocate", &cHardwareCPU::Inst_Allocate),
    tInstLibEntry<tMethod>("divide", &cHardwareCPU::Inst_Divide, nInstFlag::STALL),
    tInstLibEntry<tMethod>("divideRS", &cHardwareCPU::Inst_DivideRS, nInstFlag::STALL),
    tInstLibEntry<tMethod>("c-alloc", &cHardwareCPU::Inst_CAlloc),
    tInstLibEntry<tMethod>("c-divide", &cHardwareCPU::Inst_CDivide, nInstFlag::STALL),
    tInstLibEntry<tMethod>("inject", &cHardwareCPU::Inst_Inject, nInstFlag::STALL),
    tInstLibEntry<tMethod>("inject-r", &cHardwareCPU::Inst_InjectRand, nInstFlag::STALL),
    tInstLibEntry<tMethod>("transposon", &cHardwareCPU::Inst_Transposon),
    tInstLibEntry<tMethod>("search-f", &cHardwareCPU::Inst_SearchF),
    tInstLibEntry<tMethod>("search-b", &cHardwareCPU::Inst_SearchB),
    tInstLibEntry<tMethod>("mem-size", &cHardwareCPU::Inst_MemSize),
    
    tInstLibEntry<tMethod>("get", &cHardwareCPU::Inst_TaskGet, nInstFlag::STALL),
    tInstLibEntry<tMethod>("get-2", &cHardwareCPU::Inst_TaskGet2, nInstFlag::STALL),
    tInstLibEntry<tMethod>("stk-get", &cHardwareCPU::Inst_TaskStackGet, nInstFlag::STALL),
    tInstLibEntry<tMethod>("stk-load", &cHardwareCPU::Inst_TaskStackLoad, nInstFlag::STALL),
    tInstLibEntry<tMethod>("put", &cHardwareCPU::Inst_TaskPut, nInstFlag::STALL),
    tInstLibEntry<tMethod>("put-reset", &cHardwareCPU::Inst_TaskPutResetInputs, nInstFlag::STALL),
    tInstLibEntry<tMethod>("IO", &cHardwareCPU::Inst_TaskIO, nInstFlag::DEFAULT | nInstFlag::STALL, "Output ?BX?, and input new number back into ?BX?"),
    tInstLibEntry<tMethod>("IO-Feedback", &cHardwareCPU::Inst_TaskIO_Feedback, nInstFlag::STALL, "Output ?BX?, and input new number back into ?BX?,  and push 1,0,  or -1 onto stack1 if merit increased, stayed the same, or decreased"),
    tInstLibEntry<tMethod>("IO-bc-0.001", &cHardwareCPU::Inst_TaskIO_BonusCost_0_001, nInstFlag::STALL),
    tInstLibEntry<tMethod>("match-strings", &cHardwareCPU::Inst_MatchStrings, nInstFlag::STALL),
    tInstLibEntry<tMethod>("sell", &cHardwareCPU::Inst_Sell, nInstFlag::STALL),
    tInstLibEntry<tMethod>("buy", &cHardwareCPU::Inst_Buy, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send", &cHardwareCPU::Inst_Send, nInstFlag::STALL),
    tInstLibEntry<tMethod>("receive", &cHardwareCPU::Inst_Receive, nInstFlag::STALL),
    tInstLibEntry<tMethod>("sense", &cHardwareCPU::Inst_SenseLog2, nInstFlag::STALL),           // If you add more sense instructions
    tInstLibEntry<tMethod>("sense-unit", &cHardwareCPU::Inst_SenseUnit, nInstFlag::STALL),      // and want to keep stats, also add
    tInstLibEntry<tMethod>("sense-m100", &cHardwareCPU::Inst_SenseMult100, nInstFlag::STALL),   // the names to cStats::cStats() @JEB
    tInstLibEntry<tMethod>("if-resources", &cHardwareCPU::Inst_IfResources, nInstFlag::STALL),
    tInstLibEntry<tMethod>("collect", &cHardwareCPU::Inst_Collect, nInstFlag::STALL),
    tInstLibEntry<tMethod>("collect-no-env-remove", &cHardwareCPU::Inst_CollectNoEnvRemove, nInstFlag::STALL),
    tInstLibEntry<tMethod>("collect-no-internal-add", &cHardwareCPU::Inst_CollectNoInternalAdd, nInstFlag::STALL),
		
    tInstLibEntry<tMethod>("donate-rnd", &cHardwareCPU::Inst_DonateRandom),
    tInstLibEntry<tMethod>("donate-kin", &cHardwareCPU::Inst_DonateKin),
    tInstLibEntry<tMethod>("donate-edt", &cHardwareCPU::Inst_DonateEditDist),
    tInstLibEntry<tMethod>("donate-gbg",  &cHardwareCPU::Inst_DonateGreenBeardGene),
    tInstLibEntry<tMethod>("donate-tgb",  &cHardwareCPU::Inst_DonateTrueGreenBeard),
    tInstLibEntry<tMethod>("donate-threshgb",  &cHardwareCPU::Inst_DonateThreshGreenBeard),
    tInstLibEntry<tMethod>("donate-quantagb",  &cHardwareCPU::Inst_DonateQuantaThreshGreenBeard),
    tInstLibEntry<tMethod>("donate-NUL", &cHardwareCPU::Inst_DonateNULL),
    tInstLibEntry<tMethod>("donate-facing", &cHardwareCPU::Inst_DonateFacing),
    tInstLibEntry<tMethod>("receive-donated-energy", &cHardwareCPU::Inst_ReceiveDonatedEnergy, nInstFlag::STALL),
    tInstLibEntry<tMethod>("donate-energy", &cHardwareCPU::Inst_DonateEnergy, nInstFlag::STALL),
    tInstLibEntry<tMethod>("update-metabolic-rate", &cHardwareCPU::Inst_UpdateMetabolicRate, nInstFlag::STALL),
    tInstLibEntry<tMethod>("donate-energy-faced", &cHardwareCPU::Inst_DonateEnergyFaced, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-to-most-needy", &cHardwareCPU::Inst_RotateToMostNeedy, nInstFlag::STALL),
    tInstLibEntry<tMethod>("request-energy", &cHardwareCPU::Inst_RequestEnergy, nInstFlag::STALL),
    tInstLibEntry<tMethod>("request-energy-on", &cHardwareCPU::Inst_RequestEnergyFlagOn, nInstFlag::STALL),
    tInstLibEntry<tMethod>("request-energy-off", &cHardwareCPU::Inst_RequestEnergyFlagOff, nInstFlag::STALL),
    tInstLibEntry<tMethod>("increase-energy-donation", &cHardwareCPU::Inst_IncreaseEnergyDonation, nInstFlag::STALL),    
    tInstLibEntry<tMethod>("decrease-energy-donation", &cHardwareCPU::Inst_DecreaseEnergyDonation, nInstFlag::STALL),
    tInstLibEntry<tMethod>("IObuf-add1", &cHardwareCPU::Inst_IOBufAdd1, nInstFlag::STALL),
    tInstLibEntry<tMethod>("IObuf-add0", &cHardwareCPU::Inst_IOBufAdd0, nInstFlag::STALL),
		
    tInstLibEntry<tMethod>("rotate-l", &cHardwareCPU::Inst_RotateL, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-r", &cHardwareCPU::Inst_RotateR, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-left-one", &cHardwareCPU::Inst_RotateLeftOne, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-right-one", &cHardwareCPU::Inst_RotateRightOne, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-label", &cHardwareCPU::Inst_RotateLabel, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-to-unoccupied-cell", &cHardwareCPU::Inst_RotateUnoccupiedCell, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-to-next-unoccupied-cell", &cHardwareCPU::Inst_RotateNextUnoccupiedCell, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-to-occupied-cell", &cHardwareCPU::Inst_RotateOccupiedCell, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-to-next-occupied-cell", &cHardwareCPU::Inst_RotateNextOccupiedCell, nInstFlag::STALL),
    tInstLibEntry<tMethod>("rotate-to-event-cell", &cHardwareCPU::Inst_RotateEventCell, nInstFlag::STALL),
    
    tInstLibEntry<tMethod>("set-cmut", &cHardwareCPU::Inst_SetCopyMut),
    tInstLibEntry<tMethod>("mod-cmut", &cHardwareCPU::Inst_ModCopyMut),
    tInstLibEntry<tMethod>("get-cell-xy", &cHardwareCPU::Inst_GetCellPosition),
    tInstLibEntry<tMethod>("get-cell-x", &cHardwareCPU::Inst_GetCellPositionX),
    tInstLibEntry<tMethod>("get-cell-y", &cHardwareCPU::Inst_GetCellPositionY),
    tInstLibEntry<tMethod>("dist-from-diag", &cHardwareCPU::Inst_GetDistanceFromDiagonal),
		
    // State Grid instructions
    tInstLibEntry<tMethod>("sg-move", &cHardwareCPU::Inst_SGMove),
    tInstLibEntry<tMethod>("sg-rotate-l", &cHardwareCPU::Inst_SGRotateL),
    tInstLibEntry<tMethod>("sg-rotate-r", &cHardwareCPU::Inst_SGRotateR),
    tInstLibEntry<tMethod>("sg-sense", &cHardwareCPU::Inst_SGSense),
    

    
    // Movement instructions
    tInstLibEntry<tMethod>("tumble", &cHardwareCPU::Inst_Tumble, nInstFlag::STALL),
    tInstLibEntry<tMethod>("move", &cHardwareCPU::Inst_Move, nInstFlag::STALL),
    tInstLibEntry<tMethod>("move-to-event", &cHardwareCPU::Inst_MoveToEvent, nInstFlag::STALL),
    tInstLibEntry<tMethod>("if-event-in-unoccupied-neighbor-cell", &cHardwareCPU::Inst_IfNeighborEventInUnoccupiedCell),
    tInstLibEntry<tMethod>("if-event-in-faced-cell", &cHardwareCPU::Inst_IfFacingEventCell),
    tInstLibEntry<tMethod>("if-event-in-current-cell", &cHardwareCPU::Inst_IfEventInCell),
    
    // Threading instructions
    tInstLibEntry<tMethod>("fork-th", &cHardwareCPU::Inst_ForkThread),
    tInstLibEntry<tMethod>("forkl", &cHardwareCPU::Inst_ForkThreadLabel),
    tInstLibEntry<tMethod>("forkl!=0", &cHardwareCPU::Inst_ForkThreadLabelIfNot0),
    tInstLibEntry<tMethod>("forkl=0", &cHardwareCPU::Inst_ForkThreadLabelIf0),
    tInstLibEntry<tMethod>("kill-th", &cHardwareCPU::Inst_KillThread),
    tInstLibEntry<tMethod>("id-th", &cHardwareCPU::Inst_ThreadID),
    
    // Head-based instructions
    tInstLibEntry<tMethod>("h-alloc", &cHardwareCPU::Inst_MaxAlloc, nInstFlag::DEFAULT, "Allocate maximum allowed space"),
    tInstLibEntry<tMethod>("h-alloc-mw", &cHardwareCPU::Inst_MaxAllocMoveWriteHead),
    tInstLibEntry<tMethod>("h-divide", &cHardwareCPU::Inst_HeadDivide, nInstFlag::DEFAULT | nInstFlag::STALL, "Divide code between read and write heads."),
    tInstLibEntry<tMethod>("h-divide1RS", &cHardwareCPU::Inst_HeadDivide1RS, nInstFlag::STALL, "Divide code between read and write heads, at most one mutation on divide, resample if reverted."),
    tInstLibEntry<tMethod>("h-divide2RS", &cHardwareCPU::Inst_HeadDivide2RS, nInstFlag::STALL, "Divide code between read and write heads, at most two mutations on divide, resample if reverted."),
    tInstLibEntry<tMethod>("h-divideRS", &cHardwareCPU::Inst_HeadDivideRS, nInstFlag::STALL, "Divide code between read and write heads, resample if reverted."),
    tInstLibEntry<tMethod>("h-read", &cHardwareCPU::Inst_HeadRead),
    tInstLibEntry<tMethod>("h-write", &cHardwareCPU::Inst_HeadWrite),
    tInstLibEntry<tMethod>("h-copy", &cHardwareCPU::Inst_HeadCopy, nInstFlag::DEFAULT, "Copy from read-head to write-head; advance both"),
    tInstLibEntry<tMethod>("h-search", &cHardwareCPU::Inst_HeadSearch, nInstFlag::DEFAULT, "Find complement template and make with flow head"),
    tInstLibEntry<tMethod>("h-push", &cHardwareCPU::Inst_HeadPush),
    tInstLibEntry<tMethod>("h-pop", &cHardwareCPU::Inst_HeadPop),
    tInstLibEntry<tMethod>("set-head", &cHardwareCPU::Inst_SetHead),
    tInstLibEntry<tMethod>("adv-head", &cHardwareCPU::Inst_AdvanceHead),
    tInstLibEntry<tMethod>("mov-head", &cHardwareCPU::Inst_MoveHead, nInstFlag::DEFAULT, "Move head ?IP? to the flow head"),
    tInstLibEntry<tMethod>("jmp-head", &cHardwareCPU::Inst_JumpHead, nInstFlag::DEFAULT, "Move head ?IP? by amount in CX register; CX = old pos."),
    tInstLibEntry<tMethod>("get-head", &cHardwareCPU::Inst_GetHead, nInstFlag::DEFAULT, "Copy the position of the ?IP? head into CX"),
    tInstLibEntry<tMethod>("if-label", &cHardwareCPU::Inst_IfLabel, nInstFlag::DEFAULT, "Execute next if we copied complement of attached label"),
    tInstLibEntry<tMethod>("if-label2", &cHardwareCPU::Inst_IfLabel2, 0, "If copied label compl., exec next inst; else SKIP W/NOPS"),
    tInstLibEntry<tMethod>("set-flow", &cHardwareCPU::Inst_SetFlow, nInstFlag::DEFAULT, "Set flow-head to position in ?CX?"),
    
    tInstLibEntry<tMethod>("h-copy2", &cHardwareCPU::Inst_HeadCopy2),
    tInstLibEntry<tMethod>("h-copy3", &cHardwareCPU::Inst_HeadCopy3),
    tInstLibEntry<tMethod>("h-copy4", &cHardwareCPU::Inst_HeadCopy4),
    tInstLibEntry<tMethod>("h-copy5", &cHardwareCPU::Inst_HeadCopy5),
    tInstLibEntry<tMethod>("h-copy6", &cHardwareCPU::Inst_HeadCopy6),
    tInstLibEntry<tMethod>("h-copy7", &cHardwareCPU::Inst_HeadCopy7),
    tInstLibEntry<tMethod>("h-copy8", &cHardwareCPU::Inst_HeadCopy8),
    tInstLibEntry<tMethod>("h-copy9", &cHardwareCPU::Inst_HeadCopy9),
    tInstLibEntry<tMethod>("h-copy10", &cHardwareCPU::Inst_HeadCopy10),
    
    tInstLibEntry<tMethod>("divide-sex", &cHardwareCPU::Inst_HeadDivideSex, nInstFlag::STALL),
    tInstLibEntry<tMethod>("divide-asex", &cHardwareCPU::Inst_HeadDivideAsex, nInstFlag::STALL),
    
    tInstLibEntry<tMethod>("div-sex", &cHardwareCPU::Inst_HeadDivideSex, nInstFlag::STALL),
    tInstLibEntry<tMethod>("div-asex", &cHardwareCPU::Inst_HeadDivideAsex, nInstFlag::STALL),
    tInstLibEntry<tMethod>("div-asex-w", &cHardwareCPU::Inst_HeadDivideAsexWait, nInstFlag::STALL),
    tInstLibEntry<tMethod>("div-sex-MS", &cHardwareCPU::Inst_HeadDivideMateSelect, nInstFlag::STALL),
    
    tInstLibEntry<tMethod>("h-divide1", &cHardwareCPU::Inst_HeadDivide1, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide2", &cHardwareCPU::Inst_HeadDivide2, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide3", &cHardwareCPU::Inst_HeadDivide3, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide4", &cHardwareCPU::Inst_HeadDivide4, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide5", &cHardwareCPU::Inst_HeadDivide5, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide6", &cHardwareCPU::Inst_HeadDivide6, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide7", &cHardwareCPU::Inst_HeadDivide7, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide8", &cHardwareCPU::Inst_HeadDivide8, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide9", &cHardwareCPU::Inst_HeadDivide9, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide10", &cHardwareCPU::Inst_HeadDivide10, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide16", &cHardwareCPU::Inst_HeadDivide16, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide32", &cHardwareCPU::Inst_HeadDivide32, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide50", &cHardwareCPU::Inst_HeadDivide50, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide100", &cHardwareCPU::Inst_HeadDivide100, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide500", &cHardwareCPU::Inst_HeadDivide500, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide1000", &cHardwareCPU::Inst_HeadDivide1000, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide5000", &cHardwareCPU::Inst_HeadDivide5000, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide10000", &cHardwareCPU::Inst_HeadDivide10000, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide50000", &cHardwareCPU::Inst_HeadDivide50000, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide0.5", &cHardwareCPU::Inst_HeadDivide0_5, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide0.1", &cHardwareCPU::Inst_HeadDivide0_1, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide0.05", &cHardwareCPU::Inst_HeadDivide0_05, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide0.01", &cHardwareCPU::Inst_HeadDivide0_01, nInstFlag::STALL),
    tInstLibEntry<tMethod>("h-divide0.001", &cHardwareCPU::Inst_HeadDivide0_001, nInstFlag::STALL),
    
    // High-level instructions
		tInstLibEntry<tMethod>("repro_deme", &cHardwareCPU::Inst_ReproDeme, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-sex", &cHardwareCPU::Inst_ReproSex, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-A", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-B", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-C", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-D", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-E", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-F", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-G", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-H", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-I", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-J", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-K", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-L", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-M", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-N", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-O", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-P", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-Q", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-R", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-S", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-T", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-U", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-V", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-W", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-X", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-Y", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("repro-Z", &cHardwareCPU::Inst_Repro, nInstFlag::STALL),
		
    tInstLibEntry<tMethod>("put-repro", &cHardwareCPU::Inst_TaskPutRepro, nInstFlag::STALL),
    tInstLibEntry<tMethod>("metabolize", &cHardwareCPU::Inst_TaskPutResetInputsRepro, nInstFlag::STALL),        
		
    tInstLibEntry<tMethod>("sterilize", &cHardwareCPU::Inst_Sterilize),
    
    tInstLibEntry<tMethod>("spawn-deme", &cHardwareCPU::Inst_SpawnDeme, nInstFlag::STALL),
    
    // Suicide
    tInstLibEntry<tMethod>("kazi",	&cHardwareCPU::Inst_Kazi, nInstFlag::STALL),
    tInstLibEntry<tMethod>("kazi5", &cHardwareCPU::Inst_Kazi5, nInstFlag::STALL),
    tInstLibEntry<tMethod>("die", &cHardwareCPU::Inst_Die, nInstFlag::STALL),
    tInstLibEntry<tMethod>("suicide", &cHardwareCPU::Inst_Suicide, nInstFlag::STALL),		
    tInstLibEntry<tMethod>("relinquishEnergyToFutureDeme", &cHardwareCPU::Inst_RelinquishEnergyToFutureDeme, nInstFlag::STALL),
    tInstLibEntry<tMethod>("relinquishEnergyToNeighborOrganisms", &cHardwareCPU::Inst_RelinquishEnergyToNeighborOrganisms, nInstFlag::STALL),
    tInstLibEntry<tMethod>("relinquishEnergyToOrganismsInDeme", &cHardwareCPU::Inst_RelinquishEnergyToOrganismsInDeme, nInstFlag::STALL),

    // Energy level detection
    tInstLibEntry<tMethod>("if-energy-low", &cHardwareCPU::Inst_IfEnergyLow, nInstFlag::STALL),
    tInstLibEntry<tMethod>("if-energy-not-low", &cHardwareCPU::Inst_IfEnergyNotLow, nInstFlag::STALL),
    tInstLibEntry<tMethod>("if-faced-energy-low", &cHardwareCPU::Inst_IfFacedEnergyLow, nInstFlag::STALL),
    tInstLibEntry<tMethod>("if-faced-energy-not-low", &cHardwareCPU::Inst_IfFacedEnergyNotLow, nInstFlag::STALL),
    tInstLibEntry<tMethod>("if-energy-high", &cHardwareCPU::Inst_IfEnergyHigh, nInstFlag::STALL),
  	tInstLibEntry<tMethod>("if-energy-not-high", &cHardwareCPU::Inst_IfEnergyNotHigh, nInstFlag::STALL),
  	tInstLibEntry<tMethod>("if-faced-energy-high", &cHardwareCPU::Inst_IfFacedEnergyHigh, nInstFlag::STALL),
  	tInstLibEntry<tMethod>("if-faced-energy-not-high", &cHardwareCPU::Inst_IfFacedEnergyNotHigh, nInstFlag::STALL),
  	tInstLibEntry<tMethod>("if-energy-med", &cHardwareCPU::Inst_IfEnergyMed, nInstFlag::STALL),	 
  	tInstLibEntry<tMethod>("if-faced-energy-med", &cHardwareCPU::Inst_IfFacedEnergyMed, nInstFlag::STALL),	
  	tInstLibEntry<tMethod>("if-faced-energy-less", &cHardwareCPU::Inst_IfFacedEnergyLess, nInstFlag::STALL),	
  	tInstLibEntry<tMethod>("if-faced-energy-more", &cHardwareCPU::Inst_IfFacedEnergyMore, nInstFlag::STALL),	    
	  tInstLibEntry<tMethod>("if-energy-in-buffer", &cHardwareCPU::Inst_IfEnergyInBuffer, nInstFlag::STALL),
	  tInstLibEntry<tMethod>("if-energy-not-in-buffer", &cHardwareCPU::Inst_IfEnergyNotInBuffer, nInstFlag::STALL),
	  
    // Sleep and time
    tInstLibEntry<tMethod>("sleep", &cHardwareCPU::Inst_Sleep, nInstFlag::STALL),
    tInstLibEntry<tMethod>("sleep1", &cHardwareCPU::Inst_Sleep, nInstFlag::STALL),
    tInstLibEntry<tMethod>("sleep2", &cHardwareCPU::Inst_Sleep, nInstFlag::STALL),
    tInstLibEntry<tMethod>("sleep3", &cHardwareCPU::Inst_Sleep, nInstFlag::STALL),
    tInstLibEntry<tMethod>("sleep4", &cHardwareCPU::Inst_Sleep, nInstFlag::STALL),
    tInstLibEntry<tMethod>("time", &cHardwareCPU::Inst_GetUpdate, nInstFlag::STALL),
    
    // Promoter Model
    tInstLibEntry<tMethod>("promoter", &cHardwareCPU::Inst_Promoter),
    tInstLibEntry<tMethod>("terminate", &cHardwareCPU::Inst_Terminate),
    tInstLibEntry<tMethod>("promoter", &cHardwareCPU::Inst_Promoter),
    tInstLibEntry<tMethod>("terminate", &cHardwareCPU::Inst_Terminate),
    tInstLibEntry<tMethod>("regulate", &cHardwareCPU::Inst_Regulate),
    tInstLibEntry<tMethod>("regulate-sp", &cHardwareCPU::Inst_RegulateSpecificPromoters),
    tInstLibEntry<tMethod>("s-regulate", &cHardwareCPU::Inst_SenseRegulate),
    tInstLibEntry<tMethod>("numberate", &cHardwareCPU::Inst_Numberate),
    tInstLibEntry<tMethod>("numberate-24", &cHardwareCPU::Inst_Numberate24),
		
    // Bit Consensus
    tInstLibEntry<tMethod>("bit-cons", &cHardwareCPU::Inst_BitConsensus),
    tInstLibEntry<tMethod>("bit-cons-24", &cHardwareCPU::Inst_BitConsensus24),
    tInstLibEntry<tMethod>("if-cons", &cHardwareCPU::Inst_IfConsensus, 0, "Execute next instruction if ?BX? in consensus, else skip it"),
    tInstLibEntry<tMethod>("if-cons-24", &cHardwareCPU::Inst_IfConsensus24, 0, "Execute next instruction if ?BX[0:23]? in consensus , else skip it"),
    tInstLibEntry<tMethod>("if-less-cons", &cHardwareCPU::Inst_IfLessConsensus, 0, "Execute next instruction if Count(?BX?) < Count(?CX?), else skip it"),
    tInstLibEntry<tMethod>("if-less-cons-24", &cHardwareCPU::Inst_IfLessConsensus24, 0, "Execute next instruction if Count(?BX[0:23]?) < Count(?CX[0:23]?), else skip it"),
		
    // Energy usage
    tInstLibEntry<tMethod>("double-energy-usage", &cHardwareCPU::Inst_DoubleEnergyUsage, nInstFlag::STALL),
    tInstLibEntry<tMethod>("half-energy-usage", &cHardwareCPU::Inst_HalfEnergyUsage, nInstFlag::STALL),
    tInstLibEntry<tMethod>("default-energy-usage", &cHardwareCPU::Inst_DefaultEnergyUsage, nInstFlag::STALL),
		
    // Messaging
    tInstLibEntry<tMethod>("send-msg", &cHardwareCPU::Inst_SendMessage, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-msg-type0", &cHardwareCPU::Inst_SendMessageType0, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-msg-type1", &cHardwareCPU::Inst_SendMessageType1, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-msg-type2", &cHardwareCPU::Inst_SendMessageType2, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-msg-type3", &cHardwareCPU::Inst_SendMessageType3, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-msg-type4", &cHardwareCPU::Inst_SendMessageType4, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-msg-type5", &cHardwareCPU::Inst_SendMessageType5, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-msg-type6", &cHardwareCPU::Inst_SendMessageType6, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-msg-type7", &cHardwareCPU::Inst_SendMessageType7, nInstFlag::STALL),
    tInstLibEntry<tMethod>("retrieve-msg", &cHardwareCPU::Inst_RetrieveMessage, nInstFlag::STALL),
		
    // Alarms
    tInstLibEntry<tMethod>("send-alarm-msg-local", &cHardwareCPU::Inst_Alarm_MSG_local, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-alarm-msg-multihop", &cHardwareCPU::Inst_Alarm_MSG_multihop, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-alarm-msg-bit-cons24-local", &cHardwareCPU::Inst_Alarm_MSG_Bit_Cons24_local, nInstFlag::STALL),
    tInstLibEntry<tMethod>("send-alarm-msg-bit-cons24-multihop", &cHardwareCPU::Inst_Alarm_MSG_Bit_Cons24_multihop, nInstFlag::STALL),
    tInstLibEntry<tMethod>("alarm-label-high", &cHardwareCPU::Inst_Alarm_Label),
    tInstLibEntry<tMethod>("alarm-label-low", &cHardwareCPU::Inst_Alarm_Label),
		
		// Interrupt
    tInstLibEntry<tMethod>("msg-handler", &cHardwareCPU::Inst_MSG_Handler),
    tInstLibEntry<tMethod>("moved-handler", &cHardwareCPU::Inst_Moved_Handler), // not well tested.
		tInstLibEntry<tMethod>("end-handler", &cHardwareCPU::Inst_End_Handler),
		
    // Placebo instructions
    tInstLibEntry<tMethod>("skip", &cHardwareCPU::Inst_Skip),
		
    // @BDC additions for pheromones
    tInstLibEntry<tMethod>("phero-on", &cHardwareCPU::Inst_PheroOn),
    tInstLibEntry<tMethod>("phero-off", &cHardwareCPU::Inst_PheroOff),
    tInstLibEntry<tMethod>("pherotoggle", &cHardwareCPU::Inst_PheroToggle),
    tInstLibEntry<tMethod>("sense-target", &cHardwareCPU::Inst_SenseTarget),
    tInstLibEntry<tMethod>("sense-target-faced", &cHardwareCPU::Inst_SenseTargetFaced),
    tInstLibEntry<tMethod>("sensef", &cHardwareCPU::Inst_SenseLog2Facing),
    tInstLibEntry<tMethod>("sensef-unit", &cHardwareCPU::Inst_SenseUnitFacing),
    tInstLibEntry<tMethod>("sensef-m100", &cHardwareCPU::Inst_SenseMult100Facing),
    tInstLibEntry<tMethod>("sense-pheromone", &cHardwareCPU::Inst_SensePheromone),
    tInstLibEntry<tMethod>("sense-pheromone-faced", &cHardwareCPU::Inst_SensePheromoneFaced),
    tInstLibEntry<tMethod>("exploit", &cHardwareCPU::Inst_Exploit, nInstFlag::STALL),
    tInstLibEntry<tMethod>("exploit-forward5", &cHardwareCPU::Inst_ExploitForward5, nInstFlag::STALL),
    tInstLibEntry<tMethod>("exploit-forward3", &cHardwareCPU::Inst_ExploitForward3, nInstFlag::STALL),
    tInstLibEntry<tMethod>("explore", &cHardwareCPU::Inst_Explore, nInstFlag::STALL),
    tInstLibEntry<tMethod>("movetarget", &cHardwareCPU::Inst_MoveTarget, nInstFlag::STALL),
    tInstLibEntry<tMethod>("movetarget-forward5", &cHardwareCPU::Inst_MoveTargetForward5, nInstFlag::STALL),
    tInstLibEntry<tMethod>("movetarget-forward3", &cHardwareCPU::Inst_MoveTargetForward3, nInstFlag::STALL),
    tInstLibEntry<tMethod>("supermove", &cHardwareCPU::Inst_SuperMove, nInstFlag::STALL),
    tInstLibEntry<tMethod>("if-target", &cHardwareCPU::Inst_IfTarget),
    tInstLibEntry<tMethod>("if-not-target", &cHardwareCPU::Inst_IfNotTarget),
    tInstLibEntry<tMethod>("if-pheromone", &cHardwareCPU::Inst_IfPheromone),
    tInstLibEntry<tMethod>("if-not-pheromone", &cHardwareCPU::Inst_IfNotPheromone),
    tInstLibEntry<tMethod>("drop-pheromone", &cHardwareCPU::Inst_DropPheromone, nInstFlag::STALL),
    
    // Opinion instructions.
    // These are STALLs because opinions are only relevant with respect to time.
    tInstLibEntry<tMethod>("set-opinion", &cHardwareCPU::Inst_SetOpinion, nInstFlag::STALL),
    tInstLibEntry<tMethod>("get-opinion", &cHardwareCPU::Inst_GetOpinion, nInstFlag::STALL),
		
		// Data collection
		tInstLibEntry<tMethod>("if-cell-data-changed", &cHardwareCPU::Inst_IfCellDataChanged, nInstFlag::STALL),
    tInstLibEntry<tMethod>("collect-cell-data", &cHardwareCPU::Inst_CollectCellData, nInstFlag::STALL),
    tInstLibEntry<tMethod>("kill-cell-event", &cHardwareCPU::Inst_KillCellEvent, nInstFlag::STALL),
    tInstLibEntry<tMethod>("kill-faced-cell-event", &cHardwareCPU::Inst_KillFacedCellEvent, nInstFlag::STALL),
    tInstLibEntry<tMethod>("collect-cell-data-and-kill-event", &cHardwareCPU::Inst_CollectCellDataAndKillEvent, nInstFlag::STALL),
		
		// Synchronization
    tInstLibEntry<tMethod>("flash", &cHardwareCPU::Inst_Flash, nInstFlag::STALL),
    tInstLibEntry<tMethod>("if-recvd-flash", &cHardwareCPU::Inst_IfRecvdFlash, nInstFlag::STALL),
    tInstLibEntry<tMethod>("flash-info", &cHardwareCPU::Inst_FlashInfo, nInstFlag::STALL),
    tInstLibEntry<tMethod>("flash-info-b", &cHardwareCPU::Inst_FlashInfoB, nInstFlag::STALL),
    tInstLibEntry<tMethod>("reset-flash-info", &cHardwareCPU::Inst_ResetFlashInfo, nInstFlag::STALL),
    tInstLibEntry<tMethod>("hard-reset", &cHardwareCPU::Inst_HardReset, nInstFlag::STALL),
    tInstLibEntry<tMethod>("get-cycles", &cHardwareCPU::Inst_GetCycles, nInstFlag::STALL),		

    // Must always be the last instruction in the array
    tInstLibEntry<tMethod>("NULL", &cHardwareCPU::Inst_Nop, 0, "True no-operation instruction: does nothing"),
  };
  
  const int n_size = sizeof(s_n_array)/sizeof(cNOPEntryCPU);
  
  static cString n_names[n_size];
  static int nop_mods[n_size];
  for (int i = 0; i < n_size && i < NUM_REGISTERS; i++) {
    n_names[i] = s_n_array[i].name;
    nop_mods[i] = s_n_array[i].nop_mod;
  }
  
  const int f_size = sizeof(s_f_array)/sizeof(tInstLibEntry<tMethod>);
  static tMethod functions[f_size];
  for (int i = 0; i < f_size; i++) functions[i] = s_f_array[i].GetFunction();
	
	const int def = 0;
  const int null_inst = f_size - 1;
  
  return new tInstLib<tMethod>(f_size, s_f_array, n_names, nop_mods, functions, def, null_inst);
}

cHardwareCPU::cHardwareCPU(cAvidaContext& ctx, cWorld* world, cOrganism* in_organism, cInstSet* in_m_inst_set)
: cHardwareBase(world, in_organism, in_m_inst_set)
, m_last_cell_data(false, 0)
{
  m_functions = s_inst_slib->GetFunctions();
  
  m_spec_die = false;
  m_epigenetic_state = false;
  
  m_thread_slicing_parallel = (m_world->GetConfig().THREAD_SLICING_METHOD.Get() == 1);
  m_no_cpu_cycle_time = m_world->GetConfig().NO_CPU_CYCLE_TIME.Get();
  
  m_promoters_enabled = m_world->GetConfig().PROMOTERS_ENABLED.Get();
  m_constitutive_regulation = m_world->GetConfig().CONSTITUTIVE_REGULATION.Get();
  
  m_slip_read_head = !m_world->GetConfig().SLIP_COPY_MODE.Get();
  
  m_memory = in_organism->GetGenome();  // Initialize memory...
  Reset(ctx);                            // Setup the rest of the hardware...
}


void cHardwareCPU::internalReset()
{
  m_global_stack.Clear();
  
  // We want to reset to have a single thread.
  m_threads.Resize(1);
  
  // Reset that single thread.
  m_threads[0].Reset(m_world, this, 0);
  m_thread_id_chart = 1; // Mark only the first thread as taken...
  m_cur_thread = 0;
  
  // But then reset thread to have any epigenetic information we have saved
  if (m_epigenetic_state) {
    for (int i=0; i<NUM_REGISTERS; i++) {
      m_threads[0].reg[i] = m_epigenetic_saved_reg[i];
    }
    m_threads[0].stack = m_epigenetic_saved_stack;
  }
  
  m_mal_active = false;
  m_executedmatchstrings = false;
  
	
  // Promoter model
  if (m_world->GetConfig().PROMOTERS_ENABLED.Get())
  {
    // Ideally, this shouldn't be hard-coded
    cInstruction promoter_inst = m_world->GetHardwareManager().GetInstSet().GetInst(cStringUtil::Stringf("promoter"));
		
    m_promoter_index = -1; // Meaning the last promoter was nothing
    m_promoter_offset = 0;
    m_promoters.Resize(0);
    for (int i=0; i< m_memory.GetSize(); i++)
    {
      if (m_memory[i] == promoter_inst)
      {
        int code = Numberate(i-1, -1, m_world->GetConfig().PROMOTER_CODE_SIZE.Get());
        m_promoters.Push( cPromoter(i,code) );
      }
    }
  }
	m_last_cell_data = std::make_pair(false, 0);
}

// This function processes the very next command in the genome, and is made
// to be as optimized as possible.  This is the heart of avida.

bool cHardwareCPU::SingleProcess(cAvidaContext& ctx, bool speculative)
{
  assert(!speculative || (speculative && !m_thread_slicing_parallel));
	
  int last_IP_pos = IP().GetPosition();
  
  // Mark this organism as running...
  m_organism->SetRunning(true);
  
  if (!speculative && m_spec_die) {
    m_organism->Die();
    m_organism->SetRunning(false);
    return false;
  }
	
  cPhenotype& phenotype = m_organism->GetPhenotype();
  
  // First instruction - check whether we should be starting at a promoter, when enabled.
  if (phenotype.GetCPUCyclesUsed() == 0 && m_promoters_enabled) Inst_Terminate(ctx);
  
  // Count the cpu cycles used
  phenotype.IncCPUCyclesUsed();
  if (!m_world->GetConfig().NO_CPU_CYCLE_TIME.Get()) phenotype.IncTimeUsed();
	
  const int num_threads = m_threads.GetSize();
  
  // If we have threads turned on and we executed each thread in a single
  // timestep, adjust the number of instructions executed accordingly.
  const int num_inst_exec = m_thread_slicing_parallel ? num_threads : 1;
  
  for (int i = 0; i < num_inst_exec; i++) {
    // Setup the hardware for the next instruction to be executed.
    int last_thread = m_cur_thread++;
    if (m_cur_thread >= num_threads) m_cur_thread = 0;
    
    m_advance_ip = true;
    cHeadCPU& ip = m_threads[m_cur_thread].heads[nHardware::HEAD_IP];
    ip.Adjust();
    
#if BREAKPOINTS
    if (ip.FlagBreakpoint()) {
      m_organism->DoBreakpoint();
    }
#endif
    
    // Print the status of this CPU at each step...
    if (m_tracer != NULL) m_tracer->TraceHardware(*this);
    
    // Find the instruction to be executed
    const cInstruction& cur_inst = ip.GetInst();
    
    if (speculative && (m_spec_die || m_inst_set->ShouldStall(cur_inst))) {
      // Speculative instruction reject, flush and return
      m_cur_thread = last_thread;
      phenotype.DecCPUCyclesUsed();
      if (!m_no_cpu_cycle_time) phenotype.IncTimeUsed(-1);
      m_organism->SetRunning(false);
      return false;
    }
    
    // Test if costs have been paid and it is okay to execute this now...
    bool exec = true;
    if (m_has_any_costs) exec = SingleProcess_PayCosts(ctx, cur_inst);
		
    // Constitutive regulation applied here
    if (m_constitutive_regulation) Inst_SenseRegulate(ctx); 
		
    // If there are no active promoters and a certain mode is set, then don't execute any further instructions
    if (m_promoters_enabled && m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() == 2 && m_promoter_index == -1) exec = false;
		
    // Now execute the instruction...
    if (exec == true) {
      // NOTE: This call based on the cur_inst must occur prior to instruction
      //       execution, because this instruction reference may be invalid after
      //       certain classes of instructions (namely divide instructions) @DMB
      const int time_cost = m_inst_set->GetAddlTimeCost(cur_inst);
			
      // Prob of exec (moved from SingleProcess_PayCosts so that we advance IP after a fail)
      if (m_inst_set->GetProbFail(cur_inst) > 0.0) {
        exec = !( ctx.GetRandom().P(m_inst_set->GetProbFail(cur_inst)) );
      }
      
      // Flag instruction as executed even if it failed (moved from SingleProcess_ExecuteInst)
      // this allows division conditions to be met even if most instruction executions failed. @JEB
      
      // Mark the instruction as executed
      IP().SetFlagExecuted();
      
      // Add to the promoter inst executed count before executing the inst (in case it is a terminator)
      if (m_promoters_enabled) m_threads[m_cur_thread].IncPromoterInstExecuted();
			
      if (exec == true) SingleProcess_ExecuteInst(ctx, cur_inst);
			
      // Some instruction (such as jump) may turn m_advance_ip off.  Usually
      // we now want to move to the next instruction in the memory.
      if (m_advance_ip == true) ip.Advance();
      
      // Pay the time cost of the instruction now
      phenotype.IncTimeUsed(time_cost);
			
      // In the promoter model, we may force termination after a certain number of inst have been executed
      if (m_promoters_enabled) {
        const double processivity = m_world->GetConfig().PROMOTER_PROCESSIVITY.Get();
        if (ctx.GetRandom().P(1 - processivity)) Inst_Terminate(ctx);
        if (m_world->GetConfig().PROMOTER_INST_MAX.Get() && (m_threads[m_cur_thread].GetPromoterInstExecuted() >= m_world->GetConfig().PROMOTER_INST_MAX.Get())) 
          Inst_Terminate(ctx);
      }
			
    } // if exec
		
		if(m_world->GetConfig().INTERRUPT_ENABLED.Get() && GetInstSet().InstInSet("move")) {
			static const cInstruction moveInst = GetInstSet().GetInst("move");
			if(cur_inst == moveInst) {
				// fire move interrupt
				m_threads[m_cur_thread].interruptContextSwitch(cLocalThread::MOVE_INTERRUPT);
				m_advance_ip = false;
			}
		}
  } // Previous was executed once for each thread...
	
  // Kill creatures who have reached their max num of instructions executed
  const int max_executed = m_organism->GetMaxExecuted();
  if ((max_executed > 0 && phenotype.GetTimeUsed() >= max_executed) || phenotype.GetToDie() == true) {
    if (speculative) m_spec_die = true;
    else m_organism->Die();
  }
  if (!speculative && phenotype.GetToDelete()) m_spec_die = true;
  
  // Note: if organism just died, this will NOT let it repro.
  CheckImplicitRepro(ctx, last_IP_pos > m_threads[m_cur_thread].heads[nHardware::HEAD_IP].GetPosition());
  
  m_organism->SetRunning(false);
  
  return !m_spec_die;
}

// This method will handle the actual execution of an instruction
// within a single process, once that function has been finalized.
bool cHardwareCPU::SingleProcess_ExecuteInst(cAvidaContext& ctx, const cInstruction& cur_inst) 
{
  // Copy Instruction locally to handle stochastic effects
  cInstruction actual_inst = cur_inst;
  
#ifdef EXECUTION_ERRORS
  // If there is an execution error, execute a random instruction.
  if (m_organism->TestExeErr()) actual_inst = m_inst_set->GetRandomInst(ctx);
#endif /* EXECUTION_ERRORS */
	
  // Get a pointer to the corresponding method...
  int inst_idx = m_inst_set->GetLibFunctionIndex(actual_inst);
  
#if INSTRUCTION_COUNT
  // instruction execution count incremented
  m_organism->GetPhenotype().IncCurInstCount(actual_inst.GetOp());
#endif
	
  // And execute it.
  const bool exec_success = (this->*(m_functions[inst_idx]))(ctx);
	
  // NOTE: Organism may be dead now if instruction executed killed it (such as some divides, "die", or "kazi")
	
#if INSTRUCTION_COUNT
  // Decrement if the instruction was not executed successfully.
  if (exec_success == false) {
    m_organism->GetPhenotype().DecCurInstCount(actual_inst.GetOp());
  }
#endif	
  
  return exec_success;
}


void cHardwareCPU::ProcessBonusInst(cAvidaContext& ctx, const cInstruction& inst)
{
  // Mark this organism as running...
  bool prev_run_state = m_organism->IsRunning();
  m_organism->SetRunning(true);
  
  if (m_tracer != NULL) m_tracer->TraceHardware(*this, true);
  
  SingleProcess_ExecuteInst(ctx, inst);
  
  m_organism->SetRunning(prev_run_state);
}


bool cHardwareCPU::OK()
{
  bool result = true;
  
  if (!m_memory.OK()) result = false;
  
  for (int i = 0; i < m_threads.GetSize(); i++) {
    if (m_threads[i].stack.OK() == false) result = false;
    if (m_threads[i].next_label.OK() == false) result = false;
  }
  
  return result;
}

void cHardwareCPU::PrintStatus(ostream& fp)
{
  fp << m_organism->GetPhenotype().GetCPUCyclesUsed() << " ";
  fp << "IP:" << IP().GetPosition() << "    ";
  
  for (int i = 0; i < NUM_REGISTERS; i++) {
    fp << static_cast<char>('A' + i) << "X:" << GetRegister(i) << " ";
    fp << setbase(16) << "[0x" << GetRegister(i) << "]  " << setbase(10);
  }
  
  // Add some extra information if additional time costs are used for instructions,
  // leave this out if there are no differences to keep it cleaner
  if ( m_organism->GetPhenotype().GetTimeUsed() != m_organism->GetPhenotype().GetCPUCyclesUsed() )
  {
    fp << "  EnergyUsed:" << m_organism->GetPhenotype().GetTimeUsed();  // NOT ENERGY MODEL
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
  
  fp << "  Mem (" << m_memory.GetSize() << "):"
	<< "  " << m_memory.AsString()
	<< endl;
	
  if (m_ext_mem.GetSize()) {
    fp << "  Ext Mem: " << m_ext_mem[0];
    for (int i = 1; i < m_ext_mem.GetSize(); i++) fp << ", " << m_ext_mem[i];
    fp << endl;
  }
  
  if (m_world->GetConfig().PROMOTERS_ENABLED.Get())
  {
    fp << "  Promoters: index=" << m_promoter_index << " offset=" << m_promoter_offset;
    fp << " exe_inst=" << m_threads[m_cur_thread].GetPromoterInstExecuted();
    for (int i=0; i<m_promoters.GetSize(); i++)
    {
      fp << setfill(' ') << setbase(10) << " " << m_promoters[i].m_pos << ":";
      fp << "Ox" << setbase(16) << setfill('0') << setw(8) << (m_promoters[i].GetRegulatedBitCode()) << " "; 
    }
    fp << setfill(' ') << setbase(10) << endl;
  }
	
  fp.flush();
}





/////////////////////////////////////////////////////////////////////////
// Method: cHardwareCPU::FindLabel(direction)
//
// Search in 'direction' (+ or - 1) from the instruction pointer for the
// compliment of the label in 'next_label' and return a pointer to the
// results.  If direction is 0, search from the beginning of the genome.
//
/////////////////////////////////////////////////////////////////////////

cHeadCPU cHardwareCPU::FindLabel(int direction)
{
  cHeadCPU & inst_ptr = IP();
  
  // Start up a search head at the position of the instruction pointer.
  cHeadCPU search_head(inst_ptr);
  cCodeLabel & search_label = GetLabel();
  
  // Make sure the label is of size > 0.
  
  if (search_label.GetSize() == 0) {
    return inst_ptr;
  }
  
  // Call special functions depending on if jump is forwards or backwards.
  int found_pos = 0;
  if( direction < 0 ) {
    found_pos = FindLabel_Backward(search_label, inst_ptr.GetMemory(),
                                   inst_ptr.GetPosition() - search_label.GetSize());
  }
  
  // Jump forward.
  else if (direction > 0) {
    found_pos = FindLabel_Forward(search_label, inst_ptr.GetMemory(),
                                  inst_ptr.GetPosition());
  }
  
  // Jump forward from the very beginning.
  else {
    found_pos = FindLabel_Forward(search_label, inst_ptr.GetMemory(), 0);
  }
  
  // Return the last line of the found label, if it was found.
  if (found_pos >= 0) search_head.Set(found_pos - 1);
  
  // Return the found position (still at start point if not found).
  return search_head;
}


// Search forwards for search_label from _after_ position pos in the
// memory.  Return the first line _after_ the the found label.  It is okay
// to find search label's match inside another label.

int cHardwareCPU::FindLabel_Forward(const cCodeLabel & search_label,
                                    const cGenome & search_genome, int pos)
{
  assert (pos < search_genome.GetSize() && pos >= 0);
  
  int search_start = pos;
  int label_size = search_label.GetSize();
  bool found_label = false;
  
  // Move off the template we are on.
  pos += label_size;
  
  // Search until we find the complement or exit the memory.
  while (pos < search_genome.GetSize()) {
    
    // If we are within a label, rewind to the beginning of it and see if
    // it has the proper sub-label that we're looking for.
    
    if (m_inst_set->IsNop(search_genome[pos])) {
      // Find the start and end of the label we're in the middle of.
      
      int start_pos = pos;
      int end_pos = pos + 1;
      while (start_pos > search_start &&
             m_inst_set->IsNop( search_genome[start_pos - 1] )) {
        start_pos--;
      }
      while (end_pos < search_genome.GetSize() &&
             m_inst_set->IsNop( search_genome[end_pos] )) {
        end_pos++;
      }
      int test_size = end_pos - start_pos;
      
      // See if this label has the proper sub-label within it.
      int max_offset = test_size - label_size + 1;
      int offset = start_pos;
      for (offset = start_pos; offset < start_pos + max_offset; offset++) {
        
        // Test the number of matches for this offset.
        int matches;
        for (matches = 0; matches < label_size; matches++) {
          if (search_label[matches] !=
              m_inst_set->GetNopMod( search_genome[offset + matches] )) {
            break;
          }
        }
        
        // If we have found it, break out of this loop!
        if (matches == label_size) {
          found_label = true;
          break;
        }
      }
      
      // If we've found the complement label, set the position to the end of
      // the label we found it in, and break out.
      
      if (found_label == true) {
        // pos = end_pos;
        pos = label_size + offset;
        break;
      }
      
      // We haven't found it; jump pos to just after the current label being
      // checked.
      pos = end_pos;
    }
    
    // Jump up a block to the next possible point to find a label,
    pos += label_size;
  }
  
  // If the label was not found return a -1.
  if (found_label == false) pos = -1;
  
  return pos;
}

// Search backwards for search_label from _before_ position pos in the
// memory.  Return the first line _after_ the the found label.  It is okay
// to find search label's match inside another label.

int cHardwareCPU::FindLabel_Backward(const cCodeLabel & search_label,
                                     const cGenome & search_genome, int pos)
{
  assert (pos < search_genome.GetSize());
  
  int search_start = pos;
  int label_size = search_label.GetSize();
  bool found_label = false;
  
  // Move off the template we are on.
  pos -= label_size;
  
  // Search until we find the complement or exit the memory.
  while (pos >= 0) {
    // If we are within a label, rewind to the beginning of it and see if
    // it has the proper sub-label that we're looking for.
    
    if (m_inst_set->IsNop( search_genome[pos] )) {
      // Find the start and end of the label we're in the middle of.
      
      int start_pos = pos;
      int end_pos = pos + 1;
      while (start_pos > 0 && m_inst_set->IsNop(search_genome[start_pos - 1])) {
        start_pos--;
      }
      while (end_pos < search_start &&
             m_inst_set->IsNop(search_genome[end_pos])) {
        end_pos++;
      }
      int test_size = end_pos - start_pos;
      
      // See if this label has the proper sub-label within it.
      int max_offset = test_size - label_size + 1;
      for (int offset = start_pos; offset < start_pos + max_offset; offset++) {
        
        // Test the number of matches for this offset.
        int matches;
        for (matches = 0; matches < label_size; matches++) {
          if (search_label[matches] !=
              m_inst_set->GetNopMod(search_genome[offset + matches])) {
            break;
          }
        }
        
        // If we have found it, break out of this loop!
        if (matches == label_size) {
          found_label = true;
          break;
        }
      }
      
      // If we've found the complement label, set the position to the end of
      // the label we found it in, and break out.
      
      if (found_label == true) {
        pos = end_pos;
        break;
      }
      
      // We haven't found it; jump pos to just before the current label
      // being checked.
      pos = start_pos - 1;
    }
    
    // Jump up a block to the next possible point to find a label,
    pos -= label_size;
  }
  
  // If the label was not found return a -1.
  if (found_label == false) pos = -1;
  
  return pos;
}

// Search for 'in_label' anywhere in the hardware.
cHeadCPU cHardwareCPU::FindLabel(const cCodeLabel & in_label, int direction)
{
  assert (in_label.GetSize() > 0);
  
  // IDEALY:
  // Keep making jumps (in the proper direction) equal to the label
  // length.  If we are inside of a label, check its size, and see if
  // any of the sub-labels match properly.
  // FOR NOW:
  // Get something which works, no matter how inefficient!!!
  
  cHeadCPU temp_head(this);
  
  while (temp_head.InMemory()) {
    // IDEALY: Analyze the label we are in; see if the one we are looking
    // for could be a sub-label of it.  Skip past it if not.
    
    int i;
    for (i = 0; i < in_label.GetSize(); i++) {
      if (!m_inst_set->IsNop(temp_head.GetInst()) ||
          in_label[i] != m_inst_set->GetNopMod(temp_head.GetInst())) {
        break;
      }
    }
    if (i == GetLabel().GetSize()) {
      temp_head.AbsJump(i - 1);
      return temp_head;
    }
    
    temp_head.AbsJump(direction);     // IDEALY: MAKE LARGER JUMPS
  }
  
  temp_head.AbsSet(-1);
  return temp_head;
}


bool cHardwareCPU::InjectHost(const cCodeLabel & in_label, const cGenome & injection)
{
  // Make sure the genome will be below max size after injection.
  
  const int new_size = injection.GetSize() + m_memory.GetSize();
  if (new_size > MAX_CREATURE_SIZE) return false; // (inject fails)
  
  const int inject_line = FindLabelFull(in_label).GetPosition();
  
  // Abort if no compliment is found.
  if (inject_line == -1) return false; // (inject fails)
  
  // Inject the code!
  InjectCode(injection, inject_line+1);
  
  return true; // (inject succeeds!)
}

void cHardwareCPU::InjectCode(const cGenome & inject_code, const int line_num)
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
  m_organism->GetPhenotype().IsModified() = true;
  
  // Adjust all of the heads to take into account the new mem size.  
  for (int i = 0; i < NUM_HEADS; i++) {    
    if (GetHead(i).GetPosition() > line_num) GetHead(i).Jump(inject_size);
  }
}


void cHardwareCPU::ReadInst(const int in_inst)
{
  if (m_inst_set->IsNop( cInstruction(in_inst) )) {
    GetReadLabel().AddNop(in_inst);
  } else {
    GetReadLabel().Clear();
  }
}


void cHardwareCPU::AdjustHeads()
{
  for (int i = 0; i < m_threads.GetSize(); i++) {
    for (int j = 0; j < NUM_HEADS; j++) {
      m_threads[i].heads[j].Adjust();
    }
  }
}



// This function looks at the current position in the info of a creature,
// and sets the next_label to be the sequence of nops which follows.  The
// instruction pointer is left on the last line of the label found.

void cHardwareCPU::ReadLabel(int max_size)
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


bool cHardwareCPU::ForkThread()
{
  const int num_threads = m_threads.GetSize();
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


bool cHardwareCPU::KillThread()
{
  // Make sure that there is always at least one thread...
  if (m_threads.GetSize() == 1) return false;
  
  // Note the current thread and set the current back one.
  const int kill_thread = m_cur_thread;
  ThreadPrev();
  
  // Turn off this bit in the m_thread_id_chart...
  m_thread_id_chart ^= 1 << m_threads[kill_thread].GetID();
  
  // Copy the last thread into the kill position
  const int last_thread = m_threads.GetSize() - 1;
  if (last_thread != kill_thread) {
    m_threads[kill_thread] = m_threads[last_thread];
  }
  
  // Kill the thread!
  m_threads.Resize(m_threads.GetSize() - 1);
  
  if (m_cur_thread > kill_thread) m_cur_thread--;
  
  return true;
}

////////////////////////////
//  Instruction Helpers...
////////////////////////////

inline int cHardwareCPU::FindModifiedRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_register = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  }
  return default_register;
}

inline int cHardwareCPU::FindModifiedNextRegister(int default_register)
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

inline int cHardwareCPU::FindModifiedPreviousRegister(int default_register)
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


inline int cHardwareCPU::FindModifiedHead(int default_head)
{
  assert(default_head < NUM_HEADS); // Head ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_head = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  }
  return default_head;
}


inline int cHardwareCPU::FindNextRegister(int base_reg)
{
  return (base_reg + 1) % NUM_REGISTERS;
}


bool cHardwareCPU::Allocate_Necro(const int new_size)
{
  m_memory.ResizeOld(new_size);
  return true;
}

bool cHardwareCPU::Allocate_Random(cAvidaContext& ctx, const int old_size, const int new_size)
{
  m_memory.Resize(new_size);
	
  for (int i = old_size; i < new_size; i++) {
    m_memory.SetInst(i, m_inst_set->GetRandomInst(ctx), false);
  }
  return true;
}

bool cHardwareCPU::Allocate_Default(const int new_size)
{
  m_memory.Resize(new_size);
  
  // New space already defaults to default instruction...
  
  return true;
}

bool cHardwareCPU::Allocate_Main(cAvidaContext& ctx, const int allocated_size)
{
  // must do divide before second allocate & must allocate positive amount...
  if (m_world->GetConfig().REQUIRE_ALLOCATE.Get() && m_mal_active == true) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR, "Allocate already active");
    return false;
  }
  if (allocated_size < 1) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
										cStringUtil::Stringf("Allocate of %d too small", allocated_size));
    return false;
  }
  
  const int old_size = m_memory.GetSize();
  const int new_size = old_size + allocated_size;
  
  // Make sure that the new size is in range.
  if (new_size > MAX_CREATURE_SIZE  ||  new_size < MIN_CREATURE_SIZE) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
										cStringUtil::Stringf("Invalid post-allocate size (%d)",
																				 new_size));
    return false;
  }
  
  const int max_alloc_size = (int) (old_size * m_world->GetConfig().CHILD_SIZE_RANGE.Get());
  if (allocated_size > max_alloc_size) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
										cStringUtil::Stringf("Allocate too large (%d > %d)",
																				 allocated_size, max_alloc_size));
    return false;
  }
  
  const int max_old_size =
	(int) (allocated_size * m_world->GetConfig().CHILD_SIZE_RANGE.Get());
  if (old_size > max_old_size) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
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

int cHardwareCPU::calcCopiedSize(const int parent_size, const int child_size)
{
  int copied_size = 0;
  for (int i = parent_size; i < parent_size + child_size; i++) {
    if (m_memory.FlagCopied(i)) copied_size++;
  }
  return copied_size;
}  


bool cHardwareCPU::Divide_Main(cAvidaContext& ctx, const int div_point,
                               const int extra_lines, double mut_multiplier)
{
  const int child_size = m_memory.GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
  
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  cGenome & child_genome = m_organism->ChildGenome();
  child_genome = cGenomeUtil::Crop(m_memory, div_point, div_point+child_size);
  
  // Make sure it is an exact copy at this point (before divide mutations) if required
  if (m_world->GetConfig().REQUIRE_EXACT_COPY.Get() && (m_organism->GetGenome() != child_genome) ) {
    return false;
  }
  
  // Cut off everything in this memory past the divide point.
  m_memory.Resize(div_point);
  
  // Handle Divide Mutations...
  Divide_DoMutations(ctx, mut_multiplier);
	
  // Many tests will require us to run the offspring through a test CPU;
  // this is, for example, to see if mutations need to be reverted or if
  // lineages need to be updated.
  Divide_TestFitnessMeasures1(ctx); 
  
#if INSTRUCTION_COSTS
  // reset first time instruction costs
  for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
    m_inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
  }
#endif
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child
  bool parent_alive = m_organism->ActivateDivide(ctx);
	
  // Do more work if the parent lives through the birth of the offspring
  if (parent_alive) {
		
    if ( (m_world->GetConfig().EPIGENETIC_METHOD.Get() == EPIGENETIC_METHOD_PARENT) 
				|| (m_world->GetConfig().EPIGENETIC_METHOD.Get() == EPIGENETIC_METHOD_BOTH) ) {
      InheritState(*this);  
    }
		
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  
  return true;
}

/*
 Almost the same as Divide_Main, but resamples reverted offspring.
 
 RESAMPLING ONLY WORKS CORRECTLY WHEN ALL MUTIONS OCCUR ON DIVIDE!!
 
 AWC - 06/29/06
 */
bool cHardwareCPU::Divide_MainRS(cAvidaContext& ctx, const int div_point,
																 const int extra_lines, double mut_multiplier)
{
	
  //cStats stats = m_world->GetStats();
  const int child_size = m_memory.GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
  
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  cGenome & child_genome = m_organism->ChildGenome();
  child_genome = cGenomeUtil::Crop(m_memory, div_point, div_point+child_size);
  
  // Cut off everything in this memory past the divide point.
  m_memory.Resize(div_point);
  
  unsigned 
	totalMutations = 0,
	mutations = 0;
	//RScount = 0;
	
	
  bool
	fitTest = false;
	
  // Handle Divide Mutations...
  /*
	 Do mutations until one of these conditions are satisified:
	 we have resampled X times
	 we have an offspring with the same number of muations as the first offspring
	 that is not reverted
	 the parent is steralized (usually means an implicit mutation)
	 */
  for(unsigned i = 0; i <= 100; i++){
    if(i == 0){
      mutations = totalMutations = Divide_DoMutations(ctx, mut_multiplier);
    }
    else{
      mutations = Divide_DoMutations(ctx, mut_multiplier);
      m_world->GetStats().IncResamplings();
    }
		
    fitTest = Divide_TestFitnessMeasures1(ctx);
    
    if(!fitTest && mutations >= totalMutations) break;
		
  } 
  // think about making this mutations == totalMuations - though this may be too hard...
  /*
	 if(RScount > 2)
	 cerr << "Resampled " << RScount << endl;
	 */
  //org could not be resampled beneath the hard cap -- it is then steraalized
  if(fitTest/*RScount == 11*/) {
    m_organism->GetPhenotype().ChildFertile() = false;
    m_world->GetStats().IncFailedResamplings();
  }
	
#if INSTRUCTION_COSTS
  // reset first time instruction costs
  for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
    m_inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
  }
#endif
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child, and do more work if the parent lives through the
  // birth.
  bool parent_alive = m_organism->ActivateDivide(ctx);
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  
  return true;
}

/*
 Almost the same as Divide_Main, but only allows for one mutation 
 on divde and resamples reverted offspring.
 
 RESAMPLING ONLY WORKS CORRECTLY WHEN ALL MUTIONS OCCUR ON DIVIDE!!
 
 AWC - 07/28/06
 */
bool cHardwareCPU::Divide_Main1RS(cAvidaContext& ctx, const int div_point,
																	const int extra_lines, double mut_multiplier)
{
	
  //cStats stats = m_world->GetStats();
  const int child_size = m_memory.GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
  
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  cGenome & child_genome = m_organism->ChildGenome();
  child_genome = cGenomeUtil::Crop(m_memory, div_point, div_point+child_size);
  
  // Cut off everything in this memory past the divide point.
  m_memory.Resize(div_point);
  
  unsigned 
	totalMutations = 0,
	mutations = 0;
  //    RScount = 0;
	
  bool
	fitTest = false;
	
	
  // Handle Divide Mutations...
  /*
	 Do mutations until one of these conditions are satisified:
	 we have resampled X times
	 we have an offspring with the same number of muations as the first offspring
	 that is not reverted
	 the parent is steralized (usually means an implicit mutation)
	 */
  for(unsigned i = 0; i < 100; i++){
    if(!i){
      mutations = totalMutations = Divide_DoMutations(ctx, mut_multiplier,1);
    }
    else{
      mutations = Divide_DoExactMutations(ctx, mut_multiplier,1);
      m_world->GetStats().IncResamplings();
    }
		
    fitTest = Divide_TestFitnessMeasures1(ctx);
    //if(mutations > 1 ) cerr << "Too Many mutations!!!!!!!!!!!!!!!" << endl;
    if(!fitTest && mutations >= totalMutations) break;
		
  } 
  // think about making this mutations == totalMuations - though this may be too hard...
  /*
	 if(RScount > 2)
	 cerr << "Resampled " << RScount << endl;
	 */
  //org could not be resampled beneath the hard cap -- it is then steraalized
  if(fitTest/*RScount == 11*/) {
    m_organism->GetPhenotype().ChildFertile() = false;
    m_world->GetStats().IncFailedResamplings();
  }
	
#if INSTRUCTION_COSTS
  // reset first time instruction costs
  for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
    m_inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
  }
#endif
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child, and do more work if the parent lives through the
  // birth.
  bool parent_alive = m_organism->ActivateDivide(ctx);
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  
  return true;
}

/*
 Almost the same as Divide_Main, but only allows for one mutation 
 on divde and resamples reverted offspring.
 
 RESAMPLING ONLY WORKS CORRECTLY WHEN ALL MUTIONS OCCUR ON DIVIDE!!
 
 AWC - 07/28/06
 */
bool cHardwareCPU::Divide_Main2RS(cAvidaContext& ctx, const int div_point,
																	const int extra_lines, double mut_multiplier)
{
	
  //cStats stats = m_world->GetStats();
  const int child_size = m_memory.GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
  
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  cGenome & child_genome = m_organism->ChildGenome();
  child_genome = cGenomeUtil::Crop(m_memory, div_point, div_point+child_size);
  
  // Cut off everything in this memory past the divide point.
  m_memory.Resize(div_point);
  
  unsigned 
	totalMutations = 0,
	mutations = 0;
  //    RScount = 0;
	
  bool
	fitTest = false;
	
	
  // Handle Divide Mutations...
  /*
	 Do mutations until one of these conditions are satisified:
	 we have resampled X times
	 we have an offspring with the same number of muations as the first offspring
	 that is not reverted
	 the parent is steralized (usually means an implicit mutation)
	 */
  for(unsigned i = 0; i < 100; i++){
    if(!i){
      mutations = totalMutations = Divide_DoMutations(ctx, mut_multiplier,2);
    }
    else{
      Divide_DoExactMutations(ctx, mut_multiplier,mutations);
      m_world->GetStats().IncResamplings();
    }
		
    fitTest = Divide_TestFitnessMeasures(ctx);
    //if(mutations > 1 ) cerr << "Too Many mutations!!!!!!!!!!!!!!!" << endl;
    if(!fitTest && mutations >= totalMutations) break;
		
  } 
  // think about making this mutations == totalMuations - though this may be too hard...
  /*
	 if(RScount > 2)
	 cerr << "Resampled " << RScount << endl;
	 */
  //org could not be resampled beneath the hard cap -- it is then steraalized
  if(fitTest/*RScount == 11*/) {
    m_organism->GetPhenotype().ChildFertile() = false;
    m_world->GetStats().IncFailedResamplings();
  }
	
#if INSTRUCTION_COSTS
  // reset first time instruction costs
  for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
    m_inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
  }
#endif
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child, and do more work if the parent lives through the
  // birth.
  bool parent_alive = m_organism->ActivateDivide(ctx);
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  
  return true;
}

// Sets the current state of the hardware and also saves this state so
//  that future Reset() calls will reset to that epigenetic state
void cHardwareCPU::InheritState(cHardwareBase& in_hardware)
{ 
  m_epigenetic_state = true;
  const cHardwareCPU& in_h = (cHardwareCPU&)in_hardware; 
  const cLocalThread& thread = in_h.GetThread(in_h.GetCurThread());
  for (int i=0; i<NUM_REGISTERS; i++) {
    m_epigenetic_saved_reg[i] = thread.reg[i];
    m_threads[m_cur_thread].reg[i] = m_epigenetic_saved_reg[i];
  }
  m_epigenetic_saved_stack = thread.stack;
  m_threads[m_cur_thread].stack = m_epigenetic_saved_stack;
}

//////////////////////////
// And the instructions...
//////////////////////////

bool cHardwareCPU::Inst_If0(cAvidaContext& ctx)          // Execute next if ?bx? ==0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) != 0)  IP().Advance();
  return true; 
}

bool cHardwareCPU::Inst_IfNot0(cAvidaContext& ctx)       // Execute next if ?bx? != 0.
{ 
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) == 0)  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfEqu(cAvidaContext& ctx)      // Execute next if bx == ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) != GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfNEqu(cAvidaContext& ctx)     // Execute next if bx != ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) == GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGr0(cAvidaContext& ctx)       // Execute next if ?bx? ! < 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) <= 0)  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGr(cAvidaContext& ctx)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) <= GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGrEqu0(cAvidaContext& ctx)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) < 0)  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGrEqu(cAvidaContext& ctx)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) < GetRegister(op2)) IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLess0(cAvidaContext& ctx)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) >= 0)  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLess(cAvidaContext& ctx)       // Execute next if ?bx? < ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) >=  GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLsEqu0(cAvidaContext& ctx)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) > 0) IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLsEqu(cAvidaContext& ctx)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) >  GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfBit1(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if ((GetRegister(reg_used) & 1) == 0)  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfANotEqB(cAvidaContext& ctx)     // Execute next if AX != BX
{
  if (GetRegister(REG_AX) == GetRegister(REG_BX) )  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfBNotEqC(cAvidaContext& ctx)     // Execute next if BX != CX
{
  if (GetRegister(REG_BX) == GetRegister(REG_CX) )  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfANotEqC(cAvidaContext& ctx)     // Execute next if AX != BX
{
  if (GetRegister(REG_AX) == GetRegister(REG_CX) )  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_JumpF(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  // If there is no label, jump BX steps.
  if (GetLabel().GetSize() == 0) {
    GetActiveHead().Jump(GetRegister(REG_BX));
    return true;
  }
  
  // Otherwise, try to jump to the complement label.
  const cHeadCPU jump_location(FindLabel(1));
  if ( jump_location.GetPosition() != -1 ) {
    GetActiveHead().Set(jump_location);
    return true;
  }
  
  // If complement label was not found; record an error.
  m_organism->Fault(FAULT_LOC_JUMP, FAULT_TYPE_ERROR,
                  "jump-f: No complement label");
  return false;
}


bool cHardwareCPU::Inst_JumpB(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  // If there is no label, jump BX steps.
  if (GetLabel().GetSize() == 0) {
    GetActiveHead().Jump(GetRegister(REG_BX));
    return true;
  }
  
  // otherwise jump to the complement label.
  const cHeadCPU jump_location(FindLabel(-1));
  if ( jump_location.GetPosition() != -1 ) {
    GetActiveHead().Set(jump_location);
    return true;
  }
  
  // If complement label was not found; record an error.
  m_organism->Fault(FAULT_LOC_JUMP, FAULT_TYPE_ERROR,
                  "jump-b: No complement label");
  return false;
}

bool cHardwareCPU::Inst_Call(cAvidaContext& ctx)
{
  // Put the starting location onto the stack
  const int location = IP().GetPosition();
  StackPush(location);
  
  // Jump to the compliment label (or by the ammount in the bx register)
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  if (GetLabel().GetSize() == 0) {
    IP().Jump(GetRegister(REG_BX));
    return true;
  }
  
  const cHeadCPU jump_location(FindLabel(1));
  if (jump_location.GetPosition() != -1) {
    IP().Set(jump_location);
    return true;
  }
  
  // If complement label was not found; record an error.
  m_organism->Fault(FAULT_LOC_JUMP, FAULT_TYPE_ERROR,
                  "call: no complement label");
  return false;
}

bool cHardwareCPU::Inst_Return(cAvidaContext& ctx)
{
  IP().Set(StackPop());
  return true;
}

bool cHardwareCPU::Inst_Throw(cAvidaContext& ctx)
{
  // Only initialize this once to save some time...
  static cInstruction catch_inst = GetInstSet().GetInst(cStringUtil::Stringf("catch"));
	
  //Look for the label directly (no complement)
  ReadLabel();
	
  cHeadCPU search_head(IP());
  int start_pos = search_head.GetPosition();
  search_head++;
  
  while (start_pos != search_head.GetPosition()) 
  {
    // If we find a catch instruction, compare the NOPs following it
    if (search_head.GetInst() == catch_inst)
    {
      int catch_pos = search_head.GetPosition();
      search_head++;
			
      // Continue to examine the label after the catch
      //  (1) It ends (=> use the catch!)
      //  (2) It becomes longer than the throw label (=> use the catch!)
      //  (3) We find a NOP that doesnt match the throw (=> DON'T use the catch...)
      
      bool match = true;
      int size_matched = 0;      
      while ( match && m_inst_set->IsNop(search_head.GetInst()) && (size_matched < GetLabel().GetSize()) )
      {
        if ( GetLabel()[size_matched] != m_inst_set->GetNopMod( search_head.GetInst()) ) match = false;
        search_head++;
        size_matched++;
      }
      
      // We found a matching catch instruction
      if (match)
      {
        IP().Set(catch_pos);
        m_advance_ip = false; // Don't automatically move the IP
				// so we mark the catch as executed.
        return true;
      }
      
      //If we advanced past NOPs during testing, retreat
      if ( !m_inst_set->IsNop(search_head.GetInst()) ) search_head--;
    }
    search_head.Advance();
  }
	
  return false;
}


bool cHardwareCPU::Inst_ThrowIfNot0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) == 0) return false;
  return Inst_Throw(ctx);
}

bool cHardwareCPU::Inst_ThrowIf0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) != 0) return false;
  return Inst_Throw(ctx);
}

bool cHardwareCPU::Inst_Goto(cAvidaContext& ctx)
{
  // Only initialize this once to save some time...
  static cInstruction label_inst = GetInstSet().GetInst(cStringUtil::Stringf("label"));
	
  //Look for an EXACT label match after a 'label' instruction
  ReadLabel();
  
  cHeadCPU search_head(IP());
  int start_pos = search_head.GetPosition();
  search_head++;
  
  while (start_pos != search_head.GetPosition()) 
  {
    if (search_head.GetInst() == label_inst)
    {
      int label_pos = search_head.GetPosition();
      search_head++;
      int size_matched = 0;
      while ( size_matched < GetLabel().GetSize() )
      {
        if ( !m_inst_set->IsNop(search_head.GetInst()) ) break;
        if ( GetLabel()[size_matched] != m_inst_set->GetNopMod( search_head.GetInst()) ) break;
        if ( !m_inst_set->IsNop(search_head.GetInst()) ) break;
				
        size_matched++;
        search_head++;
      }
      
      // We found a matching 'label' instruction only if the next 
      // instruction (at the search head now) is also not a NOP
      if ( (size_matched == GetLabel().GetSize()) && !m_inst_set->IsNop(search_head.GetInst()) )
      {
        IP().Set(label_pos);
        m_advance_ip = false; // Don't automatically move the IP
				// so we mark the catch as executed.
        return true;
      }
			
      //If we advanced past NOPs during testing, retreat
      if ( !m_inst_set->IsNop(search_head.GetInst()) ) search_head--;
    }
    search_head++;
  }
	
  return false;
}


bool cHardwareCPU::Inst_GotoIfNot0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) == 0) return false;
  return Inst_Goto(ctx);
}

bool cHardwareCPU::Inst_GotoIf0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) != 0) return false;
  return Inst_Goto(ctx);
}


bool cHardwareCPU::Inst_Pop(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = StackPop();
  return true;
}

bool cHardwareCPU::Inst_Push(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  StackPush(GetRegister(reg_used));
  return true;
}

bool cHardwareCPU::Inst_HeadPop(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  GetHead(head_used).Set(StackPop());
  return true;
}

bool cHardwareCPU::Inst_HeadPush(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  StackPush(GetHead(head_used).GetPosition());
  if (head_used == nHardware::HEAD_IP) {
    GetHead(head_used).Set(GetHead(nHardware::HEAD_FLOW));
    m_advance_ip = false;
  }
  return true;
}


bool cHardwareCPU::Inst_PopA(cAvidaContext& ctx) { GetRegister(REG_AX) = StackPop(); return true;}
bool cHardwareCPU::Inst_PopB(cAvidaContext& ctx) { GetRegister(REG_BX) = StackPop(); return true;}
bool cHardwareCPU::Inst_PopC(cAvidaContext& ctx) { GetRegister(REG_CX) = StackPop(); return true;}

bool cHardwareCPU::Inst_PushA(cAvidaContext& ctx) { StackPush(GetRegister(REG_AX)); return true;}
bool cHardwareCPU::Inst_PushB(cAvidaContext& ctx) { StackPush(GetRegister(REG_BX)); return true;}
bool cHardwareCPU::Inst_PushC(cAvidaContext& ctx) { StackPush(GetRegister(REG_CX)); return true;}

bool cHardwareCPU::Inst_SwitchStack(cAvidaContext& ctx) { SwitchStack(); return true;}
bool cHardwareCPU::Inst_FlipStack(cAvidaContext& ctx)   { StackFlip(); return true;}

bool cHardwareCPU::Inst_Swap(cAvidaContext& ctx)
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  nFunctions::Swap(GetRegister(op1), GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_SwapAB(cAvidaContext& ctx)\
{
  nFunctions::Swap(GetRegister(REG_AX), GetRegister(REG_BX)); return true;
}
bool cHardwareCPU::Inst_SwapBC(cAvidaContext& ctx)
{
  nFunctions::Swap(GetRegister(REG_BX), GetRegister(REG_CX)); return true;
}
bool cHardwareCPU::Inst_SwapAC(cAvidaContext& ctx)
{
  nFunctions::Swap(GetRegister(REG_AX), GetRegister(REG_CX)); return true;
}

bool cHardwareCPU::Inst_CopyReg(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = FindNextRegister(src);
  GetRegister(dst) = GetRegister(src);
  return true;
}

bool cHardwareCPU::Inst_CopyRegAB(cAvidaContext& ctx)
{
  GetRegister(REG_AX) = GetRegister(REG_BX);   return true;
}
bool cHardwareCPU::Inst_CopyRegAC(cAvidaContext& ctx)
{
  GetRegister(REG_AX) = GetRegister(REG_CX);   return true;
}
bool cHardwareCPU::Inst_CopyRegBA(cAvidaContext& ctx)
{
  GetRegister(REG_BX) = GetRegister(REG_AX);   return true;
}
bool cHardwareCPU::Inst_CopyRegBC(cAvidaContext& ctx)
{
  GetRegister(REG_BX) = GetRegister(REG_CX);   return true;
}
bool cHardwareCPU::Inst_CopyRegCA(cAvidaContext& ctx)
{
  GetRegister(REG_CX) = GetRegister(REG_AX);   return true;
}
bool cHardwareCPU::Inst_CopyRegCB(cAvidaContext& ctx)
{
  GetRegister(REG_CX) = GetRegister(REG_BX);   return true;
}

bool cHardwareCPU::Inst_Reset(cAvidaContext& ctx)
{
  GetRegister(REG_AX) = 0;
  GetRegister(REG_BX) = 0;
  GetRegister(REG_CX) = 0;
  StackClear();
	m_last_cell_data = std::make_pair(false, 0);
  return true;
}

/*bool cHardwareCPU::Inst_LoadVal(cAvidaContext& ctx)
{
	const int reg_used = FindModifiedRegister(REG_BX);
	GetRegister(reg_used) = m_world->GetConfig().INST_LOAD_VALUE.Get();
	return true;
}*/

bool cHardwareCPU::Inst_ShiftR(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) >>= 1;
  return true;
}

bool cHardwareCPU::Inst_ShiftL(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) <<= 1;
  return true;
}

bool cHardwareCPU::Inst_Bit1(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) |=  1;
  return true;
}

bool cHardwareCPU::Inst_SetNum(cAvidaContext& ctx)
{
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsInt(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValGrey(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntGreyCode(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValDir(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntDirect(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValAddP(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntAdditivePolynomial(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValFib(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntFib(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValPolyC(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntPolynomialCoefficent(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_Inc(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) += 1;
  return true;
}

bool cHardwareCPU::Inst_Dec(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) -= 1;
  return true;
}

bool cHardwareCPU::Inst_Zero(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_Neg(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = -GetRegister(src);
  return true;
}

bool cHardwareCPU::Inst_Square(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = GetRegister(src) * GetRegister(src);
  return true;
}

bool cHardwareCPU::Inst_Sqrt(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value > 1) GetRegister(dst) = static_cast<int>(sqrt(static_cast<double>(value)));
  else if (value < 0) {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "sqrt: value is negative");
    return false;
  }
  return true;
}

bool cHardwareCPU::Inst_Log(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value >= 1) GetRegister(dst) = static_cast<int>(log(static_cast<double>(value)));
  else if (value < 0) {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "log: value is negative");
    return false;
  }
  return true;
}

bool cHardwareCPU::Inst_Log10(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value >= 1) GetRegister(dst) = static_cast<int>(log10(static_cast<double>(value)));
  else if (value < 0) {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "log10: value is negative");
    return false;
  }
  return true;
}

bool cHardwareCPU::Inst_Add(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) + GetRegister(op2);
  return true;
}

bool cHardwareCPU::Inst_Sub(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) - GetRegister(op2);
  return true;
}

bool cHardwareCPU::Inst_Mult(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) * GetRegister(op2);
  return true;
}

bool cHardwareCPU::Inst_Div(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op2) != 0) {
    if (0-INT_MAX > GetRegister(op1) && GetRegister(op2) == -1)
      m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "div: Float exception");
    else
      GetRegister(dst) = GetRegister(op1) / GetRegister(op2);
  } else {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "div: dividing by 0");
    return false;
  }
  return true;
}

bool cHardwareCPU::Inst_Mod(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op2) != 0) {
    GetRegister(dst) = GetRegister(op1) % GetRegister(op2);
  } else {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "mod: modding by 0");
    return false;
  }
  return true;
}


bool cHardwareCPU::Inst_Nand(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = ~(GetRegister(op1) & GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_Nor(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = ~(GetRegister(op1) | GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_And(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = (GetRegister(op1) & GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_Not(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = ~(GetRegister(src));
  return true;
}

bool cHardwareCPU::Inst_Order(cAvidaContext& ctx)
{
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op1) > GetRegister(op2)) {
    nFunctions::Swap(GetRegister(op1), GetRegister(op2));
  }
  return true;
}

bool cHardwareCPU::Inst_Xor(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) ^ GetRegister(op2);
  return true;
}

bool cHardwareCPU::Inst_Copy(cAvidaContext& ctx)
{
  const int op1 = REG_BX;
  const int op2 = REG_AX;
	
  const cHeadCPU from(this, GetRegister(op1));
  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  
  if (m_organism->TestCopyMut(ctx)) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();  // Mark this instruction as mutated...
    to.SetFlagCopyMut();  // Mark this instruction as copy mut...
  } else {
    to.SetInst(from.GetInst());
    to.ClearFlagMutated();  // UnMark
    to.ClearFlagCopyMut();  // UnMark
  }
  
  to.SetFlagCopied();  // Set the copied flag.
	//  cpu_stats.mut_stats.copies_exec++;
  return true;
}

bool cHardwareCPU::Inst_ReadInst(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_CX);
  const int src = REG_BX;
	
  const cHeadCPU from(this, GetRegister(src));
  
  // Dis-allowing mutations on read, for the moment (write only...)
  // @CAO This allows perfect error-correction...
  GetRegister(dst) = from.GetInst().GetOp();
  return true;
}

bool cHardwareCPU::Inst_WriteInst(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_CX);
  const int op1 = REG_BX;
  const int op2 = REG_AX;
	
  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  const int value = Mod(GetRegister(src), m_inst_set->GetSize());
  // Change value on a mutation...
  if (m_organism->TestCopyMut(ctx)) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
  } else {
    to.SetInst(cInstruction(value));
    to.ClearFlagMutated();     // UnMark
    to.ClearFlagCopyMut();     // UnMark
  }
	
  to.SetFlagCopied();  // Set the copied flag.
  return true;
}

bool cHardwareCPU::Inst_StackReadInst(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  cHeadCPU from(this, GetRegister(reg_used));
  StackPush(from.GetInst().GetOp());
  return true;
}

bool cHardwareCPU::Inst_StackWriteInst(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_AX;
  cHeadCPU to(this, GetRegister(op1) + GetRegister(dst));
  const int value = Mod(StackPop(), m_inst_set->GetSize());
  
  // Change value on a mutation...
  if (m_organism->TestCopyMut(ctx)) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
  } else {
    to.SetInst(cInstruction(value));
    to.ClearFlagMutated();     // UnMark
    to.ClearFlagCopyMut();     // UnMark
  }
  
  to.SetFlagCopied();  // Set the copied flag.
  return true;
}

bool cHardwareCPU::Inst_Compare(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_CX);
  const int op1 = REG_BX;
  const int op2 = REG_AX;
	
  cHeadCPU from(this, GetRegister(op1));
  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  
  // Compare is dangerous -- it can cause mutations!
  if (m_organism->TestCopyMut(ctx)) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
  }
  
  GetRegister(dst) = from.GetInst().GetOp() - to.GetInst().GetOp();
  
  return true;
}

bool cHardwareCPU::Inst_IfNCpy(cAvidaContext& ctx)
{
  const int op1 = REG_BX;
  const int op2 = REG_AX;
	
  const cHeadCPU from(this, GetRegister(op1));
  const cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  
  // Allow for errors in this test...
  if (m_organism->TestCopyMut(ctx)) {
    if (from.GetInst() != to.GetInst()) IP().Advance();
  } else {
    if (from.GetInst() == to.GetInst()) IP().Advance();
  }
  return true;
}

bool cHardwareCPU::Inst_Allocate(cAvidaContext& ctx)   // Allocate bx more space...
{
  const int src = REG_BX;
  const int dst = REG_AX;
  const int size = m_memory.GetSize();
  if (Allocate_Main(ctx, GetRegister(src))) {
    GetRegister(dst) = size;
    return true;
  } else return false;
}

bool cHardwareCPU::Inst_Divide(cAvidaContext& ctx)  
{ 
  const int src = REG_AX;
  return Divide_Main(ctx, GetRegister(src));    
}

/*
 Divide with resampling -- Same as regular divide but on reversions will be 
 resampled after they are reverted.
 
 AWC 06/29/06
 
 */

bool cHardwareCPU::Inst_DivideRS(cAvidaContext& ctx)  
{ 
  const int src = REG_AX;
  return Divide_MainRS(ctx, GetRegister(src));    
}


bool cHardwareCPU::Inst_CDivide(cAvidaContext& ctx) 
{ 
  return Divide_Main(ctx, m_memory.GetSize() / 2);   
}

bool cHardwareCPU::Inst_CAlloc(cAvidaContext& ctx)  
{ 
  return Allocate_Main(ctx, m_memory.GetSize());   
}

bool cHardwareCPU::Inst_MaxAlloc(cAvidaContext& ctx)   // Allocate maximal more
{
  const int dst = REG_AX;
  const int cur_size = m_memory.GetSize();
  const int alloc_size = Min((int) (m_world->GetConfig().CHILD_SIZE_RANGE.Get() * cur_size),
                             MAX_CREATURE_SIZE - cur_size);
  if (Allocate_Main(ctx, alloc_size)) {
    GetRegister(dst) = cur_size;
    return true;
  } else return false;
}

// Alloc and move write head if we're successful
bool cHardwareCPU::Inst_MaxAllocMoveWriteHead(cAvidaContext& ctx)   // Allocate maximal more
{
  const int dst = REG_AX;
  const int cur_size = m_memory.GetSize();
  const int alloc_size = Min((int) (m_world->GetConfig().CHILD_SIZE_RANGE.Get() * cur_size),
                             MAX_CREATURE_SIZE - cur_size);
  if (Allocate_Main(ctx, alloc_size)) {
    GetRegister(dst) = cur_size;
    GetHead(nHardware::HEAD_WRITE).Set(cur_size);
    return true;
  } else return false;
}

bool cHardwareCPU::Inst_Transposon(cAvidaContext& ctx)
{
  ReadLabel();
  return true;
}

void cHardwareCPU::Divide_DoTransposons(cAvidaContext& ctx)
{
  // This only works if 'transposon' is in the current instruction set
  static bool transposon_in_use = GetInstSet().InstInSet(cStringUtil::Stringf("transposon"));
  if (!transposon_in_use) return;
  
  static cInstruction transposon_inst = GetInstSet().GetInst(cStringUtil::Stringf("transposon"));
  cCPUMemory& child_genome = m_organism->ChildGenome();
	
  // Count the number of transposons that are marked as executed
  int tr_count = 0;
  for (int i=0; i < child_genome.GetSize(); i++) 
  {
    if (child_genome.FlagExecuted(i) && (child_genome[i] == transposon_inst)) tr_count++;
  }
  
  for (int i=0; i < tr_count; i++) 
  {
    if (ctx.GetRandom().P(0.01))
    {
      const unsigned int mut_line = ctx.GetRandom().GetUInt(child_genome.GetSize() + 1);
      child_genome.Insert(mut_line, transposon_inst);
    }
  }

}


// Inst_ReproDeme replicates a deme using cPopulation::ReplicateDeme.
// It is similar to Inst_SpawnDeme, but the implementation of Inst_SpawnDeme doesn't contain many new deme-level replication features
bool cHardwareCPU::Inst_ReproDeme(cAvidaContext& ctx) {
	
	cDeme* sourceDeme = m_organism->GetOrgInterface().GetDeme();
	if(sourceDeme == NULL)
		return false; // in test CPU
	
	// this function will become to depend on a predicate, but I am still thinking of how to do this (BEB)
	sourceDeme->ReplicateDeme();
	return true;
}

bool cHardwareCPU::Inst_Repro(cAvidaContext& ctx)
{ 
  // check if repro can replace an existing organism
  if(m_world->GetConfig().REPRO_METHOD.Get() == 0 && m_organism->IsNeighborCellOccupied())
    return false;
	
  if (m_organism->GetPhenotype().GetCurBonus() < m_world->GetConfig().REQUIRED_BONUS.Get()) return false;
  
  // Setup child
  cCPUMemory& child_genome = m_organism->ChildGenome();
  child_genome = m_organism->GetGenome();
	
  // Do transposon movement and copying before other mutations
  Divide_DoTransposons(ctx);
  
  // Perform Copy Mutations...
  if (m_organism->GetCopyMutProb() > 0) { // Skip this if no mutations....
    for (int i = 0; i < m_memory.GetSize(); i++) {
      if (m_organism->TestCopyMut(ctx)) {
        child_genome.SetInst(i, m_inst_set->GetRandomInst(ctx), false);
      }
    }
  }
  
  Divide_DoMutations(ctx);
  
  // Check viability
  bool viable = Divide_CheckViable(ctx, m_organism->GetGenome().GetSize(), m_organism->ChildGenome().GetSize(), 1);
  if (!viable) { return false; }
  
  // Many tests will require us to run the offspring through a test CPU;
  // this is, for example, to see if mutations need to be reverted or if
  // lineages need to be updated.
  Divide_TestFitnessMeasures(ctx);
  
#if INSTRUCTION_COSTS
  // reset first time instruction costs
  for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
    m_inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
  }
#endif
  
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) m_advance_ip = false;
  
  const bool parent_alive = m_organism->ActivateDivide(ctx);
  
  //Reset the parent
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  return true;
}

bool cHardwareCPU::Inst_ReproSex(cAvidaContext& ctx)
{
  m_organism->GetPhenotype().SetDivideSex(true);
  m_organism->GetPhenotype().SetCrossNum(1);
  return Inst_Repro(ctx);
}

bool cHardwareCPU::Inst_TaskPutRepro(cAvidaContext& ctx)
{
  // Do normal IO, but don't zero register
  //Inst_TaskPut(ctx);
  
  const int reg_used = FindModifiedRegister(REG_BX);
  const int value = GetRegister(reg_used);
	// GetRegister(reg_used) = 0;
  m_organism->DoOutput(ctx, value);
  
  // Immediately attempt a repro
  return Inst_Repro(ctx);
}

bool cHardwareCPU::Inst_TaskPutResetInputsRepro(cAvidaContext& ctx)
{
  // Do normal IO
  bool return_value = Inst_TaskPutResetInputs(ctx);
  
  // Immediately attempt a repro
  Inst_Repro(ctx);
	
  // return value of put since successful repro would wipe state anyway
  return return_value; 
}

bool cHardwareCPU::Inst_SpawnDeme(cAvidaContext& ctx)
{
  m_organism->SpawnDeme();
  return true;
}

bool cHardwareCPU::Inst_Kazi(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_AX);
  double percentProb = ((double) (GetRegister(reg_used) % 100)) / 100.0;
  if ( ctx.GetRandom().P(percentProb) ) m_organism->Kaboom(0);
  return true;
}


bool cHardwareCPU::Inst_Sterilize(cAvidaContext& ctx)
{
  m_organism->GetPhenotype().IsFertile() = false;
  return true;
}

bool cHardwareCPU::Inst_Kazi5(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_AX);
  double percentProb = ((double) (GetRegister(reg_used) % 100)) / 100.0;
  if ( ctx.GetRandom().P(percentProb) ) m_organism->Kaboom(5);
  return true;
}

bool cHardwareCPU::Inst_Die(cAvidaContext& ctx)
{
  m_organism->Die();
  return true;
}

/* Similar to Kazi, this instructon probabilistically causes
 the organism to die. However, in this case it does so in 
 order to win points for its deme and it does not take out
 any other organims. */
bool  cHardwareCPU::Inst_Suicide(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_AX);
  double percentProb = ((double) (GetRegister(reg_used) % 100)) / 100.0;
	if (m_organism->GetDeme() == NULL)
		return false; // in test CPU
  if ( ctx.GetRandom().P(percentProb) ) {
		// Add points as determined by config file to the deme.
		m_organism->GetDeme()->AddNumberOfPoints(m_world->GetConfig().DEMES_PROTECTION_POINTS.Get());
		m_organism->GetDeme()->AddSuicide();
		m_organism->Die();
	}
	return true;
}

bool cHardwareCPU::Inst_RelinquishEnergyToFutureDeme(cAvidaContext& ctx)
{
  double stored_energy = m_organism->GetPhenotype().GetStoredEnergy() * m_world->GetConfig().FRAC_ENERGY_RELINQUISH.Get();
  // put stored energy into testament pool for offspring deme
  cDeme* deme = m_organism->GetOrgInterface().GetDeme();
  if(deme == NULL)
    return false;  // in test CPU
  deme->IncreaseTotalEnergyTestament(stored_energy);
  m_world->GetStats().SumEnergyTestamentToFutureDeme().Add(stored_energy);
  m_organism->Die();
  return true;
}

bool cHardwareCPU::Inst_RelinquishEnergyToNeighborOrganisms(cAvidaContext& ctx)
{
  double stored_energy = m_organism->GetPhenotype().GetStoredEnergy() * m_world->GetConfig().FRAC_ENERGY_RELINQUISH.Get();
  // put stored energy into toBeApplied energy pool of neighbor organisms
  int numOcuppiedNeighbors(0);
  int orginalFacing = m_organism->GetFacing();
  for(int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if(m_organism->IsNeighborCellOccupied()) {
      // count neighboring organisms
      numOcuppiedNeighbors++;
    }
    m_organism->Rotate(1);
  }  
  assert(m_organism->GetFacing() == orginalFacing);
	
  for(int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if(m_organism->IsNeighborCellOccupied()) {
      // give energy testament to neighboring organisms
      m_organism->GetNeighbor()->GetPhenotype().EnergyTestament(stored_energy/numOcuppiedNeighbors);
    }
    m_organism->Rotate(1);
  }
  assert(m_organism->GetFacing() == orginalFacing);
  m_world->GetStats().SumEnergyTestamentToNeighborOrganisms().Add(stored_energy);
  m_organism->Die();
  return true;
}

bool cHardwareCPU::Inst_RelinquishEnergyToOrganismsInDeme(cAvidaContext& ctx) {
  double stored_energy = m_organism->GetPhenotype().GetStoredEnergy() * m_world->GetConfig().FRAC_ENERGY_RELINQUISH.Get();
  // put stored energy into toBeApplied energy pool of neighbor organisms
	
  m_organism->DivideOrgTestamentAmongDeme(stored_energy);
  m_world->GetStats().SumEnergyTestamentToDemeOrganisms().Add(stored_energy);
  m_organism->Die();
  return true;
}

// The inject instruction can be used instead of a divide command, paired
// with an allocate.  Note that for an inject to work, one needs to have a
// broad range for sizes allowed to be allocated.
//
// This command will cut out from read-head to write-head.
// It will then look at the template that follows the command and inject it
// into the complement template found in a neighboring organism.

bool cHardwareCPU::Inst_Inject(cAvidaContext& ctx)
{
  AdjustHeads();
  const int start_pos = GetHead(nHardware::HEAD_READ).GetPosition();
  const int end_pos = GetHead(nHardware::HEAD_WRITE).GetPosition();
  const int inject_size = end_pos - start_pos;
  
  // Make sure the creature will still be above the minimum size,
  if (inject_size <= 0) {
    m_organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: no code to inject");
    return false; // (inject fails)
  }
  if (start_pos < MIN_CREATURE_SIZE) {
    m_organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: new size too small");
    return false; // (inject fails)
  }
  
  // Since its legal to cut out the injected piece, do so.
  cGenome inject_code( cGenomeUtil::Crop(m_memory, start_pos, end_pos) );
  m_memory.Remove(start_pos, inject_size);
  
  // If we don't have a host, stop here.
  cOrganism * host_organism = m_organism->GetNeighbor();
  if (host_organism == NULL) return false;
  
  // Scan for the label to match...
  ReadLabel();
  
  // If there is no label, abort.
  if (GetLabel().GetSize() == 0) {
    m_organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: label required");
    return false; // (inject fails)
  }
  
  // Search for the label in the host...
  GetLabel().Rotate(1, NUM_NOPS);
  
  const bool inject_signal = host_organism->GetHardware().InjectHost(GetLabel(), inject_code);
  if (inject_signal) {
    m_organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_WARNING, "inject: host too large.");
    return false; // Inject failed.
  }
  
  // Set the relevent flags.
  m_organism->GetPhenotype().IsModifier() = true;
  
  return inject_signal;
}


bool cHardwareCPU::Inst_InjectRand(cAvidaContext& ctx)
{
  // Rotate to a random facing and then run the normal inject instruction
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  m_organism->Rotate(ctx.GetRandom().GetUInt(num_neighbors));
  Inst_Inject(ctx);
  return true;
}

// The inject instruction can be used instead of a divide command, paired
// with an allocate.  Note that for an inject to work, one needs to have a
// broad range for sizes allowed to be allocated.
//
// This command will cut out from read-head to write-head.
// It will then look at the template that follows the command and inject it
// into the complement template found in a neighboring organism.

bool cHardwareCPU::Inst_InjectThread(cAvidaContext& ctx)
{
  AdjustHeads();
  const int start_pos = GetHead(nHardware::HEAD_READ).GetPosition();
  const int end_pos = GetHead(nHardware::HEAD_WRITE).GetPosition();
  const int inject_size = end_pos - start_pos;
  
  // Make sure the creature will still be above the minimum size,
  if (inject_size <= 0) {
    m_organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: no code to inject");
    return false; // (inject fails)
  }
  if (start_pos < MIN_CREATURE_SIZE) {
    m_organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: new size too small");
    return false; // (inject fails)
  }
  
  // Since its legal to cut out the injected piece, do so.
  cGenome inject_code( cGenomeUtil::Crop(m_memory, start_pos, end_pos) );
  m_memory.Remove(start_pos, inject_size);
  
  // If we don't have a host, stop here.
  cOrganism * host_organism = m_organism->GetNeighbor();
  if (host_organism == NULL) return false;
  
  // Scan for the label to match...
  ReadLabel();
  
  // If there is no label, abort.
  if (GetLabel().GetSize() == 0) {
    m_organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: label required");
    return false; // (inject fails)
  }
  
  // Search for the label in the host...
  GetLabel().Rotate(1, NUM_NOPS);
  
  if (host_organism->GetHardware().InjectHost(GetLabel(), inject_code)) {
    if (ForkThread()) m_organism->GetPhenotype().IsMultiThread() = true;
  }
  
  // Set the relevent flags.
  m_organism->GetPhenotype().IsModifier() = true;
  
  return true;
}

bool cHardwareCPU::Inst_TaskGet(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  const int value = m_organism->GetNextInput();
  GetRegister(reg_used) = value;
  m_organism->DoInput(value);
  return true;
}


// @JEB - this instruction does more than two "gets" together, it also resets the inputs
bool cHardwareCPU::Inst_TaskGet2(cAvidaContext& ctx)
{
  // Randomize the inputs so they can't save numbers
  m_organism->GetOrgInterface().ResetInputs(ctx);   // Now re-randomize the inputs this organism sees
  m_organism->ClearInput();                         // Also clear their input buffers, or they can still claim
	// rewards for numbers no longer in their environment!
	
  const int reg_used_1 = FindModifiedRegister(REG_BX);
  const int reg_used_2 = FindNextRegister(reg_used_1);
  
  const int value1 = m_organism->GetNextInput();
  GetRegister(reg_used_1) = value1;
  m_organism->DoInput(value1);
  
  const int value2 = m_organism->GetNextInput();
  GetRegister(reg_used_2) = value2;
  m_organism->DoInput(value2);
  
  return true;
}

bool cHardwareCPU::Inst_TaskStackGet(cAvidaContext& ctx)
{
  const int value = m_organism->GetNextInput();
  StackPush(value);
  m_organism->DoInput(value);
  return true;
}

bool cHardwareCPU::Inst_TaskStackLoad(cAvidaContext& ctx)
{
  // @DMB - TODO: this should look at the input_size...
  for (int i = 0; i < 3; i++) 
    StackPush( m_organism->GetNextInput() );
  return true;
}

bool cHardwareCPU::Inst_TaskPut(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int value = GetRegister(reg_used);
  GetRegister(reg_used) = 0;
  m_organism->DoOutput(ctx, value);
  return true;
}

bool cHardwareCPU::Inst_TaskPutResetInputs(cAvidaContext& ctx)
{
  bool return_value = Inst_TaskPut(ctx);          // Do a normal put
  m_organism->GetOrgInterface().ResetInputs(ctx);   // Now re-randomize the inputs this organism sees
  m_organism->ClearInput();                         // Also clear their input buffers, or they can still claim
	// rewards for numbers no longer in their environment!
  return return_value;
}

bool cHardwareCPU::Inst_TaskIO(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  // Do the "put" component
  const int value_out = GetRegister(reg_used);
  m_organism->DoOutput(ctx, value_out);  // Check for tasks completed.
  
  // Do the "get" component
  const int value_in = m_organism->GetNextInput();
  GetRegister(reg_used) = value_in;
  m_organism->DoInput(value_in);
  return true;
}

bool cHardwareCPU::Inst_TaskIO_BonusCost(cAvidaContext& ctx, double bonus_cost)
{
  // Levy the cost
  double new_bonus = m_organism->GetPhenotype().GetCurBonus() * (1 - bonus_cost);
  if (new_bonus < 0) new_bonus = 0;
  //keep the bonus positive or zero
  m_organism->GetPhenotype().SetCurBonus(new_bonus);
  
  return Inst_TaskIO(ctx);
}

bool cHardwareCPU::Inst_TaskIO_Feedback(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
	
  //check cur_bonus before the output
  double preOutputBonus = m_organism->GetPhenotype().GetCurBonus();
  
  // Do the "put" component
  const int value_out = GetRegister(reg_used);
  m_organism->DoOutput(ctx, value_out);  // Check for tasks completed.
	
  //check cur_merit after the output
  double postOutputBonus = m_organism->GetPhenotype().GetCurBonus(); 
  
  
  //push the effect of the IO on merit (+,0,-) to the active stack
	
  if (preOutputBonus > postOutputBonus){
    StackPush(-1);
	}
  else if (preOutputBonus == postOutputBonus){
    StackPush(0);
	}
  else if (preOutputBonus < postOutputBonus){
    StackPush(1);
	}
  else {
    assert(0);
    //Bollocks. There was an error.
	}
	
	
  
	
	
  
  // Do the "get" component
  const int value_in = m_organism->GetNextInput();
  GetRegister(reg_used) = value_in;
  m_organism->DoInput(value_in);
  return true;
}

bool cHardwareCPU::Inst_MatchStrings(cAvidaContext& ctx)
{
	if (m_executedmatchstrings)
		return false;
	m_organism->DoOutput(ctx, 357913941);
	m_executedmatchstrings = true;
	return true;
}

bool cHardwareCPU::Inst_Sell(cAvidaContext& ctx)
{
	int search_label = GetLabel().AsInt(3) % MARKET_SIZE;
	int send_value = GetRegister(REG_BX);
	int sell_price = m_world->GetConfig().SELL_PRICE.Get();
	m_organism->SellValue(send_value, search_label, sell_price);
	return true;
}

bool cHardwareCPU::Inst_Buy(cAvidaContext& ctx)
{
	int search_label = GetLabel().AsInt(3) % MARKET_SIZE;
	int buy_price = m_world->GetConfig().BUY_PRICE.Get();
	GetRegister(REG_BX) = m_organism->BuyValue(search_label, buy_price);
	return true;
}

bool cHardwareCPU::Inst_Send(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  m_organism->SendValue(GetRegister(reg_used));
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_Receive(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = m_organism->ReceiveValue();
  return true;
}

bool cHardwareCPU::Inst_SenseLog2(cAvidaContext& ctx)
{
  return DoSense(ctx, 0, 2);
}

bool cHardwareCPU::Inst_SenseUnit(cAvidaContext& ctx)
{
  return DoSense(ctx, 1, 1);
}

bool cHardwareCPU::Inst_SenseMult100(cAvidaContext& ctx)
{
  return DoSense(ctx, 1, 100);
}

bool cHardwareCPU::DoSense(cAvidaContext& ctx, int conversion_method, double base)
{
  // Returns the amount of a resource or resources 
  // specified by modifying NOPs into register BX
  const tArray<double> res_count = m_organism->GetOrgInterface().GetResources() + 
    m_organism->GetOrgInterface().GetDemeResources(m_organism->GetOrgInterface().GetDemeID());

  // Arbitrarily set to BX since the conditional instructions use this directly.
  int reg_to_set = REG_BX;
	
  // There are no resources, return
  if (res_count.GetSize() == 0) return false;
	
  // Only recalculate logs if these values have changed
  static int last_num_resources = 0;
  static int max_label_length = 0;
  int num_nops = GetInstSet().GetNumNops();
  
  if ((last_num_resources != res_count.GetSize()))
  {
		max_label_length = (int) ceil(log((double)res_count.GetSize())/log((double)num_nops));
		last_num_resources = res_count.GetSize();
  }
	
  // Convert modifying NOPs to the index of the resource.
  // If there are fewer than the number of NOPs required
  // to uniquely specify a resource, then add together
  // a subset of resources (motivation: regulation can evolve
  // to be more specific if there is an advantage)
	
  // Find the maximum number of NOPs needed to specify this number of resources
  // Note: It's a bit wasteful to recalculate this every time and organisms will
  // definitely be confused if the number of resources changes during a run
  // because their mapping to resources will be disrupted
  
  // Attempt to read a label with this maximum length
  ReadLabel(max_label_length);
  
  // Find the length of the label that we actually obtained (max is max_reg_needed)
  int real_label_length = GetLabel().GetSize();
  
  // Start and end labels to define the start and end indices of  
  // resources that we need to add together
  cCodeLabel start_label = cCodeLabel(GetLabel());
  cCodeLabel   end_label = cCodeLabel(GetLabel());
  
  for (int i = 0; i < max_label_length - real_label_length; i++)
  {
    start_label.AddNop(0);
    end_label.AddNop(num_nops-1);
  }
  
  int start_index = start_label.AsInt(num_nops);
  int   end_index =   end_label.AsInt(num_nops);
	
  // If the label refers to ONLY resources that 
  // do not exist, then the operation fails
  if (start_index >= res_count.GetSize()) return false;
	
  // Otherwise sum all valid resources that it might refer to
  // (this will only be ONE if the label was of the maximum length).
  int resource_result = 0;
  double dresource_result = 0;
  for (int i = start_index; i <= end_index; i++)
  {
    // if it's a valid resource
    if (i < res_count.GetSize())
    {
      if (conversion_method == 0) // Log
      {
        // for log, add together and then take log
        dresource_result += (double) res_count[i];
      }
      else if (conversion_method == 1) // Addition of multiplied resource amount
      {
        int add_amount = (int) (res_count[i] * base);
        // Do some range checking to make sure we don't overflow
        resource_result = (INT_MAX - resource_result <= add_amount) ? INT_MAX : resource_result + add_amount;
      }
    } 
  }
  
  // Take the log after adding resource amounts together! This way a zero can be assigned to INT_MIN
  if (conversion_method == 0) // Log2
  {
    // You really shouldn't be using the log method if you can get to zero resources
    if(dresource_result == 0.0)
    {
      resource_result = INT_MIN;
    }
    else
    {
      resource_result = (int)(log(dresource_result)/log(base));
    }
  }
	
  //Dump this value into an arbitrary register: BX
  GetRegister(reg_to_set) = resource_result;
  
  //We have to convert this to a different index that includes all degenerate labels possible: shortest to longest
  int sensed_index = 0;
  int on = 1;
  for (int i = 0; i < real_label_length; i++)
  {
    sensed_index += on;
    on *= num_nops;
  }
  sensed_index+= GetLabel().AsInt(num_nops);
  m_organism->GetPhenotype().IncSenseCount(sensed_index);
  
  return true; 
	
  // Note that we are converting <double> resources to <int> register values
}

/* Convert modifying NOPs to the index of a resource. If there are fewer 
 * than the number of NOPs required to specify a resource, find the subset 
 * of resources.  (Motivation: can evolve to be more specific if there is 
 * an advantage.)
 *
 * Mostly ripped from Jeff B.'s DoSense(); meant to be a helper function for
 * DoSense, Inst_Collect, and anything else that wants to use this type of
 * resource NOP-specification.
 *
 * returns true if successful, false otherwise
 *
 * start_index and end_index of resource range are "returned" via their 
 * respective arguments; any int at all may be passed to these, as it will just 
 * get overwritten.  (Obviously, if the resource is fully specified, 
 * start_index == end_index.)
 */

bool cHardwareCPU::FindModifiedResource(int& start_index, int& end_index)
{
  int num_resources = m_organism->GetOrgInterface().GetResources().GetSize();
  
  //if there are no resources, translation cannot be successful; return false
  if (num_resources <= 0)
  {return false;}
  
  //calculate the maximum number of NOPs necessary to completely specify a resource
  int num_nops = GetInstSet().GetNumNops();
  int max_label_length = (int)(ceil(log((double)num_resources) / log((double)num_nops)));
  
  //attempt to read a label of the maximum length
  ReadLabel(max_label_length);
  
  //find the length of the label that was actually read
  int real_label_length = GetLabel().GetSize();
  
  /* find start and end resource indices specified by the label */
  
  cCodeLabel start_label = cCodeLabel(GetLabel());
  cCodeLabel   end_label = cCodeLabel(GetLabel());
  
  //fill out any labels that are not maximum length
  for (int i = 0; i < max_label_length - real_label_length; i++){
    start_label.AddNop(0);
    end_label.AddNop(num_nops - 1);
  }
  
  //translate into resource indices
  start_index = start_label.AsInt(num_nops);
  end_index   =   end_label.AsInt(num_nops);
  
  return true;
}

/* Helper function to reduce code redundancy in the Inst_Collect variations.
 * Does all the heavy lifting of external resource collection.  Use env_remove
 * to specify whether the collected resources should be removed from the
 * environment, and internal_add to specify whether the collected resources
 * should be added to the organism's internal resources.
 */
bool cHardwareCPU::DoCollect(cAvidaContext& ctx, bool env_remove, bool internal_add)
{
  int start_bin, end_bin, bin_used;

  bool finite_resources_exist = FindModifiedResource(start_bin, end_bin);
  if(!finite_resources_exist) {return true;}

  if(start_bin == end_bin)  // resource completely specified
  {bin_used = start_bin;}
  else
  {
    switch (m_world->GetConfig().MULTI_ABSORB_TYPE.Get())
    {
      case 0:
        bin_used = ctx.GetRandom().GetInt(start_bin, end_bin + 1);  // since the max passed to GetInt() will never be returned
        break;
      case 1:
        bin_used = start_bin;
        break;
      case 2:
        bin_used = end_bin;
        break;
      case 3:
        bin_used = -1; // not really, of course!  This just functions as a flag to say we are using a range.
        break;
      default:
        bin_used = ctx.GetRandom().GetInt(start_bin, end_bin + 1);  // arbitrary choice of the default.  Shouldn't matter, since it shouldn't be needed...
        break;
    }
  }

  // Set up res_change and max total
  const tArray<double> res_count = m_organism->GetOrgInterface().GetResources();
  tArray<double> res_change(res_count.GetSize());
  res_change.SetAll(0.0);
  double total = m_organism->GetRBinsTotal();
  double max = m_world->GetConfig().MAX_TOTAL_STORED.Get();

	/* Remove resource(s) from environment if env_remove is set;
   * add resource(s) to internal resource bins if internal_add is set
   * (and this would not fill the bin beyond max).
   */
  if(bin_used >= 0)
  {
    res_change[bin_used] = -1 * (res_count[bin_used] * m_world->GetConfig().ABSORB_RESOURCE_FRACTION.Get());
    
    if(internal_add && (max < 0 || (total + -1 * res_change[bin_used]) <= max))
    {m_organism->AddToRBin(bin_used, -1 * res_change[bin_used]);}
    else
    {res_change[bin_used] = 0.0;}
    
    if(!env_remove)
    {res_change[bin_used] = 0.0;}
  }
  else
  {
    int num_bins = end_bin - start_bin;
    for(int i = start_bin; i <= end_bin; i++)
    {
      res_change[i] = -1 * (res_count[bin_used] * m_world->GetConfig().ABSORB_RESOURCE_FRACTION.Get() / num_bins);
      if(internal_add && (max < 0 || (total + -1 * res_change[i]) <= max))
      {m_organism->AddToRBin(i, -1 * res_change[i]);}
      else
      {res_change[i] = 0.0;}   
      
      if(!env_remove)
      {res_change[i] = 0.0;}
    }
  }
  
  // Update resource counts to reflect res_change
  m_organism->GetOrgInterface().UpdateResources(res_change);
  
  return true;
}

/* Takes resource(s) from the environment and adds them to the internal resource
 * bins of the organism.
 */
bool cHardwareCPU::Inst_Collect(cAvidaContext& ctx)
{
  return DoCollect(ctx, true, true);
}

/* Like Inst_Collect, but the collected resources are not removed from the
 * environment.
 */
bool cHardwareCPU::Inst_CollectNoEnvRemove(cAvidaContext& ctx)
{
  return DoCollect(ctx, false, true);
}

/* Like Inst_Collect, but the collected resources are not added to the organism.
 */
bool cHardwareCPU::Inst_CollectNoInternalAdd(cAvidaContext& ctx)
{
  return DoCollect(ctx, true, false);
}

/*! Sense the level of resources in this organism's cell, and if all of the 
 resources present are above the min level for that resource, execute the following
 intruction.  Otherwise, skip the following instruction.
 */
bool cHardwareCPU::Inst_IfResources(cAvidaContext& ctx)
{
  // These are the current levels of resources at this cell:
  const tArray<double> resources = m_organism->GetOrgInterface().GetResources() + 
    m_organism->GetOrgInterface().GetDemeResources(m_organism->GetOrgInterface().GetDemeID());

  // Now we loop through the different reactions, checking to see if their
  // required resources are below what's available.  If so, we skip ahead an
  // instruction and return.
  const cReactionLib& rxlib = m_world->GetEnvironment().GetReactionLib();
  for(int i=0; i<rxlib.GetSize(); ++i) {
    cReaction* rx = rxlib.GetReaction(i);
    tLWConstListIterator<cReactionProcess> processes(rx->GetProcesses());
    while(!processes.AtEnd()) {
      const cReactionProcess* proc = processes.Next();
      cResource* res = proc->GetResource(); // Infinite resource == 0.
      if((res != 0) && (resources[res->GetID()] < proc->GetMinNumber())) {
        IP().Advance();
        return true;
      }
    }
  }
  return true;
}

void cHardwareCPU::DoDonate(cOrganism* to_org)
{
  assert(to_org != NULL);
	
  const double merit_given = m_world->GetConfig().MERIT_GIVEN.Get();
  const double merit_received = m_world->GetConfig().MERIT_RECEIVED.Get();
	
  double cur_merit = m_organism->GetPhenotype().GetMerit().GetDouble();
  cur_merit -= merit_given;
  if(cur_merit < 0) cur_merit=0; 
	
  // Plug the current merit back into this organism and notify the scheduler.
  m_organism->UpdateMerit(cur_merit);
  m_organism->GetPhenotype().SetIsEnergyDonor();
  
  // Update the merit of the organism being donated to...
  double other_merit = to_org->GetPhenotype().GetMerit().GetDouble();
  other_merit += merit_received;
  to_org->UpdateMerit(other_merit);
  to_org->GetPhenotype().SetIsEnergyReceiver();

}

void cHardwareCPU::DoEnergyDonate(cOrganism* to_org)
{
  assert(to_org != NULL);
	
  const double frac_energy_given = m_organism->GetFracEnergyDonating();
	
  double cur_energy = m_organism->GetPhenotype().GetStoredEnergy();
  double energy_given = cur_energy * frac_energy_given;
  
  //update energy store and merit of donor
  m_organism->GetPhenotype().ReduceEnergy(energy_given);
  m_organism->GetPhenotype().IncreaseEnergyDonated(energy_given);
  double senderMerit = cMerit::EnergyToMerit(m_organism->GetPhenotype().GetStoredEnergy()  * m_organism->GetPhenotype().GetEnergyUsageRatio(), m_world);
  m_organism->UpdateMerit(senderMerit);
  m_organism->GetPhenotype().SetIsEnergyDonor();
  
  // update energy store and merit of donee
  to_org->GetPhenotype().ReduceEnergy(-1.0*energy_given);
  to_org->GetPhenotype().IncreaseEnergyReceived(energy_given);
  double receiverMerit = cMerit::EnergyToMerit(to_org->GetPhenotype().GetStoredEnergy() * to_org->GetPhenotype().GetEnergyUsageRatio(), m_world);
  to_org->UpdateMerit(receiverMerit);
  to_org->GetPhenotype().SetIsEnergyReceiver();
}


// The difference between this version and the previous is that this one allows energy to be placed
// into the recipient's incoming energy buffer and not be applied immediately.  Also, some of the
// energy may be lost in transfer
void cHardwareCPU::DoEnergyDonatePercent(cOrganism* to_org, const double frac_energy_given)
{
  double losspct = m_world->GetConfig().ENERGY_SHARING_LOSS.Get();
  
  assert(to_org != NULL);
  assert(frac_energy_given >= 0);
  assert(frac_energy_given <= 1);
  assert(losspct >= 0);
  assert(losspct <= 1);
    
  double cur_energy = m_organism->GetPhenotype().GetStoredEnergy();
  const int update_metabolic = m_world->GetConfig().ENERGY_SHARING_UPDATE_METABOLIC.Get();
  double energy_given = cur_energy * frac_energy_given;
  
  //update energy store and merit of donor
  m_organism->GetPhenotype().ReduceEnergy(energy_given);
  m_organism->GetPhenotype().IncreaseEnergyDonated(energy_given);
	
  if(update_metabolic == 1) {
    double senderMerit = cMerit::EnergyToMerit(m_organism->GetPhenotype().GetStoredEnergy()  * m_organism->GetPhenotype().GetEnergyUsageRatio(), m_world);
    m_organism->UpdateMerit(senderMerit);
  }
	  
  //apply loss in transfer
  energy_given *= (1 - losspct);
  
  //place energy into receiver's incoming energy buffer
  to_org->GetPhenotype().ReceiveDonatedEnergy(energy_given);
  
  //if we are using the push energy method, pass the new energy into the receiver's energy store and recalculate merit
  if(m_world->GetConfig().ENERGY_SHARING_METHOD.Get() == 1) {
    to_org->GetPhenotype().ApplyDonatedEnergy();
	  
	  if(update_metabolic == 1) {
        double receiverMerit = cMerit::EnergyToMerit(to_org->GetPhenotype().GetStoredEnergy() * to_org->GetPhenotype().GetEnergyUsageRatio(), m_world);
        to_org->UpdateMerit(receiverMerit);
	  }
  }
  
} //End DoEnergyDonatePercent()


bool cHardwareCPU::Inst_DonateFacing(cAvidaContext& ctx) {
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorRand();
	
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  // Donate only if we have found a neighbor.
  if (neighbor != NULL) {
    DoEnergyDonate(neighbor);
    
    neighbor->GetPhenotype().SetIsReceiver();
  }
  return true;
}

bool cHardwareCPU::Inst_DonateRandom(cAvidaContext& ctx)
{
  
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorRand();
	
  // Turn to a random neighbor, get it, and turn back...
  int neighbor_id = ctx.GetRandom().GetInt(m_organism->GetNeighborhoodSize());
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(1);
  cOrganism * neighbor = m_organism->GetNeighbor();
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(-1);
  
  // Donate only if we have found a neighbor.
  if (neighbor != NULL) {
    DoDonate(neighbor);
    
    //print out how often random donations go to kin
    /*
		 static ofstream kinDistanceFile("kinDistance.dat");
		 kinDistanceFile << (genotype->GetPhyloDistance(neighbor->GetGenotype())<=1) << " ";
		 kinDistanceFile << (genotype->GetPhyloDistance(neighbor->GetGenotype())<=2) << " ";
		 kinDistanceFile << (genotype->GetPhyloDistance(neighbor->GetGenotype())<=3) << " ";
		 kinDistanceFile << genotype->GetPhyloDistance(neighbor->GetGenotype());
		 kinDistanceFile << endl; 
		 */
    neighbor->GetPhenotype().SetIsReceiverRand();
  }
	
  return true;
}


bool cHardwareCPU::Inst_DonateKin(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
  
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorKin();
	
	
  // Find the target as the first Kin found in the neighborhood.
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(1);
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  // If there is no max distance, just take the random neighbor we're facing.
  const int max_dist = m_world->GetConfig().MAX_DONATE_KIN_DIST.Get();
  if (max_dist != -1) {
    int max_id = neighbor_id + num_neighbors;
    bool found = false;
    cGenotype* genotype = m_organism->GetGenotype();
    while (neighbor_id < max_id) {
      neighbor = m_organism->GetNeighbor();
      if (neighbor != NULL &&
          genotype->GetPhyloDistance(neighbor->GetGenotype()) <= max_dist) {
        found = true;
        break;
      }
      m_organism->Rotate(1);
      neighbor_id++;
    }
    if (found == false) neighbor = NULL;
  }
  
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(-1);
  
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL){
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverKin();
  }
  return true;
}

bool cHardwareCPU::Inst_DonateEditDist(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorEdit();
  
  // Find the target as the first Kin found in the neighborhood.
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(1);
  cOrganism* neighbor = m_organism->GetNeighbor();
  
  // If there is no max edit distance, take the random neighbor we're facing.
  const int max_dist = m_world->GetConfig().MAX_DONATE_EDIT_DIST.Get();
  if (max_dist != -1) {
    int max_id = neighbor_id + num_neighbors;
    bool found = false;
    while (neighbor_id < max_id) {
      neighbor = m_organism->GetNeighbor();
      int edit_dist = max_dist + 1;
      if (neighbor != NULL) {
        edit_dist = cGenomeUtil::FindEditDistance(m_organism->GetGenome(),
                                                  neighbor->GetGenome());
      }
      if (edit_dist <= max_dist) {
        found = true;
        break;
      }
      m_organism->Rotate(1);
      neighbor_id++;
    }
    if (found == false) neighbor = NULL;
  }
  
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(-1);
  
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL){
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverEdit();
  }
  return true;
}

bool cHardwareCPU::Inst_DonateGreenBeardGene(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome (see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  phenotype.IncDonates();
  phenotype.SetIsDonorGbg();
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
      neighbor = m_organism->GetNeighbor();

      //if neighbor exists, do they have the green beard gene?
      if (neighbor != NULL) {
          const cGenome & neighbor_genome = neighbor->GetGenome();

          // for each instruction in the genome...
          for(int i=0;i<neighbor_genome.GetSize();i++){

            // ...see if it is donate-gbg
            if (neighbor_genome[i] == IP().GetInst()) {
              found = true;
              break;
            }
	    
          }
      }
      
      // stop searching through the neighbors if we already found one
      if (found == true) break;
  
      m_organism->Rotate(1);
      neighbor_id++;
  }
	
	if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(-1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverGbg();
  }
  
  return true;
  
}

bool cHardwareCPU::Inst_DonateTrueGreenBeard(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome AND their parents excuted it
  //(see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  phenotype.IncDonates();
  phenotype.SetIsDonorTrueGb();
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
      neighbor = m_organism->GetNeighbor();
      //if neighbor exists, AND if their parent attempted to donate,
      if (neighbor != NULL && neighbor->GetPhenotype().IsDonorTrueGbLast()) {
          const cGenome & neighbor_genome = neighbor->GetGenome();

          // for each instruction in the genome...
          for(int i=0;i<neighbor_genome.GetSize();i++){

            // ...see if it is donate-tgb, if so, we found a target
            if (neighbor_genome[i] == IP().GetInst()) {
              found = true;
              break;
            }
	    
          }
      }
      
      // stop searching through the neighbors if we already found one
      if (found == true) break;
  
      m_organism->Rotate(1);
      neighbor_id++;
  }
	
	if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(-1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverTrueGb();
  }
	
  
  return true;
  
}

bool cHardwareCPU::Inst_DonateThreshGreenBeard(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome AND their parents excuted it >=THRESHOLD number of times
  //(see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
	
  phenotype.IncDonates();
  phenotype.SetIsDonorThreshGb();
  phenotype.IncNumThreshGbDonations();
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
      neighbor = m_organism->GetNeighbor();
      //if neighbor exists, AND if their parent attempted to donate >= threshhold,
      if (neighbor != NULL && neighbor->GetPhenotype().GetNumThreshGbDonationsLast()>= m_world->GetConfig().MIN_GB_DONATE_THRESHOLD.Get() ) {
          const cGenome & neighbor_genome = neighbor->GetGenome();

          // for each instruction in the genome...
          for(int i=0;i<neighbor_genome.GetSize();i++){

	         // ...see if it is donate-threshgb, if so, we found a target
            if (neighbor_genome[i] == IP().GetInst()) {
              found = true;
              break;
            }
	    
          }
      }
      
      // stop searching through the neighbors if we already found one
      if (found == true) break;
  
      m_organism->Rotate(1);
      neighbor_id++;
  }
	
	if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(-1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverThreshGb();
    // cout << "************ neighbor->GetPhenotype().GetNumThreshGbDonationsLast() is " << neighbor->GetPhenotype().GetNumThreshGbDonationsLast() << endl;
    
  }
	
  return true;
  
}


bool cHardwareCPU::Inst_DonateQuantaThreshGreenBeard(cAvidaContext& ctx)
{
  // this donates to organisms that have this instruction anywhere
  // in their genome AND their parents excuted it more than a
  // THRESHOLD number of times where that threshold depend on the
  // number of times the individual's parents attempted to donate
  // using this instruction.  The threshold levels are multiples of
  // the quanta value set in genesis, and the highest level that
  // the donor qualifies for is the one used.
	
  // (see Dawkins 1976, The Selfish Gene, for 
  // the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (phenotype.GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  phenotype.IncDonates();
  phenotype.SetIsDonorQuantaThreshGb();
  phenotype.IncNumQuantaThreshGbDonations();
  //cout << endl << "quanta_threshgb attempt.. " ;
	
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // Get the quanta (step size) between threshold levels.
  const int donate_quanta = m_world->GetConfig().DONATE_THRESH_QUANTA.Get();
  
  // Calculate what quanta level we should be at for this individual.  We do a
  // math trick to make sure its the next lowest event multiple of donate_quanta.
  const int quanta_donate_thresh =
	(phenotype.GetNumQuantaThreshGbDonationsLast() / donate_quanta) * donate_quanta;
  //cout << " phenotype.GetNumQuantaThreshGbDonationsLast() is " << phenotype.GetNumQuantaThreshGbDonationsLast();
  //cout << " quanta thresh=  " << quanta_donate_thresh;
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
      neighbor = m_organism->GetNeighbor();
      //if neighbor exists, AND if their parent attempted to donate >= threshhold,
      if (neighbor != NULL &&
	  neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast() >= quanta_donate_thresh) {

          const cGenome & neighbor_genome = neighbor->GetGenome();

          // for each instruction in the genome...
          for(int i=0;i<neighbor_genome.GetSize();i++){

	         // ...see if it is donate-quantagb, if so, we found a target
            if (neighbor_genome[i] == IP().GetInst()) {
              found = true;
              break;
            }
	    
          }
      }
      
      // stop searching through the neighbors if we already found one
      if (found == true) break;
  
      m_organism->Rotate(1);
      neighbor_id++;
  }
	
	if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(-1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverQuantaThreshGb();
    //cout << " ************ neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast() is " << neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast();
    
  }
	
  return true;
  
}


bool cHardwareCPU::Inst_DonateNULL(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorNull();
  
  // This is a fake donate command that causes the organism to lose merit,
  // but no one else to gain any.
  
  const double merit_given = m_world->GetConfig().MERIT_GIVEN.Get();
  double cur_merit = m_organism->GetPhenotype().GetMerit().GetDouble();
  cur_merit -= merit_given;
  
  // Plug the current merit back into this organism and notify the scheduler.
  m_organism->UpdateMerit(cur_merit);
  
  return true;
}


//Move energy from an organism's received energy buffer into their energy store, recalculate merit
bool cHardwareCPU::Inst_ReceiveDonatedEnergy(cAvidaContext& ctx)
{
  if(m_organism->GetCellID() < 0) {
    return false;
  }
  
  if(m_organism->GetPhenotype().GetEnergyInBufferAmount() > 0) {
    m_organism->GetPhenotype().ApplyDonatedEnergy();
    m_organism->GetPhenotype().SetHasUsedDonatedEnergy();
	 
	  if(m_world->GetConfig().ENERGY_SHARING_UPDATE_METABOLIC.Get() == 1) {
        double receiverMerit = cMerit::EnergyToMerit(m_organism->GetPhenotype().GetStoredEnergy() * m_organism->GetPhenotype().GetEnergyUsageRatio(), m_world);
        m_organism->UpdateMerit(receiverMerit);
  	}
  }
  
  return true;
  
} //End Inst_ReceiveDonatedEnergy()


//Donate a fraction of organism's energy to the organism that last requested it.
bool cHardwareCPU::Inst_DonateEnergy(cAvidaContext& ctx)
{
  if(m_organism->GetCellID() < 0) {
    return false;
  }

  const cOrgMessage* msg = m_organism->RetrieveMessage();
  if(msg == 0) {
    return false;
  }
  
  /* MJM - by this point, the pointer returned by GetSender() may no longer
   * be any good. Instead, we should use the cell and organism ID of the
   * message sender to get hold of the sender (if it still exists and hasn't moved)
   */

  cPopulationCell senderCell = m_world->GetPopulation().GetCell(msg->GetSenderCellID());
  if (!senderCell.IsOccupied()) {
	  // the organism that made the request is gone, we can't donate...
	  return false;
  }
  cOrganism* energyReceiver = senderCell.GetOrganism();
  if (energyReceiver->GetID() != msg->GetSenderOrgID()) {
	  // some other organism has occupied this cell since the msg was sent,
	  // we can't donate...
	  return false;
  }
  
  DoEnergyDonatePercent(energyReceiver, m_organism->GetFracEnergyDonating());
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsEnergyDonor();
  
  return true;
  
} //End Inst_DonateEnergy()


//Update the organism's metabolic rate
bool cHardwareCPU::Inst_UpdateMetabolicRate(cAvidaContext& ctx)
{
  double newmerit = cMerit::EnergyToMerit(m_organism->GetPhenotype().GetStoredEnergy()  * m_organism->GetPhenotype().GetEnergyUsageRatio(), m_world);
  m_organism->UpdateMerit(newmerit);
  
  return true;
} //End Inst_UpdateMetabolocRate()


//Donate a fraction of organism's energy to faced organism.
bool cHardwareCPU::Inst_DonateEnergyFaced(cAvidaContext& ctx)
{
  if(m_organism->GetCellID() < 0) {
    return false;
  }	

  cOrganism * neighbor = m_organism->GetNeighbor();
  
  if ( (neighbor != NULL) && (!neighbor->IsDead()) ) {
    
    // If the neighbor has requested energy or if we're allowing push sharing, share energy
    if ( (neighbor->GetPhenotype().HasOpenEnergyRequest()) || (m_world->GetConfig().ENERGY_SHARING_METHOD.Get() == 1) )
    {
      DoEnergyDonatePercent(neighbor, m_organism->GetFracEnergyDonating());
      m_organism->GetPhenotype().IncDonates();
      m_organism->GetPhenotype().SetIsEnergyDonor();
    }
  }  
  
  return true;
  
} //End Inst_DonateEnergyFaced()


// Rotate to face the most energy needy neighbor
bool cHardwareCPU::Inst_RotateToMostNeedy(cAvidaContext& ctx)
{
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
  
  cPopulation& pop = m_world->GetPopulation();
  cPopulationCell& mycell = pop.GetCell(m_organism->GetCellID());
  
  double min_energy = m_world->GetConfig().ENERGY_CAP.Get();
  int num_rotations = 0;
  cOrganism *neighbor;
  double neighbor_energy;

  // Look at the energy levels of neighbors
  for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
    mycell.ConnectionList().CircNext();
    neighbor = m_organism->GetNeighbor();
        
    // If this neighbor is alive and has a request for energy or we're allowing pushing of energy, look at it
    if ( (neighbor != NULL) && (!neighbor->IsDead()) &&
         ((neighbor->GetPhenotype().HasOpenEnergyRequest()) || (m_world->GetConfig().ENERGY_SHARING_METHOD.Get() == 1)) ) {
      neighbor_energy = neighbor->GetPhenotype().GetStoredEnergy();
      
      if( (neighbor_energy > 0) && (neighbor_energy < min_energy) ) {
        num_rotations = i;
      }
    }
    
  }
    
  //Rotate to face the most needy neighbor
  for(int i = 0; i < num_rotations; i++) {
    mycell.ConnectionList().CircNext();
  }
  
  return true;
  
} //End Inst_RotateToMostNeedy()


//Broadcast a request for energy
bool cHardwareCPU::Inst_RequestEnergy(cAvidaContext& ctx)
{
  if(m_organism->GetCellID() < 0) {
    return false;
  }
      
  cOrgMessage msg(m_organism);
  // Could set the data field of the message to be the multiplier
  
  m_organism->BroadcastMessage(ctx, msg);
  m_organism->GetPhenotype().SetIsEnergyRequestor();
  
  return true;
  
} //End Inst_RequestEnergy()


//Set the request energy flag
bool cHardwareCPU::Inst_RequestEnergyFlagOn(cAvidaContext& ctx)
{
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
  
  m_organism->GetPhenotype().SetIsEnergyRequestor();
  m_organism->GetPhenotype().SetHasOpenEnergyRequest();
  return true;
} //End Inst_RequestEnergyFlagOn()


//Set the request energy flag to off
bool cHardwareCPU::Inst_RequestEnergyFlagOff(cAvidaContext& ctx)
{
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
  
  m_organism->GetPhenotype().ClearHasOpenEnergyRequest();
  return true;
} //End Inst_RequestEnergyFlagOff()


// Increase the amount of energy to be donated
bool cHardwareCPU::Inst_IncreaseEnergyDonation(cAvidaContext& ctx)
{
  double curr_amount = m_organism->GetFracEnergyDonating();
  double increment = m_world->GetConfig().ENERGY_SHARING_INCREMENT.Get();

  m_organism->SetFracEnergyDonating(min(1.0, curr_amount + increment));  
  
  return true;
  
} //End Inst_IncreaseEnergyDonation()


// Decrease the amount of energy to be donated
bool cHardwareCPU::Inst_DecreaseEnergyDonation(cAvidaContext& ctx)
{
  double curr_amount = m_organism->GetFracEnergyDonating();
  double increment = m_world->GetConfig().ENERGY_SHARING_INCREMENT.Get();
  
  m_organism->SetFracEnergyDonating(max(0.0, curr_amount - increment));  
  
  return true;
  
} //End Inst_DecreaseEnergyDonation()


bool cHardwareCPU::Inst_SearchF(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  const int search_size = FindLabel(1).GetPosition() - IP().GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  return true;
}

bool cHardwareCPU::Inst_SearchB(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  const int search_size = IP().GetPosition() - FindLabel(-1).GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  return true;
}

bool cHardwareCPU::Inst_MemSize(cAvidaContext& ctx)
{
  GetRegister(FindModifiedRegister(REG_BX)) = m_memory.GetSize();
  return true;
}

bool cHardwareCPU::Inst_IOBufAdd1(cAvidaContext& ctx)
{ 
	m_organism->AddOutput(1);
	return true;
}
bool cHardwareCPU::Inst_IOBufAdd0(cAvidaContext& ctx)
{ 
	m_organism->AddOutput(0);
	return true; 
}

bool cHardwareCPU::Inst_RotateL(cAvidaContext& ctx)
{
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  
  // If this organism has no neighbors, ignore rotate.
  if (num_neighbors == 0) return false;
  
  ReadLabel();
  
  // Always rotate at least once.
  m_organism->Rotate(-1);
  
  // If there is no label, then the one rotation was all we want.
  if (!GetLabel().GetSize()) return true;
  
  // Rotate until a complement label is found (or all have been checked).
  GetLabel().Rotate(1, NUM_NOPS);
  for (int i = 1; i < num_neighbors; i++) {
    cOrganism* neighbor = m_organism->GetNeighbor();
    
    if (neighbor != NULL && neighbor->GetHardware().FindLabelFull(GetLabel()).InMemory()) return true;
    
    // Otherwise keep rotating...
    m_organism->Rotate(-1);
  }
  return true;
}

bool cHardwareCPU::Inst_RotateR(cAvidaContext& ctx)
{
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  
  // If this organism has no neighbors, ignore rotate.
  if (num_neighbors == 0) return false;
  
  ReadLabel();
  
  // Always rotate at least once.
  m_organism->Rotate(1);
  
  // If there is no label, then the one rotation was all we want.
  if (!GetLabel().GetSize()) return true;
  
  // Rotate until a complement label is found (or all have been checked).
  GetLabel().Rotate(1, NUM_NOPS);
  for (int i = 1; i < num_neighbors; i++) {
    cOrganism* neighbor = m_organism->GetNeighbor();
    
    if (neighbor != NULL && neighbor->GetHardware().FindLabelFull(GetLabel()).InMemory()) return true;
    
    // Otherwise keep rotating...
    m_organism->Rotate(1);
  }
  return true;
}

bool cHardwareCPU::Inst_RotateLeftOne(cAvidaContext& ctx)
{
  m_organism->Rotate(-1);
  return true;
}

bool cHardwareCPU::Inst_RotateRightOne(cAvidaContext& ctx)
{
  m_organism->Rotate(1);
  return true;
}

/**
 Rotate to facing specified by following label
 */
bool cHardwareCPU::Inst_RotateLabel(cAvidaContext& ctx)
{
  int standardNeighborhoodSize, actualNeighborhoodSize, newFacing, currentFacing;
  actualNeighborhoodSize = m_organism->GetNeighborhoodSize();
  
  ReadLabel();
  if(m_world->GetConfig().WORLD_GEOMETRY.Get() == nGeometry::TORUS || m_world->GetConfig().WORLD_GEOMETRY.Get() == nGeometry::GRID)
    standardNeighborhoodSize = 8;
  else {
    exit(-1);
  }
  newFacing = GetLabel().AsIntGreyCode(NUM_NOPS) % standardNeighborhoodSize;
  
  for(int i = 0; i < actualNeighborhoodSize; i++) {
    currentFacing = m_organism->GetFacing();
    if(newFacing == currentFacing)
      break;
    m_organism->Rotate(1);
  }
  return true;
}

bool cHardwareCPU::Inst_RotateUnoccupiedCell(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);
  
  for(int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if(!m_organism->IsNeighborCellOccupied()) // faced cell is unoccupied
    {
      GetRegister(reg_used) = 1;      
      return true;
    }
    m_organism->Rotate(1); // continue to rotate
  }  
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_RotateOccupiedCell(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);
  
  for(int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if(m_organism->IsNeighborCellOccupied()) { // faced cell is occupied
      GetRegister(reg_used) = 1;      
      return true;
    }
    m_organism->Rotate(1); // continue to rotate
  }  
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_RotateNextOccupiedCell(cAvidaContext& ctx) {
	m_organism->Rotate(1);
  return Inst_RotateOccupiedCell(ctx);
}

bool cHardwareCPU::Inst_RotateNextUnoccupiedCell(cAvidaContext& ctx) {
	m_organism->Rotate(1); // continue to rotate
  return Inst_RotateUnoccupiedCell(ctx);
}

bool cHardwareCPU::Inst_RotateEventCell(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);
  
  for(int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if(m_organism->GetCellData() > 0) { // event in faced cell
      GetRegister(reg_used) = 1;      
      return true;
    }
    m_organism->Rotate(1); // continue to rotate
  }  
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_SetCopyMut(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int new_mut_rate = Max(GetRegister(reg_used), 1 );
  m_organism->SetCopyMutProb(static_cast<double>(new_mut_rate) / 10000.0);
  return true;
}

bool cHardwareCPU::Inst_ModCopyMut(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const double new_mut_rate = m_organism->GetCopyMutProb() + static_cast<double>(GetRegister(reg_used)) / 10000.0;
  if (new_mut_rate > 0.0) m_organism->SetCopyMutProb(new_mut_rate);
  return true;
}

// @WRE addition for movement
// Tumble sets the organism and cell to a new random facing
// 
bool cHardwareCPU::Inst_Tumble(cAvidaContext& ctx)
{
  // Get number of neighbor cells that the organism can move to.
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  // Exclude extreme case of the completely disconnected cell
  if (0 < num_neighbors) {
    // Choose a base 0 random number of turns to make in facing, [0 .. num_neighbors-2].
    int irot = ctx.GetRandom().GetUInt(num_neighbors-1);
    // Treat as base 0 number of turns to make
    for (int i = 0; i <= irot; i++) {
      m_organism->Rotate(1);
    }
  }
  // Logging
  // ofstream tumblelog;
  // tumblelog.open("data/tumblelog.txt",ios::app);
  // tumblelog << organism->GetID() << "," << irot << endl;
  // tumblelog.close();
	
  return true;
}



bool cHardwareCPU::Inst_SGMove(cAvidaContext& ctx)
{
  assert(m_ext_mem.GetSize() > 3);
  
  const cStateGrid& sg = m_organism->GetStateGrid();
  
  int& x = m_ext_mem[0];
  int& y = m_ext_mem[1];
  
  const int facing = m_ext_mem[2];
  
  // State grid is treated as a 2-dimensional toroidal grid with size [0, width) and [0, height)
  switch (facing) {
    case 0: // N
      if (++y == sg.GetHeight()) y = 0;
      break;
      
    case 1: // NE
      if (++x == sg.GetWidth()) x = 0;
      if (++y == sg.GetHeight()) y = 0;
      break;
      
    case 2: // E
      if (++x == sg.GetWidth()) x = 0;
      break;

    case 3: // SE
      if (++x == sg.GetWidth()) x = 0;
      if (--y == -1) y = sg.GetHeight() - 1;
      break;
      
    case 4: // S
      if (--y == -1) y = sg.GetHeight() - 1;
      break;

    case 5: // SW
      if (--x == -1) x = sg.GetWidth() - 1;
      if (--y == -1) y = sg.GetHeight() - 1;
      break;
      
    case 6: // W
      if (--x == -1) x = sg.GetWidth() - 1;
      break;

    case 7: // NW
      if (--x == -1) x = sg.GetWidth() - 1;
      if (++y == sg.GetHeight()) y = 0;
      break;
            
    default:
      assert(facing >= 0 && facing <= 7);
  }
  
  // Increment state observed count
  m_ext_mem[3 + sg.GetStateAt(x, y)]++;
  
  // Save this location in the movement history
  m_ext_mem.Push(sg.GetIDFor(x, y));
  return true;
}

bool cHardwareCPU::Inst_SGRotateL(cAvidaContext& ctx)
{
  assert(m_ext_mem.GetSize() > 3);
  if (--m_ext_mem[2] < 0) m_ext_mem[2] = 7;
  return true;
}

bool cHardwareCPU::Inst_SGRotateR(cAvidaContext& ctx)
{
  assert(m_ext_mem.GetSize() > 3);
  if (++m_ext_mem[2] > 7) m_ext_mem[2] = 0;
  return true;
}

bool cHardwareCPU::Inst_SGSense(cAvidaContext& ctx)
{
  const cStateGrid& sg = m_organism->GetStateGrid();
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = sg.SenseStateAt(m_ext_mem[0], m_ext_mem[1]);
  return true; 
}

// @WRE addition for movement
// Move uses the cPopulation::SwapCells method to move an m_organism to a different cell
// and the cPopulation::MoveOrganisms helper function to clean up after a move
// The cell selected as a destination is the one faced
bool cHardwareCPU::Inst_Move(cAvidaContext& ctx)
{
  // Declarations
  int fromcellID, destcellID; //, actualNeighborhoodSize, fromFacing, destFacing, currentFacing;
	
  fromcellID = m_organism->GetCellID(); //absolute id of current cell
	
  if(fromcellID == -1) {
    return false;
  }
	
  fromcellID = m_organism->GetCellID(); //absolute id of current cell
	
  if(fromcellID == -1) {
    return false;
  }
	
  // Get population
  cPopulation& pop = m_world->GetPopulation();
  cDeme &deme = pop.GetDeme(pop.GetCell(m_organism->GetCellID()).GetDemeID());
  
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Pheromone stuff
  double pher_amount = 0;
  int drop_mode = -1;
	
  // Code
  if (0 < stepsize) {
    // Current cell
    //fromcellID = organism->GetCellID();
    // With sanity check
    if (-1  == fromcellID) return false;
    // Destination cell
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
    
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/    
    
    // Actually perform the move using SwapCells
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    // Swap inputs and facings between cells using helper function
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    // updates movement predicates
    m_world->GetStats().Move(*m_organism);
		
    // If organism is dropping pheromones, mark the appropriate cell(s)
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

       // Old CellData-based version
       //const int newval = pop.GetCell(destcellID).GetCellData() + 1;
       //pop.GetCell(destcellID).SetCellData(newval);

    } //End laying pheromone
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().MOVETARGET_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,5",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
		
    // check tasks.  general movement tasks are not yet supported.
    //organism->DoOutput(ctx);
		
    // Brian movement
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
}

bool cHardwareCPU::Inst_MoveToEvent(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);
  int orginalFacing = m_organism->GetFacing();
  
  for(int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if(m_organism->GetNeighborCellContents() > 0) { 
      Inst_Move(ctx);
      GetRegister(reg_used) = 1;
      return true;
    }
    m_organism->Rotate(1);
  }
  assert(m_organism->GetFacing() == orginalFacing);
  Inst_Move(ctx);
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_IfNeighborEventInUnoccupiedCell(cAvidaContext& ctx) {
  int orginalFacing = m_organism->GetFacing();
  
  for(int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if(m_organism->GetNeighborCellContents() > 0 && !m_organism->IsNeighborCellOccupied()) { 
      return true;
    }
    m_organism->Rotate(1);
  }
  assert(m_organism->GetFacing() == orginalFacing);
  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfFacingEventCell(cAvidaContext& ctx) {
  if(m_organism->GetNeighborCellContents() > 0) { 
		return true;
  }
  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfEventInCell(cAvidaContext& ctx) {
  if(m_organism->GetCellData() > 0) { 
		return true;
  }
  IP().Advance();
  return true;
}

// Multi-threading.
bool cHardwareCPU::Inst_ForkThread(cAvidaContext& ctx)
{
  IP().Advance();
  if (!ForkThread()) m_organism->Fault(FAULT_LOC_THREAD_FORK, FAULT_TYPE_FORK_TH);
  return true;
}

bool cHardwareCPU::Inst_ForkThreadLabel(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  // If there is no label, then do normal fork behavior
  if (GetLabel().GetSize() == 0)
  {
    return Inst_ForkThread(ctx);
  }
  
  cHeadCPU searchHead = FindLabel(+1);
  if ( searchHead.GetPosition() != IP().GetPosition() )
  {
    int save_pos = IP().GetPosition();
    IP().Set(searchHead.GetPosition() + 1);
    if (!ForkThread()) m_organism->Fault(FAULT_LOC_THREAD_FORK, FAULT_TYPE_FORK_TH);
    IP().Set( save_pos );
  }
  
  return true;
}

bool cHardwareCPU::Inst_ForkThreadLabelIfNot0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) == 0) 
  {
    ReadLabel();
    return false;
  }
  return Inst_ForkThreadLabel(ctx);
}

bool cHardwareCPU::Inst_ForkThreadLabelIf0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) != 0)
  {
    ReadLabel();
    return false;
  }
  return Inst_ForkThreadLabel(ctx);
}

bool cHardwareCPU::Inst_KillThread(cAvidaContext& ctx)
{
  if (!KillThread()) m_organism->Fault(FAULT_LOC_THREAD_KILL, FAULT_TYPE_KILL_TH);
  else m_advance_ip = false;
  return true;
}

bool cHardwareCPU::Inst_ThreadID(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = GetCurThreadID();
  return true;
}


// Head-based instructions

bool cHardwareCPU::Inst_SetHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  m_threads[m_cur_thread].cur_head = static_cast<unsigned char>(head_used);
  return true;
}

bool cHardwareCPU::Inst_AdvanceHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_WRITE);
  GetHead(head_used).Advance();
  return true;
}

bool cHardwareCPU::Inst_MoveHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  const int target = nHardware::HEAD_FLOW;
  GetHead(head_used).Set(GetHead(target));
  if (head_used == nHardware::HEAD_IP) m_advance_ip = false;
  return true;
}

bool cHardwareCPU::Inst_JumpHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  GetHead(head_used).Jump(GetRegister(REG_CX) );
  // @JEB - probably shouldn't advance IP after jumping here?
  // Any negative number jumps to the beginning of the genome (pos 0)
  // and then we immediately advance past that first instruction.
  return true;
}

bool cHardwareCPU::Inst_GetHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  GetRegister(REG_CX) = GetHead(head_used).GetPosition();
  return true;
}

bool cHardwareCPU::Inst_IfLabel(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  if (GetLabel() != GetReadLabel())  IP().Advance();
  return true;
}

// This is a variation on IfLabel that will skip the next command if the "if"
// is false, but it will also skip all nops following that command.
bool cHardwareCPU::Inst_IfLabel2(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  if (GetLabel() != GetReadLabel()) {
    IP().Advance();
    if (m_inst_set->IsNop( IP().GetNextInst() ))  IP().Advance();
  }
  return true;
}

bool cHardwareCPU::Inst_HeadDivideMut(cAvidaContext& ctx, double mut_multiplier)
{
  AdjustHeads();
  const int divide_pos = GetHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  GetHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = m_memory.GetSize();
  const int extra_lines = m_memory.GetSize() - child_end;
  bool ret_val = Divide_Main(ctx, divide_pos, extra_lines, mut_multiplier);
  // Re-adjust heads.
  AdjustHeads();
  return ret_val; 
}

bool cHardwareCPU::Inst_HeadDivide(cAvidaContext& ctx)
{
  return Inst_HeadDivideMut(ctx, 1);
  
}

/*
 Resample Divide -- AWC 06/29/06
 */

bool cHardwareCPU::Inst_HeadDivideRS(cAvidaContext& ctx)
{
  AdjustHeads();
  const int divide_pos = GetHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  GetHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = m_memory.GetSize();
  const int extra_lines = m_memory.GetSize() - child_end;
  bool ret_val = Divide_MainRS(ctx, divide_pos, extra_lines, 1);
  // Re-adjust heads.
  AdjustHeads();
  return ret_val; 
}

/*
 Resample Divide -- single mut on divide-- AWC 07/28/06
 */

bool cHardwareCPU::Inst_HeadDivide1RS(cAvidaContext& ctx)
{
  AdjustHeads();
  const int divide_pos = GetHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  GetHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = m_memory.GetSize();
  const int extra_lines = m_memory.GetSize() - child_end;
  bool ret_val = Divide_Main1RS(ctx, divide_pos, extra_lines, 1);
  // Re-adjust heads.
  AdjustHeads();
  return ret_val; 
}

/*
 Resample Divide -- double mut on divide-- AWC 08/29/06
 */

bool cHardwareCPU::Inst_HeadDivide2RS(cAvidaContext& ctx)
{
  AdjustHeads();
  const int divide_pos = GetHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  GetHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = m_memory.GetSize();
  const int extra_lines = m_memory.GetSize() - child_end;
  bool ret_val = Divide_Main2RS(ctx, divide_pos, extra_lines, 1);
  // Re-adjust heads.
  AdjustHeads();
  return ret_val; 
}


bool cHardwareCPU::Inst_HeadDivideSex(cAvidaContext& ctx)  
{ 
  m_organism->GetPhenotype().SetDivideSex(true);
  m_organism->GetPhenotype().SetCrossNum(1);
  return Inst_HeadDivide(ctx); 
}

bool cHardwareCPU::Inst_HeadDivideAsex(cAvidaContext& ctx)  
{ 
  m_organism->GetPhenotype().SetDivideSex(false);
  m_organism->GetPhenotype().SetCrossNum(0);
  return Inst_HeadDivide(ctx); 
}

bool cHardwareCPU::Inst_HeadDivideAsexWait(cAvidaContext& ctx)  
{ 
  m_organism->GetPhenotype().SetDivideSex(true);
  m_organism->GetPhenotype().SetCrossNum(0);
  return Inst_HeadDivide(ctx); 
}

bool cHardwareCPU::Inst_HeadDivideMateSelect(cAvidaContext& ctx)  
{ 
  // Take the label that follows this divide and use it as the ID for which
  // other organisms this one is willing to mate with.
  ReadLabel();
  m_organism->GetPhenotype().SetMateSelectID( GetLabel().AsInt(NUM_NOPS) );
  
  // Proceed as normal with the rest of mate selection.
  m_organism->GetPhenotype().SetDivideSex(true);
  m_organism->GetPhenotype().SetCrossNum(1);
  return Inst_HeadDivide(ctx); 
}

bool cHardwareCPU::Inst_HeadDivide1(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 1); }
bool cHardwareCPU::Inst_HeadDivide2(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 2); }
bool cHardwareCPU::Inst_HeadDivide3(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 3); }
bool cHardwareCPU::Inst_HeadDivide4(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 4); }
bool cHardwareCPU::Inst_HeadDivide5(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 5); }
bool cHardwareCPU::Inst_HeadDivide6(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 6); }
bool cHardwareCPU::Inst_HeadDivide7(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 7); }
bool cHardwareCPU::Inst_HeadDivide8(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 8); }
bool cHardwareCPU::Inst_HeadDivide9(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 9); }
bool cHardwareCPU::Inst_HeadDivide10(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 10); }
bool cHardwareCPU::Inst_HeadDivide16(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 16); }
bool cHardwareCPU::Inst_HeadDivide32(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 32); }
bool cHardwareCPU::Inst_HeadDivide50(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 50); }
bool cHardwareCPU::Inst_HeadDivide100(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 100); }
bool cHardwareCPU::Inst_HeadDivide500(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 500); }
bool cHardwareCPU::Inst_HeadDivide1000(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 1000); }
bool cHardwareCPU::Inst_HeadDivide5000(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 5000); }
bool cHardwareCPU::Inst_HeadDivide10000(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 10000); }
bool cHardwareCPU::Inst_HeadDivide50000(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 50000); }
bool cHardwareCPU::Inst_HeadDivide0_5(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.5); }
bool cHardwareCPU::Inst_HeadDivide0_1(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.1); }
bool cHardwareCPU::Inst_HeadDivide0_05(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.05); }
bool cHardwareCPU::Inst_HeadDivide0_01(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.01); }
bool cHardwareCPU::Inst_HeadDivide0_001(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.001); }

bool cHardwareCPU::Inst_HeadRead(cAvidaContext& ctx)
{
  const int dst = REG_BX;
  
  const int head_id = FindModifiedHead(nHardware::HEAD_READ);
  GetHead(head_id).Adjust();
  
  // Mutations only occur on the read, for the moment.
  int read_inst = 0;
  if (m_organism->TestCopyMut(ctx)) {
    read_inst = m_inst_set->GetRandomInst(ctx).GetOp();
  } else {
    read_inst = GetHead(head_id).GetInst().GetOp();
  }
  GetRegister(dst) = read_inst;
  ReadInst(read_inst);
  
  if (m_slip_read_head && m_organism->TestCopySlip(ctx))
    GetHead(head_id).Set(ctx.GetRandom().GetInt(GetHead(head_id).GetMemory().GetSize()));

  GetHead(head_id).Advance();
  return true;
}

bool cHardwareCPU::Inst_HeadWrite(cAvidaContext& ctx)
{
  const int src = REG_BX;
  const int head_id = FindModifiedHead(nHardware::HEAD_WRITE);
  cHeadCPU& active_head = GetHead(head_id);
  
  active_head.Adjust();
  
  int value = GetRegister(src);
  if (value < 0 || value >= m_inst_set->GetSize()) value = 0;
  
  active_head.SetInst(cInstruction(value));
  active_head.SetFlagCopied();
  
  if (m_organism->TestCopyIns(ctx)) active_head.InsertInst(m_inst_set->GetRandomInst(ctx));
  if (m_organism->TestCopyDel(ctx)) active_head.RemoveInst();
  if (m_organism->TestCopyUniform(ctx)) doUniformCopyMutation(ctx, active_head);
  if (!m_slip_read_head && m_organism->TestCopySlip(ctx)) 
    doSlipMutation(ctx, active_head.GetMemory(), active_head.GetPosition());

  // Advance the head after write...
  active_head.Advance();
  
  return true;
}

bool cHardwareCPU::Inst_HeadCopy(cAvidaContext& ctx)
{
  // For the moment, this cannot be nop-modified.
  cHeadCPU& read_head = GetHead(nHardware::HEAD_READ);
  cHeadCPU& write_head = GetHead(nHardware::HEAD_WRITE);
  
  read_head.Adjust();
  write_head.Adjust();
  
  // Do mutations.
  cInstruction read_inst = read_head.GetInst();
  ReadInst(read_inst.GetOp());
  
  if (m_organism->TestCopyMut(ctx)) {
    read_inst = m_inst_set->GetRandomInst(ctx);
    write_head.SetFlagMutated();
    write_head.SetFlagCopyMut();
  }
  write_head.SetInst(read_inst);
  write_head.SetFlagCopied();  // Set the copied flag...
  
  if (m_organism->TestCopyIns(ctx)) write_head.InsertInst(m_inst_set->GetRandomInst(ctx));
  if (m_organism->TestCopyDel(ctx)) write_head.RemoveInst();
  if (m_organism->TestCopyUniform(ctx)) doUniformCopyMutation(ctx, write_head);
  if (m_organism->TestCopySlip(ctx)) {
    if (m_slip_read_head) {
      read_head.Set(ctx.GetRandom().GetInt(read_head.GetMemory().GetSize()));
    } else 
      doSlipMutation(ctx, write_head.GetMemory(), write_head.GetPosition());
  }
  
  read_head.Advance();
  write_head.Advance();
  return true;
}

bool cHardwareCPU::HeadCopy_ErrorCorrect(cAvidaContext& ctx, double reduction)
{
  // For the moment, this cannot be nop-modified.
  cHeadCPU & read_head = GetHead(nHardware::HEAD_READ);
  cHeadCPU & write_head = GetHead(nHardware::HEAD_WRITE);
  
  read_head.Adjust();
  write_head.Adjust();
  
  // Do mutations.
  cInstruction read_inst = read_head.GetInst();
  ReadInst(read_inst.GetOp());
  if ( ctx.GetRandom().P(m_organism->GetCopyMutProb() / reduction) ) {
    read_inst = m_inst_set->GetRandomInst(ctx);
    write_head.SetFlagMutated();
    write_head.SetFlagCopyMut();
  }
  
  write_head.SetInst(read_inst);
  write_head.SetFlagCopied();  // Set the copied flag...
  
  if (ctx.GetRandom().P(m_organism->GetCopyInsProb() / reduction)) write_head.InsertInst(m_inst_set->GetRandomInst(ctx));
  if (ctx.GetRandom().P(m_organism->GetCopyDelProb() / reduction)) write_head.RemoveInst();
  if (ctx.GetRandom().P(m_organism->GetCopyUniformProb() / reduction)) doUniformCopyMutation(ctx, write_head);
  if (ctx.GetRandom().P(m_organism->GetCopySlipProb() / reduction)) {
    if (m_slip_read_head) {
      read_head.Set(ctx.GetRandom().GetInt(read_head.GetMemory().GetSize()));
    } else 
      doSlipMutation(ctx, write_head.GetMemory(), write_head.GetPosition());
  }
  
  read_head.Advance();
  write_head.Advance();
  return true;
}

bool cHardwareCPU::Inst_HeadCopy2(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 2); }
bool cHardwareCPU::Inst_HeadCopy3(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 3); }
bool cHardwareCPU::Inst_HeadCopy4(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 4); }
bool cHardwareCPU::Inst_HeadCopy5(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 5); }
bool cHardwareCPU::Inst_HeadCopy6(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 6); }
bool cHardwareCPU::Inst_HeadCopy7(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 7); }
bool cHardwareCPU::Inst_HeadCopy8(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 8); }
bool cHardwareCPU::Inst_HeadCopy9(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 9); }
bool cHardwareCPU::Inst_HeadCopy10(cAvidaContext& ctx) { return HeadCopy_ErrorCorrect(ctx, 10); }

bool cHardwareCPU::Inst_HeadSearch(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  cHeadCPU found_pos = FindLabel(0);
  const int search_size = found_pos.GetPosition() - IP().GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  GetHead(nHardware::HEAD_FLOW).Set(found_pos);
  GetHead(nHardware::HEAD_FLOW).Advance();
  return true; 
}

bool cHardwareCPU::Inst_SetFlow(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  GetHead(nHardware::HEAD_FLOW).Set(GetRegister(reg_used));
  return true; 
}


/* Execute the next instruction if the organism's energy level is low */
bool cHardwareCPU::Inst_IfEnergyLow(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  // Note: these instructions should probably also make sure the returned energy level is not -1.
  if(m_organism->GetPhenotype().GetDiscreteEnergyLevel() != cPhenotype::ENERGY_LEVEL_LOW) {
    IP().Advance();
  }
	
  return true;
	
} //End Inst_IfEnergyLow()


/* Execute the next instruction if the organism's energy level is not low */
bool cHardwareCPU::Inst_IfEnergyNotLow(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  if(m_organism->GetPhenotype().GetDiscreteEnergyLevel() == cPhenotype::ENERGY_LEVEL_LOW) {
    IP().Advance();
  }
	
  return true;
	
} //End Inst_IfEnergyNotLow()


/* Execute the next instruction if the faced organism's energy level is low */
bool cHardwareCPU::Inst_IfFacedEnergyLow(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  if ( (neighbor != NULL) && (!neighbor->IsDead()) ){
    // Note: these instructions should probably also make sure the returned energy level is not -1.
    if(neighbor->GetPhenotype().GetDiscreteEnergyLevel() != cPhenotype::ENERGY_LEVEL_LOW) {
      IP().Advance();
    }    
  }  
	
  return true;
	
} //End Inst_IfFacedEnergyLow()


/* Execute the next instruction if the faced organism's energy level is low */
bool cHardwareCPU::Inst_IfFacedEnergyNotLow(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  if ( (neighbor != NULL) && (!neighbor->IsDead()) ) {
    // Note: these instructions should probably also make sure the returned energy level is not -1.
    if(neighbor->GetPhenotype().GetDiscreteEnergyLevel() == cPhenotype::ENERGY_LEVEL_LOW) {
      IP().Advance();
    }    
  }  
	
  return true;
	
} //End Inst_IfFacedEnergyNotLow()


/* Execute the next instruction if the organism's energy level is high */
bool cHardwareCPU::Inst_IfEnergyHigh(cAvidaContext& ctx) {
	
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  if(m_organism->GetPhenotype().GetDiscreteEnergyLevel() != cPhenotype::ENERGY_LEVEL_HIGH) {
    IP().Advance();
  }
	
  return true;
	
} //End Inst_IfEnergyHigh()


/* Execute the next instruction if the organism's energy level is not high */
bool cHardwareCPU::Inst_IfEnergyNotHigh(cAvidaContext& ctx) {

  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  if(m_organism->GetPhenotype().GetDiscreteEnergyLevel() == cPhenotype::ENERGY_LEVEL_HIGH) {
    IP().Advance();
  }
	
  return true;
	
} //End Inst_IfEnergyNotHigh()


/* Execute the next instruction if the faced organism's energy level is high */
bool cHardwareCPU::Inst_IfFacedEnergyHigh(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  if ( (neighbor != NULL) && (!neighbor->IsDead()) ) {
    // Note: these instructions should probably also make sure the returned energy level is not -1.
    if(neighbor->GetPhenotype().GetDiscreteEnergyLevel() != cPhenotype::ENERGY_LEVEL_HIGH) {
      IP().Advance();
    }    
  }  
	
  return true;
	
} //End Inst_IfFacedEnergyHigh()


/* Execute the next instruction if the faced organism's energy level is not high */
bool cHardwareCPU::Inst_IfFacedEnergyNotHigh(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  if ( (neighbor != NULL) && (!neighbor->IsDead()) ) {
    // Note: these instructions should probably also make sure the returned energy level is not -1.
    if(neighbor->GetPhenotype().GetDiscreteEnergyLevel() == cPhenotype::ENERGY_LEVEL_HIGH) {
      IP().Advance();
    }    
  }  
	
  return true;
	
} //End Inst_IfFacedEnergyNotHigh()


/* Execute the next instruction if the organism's energy level is medium */
bool cHardwareCPU::Inst_IfEnergyMed(cAvidaContext& ctx) {

  if(m_organism->GetCellID() < 0) {
    return false;
  }	

  if(m_organism->GetPhenotype().GetDiscreteEnergyLevel() != cPhenotype::ENERGY_LEVEL_MEDIUM) {
    IP().Advance();
  }
		
  return true;
	
} //End Inst_IfEnergyMed()


/* Execute the next instruction if the faced organism's energy level is medium */
bool cHardwareCPU::Inst_IfFacedEnergyMed(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  if ( (neighbor != NULL) && (!neighbor->IsDead()) ) {
    // Note: these instructions should probably also make sure the returned energy level is not -1.
    if(neighbor->GetPhenotype().GetDiscreteEnergyLevel() != cPhenotype::ENERGY_LEVEL_MEDIUM) {
      IP().Advance();
    }    
  }  
	
  return true;
	
} //End Inst_IfFacedEnergyMed()


/* Execute the next instruction if the faced organism has less energy */
bool cHardwareCPU::Inst_IfFacedEnergyLess(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  if ( (neighbor != NULL) && (!neighbor->IsDead()) ) {
    const double neighbor_energy = neighbor->GetPhenotype().GetStoredEnergy();
    const double my_energy = m_organism->GetPhenotype().GetStoredEnergy();
    const double epsilon = m_world->GetConfig().ENERGY_COMPARISON_EPSILON.Get();
    
    if(neighbor_energy >= (my_energy * (1 - epsilon))) {
      IP().Advance();
    }    
  }  
	
  return true;
	
} //End Inst_IfFacedEnergyLess()


/* Execute the next instruction if the faced organism has more energy */
bool cHardwareCPU::Inst_IfFacedEnergyMore(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
	
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  if ( (neighbor != NULL) && (!neighbor->IsDead()) ) {
    const double neighbor_energy = neighbor->GetPhenotype().GetStoredEnergy();
    const double my_energy = m_organism->GetPhenotype().GetStoredEnergy();
    const double epsilon = m_world->GetConfig().ENERGY_COMPARISON_EPSILON.Get();

    if(neighbor_energy <= (my_energy * (1 + epsilon))) {
      IP().Advance();
    }    
  }  
	
  return true;
	
} //End Inst_IfFacedEnergyMore()


/* Execute the next instruction if the organism has received energy */
bool cHardwareCPU::Inst_IfEnergyInBuffer(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
  
  if(m_organism->GetPhenotype().GetEnergyInBufferAmount() == 0) {
    IP().Advance();
  }
  
  return true;
	
} //End Inst_IfEnergyInBuffer()


/* Execute the next instruction if the organism has not received energy */
bool cHardwareCPU::Inst_IfEnergyNotInBuffer(cAvidaContext& ctx) {
  
  if(m_organism->GetCellID() < 0) {
    return false;
  }	
  
  if(m_organism->GetPhenotype().GetEnergyInBufferAmount() > 0) {
    IP().Advance();
  }
  
  return true;
	
} //End Inst_IfEnergyNotInBuffer()


bool cHardwareCPU::Inst_Sleep(cAvidaContext& ctx) {
  cPopulation& pop = m_world->GetPopulation();
  int cellID = m_organism->GetCellID();
  // Fail if we're running in the test CPU.
  if(cellID < 0) return false;
	
  if(m_world->GetConfig().LOG_SLEEP_TIMES.Get() == 1) {
    pop.AddEndSleep(cellID, m_world->GetStats().GetUpdate());
  }
  m_organism->SetSleeping(false);  //this instruction get executed at the end of a sleep cycle
  GetOrganism()->GetOrgInterface().GetDeme()->DecSleepingCount();
  if(m_world->GetConfig().APPLY_ENERGY_METHOD.Get() == 2) {
    m_organism->GetPhenotype().RefreshEnergy();
    m_organism->GetPhenotype().ApplyToEnergyStore();
    double newMerit = cMerit::EnergyToMerit(m_organism->GetPhenotype().GetStoredEnergy() * m_organism->GetPhenotype().GetEnergyUsageRatio(), m_world);
    pop.UpdateMerit(cellID, newMerit);
  }
  return true;
}

bool cHardwareCPU::Inst_GetUpdate(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = m_world->GetStats().GetUpdate();
  return true;
}


/*! This method places the calling organism's x-y coordinates in ?BX? and ?++BX?.
 
 Note that this method *will not work* from within the test CPU, so we have to guard
 against that.
 */
bool cHardwareCPU::Inst_GetCellPosition(cAvidaContext& ctx) {
  int absolute_cell_ID = m_organism->GetCellID();
  int deme_id = m_organism->GetOrgInterface().GetDemeID();
  // Fail if we're running in the test CPU.
  if((deme_id < 0) || (absolute_cell_ID < 0)) return false;
  
  std::pair<int, int> pos = m_world->GetPopulation().GetDeme(deme_id).GetCellPosition(absolute_cell_ID);  
  const int xreg = FindModifiedRegister(REG_BX);
  const int yreg = FindNextRegister(xreg);
  GetRegister(xreg) = pos.first;
  GetRegister(yreg) = pos.second;
  return true;
}

/*! This method places the calling organism's x coordinate in ?BX?.
 
 Note that this method *will not work* from within the test CPU, so we have to guard
 against that.
 */
bool cHardwareCPU::Inst_GetCellPositionX(cAvidaContext& ctx) {
  int absolute_cell_ID = m_organism->GetCellID();
  int deme_id = m_organism->GetOrgInterface().GetDemeID();
  // Fail if we're running in the test CPU.
  if((deme_id < 0) || (absolute_cell_ID < 0)) return false;
  
  std::pair<int, int> pos = m_world->GetPopulation().GetDeme(deme_id).GetCellPosition(absolute_cell_ID);  
  const int xreg = FindModifiedRegister(REG_BX);
  GetRegister(xreg) = pos.first;
  return true;
}

/*! This method places the calling organism's y coordinates in ?BX?.
 
 Note that this method *will not work* from within the test CPU, so we have to guard
 against that.
 */
bool cHardwareCPU::Inst_GetCellPositionY(cAvidaContext& ctx) {
  int absolute_cell_ID = m_organism->GetCellID();
  int deme_id = m_organism->GetOrgInterface().GetDemeID();
  // Fail if we're running in the test CPU.
  if((deme_id < 0) || (absolute_cell_ID < 0)) return false;
  
  std::pair<int, int> pos = m_world->GetPopulation().GetDeme(deme_id).GetCellPosition(absolute_cell_ID);  
  const int yreg = FindModifiedRegister(REG_BX);
  GetRegister(yreg) = pos.second;
  return true;
}

bool cHardwareCPU::Inst_GetDistanceFromDiagonal(cAvidaContext& ctx) {
  int absolute_cell_ID = m_organism->GetOrgInterface().GetCellID();
  int deme_id = m_organism->GetOrgInterface().GetDemeID();
  // Fail if we're running in the test CPU.
  if((deme_id < 0) || (absolute_cell_ID < 0)) return false;
  
  std::pair<int, int> pos = m_world->GetPopulation().GetDeme(deme_id).GetCellPosition(absolute_cell_ID);  
  const int reg = FindModifiedRegister(REG_BX);
  
  if(pos.first > pos.second) {
    GetRegister(reg) = (int)ceil((pos.first - pos.second)/2.0);
  } else {
    GetRegister(reg) = (int)floor((pos.first - pos.second)/2.0);
  }
	//  std::cerr<<"x = "<<pos.first<<"  y = "<<pos.second<<"  ans = "<<GetRegister(reg)<<std::endl;
  return true;
}

//// Promoter Model ////

bool cHardwareCPU::Inst_Promoter(cAvidaContext& ctx)
{
  // Promoters don't do anything themselves
  return true;
}

// Move the instruction ptr to the next active promoter
bool cHardwareCPU::Inst_Terminate(cAvidaContext& ctx)
{
  // Optionally,
  // Reset the thread.
  if (m_world->GetConfig().TERMINATION_RESETS.Get())
  {
    //const int write_head_pos = GetHead(nHardware::HEAD_WRITE).GetPosition();
    //const int read_head_pos = GetHead(nHardware::HEAD_READ).GetPosition();
    m_threads[m_cur_thread].Reset(m_world, this, m_threads[m_cur_thread].GetID());
    //GetHead(nHardware::HEAD_WRITE).Set(write_head_pos);
    //GetHead(nHardware::HEAD_READ).Set(read_head_pos);
    
    //Setting this makes it harder to do things. You have to be modular.
    m_organism->GetOrgInterface().ResetInputs(ctx);   // Re-randomize the inputs this organism sees
    m_organism->ClearInput();                         // Also clear their input buffers, or they can still claim
		// rewards for numbers no longer in their environment!
  }
  
  // Reset our count
  m_threads[m_cur_thread].ResetPromoterInstExecuted();
  m_advance_ip = false;
  const int reg_used = REG_BX; // register to put chosen promoter code in, for now always BX
	
  // Search for an active promoter  
  int start_offset = m_promoter_offset;
  int start_index  = m_promoter_index;
  
  bool no_promoter_found = true;
  if ( m_promoters.GetSize() > 0 ) 
  {
    while( true )
    {
      // If the next promoter is active, then break out
      NextPromoter();
      if (IsActivePromoter()) 
      {
        no_promoter_found = false;
        break;
      }
      
      // If we just checked the promoter that we were originally on, then there
      // are no active promoters.
      if ( (start_offset == m_promoter_offset) && (start_index == m_promoter_index) ) break;
			
      // If we originally were not on a promoter, then stop once we check the
      // first promoter and an offset of zero
      if (start_index == -1)
      {
				start_index = 0;
      }
    } 
  }
  
  if (no_promoter_found)
  {
    if ((m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() == 0) || (m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() == 2))
    {
      // Set defaults for when no active promoter is found
      m_promoter_index = -1;
      IP().Set(0);
      GetRegister(reg_used) = 0;
    }
    // Death to organisms that refuse to use promoters!
    else if (m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() == 1)
    {
      m_organism->Die();
    }
    else
    {
      cout << "Unrecognized NO_ACTIVE_PROMOTER_EFFECT setting: " << m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() << endl;
    }
  }
  else
  {
    // We found an active match, offset to just after it.
    // cHeadCPU will do the mod genome size for us
    IP().Set(m_promoters[m_promoter_index].m_pos + 1);
    
    // Put its bit code in BX for the organism to have if option is set
    if ( m_world->GetConfig().PROMOTER_TO_REGISTER.Get() )
    {
      GetRegister(reg_used) = m_promoters[m_promoter_index].m_bit_code;
    }
  }
  return true;
}

// Set a new regulation code (which is XOR'ed with ALL promoter codes).
bool cHardwareCPU::Inst_Regulate(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  int regulation_code = GetRegister(reg_used);
	
  for (int i=0; i< m_promoters.GetSize();i++)
  {
    m_promoters[i].m_regulation = regulation_code;
  }
  return true;
}

// Set a new regulation code, but only on a subset of promoters.
bool cHardwareCPU::Inst_RegulateSpecificPromoters(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  int regulation_code = GetRegister(reg_used);
  
  const int reg_promoter = FindModifiedRegister((reg_used+1) % NUM_REGISTERS);
  int regulation_promoter = GetRegister(reg_promoter);
  
  for (int i=0; i< m_promoters.GetSize();i++)
  {
    //Look for consensus bit matches over the length of the promoter code
    int test_p_code = m_promoters[i].m_bit_code;    
    int test_r_code = regulation_promoter;
    int bit_count = 0;
    for (int j=0; j<m_world->GetConfig().PROMOTER_EXE_LENGTH.Get();j++)
    {      
      if ((test_p_code & 1) == (test_r_code & 1)) bit_count++;
      test_p_code >>= 1;
      test_r_code >>= 1;
    }
    if (bit_count >= m_world->GetConfig().PROMOTER_EXE_LENGTH.Get() / 2)
    {
      m_promoters[i].m_regulation = regulation_code;
    }
  }
  return true;
}


bool cHardwareCPU::Inst_SenseRegulate(cAvidaContext& ctx)
{
  unsigned int bits = 0;
  const tArray<double> & res_count = m_organism->GetOrgInterface().GetResources();
  assert (res_count.GetSize() != 0);
  for (int i=0; i<m_world->GetConfig().PROMOTER_CODE_SIZE.Get(); i++)
  {
    int b = i % res_count.GetSize();
    bits <<= 1;
    bits += (res_count[b] != 0);
  }  
  
  for (int i=0; i< m_promoters.GetSize();i++)
  {
    m_promoters[i].m_regulation = bits;
  }
  return true;
}

// Create a number from inst bit codes
bool cHardwareCPU::Do_Numberate(cAvidaContext& ctx, int num_bits)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  // advance the IP now, so that it rests on the beginning of our number
  IP().Advance();
  m_advance_ip = false;
  
  int num = Numberate(IP().GetPosition(), +1, num_bits);
  GetRegister(reg_used) = num;
  return true;
}

// Move to the next promoter.
void cHardwareCPU::NextPromoter()
{
  // Move promoter index, rolling over if necessary
  m_promoter_index++;
  if (m_promoter_index == m_promoters.GetSize())
  {
    m_promoter_index = 0;
    
    // Move offset, rolling over when there are not enough bits before we would have to wrap around left
    m_promoter_offset+=m_world->GetConfig().PROMOTER_EXE_LENGTH.Get();
    if (m_promoter_offset + m_world->GetConfig().PROMOTER_EXE_LENGTH.Get() > m_world->GetConfig().PROMOTER_CODE_SIZE.Get())
    {
      m_promoter_offset = 0;
    }
  }
}


// Check whether the current promoter is active.
bool cHardwareCPU::IsActivePromoter()
{
  assert( m_promoters.GetSize() != 0 );
  int count = 0;
  unsigned int code = m_promoters[m_promoter_index].GetRegulatedBitCode();
  for(int i=0; i<m_world->GetConfig().PROMOTER_EXE_LENGTH.Get(); i++)
  {
    int offset = m_promoter_offset + i;
    offset %= m_world->GetConfig().PROMOTER_CODE_SIZE.Get();
    int state = code >> offset;
    count += (state & 1);
  }
	
  return (count >= m_world->GetConfig().PROMOTER_EXE_THRESHOLD.Get());
}

// Construct a promoter bit code from instruction bit codes
int cHardwareCPU::Numberate(int _pos, int _dir, int _num_bits)
{  
  int code_size = 0;
  unsigned int code = 0;
  unsigned int max_bits = sizeof(code) * 8;
  assert(_num_bits <= (int)max_bits);
  if (_num_bits == 0) _num_bits = max_bits;
  
  // Enforce a boundary, sometimes -1 can be passed for _pos
  int j = _pos + m_memory.GetSize();
  j %= m_memory.GetSize();
  assert(j >=0);
  assert(j < m_memory.GetSize());
  while (code_size < _num_bits)
  {
    unsigned int inst_code = (unsigned int) GetInstSet().GetInstructionCode(m_memory[j]);
    // shift bits in, one by one ... excuse the counter variable pun
    for (int code_on = 0; (code_size < _num_bits) && (code_on < m_world->GetConfig().INST_CODE_LENGTH.Get()); code_on++)
    {
      if (_dir < 0)
      {
        code >>= 1; // shift first so we don't go one too far at the end
        code += (1 << (_num_bits - 1)) * (inst_code & 1);
        inst_code >>= 1; 
      }
      else
      {
        code <<= 1; // shift first so we don't go one too far at the end;        
        code += (inst_code >> (m_world->GetConfig().INST_CODE_LENGTH.Get() - 1)) & 1;
        inst_code <<= 1; 
      }
      code_size++;
    }
    
		// move back one inst
    j += m_memory.GetSize() + _dir;
    j %= m_memory.GetSize();
		
  }
  
  return code;
}


//// Copied from cHardwareExperimental -- @JEB
static const unsigned int CONSENSUS = (sizeof(int) * 8) / 2;
static const unsigned int CONSENSUS24 = 12;
static const unsigned int MASK24 = 0xFFFFFF;

inline unsigned int cHardwareCPU::BitCount(unsigned int value) const
{
  const unsigned int w = value - ((value >> 1) & 0x55555555);
  const unsigned int x = (w & 0x33333333) + ((w >> 2) & 0x33333333);
  const unsigned int bit_count = ((x + (x >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
  return bit_count;
}

bool cHardwareCPU::Inst_BitConsensus(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedNextRegister(reg_used);
  GetRegister(reg_used) = (BitCount(GetRegister(op1)) >= CONSENSUS) ? 1 : 0;
  return true; 
}

bool cHardwareCPU::Inst_BitConsensus24(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedNextRegister(reg_used);
  GetRegister(reg_used) = (BitCount(GetRegister(op1) & MASK24) >= CONSENSUS24) ? 1 : 0;
  return true; 
}

bool cHardwareCPU::Inst_IfConsensus(cAvidaContext& ctx)
{
  const int op1 = FindModifiedRegister(REG_BX);
  if (BitCount(GetRegister(op1)) <  CONSENSUS)  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfConsensus24(cAvidaContext& ctx)
{
  const int op1 = FindModifiedRegister(REG_BX);
  if (BitCount(GetRegister(op1) & MASK24) <  CONSENSUS24)  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLessConsensus(cAvidaContext& ctx)
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindModifiedNextRegister(op1);
  if (BitCount(GetRegister(op1)) >=  BitCount(GetRegister(op2)))  IP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLessConsensus24(cAvidaContext& ctx)
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindModifiedNextRegister(op1);
  if (BitCount(GetRegister(op1) & MASK24) >=  BitCount(GetRegister(op2) & MASK24))  IP().Advance();
  return true;
}

//// End copied from cHardwareExperimental

bool cHardwareCPU::Inst_SendMessage(cAvidaContext& ctx)
{
	return Inst_SendMessageBase(ctx);
}

// send message for type 0 to faced neighbor
bool cHardwareCPU::Inst_SendMessageType0(cAvidaContext& ctx) {
	return Inst_SendMessageBase(ctx, 0);
}

// send message for type 1 to faced neighbor
bool cHardwareCPU::Inst_SendMessageType1(cAvidaContext& ctx) {
	return Inst_SendMessageBase(ctx, 1);
}

// send message for type 2 to faced neighbor
bool cHardwareCPU::Inst_SendMessageType2(cAvidaContext& ctx) {
	return Inst_SendMessageBase(ctx, 2);
}

// send message for type 3 to faced neighbor
bool cHardwareCPU::Inst_SendMessageType3(cAvidaContext& ctx) {
	return Inst_SendMessageBase(ctx, 3);
}

// send message for type 4 to faced neighbor
bool cHardwareCPU::Inst_SendMessageType4(cAvidaContext& ctx) {
	return Inst_SendMessageBase(ctx, 4);
}

// send message for type 5 to faced neighbor
bool cHardwareCPU::Inst_SendMessageType5(cAvidaContext& ctx) {
	return Inst_SendMessageBase(ctx, 5);
}

// send message for type 6 to faced neighbor
bool cHardwareCPU::Inst_SendMessageType6(cAvidaContext& ctx) {
	return Inst_SendMessageBase(ctx, 6);
}

// send message for type 7 to faced neighbor
bool cHardwareCPU::Inst_SendMessageType7(cAvidaContext& ctx) {
	return Inst_SendMessageBase(ctx, 7);
}


/*! Send a message to the organism that is currently faced by this cell,
 where the label field of sent message is from register ?BX?, and the data field
 is from register ~?BX?.
 */
// If INTERRUPT_ENABLED then with function will cause the MSG receiver to jump inside its interrupt handler, 
//     and call Inst_RetrieveMessage which eats a NOP if one exists
/* basic message sending functionallity  */
bool cHardwareCPU::Inst_SendMessageBase(cAvidaContext& ctx, int messageType) {
	const int label_reg = FindModifiedRegister(REG_BX);
  const int data_reg = FindNextRegister(label_reg);
  
  cOrgMessage msg = cOrgMessage(m_organism, messageType);
  msg.SetLabel(GetRegister(label_reg));
  msg.SetData(GetRegister(data_reg));
  return m_organism->SendMessage(ctx, msg);
}

/*! This method /attempts/ to retrieve a message -- It may not be possible, as in
 the case of an empty receive buffer.
 
 If a message is available, ?BX? is set to the message's label, and ~?BX? is set
 to its data.
 */
bool cHardwareCPU::Inst_RetrieveMessage(cAvidaContext& ctx) 
{
  const cOrgMessage* msg = m_organism->RetrieveMessage();
  if(msg == 0)
    return false;
  
  const int label_reg = FindModifiedRegister(REG_BX);
  const int data_reg = FindNextRegister(label_reg);
  
  GetRegister(label_reg) = msg->GetLabel();
  GetRegister(data_reg) = msg->GetData();
  return true;
}

bool cHardwareCPU::Inst_Alarm_MSG_multihop(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);  
  return m_organism->BcastAlarmMSG(ctx, abs(GetRegister(reg_used)%2), m_world->GetConfig().BCAST_HOPS.Get()); // jump to Alarm-label-  odd=high  even=low
}

bool cHardwareCPU::Inst_Alarm_MSG_Bit_Cons24_multihop(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);
  return m_organism->BcastAlarmMSG(ctx, (BitCount(GetRegister(reg_used) & MASK24) >= CONSENSUS24) ? 1 : 0, m_world->GetConfig().BCAST_HOPS.Get());// jump to Alarm-label-high OR Alarm-label-low
}

bool cHardwareCPU::Inst_Alarm_MSG_local(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);  
  return m_organism->BcastAlarmMSG(ctx, abs(GetRegister(reg_used)%2), 1); // jump to Alarm-label-  odd=high  even=low
}

bool cHardwareCPU::Inst_Alarm_MSG_Bit_Cons24_local(cAvidaContext& ctx) {
  const int reg_used = FindModifiedRegister(REG_BX);
  return m_organism->BcastAlarmMSG(ctx, (BitCount(GetRegister(reg_used) & MASK24) >= CONSENSUS24) ? 1 : 0, 1);// jump to Alarm-label-high OR Alarm-label-low
}


bool cHardwareCPU::Inst_Alarm_Label(cAvidaContext& ctx) {
  return true;
}

bool cHardwareCPU::Jump_To_Alarm_Label(int jump_label) {
  if(m_organism->IsSleeping()) {
    return false;
  }
	
  cString channel;
  
  if(jump_label == 1) {
    channel = "high";
  } else if(jump_label == 0) {
    channel = "low";
  } else {
    assert(false);
  }
  
  cInstruction label_inst = GetInstSet().GetInst(cStringUtil::Stringf("alarm-label-")+channel);
  
  cHeadCPU search_head(IP());
  int start_pos = search_head.GetPosition();
  search_head++;
  
  while (start_pos != search_head.GetPosition()) {
    if (search_head.GetInst() == label_inst) {
      // move IP to here
      IP().Set(search_head.GetPosition());
      m_advance_ip = false; // Don't automatically move the IP
      return true;
    }
    search_head++;
  }
  return false;
}


bool cHardwareCPU::moveInstructionHeadToInterruptEnd() {
	//Jump 1 instruction passed msg-handler
	cInstruction label_inst = GetInstSet().GetInst("end-handler");  //cStringUtil::Stringf("MSG_received_handler_END"));
	
	cHeadCPU search_head(IP());
	int start_pos = search_head.GetPosition();
	search_head++;
	
	while (start_pos != search_head.GetPosition()) {
		if (search_head.GetInst() == label_inst) {
			// move IP to here
			search_head++;
			IP().Set(search_head.GetPosition());
			return true;
		}
		search_head++;
	}
	return false;
}

// jumps one instruction passed MSG_received_handler_END
bool cHardwareCPU::Inst_MSG_Handler(cAvidaContext& ctx) {
	m_advance_ip = false;
	return moveInstructionHeadToInterruptEnd();
}

bool cHardwareCPU::Inst_Moved_Handler(cAvidaContext& ctx) {
	m_advance_ip = false;
	return moveInstructionHeadToInterruptEnd();
}

bool cHardwareCPU::Inst_End_Handler(cAvidaContext& ctx) {
	
	/*
	 Process instructions in interrupt handler until MSG_received_handler_END instruction
	 On MSG_received_handler_END, process next interrupt or restore previous state
	 */
	const int threadID = GetCurThread();
	if(m_threads[threadID].isInterrupted()) {
		m_advance_ip = false;
	}
	m_threads[threadID].interruptContextSwitch(cLocalThread::INTERRUPT_COMPLETE);
	return true;
}


//// Placebo insts ////
bool cHardwareCPU::Inst_Skip(cAvidaContext& ctx)
{
  IP().Advance();
  return true;
}

// @BDC Pheromone-related instructions
bool cHardwareCPU::Inst_PheroOn(cAvidaContext& ctx)
{
  m_organism->SetPheromone(true);
  return true;
} //End Inst_PheroOn()

bool cHardwareCPU::Inst_PheroOff(cAvidaContext& ctx)
{
  m_organism->SetPheromone(false);
  return true;
} //End Inst_PheroOff()

bool cHardwareCPU::Inst_PheroToggle(cAvidaContext& ctx)
{
  m_organism->TogglePheromone();
  return true;
} //End Inst_PheroToggle()

// BDC: same as DoSense, but uses senses from cell that org is facing
bool cHardwareCPU::DoSenseFacing(cAvidaContext& ctx, int conversion_method, double base)
{
  cPopulationCell& mycell = m_world->GetPopulation().GetCell(m_organism->GetCellID());
	
  int faced_id = mycell.GetCellFaced().GetID();
	
  // Returns the amount of a resource or resources 
  // specified by modifying NOPs into register BX
  const tArray<double> & res_count = m_world->GetPopulation().GetCellResources(faced_id); 
  
  // Arbitrarily set to BX since the conditional instructions use this directly.
  int reg_to_set = REG_BX;
  
  // There are no resources, return
  if (res_count.GetSize() == 0) return false;
  
  // Only recalculate logs if these values have changed
  static int last_num_resources = 0;
  static int max_label_length = 0;
  int num_nops = GetInstSet().GetNumNops();
  
  if ((last_num_resources != res_count.GetSize()))
  {
    max_label_length = (int) ceil(log((double)res_count.GetSize())/log((double)num_nops));
    last_num_resources = res_count.GetSize();
  }
  
  // Convert modifying NOPs to the index of the resource.
  // If there are fewer than the number of NOPs required
  // to uniquely specify a resource, then add together
  // a subset of resources (motivation: regulation can evolve
  // to be more specific if there is an advantage)
  
  // Find the maximum number of NOPs needed to specify this number of resources
  // Note: It's a bit wasteful to recalculate this every time and organisms will
  // definitely be confused if the number of resources changes during a run
  // because their mapping to resources will be disrupted
  
  // Attempt to read a label with this maximum length
  ReadLabel(max_label_length);
  
  // Find the length of the label that we actually obtained (max is max_reg_needed)
  int real_label_length = GetLabel().GetSize();
  
  // Start and end labels to define the start and end indices of  
  // resources that we need to add together
  cCodeLabel start_label = cCodeLabel(GetLabel());
  cCodeLabel   end_label = cCodeLabel(GetLabel());
  
  for (int i = 0; i < max_label_length - real_label_length; i++)
  {
    start_label.AddNop(0);
    end_label.AddNop(num_nops-1);
  }
  
  int start_index = start_label.AsInt(num_nops);
  int   end_index =   end_label.AsInt(num_nops);
  
  // If the label refers to ONLY resources that 
  // do not exist, then the operation fails
  if (start_index >= res_count.GetSize()) return false;
  
  // Otherwise sum all valid resources that it might refer to
  // (this will only be ONE if the label was of the maximum length).
  int resource_result = 0;
  double dresource_result = 0;
  for (int i = start_index; i <= end_index; i++)
  {
    // if it's a valid resource
    if (i < res_count.GetSize())
    {
      if (conversion_method == 0) // Log
      {
        // for log, add together and then take log
        dresource_result += (double) res_count[i];
      }
      else if (conversion_method == 1) // Addition of multiplied resource amount
      {
        int add_amount = (int) (res_count[i] * base);
        // Do some range checking to make sure we don't overflow
        resource_result = (INT_MAX - resource_result <= add_amount) ? INT_MAX : resource_result + add_amount;
      }
    } 
  }
  
  // Take the log after adding resource amounts together! This way a zero can be assigned to INT_MIN
  if (conversion_method == 0) // Log2
  {
    // You really shouldn't be using the log method if you can get to zero resources
    if(dresource_result == 0.0)
    {
      resource_result = INT_MIN;
    }
    else
    {
      resource_result = (int)(log(dresource_result)/log(base));
    }
  }
  
  //Dump this value into an arbitrary register: BX
  GetRegister(reg_to_set) = resource_result;
  
  //We have to convert this to a different index that includes all degenerate labels possible: shortest to longest
  int sensed_index = 0;
  int on = 1;
  for (int i = 0; i < real_label_length; i++)
  {
    sensed_index += on;
    on *= num_nops;
  }
  sensed_index+= GetLabel().AsInt(num_nops);
  m_organism->GetPhenotype().IncSenseCount(sensed_index);
  
  return true; 
  
  // Note that we are converting <double> resources to <int> register values
} //End DoSenseFacing()

bool cHardwareCPU::Inst_SenseLog2Facing(cAvidaContext& ctx)
{
  return DoSenseFacing(ctx, 0, 2);
}

bool cHardwareCPU::Inst_SenseUnitFacing(cAvidaContext& ctx)
{
  return DoSenseFacing(ctx, 1, 1);
}

bool cHardwareCPU::Inst_SenseMult100Facing(cAvidaContext& ctx)
{
  return DoSenseFacing(ctx, 1, 100);
}

// Sense if the organism is on a target -- put 1 in reg is so, 0 otherwise
bool cHardwareCPU::Inst_SenseTarget(cAvidaContext& ctx) {
  int reg_to_set = FindModifiedRegister(REG_CX);
  int cellid = m_organism->GetCellID();

  if(cellid == -1) {
    return false;
  }
	
  int cell_data = m_world->GetPopulation().GetCell(cellid).GetCellData();
  int val = 0;
	
  if(cell_data > 0) {
    val = 1;
  }
	
  GetRegister(reg_to_set) = val;
  return true;
} //End Inst_SenseTarget()

// Sense if the cell faced is a target -- put 1 in reg is so, 0 otherwise
bool cHardwareCPU::Inst_SenseTargetFaced(cAvidaContext& ctx) {
  int reg_to_set = FindModifiedRegister(REG_CX);
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return false;
  }
	
  cPopulationCell& mycell = pop.GetCell(cellid);
	
  int cell_data = mycell.GetCellFaced().GetCellData(); //absolute id of faced cell
  int val = 0;
	
  if(cell_data > 0) {
    val = 1;
  }
	
  GetRegister(reg_to_set) = val;
  return true;
} //End Inst_SenseTargetFaced()

// DoSensePheromone -- modified version of DoSense to only sense from
// pheromone resource in given cell
bool cHardwareCPU::DoSensePheromone(cAvidaContext& ctx, int cellid)
{
  int reg_to_set = FindModifiedRegister(REG_BX);
	
  if(cellid == -1) {
    return false;
  }
	
  if(cellid == -1) {
    return false;
  }

  cPopulation& pop = m_world->GetPopulation();
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  int relative_cell_id = deme.GetRelativeCellID(cellid);
	
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
  tArray<double> cell_resources = deme_resource_count.GetCellResources(relative_cell_id);
  double pher_amount = 0;
	
  if(deme_resource_count.GetSize() == 0) return false;
  
  for (int i = 0; i < deme_resource_count.GetSize(); i++) {
    if(strncmp(deme_resource_count.GetResName(i), "pheromone", 9) == 0) {
      pher_amount += cell_resources[i];
    }
  }
	
  // In Visual Studio 2005 round function does not exist use floor instead
  //  GetRegister(reg_to_set) = (int)round(pher_amount);
	
  GetRegister(reg_to_set) = (int)floor(pher_amount + 0.5);
	
  return true;
	
} //End DoSensePheromone()

bool cHardwareCPU::Inst_SensePheromone(cAvidaContext& ctx)
{
  int cellid = m_organism->GetCellID(); //absolute id of current cell
	
  if(cellid == -1) {
    return false;
  }
	
  return DoSensePheromone(ctx, cellid);
} //End Inst_SensePheromone()

bool cHardwareCPU::Inst_SensePheromoneFaced(cAvidaContext& ctx)
{
  int cellid = m_organism->GetCellID(); //absolute id of current cell
	
  if(cellid == -1) {
    return false;
  }
	
  cPopulation& pop = m_world->GetPopulation();
  cPopulationCell& mycell = pop.GetCell(cellid);
	
  int fcellid = mycell.GetCellFaced().GetID(); //absolute id of faced cell
	
  return DoSensePheromone(ctx, fcellid);
} //End Inst_SensePheromoneFacing()

bool cHardwareCPU::Inst_Exploit(cAvidaContext& ctx)
{
  int num_rotations = 0;
  double phero_amount = 0;
  double max_pheromone = 0;
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return false;
  }
	
  cPopulationCell& mycell = pop.GetCell(cellid);
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
	//  int relative_cell_id = deme.GetRelativeCellID(cellid);
  //tArray<double> cell_resources = deme_resource_count.GetCellResources(relative_cell_id);
  tArray<double> cell_resources;
	
  int fromcellID, destcellID;
  
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Pheromone drop stuff
  double pher_amount = 0; // this is used in the logging
  int drop_mode = -1;
	
  if( (m_world->GetConfig().EXPLOIT_EXPLORE_PROB.Get() >= 0) &&
		 (m_world->GetRandom().P(m_world->GetConfig().EXPLOIT_EXPLORE_PROB.Get())) ) {
    num_rotations = ctx.GetRandom().GetUInt(m_organism->GetNeighborhoodSize());
  } else {
    // Find which neighbor has the strongest pheromone
    for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
			
      phero_amount = 0;
      cell_resources = deme_resource_count.GetCellResources(deme.GetRelativeCellID(mycell.GetCellFaced().GetID()));
			
      for (int j = 0; j < deme_resource_count.GetSize(); j++) {
        if(strncmp(deme_resource_count.GetResName(j), "pheromone", 9) == 0) {
          phero_amount += cell_resources[j];
        }
      }
			
      if(phero_amount > max_pheromone) {
        num_rotations = i;
        max_pheromone = phero_amount;
      }
			
      mycell.ConnectionList().CircNext();
    }
  }
	
  // Rotate until we face the neighbor with the strongest pheromone.
  // If there was no winner, just move forward.
  for(int i = 0; i < num_rotations; i++) {
    mycell.ConnectionList().CircNext();
  }
	
  // Move to the faced cell
  if(stepsize > 0) {
    fromcellID = m_organism->GetCellID();
		
    if(fromcellID == -1) {
      return false;
    }
		
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
		
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/ 
		
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    m_world->GetStats().Move(*m_organism);
		
    // If organism is dropping pheromones, mark the appropriate cell(s)
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

    } //End laying pheromone
		
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().EXPLOIT_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,3",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
	
  return true;
	
} //End Inst_Exploit()



// Sense neighboring cells, and rotate and move to the neighbor with the
// strongest pheromone.  If there is no winner, just move forward.
// This instruction doesn't sense the three cells behind the organism,
// i.e. the one directly behind, and behind and to the left and right.
bool cHardwareCPU::Inst_ExploitForward5(cAvidaContext& ctx)
{
  int num_rotations = 0;
  double phero_amount = 0;
  double max_pheromone = 0;
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return false;
  }
	
  cPopulationCell& mycell = pop.GetCell(cellid);
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
	//  int relative_cell_id = deme.GetRelativeCellID(cellid);
  //tArray<double> cell_resources = deme_resource_count.GetCellResources(relative_cell_id);
  tArray<double> cell_resources;
	
  int fromcellID, destcellID;
  
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Pheromone drop stuff
  double pher_amount = 0; // this is used in the logging.
  int drop_mode = -1;
	
	
  if( (m_world->GetConfig().EXPLOIT_EXPLORE_PROB.Get() >= 0) &&
		 (m_world->GetRandom().P(m_world->GetConfig().EXPLOIT_EXPLORE_PROB.Get())) ) {
    num_rotations = ctx.GetRandom().GetUInt(m_organism->GetNeighborhoodSize());
  } else {
    // Find which neighbor has the strongest pheromone
    for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
			
      // Skip the cells in the back
      if(i == 3 || i == 4 || i == 5) {
        mycell.ConnectionList().CircNext();
        continue;
      }
			
      phero_amount = 0;
      cell_resources = deme_resource_count.GetCellResources(deme.GetRelativeCellID(mycell.GetCellFaced().GetID()));
			
      for (int j = 0; j < deme_resource_count.GetSize(); j++) {
        if(strncmp(deme_resource_count.GetResName(j), "pheromone", 9) == 0) {
          phero_amount += cell_resources[j];
        }
      }
			
      if(phero_amount > max_pheromone) {
        num_rotations = i;
        max_pheromone = phero_amount;
      }
			
      mycell.ConnectionList().CircNext();
    }
  }
	
  // Rotate until we face the neighbor with the strongest pheromone.
  // If there was no winner, just move forward.
  for(int i = 0; i < num_rotations; i++) {
    mycell.ConnectionList().CircNext();
  }
	
  // Move to the faced cell
  if(stepsize > 0) {
    fromcellID = m_organism->GetCellID();
		
    if(fromcellID == -1) {
      return false;
    }
		
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
		
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/ 
		
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    m_world->GetStats().Move(*m_organism);
		
    // If organism is dropping pheromones, mark the appropriate cell(s)
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

    } //End laying pheromone
		
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().EXPLOIT_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,7",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
	
  return true;
	
} //End Inst_ExploitForward5()


// Sense neighboring cells, and rotate and move to the neighbor with the
// strongest pheromone.  If there is no winner, just move forward.
// This instruction doesn't sense the three cells behind the organism,
// or the ones to the sides.
bool cHardwareCPU::Inst_ExploitForward3(cAvidaContext& ctx)
{
  int num_rotations = 0;
  double phero_amount = 0;
  double max_pheromone = 0;
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return false;
  }
	
  cPopulationCell& mycell = pop.GetCell(cellid);
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
	//  int relative_cell_id = deme.GetRelativeCellID(cellid);
  //tArray<double> cell_resources = deme_resource_count.GetCellResources(relative_cell_id);
  tArray<double> cell_resources;
	
  int fromcellID, destcellID;
  
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Pheromone drop stuff
  double pher_amount = 0; // this is used in the logging.
  int drop_mode = -1;
	
  if( (m_world->GetConfig().EXPLOIT_EXPLORE_PROB.Get() >= 0) &&
		 (m_world->GetRandom().P(m_world->GetConfig().EXPLOIT_EXPLORE_PROB.Get())) ) {
    num_rotations = ctx.GetRandom().GetUInt(m_organism->GetNeighborhoodSize());
  } else {
    // Find which neighbor has the strongest pheromone
    for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
			
      // Skip the cells in the back
      if(i == 2 || i == 3 || i == 4 || i == 5 || i == 6) {
        mycell.ConnectionList().CircNext();
        continue;
      }
			
      phero_amount = 0;
      cell_resources = deme_resource_count.GetCellResources(deme.GetRelativeCellID(mycell.GetCellFaced().GetID()));
			
      for (int j = 0; j < deme_resource_count.GetSize(); j++) {
        if(strncmp(deme_resource_count.GetResName(j), "pheromone", 9) == 0) {
          phero_amount += cell_resources[j];
        }
      }
			
      if(phero_amount > max_pheromone) {
        num_rotations = i;
        max_pheromone = phero_amount;
      }
			
      mycell.ConnectionList().CircNext();
    }
  }
	
  // Rotate until we face the neighbor with the strongest pheromone.
  // If there was no winner, just move forward.
  for(int i = 0; i < num_rotations; i++) {
    mycell.ConnectionList().CircNext();
  }
	
  // Move to the faced cell
  if(stepsize > 0) {
    fromcellID = m_organism->GetCellID();
		
    if(fromcellID == -1) {
      return false;
    }
		
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
		
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/ 
		
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    m_world->GetStats().Move(*m_organism);
		
    // If organism is dropping pheromones, mark the appropriate cell(s)
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

    } //End laying pheromone
		
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().EXPLOIT_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,9",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
	
  return true;
	
} //End Inst_ExploitForward3()

bool cHardwareCPU::Inst_Explore(cAvidaContext& ctx)
{
	//  int num_rotations = 0;
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return true;
  }
	
	//  cPopulationCell& mycell = pop.GetCell(cellid);
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
	
  int fromcellID, destcellID;
	//  int cell_data;
	
  // Pheromone drop stuff
  double pher_amount = 0;
  int drop_mode = -1;
	
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Rotate randomly.  Code taken from tumble.
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  for(unsigned int i = 0; i < ctx.GetRandom().GetUInt(num_neighbors); i++) {
    m_organism->Rotate(1);  // Rotate doesn't rotate N times, just once.
  }
	
	
  // Move to the faced cell
  if(stepsize > 0) {
    fromcellID = m_organism->GetCellID();
		
    if(fromcellID == -1) {
      return false;
    }
		
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
		
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/ 
		
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    m_world->GetStats().Move(*m_organism);
		
    // If organism is dropping pheromones, mark the cells appropriately
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

    } //End laying pheromone
		
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().EXPLORE_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,2",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
	
  return true;
	
} // End Inst_Explore()

// This command should move the organism to the neighbor cell that is a
// target.  If more than one target exists, it moves towards the last-seen
// one.  If no target exists, the organism moves forward in its original
// facing.
bool cHardwareCPU::Inst_MoveTarget(cAvidaContext& ctx)
{
  int num_rotations = 0;
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return true;
  }
	
  cPopulationCell& mycell = pop.GetCell(cellid);
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
	//  const cResourceCount& deme_resource_count = deme.GetDemeResourceCount();
	
  int fromcellID, destcellID;
  int cell_data;
  
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Pheromone drop stuff
  double pher_amount = 0; // this is used in logging
  int drop_mode = -1;
	
  cPopulationCell faced = mycell.GetCellFaced();
	
  // Find if any neighbor is a target
  for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
    cell_data = mycell.GetCellFaced().GetCellData();
		
    if(cell_data > 0) {
      num_rotations = i;
    }
		
    mycell.ConnectionList().CircNext();
  }
	
  // Rotate until we face the neighbor with a target.
  // If there was no winner, just move forward.
  for(int i = 0; i < num_rotations; i++) {
    mycell.ConnectionList().CircNext();
  }
	
  // Move to the faced cell
  if(stepsize > 0) {
    fromcellID = m_organism->GetCellID();
		
    if(fromcellID == -1) {
      return false;
    }
		
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
		
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/ 
		
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    m_world->GetStats().Move(*m_organism);
		
		
    // If organism is dropping pheromones, mark the appropriate cell(s)
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

    } //End laying pheromone
		
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().MOVETARGET_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,1",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
	
  return true;
	
} // End Inst_MoveTarget()



// This command should move the organism to the neighbor cell that is a
// target.  If more than one target exists, it moves towards the last-seen
// one.  If no target exists, the organism moves forward in its original
// facing.  This version ignores the neighbors beind the organism, i.e.
// the cells directly behind and behind and to the left and right.
bool cHardwareCPU::Inst_MoveTargetForward5(cAvidaContext& ctx)
{
  int num_rotations = 0;
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return false;
  }
	
  cPopulationCell& mycell = pop.GetCell(cellid);
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
	
  int fromcellID, destcellID;
  int cell_data;
  
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Pheromone drop stuff
  double pher_amount = 0;
  int drop_mode = -1;
	
  cPopulationCell faced = mycell.GetCellFaced();
	
  // Find if any neighbor is a target
  for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
    
    // Skip the cells behind
    if(i == 3 || i == 4 || i == 5) {
      mycell.ConnectionList().CircNext();
      continue;
    }
		
    cell_data = mycell.GetCellFaced().GetCellData();
		
    if(cell_data > 0) {
      num_rotations = i;
    }
		
    mycell.ConnectionList().CircNext();
  }
	
	//  assert(faced == pop.GetCell(fromcellID).GetCellFaced());
	
  // Rotate until we face the neighbor with a target.
  // If there was no winner, just move forward.
  for(int i = 0; i < num_rotations; i++) {
    mycell.ConnectionList().CircNext();
  }
	
  // Move to the faced cell
  if(stepsize > 0) {
    fromcellID = m_organism->GetCellID();
		
    if(fromcellID == -1) {
      return false;
    }
		
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
		
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/ 
		
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    m_world->GetStats().Move(*m_organism);
		
		
    // If organism is dropping pheromones, mark the appropriate cell(s)
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

    } //End laying pheromone
		
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().MOVETARGET_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,6",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
	
  return true;
	
} // End Inst_MoveTargetForward5()


// This command should move the organism to the neighbor cell that is a
// target.  If more than one target exists, it moves towards the last-seen
// one.  If no target exists, the organism moves forward in its original
// facing.  This version ignores the neighbors beind and to the side of
//  the organism
bool cHardwareCPU::Inst_MoveTargetForward3(cAvidaContext& ctx)
{
  int num_rotations = 0;
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return false;
  }
	
  cPopulationCell& mycell = pop.GetCell(cellid);
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
	
  int fromcellID, destcellID;
  int cell_data;
  
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Pheromone drop stuff
  double pher_amount = 0;
  int drop_mode = -1;
	
  cPopulationCell faced = mycell.GetCellFaced();
	
  // Find if any neighbor is a target
  for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
    
    // Skip the cells behind
    if(i==2 || i == 3 || i == 4 || i == 5 || i == 6) {
      mycell.ConnectionList().CircNext();
      continue;
    }
		
    cell_data = mycell.GetCellFaced().GetCellData();
		
    if(cell_data > 0) {
      num_rotations = i;
    }
		
    mycell.ConnectionList().CircNext();
  }
	
	//  assert(faced == pop.GetCell(fromcellID).GetCellFaced());
	
  // Rotate until we face the neighbor with a target.
  // If there was no winner, just move forward.
  for(int i = 0; i < num_rotations; i++) {
    mycell.ConnectionList().CircNext();
  }
	
  // Move to the faced cell
  if(stepsize > 0) {
    fromcellID = m_organism->GetCellID();
		
    if(fromcellID == -1) {
      return false;
    }
		
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
		
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/ 
		
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    m_world->GetStats().Move(*m_organism);
		
		
    // If organism is dropping pheromones, mark the appropriate cell(s)
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

    } //End laying pheromone
		
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().MOVETARGET_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,8",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
	
  return true;
	
} // End Inst_MoveTargetForward3()

bool cHardwareCPU::Inst_SuperMove(cAvidaContext& ctx)
{
  int num_rotations = 0;
  float phero_amount = 0;
  float max_pheromone = 0;
	
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return false;
  }
	
  cPopulationCell& mycell = pop.GetCell(cellid);
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
  int relative_cell_id = deme.GetRelativeCellID(cellid);
  tArray<double> cell_resources = deme_resource_count.GetCellResources(relative_cell_id);
	
  int fromcellID, destcellID;
  int cell_data;
  
  // Get stepsize. Currently, all moves are one cell regardless of stepsize.
  // This could be changed in the future.
  const int stepsize = m_world->GetConfig().BIOMIMETIC_MOVEMENT_STEP.Get();
	
  // Pheromone drop stuff
  double pher_amount = 0;
  int drop_mode = -1;
	
  // Set num_rotations to a random number for explore -- lowest priority
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  num_rotations = ctx.GetRandom().GetUInt(num_neighbors);
	
	
  // Find the neighbor with highest pheromone -- medium priority
  for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
		
    phero_amount = 0;
    cell_resources = deme_resource_count.GetCellResources(deme.GetRelativeCellID(mycell.GetCellFaced().GetID()));
		
    for (int j = 0; j < deme_resource_count.GetSize(); j++) {
      if(strncmp(deme_resource_count.GetResName(j), "pheromone", 9) == 0) {
        phero_amount += cell_resources[j];
      }
    }
		
    if(phero_amount > max_pheromone) {
      num_rotations = i;
      max_pheromone = phero_amount;
    }
		
    mycell.ConnectionList().CircNext();
  }
	
  // Find if any neighbor is a target -- highest priority
  for(int i = 0; i < mycell.ConnectionList().GetSize(); i++) {
    cell_data = mycell.GetCellFaced().GetCellData();
		
    if(cell_data > 0) {
      num_rotations = i;
    }
		
    mycell.ConnectionList().CircNext();
  }
	
  // Rotate until we face the neighbor with a target.
  // If there was no winner, just move forward.
  for(int i = 0; i < num_rotations; i++) {
    mycell.ConnectionList().CircNext();
  }
	
  // Move to the faced cell
  if(stepsize > 0) {
    fromcellID = m_organism->GetCellID();
		
    if(fromcellID == -1) {
      return false;
    }
		
    destcellID = pop.GetCell(fromcellID).GetCellFaced().GetID();
		
    /*********************/
    // TEMP.  Remove once movement tasks are implemented.
    if(pop.GetCell(fromcellID).GetCellData() < pop.GetCell(destcellID).GetCellData()) { // move up gradient
      m_organism->SetGradientMovement(1.0);
    } else if(pop.GetCell(fromcellID).GetCellData() == pop.GetCell(destcellID).GetCellData()) {
      m_organism->SetGradientMovement(0.0);
    } else { // move down gradient
      m_organism->SetGradientMovement(-1.0);    
    }
    /*********************/ 
		
    pop.SwapCells(pop.GetCell(fromcellID),pop.GetCell(destcellID));
    pop.MoveOrganisms(ctx, pop.GetCell(fromcellID), pop.GetCell(destcellID));
    
    m_world->GetStats().Move(*m_organism);
		
    // If organism is dropping pheromones, mark the appropriate cell(s)
    if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
        (m_organism->GetPheromoneStatus() == true) ) {

        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
	drop_mode = m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
	
	if(drop_mode == 0) {
          deme.AddPheromone(fromcellID, pher_amount/2);
          deme.AddPheromone(destcellID, pher_amount/2);
	} else if(drop_mode == 1) {
          deme.AddPheromone(fromcellID, pher_amount);
	}
	else if(drop_mode == 2) {
          deme.AddPheromone(destcellID, pher_amount);
	}

    } //End laying pheromone
		
		
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().MOVETARGET_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("movelog.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_srcid = deme.GetRelativeCellID(fromcellID);
      int rel_destid = deme.GetRelativeCellID(destcellID);
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%d,%f,%d,4",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_srcid, rel_destid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
    m_organism->Move(ctx);
		
    return true;
  } else {
    return false;
  }
	
  return true;
	
} // End Inst_SuperMove()

bool cHardwareCPU::Inst_IfTarget(cAvidaContext& ctx)
{
  int cellid = m_organism->GetCellID(); //absolute id of current cell
	
  if(cellid == -1) {
	return true;
  }		
	
  int cell_data = m_world->GetPopulation().GetCell(cellid).GetCellData();

  if(cell_data == -1) {
    IP().Advance();
  }
	
  return true;
} //End Inst_IfTarget()


bool cHardwareCPU::Inst_IfNotTarget(cAvidaContext& ctx)
{
  int cellid = m_organism->GetCellID(); //absolute id of current cell
	
  if(cellid == -1) {
    return true;
  }	
	
  int cell_data = m_world->GetPopulation().GetCell(cellid).GetCellData();

  if(cell_data > 0) {
    IP().Advance();
  }
	
  return true;
} //End Inst_IfNotTarget()


bool cHardwareCPU::Inst_IfPheromone(cAvidaContext& ctx)
{
  int cellid = m_organism->GetCellID(); //absolute id of current cell
	
  if(cellid == -1) {
    return true;
  }
	
  cPopulation& pop = m_world->GetPopulation();
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  int relative_cell_id = deme.GetRelativeCellID(cellid);
	
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
  tArray<double> cell_resources = deme_resource_count.GetCellResources(relative_cell_id);
	
  if(deme_resource_count.GetSize() == 0) return false;
	
  double pher_amount = 0;
  
  for (int i = 0; i < deme_resource_count.GetSize(); i++) {
    if(strncmp(deme_resource_count.GetResName(i), "pheromone", 9) == 0) {
      pher_amount += cell_resources[i];
    }
  }
	
  if(pher_amount == 0) {
    IP().Advance();
  }
	
  return true;
	
} //End Inst_IfPheromone()


bool cHardwareCPU::Inst_IfNotPheromone(cAvidaContext& ctx)
{
  int cellid = m_organism->GetCellID(); //absolute id of current cell
	
  if(cellid == -1) {
    return true;
  }
	
  cPopulation& pop = m_world->GetPopulation();
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
  int relative_cell_id = deme.GetRelativeCellID(cellid);
	
  cResourceCount deme_resource_count = deme.GetDemeResourceCount();
  tArray<double> cell_resources = deme_resource_count.GetCellResources(relative_cell_id);
	
  if(deme_resource_count.GetSize() == 0) return false;
	
  double pher_amount = 0;
  
  for (int i = 0; i < deme_resource_count.GetSize(); i++) {
    if(strncmp(deme_resource_count.GetResName(i), "pheromone", 9) == 0) {
      pher_amount += cell_resources[i];
    }
  }
	
  if(pher_amount > 0) {
    IP().Advance();
  }
	
  return true;
	
} //End Inst_IfNotPheromone()


bool cHardwareCPU::Inst_DropPheromone(cAvidaContext& ctx)
{
  cPopulation& pop = m_world->GetPopulation();
  int cellid = m_organism->GetCellID();
	
  if(cellid == -1) {
    return true;
  }
	
  cDeme &deme = pop.GetDeme(pop.GetCell(cellid).GetDemeID());
	
  // If organism is dropping pheromones, mark the appropriate cell
  // Note: right now, we're ignoring the organism's pheromone status and always
  //   dropping if pheromones are enabled
  if(m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) {
		
    const double pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
    //const int drop_mode =  m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
		
    // We can't use the different drop modes, because we only know the cell
    // that the organism is currently in.
    /*
		 if(drop_mode == 0) {
		 deme.AddPheromone(fromcellID, pher_amount/2);
		 deme.AddPheromone(destcellID, pher_amount/2);
		 } else if(drop_mode == 1) {
		 deme.AddPheromone(fromcellID, pher_amount);
		 } else if(drop_mode == 2) {
		 deme.AddPheromone(destcellID, pher_amount);
		 }
		 */
    deme.AddPheromone(cellid, pher_amount);
    
    // Write some logging information if LOG_PHEROMONE is set.  This is done
    // out here so that non-pheromone moves are recorded.
    if( (m_world->GetConfig().LOG_PHEROMONE.Get() == 1) &&
			 (m_world->GetStats().GetUpdate() >= m_world->GetConfig().PHEROMONE_LOG_START.Get()) ) {
      cString tmpfilename = cStringUtil::Stringf("drop-pheromone-log.dat");
      cDataFile& df = m_world->GetDataFile(tmpfilename);
			
      int rel_cellid = deme.GetRelativeCellID(cellid);
      double pher_amount;
      const int drop_mode =  m_world->GetConfig().PHEROMONE_DROP_MODE.Get();
			
      // By columns: update ID, org ID, source cell (relative), destination cell (relative), amount dropped, drop mode
      if( (m_world->GetConfig().PHEROMONE_ENABLED.Get() == 1) &&
          (m_organism->GetPheromoneStatus() == true) ) {
        pher_amount = m_world->GetConfig().PHEROMONE_AMOUNT.Get();
      } else {
        pher_amount = 0;
      }
			
      cString UpdateStr = cStringUtil::Stringf("%d,%d,%d,%d,%f,%d",  m_world->GetStats().GetUpdate(), m_organism->GetID(), deme.GetDemeID(), rel_cellid, pher_amount, drop_mode);
      df.WriteRaw(UpdateStr);
    }
		
  } //End laying pheromone
	
  return true;
	
} //End Inst_DropPheromone()


/*! Set this organism's current opinion to the value in ?BX?.
 */
bool cHardwareCPU::Inst_SetOpinion(cAvidaContext& ctx)
{
	assert(m_organism != 0);
  m_organism->SetOpinion(GetRegister(FindModifiedRegister(REG_BX)));
  return true;
}


/*! Sense this organism's current opinion, placing the opinion in register ?BX?
 and the age of that opinion (in updates) in register !?BX?.  If the organism has no
 opinion, do nothing.
 */
bool cHardwareCPU::Inst_GetOpinion(cAvidaContext& ctx)
{
	assert(m_organism != 0);
  if(m_organism->HasOpinion()) {
    const int opinion_reg = FindModifiedRegister(REG_BX);
    const int age_reg = FindNextRegister(opinion_reg);
		
    GetRegister(opinion_reg) = m_organism->GetOpinion().first;
    GetRegister(age_reg) = m_world->GetStats().GetUpdate() - m_organism->GetOpinion().second;
  }
  return true;
}


/*! Collect this cell's data, and place it in ?BX?.  Set the flag indicating that
 this organism has collected cell data to true, and set the last collected cell data
 as well.
 */
bool cHardwareCPU::Inst_CollectCellData(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_organism->GetCellData();
	// Update last collected cell data:
	m_last_cell_data = std::make_pair(true, GetRegister(out_reg));
  return true;
}


/*! Detect if the cell data in which this organism lives has changed since the
 last time that this organism has collected cell data.  Note that this process
 DOES NOT take into account organism movement, and it only works with explicit
 collection of cell data.
 */
bool cHardwareCPU::Inst_IfCellDataChanged(cAvidaContext& ctx)
{
  assert(m_organism != 0);
	// If we haven't collected cell data yet, or it's the same as the current cell data, advance
	// the IP:
	if(!m_last_cell_data.first || (m_last_cell_data.second == m_organism->GetCellData())) {
		IP().Advance();
	}
	
	return true;
}


bool cHardwareCPU::Inst_KillCellEvent(cAvidaContext& ctx) {
  // Fail if we're running in the test CPU.
  if((m_organism->GetOrgInterface().GetDemeID() < 0) || (m_organism->GetCellID() < 0))
    return false;
	
  const int reg = FindModifiedRegister(REG_BX);
  int eventID = m_organism->GetCellData();
  GetRegister(reg) = m_organism->GetOrgInterface().GetDeme()->KillCellEvent(eventID);
  return true;
}


bool cHardwareCPU::Inst_KillFacedCellEvent(cAvidaContext& ctx) {
  // Fail if we're running in the test CPU.
  if((m_organism->GetOrgInterface().GetDemeID() < 0) || (m_organism->GetCellID() < 0))
    return false;
	
  const int reg = FindModifiedRegister(REG_BX);
  int eventID = m_organism->GetNeighborCellContents();
  GetRegister(reg) = m_organism->GetOrgInterface().GetDeme()->KillCellEvent(eventID);
  
  if(GetRegister(reg))
    m_organism->SetEventKilled();
  
  return true;
}


bool cHardwareCPU::Inst_CollectCellDataAndKillEvent(cAvidaContext& ctx) {
  // Fail if we're running in the test CPU.
  if((m_organism->GetOrgInterface().GetDemeID() < 0) || (m_organism->GetCellID() < 0))
    return false;
  
  const int out_reg = FindModifiedRegister(REG_BX);
  int eventID = m_organism->GetCellData();
  GetRegister(out_reg) = eventID;
  
  m_organism->GetOrgInterface().GetDeme()->KillCellEvent(eventID);
  return true;
}


/*! Called when the organism that owns this CPU has received a flash from a neighbor. */
void cHardwareCPU::ReceiveFlash() {
  m_flash_info.first = 1; // Yes, we've received a flash.
  m_flash_info.second = m_cycle_counter; // When we received it.
}


/*! Send a "flash" event to all neighboring organisms. */
bool cHardwareCPU::Inst_Flash(cAvidaContext& ctx) {
  assert(m_organism != 0);
  m_organism->SendFlash(ctx);
  return true;
}


/*! Test if this organism has ever recieved a flash event. */
bool cHardwareCPU::Inst_IfRecvdFlash(cAvidaContext& ctx) {
  assert(m_organism != 0);
  if(m_flash_info.first == 0) {
    IP().Advance();
  }
  return true;
}


/*! Retrieve if & when this organism has last received a flash. */
bool cHardwareCPU::Inst_FlashInfo(cAvidaContext& ctx) {
  assert(m_organism != 0);
  const int bx = FindModifiedRegister(REG_BX);
  const int cx = FindNextRegister(bx);
  
  if(m_flash_info.first > 0) {
    assert(m_cycle_counter >= m_flash_info.second);
    GetRegister(bx) = m_flash_info.first;
    GetRegister(cx) = m_cycle_counter - m_flash_info.second;
  } else {
    GetRegister(bx) = 0;
    GetRegister(cx) = 0;
  }
  return true;
}


/*! Retrieve if (but not when) this organism has last received a flash. */
bool cHardwareCPU::Inst_FlashInfoB(cAvidaContext& ctx) {
  assert(m_organism != 0);
  const int bx = FindModifiedRegister(REG_BX);
  
  if(m_flash_info.first > 0) {
    assert(m_cycle_counter >= m_flash_info.second);
    GetRegister(bx) = m_flash_info.first;
  } else {
    GetRegister(bx) = 0;
  }
  return true;
}


bool cHardwareCPU::Inst_ResetFlashInfo(cAvidaContext& ctx) {
  assert(m_organism != 0);
  m_flash_info.first = 0;
  m_flash_info.second = 0;
  return true;
}


bool cHardwareCPU::Inst_HardReset(cAvidaContext& ctx) {
  Reset(ctx);
  m_advance_ip = false;
  return true;
}


//! Current "time": the number of cycles this CPU has been "alive."
bool cHardwareCPU::Inst_GetCycles(cAvidaContext& ctx) {
  GetRegister(FindModifiedRegister(REG_BX)) = m_cycle_counter;
  return true;
}
