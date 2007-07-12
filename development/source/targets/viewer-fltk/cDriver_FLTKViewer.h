/*
 *  cDriver_FLTKViewer.h
 *  Avida
 *
 *  Created by Charles on 7-9-07
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

#ifndef cDriver_FLTKViewer_h
#define cDriver_FLTKViewer_h

#ifndef cAvidaDriver_h
#include "cAvidaDriver.h"
#endif

#ifndef cCoreView_Info_h
#include "cCoreView_Info.h"
#endif
 
#ifndef cWorldDriver_h
#include "cWorldDriver.h"
#endif

#ifndef cGUIDriver_h
#include "cGUIDriver.h"
#endif

#include <sstream>
#include <iostream>
#include <fstream>

class cWorld;
class cGUIContainer;

using namespace std;

class cDriver_FLTKViewer : public cAvidaDriver, public cWorldDriver,  public cGUIDriver {
private:
  // Overloaded methods from cGUIDriver...
  cGUIWindow * BuildWindow(int width, int height, const cString & name);
  cGUIBox * BuildBox(cGUIContainer * container, int x, int y, int width, int height, const cString & name);

public:
  cDriver_FLTKViewer(cWorld* world);
  ~cDriver_FLTKViewer();
  
  void Run();
  
  // Driver Actions
  void SignalBreakpoint();
  void SetDone() { m_done = true; }

  // IO
  void Flush();
  int GetKeypress() { return 0; } // @CAO FIX!
  bool ProcessKeypress(int keypress);

  void RaiseException(const cString& in_string);
  void RaiseFatalException(int exit_code, const cString& in_string);

  // Drawing and interaction.
  void Draw();
  void DoUpdate();

  // Notifications
  void NotifyComment(const cString& in_string);
  void NotifyWarning(const cString& in_string);
  void NotifyError(const cString& in_string);
  void NotifyOutput(const cString& in_string);
  void Notify(const cString& in_string);

  int Confirm(const cString& in_string);

  // Tests
  bool IsInteractive() { return true; }
};


void ExitFLTKViewer(int exit_code);

#endif
