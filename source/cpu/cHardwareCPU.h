/*
 *  cHardwareCPU.h
 *  Avida
 *
 *  Called "hardware_cpu.hh" prior to 11/17/05.
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

#ifndef cHardwareCPU_h
#define cHardwareCPU_h

#include <iomanip>
#include <vector>

#ifndef cCodeLabel_h
#include "cCodeLabel.h"
#endif
#ifndef cHeadCPU_h
#include "cHeadCPU.h"
#endif
#ifndef cCPUMemory_h
#include "cCPUMemory.h"
#endif
#ifndef cCPUStack_h
#include "cCPUStack.h"
#endif
#ifndef cHardwareBase_h
#include "cHardwareBase.h"
#endif
#ifndef cString_h
#include "cString.h"
#endif
#ifndef cStats_h
#include "cStats.h"
#endif
#ifndef tArray_h
#include "tArray.h"
#endif
#ifndef tInstLib_h
#include "tInstLib.h"
#endif
#ifndef cPopulation_h
#include "cPopulation.h"
#endif

#ifndef defs_h
#include "defs.h"
#endif
#ifndef nHardware_h
#include "nHardware.h"
#endif

/**
 * Each organism may have a cHardwareCPU structure which keeps track of the
 * current status of all the components of the simulated hardware.
 *
 * @see cHardwareCPU_Thread, cCPUStack, cCPUMemory, cInstSet
 **/

class cInjectGenotype;
class cInstLib;
class cInstSet;
class cMutation;
class cOrganism;


class cHardwareCPU : public cHardwareBase
{
public:
  typedef bool (cHardwareCPU::*tMethod)(cAvidaContext& ctx);

protected:
  // --------  Structure Constants  --------
  static const int NUM_REGISTERS = 3;
  static const int NUM_HEADS = nHardware::NUM_HEADS >= NUM_REGISTERS ? nHardware::NUM_HEADS : NUM_REGISTERS;
  enum tRegisters { REG_AX = 0, REG_BX, REG_CX, REG_DX, REG_EX, REG_FX };
  static const int NUM_NOPS = 3;
  
  // --------  Data Structures  --------
  struct cLocalThread
  {
  private:
    int m_id;
    
  public:
    int reg[NUM_REGISTERS];
    cHeadCPU heads[NUM_HEADS];
    cCPUStack stack;
    unsigned char cur_stack;              // 0 = local stack, 1 = global stack.
    unsigned char cur_head;
    
    cCodeLabel read_label;
    cCodeLabel next_label;
    
    
    cLocalThread(cHardwareBase* in_hardware = NULL, int in_id = -1) { Reset(in_hardware, in_id); }
    ~cLocalThread() { ; }
    
    void operator=(const cLocalThread& in_thread);
    
    void Reset(cHardwareBase* in_hardware, int in_id);
    int GetID() const { return m_id; }
    void SetID(int in_id) { m_id = in_id; }
  };

    
  // --------  Static Variables  --------
  static tInstLib<tMethod>* s_inst_slib;
  static tInstLib<tMethod>* initInstLib(void);


  // --------  Member Variables  --------
  const tMethod* m_functions;

  cCPUMemory m_memory;          // Memory...
  cCPUStack m_global_stack;     // A stack that all threads share.

  tArray<cLocalThread> m_threads;
  int m_thread_id_chart;
  int m_cur_thread;

  // Flags...
  bool m_mal_active;         // Has an allocate occured since last divide?
  bool m_advance_ip;         // Should the IP advance after this instruction?
  bool m_executedmatchstrings;	// Have we already executed the match strings instruction?

  // Instruction costs...
#if INSTRUCTION_COSTS
  tArray<int> inst_cost;
  tArray<int> inst_ft_cost;
  tArray<int> inst_energy_cost;
  bool m_has_costs;
  bool m_has_ft_costs;
  bool m_has_energy_costs;
#endif
  
  
  bool SingleProcess_PayCosts(cAvidaContext& ctx, const cInstruction& cur_inst);
  bool SingleProcess_ExecuteInst(cAvidaContext& ctx, const cInstruction& cur_inst);
  
