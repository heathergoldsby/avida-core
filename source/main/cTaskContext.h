/*
 *  cTaskContext.h
 *  Avida
 *
 *  Created by David on 3/29/06.
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

#ifndef cTaskContext_h
#define cTaskContext_h

#ifndef cOrgInterface_h
#include "cOrgInterface.h"
#endif
#ifndef tBuffer_h
#include "tBuffer.h"
#endif
#ifndef tList_h
#include "tList.h"
#endif
#ifndef tHashTable_h
#include "tHashTable.h"
#endif
#ifndef cOrganism
#include "cOrganism.h"
#endif


class cTaskEntry;
class cTaskState;
class cOrganism;


class cTaskContext
{

  friend class cTaskLib;

private:
  cOrgInterface* m_interface;
  const tBuffer<int>& m_input_buffer;
  const tBuffer<int>& m_output_buffer;
  const tList<tBuffer<int> >& m_other_input_buffers;
  const tList<tBuffer<int> >& m_other_output_buffers;
  bool m_net_valid;
  int m_net_completed;
  tBuffer<int>* m_received_messages;
  int m_logic_id;
  bool m_on_divide;
  bool task_success_complete;

  
  cTaskEntry* m_task_entry;
  tHashTable<void*, cTaskState*>* m_task_states;
  cOrganism* organism;


public:
  cTaskContext(cOrgInterface* interface, const tBuffer<int>& inputs, const tBuffer<int>& outputs,
               const tList<tBuffer<int> >& other_inputs, const tList<tBuffer<int> >& other_outputs,
               bool in_net_valid, int in_net_completed, bool in_on_divide = false,
               tBuffer<int>* in_received_messages = NULL, cOrganism* in_org = NULL)
    : m_interface(interface)
    , m_input_buffer(inputs)
    , m_output_buffer(outputs)
    , m_other_input_buffers(other_inputs)
    , m_other_output_buffers(other_outputs)
    , m_net_valid(in_net_valid)
    , m_net_completed(in_net_completed)
    , m_received_messages(in_received_messages)
    , m_logic_id(0)
    , m_on_divide(in_on_divide)
    , m_task_entry(NULL)
    , m_task_states(NULL)
    , organism(in_org)
    , task_success_complete(true)

  {
  }
  
  inline int GetInputAt(int index) { return m_interface->GetInputAt(index); }
  inline const tBuffer<int>& GetInputBuffer() { return m_input_buffer; }
  inline const tBuffer<int>& GetOutputBuffer() { return m_output_buffer; }
  inline const tList<tBuffer<int> >& GetNeighborhoodInputBuffers() { return m_other_input_buffers; }
  inline const tList<tBuffer<int> >& GetNeighborhoodOutputBuffers() { return m_other_output_buffers; }
  inline bool NetIsValid() const { return m_net_valid; }
  inline int GetNetCompleted() const { return m_net_completed; }
  inline tBuffer<int>* GetReceivedMessages() { return m_received_messages; }
  inline int GetLogicId() const { return m_logic_id; }
  inline void SetLogicId(int v) { m_logic_id = v; }
  inline bool GetOnDivide() const { return m_on_divide; }
  
  inline void SetTaskEntry(cTaskEntry* in_entry) { m_task_entry = in_entry; }
  inline cTaskEntry* GetTaskEntry() { return m_task_entry; }
  
  inline void SetTaskStates(tHashTable<void*, cTaskState*>* states) { m_task_states = states; }
  
  inline cTaskState* GetTaskState()
  {
    cTaskState* ret = NULL;
    m_task_states->Find(m_task_entry, ret);
    return ret;
  }
  inline void AddTaskState(cTaskState* value) { m_task_states->Add(m_task_entry, value); }
};


#ifdef ENABLE_UNIT_TESTS
namespace nTaskContext {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
