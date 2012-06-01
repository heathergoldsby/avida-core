/*
 *  cPopulationInterface.cc
 *  Avida
 *
 *  Called "pop_interface.cc" prior to 12/5/05.
 *  Copyright 1999-2011 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
 *
 *
 *  This file is part of Avida.
 *
 *  Avida is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  Avida is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License along with Avida.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "cPopulationInterface.h"

#include "apto/platform.h"

#include "cDeme.h"
#include "cEnvironment.h"
#include "cHardwareManager.h"
#include "cOrganism.h"
#include "cOrgSinkMessage.h"
#include "cOrgMessage.h"
#include "cPopulation.h"
#include "cStats.h"
#include "cTestCPU.h"
#include "cRandom.h"
#include "cInstSet.h"

#include <cassert>
#include <algorithm>
#include <iterator>

#ifndef NULL
#define NULL 0
#endif

#if APTO_PLATFORM(WINDOWS)
namespace std
{
  /*inline __int64  abs(__int64 i) { return _abs64(i); }*/
}
#endif


cPopulationInterface::cPopulationInterface(cWorld* world) 
: m_world(world)
, m_cell_id(-1)
, m_deme_id(-1)
, m_prevseen_cell_id(-1)
, m_prev_task_cell(-1)
, m_num_task_cells(0)
, m_hgt_support(NULL)
{
}

cPopulationInterface::~cPopulationInterface() {
	if(m_hgt_support) {
		delete m_hgt_support;
	}
}

cOrganism* cPopulationInterface::GetOrganism() {
	return GetCell()->GetOrganism();
}

const tSmartArray <cOrganism*> cPopulationInterface::GetLiveOrgList() const {
  return m_world->GetPopulation().GetLiveOrgList();
}

cPopulationCell* cPopulationInterface::GetCell() { 
	return &m_world->GetPopulation().GetCell(m_cell_id);
}

cPopulationCell* cPopulationInterface::GetCell(int cell_id) { 
	return &m_world->GetPopulation().GetCell(cell_id);
}

int cPopulationInterface::GetCellXPosition()
{
  std::pair<int, int> pos = m_world->GetPopulation().GetDeme(m_deme_id).GetCellPosition(m_cell_id);
  return pos.first;
}

int cPopulationInterface::GetCellYPosition()
{
  std::pair<int, int> pos = m_world->GetPopulation().GetDeme(m_deme_id).GetCellPosition(m_cell_id);
  return pos.second;
}

cPopulationCell* cPopulationInterface::GetCellFaced() {
	return &GetCell()->GetCellFaced();
}

cDeme* cPopulationInterface::GetDeme() {
  return &m_world->GetPopulation().GetDeme(m_deme_id);
}

int cPopulationInterface::GetCellData() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellData();
}

int cPopulationInterface::GetCellDataOrgID() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellDataOrgID();
}

int cPopulationInterface::GetCellDataUpdate() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellDataUpdate();
}

int cPopulationInterface::GetCellDataTerritory() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellDataTerritory();
}

int cPopulationInterface::GetCellDataForagerType() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellDataForagerType();
}

int cPopulationInterface::GetFacedCellData() {
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced().GetCellData();
}

int cPopulationInterface::GetFacedCellDataOrgID() {
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced().GetCellDataOrgID();
}

int cPopulationInterface::GetFacedCellDataUpdate() {
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced().GetCellDataUpdate();
}

int cPopulationInterface::GetFacedCellDataTerritory() {
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced().GetCellDataTerritory();
}

void cPopulationInterface::SetCellData(const int newData) {
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  cell.SetCellData(newData, cell.GetOrganism()->GetID());
}

bool cPopulationInterface::Divide(cAvidaContext& ctx, cOrganism* parent, const Genome& offspring_genome)
{
  assert(parent != NULL);
  assert(m_world->GetPopulation().GetCell(m_cell_id).GetOrganism() == parent);
  return m_world->GetPopulation().ActivateOffspring(ctx, offspring_genome, parent);
}

cOrganism* cPopulationInterface::GetNeighbor()
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  return cell.ConnectionList().GetFirst()->GetOrganism();
}

bool cPopulationInterface::IsNeighborCellOccupied() {
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  return cell.ConnectionList().GetFirst()->IsOccupied();
}

int cPopulationInterface::GetNumNeighbors()
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  return cell.ConnectionList().GetSize();
}

void cPopulationInterface::GetNeighborhoodCellIDs(tArray<int>& list)
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  list.Resize(cell.ConnectionList().GetSize());
  tConstListIterator<cPopulationCell> it(cell.ConnectionList());
  int i = 0;
  while (it.Next() != NULL) list[i++] = it.Get()->GetID();
}

int cPopulationInterface::GetFacing()
{
	cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
	assert(cell.IsOccupied());
	return cell.GetFacing();
}

int cPopulationInterface::GetFacedCellID()
{
	cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced();
	return cell.GetID();
}

int cPopulationInterface::GetFacedDir()
{
	cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
	assert(cell.IsOccupied());
	return cell.GetFacedDir();
}

int cPopulationInterface::GetNeighborCellContents() {
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  return cell.ConnectionList().GetFirst()->GetCellData();
}

void cPopulationInterface::Rotate(int direction)
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
	
  if (m_world->GetConfig().USE_AVATARS.Get()) {
    if (direction >= 0) RotateAV(1);
    else RotateAV(-1);
  }
  else {
    if (direction >= 0) cell.ConnectionList().CircNext();
    else cell.ConnectionList().CircPrev();
  }
}

int cPopulationInterface::GetInputAt(int& input_pointer)
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  return cell.GetInputAt(input_pointer);
}

void cPopulationInterface::ResetInputs(cAvidaContext& ctx) 
{ 
  m_world->GetPopulation().GetCell(m_cell_id).ResetInputs(ctx); 
}

const tArray<int>& cPopulationInterface::GetInputs() const
{
  return m_world->GetPopulation().GetCell(m_cell_id).GetInputs();
}

const tArray<double>& cPopulationInterface::GetResources(cAvidaContext& ctx) 
{
  return m_world->GetPopulation().GetCellResources(m_cell_id, ctx); 
}

const tArray<double>& cPopulationInterface::GetFacedCellResources(cAvidaContext& ctx) 
{
  return m_world->GetPopulation().GetCellResources(GetCell()->GetCellFaced().GetID(), ctx); 
}

const tArray<double>& cPopulationInterface::GetCellResources(int cell_id, cAvidaContext& ctx) 
{
  return m_world->GetPopulation().GetCellResources(cell_id, ctx); 
}

const tArray<double>& cPopulationInterface::GetFrozenResources(cAvidaContext& ctx, int cell_id) 
{
  return m_world->GetPopulation().GetFrozenResources(ctx, cell_id); 
}

const tArray<double>& cPopulationInterface::GetDemeResources(int deme_id, cAvidaContext& ctx) 
{
  return m_world->GetPopulation().GetDemeCellResources(deme_id, m_cell_id, ctx); 
}

const tArray< tArray<int> >& cPopulationInterface::GetCellIdLists()
{
	return m_world->GetPopulation().GetCellIdLists();
}

int cPopulationInterface::GetCurrPeakX(cAvidaContext& ctx, int res_id) 
{ 
  return m_world->GetPopulation().GetCurrPeakX(ctx, res_id); 
} 

int cPopulationInterface::GetCurrPeakY(cAvidaContext& ctx, int res_id) 
{ 
  return m_world->GetPopulation().GetCurrPeakY(ctx, res_id); 
} 

int cPopulationInterface::GetFrozenPeakX(cAvidaContext& ctx, int res_id) 
{ 
  return m_world->GetPopulation().GetFrozenPeakX(ctx, res_id); 
} 

int cPopulationInterface::GetFrozenPeakY(cAvidaContext& ctx, int res_id) 
{ 
  return m_world->GetPopulation().GetFrozenPeakY(ctx, res_id); 
} 

void cPopulationInterface::TriggerDoUpdates(cAvidaContext& ctx)
{
  m_world->GetPopulation().TriggerDoUpdates(ctx);
}

void cPopulationInterface::UpdateResources(cAvidaContext& ctx, const tArray<double>& res_change)
{
  return m_world->GetPopulation().UpdateCellResources(ctx, res_change, m_cell_id);
}

void cPopulationInterface::UpdateDemeResources(cAvidaContext& ctx, const tArray<double>& res_change)
{
  return m_world->GetPopulation().UpdateDemeCellResources(ctx, res_change, m_cell_id);
}

void cPopulationInterface::Die(cAvidaContext& ctx) 
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  m_world->GetPopulation().KillOrganism(cell, ctx);
}

void cPopulationInterface::KillCellID(int target, cAvidaContext& ctx) 
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(target);
  m_world->GetPopulation().KillOrganism(cell, ctx); 
}

void cPopulationInterface::Kaboom(int distance, cAvidaContext& ctx) 
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  m_world->GetPopulation().Kaboom(cell, ctx, distance); 
}

void cPopulationInterface::SpawnDeme(cAvidaContext& ctx) 
{
  // const int num_demes = m_world->GetPopulation().GetNumDemes();
	
  // Spawn the current deme; no target ID will put it into a random deme.
  const int deme_id = m_world->GetPopulation().GetCell(m_cell_id).GetDemeID();
	
  m_world->GetPopulation().SpawnDeme(deme_id, ctx); 
}

cOrgSinkMessage* cPopulationInterface::NetReceive()
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  switch(m_world->GetConfig().NET_STYLE.Get())
  {
		case 1: // Receiver Facing
    {
      cOrganism* cur_neighbor = cell.ConnectionList().GetFirst()->GetOrganism();
      cOrgSinkMessage* msg = NULL;
      if (cur_neighbor != NULL && (msg = cur_neighbor->NetPop()) != NULL) return msg;
    }
			break;
			
		case 0: // Random Next - First Available
		default:
    {
      const int num_neighbors = cell.ConnectionList().GetSize();
      for (int i = 0; i < num_neighbors; i++) {
        cell.ConnectionList().CircNext();
        
        cOrganism* cur_neighbor = cell.ConnectionList().GetFirst()->GetOrganism();
        cOrgSinkMessage* msg = NULL;
        if (cur_neighbor != NULL && (msg = cur_neighbor->NetPop()) != NULL ) return msg;
      }
    }
			break;
  }
  
  return NULL;
}

bool cPopulationInterface::NetRemoteValidate(cAvidaContext& ctx, cOrgSinkMessage* msg)
{
  cOrganism* org = m_world->GetPopulation().GetCell(msg->GetSourceID()).GetOrganism();
  return (org != NULL && org->NetRemoteValidate(ctx, msg->GetOriginalValue()));
}

int cPopulationInterface::ReceiveValue()
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  const int num_neighbors = cell.ConnectionList().GetSize();
  for (int i = 0; i < num_neighbors; i++) {
    cell.ConnectionList().CircNext();
    
    cOrganism* cur_neighbor = cell.ConnectionList().GetFirst()->GetOrganism();
    if (cur_neighbor == NULL || cur_neighbor->GetSentActive() == false) {
      continue;
    }
    
    return cur_neighbor->RetrieveSentValue();
  }
  
  return 0;
}

void cPopulationInterface::SellValue(const int data, const int label, const int sell_price, const int org_id)
{
	m_world->GetPopulation().AddSellValue(data, label, sell_price, org_id, m_cell_id);
}

int cPopulationInterface::BuyValue(const int label, const int buy_price)
{
	int value = m_world->GetPopulation().BuyValue(label, buy_price, m_cell_id);
	return value;
}

bool cPopulationInterface::InjectParasite(cOrganism* host, cBioUnit* parent, const cString& label, const Sequence& injected_code)
{
  assert(parent != NULL);
  assert(m_world->GetPopulation().GetCell(m_cell_id).GetOrganism() == host);
  
  return m_world->GetPopulation().ActivateParasite(host, parent, label, injected_code);
}

