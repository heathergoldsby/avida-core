/*
 *  cReactionResult.cc
 *  Avida
 *
 *  Called "reaction_result.cc" prior to 12/5/05.
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
 *  Copyright 1993-2004 California Institute of Technology.
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

#include "cReactionResult.h"


cReactionResult::cReactionResult(const int num_resources,
				 const int num_tasks,
				 const int num_reactions)
  : resources_consumed(num_resources)
  , resources_produced(num_resources)
  , resources_detected(num_resources)
  , tasks_done(num_tasks)
  , tasks_quality(num_tasks)
  , tasks_value(num_tasks)
  , task_reward_bonus(num_tasks)
  , reactions_triggered(num_reactions)
  , reaction_add_bonus(num_reactions)
  , energy_add(0.0)
  , bonus_add(0.0)
  , bonus_mult(1.0)
  , insts_triggered(0)
  , lethal(false)
  , active_reaction(false)
{
}

void cReactionResult::ActivateReaction()
{
  // If this reaction is already active, don't worry about it.
  if (active_reaction == true) return;

  // To activate the reaction, we must initialize all counter settings.
  resources_consumed.SetAll(0.0);
  resources_produced.SetAll(0.0);
  resources_detected.SetAll(-1.0);
  tasks_done.SetAll(false);
  tasks_quality.SetAll(0.0);
  tasks_value.SetAll(0.0);
  task_reward_bonus.SetAll(0.0);
  reactions_triggered.SetAll(false);
  reaction_add_bonus.SetAll(0.0);

  // And finally note that this is indeed already active.
  active_reaction = true;
}


void cReactionResult::Consume(int id, double num)
{
  ActivateReaction();
  resources_consumed[id] += num;
}


void cReactionResult::Produce(int id, double num)
{
  ActivateReaction();
  resources_produced[id] += num;
}


void cReactionResult::Detect(int id, double num)
{
  ActivateReaction();
  resources_detected[id] += num;
}

void cReactionResult::Lethal(bool flag)
{
 ActivateReaction();
 lethal = flag;
}

void cReactionResult::MarkTask(int id, const double quality, const double value)
{
  ActivateReaction();
  tasks_done[id] = true;
  tasks_quality[id] = quality;
  tasks_value[id] = value;
}


void cReactionResult::MarkReaction(int id)
{
  ActivateReaction();
  reactions_triggered[id] = true;
}

void cReactionResult::AddEnergy(double value)
{
  ActivateReaction();
  energy_add += value;
}

void cReactionResult::AddBonus(double value, int id)
{
  ActivateReaction();
  bonus_add += value;
  reaction_add_bonus[id] += value;
}


void cReactionResult::MultBonus(double value)
{
  ActivateReaction();
  bonus_mult *= value;
}

void cReactionResult::AddInst(int id)
{
  insts_triggered.Push(id);
}

double cReactionResult::GetConsumed(int id)
{
  if (GetActive() == false) return 0.0;
  return resources_consumed[id];
}


double cReactionResult::GetProduced(int id)
{
  if (GetActive() == false) return 0.0;
  return resources_produced[id];
}

double cReactionResult::GetDetected(int id)
{
  if (GetActive() == false) return 0.0;
  return resources_detected[id];
}

bool cReactionResult::GetLethal()
{
  if (GetActive() == false) return false;
  return lethal;
}

bool cReactionResult::ReactionTriggered(int id)
{
  if (GetActive() == false) return false;
  return reactions_triggered[id];
}

bool cReactionResult::TaskDone(int id)
{
  if (GetActive() == false) return false;
  return tasks_done[id];
}

double cReactionResult::TaskQuality(int id)
{
	if (GetActive() == false) return 0;
	return tasks_quality[id];
}

double cReactionResult::TaskValue(int id)
{
	if (GetActive() == false) return 0;
	return tasks_value[id];
}

double cReactionResult::GetTaskBonus(int id)
{
  if (GetActive() == false) return 0.0;
  return task_reward_bonus[id];
}

void cReactionResult::AddTaskBonus(int id, double bonus)
{
    task_reward_bonus[id] += bonus;
}
