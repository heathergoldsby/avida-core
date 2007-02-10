/*
 *  cSchedule.h
 *  Avida
 *
 *  Called "schedule.hh" prior to 12/7/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology
 *
 */

#ifndef cSchedule_h
#define cSchedule_h

#if USE_tMemTrack
# ifndef tMemTrack_h
#  include "tMemTrack.h"
# endif
#endif


/**
 * This class is the base object to handle time-slicing. All other schedulers
 * are derived from this class.  This is a pure virtual class.
 *
 **/

class cMerit;
class cChangeList;

class cSchedule
{
#if USE_tMemTrack
  tMemTrack<cSchedule> mt;
#endif
protected:
  int item_count;
  cChangeList* m_change_list;
  

  cSchedule(); // @not_implemented
  cSchedule(const cSchedule&); // @not_implemented
  cSchedule& operator=(const cSchedule&); // @not_implemented
  
public:
  cSchedule(int _item_count);
  virtual ~cSchedule();

  virtual bool OK() { return true; }
  virtual void Adjust(int item_id, const cMerit & merit) { ; }
  virtual int GetNextID() = 0;
  virtual double GetStatus(int id) { return 0.0; }
  void SetChangeList(cChangeList *change_list);
  cChangeList *GetChangeList() { return m_change_list; }

  void SetSize(int _item_count);
};

#endif
