/*
 *  cChangeList.h
 *  Avida
 *
 *  Called "change_list.hh" prior to 12/2/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2005 California Institute of Technology.
 *
 */

#ifndef cChangeList_h
#define cChangeList_h

#ifndef tArray_h
#include "tArray.h"
#endif
#if USE_tMemTrack
# ifndef tMemTrack_h
#  include "tMemTrack.h"
# endif
#endif


/**
 * This class provides an array of indices of changes to some list of
 * objects. Entries in the array of indices have the same order as
 * registered changes.
 **/ 

class cChangeList
{
#if USE_tMemTrack
  tMemTrack<cChangeList> mt;
#endif
protected:
  /*
  Note that size of m_change_list is size of m_change_tracking, and that
  0 <= m_change_count <= size of m_change_tracking.
  */
  // Number of changes.
  int m_change_count;
  /*
  List of changed indices. When n changes are listed, the first n
  entries of m_change_list store the indices, and the remaining entries
  are invalid.
  */
  tArray<int> m_change_list;
  // m_change_tracking[i] is true iff i is in m_change_list.
  tArray<bool> m_change_tracking;

public:
  explicit cChangeList(int capacity = 0)
    : m_change_count(0), m_change_list(0), m_change_tracking(0)
  {
    ResizeClear(capacity);
  }
  ~cChangeList() { ; }

  void ResizeClear(int capacity);

  int GetSize() const { return m_change_list.GetSize(); }
  int GetChangeCount() const { return m_change_count; }

  // Note that decreasing size invalidates stored changes.
  void Resize(int capacity);
  
  // Unsafe version : assumes index is within change count.
  int GetChangeAt(int index) const { return m_change_list[index]; }

  // Safe version : returns -1 if index is outside change count.
  int CheckChangeAt(int index) const
  {
    return (index < m_change_count) ? ((int) GetChangeAt(index)) : (-1);
  }
  
  // Unsafe version : assumes changed_index is within capacity.
  void MarkChange(int changed_index);

  // Safe version : will resize to accommodate changed_index greater
  // than capacity.
  void PushChange(int changed_index);

  void Reset();

  /**
   * Save to archive
   **/
  template<class Archive>
  void save(Archive & a, const unsigned int version) const {
    /*
    Must convert tArray<bool> m_change_tracking into tArray<int>
    change_tracking, because our serializer doesn't like bools...
    */
    int count = m_change_tracking.GetSize();
    tArray<int> change_tracking(count);
    while(count--) change_tracking[count] = (m_change_tracking[count] == false)?(0):(1);

    a.ArkvObj("m_change_count", m_change_count);
    a.ArkvObj("m_change_list", m_change_list);
    a.ArkvObj("m_change_tracking", change_tracking);
  }
  
  /**
   * Load from archive
   **/
  template<class Archive>
  void load(Archive & a, const unsigned int version){
    tArray<int> change_tracking;

    a.ArkvObj("m_change_count", m_change_count);
    a.ArkvObj("m_change_list", m_change_list);
    a.ArkvObj("m_change_tracking", change_tracking);

    int count = change_tracking.GetSize();
    m_change_tracking.Resize(count);
    while(count--) m_change_tracking[count] = (change_tracking[count] == 0)?(false):(true);
  }

  /**
   * Ask archive to handle loads and saves separately
   **/
  template<class Archive>
  void serialize(Archive & a, const unsigned int version){
    a.SplitLoadSave(*this, version);
  }
};

#endif
