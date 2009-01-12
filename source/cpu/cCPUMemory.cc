/*
 *  cCPUMemory.cc
 *  Avida
 *
 *  Called "cpu_memory.cc" prior to 11/22/05.
 *  Copyright 1999-2009 Michigan State University. All rights reserved.
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

#include "cCPUMemory.h"

using namespace std;

const int MEMORY_INCREASE_MINIMUM = 10;
const double MEMORY_INCREASE_FACTOR = 1.5;
const double MEMORY_SHRINK_TEST_FACTOR = 4.0;

cCPUMemory::cCPUMemory(const cCPUMemory& in_memory)
  : cGenome(in_memory), flag_array(in_memory.GetSize())
{
  for (int i = 0; i < flag_array.GetSize(); i++) {
    flag_array[i] = in_memory.flag_array[i];
  }
}


void cCPUMemory::SloppyResize(int new_size)
{
  assert(new_size > 0);

  // Make sure we're really changing the size...
  if (new_size == active_size) return;

  const int array_size = genome.GetSize();

  // Determine if we need to adjust the allocated array sizes...
  if (new_size > array_size || new_size * MEMORY_SHRINK_TEST_FACTOR < array_size) {
    int new_array_size = (int) (new_size * MEMORY_INCREASE_FACTOR);
    const int new_array_min = new_size + MEMORY_INCREASE_MINIMUM;
		if (new_array_min > new_array_size) new_array_size = new_array_min;
    genome.Resize(new_array_size);
		protected_sites.Resize(new_array_size, false);
    flag_array.Resize(new_array_size);
  }
  
  // And just change the active_size once we're sure it will be in range.
  active_size = new_size;
}


void cCPUMemory::SloppyInsert(int pos, int num_lines)
{
  assert(pos >= 0 && pos <= active_size); // Must insert at a legal position!
  assert(num_lines > 0);  // Must insert positive number of lines!

  // Re-adjust the size...
  const int old_size = active_size;
  const int new_size = active_size + num_lines;
  SloppyResize(new_size);

  // Shift any lines needed...
  for (int i = old_size - 1; i >= pos; i--) {
    genome[i+num_lines] = genome[i];
		protected_sites[i+num_lines] = protected_sites[i];
    flag_array[i+num_lines] = flag_array[i];
  }
}


// ---  Public Methods ---


void cCPUMemory::operator=(const cCPUMemory & other_memory)
{
  SloppyResize(other_memory.active_size);

	protected_sites.SetAll(false);
  // Fill in the new information...
  for (int i = 0; i < active_size; i++) {
    genome[i] = other_memory.genome[i];
		protected_sites[i] = other_memory.IsProtected(i);
    flag_array[i] = other_memory.flag_array[i];
  }
}


void cCPUMemory::operator=(const cGenome & other_genome)
{
  SloppyResize(other_genome.GetSize());

	protected_sites.SetAll(false);
  // Fill in the new information...
  for (int i = 0; i < active_size; i++) {
    genome[i] = other_genome[i];
		protected_sites[i] = other_genome.IsProtected(i);
    flag_array[i] = 0;
  }
}

void cCPUMemory::Copy(int to, int from)
{
  assert(to >= 0);
  assert(to < genome.GetSize());
  assert(from >= 0);
  assert(from < genome.GetSize());

  genome[to] = genome[from];
	protected_sites[to] = protected_sites[from];
  flag_array[to] = flag_array[from];
}

void cCPUMemory::Reset(int new_size)
{
  assert(new_size >= 0);

  SloppyResize(new_size);
  Clear();
}


void cCPUMemory::Resize(int new_size)
{
  assert(new_size >= 0);

  // Do a sloppy resize first, saving old values...
  const int old_size = active_size;
  SloppyResize(new_size);
  
  // Clean up all of the old memory that might need it...
  for (int i = old_size; i < new_size; i++) {
    genome[i].SetOp(0);
		protected_sites[i] = false;
    flag_array[i] = 0;
  }
}


void cCPUMemory::ResizeOld(int new_size)
{
  assert(new_size >= 0);

  const int old_size = active_size;

  // Do a sloppy resize, which will still have old values.
  SloppyResize(new_size);

  for (int i = old_size; i < new_size; i++) {
    flag_array[i] = 0;
  }
}


void cCPUMemory::Insert(int pos, const cInstruction & in_inst)
{
  assert(pos >= 0);
  assert(pos <= genome.GetSize());

  SloppyInsert(pos, 1);
  genome[pos] = in_inst;
	protected_sites[pos] = false;
  flag_array[pos] = 0;
}

void cCPUMemory::Insert(int pos, const cGenome & in_genome)
{
  assert(pos >= 0);
  assert(pos <= genome.GetSize());

  SloppyInsert(pos, in_genome.GetSize());
  for (int i = 0; i < in_genome.GetSize(); i++) {
    genome[i+pos] = in_genome[i];
		protected_sites[i+pos] = protected_sites[i];
    flag_array[i+pos] = 0;
  }
}

void cCPUMemory::Remove(int pos, int num_insts)
{
  assert(num_insts > 0);                  // Must remove something...
  assert(pos >= 0);                       // Removal must be in genome.
  assert(pos + num_insts <= active_size); // Cannot extend past end of genome.

	int new_size = active_size - num_insts;
  for (int i = pos; i < new_size; i++) {
		if(protected_sites[i]) {
			new_size++;
		} else {
			genome[i] = genome[i + num_insts];
			protected_sites[i] = protected_sites[i + num_insts];
			flag_array[i] = flag_array[i + num_insts];
		}
  }
  SloppyResize(new_size);
}

void cCPUMemory::Replace(int pos, int num_insts, const cGenome & in_genome)
{
  assert(pos >= 0);                       // Replace must be in genome.
  assert(num_insts >= 0);                 // Cannot replace negative.
  assert(pos + num_insts <= active_size); // Cannot extend past end!

  const int size_change = in_genome.GetSize() - num_insts;

  // First, get the size right.
  if (size_change > 0) SloppyInsert(pos, size_change);
  else if (size_change < 0) Remove(pos, -size_change);  // BEB TODO: what should happen on an attempt to replace a protected site 

  // Now just copy everything over!
  for (int i = 0; i < in_genome.GetSize(); i++) {
    genome[i + pos] = in_genome[i];
    flag_array[i + pos] = 0;
  }
}