  // --------  Stack Manipulation...  --------
  inline void StackPush(int value);
  inline int StackPop();
  inline void StackFlip();
  inline void StackClear();
  inline void SwitchStack();
  
  
  // --------  Head Manipulation (including IP)  --------
  cHeadCPU& GetActiveHead() { return m_threads[m_cur_thread].heads[m_threads[m_cur_thread].cur_head]; }
  void AdjustHeads();
  
  
  // --------  Label Manipulation  -------
  const cCodeLabel& GetLabel() const { return m_threads[m_cur_thread].next_label; }
  cCodeLabel& GetLabel() { return m_threads[m_cur_thread].next_label; }
  void ReadLabel(int max_size=nHardware::MAX_LABEL_SIZE);
  cHeadCPU FindLabel(int direction);
  int FindLabel_Forward(const cCodeLabel & search_label, const cGenome& search_genome, int pos);
  int FindLabel_Backward(const cCodeLabel & search_label, const cGenome& search_genome, int pos);
  cHeadCPU FindLabel(const cCodeLabel & in_label, int direction);
  const cCodeLabel& GetReadLabel() const { return m_threads[m_cur_thread].read_label; }
  cCodeLabel& GetReadLabel() { return m_threads[m_cur_thread].read_label; }
  
  
  // --------  Thread Manipulation  -------
  bool ForkThread(); // Adds a new thread based off of m_cur_thread.
  bool KillThread(); // Kill the current thread!
  
  
  // ---------- Instruction Helpers -----------
  int FindModifiedRegister(int default_register);
  int FindModifiedNextRegister(int default_register);
  int FindModifiedPreviousRegister(int default_register);
  int FindModifiedHead(int default_head);
  int FindNextRegister(int base_reg);
  
  bool Allocate_Necro(const int new_size);
  bool Allocate_Random(cAvidaContext& ctx, const int old_size, const int new_size);
  bool Allocate_Default(const int new_size);
  bool Allocate_Main(cAvidaContext& ctx, const int allocated_size);
  
  int GetCopiedSize(const int parent_size, const int child_size);
  
  bool Divide_Main(cAvidaContext& ctx, const int divide_point, const int extra_lines=0, double mut_multiplier=1);
  bool Divide_MainRS(cAvidaContext& ctx, const int divide_point, const int extra_lines=0, double mut_multiplier=1); //AWC 06/29/06
  bool Divide_Main1RS(cAvidaContext& ctx, const int divide_point, const int extra_lines=0, double mut_multiplier=1); //AWC 07/28/06
  bool Divide_Main2RS(cAvidaContext& ctx, const int divide_point, const int extra_lines=0, double mut_multiplier=1); //AWC 07/28/06

  void Divide_DoTransposons(cAvidaContext& ctx);
  
  void InjectCode(const cGenome& injection, const int line_num);
  
  bool HeadCopy_ErrorCorrect(cAvidaContext& ctx, double reduction);
  bool Inst_HeadDivideMut(cAvidaContext& ctx, double mut_multiplier = 1);
  
  void ReadInst(const int in_inst);

  
  cHardwareCPU& operator=(const cHardwareCPU&); // @not_implemented

public:
  cHardwareCPU(cWorld* world, cOrganism* in_organism, cInstSet* in_inst_set);
  explicit cHardwareCPU(const cHardwareCPU&);
  ~cHardwareCPU() { ; }
  static tInstLib<tMethod>* GetInstLib() { return s_inst_slib; }
  static cString GetDefaultInstFilename() { return "instset-classic.cfg"; }

  void Reset();
  void SingleProcess(cAvidaContext& ctx);
  void ProcessBonusInst(cAvidaContext& ctx, const cInstruction& inst);

  
  // --------  Helper methods  --------
  int GetType() const { return HARDWARE_TYPE_CPU_ORIGINAL; }  
  bool OK();
  void PrintStatus(std::ostream& fp);


  // --------  Stack Manipulation...  --------
  inline int GetStack(int depth=0, int stack_id=-1, int in_thread=-1) const;
  inline int GetNumStacks() const { return 2; }


  // --------  Head Manipulation (including IP)  --------
  const cHeadCPU& GetHead(int head_id) const { return m_threads[m_cur_thread].heads[head_id]; }
  cHeadCPU& GetHead(int head_id) { return m_threads[m_cur_thread].heads[head_id];}
  const cHeadCPU& GetHead(int head_id, int thread) const { return m_threads[thread].heads[head_id]; }
  cHeadCPU& GetHead(int head_id, int thread) { return m_threads[thread].heads[head_id];}
  int GetNumHeads() const { return NUM_HEADS; }
  
