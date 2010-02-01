/*
 *  cDeme.h
 *  Avida
 *
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

#ifndef cDeme_h
#define cDeme_h

#include "cDemeCellEvent.h"
#include "cGermline.h"
#include "tArray.h"
#include "cResourceCount.h"
#include "cStringList.h"
#include "cAvidaContext.h"

class cPopulation;
class cResource;
class cWorld;
class cGenotype;
class cOrganism;

/*! Demes are groups of cells in the population that are somehow bound together
as a unit.  The deme object is used from within cPopulation to manage these 
groups. */



class cDeme
{
private:
  cWorld* m_world;
  cPopulation* m_population;
  tArray<int> cell_ids;
  int width; //!< Width of this deme.
  int birth_count; //!< Number of organisms that have been born into this deme since reset.
  int org_count; //!< Number of organisms are currently in this deme.
  int _age; //!< Age of this deme, in updates.
  int time_used;
  double cur_normalized_time_used;
  double last_normalized_time_used;
  
  int m_instset_id;  //@MRR Cludging together linkage to cHardwareManager
  
  cGermline _germline; //!< The germline for this deme, if used.

  cDeme(const cDeme&); // @not_implemented
  
  cResourceCount deme_resource_count; //!< Resources available to the deme
  tArray<int> energy_res_ids; //!< IDs of energy resources
  
  tArray<cDemeCellEvent> cell_events;
  
public:
  cDeme() : width(0), birth_count(0), org_count(0), _age(0), m_instset_id(0), deme_resource_count(0) { ; }
  ~cDeme() { ; }

  void Setup(const tArray<int>& in_cells, int in_width = -1, cWorld* world = NULL);

  int GetSize() const { return cell_ids.GetSize(); }
  int GetCellID(int pos) const { return cell_ids[pos]; }
  int GetCellID(int x, int y) const;
  //! Returns an (x,y) pair for the position of the passed-in cell ID.
  std::pair<int, int> GetCellPosition(int cellid) const;

  //@MRR
  int GetInsetSetID() const { return m_instset_id; }
  void SetInstSetID(int id)  { cDeme::ForEachDemeOrganism(cDeme::SynchInstSet); }
  static void SynchInstSet(cOrganism*, cDeme*);
  void ForEachDemeOrganism( void (*func)(cOrganism*, cDeme*) );
  
  int GetWidth() const { return width; }
  int GetHeight() const { return cell_ids.GetSize() / width; }

  void Reset();
  int GetBirthCount() const { return birth_count; }
  void IncBirthCount() { birth_count++; }

  int GetOrgCount() const { return org_count; }
  void IncOrgCount() { org_count++; }
  void DecOrgCount() { org_count--; }

  bool IsEmpty() const { return org_count == 0; }
  bool IsFull() const { return org_count == cell_ids.GetSize(); }
  
  // -= Germline =-
  //! Returns this deme's germline.
  cGermline& GetGermline() { return _germline; }
  //! Replaces this deme's germline.
  void ReplaceGermline(const cGermline& germline);
  
  // -= Update support =-
  //! Called once, at the end of every update.
  void ProcessUpdate();
  /*! Returns the age of this deme, updates.  Age is defined as the number of 
    updates since the last time Reset() was called. */
  int GetAge() const { return _age; }
  
  void Sterilize() const;
  void ReplaceDeme(const cDeme& source);
  void SeedDeme(const cGenotype& genotype, int cell_id);
  
  const cResourceCount& GetDemeResourceCount() const { return deme_resource_count; }
  void SetDemeResourceCount(const cResourceCount in_res) { deme_resource_count = in_res; }
  void ResizeSpatialGrids(const int in_x, const int in_y) { deme_resource_count.ResizeSpatialGrids(in_x, in_y); }
  void ModifyDemeResCount(const tArray<double> & res_change, const int absolute_cell_id);
  double GetAndClearCellEnergy(int absolute_cell_id);
  void GiveBackCellEnergy(int absolute_cell_id, double value);
  void SetupDemeRes(int id, cResource * res, int verbosity);
  void UpdateDemeRes() { deme_resource_count.GetResources(); }
  void Update(double time_step) { deme_resource_count.Update(time_step); }
  int GetRelativeCellID(int absolute_cell_id) { return absolute_cell_id % GetSize(); } //!< assumes all demes are the same size

  void IncTimeUsed(double merit) { time_used++; cur_normalized_time_used += 1.0/merit/(double)org_count; }
  int GetTimeUsed() { return time_used; }
  
  void SetCellEvent(int x1, int y1, int x2, int y2, int delay, int duration);
};

#endif
