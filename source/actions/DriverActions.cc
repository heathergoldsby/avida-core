/*
 *  DriverActions.cc
 *  Avida
 *
 *  Created by David Bryson on 7/19/06.
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

#include "DriverActions.h"

#include "cAction.h"
#include "cActionLibrary.h"
#include "cStats.h"
#include "cWorld.h"
#include "cWorldDriver.h"

class cActionExit : public cAction
{
public:
  cActionExit(cWorld* world, const cString& args) : cAction(world, args) { ; }
  static const cString GetDescription() { return "No Arguments"; }
  void Process(cAvidaContext& ctx) { m_world->GetDriver().SetDone(); }
};

class cActionExitAveLineageLabelGreater : public cAction
{
private:
  double m_threshold;
public:
  cActionExitAveLineageLabelGreater(cWorld* world, const cString& args) : cAction(world, args), m_threshold(0.0)
  {
    cString largs(args);
    if (largs.GetSize()) m_threshold = largs.PopWord().AsDouble();
  }
  
  static const cString GetDescription() { return "Arguments: <double threshold>"; }
  
  void Process(cAvidaContext& ctx)
  {
    if (m_world->GetStats().GetAveLineageLabel() > m_threshold) {
      m_world->GetDriver().SetDone();
    }
  }
};

class cActionExitAveLineageLabelLess : public cAction
{
private:
  double m_threshold;
public:
  cActionExitAveLineageLabelLess(cWorld* world, const cString& args) : cAction(world, args), m_threshold(0.0)
  {
    cString largs(args);
    if (largs.GetSize()) m_threshold = largs.PopWord().AsDouble();
  }
  
  static const cString GetDescription() { return "Arguments: <double threshold>"; }
  
  void Process(cAvidaContext& ctx)
  {
    if (m_world->GetStats().GetAveLineageLabel() < m_threshold) {
      m_world->GetDriver().SetDone();
    }
  }
};


class cActionExitPropertyGeneratedGreater : public cAction
{
private:
  double m_threshold;
public:
  cActionExitPropertyGeneratedGreater(cWorld* world, const cString& args) : cAction(world, args), m_threshold(0.0)
  {
    cString largs(args);
    if (largs.GetSize()) m_threshold = largs.PopWord().AsDouble();
  }
  
  static const cString GetDescription() { return "Arguments: <double "; }
  
  void Process(cAvidaContext& ctx)
  {
	if (m_world->GetStats().getPropTotal() > m_threshold ) {
		m_world->GetDriver().SetDone();
    }
  }
};

class cActionExitModelsPassedPropertyGreater : public cAction
{
private:
	double m_threshold;
public:
	cActionExitModelsPassedPropertyGreater(cWorld* world, const cString& args) : cAction(world, args), m_threshold(0.0)
	{
		cString largs(args);
		if (largs.GetSize()) m_threshold = largs.PopWord().AsDouble();
	}
	
	static const cString GetDescription() { return "Arguments: <double "; }
	
	void Process(cAvidaContext& ctx)
	{
		if (m_world->GetStats().getN1Passed() > m_threshold ) {
			m_world->GetDriver().SetDone();
		}
	}
};

/*! Exit Avida when the elapsed wallclock time has exceeded a threshold number
of seconds, beginning from the construction of this object.
*/
class cActionExitElapsedTime : public cAction {
public:
	/*! Constructor; parse out the threshold time.
	*/
	cActionExitElapsedTime(cWorld* world, const cString& args) : cAction(world, args) {
		cString largs(args);
		if(largs.GetSize()) {
			m_time = largs.PopWord().AsInt();
		} else {
			// error; no default value for elapsed time.
			m_world->GetDriver().RaiseFatalException(-1, "ExitElapsedTime event requires elapsed time.");
		}
		
		// When did we start?
		m_then = time(0);
	}
	
	static const cString GetDescription() { return "Arguments: <int elapsed time [seconds]>"; }
	
	/*! Check to see if we should exit Avida based on the elapsed time since construction
	of this object.  This method is called based on the events file.
	*/
	void Process(cAvidaContext& ctx) {
		if((time(0) - m_then) >= m_time) {
			m_world->GetDriver().SetDone();
		}
	}
	
protected:
	int m_time; //!< Number of seconds after which Avida should exit.
	int m_then; //!< Time at which this object was constructed (the 'start' of Avida).
};

void RegisterDriverActions(cActionLibrary* action_lib)
{
  action_lib->Register<cActionExit>("Exit");
  action_lib->Register<cActionExitAveLineageLabelGreater>("ExitAveLineageLabelGreater");
  action_lib->Register<cActionExitAveLineageLabelLess>("ExitAveLineageLabelLess");
  action_lib->Register<cActionExitPropertyGeneratedGreater>("ExitPropGeneratedGreater");
  action_lib->Register<cActionExitModelsPassedPropertyGreater>("ExitModelsPassedPropertyGreater");
  action_lib->Register<cActionExitElapsedTime>("ExitElapsedTime");
  

  // @DMB - The following actions are DEPRECATED aliases - These will be removed in 2.7.
  action_lib->Register<cActionExit>("exit");
  action_lib->Register<cActionExitAveLineageLabelGreater>("exit_if_ave_lineage_label_larger");
  action_lib->Register<cActionExitAveLineageLabelLess>("exit_if_ave_lineage_label_smaller");
}