  const cHeadCPU& IP() const { return m_threads[m_cur_thread].heads[nHardware::HEAD_IP]; }
  cHeadCPU& IP() { return m_threads[m_cur_thread].heads[nHardware::HEAD_IP]; }
  const cHeadCPU& IP(int thread) const { return m_threads[thread].heads[nHardware::HEAD_IP]; }
  cHeadCPU& IP(int thread) { return m_threads[thread].heads[nHardware::HEAD_IP]; }
  
  
  // --------  Memory Manipulation  --------
  const cCPUMemory& GetMemory() const { return m_memory; }
  cCPUMemory& GetMemory() { return m_memory; }
  const cCPUMemory& GetMemory(int value) const { return m_memory; }
  cCPUMemory& GetMemory(int value) { return m_memory; }
  int GetNumMemSpaces() const { return 1; }
  
  
  // --------  Register Manipulation  --------
  const int GetRegister(int reg_id) const { return m_threads[m_cur_thread].reg[reg_id]; }
  int& GetRegister(int reg_id) { return m_threads[m_cur_thread].reg[reg_id]; }
  int GetNumRegisters() const { return NUM_REGISTERS; }

  
  // --------  Thread Manipulation  --------
  bool ThreadSelect(const int thread_num);
  bool ThreadSelect(const cCodeLabel& in_label) { return false; } // Labeled threads not supported
  inline void ThreadPrev(); // Shift the current thread in use.
  inline void ThreadNext();
  cInjectGenotype* ThreadGetOwner() { return NULL; } // @DMB - cHardwareCPU does not really implement cInjectGenotype yet
  void ThreadSetOwner(cInjectGenotype* in_genotype) { return; }
  
  int GetNumThreads() const     { return m_threads.GetSize(); }
  int GetCurThread() const      { return m_cur_thread; }
  int GetCurThreadID() const    { return m_threads[m_cur_thread].GetID(); }
  
  
  // --------  Parasite Stuff  --------
  bool InjectHost(const cCodeLabel& in_label, const cGenome& injection);

  
  // Non-Standard Methods
  
  int GetActiveStack() const { return m_threads[m_cur_thread].cur_stack; }
  bool GetMalActive() const   { return m_mal_active; }
  
private:
  // ---------- Instruction Library -----------

  // Flow Control
  bool Inst_If0(cAvidaContext& ctx);
  bool Inst_IfEqu(cAvidaContext& ctx);
  bool Inst_IfNot0(cAvidaContext& ctx);
  bool Inst_IfNEqu(cAvidaContext& ctx);
  bool Inst_IfGr0(cAvidaContext& ctx);
  bool Inst_IfGr(cAvidaContext& ctx);
  bool Inst_IfGrEqu0(cAvidaContext& ctx);
  bool Inst_IfGrEqu(cAvidaContext& ctx);
  bool Inst_IfLess0(cAvidaContext& ctx);
  bool Inst_IfLess(cAvidaContext& ctx);
  bool Inst_IfLsEqu0(cAvidaContext& ctx);
  bool Inst_IfLsEqu(cAvidaContext& ctx);
  bool Inst_IfBit1(cAvidaContext& ctx);
  bool Inst_IfANotEqB(cAvidaContext& ctx);
  bool Inst_IfBNotEqC(cAvidaContext& ctx);
  bool Inst_IfANotEqC(cAvidaContext& ctx);

  bool Inst_JumpF(cAvidaContext& ctx);
  bool Inst_JumpB(cAvidaContext& ctx);
  bool Inst_Call(cAvidaContext& ctx);
  bool Inst_Return(cAvidaContext& ctx);
  
  bool Inst_Throw(cAvidaContext& ctx);
  bool Inst_ThrowIf0(cAvidaContext& ctx);
  bool Inst_ThrowIfNot0(cAvidaContext& ctx);  
  bool Inst_Catch(cAvidaContext& ctx) { ReadLabel(); return true; };
 
  bool Inst_Goto(cAvidaContext& ctx);
  bool Inst_GotoIf0(cAvidaContext& ctx);
  bool Inst_GotoIfNot0(cAvidaContext& ctx);  
  bool Inst_Label(cAvidaContext& ctx) { ReadLabel(); return true; };
    
