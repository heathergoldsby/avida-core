# -*- coding: utf-8 -*-

from qt import *
from pyGradientScaleView import pyGradientScaleView


class pyGradientScaleCtrl(pyGradientScaleView):

  def __init__(self,parent = None,name = None,fl = 0):
    pyGradientScaleView.__init__(self,parent,name,fl)
    QToolTip.add(self, "Dynamic scale for current variable (scale changes as maximum value in population increases or decreases)")



  def construct(self, session_mdl):
    self.m_session_mdl = session_mdl
    self.m_avida = None
    self.m_current_map_mode_name = None
    self.connect(
      self.m_session_mdl.m_session_mdtr, PYSIGNAL("setAvidaSig"),
      self.setAvidaSlot)
    self.connect(self.m_session_mdl.m_session_mdtr,
       PYSIGNAL("mapModeChangedSig"), self.setMapModeSlot)


  def setAvidaSlot(self, avida):
    print "pyGradientScaleCtrl.setAvidaSlot() ..."
    old_avida = self.m_avida
    self.m_avida = avida
    if(old_avida):
      print "pyGradientScaleCtrl.setAvidaSlot() disconnecting old_avida ..."
      self.disconnect(
        self.m_avida.m_avida_thread_mdtr, PYSIGNAL("AvidaUpdatedSig"),
        self.avidaUpdatedSlot)
      del old_avida
    if(self.m_avida):
      print "pyGradientScaleCtrl.setAvidaSlot() connecting self.m_avida ..."
      self.connect(
        self.m_avida.m_avida_thread_mdtr, PYSIGNAL("AvidaUpdatedSig"),
        self.avidaUpdatedSlot)

  def setMapModeSlot(self,index):
    self.m_current_map_mode_name = index

  def avidaUpdatedSlot(self):
    pass

  def destruct(self):
    print "*** called pyGradientScaleCtrl.py:destruct ***"
    self.m_avida = None
    self.disconnect(
      self.m_session_mdl.m_session_mdtr, PYSIGNAL("setAvidaSig"),
      self.setAvidaSlot)
    self.m_session_mdl = None
  