bool cPopulationInterface::UpdateMerit(double new_merit)
{
  return m_world->GetPopulation().UpdateMerit(m_cell_id, new_merit);
}

bool cPopulationInterface::TestOnDivide()
{
  return m_world->GetTestOnDivide();
}


/*! Internal-use method to consolidate message-sending code.
 */
bool cPopulationInterface::SendMessage(cOrgMessage& msg, cPopulationCell& rcell)
{
  bool dropped = false;
  bool lost = false;

  static const double drop_prob = m_world->GetConfig().NET_DROP_PROB.Get();
  if ((drop_prob > 0.0) && m_world->GetRandom().P(drop_prob)) {
    // message dropped
    GetDeme()->messageDropped();
    GetDeme()->messageSendFailed();
    dropped = true;
  }

  // Fail if the cell we're facing is not occupied.
  if(!rcell.IsOccupied()) lost = true;

  if (lost) GetDeme()->messageSendFailed();

  // record this message, regardless of whether it's actually received.
  if (m_world->GetConfig().NET_LOG_MESSAGES.Get()) {
    m_world->GetStats().LogMessage(msg, dropped, lost);
  }

  if (dropped || lost) return false;

  cOrganism* recvr = rcell.GetOrganism();
  assert(recvr != 0);
  recvr->ReceiveMessage(msg);
  m_world->GetStats().SentMessage(msg);
  GetDeme()->MessageSuccessfullySent();
  return true;
}

bool cPopulationInterface::SendMessage(cOrgMessage& msg, int cellid)
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(cellid);
  return SendMessage(msg, cell);
}

/*! Send a message to the faced organism, failing if this cell does not have 
 neighbors or if the cell currently faced is not occupied.
 */
bool cPopulationInterface::SendMessage(cOrgMessage& msg)
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied()); // This organism; sanity.

  cPopulationCell* rcell = cell.ConnectionList().GetFirst();
  assert(rcell != 0); // Cells should never be null.
  return SendMessage(msg, *rcell);
}


int cPopulationInterface::CheckForDemeTask(cAvidaContext& ctx, int value)
{
  return GetDeme()->CheckForTask(ctx, value);
}

/*! Send a message to the faced organism, failing if this cell does not have 
 neighbors or if the cell currently faced is not occupied. */
bool cPopulationInterface::BroadcastMessage(cOrgMessage& msg, int depth) {
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied()); // This organism; sanity.
	
	// Get the set of cells that are within range.
	std::set<cPopulationCell*> cell_set;
	cell.GetNeighboringCells(cell_set, depth);
	
	// Remove this cell from the set!
	cell_set.erase(&cell);
	
	// Now, send a message towards each cell:
	for(std::set<cPopulationCell*>::iterator i=cell_set.begin(); i!=cell_set.end(); ++i) {
		SendMessage(msg, **i);
	}
	return true;
}


bool cPopulationInterface::BcastAlarm(int jump_label, int bcast_range) {
  bool successfully_sent(false);
  cPopulationCell& scell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(scell.IsOccupied()); // This organism; sanity.
	
  const int ALARM_SELF = m_world->GetConfig().ALARM_SELF.Get(); // does an alarm affect the sender; 0=no  non-0=yes
  
  if(bcast_range > 1) { // multi-hop messaging
    cDeme& deme = m_world->GetPopulation().GetDeme(GetDemeID());
    for(int i = 0; i < deme.GetSize(); i++) {
      int possible_receiver_id = deme.GetCellID(i);
      cPopulationCell& rcell = m_world->GetPopulation().GetCell(possible_receiver_id);
			
      if(rcell.IsOccupied() && possible_receiver_id != GetCellID()) {
        //check distance
        pair<int, int> sender_pos = deme.GetCellPosition(GetCellID());
        pair<int, int> possible_receiver_pos = deme.GetCellPosition(possible_receiver_id);
        int hop_distance = max( abs(sender_pos.first  - possible_receiver_pos.first),
															 abs(sender_pos.second - possible_receiver_pos.second));
        if(hop_distance <= bcast_range) {
          // send alarm to organisms
          cOrganism* recvr = rcell.GetOrganism();
          assert(recvr != NULL);
          recvr->moveIPtoAlarmLabel(jump_label);
          successfully_sent = true;
        }
      }
    }
  } else { // single hop messaging
    for(int i = 0; i < scell.ConnectionList().GetSize(); i++) {
      cPopulationCell* rcell = scell.ConnectionList().GetPos(i);
      assert(rcell != NULL); // Cells should never be null.
			
      // Fail if the cell we're facing is not occupied.
      if(!rcell->IsOccupied())
        continue;
      cOrganism* recvr = rcell->GetOrganism();
      assert(recvr != NULL);
      recvr->moveIPtoAlarmLabel(jump_label);
      successfully_sent = true;
    }
  }
  
  if(ALARM_SELF) {
    scell.GetOrganism()->moveIPtoAlarmLabel(jump_label);
  }
  return successfully_sent;
}

void cPopulationInterface::DivideOrgTestamentAmongDeme(double value){
  cDeme* deme = GetDeme();
  for(int i = 0; i < deme->GetSize(); i++) {
    cPopulationCell& cell = deme->GetCell(i);
    if(cell.IsOccupied()) {
      cOrganism* org = cell.GetOrganism();
      org->GetPhenotype().EnergyTestament(value/deme->GetOrgCount());
    }
  }
}

/*! Send a flash to all neighboring organisms. */
void cPopulationInterface::SendFlash() {
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
	
  for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
    cPopulationCell* neighbor = cell.ConnectionList().GetFirst();
    if(neighbor->IsOccupied()) {
      neighbor->GetOrganism()->ReceiveFlash();
    }
    cell.ConnectionList().CircNext();
  }
}

int cPopulationInterface::GetStateGridID(cAvidaContext& ctx)
{
  return ctx.GetRandom().GetUInt(m_world->GetEnvironment().GetNumStateGrids());
}

/* Rotate an organism to face the neighbor with the highest reputation */
void cPopulationInterface::RotateToGreatestReputation() 
{
	
	cPopulationCell& cell = m_world->GetPopulation().GetCell(GetCellID());
	int high_rep=-1; 
	vector <int> high_rep_orgs;
	
	
	// loop to find the max reputation
	for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
		const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
		// cell->organism, if occupied, check reputation, etc.
		if (IsNeighborCellOccupied()) {
			cOrganism* cur_neighbor = faced_cell->GetOrganism();
			
			// if it has high reputation	
			if (cur_neighbor->GetReputation() >= high_rep) {
				if (cur_neighbor->GetReputation() > high_rep) {
					high_rep = cur_neighbor->GetReputation();
					high_rep_orgs.clear();
				}
				high_rep_orgs.push_back(cur_neighbor->GetID()); 
			} 
		}
		
		// check the next neighbor
		cell.ConnectionList().CircNext();
	}
	
	// Pick an organism to donate to
	
	if (high_rep_orgs.size() > 0) {
		unsigned int rand_num = m_world->GetRandom().GetUInt(0, high_rep_orgs.size()); 
		int high_org_id = high_rep_orgs[rand_num];
		
		for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
			const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
			
			if (IsNeighborCellOccupied()) {
				
				cOrganism* cur_neighbor = faced_cell->GetOrganism();
				
				// if it has high reputation	
				if (cur_neighbor->GetID() == high_org_id) {
					break;
				}
			}
			
			cell.ConnectionList().CircNext();
			
		}
	}
	
}

/* Rotate an organism to face the neighbor with the highest reputation 
 where the neighbor has a different tag than the organism*/
void cPopulationInterface::RotateToGreatestReputationWithDifferentTag(int tag) 
{
	
	cPopulationCell& cell = m_world->GetPopulation().GetCell(GetCellID());
	int high_rep=-1; 
	vector <int> high_rep_orgs;
	
	// loop to find the max reputation
	for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
		const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
		// cell->organism, if occupied, check reputation, etc.
		if (IsNeighborCellOccupied()) {
			cOrganism* cur_neighbor = faced_cell->GetOrganism();
			
			// if it has high reputation	
			if ((cur_neighbor->GetTagLabel() != tag) && (cur_neighbor->GetReputation() >= high_rep)) {
				if (cur_neighbor->GetReputation() > high_rep) {
					high_rep = cur_neighbor->GetReputation();
					high_rep_orgs.clear();
				}
				high_rep_orgs.push_back(cur_neighbor->GetID()); 
			} 
		}
		
		// check the next neighbor
		cell.ConnectionList().CircNext();
	}
	
	// Pick an organism to donate to
	
	if (high_rep_orgs.size() > 0) {
		unsigned int rand_num = m_world->GetRandom().GetUInt(0, high_rep_orgs.size()); 
		int high_org_id = high_rep_orgs[rand_num];
		
		for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
			const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
			
			if (IsNeighborCellOccupied()) {
				
				cOrganism* cur_neighbor = faced_cell->GetOrganism();
				
				// if it has high reputation	
				if (cur_neighbor->GetID() == high_org_id) {
					break;
				}
			}
			
			cell.ConnectionList().CircNext();
			
		}
		
		
		
	}
	
}

/* Rotate an organism to face the neighbor with the highest reputation 
 where the neighbor has a different tag than the organism*/
void cPopulationInterface::RotateToGreatestReputationWithDifferentLineage(int line) 
{
	
	cPopulationCell& cell = m_world->GetPopulation().GetCell(GetCellID());
	int high_rep=-1; 
	vector <int> high_rep_orgs;
	
	// loop to find the max reputation
	for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
		const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
		// cell->organism, if occupied, check reputation, etc.
		if (IsNeighborCellOccupied()) {
			cOrganism* cur_neighbor = faced_cell->GetOrganism();
			
			// if it has high reputation	
			if ((cur_neighbor->GetLineageLabel() != line) && (cur_neighbor->GetReputation() >= high_rep)) {
				if (cur_neighbor->GetReputation() > high_rep) {
					high_rep = cur_neighbor->GetReputation();
					high_rep_orgs.clear();
				}
				high_rep_orgs.push_back(cur_neighbor->GetID()); 
			} 
		}
		
		// check the next neighbor
		cell.ConnectionList().CircNext();
	}
	
	// Pick an organism to donate to
	
	if (high_rep_orgs.size() > 0) {
		unsigned int rand_num = m_world->GetRandom().GetUInt(0, high_rep_orgs.size()); 
		int high_org_id = high_rep_orgs[rand_num];
		
		for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
			const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
			
			if (IsNeighborCellOccupied()) {
				
				cOrganism* cur_neighbor = faced_cell->GetOrganism();
				
				// if it has high reputation	
				if (cur_neighbor->GetID() == high_org_id) {
					break;
				}
			}
			
			cell.ConnectionList().CircNext();
			
		}
	}	
}

/*! Link this organism's cell to the cell it is currently facing.
 */
void cPopulationInterface::CreateLinkByFacing(double weight) {
	cDeme* deme = GetDeme(); assert(deme);
	cPopulationCell* this_cell = GetCell(); assert(this_cell);
	cPopulationCell* that_cell = GetCellFaced(); assert(that_cell);
	deme->GetNetwork().Connect(*this_cell, *that_cell, weight);
}

/*! Link this organism's cell to the cell with coordinates (x,y).
 */
void cPopulationInterface::CreateLinkByXY(int x, int y, double weight) {
	cDeme* deme = GetDeme(); assert(deme);
	cPopulationCell* this_cell = GetCell(); assert(this_cell);
	// the static casts here are to fix a problem with -2^31 being sent in as a 
	// cell coordinate.  the problem is that a 2s-complement int can hold a negative
	// number whose absolute value is too large for the int to hold.  when this happens,
	// abs returns the value unmodified.
	int cellx = std::abs(static_cast<long long int>(x)) % deme->GetWidth();
	int celly = std::abs(static_cast<long long int>(y)) % deme->GetHeight();
	assert(cellx >= 0);
	assert(cellx < deme->GetWidth());
	assert(celly >= 0);
	assert(celly < deme->GetHeight());
	deme->GetNetwork().Connect(*this_cell, deme->GetCell(cellx, celly), weight);
}