  // Stack and Register Operations
  bool Inst_Pop(cAvidaContext& ctx);
  bool Inst_Push(cAvidaContext& ctx);
  bool Inst_HeadPop(cAvidaContext& ctx);
  bool Inst_HeadPush(cAvidaContext& ctx);

  bool Inst_PopA(cAvidaContext& ctx);
  bool Inst_PopB(cAvidaContext& ctx);
  bool Inst_PopC(cAvidaContext& ctx);
  bool Inst_PushA(cAvidaContext& ctx);
  bool Inst_PushB(cAvidaContext& ctx);
  bool Inst_PushC(cAvidaContext& ctx);

  bool Inst_SwitchStack(cAvidaContext& ctx);
  bool Inst_FlipStack(cAvidaContext& ctx);
  bool Inst_Swap(cAvidaContext& ctx);
  bool Inst_SwapAB(cAvidaContext& ctx);
  bool Inst_SwapBC(cAvidaContext& ctx);
  bool Inst_SwapAC(cAvidaContext& ctx);
  bool Inst_CopyReg(cAvidaContext& ctx);
  bool Inst_CopyRegAB(cAvidaContext& ctx);
  bool Inst_CopyRegAC(cAvidaContext& ctx);
  bool Inst_CopyRegBA(cAvidaContext& ctx);
  bool Inst_CopyRegBC(cAvidaContext& ctx);
  bool Inst_CopyRegCA(cAvidaContext& ctx);
  bool Inst_CopyRegCB(cAvidaContext& ctx);
  bool Inst_Reset(cAvidaContext& ctx);

  // Single-Argument Math
  bool Inst_ShiftR(cAvidaContext& ctx);
  bool Inst_ShiftL(cAvidaContext& ctx);
  bool Inst_Bit1(cAvidaContext& ctx);
  bool Inst_SetNum(cAvidaContext& ctx);
  bool Inst_ValGrey(cAvidaContext& ctx);
  bool Inst_ValDir(cAvidaContext& ctx);
  bool Inst_ValAddP(cAvidaContext& ctx);
  bool Inst_ValFib(cAvidaContext& ctx);
  bool Inst_ValPolyC(cAvidaContext& ctx);
  bool Inst_Inc(cAvidaContext& ctx);
  bool Inst_Dec(cAvidaContext& ctx);
  bool Inst_Zero(cAvidaContext& ctx);
  bool Inst_Not(cAvidaContext& ctx);
  bool Inst_Neg(cAvidaContext& ctx);
  bool Inst_Square(cAvidaContext& ctx);
  bool Inst_Sqrt(cAvidaContext& ctx);
  bool Inst_Log(cAvidaContext& ctx);
  bool Inst_Log10(cAvidaContext& ctx);
  bool Inst_Minus17(cAvidaContext& ctx);

  // Double Argument Math
  bool Inst_Add(cAvidaContext& ctx);
  bool Inst_Sub(cAvidaContext& ctx);
  bool Inst_Mult(cAvidaContext& ctx);
  bool Inst_Div(cAvidaContext& ctx);
  bool Inst_Mod(cAvidaContext& ctx);
  bool Inst_Nand(cAvidaContext& ctx);
  bool Inst_Nor(cAvidaContext& ctx);
  bool Inst_And(cAvidaContext& ctx);
  bool Inst_Order(cAvidaContext& ctx);
  bool Inst_Xor(cAvidaContext& ctx);

  // Biological
  bool Inst_Copy(cAvidaContext& ctx);
  bool Inst_ReadInst(cAvidaContext& ctx);
  bool Inst_WriteInst(cAvidaContext& ctx);
  bool Inst_StackReadInst(cAvidaContext& ctx);
  bool Inst_StackWriteInst(cAvidaContext& ctx);
  bool Inst_Compare(cAvidaContext& ctx);
  bool Inst_IfNCpy(cAvidaContext& ctx);
  bool Inst_Allocate(cAvidaContext& ctx);
  bool Inst_Divide(cAvidaContext& ctx);
  bool Inst_DivideRS(cAvidaContext& ctx); // AWC 06/29/06
  bool Inst_CAlloc(cAvidaContext& ctx);
  bool Inst_CDivide(cAvidaContext& ctx);
  bool Inst_MaxAlloc(cAvidaContext& ctx);
  bool Inst_MaxAllocMoveWriteHead(cAvidaContext& ctx);
  bool Inst_Inject(cAvidaContext& ctx);
  bool Inst_InjectRand(cAvidaContext& ctx);
  bool Inst_InjectThread(cAvidaContext& ctx);
  bool Inst_Transposon(cAvidaContext& ctx);
  bool Inst_Repro(cAvidaContext& ctx);
  bool Inst_TaskPutRepro(cAvidaContext& ctx);
  bool Inst_TaskPutResetInputsRepro(cAvidaContext& ctx);

