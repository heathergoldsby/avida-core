/*
 *  cInjectGenotypeQueue.cc
 *  Avida
 *
 *  Called "inject_genotype_queue.cc" prior to 11/30/05.
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
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

#include "cInjectGenotypeQueue.h"

#include "cInjectGenotype.h"

#include <cassert>


cInjectGenotypeQueue::cInjectGenotypeQueue()
{
  size = 0;
  root.SetNext(&root);
  root.SetPrev(&root);
}


cInjectGenotypeQueue::~cInjectGenotypeQueue()
{
  while (root.GetNext() != &root) {
    Remove(root.GetNext());
  }
}

bool cInjectGenotypeQueue::OK()
{
  bool result = true;
  int count = 0;

  for (cInjectGenotypeElement * temp_element = root.GetNext();
       temp_element != &root;
       temp_element = temp_element->GetNext()) {
    assert (temp_element->GetNext()->GetPrev() == temp_element);
    assert (temp_element->GetInjectGenotype()->GetID() >= 0);

    count++;
    assert (count <= size);
  }

  assert (count == size);

  return result;
}

void cInjectGenotypeQueue::Insert(cInjectGenotype & in_inject_genotype)
{
  cInjectGenotypeElement * new_element = new cInjectGenotypeElement(&in_inject_genotype);
  new_element->SetNext(root.GetNext());
  new_element->SetPrev(&root);
  root.GetNext()->SetPrev(new_element);
  root.SetNext(new_element);
  size++;
}

void cInjectGenotypeQueue::Remove(cInjectGenotype & in_inject_genotype)
{
  cInjectGenotypeElement * cur_element;

  for (cur_element = root.GetNext();
       cur_element != &root;
       cur_element = cur_element->GetNext()) {
    if (cur_element->GetInjectGenotype() == &in_inject_genotype) break;
  }

  assert (cur_element != &root);

  Remove(cur_element);
}

void cInjectGenotypeQueue::Remove(cInjectGenotypeElement * in_element)
{
  in_element->GetPrev()->SetNext(in_element->GetNext());
  in_element->GetNext()->SetPrev(in_element->GetPrev());
  in_element->SetNext(NULL);
  in_element->SetPrev(NULL);
  delete(in_element);

  size--;
}

cInjectGenotype * cInjectGenotypeQueue::Find(const cGenome & in_genome) const
{
  for (cInjectGenotypeElement * cur_element = root.GetNext();
       cur_element != &root;
       cur_element = cur_element->GetNext()) {
    if (cur_element->GetInjectGenotype()->GetGenome() == in_genome) {
      return cur_element->GetInjectGenotype();
    }
  }

  return NULL;
}
