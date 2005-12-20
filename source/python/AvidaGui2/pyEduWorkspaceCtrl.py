# -*- coding: utf-8 -*-

from pyEduWorkspaceView import pyEduWorkspaceView
from pyMdtr import pyMdtr
from pyOneAnalyzeCtrl import pyOneAnalyzeCtrl
from pyOneOrganismCtrl import pyOneOrganismCtrl
from pyOnePopulationCtrl import pyOnePopulationCtrl
from pyTwoAnalyzeCtrl import pyTwoAnalyzeCtrl
from pyTwoOrganismCtrl import pyTwoOrganismCtrl
from pyTwoPopulationCtrl import pyTwoPopulationCtrl
from pyPetriConfigureCtrl import pyPetriConfigureCtrl
from pyQuitDialogCtrl import pyQuitDialogCtrl
import os.path


from qt import *

class pyEduWorkspaceCtrl(pyEduWorkspaceView):

  def construct(self, session_mdl):
    self.m_session_mdl = session_mdl
    self.m_avida = None
    self.startStatus = True
    self.m_nav_bar_ctrl.construct(session_mdl)
    self.m_freezer_ctrl.construct(session_mdl)
    self.m_cli_to_ctrl_dict = {}
    self.m_ctrl_to_cli_dict = {}
   
    while self.m_widget_stack.visibleWidget():
      self.m_widget_stack.removeWidget(self.m_widget_stack.visibleWidget())

    self.m_one_population_ctrl  = pyOnePopulationCtrl(self.m_widget_stack,  "m_one_population_ctrl")
    self.m_two_population_ctrl  = pyTwoPopulationCtrl(self.m_widget_stack,  "m_two_population_ctrl")
    self.m_one_organism_ctrl    = pyOneOrganismCtrl(self.m_widget_stack,    "m_one_organism_ctrl")
    self.m_two_organism_ctrl    = pyTwoOrganismCtrl(self.m_widget_stack,    "m_two_organism_ctrl")
    self.m_one_analyze_ctrl     = pyOneAnalyzeCtrl(self.m_widget_stack,     "m_one_analyze_ctrl")
    self.m_two_analyze_ctrl     = pyTwoAnalyzeCtrl(self.m_widget_stack,     "m_two_analyze_ctrl")

    for (cli, ctrl) in (
      (self.m_nav_bar_ctrl.m_one_population_cli, self.m_one_population_ctrl),
#      (self.m_nav_bar_ctrl.m_two_population_cli, self.m_two_population_ctrl),
      (self.m_nav_bar_ctrl.m_one_organism_cli,   self.m_one_organism_ctrl),
#      (self.m_nav_bar_ctrl.m_two_organism_cli,   self.m_two_organism_ctrl),
      (self.m_nav_bar_ctrl.m_one_analyze_cli,    self.m_one_analyze_ctrl),
#      (self.m_nav_bar_ctrl.m_two_analyze_cli,    self.m_two_analyze_ctrl),
    ):
      self.m_cli_to_ctrl_dict[cli] = ctrl
      self.m_ctrl_to_cli_dict[ctrl] = cli

    #self.m_nav_bar_ctrl.m_one_population_cli.setState(QCheckListItem.On)
    #self.m_nav_bar_ctrl.m_one_population_cli.setState(QCheckListItem.Off)
    #self.m_nav_bar_ctrl.m_one_population_cli.setSelected(True)
    #self.m_nav_bar_ctrl.m_one_population_cli.setSelected(False)

    #for ctrl in self.m_ctrl_to_cli_dict.keys():
    #  ctrl.construct(self.m_session_mdl)
    self.m_one_population_ctrl.construct(self.m_session_mdl)
    self.m_one_organism_ctrl.construct(self.m_session_mdl)
    self.m_one_analyze_ctrl.construct(self.m_session_mdl)
        
    self.connect(
      self, PYSIGNAL("quitAvidaPhaseISig"), self.startQuitProcessSlot)
    self.connect(self, PYSIGNAL("quitAvidaPhaseIISig"), qApp, SLOT("quit()"))
    self.connect(
      self.m_nav_bar_ctrl.m_list_view, SIGNAL("clicked(QListViewItem *)"), 
      self.navBarItemClickedSlot)
    # self.connect(
    #   self.fileOpenFreezerAction,SIGNAL("activated()"),self.freezerOpenSlot)
    self.connect(
      self.controlNext_UpdateAction,SIGNAL("activated()"),
      self.next_UpdateActionSlot)
    self.connect(
      self.controlStartAction,SIGNAL("activated()"),self.startActionSlot)
    self.connect(
      self.m_session_mdl.m_session_mdtr, PYSIGNAL("setAvidaSig"),
      self.setAvidaSlot)
    self.connect(
      self.m_session_mdl.m_session_mdtr, PYSIGNAL("doPauseAvidaSig"),
      self.doPauseAvidaSlot)
    self.connect(
      self.m_session_mdl.m_session_mdtr, PYSIGNAL("doStartAvidaSig"),
      self.doStartAvidaSlot)

    self.connect(
      self.m_session_mdl.m_session_mdtr,
      PYSIGNAL("doStartSig"),
      self.doStartAvidaSlot)
    self.connect(
      self.m_session_mdl.m_session_mdtr,
      PYSIGNAL("doPauseSig"),
      self.doPauseAvidaSlot)
    # self.connect(
    #   self.m_session_mdl.m_session_mdtr,
    #   PYSIGNAL("doNextUpdateSig"),
    #   self.updatePBClickedSlot)

    self.connect(self.controlRepopulateAction,SIGNAL("activated()"),
      self.RepopulateActionSlot)

    self.connect(self.m_session_mdl.m_session_mdtr, PYSIGNAL("addStatusBarWidgetSig"), self.addStatusBarWidgetSlot)
    self.connect(self.m_session_mdl.m_session_mdtr, PYSIGNAL("removeStatusBarWidgetSig"), self.removeStatusBarWidgetSlot)
    self.connect(self.m_session_mdl.m_session_mdtr, PYSIGNAL("statusBarMessageSig"), self.statusBarMessageSlot)
    self.connect(self.m_session_mdl.m_session_mdtr, PYSIGNAL("statusBarClearSig"), self.statusBarClearSlot)

    self.navBarItemClickedSlot(self.m_nav_bar_ctrl.m_one_population_cli)
    self.m_nav_bar_ctrl.m_list_view.setSelected(self.m_nav_bar_ctrl.m_one_population_cli, True)
    self.splitter1.setSizes([100])

    self.show()

  def __del__(self):
    for key in self.m_cli_to_ctrl_dict.keys():
      del self.m_cli_to_ctrl_dict[key]
    for key in self.m_ctrl_to_cli_dict.keys():
      del self.m_ctrl_to_cli_dict[key]

  def navBarItemClickedSlot(self, item):
    if item:
      if self.m_cli_to_ctrl_dict.has_key(item):
        controller = self.m_cli_to_ctrl_dict[item]
        old_controller = self.m_widget_stack.visibleWidget()
        if old_controller is not None:
          old_controller.aboutToBeLowered()
        controller.aboutToBeRaised()
        self.m_widget_stack.raiseWidget(controller)

  def close(self, also_delete = False):
    self.emit(PYSIGNAL("quitAvidaPhaseISig"), ())
    return False


  def __del__(self):
    print "pyEduWorkspaceCtrl.__del__(): Not implemented yet"

  def __init__(self, parent = None, name = None, fl = 0):
    pyEduWorkspaceView.__init__(self,parent,name,fl)
    print "pyEduWorkspaceCtrl.__init__(): Not implemented yet"

  # public slot

  def fileNew(self):
    print "pyEduWorkspaceCtrl.fileNew(): Not implemented yet"

  # public slot

  def fileOpen(self):
    workspace_dir = QFileDialog.getExistingDirectory(
                    self.m_session_mdl.m_current_workspace,
                    None,
                    "get existing directory",
                    "Choose a directory",
                    True);
    workspace_dir = str(workspace_dir)              

    if workspace_dir.strip() != "":
      self.m_session_mdl.m_current_workspce = str(workspace_dir)
      self.m_session_mdl.m_current_freezer = os.path.join(self.m_session_mdl.m_current_workspce, "freezer")
      self.m_session_mdl.m_session_mdtr.emit(
        PYSIGNAL("doRefreshFreezerInventorySig"), ())

  # public slot

  def freezerOpenSlot(self):
    freezer_dir = QFileDialog.getExistingDirectory(
                    self.m_session_mdl.m_current_freezer,
                    None,
                    "get existing directory",
                    "Choose a directory",
                    True);
    freezer_dir = str(freezer_dir)
    if freezer_dir.strip() != "":
      self.m_session_mdl.m_current_freezer = str(freezer_dir)
      self.m_session_mdl.m_session_mdtr.emit(
        PYSIGNAL("doRefreshFreezerInventorySig"), ())

  # public slot

  def fileSave(self):
    print "pyEduWorkspaceCtrl.fileSave(): Not implemented yet"

  # public slot

  def fileSaveAs(self):
    print "pyEduWorkspaceCtrl.fileSaveAs(): Not implemented yet"

  # public slot

  def filePrint(self):
    print "pyEduWorkspaceCtrl.filePrint() emitting printGraphSig via self.m_session_mdl.m_session_mdtr"
    self.m_session_mdl.m_session_mdtr.emit(PYSIGNAL("printGraphSig"), ())

  # public slot

  def fileExit(self):
    print "pyEduWorkspaceCtrl.fileExit(): Not implemented yet"

  # public slot

  def editUndo(self):
    print "pyEduWorkspaceCtrl.editUndo(): Not implemented yet"

  # public slot

  def editRedo(self):
    print "pyEduWorkspaceCtrl.editRedo(): Not implemented yet"

  # public slot

  def editCut(self):
    print "pyEduWorkspaceCtrl.editCut(): Not implemented yet"

  # public slot

  def editCopy(self):
    print "pyEduWorkspaceCtrl.editCopy(): Not implemented yet"

  # public slot

  def editPaste(self):
    print "pyEduWorkspaceCtrl.editPaste(): Not implemented yet"

  # public slot

  def editFind(self):
    print "pyEduWorkspaceCtrl.editFind(): Not implemented yet"

  # public slot

  def helpIndex(self):
    print "pyEduWorkspaceCtrl.helpIndex(): Not implemented yet"

  # public slot

  def helpContents(self):
    print "pyEduWorkspaceCtrl.helpContents(): Not implemented yet"

  # public slot

  def helpAbout(self):
    print "pyEduWorkspaceCtrl.helpAbout(): Not implemented yet"

  def next_UpdateActionSlot(self):
    self.m_session_mdl.m_session_mdtr.emit(
      PYSIGNAL("fromLiveCtrlUpdateAvidaSig"), ())

  def startActionSlot(self):
    if self.startStatus:
      self.m_session_mdl.m_session_mdtr.emit(
        PYSIGNAL("fromLiveCtrlStartAvidaSig"), ())
    else:
      self.m_session_mdl.m_session_mdtr.emit(
        PYSIGNAL("fromLiveCtrlPauseAvidaSig"), ())
    
  def setAvidaSlot(self, avida):
    print "pyEduWorkspaceCtrl.setAvidaSlot() ..."
    old_avida = self.m_avida
    self.m_avida = avida
    if old_avida:
      self.disconnect(
        old_avida.m_avida_thread_mdtr, PYSIGNAL("AvidaUpdatedSig"),
        self.avidaUpdatedSlot)
      del old_avida
    if self.m_avida:
      self.connect(
        self.m_avida.m_avida_thread_mdtr, PYSIGNAL("AvidaUpdatedSig"),
        self.avidaUpdatedSlot)

  def avidaUpdatedSlot(self):
    pass
    
  def doPauseAvidaSlot(self):
    self.controlStartAction.text = "Start"
    self.controlStartAction.menuText = "Start"
    self.startStatus = True
    
  def doStartAvidaSlot(self):
    self.controlStartAction.text = "Pause"
    self.controlStartAction.menuText = "Pause"
    self.startStatus = False
    
  def startQuitProcessSlot(self):

    # Be sure that the session is paused before quitting (to reduce confusion
    # if the user decides to save before quiting)

    self.m_session_mdl.m_session_mdtr.emit(PYSIGNAL("doPauseAvidaSig"), ())


    # Check if there unsaved petri dishes if there are ask the user if they 
    # want to save them, just quit the program or cancel the quit.  If there
    # are no unsaved populations just quit.
    # (actually only works with one population will need to expand to
    # two populations in the future)

    if (not self.m_one_population_ctrl.m_session_mdl.saved_full_dish and
        not self.m_one_population_ctrl.m_session_mdl.new_full_dish):
      m_quit_avida_ed = pyQuitDialogCtrl()
      quit_return = m_quit_avida_ed.showDialog()
      if quit_return == m_quit_avida_ed.QuitFlag:
        self.emit(PYSIGNAL("quitAvidaPhaseIISig"), ())
      elif quit_return == m_quit_avida_ed.FreezeQuitFlag:
        self.m_one_population_ctrl.m_session_mdl.m_session_mdtr.emit(PYSIGNAL("freezeDishPhaseISig"), (False, True, ))
    else:
      self.emit(PYSIGNAL("quitAvidaPhaseIISig"), ())

  def RepopulateActionSlot(self):

    # If the user clicks the repopulate button pretend that they double
    # the default empty petri dish from the freezer

    file_name = os.path.join(self.m_session_mdl.m_current_freezer, 
      "default.empty")
    self.m_session_mdl.m_session_mdtr.emit(
      PYSIGNAL("freezerItemDoubleClicked"), (file_name, ))


  def addStatusBarWidgetSlot(self, *args):
    widget = args[0]
    pt = QPoint()
    widget.reparent(self, pt)
    apply(QStatusBar.addWidget,(self.statusBar(),) + args)
  def removeStatusBarWidgetSlot(self, *args):
    apply(QStatusBar.removeWidget,(self.statusBar(),) + args)
  def statusBarMessageSlot(self, *args):
    apply(QStatusBar.message,(self.statusBar(),) + args)
  def statusBarClearSlot(self):
    self.statusBar().clear()