  bool Inst_SpawnDeme(cAvidaContext& ctx);
  bool Inst_Kazi(cAvidaContext& ctx);
  bool Inst_Kazi5(cAvidaContext& ctx);
  bool Inst_Die(cAvidaContext& ctx);

  // I/O and Sensory
  bool Inst_TaskGet(cAvidaContext& ctx);
  bool Inst_TaskGet2(cAvidaContext& ctx);
  bool Inst_TaskStackGet(cAvidaContext& ctx);
  bool Inst_TaskStackLoad(cAvidaContext& ctx);
  bool Inst_TaskPut(cAvidaContext& ctx);
  bool Inst_TaskPutResetInputs(cAvidaContext& ctx);
  bool Inst_TaskIO(cAvidaContext& ctx);
  bool Inst_TaskIO_Feedback(cAvidaContext& ctx);
  bool Inst_MatchStrings(cAvidaContext& ctx);
  bool Inst_Sell(cAvidaContext& ctx);
  bool Inst_Buy(cAvidaContext& ctx);
  bool Inst_Send(cAvidaContext& ctx);
  bool Inst_Receive(cAvidaContext& ctx);
  bool Inst_SenseLog2(cAvidaContext& ctx);
  bool Inst_SenseUnit(cAvidaContext& ctx);
  bool Inst_SenseMult100(cAvidaContext& ctx);
  bool DoSense(cAvidaContext& ctx, int conversion_method, double base);

  void DoDonate(cOrganism * to_org);
  bool Inst_DonateRandom(cAvidaContext& ctx);
  bool Inst_DonateKin(cAvidaContext& ctx);
  bool Inst_DonateEditDist(cAvidaContext& ctx);
  bool Inst_DonateGreenBeardGene(cAvidaContext& ctx);
  bool Inst_DonateTrueGreenBeard(cAvidaContext& ctx);
  bool Inst_DonateThreshGreenBeard(cAvidaContext& ctx);
  bool Inst_DonateQuantaThreshGreenBeard(cAvidaContext& ctx);
  bool Inst_DonateNULL(cAvidaContext& ctx);

  bool Inst_SearchF(cAvidaContext& ctx);
  bool Inst_SearchB(cAvidaContext& ctx);
  bool Inst_MemSize(cAvidaContext& ctx);

  bool Inst_IOBufAdd1(cAvidaContext& ctx);
  bool Inst_IOBufAdd0(cAvidaContext& ctx);

  // Environment

  bool Inst_RotateL(cAvidaContext& ctx);
  bool Inst_RotateR(cAvidaContext& ctx);
  bool Inst_RotateLabel(cAvidaContext& ctx);
  bool Inst_SetCopyMut(cAvidaContext& ctx);
  bool Inst_ModCopyMut(cAvidaContext& ctx);
  // @WRE additions for movement
  bool Inst_Tumble(cAvidaContext& ctx);
  bool Inst_Move(cAvidaContext& ctx);

  // Energy use
  
  bool Inst_ZeroEnergyUsed(cAvidaContext& ctx); 

  // Multi-threading...

  bool Inst_ForkThread(cAvidaContext& ctx);
  bool Inst_KillThread(cAvidaContext& ctx);
  bool Inst_ThreadID(cAvidaContext& ctx);

  // Head-based instructions...

