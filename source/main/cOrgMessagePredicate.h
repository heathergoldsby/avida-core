/*
 *  cOrgMessagePredicate.h
 *  Avida
 *
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
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
#ifndef cOrgMessagePredicate_h
#define cOrgMessagePredicate_h

#include <iostream>
#include <functional>
#include <set>

#include "cOrgMessage.h"
#include "cOrganism.h"


/*! \brief An STL-compatible predicate on cOrgMessages.  The intent here is to
provide a straightforward way to track arbitrary messages *wherever* they appear
in the population.  The most utility can be had from message predicates if they're
installed into cStats (since every message goes through cStats). */
struct cOrgMessagePredicate : public std::unary_function<cOrgMessage, bool> 
{
  virtual ~cOrgMessagePredicate() { }
  virtual bool operator()(const cOrgMessage& msg) = 0;
  virtual void Print(std::ostream& out) { }
  virtual void Reset() { }
  virtual bool PreviouslySatisfied() = 0;
  virtual cString GetName() = 0;
  virtual void UpdateStats(cStats& stats) {}
  virtual cDemeCellEvent* GetEvent() { return NULL; }
};


/*! A predicate that returns true and tracks the sending cell_id for messages
that contain the same data field as this predicate was constructed with.
*/
struct cOrgMessagePred_DataEQU : public cOrgMessagePredicate 
{
  cOrgMessagePred_DataEQU(unsigned int data) : m_data(data) { }
  
  virtual bool operator()(const cOrgMessage& msg) {
    if(m_data==msg.GetData()) {
      m_cell_ids.insert(msg.GetSender()->GetCellID());
    }
    return true;
  }
  
  virtual void Print(std::ostream& out) { 
    out << "data==" << m_data << ":{";
    for(std::set<int>::iterator i=m_cell_ids.begin(); i!=m_cell_ids.end(); ++i) {
      out << *i << ",";
    }
    out << "}";
  }
  
  virtual void Reset() { 
    m_cell_ids.clear();
  }
  
  unsigned int m_data;
  std::set<int> m_cell_ids;
};


/*! A predicate that returns true and tracks the label and data field for messages
that contain a sink as the receiver.
*/
struct cOrgMessagePred_SinkReceiverEQU : public cOrgMessagePredicate {
  cOrgMessagePred_SinkReceiverEQU(unsigned int data) : m_data(data) { }
  
  virtual bool operator()(const cOrgMessage& msg) {
    if(m_data==(unsigned int)msg.GetReceiver()->GetCellID()) {
      m_cell_ids.insert(msg.GetData());
//      cWorld* w = msg.GetSender()->getWorld();
      /*      int i = w->GetPopulation().GetCell(289).GetRandomCellID();
      if(msg.GetData() == i) {
        std::cout<<"289 = "<< i<<endl;
      }*/
    }
    return true;
  }
  
  virtual void print(std::ostream& out) { 
//    cPopulationCell::t_id_map& ids = cPopulationCell::GetRandomCellIDMap();
//    int badMSGs = 0;
//    
//    out << "data==" << m_data << ":{";
//    for(std::set<int>::iterator i=m_cell_ids.begin(); i!=m_cell_ids.end(); ++i) {
//      int x,y;
//      //check if # is ID and log cell location and count bad messages
//      if(cPopulationCell::IsRandomCellID(*i)) {
//        cPopulationCell* cell = ids.find(*i)->second;
//        cell->GetPosition(x, y);
//        out << x <<" "<< y << ",";
//        m_cell_ids_total_good.insert(cell->GetRandomCellID());
//      } else {
//        badMSGs++;
//        //        out << *i << ",";
//      }
//    }
//    //write # bad messages in last slot
//    out << badMSGs;
//    out << "} "<<m_cell_ids_total_good.size();
  }
  
  virtual void reset() { 
    m_cell_ids.clear();
  }
  
  unsigned int m_data;
  std::set<int> m_cell_ids;
  std::set<int> m_cell_ids_total_good;
};


/*! A predicate that returns true if a demeCellEvent has been received but the base station
*/
struct cOrgMessagePred_EventReceivedCenter : public cOrgMessagePredicate {
  cOrgMessagePred_EventReceivedCenter(cDemeCellEvent* event, int base_station, int times) : 
  m_base_station(base_station)
  , m_event_received(false)
  , m_stats_updated(false)
  , m_event(event)
  , m_total_times(times)
  , m_current_times(0) { }
  
  ~cOrgMessagePred_EventReceivedCenter() { }
  
  virtual bool operator()(const cOrgMessage& msg) {
    int deme_id = msg.GetSender()->GetOrgInterface().GetDemeID();
    
    if(deme_id != m_event->GetDeme()->GetDemeID() || m_event->IsDead()) {
      return false;
    }
    
    unsigned int eventID = m_event->GetEventID();
    
    if(m_event->IsActive() && eventID != 0 &&
       (eventID == msg.GetData() ||
        eventID == msg.GetLabel())) {
      m_cell_ids.insert(msg.GetSender()->GetCellID());

      if(m_base_station == msg.GetReceiver()->GetCellID()) {
        m_current_times++;
        if(m_current_times >= m_total_times) {
          m_event_received = true;
        }
      }
    }
    return m_event_received;
  }
  
