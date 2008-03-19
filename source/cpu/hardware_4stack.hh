//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2003 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#ifndef HARDWARE_4STACK_HH
#define HARDWARE_4STACK_HH

#include "hardware_base.hh"

#include "../defs.hh"

#include "../tools/functions.hh"
#include "../tools/tBuffer.hh"

#include "cpu_memory.hh"
#include "cpu_stack.hh"
#include "cpu_defs.hh"
#include "label.hh"
#include "head.hh"
#include "../main/config.hh"

class cInstSet;
class cInstLibBase;
class cOrganism;
class cMutation;
class cInjectGenotype;

//new-style constant declarations - law 
static const int NUM_LOCAL_STACKS = 3;
static const int NUM_GLOBAL_STACKS = 1;
static const int NUM_STACKS = NUM_LOCAL_STACKS + NUM_GLOBAL_STACKS;
static const int STACK_AX = 0;
static const int STACK_BX = 1;
static const int STACK_CX = 2;
static const int STACK_DX = 3;
static const int NUM_NOPS_4STACK = 4;
static const int NUM_MEMORY_SPACES = 4; 

/**
 * This class is needed to run several threads on a single genome.
 *
 * @see cCPUStack, c4StackHead, cHardware4Stack
 **/

struct cHardware4Stack_Thread {
private:
  int id;

public:
  c4StackHead heads[NUM_HEADS];
  UCHAR cur_head;
  cCPUStack local_stacks[NUM_LOCAL_STACKS];

  int input_pointer;
  bool advance_ip;         // Should the IP advance after this instruction?
  tBuffer<int> input_buf;
  tBuffer<int> output_buf;
  cCodeLabel read_label;
  cCodeLabel next_label;
  // If this thread was spawned by Inject, this will point to the genotype 
  // of the parasite running the thread.  Otherwise, it will be NULL.
  cInjectGenotype* owner;
public:
  cHardware4Stack_Thread(cHardwareBase * in_hardware=NULL, int _id=-1);
  cHardware4Stack_Thread(const cHardware4Stack_Thread & in_thread, int _id=-1);
  ~cHardware4Stack_Thread();

  void operator=(const cHardware4Stack_Thread & in_thread);

  void Reset(cHardwareBase * in_hardware, int _id);
  int GetID() const { return id; }
  void SetID(int _id) { id = _id; }

  void SaveState(std::ostream & fp);
  void LoadState(std::istream & fp);
};

#ifdef SINGLE_IO_BUFFER   // For Single IOBuffer vs IOBuffer for each Thread
# define IO_THREAD 0
#else
# define IO_THREAD cur_thread
#endif

/**
 * Each organism may have a cHardware4Stack structure which keeps track of the
 * current status of all the components of the simulated hardware.
 *
 * @see cHardware4Stack_Thread, cCPUStack, cCPUMemory, cInstSet
 **/

class cInstLib4Stack;
class cHardware4Stack : public cHardwareBase {
public:
  typedef bool (cHardware4Stack::*tHardware4StackMethod)();
private:
  static cInstLib4Stack *s_inst_slib;
  static cInstLib4Stack *initInstLib(void);
  tHardware4StackMethod *m_functions;
private:
  tArray<cCPUMemory> memory_array;          // Memory...
  //cCPUStack global_stack;     // A stack that all threads share.
  cCPUStack global_stacks[NUM_GLOBAL_STACKS];
  //int thread_time_used;

  tArray<cHardware4Stack_Thread> threads;
  int thread_id_chart;
  int cur_thread;

  // Flags...
  bool mal_active;         // Has an allocate occured since last dividehe?
  //bool advance_ip;         // Should the IP advance after this instruction?

  // Instruction costs...
#ifdef INSTRUCTION_COSTS
  tArray<int> inst_cost;
  tArray<int> inst_ft_cost;
#endif

  // Thread slicing...

    // Keeps track of the base thread slicing number for each possible number of threads
  float slice_array[10]; //***HACK!  How do I do this right? -law
                         //this wouldn't compile -> [cConfig::GetMaxCPUThreads()+1]***; 

  // Keeps track of fractional instructions that carry over into next update
  float inst_remainder; 

public:
  cHardware4Stack(cOrganism * in_organism, cInstSet * in_inst_set);
  ~cHardware4Stack();
  void Recycle(cOrganism * new_organism, cInstSet * in_inst_set);
  static cInstLibBase *GetInstLib();
  static cString GetDefaultInstFilename() { return "inst_lib.4stack"; }
  static void WriteDefaultInstSet() { ; }

