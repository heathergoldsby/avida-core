/*
 *  cEventList.h
 *  Avida
 *
 *  Called "event_list.hh" prior to 12/2/05.
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

#ifndef cEventList_h
#define cEventList_h

#ifndef defs_h
#include "defs.h"
#endif

#ifndef cAction_h
#include "cAction.h"
#endif

#if USE_tMemTrack
# ifndef tMemTrack_h
#  include "tMemTrack.h"
# endif
#endif

#ifndef tList_h
#include "tList.h"
#endif


class cAvidaContext;
class cString;
class cWorld;


// This is the fundamental class for event management. It holds a list of all
// events, and provides methods to add new events and to process existing
// events.

class cEventList
{
#if USE_tMemTrack
  tMemTrack<cEventList> mt;
#endif
public:
  enum eTriggerType { UPDATE, GENERATION, IMMEDIATE, EVENT, UNDEFINED };
  
  static const double TRIGGER_BEGIN;
  static const double TRIGGER_END;
  static const double TRIGGER_ALL;
  static const double TRIGGER_ONCE;
  static const double TRIGGER_EVENT;


private:
  
  class cEventTriggerEntry
  {
    private:
      cAction* m_action;
      cString  m_name;
      eEventTrigger m_trigger;
    public:
      cEventTriggerEntry(cAction* action, const cString& name, eEventTrigger trigger) : m_action(action), m_name(name), m_trigger(trigger) {;}
      cString  GetName() const {return m_name;}
      cAction* GetAction() const {return m_action;}
      eEventTrigger GetEventTrigger() const {return m_trigger;}
  };
  
  class cEventListEntry
  {
  private:
    cAction* m_action;
    cString m_name;
    
    eTriggerType m_trigger;
    double m_start;
    double m_interval;
    double m_stop;
    double m_original_start;
    
    cEventListEntry* m_prev;
    cEventListEntry* m_next;
  
    
  public:
  
    cEventListEntry(cAction* action, const cString& name, eTriggerType trigger = UPDATE, double start = TRIGGER_BEGIN,
                    double interval = TRIGGER_ONCE, double stop = TRIGGER_END, cEventListEntry* prev = NULL,
                    cEventListEntry* next = NULL) :
      m_action(action), m_name(name), m_trigger(trigger), m_start(start), m_interval(interval), m_stop(stop),
      m_original_start(start), m_prev(prev), m_next(next)
    {
    }
    
    virtual ~cEventListEntry() { delete m_action; }
    
    void SetPrev(cEventListEntry* prev) { m_prev = prev; }
    void SetNext(cEventListEntry* next) { m_next = next; }
    
    void NextInterval() { m_start += m_interval; }
    void Reset() { m_start = m_original_start; }
    
    // accessors
    cAction* GetAction() const { assert(m_action != NULL); return m_action; }
    
    const cString GetName() const { assert(m_action != NULL); return m_name; }
    const cString& GetArgs() const { assert(m_action != NULL); return m_action->GetArgs(); }
    
    eTriggerType GetTrigger() const { return m_trigger; }
    double GetStart() const { return m_start; }
    double GetInterval() const { return m_interval; }
    double GetStop() const { return m_stop; }
    
    cEventListEntry* GetPrev() const { return m_prev; }
    cEventListEntry* GetNext() const { return m_next; }
  };
  
  
  cWorld* m_world;
  cEventListEntry* m_head;
  cEventListEntry* m_tail;
  tList<cEventTriggerEntry> m_trigger_events;
  int m_num_events;


  void SyncEvent(cEventListEntry* event);
  double GetTriggerValue(eTriggerType trigger) const;
  void Delete(cEventListEntry* entry);

  cEventList(); // @not_implemented
  cEventList(const cEventList&); // @not_implemented
  cEventList& operator=(const cEventList&); // @not_implemented

  
public:
  /**
   * The cEventList assumes ownership of triggers and destroys it when done.
   *
   * @param triggers A trigger object. The event list needs a trigger object
   * to determine what events to call when.
   **/
  cEventList(cWorld* world) : m_world(world), m_head(NULL), m_tail(NULL), m_num_events(0) { ; }
  ~cEventList();
    

  /**
   * Adds an event with given name and argument list. The event will be of
   * type immediate, i.e. it is processed only once, and then deleted.
   *
   * @param name The name of the event.
   * @param args The argument list.
   **/
  bool AddEvent(const cString& name, const cString& args)
  {
    return AddEvent(IMMEDIATE, TRIGGER_BEGIN, TRIGGER_ONCE, TRIGGER_END, name, args);
  }


  /**
   * Adds an event with specified trigger type.
   *
   * @param trigger The type of the trigger.
   * @param start The start value of the trigger variable.
   * @param interval The length of the interval between one processing
   * and the next.
   * @param stop The value of the trigger variable at which the event should
   * be deleted.
   * @param name The name of the even.
   * @param args The argument list.
   **/
  bool AddEvent(eTriggerType trigger, double start, double interval, double stop, const cString &name, const cString& args);

  /**
   * This function adds an event that is given in the event list file format.
   * In other words, it can be used to parse one line from an event list file,
   * and construct the appropriate event.
   **/
  bool AddEventFileFormat(const cString& line);


  bool LoadEventFile(const cString& filename);

  void Process(cAvidaContext& ctx);   // Go through list executing appropriate events.
  void Sync(); // Get all events caught up.

  void PrintEventList(std::ostream& os = std::cout);
  
  eEventTrigger ParseEventCode(cString event);
  bool AddTriggerEvent(const cString& trigger, const cString& name, const cString& arg);
  bool TriggerEvent(eEventTrigger id, cAvidaContext& ctx);
};


#ifdef ENABLE_UNIT_TESTS
namespace nEventList {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