/*! Link this organism's cell to the cell with index idx.
 */
void cPopulationInterface::CreateLinkByIndex(int idx, double weight) {
	cDeme* deme = GetDeme(); assert(deme);
	cPopulationCell* this_cell = GetCell(); assert(this_cell);
	// the static casts here are to fix a problem with -2^31 being sent in as a 
	// cell coordinate.  the problem is that a 2s-complement int can hold a negative
	// number whose absolute value is too large for the int to hold.  when this happens,
	// abs returns the value unmodified.
	int that_cell = std::abs(static_cast<long long int>(idx)) % deme->GetSize();
	assert(that_cell >= 0);
	assert(that_cell < deme->GetSize());
	deme->GetNetwork().Connect(*this_cell, deme->GetCell(that_cell), weight);
}

/*! Broadcast a message to all organisms that are connected by this network.
 */
bool cPopulationInterface::NetworkBroadcast(cOrgMessage& msg) {	
	cDeme* deme = GetDeme(); assert(deme);
	cPopulationCell* this_cell = GetCell(); assert(this_cell);
	deme->GetNetwork().BroadcastToNeighbors(*this_cell, msg, this);
	return true;
}

/*! Unicast a message to the current selected organism.
 */
bool cPopulationInterface::NetworkUnicast(cOrgMessage& msg) {
	cDeme* deme = GetDeme(); assert(deme);
	cPopulationCell* this_cell = GetCell(); assert(this_cell);
	deme->GetNetwork().Unicast(*this_cell, msg, this);
	return true;
}

/*! Rotate to select a new network link.
 */
bool cPopulationInterface::NetworkRotate(int x) {
	cDeme* deme = GetDeme(); assert(deme);
	cPopulationCell* this_cell = GetCell(); assert(this_cell);
	deme->GetNetwork().Rotate(*this_cell, x);
	return true;
}

/*! Select a new network link.
 */
bool cPopulationInterface::NetworkSelect(int x) {
	cDeme* deme = GetDeme(); assert(deme);
	cPopulationCell* this_cell = GetCell(); assert(this_cell);
	deme->GetNetwork().Select(*this_cell, x);
	return true;
}

/* If the cell is turned on for deme input, retrieves the deme's next input value.
 */
int cPopulationInterface::GetNextDemeInput(cAvidaContext& ctx)
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  if (cell.GetCanInput()) {
    cDeme* deme = GetDeme();
    return deme->GetNextDemeInput(ctx, deme->GetRelativeCellID(m_cell_id));
  }
  return -1;
}

// If the cell is turned on for deme input, adds the value to the deme's input buffer.
void cPopulationInterface::DoDemeInput(int value)
{
  if (m_world->GetPopulation().GetCell(m_cell_id).GetCanInput()) {
    GetDeme()->DoDemeInput(value);
  }
}

// If the cell is turned on for deme output, adds the value to the deme's output buffer.
void cPopulationInterface::DoDemeOutput(cAvidaContext& ctx, int value)
{
  double cell_bonus = m_world->GetPopulation().GetCell(m_cell_id).GetOutputBonus();
  if (cell_bonus > 0.0) {
    GetDeme()->DoDemeOutput(ctx, value, cell_bonus);
  }
}


/*! Called when this individual is the donor organism during conjugation.
 
 This method causes this individual to "donate" a fragment of its own genome to
 another organism selected from the population.
 */
void cPopulationInterface::DoHGTDonation(cAvidaContext& ctx) {
	cPopulationCell* target=0;
	
	switch(m_world->GetConfig().HGT_CONJUGATION_METHOD.Get()) {
		case 0: { // selected at random from neighborhood
			std::set<cPopulationCell*> occupied_cell_set;
			GetCell()->GetOccupiedNeighboringCells(occupied_cell_set, 1);
			if(occupied_cell_set.size()==0) {
				// nothing to do here, there are no neighbors
				return;
			}
			std::set<cPopulationCell*>::iterator selected=occupied_cell_set.begin();
			std::advance(selected, ctx.GetRandom().GetInt(occupied_cell_set.size()));
			target = *selected;
			break;
		}
		case 1: { // faced individual
			target = GetCellFaced();
			if(!target->IsOccupied()) {
				// nothing to do, we're facing an empty cell.
				return;
			}
			break;
		}
		default: {
			m_world->GetDriver().RaiseFatalException(1, "HGT_CONJUGATION_METHOD is set to an invalid value.");
			break;
		}
	}
	assert(target != 0);
	fragment_list_type fragments;
	cGenomeUtil::RandomSplit(ctx, 
													 m_world->GetConfig().HGT_FRAGMENT_SIZE_MEAN.Get(),
													 m_world->GetConfig().HGT_FRAGMENT_SIZE_VARIANCE.Get(),
													 GetOrganism()->GetGenome().GetSequence(),
													 fragments);
	target->GetOrganism()->GetOrgInterface().ReceiveHGTDonation(fragments[ctx.GetRandom().GetInt(fragments.size())]);
}


/*! Called when this organism "requests" an HGT conjugation.
 
 Technically, organisms don't request an HGT conjugation.  However, this provides
 an alternative to population-level conjugational events.  Specifically, whenever
 an organism replicates, there is the possibility that its offspring conjugates
 with another organism in the population -- that is what we check here.
 
 This method is closely related to HGT donation, except here we're looking for
 the donatOR, instead of the donatEE.
 */
void cPopulationInterface::DoHGTConjugation(cAvidaContext& ctx) {
	cPopulationCell* source=0;
	
	switch(m_world->GetConfig().HGT_CONJUGATION_METHOD.Get()) {
		case 0: { // selected at random from neighborhood
			std::set<cPopulationCell*> occupied_cell_set;
			GetCell()->GetOccupiedNeighboringCells(occupied_cell_set, 1);
			if(occupied_cell_set.size()==0) {
				// nothing to do here, there are no neighbors
				return;
			}
			std::set<cPopulationCell*>::iterator selected=occupied_cell_set.begin();
			std::advance(selected, ctx.GetRandom().GetInt(occupied_cell_set.size()));
			source = *selected;
			break;
		}
		case 1: { // faced individual
			source = GetCellFaced();
			if(!source->IsOccupied()) {
				// nothing to do, we're facing an empty cell.
				return;
			}
			break;
		}
		default: {
			m_world->GetDriver().RaiseFatalException(1, "HGT_CONJUGATION_METHOD is set to an invalid value.");
			break;
		}
	}
	assert(source != 0);
	fragment_list_type fragments;
	cGenomeUtil::RandomSplit(ctx, 
													 m_world->GetConfig().HGT_FRAGMENT_SIZE_MEAN.Get(),
													 m_world->GetConfig().HGT_FRAGMENT_SIZE_VARIANCE.Get(),
													 source->GetOrganism()->GetGenome().GetSequence(),
													 fragments);
	ReceiveHGTDonation(fragments[ctx.GetRandom().GetInt(fragments.size())]);	
}


/*! Perform an HGT mutation on this offspring. 
 
 HGT mutations are location-dependent, hence they are piped through the population
 interface as opposed to being implemented in the CPU or organism.

 There is the possibility that more than one HGT mutation occurs when this method 
 is called.
 */
void cPopulationInterface::DoHGTMutation(cAvidaContext& ctx, Genome& offspring) {
	InitHGTSupport();
	
	// first, gather up all the fragments that we're going to be inserting into this offspring:
	// these come from a per-replication conjugational event:
	if((m_world->GetConfig().HGT_CONJUGATION_P.Get() > 0.0)
		 && (ctx.GetRandom().P(m_world->GetConfig().HGT_CONJUGATION_P.Get()))) {
		DoHGTConjugation(ctx);
	}	
	
	// the pending list includes both the fragments selected via the above process,
	// as well as from population-level conjugational events (see cPopulationActions.cc:cActionAvidianConjugation).
	fragment_list_type& fragments = m_hgt_support->_pending;

	// these come from "natural" competence (ie, eating the dead):
	if((m_world->GetConfig().HGT_COMPETENCE_P.Get() > 0.0)
		 && (ctx.GetRandom().P(m_world->GetConfig().HGT_COMPETENCE_P.Get()))) {
		
		// get this organism's cell:
		cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
		
		// the hgt source controls where the genetic material for HGT comes from.
		switch(m_world->GetConfig().HGT_SOURCE.Get()) {
			case 0: { // source is other genomes, nothing to do here (default)
				break;
			}
			case 1: { // source is the parent (a control)
				// this is a little hackish, but this is the cleanest way to make sure
				// that all downstream stuff works right.
				cell.ClearFragments(ctx);
				cell.AddGenomeFragments(ctx, cell.GetOrganism()->GetGenome().GetSequence());
				break;
			}
			default: { // error
				m_world->GetDriver().RaiseFatalException(1, "HGT_SOURCE is set to an invalid value.");
				break;
			}
		}
		
		// do we have any fragments available?
		if(cell.CountGenomeFragments() > 0) {
			// add a randomly-selected fragment to the list of fragments to be HGT'd,
			// remove it from the cell, and adjust the level of HGT resource.
			fragment_list_type::iterator selected=cell.GetFragments().begin();
			std::advance(selected, ctx.GetRandom().GetInt(cell.GetFragments().size()));
			fragments.insert(fragments.end(), *selected);			
			m_world->GetPopulation().AdjustHGTResource(ctx, -selected->GetSize());
			cell.GetFragments().erase(selected);
		}
	}
	
	// now, for each fragment being HGT'd, figure out where to put it:
	for(fragment_list_type::iterator i=fragments.begin(); i!=fragments.end(); ++i) {
		cGenomeUtil::substring_match location;
		switch(m_world->GetConfig().HGT_FRAGMENT_SELECTION.Get()) {
			case 0: { // match placement
				HGTMatchPlacement(ctx, offspring.GetSequence(), i, location);
				break;
			}
			case 1: { // match placement with redundant instruction trimming
				HGTTrimmedPlacement(ctx, offspring.GetSequence(), i, location);
				break;
			}
			case 2: { // random placement
				HGTRandomPlacement(ctx, offspring.GetSequence(), i, location);
				break;
			}
			default: { // error
				m_world->GetDriver().RaiseFatalException(1, "HGT_FRAGMENT_SELECTION is set to an invalid value.");
				break;
			}
		}
		
		// at this stage, we have a fragment and a location we're going to put it.
		// there are various transformations to this fragment that we could perform,
		// more as controls than anything else.
		switch(m_world->GetConfig().HGT_FRAGMENT_XFORM.Get()) {
			case 0: { // no transformation.
				break;
			}
			case 1: { // random shuffle of the instructions in the fragment.
				cGenomeUtil::RandomShuffle(ctx, *i);
				break;
			}
			case 2: { // replace the instructions in the fragment with random instructions.
				const cInstSet& instset = m_world->GetHardwareManager().GetInstSet(offspring.GetInstSet());
				for(int j=0; j<i->GetSize(); ++j) {
					(*i)[j] = instset.GetRandomInst(ctx);
				}
				break;
			}
			default: { // error
				m_world->GetDriver().RaiseFatalException(1, "HGT_FRAGMENT_XFORM is set to an invalid value.");
				break;
			}				
		}
		
		// do the mutation; we currently support insertions and replacements, but this can
		// be extended in the same way as fragment selection if need be.
		if(ctx.GetRandom().P(m_world->GetConfig().HGT_INSERTION_MUT_P.Get())) {
			// insert the fragment just after the final location:
			offspring.GetSequence().Insert(location.end, *i);
		} else {
			// replacement: replace [begin,end) instructions in the genome with the fragment,
			// respecting circularity.
			offspring.GetSequence().Replace(*i, location.begin, location.end);
		}
		
		// stats tracking:
		m_world->GetStats().GenomeFragmentInserted(GetOrganism(), *i, location);
	}
	
	// clean-up; be sure to empty the pending list so that we don't end up doing an HGT
	// operation multiple times on the same fragment.
	fragments.clear();
}