  void Reset();
  void SingleProcess(std::ostream * trace_fp=NULL);
  bool SingleProcess_PayCosts(const cInstruction & cur_inst);
  bool SingleProcess_ExecuteInst(const cInstruction & cur_inst);
  void LoadGenome(const cGenome & new_genome);

  // --------  Helper methods  --------
  bool OK();
  void PrintStatus(std::ostream & fp);


  // --------  Flag Accessors --------
  bool GetMalActive() const   { return mal_active; }

  // --------  Stack Manipulation...  --------
  inline void StackFlip();
  inline int GetStack(int depth=0, int stack_id=-1, int in_thread=-1) const;
  inline void StackClear();
  inline void SwitchStack();
  cString GetActiveStackID(int stackID) const;
  //retrieves appropriate stack
  inline cCPUStack & Stack(int stack_id); 
  inline const cCPUStack & Stack(int stack_id) const;
  inline cCPUStack & Stack(int stack_id, int in_thread);
  inline const cCPUStack & Stack(int stack_id, int in_thread) const;

  // --------  Tasks & IO  --------
  tBuffer<int> & GetInputBuffer() { return threads[cur_thread].input_buf; }
  tBuffer<int> & GetOutputBuffer() { return threads[cur_thread].output_buf; }
  void DoInput(const int value);
  void DoOutput(const int value);

  // --------  Head Manipulation (including IP)  --------
  inline void SetActiveHead(const int new_head)
  { threads[cur_thread].cur_head = (UCHAR) new_head; }

  int GetCurHead() const { return threads[cur_thread].cur_head; }
  
  const c4StackHead & GetHead(int head_id) const
  { return threads[cur_thread].heads[head_id]; }
  c4StackHead & GetHead(int head_id) 
  { return threads[cur_thread].heads[head_id];}
  
  const c4StackHead & GetHead(int head_id, int thread) const
  { return threads[thread].heads[head_id]; }
  c4StackHead & GetHead(int head_id, int thread) 
  { return threads[thread].heads[head_id];}

  const c4StackHead & GetActiveHead() const { return GetHead(GetCurHead()); }
  c4StackHead & GetActiveHead() { return GetHead(GetCurHead()); }

  void AdjustHeads();

  inline const c4StackHead & IP() const
    { return threads[cur_thread].heads[HEAD_IP]; }
  inline c4StackHead & IP() { return threads[cur_thread].heads[HEAD_IP]; }

  inline const c4StackHead & IP(int thread) const
  { return threads[thread].heads[HEAD_IP]; }
  inline c4StackHead & IP(int thread) 
  { return threads[thread].heads[HEAD_IP]; }


  inline const bool & AdvanceIP() const
    { return threads[cur_thread].advance_ip; }
  inline bool & AdvanceIP() { return threads[cur_thread].advance_ip; }

  // --------  Label Manipulation  -------
  void ReadLabel(int max_size=MAX_LABEL_SIZE);
  const cCodeLabel & GetLabel() const 
    { return threads[cur_thread].next_label; }
  cCodeLabel & GetLabel() { return threads[cur_thread].next_label; }
  const cCodeLabel & GetReadLabel() const
    { return threads[cur_thread].read_label; }
  cCodeLabel & GetReadLabel() { return threads[cur_thread].read_label; }


  // --------  Register Manipulation  --------
  //int Register(int reg_id) const { return threads[cur_thread].reg[reg_id]; }
  //int & Register(int reg_id) { return threads[cur_thread].reg[reg_id]; }

  // --------  Memory Manipulation  --------}
  inline cCPUMemory & GetMemory();
  inline cCPUMemory & GetMemory(int mem_space);
  inline const cCPUMemory & GetMemory(int mem_space) const;
  inline const cCPUMemory & GetMemory() const;

  // --------  Thread Manipulation  --------
  bool ForkThread(); // Adds a new thread based off of cur_thread.
  bool KillThread(); // Kill the current thread!
  inline void PrevThread(); // Shift the current thread in use.
  inline void NextThread();
  inline void SetThread(int value);
  inline cInjectGenotype * GetCurThreadOwner(); 
  inline cInjectGenotype * GetThreadOwner(int in_thread);
  inline void SetThreadOwner(cInjectGenotype * in_genotype);

  // --------  Tests  --------

  int TestParasite() const;

  // --------  Accessors  --------
  //int GetThreadTimeUsed() const { return thread_time_used; }
  int GetNumThreads() const     { return threads.GetSize(); }
  int GetCurThread() const      { return cur_thread; }
  int GetCurThreadID() const    { return threads[cur_thread].GetID(); }