  bool Inst_SetHead(cAvidaContext& ctx);
  bool Inst_AdvanceHead(cAvidaContext& ctx);
  bool Inst_MoveHead(cAvidaContext& ctx);
  bool Inst_JumpHead(cAvidaContext& ctx);
  bool Inst_GetHead(cAvidaContext& ctx);
  bool Inst_IfLabel(cAvidaContext& ctx);
  bool Inst_IfLabel2(cAvidaContext& ctx);
  bool Inst_HeadDivide(cAvidaContext& ctx);
  bool Inst_HeadDivideRS(cAvidaContext& ctx); //AWC 06/29/06
  bool Inst_HeadDivide1RS(cAvidaContext& ctx); //AWC 07/28/06
  bool Inst_HeadDivide2RS(cAvidaContext& ctx); //AWC 08/29/06
  bool Inst_HeadRead(cAvidaContext& ctx);
  bool Inst_HeadWrite(cAvidaContext& ctx);
  bool Inst_HeadCopy(cAvidaContext& ctx);
  bool Inst_HeadSearch(cAvidaContext& ctx);
  bool Inst_SetFlow(cAvidaContext& ctx);

  bool Inst_HeadCopy2(cAvidaContext& ctx);
  bool Inst_HeadCopy3(cAvidaContext& ctx);
  bool Inst_HeadCopy4(cAvidaContext& ctx);
  bool Inst_HeadCopy5(cAvidaContext& ctx);
  bool Inst_HeadCopy6(cAvidaContext& ctx);
  bool Inst_HeadCopy7(cAvidaContext& ctx);
  bool Inst_HeadCopy8(cAvidaContext& ctx);
  bool Inst_HeadCopy9(cAvidaContext& ctx);
  bool Inst_HeadCopy10(cAvidaContext& ctx);

  bool Inst_HeadDivideSex(cAvidaContext& ctx);
  bool Inst_HeadDivideAsex(cAvidaContext& ctx);
  bool Inst_HeadDivideAsexWait(cAvidaContext& ctx);
  bool Inst_HeadDivideMateSelect(cAvidaContext& ctx);

  bool Inst_HeadDivide1(cAvidaContext& ctx);
  bool Inst_HeadDivide2(cAvidaContext& ctx);
  bool Inst_HeadDivide3(cAvidaContext& ctx);
  bool Inst_HeadDivide4(cAvidaContext& ctx);
  bool Inst_HeadDivide5(cAvidaContext& ctx);
  bool Inst_HeadDivide6(cAvidaContext& ctx);
  bool Inst_HeadDivide7(cAvidaContext& ctx);
  bool Inst_HeadDivide8(cAvidaContext& ctx);
  bool Inst_HeadDivide9(cAvidaContext& ctx);
  bool Inst_HeadDivide10(cAvidaContext& ctx);
  bool Inst_HeadDivide16(cAvidaContext& ctx);
  bool Inst_HeadDivide32(cAvidaContext& ctx);
  bool Inst_HeadDivide50(cAvidaContext& ctx);
  bool Inst_HeadDivide100(cAvidaContext& ctx);
  bool Inst_HeadDivide500(cAvidaContext& ctx);
  bool Inst_HeadDivide1000(cAvidaContext& ctx);
  bool Inst_HeadDivide5000(cAvidaContext& ctx);
  bool Inst_HeadDivide10000(cAvidaContext& ctx);
  bool Inst_HeadDivide50000(cAvidaContext& ctx);
  bool Inst_HeadDivide0_5(cAvidaContext& ctx);
  bool Inst_HeadDivide0_1(cAvidaContext& ctx);
  bool Inst_HeadDivide0_05(cAvidaContext& ctx);
  bool Inst_HeadDivide0_01(cAvidaContext& ctx);
  bool Inst_HeadDivide0_001(cAvidaContext& ctx);

  bool Inst_Sleep(cAvidaContext& ctx);
  bool Inst_GetUpdate(cAvidaContext& ctx);

  //// Promoter Model ////
  void GetPromoterPattern(tArray<int>& promoter);
  void RegulatePromoter(cAvidaContext& ctx, bool up);
  bool Inst_UpRegulatePromoter(cAvidaContext& ctx) { RegulatePromoter(ctx, true); return true; }
  bool Inst_DownRegulatePromoter(cAvidaContext& ctx) { RegulatePromoter(ctx, false); return true; }
  void RegulatePromoterNop(cAvidaContext& ctx, bool up);
  bool Inst_UpRegulatePromoterNop(cAvidaContext& ctx) { RegulatePromoterNop(ctx, true); return true; }
  bool Inst_DownRegulatePromoterNop(cAvidaContext& ctx) { RegulatePromoterNop(ctx, false); return true; }
  void RegulatePromoterNopIfGT0(cAvidaContext& ctx, bool up); 
  bool Inst_UpRegulatePromoterNopIfGT0(cAvidaContext& ctx) { RegulatePromoterNopIfGT0(ctx, true); return true; }
  bool Inst_DownRegulatePromoterNopIfGT0(cAvidaContext& ctx) { RegulatePromoterNopIfGT0(ctx, false); return true; } 
  bool Inst_Terminate(cAvidaContext& ctx);
  bool Inst_Promoter(cAvidaContext& ctx);
  bool Inst_DecayRegulation(cAvidaContext& ctx);
  
