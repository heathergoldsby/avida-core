/*
 *  cOrgMessage.cc
 *  Avida
 *
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
 
#include "cOrgMessage.h"
#include "cOrganism.h"


cOrgMessage::cOrgMessage()
: m_pSender(NULL), m_pReceiver(NULL), m_messageType(0), m_data(0)
, m_label(0), m_senderOrgID(-1), m_senderCellID(-1)
, m_receiverOrgID(-1), m_receiverCellID(-1), m_transCellID(-1)
, m_label_task_id(-1), m_data_task_id(-1) {
}


cOrgMessage::cOrgMessage(cOrganism* sender, int messageType)
: m_pSender(sender), m_pReceiver(NULL), m_messageType(messageType), m_data(0)
, m_label(0), m_receiverOrgID(-1), m_receiverCellID(-1), m_transCellID(-1)
, m_label_task_id(-1), m_data_task_id(-1) {
	assert(m_pSender);
	m_senderCellID = sender->GetCellID();
	m_senderOrgID = sender->GetID();
}


void cOrgMessage::SetReceiver(cOrganism *recvr)
{
	m_pReceiver = recvr;
	if (recvr) {
		m_receiverOrgID = recvr->GetID();
		m_receiverCellID = recvr->GetCellID();
	} else {
		m_receiverOrgID = -1;
		m_receiverCellID = -1;
	}
}