  int GetThreadDist() const {
    if (GetNumThreads() == 1) return 0;
    return threads[0].heads[HEAD_IP].GetPosition() -
      threads[1].heads[HEAD_IP].GetPosition();
  }

  // Complex label manipulation...
  c4StackHead FindLabel(int direction);
  int FindLabel_Forward(const cCodeLabel & search_label,
			  const cGenome & search_genome, int pos);
  int FindLabel_Backward(const cCodeLabel & search_label,
			  const cGenome & search_genome, int pos);
  c4StackHead FindLabel(const cCodeLabel & in_label, int direction);
  c4StackHead FindFullLabel(const cCodeLabel & in_label);

  int GetType() const { return HARDWARE_TYPE_CPU_4STACK; }
  bool InjectParasite(double mut_multiplier);
  bool InjectHost(const cCodeLabel & in_label, const cGenome & injection);
  int InjectThread(const cCodeLabel &, const cGenome &) { return -1; }
  void Mutate(const int mut_point);
  int PointMutate(const double mut_rate);
  int FindFirstEmpty();
  bool isEmpty(int mem_space_used);

  bool TriggerMutations(int trigger);
  bool TriggerMutations(int trigger, c4StackHead & cur_head);
  bool TriggerMutations_ScopeGenome(const cMutation * cur_mut,
        cCPUMemory & target_memory, c4StackHead & cur_head, const double rate);
  bool TriggerMutations_ScopeLocal(const cMutation * cur_mut,
        cCPUMemory & target_memory, c4StackHead & cur_head, const double rate);
  int TriggerMutations_ScopeGlobal(const cMutation * cur_mut,
        cCPUMemory & target_memory, c4StackHead & cur_head, const double rate);
  void TriggerMutations_Body(int type, cCPUMemory & target_memory,
			     c4StackHead & cur_head);

  void ReadInst(const int in_inst);

  void SaveState(std::ostream & fp);
  void LoadState(std::istream & fp);

  void InitInstSet(const cString & filename, cInstSet & inst_set);
  cString ConvertToInstruction(int mem_space_used);


private:
 
 /////////---------- Instruction Helpers ------------//////////

  int FindModifiedStack(int default_stack);
  int FindModifiedHead(int default_head);
  int FindComplementStack(int base_stack);

  void Fault(int fault_loc, int fault_type, cString fault_desc=""); 
  bool Allocate_Necro(const int new_size);
  bool Allocate_Random(const int old_size, const int new_size);
  bool Allocate_Default(const int new_size);
  bool Allocate_Main(const int allocated_size);

  bool Divide_Main(const int mem_space_used, double mut_multiplier=1);
  bool Divide_CheckViable(const int parent_size, const int child_size, const int mem_space);
  void Divide_DoMutations(double mut_multiplier=1);
  void Inject_DoMutations(double mut_multiplier, cCPUMemory & injected_code);
  void Divide_TestFitnessMeasures();

  bool HeadCopy_ErrorCorrect(double reduction);
  bool Inst_HeadDivideMut(double mut_multiplier=1);

public:
  /////////---------- Instruction Library ------------//////////

  //6
  bool Inst_ShiftR();
  //7
  bool Inst_ShiftL();
  //8
  bool Inst_Val_Nand();
  //9
  bool Inst_Val_Add();
  //10
  bool Inst_Val_Sub();
  //11
  bool Inst_Val_Mult();
  //12
  bool Inst_Val_Div();
  //13
  bool Inst_SetMemory();
  //14
  bool Inst_Divide();
  //15
  bool Inst_HeadRead();
  //16
  bool Inst_HeadWrite();
  //??
  bool Inst_HeadCopy();
  //17
  bool Inst_IfEqual();
  //18
  bool Inst_IfNotEqual();
  //19
  bool Inst_IfLess();
  //20
  bool Inst_IfGreater();
  //21
  bool Inst_HeadPush();
  //22
  bool Inst_HeadPop();
  //23
  bool Inst_HeadMove();
  //24
  bool Inst_Search();
  //25
  bool Inst_PushNext();
  //26
  bool Inst_PushPrevious();
  //27
  bool Inst_PushComplement();
  //28
  bool Inst_ValDelete();
  //29
  bool Inst_ValCopy();
  //30
  bool Inst_ForkThread();
  //31
  bool Inst_IfLabel();
  //32
  bool Inst_Increment();
  //33
  bool Inst_Decrement();
  //34
  bool Inst_Mod();
  //35 
  bool Inst_KillThread();
  //36
  bool Inst_IO();
  //37
  bool Inst_Inject();
  
