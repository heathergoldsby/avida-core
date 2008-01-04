/*
 *  cResourceCount.h
 *  Avida
 *
 *  Called "resource_count.hh" prior to 12/5/05.
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
 *  Copyright 1993-2001 California Institute of Technology.
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef cResourceCount_h
#define cResourceCount_h

#ifndef cSpatialResCount_h
#include "cSpatialResCount.h"
#endif
#ifndef cString_h
#include "cString.h"
#endif
#ifndef tArray_h
#include "tArray.h"
#endif
#ifndef tMatrix_h
#include "tMatrix.h"
#endif
#ifndef defs_h
#include "defs.h"
#endif

class cResourceCount
{
private:
  mutable tArray<cString> resource_name;
  mutable tArray<double> resource_initial;  // Initial quantity of each resource
  mutable tArray<double> resource_count;  // Current quantity of each resource
  tArray<double> decay_rate;      // Multiplies resource count at each step
  tArray<double> inflow_rate;     // An increment for resource at each step
  tMatrix<double> decay_precalc;  // Precalculation of decay values
  tMatrix<double> inflow_precalc; // Precalculation of inflow values
  tArray<int> geometry;           // Spatial layout of each resource
  mutable tArray<cSpatialResCount> spatial_resource_count;
  mutable tArray<double> curr_grid_res_cnt;
  mutable tArray< tArray<double> > curr_spatial_res_cnt;
  int verbosity;

  // Setup the update process to use lazy evaluation...
  mutable double update_time;     // Portion of an update compleated...
  mutable double spatial_update_time;
  void DoUpdates() const;         // Update resource count based on update time

  // A few constants to describe update process...
  static const double UPDATE_STEP;   // Fraction of an update per step
  static const double EPSILON;       // Tolorance for round off errors
  static const int PRECALC_DISTANCE; // Number of steps to precalculate
  
public:
  cResourceCount(int num_resources = 0);
  cResourceCount(const cResourceCount&);
  ~cResourceCount();

  const cResourceCount& operator=(const cResourceCount&);

  void SetSize(int num_resources);
  void SetCellResources(int cell_id, const tArray<double> & res);

  void Setup(int id, cString name, double initial, double inflow, double decay,
             int in_geometry, double in_xdiffuse, double in_xgravity, 
             double in_ydiffuse, double in_ygravity,
             int in_inflowX1, int in_inflowX2, int in_inflowY1, int in_inflowY2,
             int in_outflowX1, int in_outflowX2, int in_outflowY1, 
             int in_outflowY, tArray<cCellResource> *in_cell_list_ptr,
             int verbosity_level);
  void Update(double in_time);

  int GetSize(void) const { return resource_count.GetSize(); }
  const tArray<double>& ReadResources(void) const { return resource_count; }
  const tArray<double>& GetResources() const;
  const tArray<double>& GetCellResources(int cell_id) const;
  const tArray<int>& GetResourcesGeometry() const;
  const tArray<tArray<double> >& GetSpatialRes();
  void Modify(const tArray<double>& res_change);
  void Modify(int id, double change);
  void ModifyCell(const tArray<double> & res_change, int cell_id);
  void Set(int id, double new_level);
  double Get(int id) const;
  void ResizeSpatialGrids(int in_x, int in_y);
  cSpatialResCount GetSpatialResource(int id) { return spatial_resource_count[id]; }
  void ReinitializeResources();
  cString GetResName(int id) { return resource_name[id]; }
};



#ifdef ENABLE_UNIT_TESTS
namespace nResourceCount {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
