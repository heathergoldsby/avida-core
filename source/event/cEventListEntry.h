//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2003 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#ifndef EVENT_LIST_ENTRY_HH
#define EVENT_LIST_ENTRY_HH

#include <assert.h>

#ifndef EVENT_HH
#include "cEvent.h"
#endif
#ifndef EVENT_TRIGGERS_HH
#include "cEventTriggers.h"
#endif

/**
 * This class holds a single event in the event list. It is used by the class
 * @ref cEventList. There is no need to use it in any other context.
 **/

class cEvent; // access
class cEventTriggers; // access

class cEventListEntry {
private:
  cEvent *m_event;

  cEventTriggers::eTriggerVariable m_trigger;
  double m_start;
  double m_interval;
  double m_stop;
  double m_original_start;

  cEventListEntry *m_prev;
  cEventListEntry *m_next;


   // not implemented, prevents inadvertent wrong instantiation
  cEventListEntry();
  cEventListEntry( const cEventListEntry & );
  cEventListEntry& operator=( const cEventListEntry& );
public:
  // creators
  cEventListEntry(cEvent *event = NULL,
                  cEventTriggers::eTriggerVariable trigger = cEventTriggers::UPDATE,
                  double start = cEventTriggers::TRIGGER_BEGIN,
                  double interval = cEventTriggers::TRIGGER_ONCE,
                  double stop = cEventTriggers::TRIGGER_END,
                  cEventListEntry *prev = NULL,
                  cEventListEntry *next = NULL ) :
    m_event( event ),
    m_trigger( trigger ),
    m_start( start ),
    m_interval( interval ),
    m_stop( stop ),
    m_original_start( start ),
    m_prev( prev ),
    m_next( next )
  {
  }

  virtual ~cEventListEntry() { delete m_event; }


  // manipulators
  void Setup( cEvent *event,
	      cEventTriggers::eTriggerVariable trigger,
	      double start = cEventTriggers::TRIGGER_BEGIN,
	      double interval = cEventTriggers::TRIGGER_ONCE,
	      double stop = cEventTriggers::TRIGGER_END ){
    m_event = event;
    m_trigger = trigger;
    m_start = start;
    m_original_start = start;
    m_interval = interval;
    m_stop = stop;
  }

  void SetPrev(cEventListEntry * prev) { m_prev = prev; }
  void SetNext(cEventListEntry * next) { m_next = next; }

  void NextInterval() { m_start += m_interval; }
  void Reset() { m_start = m_original_start; }


  // accessors
  cEvent* GetEvent() const {
    assert(m_event!=NULL); return m_event; }

  const cString GetName() const {
    assert(m_event!=NULL); return m_event->GetName(); }
  const cString& GetArgs() const {
    assert(m_event!=NULL); return m_event->GetArgs(); }

  cEventTriggers::eTriggerVariable  GetTrigger()  const { return m_trigger; }
  double            GetStart()    const { return m_start; }
  double            GetInterval() const { return m_interval; }
  double            GetStop()     const { return m_stop; }

  cEventListEntry*  GetPrev()     const { return m_prev; }
  cEventListEntry*  GetNext()     const { return m_next; }

};

#endif