  /*
  bool Inst_InjectRand();
  bool Inst_InjectThread();
  bool Inst_Repro();
  */
 
};


//////////////////
//  cHardware4Stack
//////////////////

//Not used, but here to satisfy the requirements of HardwareBase
inline const cCPUMemory & cHardware4Stack::GetMemory() const
{
  return memory_array[0];
}

//Not used, but here to satisfy the requirements of HardwareBase 
inline cCPUMemory & cHardware4Stack::GetMemory()
{
  return memory_array[0];
}

inline const cCPUMemory & cHardware4Stack::GetMemory(int mem_space) const
{
  if(mem_space >= NUM_MEMORY_SPACES)
    mem_space %= NUM_MEMORY_SPACES;
  return memory_array[mem_space];
}

inline cCPUMemory & cHardware4Stack::GetMemory(int mem_space)
{
 if(mem_space >= NUM_MEMORY_SPACES)
    mem_space %= NUM_MEMORY_SPACES;
  return memory_array[mem_space];
}

inline void cHardware4Stack::NextThread()
{
  cur_thread++;
  if (cur_thread >= GetNumThreads()) cur_thread = 0;
}

inline void cHardware4Stack::PrevThread()
{
  if (cur_thread == 0) cur_thread = GetNumThreads() - 1;
  else cur_thread--;
}

inline void cHardware4Stack::SetThread(int value)
{
  if (value>=0 && value < GetNumThreads())
    cur_thread=value;
}

inline cInjectGenotype * cHardware4Stack::GetCurThreadOwner() 
{ 
  return threads[cur_thread].owner; 
}

inline cInjectGenotype * cHardware4Stack::GetThreadOwner(int thread) 
{ 
  return threads[thread].owner; 
}

inline void cHardware4Stack::SetThreadOwner(cInjectGenotype * in_genotype)
{ 
  threads[cur_thread].owner = in_genotype; 
}

/*inline void cHardware4Stack::StackFlip()
{
  if (threads[cur_thread].cur_stack == 0) {
    threads[cur_thread].stack.Flip();
  } else {
    global_stack.Flip();
  }
}*/

inline int cHardware4Stack::GetStack(int depth, int stack_id, int in_thread) const
{
  if(stack_id<0 || stack_id>NUM_STACKS) stack_id=0;
  
  if(in_thread==-1)
    in_thread=cur_thread;
  
  return Stack(stack_id, in_thread).Get(depth);
}

//inline void cHardware4Stack::StackClear()
//{
  
  //if (threads[cur_thread].cur_stack == 0) {
  //  threads[cur_thread].stack.Clear();
  //} else {
  //  global_stack.Clear();
  //}
//}

//inline void cHardware4Stack::SwitchStack()
//{
//  threads[cur_thread].cur_stack++;
//  if (threads[cur_thread].cur_stack > 1) threads[cur_thread].cur_stack = 0;
//}

inline cCPUStack& cHardware4Stack::Stack(int stack_id)
{
  if(stack_id >= NUM_STACKS)
    {
      stack_id=0;
    }
  if(stack_id < NUM_LOCAL_STACKS)
    return threads[cur_thread].local_stacks[stack_id];
  else
    return global_stacks[stack_id % NUM_LOCAL_STACKS];
}

inline const cCPUStack& cHardware4Stack::Stack(int stack_id) const 
{
  if(stack_id >= NUM_STACKS)
    {
      stack_id=0;
    }
  if(stack_id < NUM_LOCAL_STACKS)
    return threads[cur_thread].local_stacks[stack_id];
  else
    return global_stacks[stack_id % NUM_LOCAL_STACKS];
}

inline cCPUStack& cHardware4Stack::Stack(int stack_id, int in_thread) 
{
  if(stack_id >= NUM_STACKS)
      stack_id=0;
  if(in_thread >= threads.GetSize())
      in_thread=cur_thread;

  if(stack_id < NUM_LOCAL_STACKS)
    return threads[in_thread].local_stacks[stack_id];
  else
    return global_stacks[stack_id % NUM_LOCAL_STACKS];
}

inline const cCPUStack& cHardware4Stack::Stack(int stack_id, int in_thread) const 
{
  if(stack_id >= NUM_STACKS)
      stack_id=0;
  if(in_thread >= threads.GetSize())
      in_thread=cur_thread;

  if(stack_id < NUM_LOCAL_STACKS)
    return threads[in_thread].local_stacks[stack_id];
  else
    return global_stacks[stack_id % NUM_LOCAL_STACKS];
}

#endif
