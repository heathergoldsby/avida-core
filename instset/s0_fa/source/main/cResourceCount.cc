/*
 *  cResourceCount.cc
 *  Avida
 *
 *  Called "resource_count.cc" prior to 12/5/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2001 California Institute of Technology.
 *
 */

#include "cResourceCount.h"

#include "nGeometry.h"

extern "C" {
#include <math.h>
}

using namespace std;

const double cResourceCount::UPDATE_STEP(1.0 / 10000.0);
const double cResourceCount::EPSILON (1.0e-15);
const int cResourceCount::PRECALC_DISTANCE(100);


void FlowMatter(cSpatialCountElem &elem1, cSpatialCountElem &elem2, 
                double inxdiffuse, 
                double inydiffuse, double inxgravity, double inygravity,
                int xdist, int ydist, double dist) {

  /* Routine to calculate the amount of flow from one Element to another.
     Amount of flow is a function of:

       1) Amount of material in each cell (will try to equalize)
       2) Distance between each cell
       3) x and y "gravity"

     This method only effect the delta amount of each element.  The State
     method will need to be called at the end of each time step to complete
     the movement of material.
  */

  double  diff, flowamt, xgravity, xdiffuse, ygravity,  ydiffuse;

  if (((elem1.amount == 0.0) && (elem2.amount == 0.0)) && (dist < 0.0)) return;
  diff = (elem1.amount - elem2.amount);
  if (xdist != 0) {

    /* if there is material to be effected by x gravity */

    if (((xdist>0) && (inxgravity>0.0)) || ((xdist<0) && (inxgravity<0.0))) {
      xgravity = elem1.amount * fabs(inxgravity)/3.0;
    } else {
      xgravity = -elem2.amount * fabs(inxgravity)/3.0;
    }
    
    /* Diffusion uses the diffusion constant x half the difference (as the 
       elements attempt to equalize) / the number of possible neighbors (8) */

    xdiffuse = inxdiffuse * diff / 16.0;
  } else {
    xdiffuse = 0.0;
    xgravity = 0.0;
  }  
  if (ydist != 0) {

    /* if there is material to be effected by y gravity */

    if (((ydist>0) && (inygravity>0.0)) || ((ydist<0) && (inygravity<0.0))) {
      ygravity = elem1.amount * fabs(inygravity)/3.0;
    } else {
      ygravity = -elem2.amount * fabs(inygravity)/3.0;
    }
    ydiffuse = inydiffuse * diff / 16.0;
  } else {
    ydiffuse = 0.0;
    ygravity = 0.0;
  }  

  flowamt = ((xdiffuse + ydiffuse + xgravity + ygravity)/
             (fabs(xdist*1.0) + fabs(ydist*1.0)))/dist;
  elem1.delta -= flowamt;
  elem2.delta += flowamt;
}

cResourceCount::cResourceCount(int num_resources)
  : update_time(0.0)
  , spatial_update_time(0.0)
{
  if(num_resources > 0) {
    SetSize(num_resources);
  }

  return;
}

cResourceCount::cResourceCount(const cResourceCount &rc) {
  *this = rc;

  return;
}

const cResourceCount &cResourceCount::operator=(const cResourceCount &rc) {
  resource_count = rc.resource_count;
  decay_rate = rc.decay_rate;
  inflow_rate = rc.inflow_rate;
  decay_precalc = rc.decay_precalc;
  inflow_precalc = rc.inflow_precalc;
  geometry = rc.geometry;
  spatial_resource_count = rc.spatial_resource_count;
  curr_grid_res_cnt = rc.curr_grid_res_cnt;
  curr_spatial_res_cnt = rc.curr_spatial_res_cnt;
  update_time = rc.update_time;
  spatial_update_time = rc.spatial_update_time;

  return *this;
}

void cResourceCount::SetSize(int num_resources)
{
  resource_count.ResizeClear(num_resources);
  decay_rate.ResizeClear(num_resources);
  inflow_rate.ResizeClear(num_resources);
  if(num_resources > 0) {
    decay_precalc.ResizeClear(num_resources, PRECALC_DISTANCE+1);
    inflow_precalc.ResizeClear(num_resources, PRECALC_DISTANCE+1);
  }
  geometry.ResizeClear(num_resources);
  spatial_resource_count.ResizeClear(num_resources);
  curr_grid_res_cnt.ResizeClear(num_resources);
  curr_spatial_res_cnt.ResizeClear(num_resources);

  resource_count.SetAll(0.0);
  decay_rate.SetAll(0.0);
  inflow_rate.SetAll(0.0);
  decay_precalc.SetAll(1.0); // This is 1-inflow, so there should be no inflow by default, JEB
  inflow_precalc.SetAll(0.0);
  geometry.SetAll(nGeometry::GLOBAL);
  curr_grid_res_cnt.SetAll(0.0);
}

cResourceCount::~cResourceCount()
{
}