/*! Place the fragment at the location of best match.
 */
void cPopulationInterface::HGTMatchPlacement(cAvidaContext& ctx, const Sequence& offspring,
																						 fragment_list_type::iterator& selected,
																						 substring_match& location) {
	// find the location within the offspring's genome that best matches the selected fragment:
	location = cGenomeUtil::FindUnbiasedCircularMatch(ctx, offspring, *selected);
}


/*! Place the fragment at the location of best match, with redundant instructions trimmed.

 In this fragment selection method, the
 match location within the genome is calculated on a "trimmed" fragment.  Specifically,
 the trimmed fragment has all duplicate instructions at its end removed prior to the match.
 
 Mutations to the offspring are still performed using the entire fragment, so this effectively
 increases the insertion rate.  E.g., hgt(abcde, abcccc) -> abccccde.
 */
void cPopulationInterface::HGTTrimmedPlacement(cAvidaContext& ctx, const Sequence& offspring,
																											 fragment_list_type::iterator& selected,
																											 substring_match& location) {
	// copy the selected fragment, trimming redundant instructions at the end:
	Sequence trimmed(*selected);
	while((trimmed.GetSize() >= 2) && (trimmed[trimmed.GetSize()-1] == trimmed[trimmed.GetSize()-2])) {
		trimmed.Remove(trimmed.GetSize()-1);
	}
	
	// find the location within the offspring's genome that best matches the selected fragment:
	location = cGenomeUtil::FindUnbiasedCircularMatch(ctx, offspring, trimmed);
}


/*! Place the fragment at a random location.
 
 Here we select a random location for the fragment within the offspring.
 The beginning of the fragment location is selected at random, while the end is selected a
 random distance (up to the length of the selected fragment * 2) instructions away.
 */
void cPopulationInterface::HGTRandomPlacement(cAvidaContext& ctx, const Sequence& offspring,
																											fragment_list_type::iterator& selected,
																											substring_match& location) {
	// select a random location within the offspring's genome for this fragment to be
	// inserted:
	location.begin = ctx.GetRandom().GetUInt(offspring.GetSize());
	location.end = location.begin + ctx.GetRandom().GetUInt(selected->GetSize()*2);
	location.size = offspring.GetSize();
	location.resize(offspring.GetSize());
}

/*! Called when this organism is the receiver of an HGT donation.
 */
void cPopulationInterface::ReceiveHGTDonation(const Sequence& fragment) {
	InitHGTSupport();
	m_hgt_support->_pending.push_back(fragment);
}


bool cPopulationInterface::Move(cAvidaContext& ctx, int src_id, int dest_id)
{
  return m_world->GetPopulation().MoveOrganisms(ctx, src_id, dest_id, -1);
}

void cPopulationInterface::AddLiveOrg()  
{
  m_world->GetPopulation().AddLiveOrg(GetOrganism());
}

void cPopulationInterface::RemoveLiveOrg() 
{
  m_world->GetPopulation().RemoveLiveOrg(GetOrganism());
}

bool cPopulationInterface::HasOpinion(cOrganism* in_organism)
{
  return in_organism->HasOpinion();
}

void cPopulationInterface::SetOpinion(int opinion, cOrganism* in_organism)
{
  in_organism->SetOpinion(opinion);
}

void cPopulationInterface::ClearOpinion(cOrganism* in_organism)
{
  in_organism->ClearOpinion();
}

void cPopulationInterface::JoinGroup(int group_id)
{
  m_world->GetPopulation().JoinGroup(GetOrganism(), group_id);
}

void cPopulationInterface::MakeGroup()
{
  m_world->GetPopulation().MakeGroup(GetOrganism());
}

void cPopulationInterface::LeaveGroup(int group_id)
{
  m_world->GetPopulation().LeaveGroup(GetOrganism(), group_id);
}

int cPopulationInterface::NumberOfOrganismsInGroup(int group_id)
{
  return m_world->GetPopulation().NumberOfOrganismsInGroup(group_id);
}

int cPopulationInterface::NumberGroupFemales(int group_id)
{
  return m_world->GetPopulation().NumberGroupFemales(group_id);
}

int cPopulationInterface::NumberGroupMales(int group_id)
{
  return m_world->GetPopulation().NumberGroupMales(group_id);
}

int cPopulationInterface::NumberGroupJuvs(int group_id)
{
  return m_world->GetPopulation().NumberGroupJuvs(group_id);
}

void cPopulationInterface::ChangeGroupMatingTypes(cOrganism* org, int group_id, int old_type, int new_type)  
{
  m_world->GetPopulation().ChangeGroupMatingTypes(org, group_id, old_type, new_type);  
}

/* Increases tolerance towards the addition of members to the group.
 * toleranceType:
 *    0: increases tolerance towards immigrants
 *    1: increases tolerance towards own offspring
 *    2: increases tolerance towards other offspring of the group
 * Removes the most recent record of dec-tolerance
 * Returns the modified tolerance total.
 */
int cPopulationInterface::IncTolerance(const int tolerance_type, cAvidaContext &ctx)
{
  int group_id = GetOrganism()->GetOpinion().first;
  
  if (tolerance_type == 0) {
    // Modify tolerance towards immigrants
    PushToleranceInstExe(0, ctx);
    
    // Update tolerance list by removing the most recent dec_tolerance record
    delete GetOrganism()->GetPhenotype().GetToleranceImmigrants().Pop();
    
    // If not at individual's max tolerance, adjust both caches
    if (GetOrganism()->GetPhenotype().GetIntolerances()[0].second != 0) {
      GetOrganism()->GetPhenotype().GetIntolerances()[0].second--;
      GetGroupIntolerances(group_id, 0, -1)--;
      if (m_world->GetConfig().TOLERANCE_VARIATIONS.Get() == 2) {
        if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_FEMALE) GetGroupIntolerances(group_id, 0, 0)--;
        else if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_MALE) GetGroupIntolerances(group_id, 0, 1)--;
        else if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) GetGroupIntolerances(group_id, 0, 2)--;
      }
    }
    // Retrieve modified tolerance total for immigrants
    return GetOrganism()->GetPhenotype().CalcToleranceImmigrants();
  }
  if (tolerance_type == 1) {
    // Modify tolerance towards own offspring
    PushToleranceInstExe(1, ctx);
    
    // Update tolerance list by removing the most recent dec_tolerance record
    delete  GetOrganism()->GetPhenotype().GetToleranceOffspringOwn().Pop();
    
    // If not at max tolerance, decrease the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[1].second != 0) {
      GetOrganism()->GetPhenotype().GetIntolerances()[1].second--;
    }
    // Retrieve modified tolerance total for own offspring.
    return GetOrganism()->GetPhenotype().CalcToleranceOffspringOwn();
  }
  if (tolerance_type == 2) {
    // Modify tolerance towards other offspring of the group
    PushToleranceInstExe(2, ctx);
    
    // Update tolerance list by removing the most recent dec_tolerance record
    delete GetOrganism()->GetPhenotype().GetToleranceOffspringOthers().Pop();
    
    // If not at max tolerance, decrease the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[2].second != 0) {
      GetOrganism()->GetPhenotype().GetIntolerances()[2].second--;
      GetGroupIntolerances(group_id, 1, -1)--;
    }
    // Retrieve modified tolerance total for other offspring in group.
    return GetOrganism()->GetPhenotype().CalcToleranceOffspringOthers();
  }
  return -1;
}

/* Decreases tolerance towards the addition of members to the group.
 * toleranceType:
 *    0: decreases tolerance towards immigrants
 *    1: decreases tolerance towards own offspring
 *    2: decreases tolerance towards other offspring of the group
 * Records the update during which dec-tolerance was executed
 * Returns the modified tolerance total.
 */
int cPopulationInterface::DecTolerance(const int tolerance_type, cAvidaContext &ctx)
{
  const int cur_update = m_world->GetStats().GetUpdate();
  const int tolerance_max = m_world->GetConfig().MAX_TOLERANCE.Get();
  int group_id = GetOrganism()->GetOpinion().first;
  
  if (tolerance_type == 0) {
    // Modify tolerance towards immigrants
    PushToleranceInstExe(3, ctx);
    
    // Update tolerance list by inserting new record (at the front)
    tList<int>& tolerance_list = GetOrganism()->GetPhenotype().GetToleranceImmigrants();
    tolerance_list.Push(new int(cur_update));
    if (tolerance_list.GetSize() > tolerance_max) delete tolerance_list.PopRear();
    
    // If not at min tolerance, increase the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[0].second != tolerance_max) {
      GetOrganism()->GetPhenotype().GetIntolerances()[0].second++;
      GetGroupIntolerances(group_id, 0, -1)++;
      if (m_world->GetConfig().TOLERANCE_VARIATIONS.Get() == 2) {
        if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_FEMALE) GetGroupIntolerances(group_id, 0, 0)++;
        else if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_MALE) GetGroupIntolerances(group_id, 0, 1)++;
        else if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) GetGroupIntolerances(group_id, 0, 2)++;
      }
    }
    
    // Return modified tolerance total for immigrants.
    return GetOrganism()->GetPhenotype().CalcToleranceImmigrants();
  }
  if (tolerance_type == 1) {
    PushToleranceInstExe(4, ctx);
    
    // Update tolerance list by inserting new record (at the front)
    tList<int>& tolerance_list = GetOrganism()->GetPhenotype().GetToleranceOffspringOwn();
    tolerance_list.Push(new int(cur_update));
    if(tolerance_list.GetSize() > tolerance_max) delete tolerance_list.PopRear();
    
    // If not at min tolerance, increase the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[1].second != tolerance_max) {
      GetOrganism()->GetPhenotype().GetIntolerances()[1].second++;
    }
    // Return modified tolerance total for own offspring.
    return GetOrganism()->GetPhenotype().CalcToleranceOffspringOwn();

  }
  if (tolerance_type == 2) {
    PushToleranceInstExe(5, ctx);
    
    // Update tolerance list by inserting new record (at the front)
    tList<int>& tolerance_list = GetOrganism()->GetPhenotype().GetToleranceOffspringOthers();
    tolerance_list.Push(new int(cur_update));
    if(tolerance_list.GetSize() > tolerance_max) delete tolerance_list.PopRear();
    
    // If not at min tolerance, increase the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[2].second != tolerance_max) {
      GetOrganism()->GetPhenotype().GetIntolerances()[2].second++;
      GetOrganism()->GetOrgInterface().GetGroupIntolerances(group_id, 1, -1)++;
    }
    // Retrieve modified tolerance total for other offspring in the group.
    return GetOrganism()->GetPhenotype().CalcToleranceOffspringOthers();
  }
  return -1;
}

int cPopulationInterface::CalcGroupToleranceImmigrants(int prop_group_id, int mating_type)
{
  return m_world->GetPopulation().CalcGroupToleranceImmigrants(prop_group_id, mating_type);
}

int cPopulationInterface::CalcGroupToleranceOffspring(cOrganism* parent_organism)
{
  return m_world->GetPopulation().CalcGroupToleranceOffspring(parent_organism);
}

double cPopulationInterface::CalcGroupOddsImmigrants(int group_id, int mating_type)
{
  return m_world->GetPopulation().CalcGroupOddsImmigrants(group_id, mating_type);
}

double cPopulationInterface::CalcGroupOddsOffspring(cOrganism* parent)
{
  return m_world->GetPopulation().CalcGroupOddsOffspring(parent);
}

double cPopulationInterface::CalcGroupOddsOffspring(int group_id)
{
  return m_world->GetPopulation().CalcGroupOddsOffspring(group_id);
}

