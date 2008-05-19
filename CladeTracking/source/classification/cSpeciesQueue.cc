/*
 *  cSpeciesQueue.cc
 *  Avida
 *
 *  Called "species_queue.cc" prior to 11/30/05.
 *  Copyright 1999-2008 Michigan State University. All rights reserved.
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

#include "cSpeciesQueue.h"

#include "defs.h"
#include "cSpecies.h"


void cSpeciesQueue::InsertRear(cSpecies & new_species)
{
  // If the queue doesn't exist, create it with this species as the only
  // element.

  if (!first) {
    first = &new_species;
    first->SetNext(first);
    first->SetPrev(first);
  }

  // Otherwise, put this species at the end of the queue.

  else {
    new_species.SetNext(first);
    new_species.SetPrev(first->GetPrev());
    first->GetPrev()->SetNext(&new_species);
    first->SetPrev(&new_species);
  }

  size++;
}

void cSpeciesQueue::Remove(cSpecies & in_species)
{
  size--;

  // If the queue is now empty, delete it properly.
  if (size == 0) {
    first = NULL;
    return;
  }

  // If we are removing the first element of the queue, slide the queue to
  // the new first before removing it.

  if (first == &in_species) {
    first = in_species.GetNext();
  }

  // Remove the in_species

  in_species.GetPrev()->SetNext(in_species.GetNext());
  in_species.GetNext()->SetPrev(in_species.GetPrev());
  in_species.SetNext(NULL);
  in_species.SetPrev(NULL);
}

void cSpeciesQueue::Adjust(cSpecies & in_species)
{
  // First move it up the list if need be...

  cSpecies * prev_species = in_species.GetPrev();
  while (&in_species != first &&
	 in_species.GetNumThreshold() > prev_species->GetNumThreshold()) {
    // Swap the position of this species with the previous one.
    if (prev_species == first) first = &in_species;

    // Outer connections...
    prev_species->SetNext(in_species.GetNext());
    in_species.GetNext()->SetPrev(prev_species);
    in_species.SetPrev(prev_species->GetPrev());
    prev_species->GetPrev()->SetNext(&in_species);

    // Inner connections...
    prev_species->SetPrev(&in_species);
    in_species.SetNext(prev_species);

    prev_species = in_species.GetPrev();
  }
	
  // Then see if it needs to be moved down.
  cSpecies * next_species = in_species.GetNext();
  while (next_species != first &&
	 in_species.GetNumThreshold() < next_species->GetNumThreshold()) {
    // Swap the position of this species with the next one.
    if (&in_species == first) first = next_species;

    // Outer Connections...
    in_species.SetNext(next_species->GetNext());
    next_species->GetNext()->SetPrev(&in_species);
    next_species->SetPrev(in_species.GetPrev());
    in_species.GetPrev()->SetNext(next_species);

    // Inner Connections...
    in_species.SetPrev(next_species);
    next_species->SetNext(&in_species);

    next_species = in_species.GetNext();
  }
}

void cSpeciesQueue::Purge()
{
  cSpecies * cur_species = first;
  cSpecies * next_species = NULL;

  // Loop through the species deleting them until there is only one left.
  for (int i = 1; i < size; i++) {
    next_species = cur_species->GetNext();
    delete cur_species;
    cur_species = next_species;
  }

  // And delete that last one.
  delete cur_species;
  first = NULL;
  size = 0;
}

bool cSpeciesQueue::OK(int queue_type)
{
  cSpecies * cur_species = NULL;
  int count = 0;
  bool ret_value = true;

  while (first && cur_species != first) {
    // If we are just starting, set cur_species to the first element.
    if (!cur_species) cur_species = first;

    // Check that the list is correct in both directions...

    assert (cur_species->GetNext()->GetPrev() == cur_species);

    // Check to make sure the current species is OK() and that it should
    // indeed be in this list.

    if (!cur_species->OK()) ret_value = false;

    assert (queue_type == cur_species->GetQueueType());
    count++;

    assert (count <= size);

    cur_species = cur_species->GetNext();
  }

  assert (count == size);

  return ret_value;
}
