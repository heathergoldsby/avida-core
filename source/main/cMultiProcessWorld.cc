/*
 *  cMultiProcessWorld.cc
 *  Avida
 *
 *  Copyright 1999-2009 Michigan State University. All rights reserved.
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

#include "defs.h"
#if BOOST_IS_AVAILABLE
#include "cGenome.h"
#include "cOrganism.h"
#include "cMerit.h"
#include "cWorld.h"
#include "cPopulation.h"
#include "cMultiProcessWorld.h"
#include <map>
#include <iostream>
#include <sstream>
#include <cmath>
#include <boost/optional.hpp>


/*! Message that is sent from one MPI world to another during organism migration.
 */
struct migration_message {
	migration_message() { }
	migration_message(cOrganism* org, const cPopulationCell& cell) {
		_genome = org->GetGenome().AsString();
		cell.GetPosition(_x, _y);
	}
	
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & _genome & _x & _y;
	}
	
	std::string _genome; //!< Genome of the organism.
	int _x; //!< X-coordinate of the cell from which this migrant originated.
	int _y; //!< Y-coordinate of the cell from which this migrant originated.
};


/*! Constructor.
 
 Since we're running in a multi-process environment from a single command line,
 we need to tweak the random seed and data dirs a bit.
 */
cMultiProcessWorld::cMultiProcessWorld(cAvidaConfig* cfg, boost::mpi::environment& env, boost::mpi::communicator& world) 
: cWorld(cfg)
, m_mpi_env(env)
, m_mpi_world(world)
, m_universe_dim(0)
, m_universe_x(0)
, m_universe_y(0) {
	
	if(GetConfig().MP_MIGRATION_STYLE.Get() == 1) {
		m_universe_dim = static_cast<std::size_t>(sqrt(m_mpi_world.size()));
		if((m_universe_dim*m_universe_dim) != m_mpi_world.size()) {
			GetDriver().RaiseFatalException(-1, "Spatial Avida-MP worlds must be square.");
		}
		
		m_universe_x = m_mpi_world.rank() % m_universe_dim;
		m_universe_y = m_mpi_world.rank() / m_universe_dim;
	}
}


/*! Migrate this organism to a different world.
 
 Send this organism to a different world, selected at random.
 */
void cMultiProcessWorld::MigrateOrganism(cOrganism* org, const cPopulationCell& cell) {
	assert(org!=0);
	int dst_world=-1;
	
	// which world is this organism migrating to?
	switch(GetConfig().MP_MIGRATION_STYLE.Get()) {
		case 0: { // mass action
			dst_world = GetRandom().GetInt(m_mpi_world.size());
			break;
		}
		case 1: { // spatial
			int x, y;
			cell.GetPosition(x,y);
			if(x == 0) {
				// migrate left
				dst_world = m_mpi_world.rank() - 1;
			} else if(x == (GetConfig().WORLD_X.Get()-1)) {
				// migrate right
				dst_world = m_mpi_world.rank() + 1;
			} else if(y == 0) {
				// migrate down
				dst_world = m_mpi_world.rank() - m_universe_dim;
			} else if(y == (GetConfig().WORLD_Y.Get()-1)) {
				// migrate up
				dst_world = m_mpi_world.rank() + m_universe_dim;
			}
			break;
		}
		default: {
			GetDriver().RaiseFatalException(-1, "Unrecognized MP_MIGRATION_STYLE.");
		}
	}
	
	// the tag is set to the number of messages previously sent; this is to allow
	// the receiver to sort messages for consistency.
	m_reqs.push_back(m_mpi_world.isend(dst_world, m_reqs.size(), migration_message(org, cell)));
}


/*! Returns true if the given cell is on the boundary of the world, false otherwise.
 */