bool cPopulationInterface::AttemptImmigrateGroup(int group_id, cOrganism* org)
{
  return m_world->GetPopulation().AttemptImmigrateGroup(group_id, org);
}

void cPopulationInterface::PushToleranceInstExe(int tol_inst, cAvidaContext& ctx)
{
  if(!m_world->GetConfig().TRACK_TOLERANCE.Get()) {
    m_world->GetStats().PushToleranceInstExe(tol_inst);
    return;
  }
  
  const tArray<double> res_count = GetResources(ctx);
  
  int group_id = GetOrganism()->GetOpinion().first;
  int group_size = NumberOfOrganismsInGroup(group_id);
  double resource_level = res_count[group_id];
  int tol_max = m_world->GetConfig().MAX_TOLERANCE.Get();
  
  double immigrant_odds = CalcGroupOddsImmigrants(group_id, -1);
  double offspring_own_odds;
  double offspring_others_odds;
  int tol_immi = GetOrganism()->GetPhenotype().CalcToleranceImmigrants();
  int tol_own;
  int tol_others;
  
  if(m_world->GetConfig().TOLERANCE_VARIATIONS.Get() > 0) {
    offspring_own_odds = 1.0;
    offspring_others_odds = 1.0;
    tol_own = tol_max;
    tol_others = tol_max;
  } else {
    offspring_own_odds = CalcGroupOddsOffspring(GetOrganism());
    offspring_others_odds = CalcGroupOddsOffspring(group_id);
    tol_own = GetOrganism()->GetPhenotype().CalcToleranceOffspringOwn();
    tol_others = GetOrganism()->GetPhenotype().CalcToleranceOffspringOthers();
  }
  
  double odds_immi = immigrant_odds * 100;
  double odds_own = offspring_own_odds * 100;
  double odds_others = offspring_others_odds * 100;
  
  m_world->GetStats().PushToleranceInstExe(tol_inst, group_id, group_size, resource_level, odds_immi, odds_own, odds_others, tol_immi, tol_own, tol_others, tol_max);
  return;
}

int& cPopulationInterface::GetGroupIntolerances(int group_id, int tol_num, int mating_type)
{
  return m_world->GetPopulation().GetGroupIntolerances(group_id, tol_num, mating_type);
}

void cPopulationInterface::DecNumPreyOrganisms()
{
  m_world->GetPopulation().DecNumPreyOrganisms();
}

void cPopulationInterface::DecNumPredOrganisms()
{
  m_world->GetPopulation().DecNumPredOrganisms();
}

void cPopulationInterface::IncNumPreyOrganisms()
{
  m_world->GetPopulation().IncNumPreyOrganisms();
}

void cPopulationInterface::IncNumPredOrganisms()
{
  m_world->GetPopulation().IncNumPredOrganisms();
}

void cPopulationInterface::AttackFacedOrg(cAvidaContext& ctx, int loser)
{
  m_world->GetPopulation().AttackFacedOrg(ctx, loser);
}


//**
bool cPopulationInterface::SendNeuralMessage(cAvidaContext& ctx, cOrgMessage& msg)
{
  bool message_sent = false;
  for (int avatar_id = 0; avatar_id < GetNumAV(); avatar_id++) {
    if (m_avatars[avatar_id].av_output) {
      message_sent = (message_sent || SendNeuralMessage(ctx, msg, m_avatars[avatar_id].av_cell_id));
    }
  }
  return message_sent;
}

//**
bool cPopulationInterface::SendNeuralMessage(cAvidaContext& ctx, cOrgMessage& msg, int cell_id)
{
  msg.SetTransCellID(cell_id);
  cPopulationCell& cell = m_world->GetPopulation().GetCell(cell_id);

  bool dropped = false;
  bool lost = false;

  static const double drop_prob = m_world->GetConfig().NET_DROP_PROB.Get();
  if ((drop_prob > 0.0) && m_world->GetRandom().P(drop_prob)) {
    // message dropped
    GetDeme()->messageDropped();
    GetDeme()->messageSendFailed();
    dropped = true;
  }

  if (!dropped) {
    if (m_world->GetConfig().DEMES_IO_HANDLING.Get() == 2 || m_world->GetConfig().DEMES_IO_HANDLING.Get() == 4) {
      double cell_bonus = m_world->GetPopulation().GetCell(cell_id).GetOutputBonus();
      if (cell_bonus > 0.0) {
        if (m_world->GetConfig().ACTIVE_MESSAGES_ENABLED.Get() == 3) {
          int task = GetDeme()->DoDemeOutput(ctx, msg.GetLabel(), cell_bonus);
          if (m_world->GetConfig().DEMES_IO_FEEDBACK.Get()) {
            cDeme* deme = GetDeme();
            for (int i = 0; i < deme->GetSize(); i++) {
              m_world->GetPopulation().GetCell(deme->GetAbsoluteCellID(i)).SetCellData(task);
            }
          }
        } else {
          int task1 = GetDeme()->DoDemeOutput(ctx, msg.GetLabel(), cell_bonus);
          int task2 = GetDeme()->DoDemeOutput(ctx, msg.GetData(), cell_bonus);
          if (m_world->GetConfig().DEMES_IO_FEEDBACK.Get()) {
            if (task2 > task1) task1 = task2;
            cDeme* deme = GetDeme();
            for (int i = 0; i < deme->GetSize(); i++) {
              m_world->GetPopulation().GetCell(deme->GetAbsoluteCellID(i)).SetCellData(task1);
            }
          }
        }
      }
    }
  }

  // Check for inputs in this cell
  if (!cell.GetNumAVInputs()) lost = true;
  // If self communication is not allowed, must check for an input avatar for another organism
  else if (!m_world->GetConfig().SELF_COMMUNICATION.Get()) {
    lost = true;
    cOrganism* sender = GetOrganism();
    for (int i = 0; i < cell.GetNumAVInputs(); i++) {
      if (sender != cell.GetCellInputAVs()[i]) lost = false;
    }
  }

  if (lost) {
    GetDeme()->messageSendFailed();
    cell.IncLostMessages();
    cell.IncSentMessages();
  }

  if (dropped || lost) {
    if (m_world->GetConfig().NET_LOG_MESSAGES.Get()) {
      m_world->GetStats().LogMessage(msg, dropped, lost);
    }
    return false;
  }

  cOrganism* sender = GetOrganism();
  for (int i = 0; i < cell.GetNumAVInputs(); i++) {
    cOrganism* recvr = cell.GetCellInputAVs()[i];
    assert(recvr != 0);
    if ((sender != recvr) || m_world->GetConfig().SELF_COMMUNICATION.Get()) {
      recvr->ReceiveMessage(msg);
      m_world->GetStats().SentMessage(msg);
      GetDeme()->MessageSuccessfullySent();
    }
  }
  if (cell.GetLostMessages()) cell.ResetMessageCounts();
  cell.IncSentMessages();
  cell.IncSuccessfulMessages();
  return true;
}

// -------- Avatar support --------
/* Each organism carries an array of avatars linking the organism to any cells it is occupying.
 * Each cell contains an array of the organisms with avatars in that cell, linking the cells back to
 * the organisms (in cPopulationCell). This allows both multiple organisms to occupy the same cell
 * and organisms to occupy/interact with multiple cells. Currently only two types of avatars are
 * supported: input and output, also used as predators and prey.
 */

// Check if the avatar has any output avatars sharing the same cell
bool cPopulationInterface::HasOutputAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the avatar's cell for an output avatar
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).HasOutputAV(GetOrganism());
  }
  return false;
}

// Check if the avatar's faced cell has any output avatars
bool cPopulationInterface::FacedHasOutputAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the avatar's faced cell for an output avatar
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).HasOutputAV(GetOrganism());
  }
  return false;
}

// Check if the avatar's faced cell has any avatars
bool cPopulationInterface::FacedHasAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the avatar's faced cell for other avatars
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).HasAV();
  }
  return false;
}

// Check if the avatar's faced cell has any predator (input) avatars
bool cPopulationInterface::FacedHasPredAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the faced cell for predators (inputs)
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).HasInputAV();
  }
  return false;
}

// Check it the avatar's faced cell has any prey (output) avatars
bool cPopulationInterface::FacedHasPreyAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the faced cell for prey (outputs)
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).HasOutputAV();
  }
  return false;
}

// Checks if the cell is within the movement range allowed to this organism's avatars
bool cPopulationInterface::CellInAvatarRange(int cell_id)
{
  if (!m_world->GetConfig().NEURAL_NETWORKING.Get()) return true;
  if (m_world->GetConfig().NEURON_MAX_DIST.Get() == 0) return true;

  std::pair<int, int> org_pos = m_world->GetPopulation().GetDeme(m_deme_id).GetCellPosition(m_cell_id);
  std::pair<int, int> cell_pos = m_world->GetPopulation().GetDeme(m_deme_id).GetCellPosition(cell_id);
  int x_offset = abs(org_pos.first - cell_pos.first);
  int y_offset = abs(org_pos.second - cell_pos.second);
  int total_offset = max(x_offset, y_offset);
  if (total_offset <= m_world->GetConfig().NEURON_MAX_DIST.Get()) return true;
  else return false;
}

// Creates a new avatar and adds it to the cell avatar lists
void cPopulationInterface::AddAV(int av_cell_id, int av_facing, bool input, bool output)
{
  // Add new avatar to m_avatars
  sIO_avatar tmpAV(av_cell_id, av_facing, input, output);
  m_avatars.Push(tmpAV);

  int last = m_avatars.GetSize() - 1;

  // If this is an input avatar add to the target cell
  if (input) {
    m_avatars[last].av_cell_index = m_world->GetPopulation().GetCell(av_cell_id).AddInputAV(GetOrganism(), last);
  }

  // If this is an output avatar add to the target cell
  if (output) {
    m_avatars[last].av_cell_index = m_world->GetPopulation().GetCell(av_cell_id).AddOutputAV(GetOrganism(), last);
  }

  // Find the created avatar's faced cell
  SetAVFacedCellID(GetNumAV() - 1);
}

// Creates a new avatar based on the organism's forage target as a predator or prey, and adds it to the cell
void cPopulationInterface::AddPredPreyAV(int av_cell_id)
{
  // Add predator (saved as input avatar)
  if (GetOrganism()->GetForageTarget() == -2) {
    sIO_avatar predAV(av_cell_id, 0, true, false);
    m_avatars.Push(predAV);
    m_avatars[0].av_cell_index = m_world->GetPopulation().GetCell(av_cell_id).AddInputAV(GetOrganism());
  // Add prey (saved as output avatar)
  } else {
    sIO_avatar preyAV(av_cell_id, 0, false, true);
    m_avatars.Push(preyAV);
    m_avatars[0].av_cell_index = m_world->GetPopulation().GetCell(av_cell_id).AddOutputAV(GetOrganism());
  }
  // Find the created avatar's faced cell
  SetAVFacedCellID(GetNumAV() - 1);
}

// Switches the avatar from being a predator to a prey avatar or vice-versa
void cPopulationInterface::SwitchPredPrey(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Is a predator, switching to a prey (input to output)
    if (m_avatars[av_num].av_input) {
      m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).RemoveInputAV(GetOrganism(), m_avatars[av_num].av_cell_index);
      m_avatars[av_num].av_cell_index = m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).AddOutputAV(GetOrganism(), av_num);
      m_avatars[av_num].av_input = false;
      m_avatars[av_num].av_output = true;
    // Is prey, switching to a predator (output to intput)
    } else if (m_avatars[av_num].av_output) {
      m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).RemoveOutputAV(GetOrganism(), m_avatars[av_num].av_cell_index);
      m_avatars[av_num].av_cell_index = m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).AddInputAV(GetOrganism(), av_num);
      m_avatars[av_num].av_input = true;
      m_avatars[av_num].av_output = false;
    }
  }
}