  //need to print update!!!
  virtual void Print(std::ostream& out) {
    if(m_event->IsDead()) {
      return;
    }

    out << m_event->GetEventID() << " [ ";
    for(std::set<int>::iterator i=m_cell_ids.begin(); i!=m_cell_ids.end(); i++) {
      out << *i << " ";
    }
    out << "]\n";
    
    m_cell_ids.clear();
  }
  
  virtual void Reset() { 
    m_event_received = false;
    m_stats_updated = false;
    m_current_times = 0;
  }

  virtual bool PreviouslySatisfied() {
    return m_event_received;
  }

  virtual cString GetName() {
    return "EventReceivedCenter";
  }

  virtual void UpdateStats(cStats& stats) {
    if(m_event_received && !m_stats_updated) {
      int eventCell = m_event->GetNextEventCellID();
      while(eventCell != -1) {
        stats.IncPredSat(eventCell);
        eventCell = m_event->GetNextEventCellID();
      }
      m_stats_updated = true;
    }
  }
  
  cDemeCellEvent* GetEvent() { return m_event; }
  
  int m_base_station;
  bool m_event_received;
  bool m_stats_updated;
  cDemeCellEvent* m_event;
  std::set<int> m_cell_ids;
  int m_total_times;
  int m_current_times;
};

/*! A predicate that returns true if a demeCellEvent has been received but the base station
*/
struct cOrgMessagePred_EventReceivedLeftSide : public cOrgMessagePredicate {
  cOrgMessagePred_EventReceivedLeftSide(cDemeCellEvent* event, cPopulation& population, int times) :
  pop(population)
  , m_event_received(false)
  , m_stats_updated(false)
  , m_event(event)
  , m_total_times(times)
  , m_current_times(0){ }
  
  ~cOrgMessagePred_EventReceivedLeftSide() { }
  
  virtual bool operator()(const cOrgMessage& msg) {
    int deme_id = msg.GetSender()->GetOrgInterface().GetDemeID();
    
    if(deme_id != m_event->GetDeme()->GetDemeID() || m_event->IsDead()) {
      return false;
    }

    if(m_event->IsActive() && 
       ((unsigned int)m_event->GetEventID() == msg.GetData() ||
        (unsigned int)m_event->GetEventID() == msg.GetLabel())) {
      m_cell_ids.insert(msg.GetSender()->GetCellID());
      
      // find receiver coordinates
      cOrganism* receiver = msg.GetReceiver();
      int absolute_cell_ID = receiver->GetCellID();
      int deme_id = receiver->GetOrgInterface().GetDemeID();
      std::pair<int, int> pos = pop.GetDeme(deme_id).GetCellPosition(absolute_cell_ID);  

      // does receiver have x cordinate of zero
      if(pos.first == 0) {
        m_current_times++;
        if(m_current_times >= m_total_times) {
          m_event_received = true;
        }
      }
    }
    return m_event_received;
  }
  
  virtual void Print(std::ostream& out) {
    if(m_event->IsDead()) {
      return;
    }

    out << m_event->GetEventID() << " [ ";
    for(std::set<int>::iterator i=m_cell_ids.begin(); i!=m_cell_ids.end(); i++) {
      out << *i << " ";
    }
    out << "]\n";
    
    m_cell_ids.clear();
  }
  
  virtual void Reset() { 
    m_event_received = false;
    m_stats_updated = false;
    m_current_times = 0;
    m_cell_ids.clear();
  }

  virtual bool PreviouslySatisfied() {
    return m_event_received;
  }

  virtual cString GetName() {
    return "EventReceivedLeftSide";
  }

  virtual void UpdateStats(cStats& stats) {
    if(m_event_received && !m_stats_updated) {
      int eventCell = m_event->GetNextEventCellID();
      while(eventCell != -1) {
        stats.IncPredSat(eventCell);
        eventCell = m_event->GetNextEventCellID();
      }
      m_stats_updated = true;
    }
  }
  
  cDemeCellEvent* GetEvent() { return m_event; }
  
  cPopulation& pop;
  bool m_event_received;
  bool m_stats_updated;
  cDemeCellEvent* m_event;
  std::set<int> m_cell_ids;
  int m_total_times;
  int m_current_times;
};


/*quorum sensing - no births once deme is full*/
/*struct cOrgMessagePred_MinBirths : public cOrgMessagePredicate 
{
	~cOrgMessagePredicate() { }
	bool operator()(const cOrgMessage& msg);
	void Print(std::ostream& out) { }
	void Reset() { }
	bool PreviouslySatisfied();
	cString GetName();
	void UpdateStats(cStats& stats) {}
	cDemeCellEvent* GetEvent() { return NULL; }
};*/

#endif