bool cMultiProcessWorld::IsWorldBoundary(const cPopulationCell& cell) {
	if(GetConfig().ENABLE_MP.Get() && (GetConfig().MP_MIGRATION_STYLE.Get()==1)) {
		int x, y;
		cell.GetPosition(x,y);
		
		// if this cell isn't on the boundary of this world, then there's no way that
		// it can cause a migration, regardless of world geometry.
		if(!((x == 0) || (x == (GetConfig().WORLD_X.Get()-1)) || (y == 0) || (y == (GetConfig().WORLD_Y.Get()-1)))) {
			return false;
		}
		
		switch(GetConfig().WORLD_GEOMETRY.Get()) {
			case 1: { // bounded grid: prevent the universe boundary cells from causing migrations.
				int uni_x = x + GetConfig().WORLD_X.Get() * m_universe_x;
				int uni_y = y + GetConfig().WORLD_Y.Get() * m_universe_y;
				return !((uni_x == 0) || (uni_x == (GetConfig().WORLD_X.Get() * m_universe_dim-1))
								 || (uni_y == 0) || (uni_y == (GetConfig().WORLD_Y.Get() * m_universe_dim-1)));
			}				
			case 2: { // torus: this always results in a migration (because, if we reached
				// here, we know the cell is on the boundary).
				return true;
			}
			default: {
				GetDriver().RaiseFatalException(-1, "Only bounded grid and toroidal geometries are supported for cell migration.");
			}
		}
	}
	return false;	
}


/*! Process post-update events.
 
 This method is called after each update of the local population completes.  Here
 we check to see if this world has received any migrants from other worlds, and if
 so, we inject them into the local population.  Note that this is an unconditional
 injection -- that is, migrants are "pushed" to this world.
 
 Migrants are always injected at a random location in this world.
 
 \todo What to do about cross-world lineage labels?
 
 \todo At the moment, this method forces synchronization on update boundaries across
 *all* worlds.  This could slow things down quite a bit, so we may need to consider a
 second mode that relaxes consistency in favor of speed.
 */
void cMultiProcessWorld::ProcessPostUpdate(cAvidaContext& ctx) {
	namespace mpi = boost::mpi;
	using namespace boost;
	
	// wait until we're sure that this process has sent all its messages:
	mpi::wait_all(m_reqs.begin(), m_reqs.end());
	m_reqs.clear();
	
	// at this point, we know that *this* process has sent everything.  but, we don't
	// know if it's *received* everything.  so, we're going to put in a synchronization
	// barrier, which means that every process must reach the barrier before any are allowed
	// to proceed.  since we just finished waiting for all communication to complete,
	// this means that all messages must have been received, too.
	m_mpi_world.barrier();
	
	// now, receive all the messages, but store them in order by source and tag:
	typedef std::map<int,migration_message> rx_tag_t;
	typedef std::map<int,rx_tag_t> rx_src_t;
	rx_src_t recvd;
	optional<mpi::status> s = m_mpi_world.iprobe(mpi::any_source,mpi::any_tag);
	while(s.is_initialized()) {
		migration_message msg;
		m_mpi_world.recv(s->source(), s->tag(), msg);
		recvd[s->source()][s->tag()] = msg;		
		// any others?
		s = m_mpi_world.iprobe(mpi::any_source,mpi::any_tag);		
	}
		
	// iterate over received messages in-order, injecting genomes into our population:
	for(rx_src_t::iterator i=recvd.begin(); i!=recvd.end(); ++i) {
		for(rx_tag_t::iterator j=i->second.begin(); j!=i->second.end(); ++j) {
			//std::cerr << "**********  INJECTING *********** " << i->first << " " << j->first << std::endl;
			// ok, add this migrant to the current population
			migration_message& migrant = j->second;
			int target_cell=-1;

			switch(GetConfig().MP_MIGRATION_STYLE.Get()) {
				case 0: { // mass action
					target_cell = GetRandom().GetInt(GetPopulation().GetSize());
					break;
				}
				case 1: { // spatial
					// invert the orginating cell
					migrant._x = GetConfig().WORLD_X.Get() - migrant._x - 1;
					migrant._y = GetConfig().WORLD_Y.Get() - migrant._y - 1;
					target_cell = GetConfig().WORLD_Y.Get() * migrant._y + migrant._x;
					break;
				}
				default: {
					GetDriver().RaiseFatalException(-1, "Unrecognized MP_MIGRATION_STYLE.");
				}
			}
			
			GetPopulation().InjectGenome(target_cell,
																	 SRC_ORGANISM_RANDOM, // for right now, we'll treat this as a random organism injection
																	 cGenome(cString(migrant._genome.c_str())), // genome unpacked from message
																	 -1); // lineage label??
		}
	}
	
	// oh, sweet sanity; make sure that we actually processed all messages.
	assert(!m_mpi_world.iprobe(mpi::any_source,mpi::any_tag).is_initialized());
	
	// finally, we need another barrier down here, in the off chance that one of the
	// processes is really speedy and manages to migrate another org to this world
	// before we finished the probe-loop.
	m_mpi_world.barrier();
}

#endif // boost_is_available