// Removes all the organism's avatars and removes them from cell lists
void cPopulationInterface::RemoveAllAV()
{
  int num_av = GetNumAV();
  // Cycle through removing all avatars
  for (int i = 0; i < num_av; i++) {
    sIO_avatar tmpAV = m_avatars.Pop();
    // Check that avatar is actually in a cell
    if (tmpAV.av_cell_id >= 0) {
      // If input avatar remove from the cell
      if (tmpAV.av_input) {
        m_world->GetPopulation().GetCell(tmpAV.av_cell_id).RemoveInputAV(GetOrganism(), tmpAV.av_cell_index);
      }
      // If output avatar remove from the cell
      if (tmpAV.av_output) {
        m_world->GetPopulation().GetCell(tmpAV.av_cell_id).RemoveOutputAV(GetOrganism(), tmpAV.av_cell_index);
      }
    }
  }
}

// Returns the avatar's faced direction
int cPopulationInterface::GetAVFacing(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Return avatar's facing
    return m_avatars[av_num].av_facing;
  }
  return 0;
}

// Returns the avatar's cell id
int cPopulationInterface::GetAVCellID(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Return the avatar's cell id
    return m_avatars[av_num].av_cell_id;
  }
  return -1;
}

// Returns cell id faced by avatar, only for torus and bounded worlds
int cPopulationInterface::GetAVFacedCellID(int av_num)
{
  if (av_num < GetNumAV()) {
    return m_avatars[av_num].av_faced_cell;
  }
  return -1;
}

// Returns the number of cells neighboring the avatar's
// Avatar facing and movement only works in bounded or toroidial geometries
int cPopulationInterface::GetAVNumNeighbors(int av_num)
{
  if ((m_world->GetConfig().WORLD_GEOMETRY.Get() != 1) && (m_world->GetConfig().WORLD_GEOMETRY.Get() != 2)) m_world->GetDriver().RaiseFatalException(-1, "Not valid WORLD_GEOMETRY for USE_AVATAR, must be torus or bounded.");

  // If the avatar exists..
  if (av_num < GetNumAV()) {
    if (m_world->GetConfig().WORLD_GEOMETRY.Get() == 2) return 8;

    const int cell_id = m_avatars[av_num].av_cell_id;
    const int x_size = m_world->GetConfig().WORLD_X.Get();
    const int y_size = m_world->GetConfig().WORLD_Y.Get() / m_world->GetConfig().NUM_DEMES.Get();
    const int deme_size = x_size * y_size;
    int deme_cell = cell_id % deme_size;
    int x = deme_cell % x_size;
    int y = deme_cell / x_size;

    // Is the cell on a corner..
    if (x == 0 || x == (x_size - 1)) {
      if (y == 0 || y == (y_size - 1)) return 3;
      // Is the cell on a side-edge..
      else return 5;
    }
    // Is the cell on a top or bottom edge..
    if (y == 0 || y == (y_size - 1)) return 5;
    // The cell must be on the interior..
    return 8;
  }
  return 0;
}

// Returns the avatar's faced cell data
int cPopulationInterface::GetAVFacedData(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Return the avatar's faced cell data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellData();
  }
  return -1;
}

// Returns the avatar's faced cell org id data
int cPopulationInterface::GetAVFacedDataOrgID(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell org id data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellDataOrgID();
  }
  return -1;
}

// Returns the avatar's faced cell update data
int cPopulationInterface::GetAVFacedDataUpdate(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell update data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellDataUpdate();
  }
  return -1;
}

// Returns the avatar's faced cell territory data
int cPopulationInterface::GetAVFacedDataTerritory(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell territory data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellDataTerritory();
  }
  return -1;
}

// Returns the avatar's deme x-coordinate position
int cPopulationInterface::GetAVCellXPosition(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    std::pair<int, int> position = m_world->GetPopulation().GetDeme(m_deme_id).GetCellPosition(m_avatars[av_num].av_cell_id);
    return position.first;
  }
  return -1;
}

// Returns the avatar's deme y-coordinate position
int cPopulationInterface::GetAVCellYPosition(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    std::pair<int, int> position = m_world->GetPopulation().GetDeme(m_deme_id).GetCellPosition(m_avatars[av_num].av_cell_id);
    return position.second;
  }
  return -1;
}

// Finds the index of the next avatar which matches input/output specifications
int cPopulationInterface::FindAV(bool input, bool output, int av_num)
{
  assert(GetNumAV() > 0);
  const int num_AV = GetNumAV();
  av_num = abs(av_num % num_AV);
  for (int i = 0; i < num_AV; i++) {
    int index = (i + av_num) % num_AV;
    if (m_avatars[index].av_input == input && m_avatars[index].av_output == output) {
      return index;
    }
  }
}

void cPopulationInterface::SetAVCellIndex(int av_index, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    m_avatars[av_num].av_cell_index = av_index;
  }
}

// Sets the avatar's facing, then sets the faced cell
void cPopulationInterface::SetAVFacing(int av_facing, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    m_avatars[av_num].av_facing = av_facing;
    // Set the new avatar faced cell id
    SetAVFacedCellID(av_num);
  }
}

// Changes the avatar's cell id, moving it to the correct cell lists, and find the new faced cell id
bool cPopulationInterface::SetAVCellID(int av_cell_id, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Not necessary to move the avatar
    if (m_avatars[av_num].av_cell_id == av_cell_id) return false;

    // Avatar can't be moved into a cell outside of its maximum range
    if (!CellInAvatarRange(av_cell_id)) return false;

    // If the avatar was previously in another cell remove it
    if (m_avatars[av_num].av_cell_id > -1) {
      if (m_avatars[av_num].av_input) {
        m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).RemoveInputAV(GetOrganism(), m_avatars[av_num].av_cell_index);
      }
      if (m_avatars[av_num].av_output) {
        m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).RemoveOutputAV(GetOrganism(), m_avatars[av_num].av_cell_index);
      }
    }

    // If it is an input avatar, add to the new cell
    if (m_avatars[av_num].av_input) {
      m_avatars[av_num].av_cell_index = m_world->GetPopulation().GetCell(av_cell_id).AddInputAV(GetOrganism(), av_num);
    }
    // If it is an output avatar, add to the new cell
    if (m_avatars[av_num].av_output) {
      m_avatars[av_num].av_cell_index = m_world->GetPopulation().GetCell(av_cell_id).AddOutputAV(GetOrganism(), av_num);
    }

    // Set the avatar's cell
    m_avatars[av_num].av_cell_id = av_cell_id;

    // Set the avatar's new faced cell
    SetAVFacedCellID(av_num);
    return true;
  }
  return false;
}

// Determine and store the cell id faced by the avatar
// Note:
void cPopulationInterface::SetAVFacedCellID(int av_num)
{
  const int world_geometry = m_world->GetConfig().WORLD_GEOMETRY.Get();
  // Avatars only supported in bounded and toroidal world geometries
  if ((world_geometry != 1) && (world_geometry != 2)) m_world->GetDriver().RaiseFatalException(-1, "Not valid WORLD_GEOMETRY for USE_AVATAR, must be torus or bounded.");

  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Convert the cell id into a deme x,y position
    const int x_size = m_world->GetConfig().WORLD_X.Get();
    const int y_size = m_world->GetConfig().WORLD_Y.Get() / m_world->GetConfig().NUM_DEMES.Get();
    const int deme_size = x_size * y_size;

    const int old_cell_id = m_avatars[av_num].av_cell_id;
    const int facing = m_avatars[av_num].av_facing;

    const int deme_id = old_cell_id / deme_size;
    const int old_deme_cell = old_cell_id % deme_size;

    int x = old_deme_cell % x_size;
    int y = old_deme_cell / x_size;

    // If this happens to be an avatar in a single cell world, it can't face any cell beyond its own
    if (deme_size == 1) {
      m_avatars[av_num].av_faced_cell = m_avatars[av_num].av_cell_id;
      return;
    }

    bool off_the_edge_facing = false;
    // If a bounded grid, do checks for facing off the edge of a bounded world grid..
    if (world_geometry == 1) {
      // Check if the avatar is at the end of a single column world
      if (x_size == 1) {
        if (y == 0) {
          y += 1;
          off_the_edge_facing = true;
        } else if (y == (y_size - 1)) {
          y -= 1;
          off_the_edge_facing = true;
        }
      // Check if the avatar is at the end of a single row world
      } else if (y_size == 1) {
        if (x == 0) {
          x += 1;
          off_the_edge_facing = true;
        } else if (y == (y_size - 1)) {
          x -= 1;
          off_the_edge_facing = true;
        }
      }

      // The world is neither a single row or column, continuing border facing checks
      if (!off_the_edge_facing) {
        // West edge..
        if (x == 0) {
          // Northwest corner
          if (y == 0 && (facing == 0 || facing == 7 || facing == 6)) {
            if (m_world->GetRandom().GetInt(0, 2)) {
              x += 1;
              off_the_edge_facing = true;
            } else {
              y += 1;
              off_the_edge_facing = true;
            }
          // Southwest corner
          } else if (y == (y_size - 1) && (facing == 4 || facing == 5 || facing == 6)) {
            if (m_world->GetRandom().GetInt(0, 2)) {
              x += 1;
              off_the_edge_facing = true;
            } else {
              y -= 1;
              off_the_edge_facing = true;
            }
          }
          // West edge facings not checked yet
          if (!off_the_edge_facing) {
            // West edge facing southwest
            if (facing == 5) {
              y += 1;
              off_the_edge_facing = true;
            // West edge facing west
            } else if (facing == 6) {
              if (m_world->GetRandom().GetInt(0, 2)) {
                y += 1;
                off_the_edge_facing = true;
              } else {
                y -= 1;
                off_the_edge_facing = true;
              }
            // West edge facing northwest
            } else if (facing == 7) {
              y -= 1;
              off_the_edge_facing = true;
            }
          }

        // East edge..
        } else if (x == (x_size - 1)) {
          // Northeast corner
          if (y == 0 && (facing == 0 || facing == 1 || facing == 2)) {
            if (m_world->GetRandom().GetInt(0, 2)) {
              x -= 1;
              off_the_edge_facing = true;
            } else {
              y += 1;
              off_the_edge_facing = true;
            }
          // Southeast corner
          } else if (y == (y_size - 1) && (facing == 2 || facing == 3 || facing == 4)) {
            if (m_world->GetRandom().GetInt(0, 2)) {
              x -= 1;
              off_the_edge_facing = true;
            } else {
              y -= 1;
              off_the_edge_facing = true;
            }
          }
          // East edge facings not checked yet
          if (!off_the_edge_facing) {
            // East edge facing northeast
            if (facing == 1) {
              y -= 1;
              off_the_edge_facing = true;
            // East edge facing east
            } else if (facing == 2) {
              if (m_world->GetRandom().GetInt(0, 2)) {
                y += 1;
                off_the_edge_facing = true;
              } else {
                y -= 1;
                off_the_edge_facing = true;
              }
            // East edge facing southeast
            } else if (facing == 3) {
              y += 1;
              off_the_edge_facing = true;
            }
          }

        // North edge..
        } else if (y == 0) {
          // North edge facing northwest
          if (facing == 7) {
            x -= 1;
            off_the_edge_facing = true;
          // North edge facing north
          } else if (facing == 0) {
            if (m_world->GetRandom().GetInt(0, 2)) {
              x += 1;
              off_the_edge_facing = true;
            } else {
              x -= 1;
              off_the_edge_facing = true;
            }
          // North edge facing northeast
          } else if (facing == 1) {
            x += 1;
            off_the_edge_facing = true;
          }

        // South edge..
        } else if (y == (y_size - 1)) {
          // South edge facing southeast
          if (facing == 3) {
            x += 1;
            off_the_edge_facing = true;
          // South edge facing south
          } else if (facing == 4) {
            if (m_world->GetRandom().GetInt(0, 2)) {
              x += 1;
              off_the_edge_facing = true;
            } else {
              x -= 1;
              off_the_edge_facing = true;
            }
          // South edge facing southwest
          } else if (facing == 5) {
            x -= 1;
            off_the_edge_facing = true;
          }
        }
      }
    }

    // Torus world geometry or not a bounded outward facing edge..
    if (!off_the_edge_facing || world_geometry == 2) {
      // North facing
      if ((facing == 0) || (facing == 1) || (facing == 7)) {
        y = (y - 1 + y_size) % y_size;
      }

      // South facing
      if ((facing == 3) || (facing == 4) || (facing == 5)) {
        y = (y + 1) % y_size;
      }

      // East facing
      if ((facing == 1) || (facing == 2) || (facing == 3)) {
        x = (x + 1) % x_size;
      }

      // West facing
      if ((facing == 5) || (facing == 6) || (facing == 7)) {
        x = (x - 1 + x_size) % x_size;
      }
    }

    // Convert the x,y deme coordinates back into a cell id
    const int new_deme_cell = y * x_size + x;
    const int new_cell_id = deme_id * deme_size + new_deme_cell;

    // Store the faced cell id
    m_avatars[av_num].av_faced_cell = new_cell_id;
  }
}