void cResourceCount::SetCellResources(int cell_id, const tArray<double> & res)
{
  assert(resource_count.GetSize() == res.GetSize());

  for (int i = 0; i < resource_count.GetSize(); i++) {
    if (geometry[i] == nGeometry::GLOBAL) {
      // Set global quantity of resource
    } else {
      spatial_resource_count[i].SetCellAmount(cell_id, res[i]);

      /* Ideally the state of the cell's resource should not be set till
         the end of the update so that all processes (inflow, outflow, 
         diffision, gravity and organism demand) have the same weight.  However
         waiting can cause problems with negative resources so we allow
         the organism demand to work immediately on the state of the resource */ 
    }
  }
}

void cResourceCount::Setup(int id, cString name, double initial, double inflow,
                           double decay, int in_geometry, double in_xdiffuse,
                           double in_xgravity, double in_ydiffuse, 
                           double in_ygravity, int in_inflowX1, 
                           int in_inflowX2, int in_inflowY1, 
                           int in_inflowY2, int in_outflowX1, 
                           int in_outflowX2, int in_outflowY1, 
                           int in_outflowY2, tArray<cCellResource> *in_cell_list_ptr,
                           int verbosity_level)
{
  assert(id >= 0 && id < resource_count.GetSize());
  assert(initial >= 0.0);
  assert(decay >= 0.0);
  assert(inflow >= 0.0);
  assert(spatial_resource_count[id].GetSize() > 0);

  cString geo_name;
  if (in_geometry == nGeometry::GLOBAL) {
    geo_name = "GLOBAL";
  } else if (in_geometry == nGeometry::GRID) {
    geo_name = "GRID";
  } else if (in_geometry == nGeometry::TORUS) {
    geo_name = "TORUS";
  }

  /* If the verbose flag is set print out information about resources */

  if (verbosity_level > VERBOSE_NORMAL) {
    cout << "Setting up resource " << name
         << "(" << geo_name 
         << ") with initial quatity=" << initial
         << ", decay=" << decay
         << ", inflow=" << inflow
         << endl;
    if ((in_geometry == nGeometry::GRID) || (in_geometry == nGeometry::TORUS)) {
      cout << "  Inflow rectangle (" << in_inflowX1 
           << "," << in_inflowY1 
           << ") to (" << in_inflowX2 
           << "," << in_inflowY2 
           << ")" << endl; 
      cout << "  Outflow rectangle (" << in_outflowX1 
           << "," << in_outflowY1 
           << ") to (" << in_outflowX2 
           << "," << in_outflowY2 
           << ")" << endl;
      cout << "  xdiffuse=" << in_xdiffuse
           << ", xgravity=" << in_xgravity
           << ", ydiffuse=" << in_ydiffuse
           << ", ygravity=" << in_ygravity
           << endl;
    }   
  }

  resource_count[id] = initial;
  spatial_resource_count[id].RateAll
                              (initial/spatial_resource_count[id].GetSize());
  spatial_resource_count[id].StateAll();  
  decay_rate[id] = decay;
  inflow_rate[id] = inflow;
  geometry[id] = in_geometry;
  spatial_resource_count[id].SetGeometry(in_geometry);
  spatial_resource_count[id].SetPointers();
  spatial_resource_count[id].SetCellList(in_cell_list_ptr);

  double step_decay = pow(decay, UPDATE_STEP);
  double step_inflow = inflow * UPDATE_STEP;
  
  decay_precalc(id, 0) = 1.0;
  inflow_precalc(id, 0) = 0.0;
  for (int i = 1; i <= PRECALC_DISTANCE; i++) {
    decay_precalc(id, i)  = decay_precalc(id, i-1) * step_decay;
    inflow_precalc(id, i) = inflow_precalc(id, i-1) * step_decay + step_inflow;
  }
  spatial_resource_count[id].SetXdiffuse(in_xdiffuse);
  spatial_resource_count[id].SetXgravity(in_xgravity);
  spatial_resource_count[id].SetYdiffuse(in_ydiffuse);
  spatial_resource_count[id].SetYgravity(in_ygravity);
  spatial_resource_count[id].SetInflowX1(in_inflowX1);
  spatial_resource_count[id].SetInflowX2(in_inflowX2);
  spatial_resource_count[id].SetInflowY1(in_inflowY1);
  spatial_resource_count[id].SetInflowY2(in_inflowY2);
  spatial_resource_count[id].SetOutflowX1(in_outflowX1);
  spatial_resource_count[id].SetOutflowX2(in_outflowX2);
  spatial_resource_count[id].SetOutflowY1(in_outflowY1);
  spatial_resource_count[id].SetOutflowY2(in_outflowY2);
}

void cResourceCount::Update(double in_time) 
{ 
  update_time += in_time;
  spatial_update_time += in_time;
 }

 