  //// Placebo ////
  bool Inst_Skip(cAvidaContext& ctx);
  
  //// UML Element Construction ////
//  bool Inst_AddState(cAvidaContext& ctx);
//  bool Inst_Next(cAvidaContext& ctx);
//  bool Inst_Prev(cAvidaContext& ctx);
//  bool Inst_JumpIndex(cAvidaContext& ctx);
//  bool Inst_JumpDist(cAvidaContext& ctx);
//  bool Inst_AddTransitionLabel(cAvidaContext& ctx);
  bool Inst_AddTransitionFromLabel(cAvidaContext& ctx);
  bool Inst_AddTransitionTotal(cAvidaContext& ctx);
//  bool Inst_Last(cAvidaContext& ctx);
//  bool Inst_First(cAvidaContext& ctx);
  
  bool Inst_StateDiag0(cAvidaContext& ctx);
  bool Inst_StateDiag1(cAvidaContext& ctx);
  bool Inst_StateDiag2(cAvidaContext& ctx);
  
  bool Inst_OrigState0(cAvidaContext& ctx);
  bool Inst_OrigState1(cAvidaContext& ctx);
  bool Inst_OrigState2(cAvidaContext& ctx);
  bool Inst_OrigState3(cAvidaContext& ctx);
  bool Inst_OrigState4(cAvidaContext& ctx);
  bool Inst_OrigState5(cAvidaContext& ctx);
  bool Inst_OrigState6(cAvidaContext& ctx);
  bool Inst_OrigState7(cAvidaContext& ctx);
  bool Inst_OrigState8(cAvidaContext& ctx);
  bool Inst_OrigState9(cAvidaContext& ctx);
  bool Inst_OrigState10(cAvidaContext& ctx);
  bool Inst_OrigState11(cAvidaContext& ctx);
  bool Inst_OrigState12(cAvidaContext& ctx);
  

  bool Inst_DestState0(cAvidaContext& ctx);
  bool Inst_DestState1(cAvidaContext& ctx);
  bool Inst_DestState2(cAvidaContext& ctx);
  bool Inst_DestState3(cAvidaContext& ctx);
  bool Inst_DestState4(cAvidaContext& ctx);
  bool Inst_DestState5(cAvidaContext& ctx);
  bool Inst_DestState6(cAvidaContext& ctx);
  bool Inst_DestState7(cAvidaContext& ctx);
  bool Inst_DestState8(cAvidaContext& ctx);
  bool Inst_DestState9(cAvidaContext& ctx);
  bool Inst_DestState10(cAvidaContext& ctx);
  bool Inst_DestState11(cAvidaContext& ctx);
  bool Inst_DestState12(cAvidaContext& ctx);

  
  bool Inst_TransLab0(cAvidaContext& ctx);
  bool Inst_TransLab1(cAvidaContext& ctx);
  bool Inst_TransLab2(cAvidaContext& ctx);
  bool Inst_TransLab3(cAvidaContext& ctx);
  bool Inst_TransLab4(cAvidaContext& ctx);
  bool Inst_TransLab5(cAvidaContext& ctx);
  bool Inst_TransLab6(cAvidaContext& ctx);
  bool Inst_TransLab7(cAvidaContext& ctx);
  bool Inst_TransLab8(cAvidaContext& ctx);
  bool Inst_TransLab9(cAvidaContext& ctx);
  bool Inst_TransLab10(cAvidaContext& ctx);
  bool Inst_TransLab11(cAvidaContext& ctx);