// Sets the avatar's cell's data
void cPopulationInterface::SetAVCellData(const int newData, const int org_id, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).SetCellData(newData, org_id);
  }
}

// Move input avatar into faced cell
bool cPopulationInterface::MoveAV(cAvidaContext& ctx, int av_num)
{
  // If the avatar exists..
  bool success = false;
  if (av_num < GetNumAV()) {
    // Move the avatar into the faced cell
    int src_id = m_avatars[av_num].av_cell_id;
    int dest_id = m_avatars[av_num].av_faced_cell;
    int true_cell = m_cell_id;
    if (m_world->GetConfig().NEURAL_NETWORKING.Get() || m_world->GetPopulation().MoveOrganisms(ctx, src_id, dest_id, true_cell)) {
      return SetAVCellID(m_avatars[av_num].av_faced_cell, av_num);
    }
  }
  return success;
}

// Rotate the avatar by input increment, then set the new faced cell
bool cPopulationInterface::RotateAV(int increment, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    if (increment >= 0) {
      increment %= 8;
    } else {
      increment = -increment;
      increment %= 8;
      increment = -increment;
    }
    // Adjust facing by increment
    SetAVFacing((m_avatars[av_num].av_facing + increment + 8) % 8);
    return true;
  }
  return false;
}

// Returns a random input or output avatar from the faced cell
cOrganism* cPopulationInterface::GetRandFacedAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetRandAV();
  }
  return NULL;
}

// Returns a random predator (input) avatar from the faced cell
cOrganism* cPopulationInterface::GetRandFacedPredAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetRandPredAV();
  }
  return NULL;
}

// Returns a random prey (output) avatar from the faced cell
cOrganism* cPopulationInterface::GetRandFacedPreyAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetRandPreyAV();
  }
  return NULL;
}

// Returns an array of all avatars in the organism's avatar's faced cell
tArray<cOrganism*> cPopulationInterface::GetFacedAVs(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellAVs();
  }
  tArray<cOrganism*> null_array(0, NULL);
  return null_array;
}

// Returns an array of all prey avatars in the organism's avatar's faced cell
tArray<cOrganism*> cPopulationInterface::GetFacedPreyAVs(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellOutputAVs();
  }
  tArray<cOrganism*> null_array(0, NULL);
  return null_array;
}

// Returns the avatar's cell resources
const tArray<double>& cPopulationInterface::GetAVResources(cAvidaContext& ctx, int av_num)
{
  assert(av_num < GetNumAV());
  return m_world->GetPopulation().GetCellResources(m_avatars[av_num].av_cell_id, ctx);
}

// Returns the avatar's faced cell's resources
const tArray<double>& cPopulationInterface::GetAVFacedResources(cAvidaContext& ctx, int av_num)
{
  assert(av_num < GetNumAV());
  return m_world->GetPopulation().GetCellResources(m_avatars[av_num].av_faced_cell, ctx);
}

// Updates the avatar's cell resources
void cPopulationInterface::UpdateAVResources(cAvidaContext& ctx, const tArray<double>& res_change, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    m_world->GetPopulation().UpdateCellResources(ctx, res_change, m_avatars[av_num].av_cell_id);
  }
}

//**
tArray<int> cPopulationInterface::NeuronLookAhead(cAvidaContext& ctx, int av_num, int distance_sought)
{
  const int av_cell_x = GetAVCellXPosition(av_num);
  const int av_cell_y = GetAVCellYPosition(av_num);
  const int facing = m_avatars[av_num].av_facing;

  cDeme* deme = GetDeme();
  const int deme_x_size = m_world->GetConfig().WORLD_X.Get();
  const int deme_y_size = deme->GetSize() / deme_x_size;

  int facing_offset_x = 0;
  int facing_offset_y = 0;
  int left_offset_x = 0;
  int left_offset_y = 0;
  int right_offset_x = 0;
  int right_offset_y = 0;
  switch (facing) {
  case 0:
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_x = 1;
    break;
  case 1:
    facing_offset_x = 1;
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_y = 1;
    break;
  case 2:
    facing_offset_x = 1;
    left_offset_y = -1;
    right_offset_y = 1;
    break;
  case 3:
    facing_offset_x = 1;
    facing_offset_y = 1;
    left_offset_y = -1;
    right_offset_x = -1;
    break;
  case 4:
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_x = -1;
    break;
  case 5:
    facing_offset_x = -1;
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_y = -1;
    break;
  case 6:
    facing_offset_x = -1;
    left_offset_y = 1;
    right_offset_y = -1;
    break;
  case 7:
    facing_offset_x = -1;
    facing_offset_y = -1;
    left_offset_y = 1;
    right_offset_x = 1;
    break;
  }

  int sent_msg_count = 0;
  int lost_msg_count = 0;
  int successful_msg_count = 0;
  int num_input_AV = 0;
  int num_output_AV = 0;
  int num_empty_output_AV = 0;
  int num_m_AV = 0;

  if (distance_sought <= 0) distance_sought = m_world->GetConfig().LOOK_DIST.Get();
  int max_dist = min(distance_sought, m_world->GetConfig().LOOK_DIST.Get());
  int dist_used = max_dist;
  int cur_x;
  int cur_y;
  int cell_id;
  for (int cur_dist = 1; cur_dist <= max_dist; cur_dist++) {
    // Center faced cells
    cur_x = cur_dist * facing_offset_x;
    cur_y = cur_dist * facing_offset_y;
    if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) {
      dist_used = cur_dist - 1;
      break;
    }
    cell_id = deme->GetCellID(cur_x, cur_y);
    bool has_m_AV = false;
    for (int i = 0; i < m_avatars.GetSize(); i++) {
      if (m_avatars[i].av_cell_id == cell_id) {
        has_m_AV = true;
        break;
      }
    }
    if (!has_m_AV) {
      cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
      sent_msg_count += cell.GetSentMessages();
      lost_msg_count += cell.GetLostMessages();
      successful_msg_count += cell.GetSuccessfulMessages();
      num_input_AV += cell.GetNumAVInputs();
      num_output_AV += cell.GetNumAVOutputs();
      if (cell.GetNumAVOutputs() > 0 && cell.GetNumAVInputs() == 0) num_empty_output_AV += cell.GetNumAVOutputs();
    } else num_m_AV++;

    // Left faced cells
    int look_width = cur_dist / 2;
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * left_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * left_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;

      cell_id = deme->GetCellID(cur_x, cur_y);
      has_m_AV = false;
      for (int i = 0; i < m_avatars.GetSize(); i++) {
        if (m_avatars[i].av_cell_id == cell_id) {
          has_m_AV = true;
          break;
        }
      }
      if (!has_m_AV) {
        cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
        sent_msg_count += cell.GetSentMessages();
        lost_msg_count += cell.GetLostMessages();
        successful_msg_count += cell.GetSuccessfulMessages();
        num_input_AV += cell.GetNumAVInputs();
        num_output_AV += cell.GetNumAVOutputs();
        if (cell.GetNumAVOutputs() > 0 && cell.GetNumAVInputs() == 0) num_empty_output_AV += cell.GetNumAVOutputs();
      } else num_m_AV++;
    }

    // Right faced cells
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * right_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * right_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;

      cell_id = deme->GetCellID(cur_x, cur_y);
      has_m_AV = false;
      for (int i = 0; i < m_avatars.GetSize(); i++) {
        if (m_avatars[i].av_cell_id == cell_id) {
          has_m_AV = true;
          break;
        }
      }
      if (!has_m_AV) {
        cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
        sent_msg_count += cell.GetSentMessages();
        lost_msg_count += cell.GetLostMessages();
        successful_msg_count += cell.GetSuccessfulMessages();
        num_input_AV += cell.GetNumAVInputs();
        num_output_AV += cell.GetNumAVOutputs();
        if (cell.GetNumAVOutputs() > 0 && cell.GetNumAVInputs() == 0) num_empty_output_AV ++;
      } else num_m_AV++;
    }
  }

  tArray<int> return_values(8);
  return_values[0] = dist_used;
  return_values[1] = sent_msg_count;
  return_values[2] = lost_msg_count;
  return_values[3] = successful_msg_count;
  return_values[4] = num_input_AV;
  return_values[5] = num_output_AV;
  return_values[6] = num_empty_output_AV;
  return_values[7] = num_m_AV;
  return return_values;
}

tArray<int> cPopulationInterface::NeuronLookLostMessages(cAvidaContext& ctx, int av_num)
{
  const int av_cell_x = GetAVCellXPosition(av_num);
  const int av_cell_y = GetAVCellYPosition(av_num);
  const int facing = m_avatars[av_num].av_facing;

  cDeme* deme = GetDeme();
  const int deme_x_size = m_world->GetConfig().WORLD_X.Get();
  const int deme_y_size = deme->GetSize() / deme_x_size;

  int facing_offset_x = 0;
  int facing_offset_y = 0;
  int left_offset_x = 0;
  int left_offset_y = 0;
  int right_offset_x = 0;
  int right_offset_y = 0;
  switch (facing) {
  case 0:
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_x = 1;
    break;
  case 1:
    facing_offset_x = 1;
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_y = 1;
    break;
  case 2:
    facing_offset_x = 1;
    left_offset_y = -1;
    right_offset_y = 1;
    break;
  case 3:
    facing_offset_x = 1;
    facing_offset_y = 1;
    left_offset_y = -1;
    right_offset_x = -1;
    break;
  case 4:
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_x = -1;
    break;
  case 5:
    facing_offset_x = -1;
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_y = -1;
    break;
  case 6:
    facing_offset_x = -1;
    left_offset_y = 1;
    right_offset_y = -1;
    break;
  case 7:
    facing_offset_x = -1;
    facing_offset_y = -1;
    left_offset_y = 1;
    right_offset_x = 1;
    break;
  }

  int lost_msg_count = 0;
  int distance = 0;

  int look_dist = m_world->GetConfig().LOOK_DIST.Get();
  int cur_x;
  int cur_y;
  bool found_closest = false;
  for (int cur_dist = 1; cur_dist <= look_dist; cur_dist++) {
    // Center faced cells
    cur_x = cur_dist * facing_offset_x;
    cur_y = cur_dist * facing_offset_y;
    if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
    cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
    if (cell.GetLostMessages()) {
      if (!found_closest) {
        distance = cur_dist;
        found_closest = true;
      }
      lost_msg_count += cell.GetLostMessages();
    }

    // Left faced cells
    int look_width = cur_dist / 2;
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * left_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * left_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
      cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
      if (cell.GetLostMessages()) {
        if (!found_closest) {
          distance = cur_dist;
          found_closest = true;
        }
        lost_msg_count += cell.GetLostMessages();
      }
    }

    // Right faced cells
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * right_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * right_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
      cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
      if (cell.GetLostMessages()) {
        if (!found_closest) {
          distance = cur_dist;
          found_closest = true;
        }
        lost_msg_count += cell.GetLostMessages();
      }
    }
  }

  tArray<int> return_values(2);
  return_values[0] = lost_msg_count;
  return_values[1] = distance;
  return return_values;
}