const tArray<double> & cResourceCount::GetResources() const
{
  DoUpdates();
  return resource_count;
}
 
const tArray<double> & cResourceCount::GetCellResources(int cell_id) const

  // Get amount of the resource for a given cell in the grid.  If it is a
  // global resource pass out the entire content of that resource.

{
  int num_resources = resource_count.GetSize();
  DoUpdates();

  for (int i = 0; i < num_resources; i++) {
    if (geometry[i] == nGeometry::GLOBAL) {
      curr_grid_res_cnt[i] = resource_count[i];
    } else {
      curr_grid_res_cnt[i] = spatial_resource_count[i].GetAmount(cell_id);
    }
  }
  return curr_grid_res_cnt;

}

const tArray<int> & cResourceCount::GetResourcesGeometry() const
{
  return geometry;
}

const tArray< tArray<double> > &  cResourceCount::GetSpatialRes()
{
  const int num_spatial_resources = spatial_resource_count.GetSize();
  if (num_spatial_resources > 0) {
    const int num_cells = spatial_resource_count[0].GetSize();
    DoUpdates();
    for (int i = 0; i < num_spatial_resources; i++) {
      for (int j = 0; j < num_cells; j++) {
	curr_spatial_res_cnt[i][j] = spatial_resource_count[i].GetAmount(j);
      }
    }
  }

  return curr_spatial_res_cnt;
}

void cResourceCount::Modify(const tArray<double> & res_change)
{
  assert(resource_count.GetSize() == res_change.GetSize());

  for (int i = 0; i < resource_count.GetSize(); i++) {
    resource_count[i] += res_change[i];
    assert(resource_count[i] >= 0.0);
  }
}


void cResourceCount::Modify(int id, double change)
{
  assert(id < resource_count.GetSize());

  resource_count[id] += change;
  assert(resource_count[id] >= 0.0);
}

void cResourceCount::ModifyCell(const tArray<double> & res_change, int cell_id)
{
  assert(resource_count.GetSize() == res_change.GetSize());

  for (int i = 0; i < resource_count.GetSize(); i++) {
    if (geometry[i] == nGeometry::GLOBAL) {
      resource_count[i] += res_change[i];
      assert(resource_count[i] >= 0.0);
    } else {
      spatial_resource_count[i].Rate(cell_id, res_change[i]);

      /* Ideally the state of the cell's resource should not be set till
         the end of the update so that all processes (inflow, outflow, 
         diffision, gravity and organism demand) have the same weight.  However
         waiting can cause problems with negative resources so we allow
         the organism demand to work immediately on the state of the resource */ 
    
      spatial_resource_count[i].State(cell_id);
    }
  }
}


void cResourceCount::Set(int id, double new_level)
{
  assert(id < resource_count.GetSize());

  resource_count[id] = new_level;
}

void cResourceCount::ResizeSpatialGrids(int in_x, int in_y)
{
  for (int i = 0; i < resource_count.GetSize(); i++) {
    spatial_resource_count[i].ResizeClear(in_x, in_y, geometry[i]);
    curr_spatial_res_cnt[i].Resize(in_x * in_y);
  }
}
///// Private Methods /////////
void cResourceCount::DoUpdates() const
{ 
  assert(update_time >= -EPSILON);

  // Determine how many update steps have progressed
  int num_steps = (int) (update_time / UPDATE_STEP);

  // Preserve remainder of update_time
  update_time -=  num_steps * UPDATE_STEP;

  while (num_steps > PRECALC_DISTANCE) {
    for (int i = 0; i < resource_count.GetSize(); i++) {
      if (geometry[i] == nGeometry::GLOBAL) {
        resource_count[i] *= decay_precalc(i, PRECALC_DISTANCE);
        resource_count[i] += inflow_precalc(i, PRECALC_DISTANCE);
      }
    }
    num_steps -= PRECALC_DISTANCE;
  }

  for (int i = 0; i < resource_count.GetSize(); i++) {
    if (geometry[i] == nGeometry::GLOBAL) {
      resource_count[i] *= decay_precalc(i, num_steps);
      resource_count[i] += inflow_precalc(i, num_steps);
    }
  }

  // If one (or more) complete update has occured update the spatial resources

  while (spatial_update_time >= 1.0) { 
    spatial_update_time -= 1.0;
    for (int i = 0; i < resource_count.GetSize(); i++) {
      if (geometry[i] != nGeometry::GLOBAL) {
        spatial_resource_count[i].Source(inflow_rate[i]);
        spatial_resource_count[i].Sink(decay_rate[i]);
        if (spatial_resource_count[i].GetCellListSize() > 0) {
          spatial_resource_count[i].CellInflow();
          spatial_resource_count[i].CellOutflow();
        }
        spatial_resource_count[i].FlowAll();
        spatial_resource_count[i].StateAll();
        resource_count[i] = spatial_resource_count[i].SumAll();
      }
    }
  }
}