  bool Inst_Trigger0(cAvidaContext& ctx);
  bool Inst_Trigger1(cAvidaContext& ctx);
  bool Inst_Trigger2(cAvidaContext& ctx);
  bool Inst_Trigger3(cAvidaContext& ctx);
  bool Inst_Trigger4(cAvidaContext& ctx);
  bool Inst_Trigger5(cAvidaContext& ctx);
  bool Inst_Trigger6(cAvidaContext& ctx);
  bool Inst_Trigger7(cAvidaContext& ctx);
  bool Inst_Trigger8(cAvidaContext& ctx);
  bool Inst_Trigger9(cAvidaContext& ctx);
  bool Inst_Trigger10(cAvidaContext& ctx);
  bool Inst_Trigger11(cAvidaContext& ctx);
  
  bool Inst_Guard0(cAvidaContext& ctx);
  bool Inst_Guard1(cAvidaContext& ctx);
  bool Inst_Guard2(cAvidaContext& ctx);
  bool Inst_Guard3(cAvidaContext& ctx);
  bool Inst_Guard4(cAvidaContext& ctx);

  
  bool Inst_Action0(cAvidaContext& ctx);
  bool Inst_Action1(cAvidaContext& ctx);
  bool Inst_Action2(cAvidaContext& ctx);
  bool Inst_Action3(cAvidaContext& ctx);
  bool Inst_Action4(cAvidaContext& ctx);
  bool Inst_Action5(cAvidaContext& ctx);
  bool Inst_Action6(cAvidaContext& ctx);
  bool Inst_Action7(cAvidaContext& ctx);
  bool Inst_Action8(cAvidaContext& ctx);
  bool Inst_Action9(cAvidaContext& ctx);
  bool Inst_Action10(cAvidaContext& ctx);
  bool Inst_Action11(cAvidaContext& ctx);
  bool Inst_Action12(cAvidaContext& ctx);
  bool Inst_Action13(cAvidaContext& ctx);
  
  // UML instructions used to construct properties
  bool Inst_AbsenceProperty(cAvidaContext& ctx);
  bool Inst_UniversialityProperty(cAvidaContext& ctx);
  bool Inst_ExistenceProperty(cAvidaContext& ctx);
    
};


#ifdef ENABLE_UNIT_TESTS
namespace nHardwareCPU {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  


inline bool cHardwareCPU::ThreadSelect(const int thread_num)
{
  if (thread_num >= 0 && thread_num < m_threads.GetSize()) {
    m_cur_thread = thread_num;
    return true;
  }
  
  return false;
}

inline void cHardwareCPU::ThreadNext()
{
  m_cur_thread++;
  if (m_cur_thread >= GetNumThreads()) m_cur_thread = 0;
}

inline void cHardwareCPU::ThreadPrev()
{
  if (m_cur_thread == 0) m_cur_thread = GetNumThreads() - 1;
  else m_cur_thread--;
}

inline void cHardwareCPU::StackPush(int value)
{
  if (m_threads[m_cur_thread].cur_stack == 0) {
    m_threads[m_cur_thread].stack.Push(value);
  } else {
    m_global_stack.Push(value);
  }
}

inline int cHardwareCPU::StackPop()
{
  int pop_value;

  if (m_threads[m_cur_thread].cur_stack == 0) {
    pop_value = m_threads[m_cur_thread].stack.Pop();
  } else {
    pop_value = m_global_stack.Pop();
  }

  return pop_value;
}

inline void cHardwareCPU::StackFlip()
{
  if (m_threads[m_cur_thread].cur_stack == 0) {
    m_threads[m_cur_thread].stack.Flip();
  } else {
    m_global_stack.Flip();
  }
}

inline int cHardwareCPU::GetStack(int depth, int stack_id, int in_thread) const
{
  int value = 0;

  if(in_thread >= m_threads.GetSize() || in_thread < 0) in_thread = m_cur_thread;

  if (stack_id == -1) stack_id = m_threads[in_thread].cur_stack;

  if (stack_id == 0) value = m_threads[in_thread].stack.Get(depth);
  else if (stack_id == 1) value = m_global_stack.Get(depth);

  return value;
}

inline void cHardwareCPU::StackClear()
{
  if (m_threads[m_cur_thread].cur_stack == 0) {
    m_threads[m_cur_thread].stack.Clear();
  } else {
    m_global_stack.Clear();
  }
}

inline void cHardwareCPU::SwitchStack()
{
  m_threads[m_cur_thread].cur_stack++;
  if (m_threads[m_cur_thread].cur_stack > 1) m_threads[m_cur_thread].cur_stack = 0;
}

#endif