tArray<int> cPopulationInterface::NeuronLookEmptyOutputs(cAvidaContext& ctx, int av_num)
{
  const int av_cell_x = GetAVCellXPosition(av_num);
  const int av_cell_y = GetAVCellYPosition(av_num);
  const int facing = m_avatars[av_num].av_facing;

  cDeme* deme = GetDeme();
  const int deme_x_size = m_world->GetConfig().WORLD_X.Get();
  const int deme_y_size = deme->GetSize() / deme_x_size;

  int facing_offset_x = 0;
  int facing_offset_y = 0;
  int left_offset_x = 0;
  int left_offset_y = 0;
  int right_offset_x = 0;
  int right_offset_y = 0;
  switch (facing) {
  case 0:
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_x = 1;
    break;
  case 1:
    facing_offset_x = 1;
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_y = 1;
    break;
  case 2:
    facing_offset_x = 1;
    left_offset_y = -1;
    right_offset_y = 1;
    break;
  case 3:
    facing_offset_x = 1;
    facing_offset_y = 1;
    left_offset_y = -1;
    right_offset_x = -1;
    break;
  case 4:
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_x = -1;
    break;
  case 5:
    facing_offset_x = -1;
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_y = -1;
    break;
  case 6:
    facing_offset_x = -1;
    left_offset_y = 1;
    right_offset_y = -1;
    break;
  case 7:
    facing_offset_x = -1;
    facing_offset_y = -1;
    left_offset_y = 1;
    right_offset_x = 1;
    break;
  }

  int empty_outputs_count = 0;
  int distance = 0;

  int look_dist = m_world->GetConfig().LOOK_DIST.Get();
  int cur_x;
  int cur_y;
  bool found_closest = false;
  for (int cur_dist = 1; cur_dist <= look_dist; cur_dist++) {
    // Center faced cells
    cur_x = cur_dist * facing_offset_x;
    cur_y = cur_dist * facing_offset_y;
    if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
    cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
    if ((cell.GetNumAVOutputs() > 0 || cell.GetCanInput()) && cell.GetNumAVInputs() == 0) {
      if (!found_closest) {
        distance = cur_dist;
        found_closest = true;
      }
      empty_outputs_count += cell.GetNumAVOutputs();
      if (cell.GetCanInput()) empty_outputs_count++;
    }

    // Left faced cells
    int look_width = cur_dist / 2;
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * left_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * left_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
      cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
      if ((cell.GetNumAVOutputs() > 0 || cell.GetCanInput()) && cell.GetNumAVInputs() == 0) {
        if (!found_closest) {
          distance = cur_dist;
          found_closest = true;
        }
        empty_outputs_count += cell.GetNumAVOutputs();
        if (cell.GetCanInput()) empty_outputs_count++;
      }
    }

    // Right faced cells
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * right_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * right_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
      cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
      if ((cell.GetNumAVOutputs() > 0 || cell.GetCanInput()) && cell.GetNumAVInputs() == 0) {
        if (!found_closest) {
          distance = cur_dist;
          found_closest = true;
        }
        empty_outputs_count += cell.GetNumAVOutputs();
        if (cell.GetCanInput()) empty_outputs_count++;
      }
    }
  }

  tArray<int> return_values(2);
  return_values[0] = empty_outputs_count;
  return_values[1] = distance;
  return return_values;
}

tArray<int> cPopulationInterface::NeuronLookOutputs(cAvidaContext& ctx, int av_num)
{
  const int av_cell_x = GetAVCellXPosition(av_num);
  const int av_cell_y = GetAVCellYPosition(av_num);
  const int facing = m_avatars[av_num].av_facing;

  cDeme* deme = GetDeme();
  const int deme_x_size = m_world->GetConfig().WORLD_X.Get();
  const int deme_y_size = deme->GetSize() / deme_x_size;

  int facing_offset_x = 0;
  int facing_offset_y = 0;
  int left_offset_x = 0;
  int left_offset_y = 0;
  int right_offset_x = 0;
  int right_offset_y = 0;
  switch (facing) {
  case 0:
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_x = 1;
    break;
  case 1:
    facing_offset_x = 1;
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_y = 1;
    break;
  case 2:
    facing_offset_x = 1;
    left_offset_y = -1;
    right_offset_y = 1;
    break;
  case 3:
    facing_offset_x = 1;
    facing_offset_y = 1;
    left_offset_y = -1;
    right_offset_x = -1;
    break;
  case 4:
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_x = -1;
    break;
  case 5:
    facing_offset_x = -1;
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_y = -1;
    break;
  case 6:
    facing_offset_x = -1;
    left_offset_y = 1;
    right_offset_y = -1;
    break;
  case 7:
    facing_offset_x = -1;
    facing_offset_y = -1;
    left_offset_y = 1;
    right_offset_x = 1;
    break;
  }

  int outputs_count = 0;
  int distance = 0;

  int look_dist = m_world->GetConfig().LOOK_DIST.Get();
  int cur_x;
  int cur_y;
  bool found_closest = false;
  for (int cur_dist = 1; cur_dist <= look_dist; cur_dist++) {
    // Center faced cells
    cur_x = cur_dist * facing_offset_x;
    cur_y = cur_dist * facing_offset_y;
    if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
    cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
    if (cell.GetNumAVOutputs() || cell.GetCanInput()) {
      if (!found_closest) {
        distance = cur_dist;
        found_closest = true;
      }
      outputs_count += cell.GetNumAVOutputs();
      if (cell.GetCanInput()) outputs_count++;
    }

    // Left faced cells
    int look_width = cur_dist / 2;
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * left_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * left_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
      cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
      if (cell.GetNumAVOutputs() || cell.GetCanInput()) {
        if (!found_closest) {
          distance = cur_dist;
          found_closest = true;
        }
        outputs_count += cell.GetNumAVOutputs();
        if (cell.GetCanInput()) outputs_count++;
      }
    }

    // Right faced cells
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * right_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * right_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;
      cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
      if (cell.GetNumAVOutputs() || cell.GetCanInput()) {
        if (!found_closest) {
          distance = cur_dist;
          found_closest = true;
        }
        outputs_count += cell.GetNumAVOutputs();
        if (cell.GetCanInput()) outputs_count++;
      }
    }
  }

  tArray<int> return_values(2);
  return_values[0] = outputs_count;
  return_values[1] = distance;
  return return_values;
}

tArray<int> cPopulationInterface::NeuronLookUnconnectedOutputs(cAvidaContext& ctx, int av_num)
{
  const int av_cell_x = GetAVCellXPosition(av_num);
  const int av_cell_y = GetAVCellYPosition(av_num);
  const int facing = m_avatars[av_num].av_facing;

  cDeme* deme = GetDeme();
  const int deme_x_size = m_world->GetConfig().WORLD_X.Get();
  const int deme_y_size = deme->GetSize() / deme_x_size;

  int facing_offset_x = 0;
  int facing_offset_y = 0;
  int left_offset_x = 0;
  int left_offset_y = 0;
  int right_offset_x = 0;
  int right_offset_y = 0;
  switch (facing) {
  case 0:
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_x = 1;
    break;
  case 1:
    facing_offset_x = 1;
    facing_offset_y = -1;
    left_offset_x = -1;
    right_offset_y = 1;
    break;
  case 2:
    facing_offset_x = 1;
    left_offset_y = -1;
    right_offset_y = 1;
    break;
  case 3:
    facing_offset_x = 1;
    facing_offset_y = 1;
    left_offset_y = -1;
    right_offset_x = -1;
    break;
  case 4:
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_x = -1;
    break;
  case 5:
    facing_offset_x = -1;
    facing_offset_y = 1;
    left_offset_x = 1;
    right_offset_y = -1;
    break;
  case 6:
    facing_offset_x = -1;
    left_offset_y = 1;
    right_offset_y = -1;
    break;
  case 7:
    facing_offset_x = -1;
    facing_offset_y = -1;
    left_offset_y = 1;
    right_offset_x = 1;
    break;
  }

  int outputs_count = 0;
  int distance = 0;

  int look_dist = m_world->GetConfig().LOOK_DIST.Get();
  int cur_x;
  int cur_y;
  int cell_id;
  bool found_closest = false;
  for (int cur_dist = 1; cur_dist <= look_dist; cur_dist++) {
    // Center faced cells
    cur_x = cur_dist * facing_offset_x;
    cur_y = cur_dist * facing_offset_y;
    if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;

    cell_id = deme->GetCellID(cur_x, cur_y);
    bool has_m_AV = false;
    for (int i = 0; i < m_avatars.GetSize(); i++) {
      if (m_avatars[i].av_cell_id == cell_id) {
        has_m_AV = true;
        break;
      }
    }
    if (!has_m_AV) {
      cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
      if (cell.GetNumAVOutputs() || cell.GetCanInput()) {
        if (!found_closest) {
          distance = cur_dist;
          found_closest = true;
        }
        outputs_count += cell.GetNumAVOutputs();
        if (cell.GetCanInput()) outputs_count++;
      }
    }

    // Left faced cells
    int look_width = cur_dist / 2;
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * left_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * left_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;

      cell_id = deme->GetCellID(cur_x, cur_y);
      bool has_m_AV = false;
      for (int i = 0; i < m_avatars.GetSize(); i++) {
        if (m_avatars[i].av_cell_id == cell_id) {
          has_m_AV = true;
          break;
        }
      }
      if (!has_m_AV) {
        cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
        if (cell.GetNumAVOutputs() || cell.GetCanInput()) {
          if (!found_closest) {
            distance = cur_dist;
            found_closest = true;
          }
          outputs_count += cell.GetNumAVOutputs();
          if (cell.GetCanInput()) outputs_count++;
        }
      }
    }

    // Right faced cells
    for (int width_offset = 1; width_offset <= look_width; width_offset++) {
      cur_x = cur_dist * facing_offset_x + width_offset * right_offset_x;
      cur_y = cur_dist * facing_offset_y + width_offset * right_offset_y;
      if (cur_x < 0 || cur_x >= deme_x_size || cur_y < 0 || cur_y >= deme_y_size) break;

      cell_id = deme->GetCellID(cur_x, cur_y);
      bool has_m_AV = false;
      for (int i = 0; i < m_avatars.GetSize(); i++) {
        if (m_avatars[i].av_cell_id == cell_id) {
          has_m_AV = true;
          break;
        }
      }
      if (!has_m_AV) {
        cPopulationCell& cell = deme->GetCell(cur_x, cur_y);
        if (cell.GetNumAVOutputs() || cell.GetCanInput()) {
          if (!found_closest) {
            distance = cur_dist;
            found_closest = true;
          }
          outputs_count += cell.GetNumAVOutputs();
          if (cell.GetCanInput()) outputs_count++;
        }
      }
    }
  }

  tArray<int> return_values(2);
  return_values[0] = outputs_count;
  return_values[1] = distance;
  return return_values;
}


void cPopulationInterface::BeginSleep()
{
  if(m_world->GetConfig().LOG_SLEEP_TIMES.Get() == 1)
    m_world->GetPopulation().AddBeginSleep(m_cell_id, m_world->GetStats().GetUpdate());
  GetDeme()->IncSleepingCount();
}

void cPopulationInterface::EndSleep()
{
  if(m_world->GetConfig().LOG_SLEEP_TIMES.Get() == 1)
    m_world->GetPopulation().AddEndSleep(m_cell_id, m_world->GetStats().GetUpdate());
  GetDeme()->DecSleepingCount();
}
