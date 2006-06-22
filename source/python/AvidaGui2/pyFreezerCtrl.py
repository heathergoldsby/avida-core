# -*- coding: utf-8 -*-

from descr import *

import os
from qt import *
from pyFreezerView import *
from pyReadFreezer import pyReadFreezer
from pyWriteToFreezer import pyWriteToFreezer
from pyFreezeOrganismCtrl import pyFreezeOrganismCtrl
from pyRightClickDialogCtrl import pyRightClickDialogCtrl
import os.path

class pyFreezerListView(QListView):
  def __init__(self, *args):
    descr()
    apply(QListView.__init__,(self,) + args)
    self.setAcceptDrops( True )
    self.viewport().setAcceptDrops( True )
  def construct(self, session_mdl):
    descr()
    self.m_session_mdl = session_mdl

  def contentsDropEvent(self, e):
    descr(e)
    freezer_item_name = QString()
    if e.source() is self:
      return

    # freezer_item_name is a string...the file name 
 
    print type(e)

    if ( QIconDrag.canDecode(e)):
      print "BDB -- can decode icon"
      format = QDropEvent.format(e, 0)
      print "format = " + str(format)
      print type(e.encodedData(format))
      print "--------------------"
      print dir(e.encodedData(format))
      print str(e.encodedData(format))
      print "BDB -- " + str(e.encodedData(format).data())
    if ( QTextDrag.decode( e, freezer_item_name ) ) :
      print "BDB:pyFreezerListView freezer_item_name = ", str(freezer_item_name)
      if freezer_item_name[:9] == 'organism.':
        freezer_item_name = freezer_item_name[9:] 
        self.FreezeOrganism(freezer_item_name)
      else:
        pass
    
  def FreezeOrganism(self, freezer_item_name):
    tmp_dict = {1:freezer_item_name}
    pop_up_organism_file_name = pyFreezeOrganismCtrl()
    file_name = pop_up_organism_file_name.showDialog(self.m_session_mdl)

    file_name_len = len(file_name.rstrip())
    if (file_name_len > 0):
      freezer_file = pyWriteToFreezer(tmp_dict, file_name)
    
    self.m_session_mdl.m_session_mdtr.emit(
      PYSIGNAL("doRefreshFreezerInventorySig"), ())

class pyEmptyDishListViewItem(QListViewItem):
  def __init__(self, *args):
    apply(QListViewItem.__init__,(self,) + args)
    descr()

  def dragEntered(self):
    descr()

  def dragLeft(self):
    descr()

  def dropped(self, e):
    descr(e)

class pyFullDishListViewItem(QListViewItem):
  def __init__(self, *args):
    apply(QListViewItem.__init__,(self,) + args)
    descr()

  def dragEntered(self):
    descr()

  def dragLeft(self):
    descr()

  def dropped(self, e):
    descr(e)

class pyOrganismListViewItem(QListViewItem):
  def __init__(self, *args):
    apply(QListViewItem.__init__,(self,) + args)
    descr()

  def dragEntered(self):
    self.setDropEnabled(True)
    descr()

  def dragLeft(self):
    descr()

  def dropped(self, e):
    descr(e)

#class pyFreezerCtrl(pyFreezerView):
class pyFreezerCtrl(QWidget):

  def __init__(self,parent = None,name = None,fl = 0):
    QWidget.__init__(self,parent,name,fl)

    self.image0 = QPixmap()
    self.image0.loadFromData(image0_data,"PNG")
    self.image1 = QPixmap()
    self.image1.loadFromData(image1_data,"PNG")
    self.image2 = QPixmap()
    self.image2.loadFromData(image2_data,"PNG")
    if not name:
        self.setName("pyFreezerView")

    self.setSizePolicy(QSizePolicy(QSizePolicy.Preferred,QSizePolicy.Preferred,0,0,self.sizePolicy().hasHeightForWidth()))

    pyFreezerViewLayout = QVBoxLayout(self,0,6,"pyFreezerViewLayout")

    self.m_list_view = pyFreezerListView(self,"m_list_view")
    self.m_list_view.addColumn("Freezer")
    self.m_list_view.header().setClickEnabled(0,self.m_list_view.header().count() - 1)
    self.m_list_view.header().setResizeEnabled(0,self.m_list_view.header().count() - 1)
    self.m_list_view.setSizePolicy(QSizePolicy(QSizePolicy.Preferred,QSizePolicy.Preferred,0,0,self.m_list_view.sizePolicy().hasHeightForWidth()))
    m_list_view_font = QFont(self.m_list_view.font())
    m_list_view_font.setPointSize(9)
    self.m_list_view.setFont(m_list_view_font)
    self.m_list_view.setAcceptDrops(1)
    self.m_list_view.setFrameShape(QListView.StyledPanel)
    self.m_list_view.setFrameShadow(QListView.Sunken)
    self.m_list_view.setHScrollBarMode(QListView.AlwaysOff)
    self.m_list_view.setAllColumnsShowFocus(1)
    self.m_list_view.setRootIsDecorated(1)
    self.m_list_view.setResizeMode(QListView.AllColumns)
    pyFreezerViewLayout.addWidget(self.m_list_view)

    self.languageChange()

    self.resize(QSize(232,383).expandedTo(self.minimumSizeHint()))
    self.clearWState(Qt.WState_Polished)

    #pyFreezerView.__init__(self,parent,name,fl)
    self.m_list_view.setSelectionMode(QListView.Extended)
    self.connect(self.m_list_view, 
      SIGNAL("doubleClicked(QListViewItem*, const QPoint &, int)"),
      self.clicked_itemSlot)
    self.connect(self.m_list_view, 
      SIGNAL("pressed(QListViewItem*, const QPoint &, int )"),
      self.pressed_itemSlot)
    self.connect(self.m_list_view, 
      SIGNAL("rightButtonPressed(QListViewItem*, const QPoint &, int )"),
      self.right_clicked_itemSlot)
    self.setAcceptDrops(1)
    QToolTip.add(self,"<p>Storage for environment settings, full populations and individual organisms</p>")
    descr()

  # This shadows pyFreezerView.languageChange, which was generated by
  # pyuic from pyFreezerView.ui. @kgn
  def languageChange(self):
    self.setCaption("pyFreezerView")
    self.m_list_view.header().setLabel(0,"Freezer")
    self.m_list_view.clear()
    item = pyEmptyDishListViewItem(self.m_list_view,None)
    item.setText(0," Empty Petri Dishes")
    item.setPixmap(0,self.image0)

    item = pyFullDishListViewItem(self.m_list_view,item)
    item.setText(0," Full Petri Dishes")
    item.setPixmap(0,self.image1)

    item = pyOrganismListViewItem(self.m_list_view,item)
    item.setText(0," Organisms")
    item.setPixmap(0,self.image2)

  def construct(self, session_mdl):
    descr()
    self.m_session_mdl = session_mdl
    self.m_list_view.construct(session_mdl)
    self.connect(self.m_session_mdl.m_session_mdtr,
      PYSIGNAL("doRefreshFreezerInventorySig"),
      self.createFreezerIndexSlot)
    self.createFreezerIndexSlot()
    self.m_list_view.setAcceptDrops(True)
    self.m_empty_item.setDropEnabled(True)
    self.m_full_item.setDropEnabled(True)
    self.m_organism_item.setDropEnabled(True)


  def createFreezerIndexSlot(self):
    descr()

    # Freezer is hardcoded to list in order :
    #   Empty Dishes, Full Dishes, Organisms

    self.m_empty_item = self.m_list_view.firstChild()
    while self.m_empty_item.firstChild():
      tmp_child = self.m_empty_item.firstChild()
      self.m_empty_item.takeItem(tmp_child)
      del (tmp_child)
    self.m_full_item = self.m_empty_item.nextSibling()
    while self.m_full_item.firstChild():
      tmp_child = self.m_full_item.firstChild()
      self.m_full_item.takeItem(tmp_child)
      del (tmp_child)
    self.m_organism_item = self.m_full_item.nextSibling()
    while self.m_organism_item.firstChild():
      tmp_child = self.m_organism_item.firstChild()
      self.m_organism_item.takeItem(tmp_child)
      del (tmp_child)
    if os.path.exists(self.m_session_mdl.m_current_freezer) == False:
      os.mkdir(self.m_session_mdl.m_current_freezer)
    freezer_dir =  os.listdir(self.m_session_mdl.m_current_freezer)
    for file in freezer_dir:
      if file.endswith(".empty"):
        dish_name = file[:-6]
        tmp_item = QListViewItem(self.m_empty_item)
        tmp_item.setText(0,dish_name)
      if file.endswith(".full"):
        dish_name = file[:-5]
        tmp_item = QListViewItem(self.m_full_item)
        tmp_item.setText(0,dish_name)
      if file.endswith(".organism"):
        organism_name = file[:-9]
        tmp_item = QListViewItem(self.m_organism_item)
        tmp_item.setText(0,organism_name)

  # if mouse is pressed on list item prepare its info to be dragged        

  def pressed_itemSlot(self, item):

    if item != None and item.depth() > 0:
      top_level = item
      while top_level.parent():
        top_level = top_level.parent()

      # Rebuild the file name

      if str(top_level.text(0)).startswith(" Empty Petri"):
        file_name = str(item.text(0)) + ".empty"
      elif str(top_level.text(0)).startswith(" Full Petri"):
        file_name = str(item.text(0)) + ".full"
      elif str(top_level.text(0)).startswith(" Organism"):
        file_name = str(item.text(0)) + ".organism"
      file_name = os.path.join(self.m_session_mdl.m_current_freezer, file_name)

      dragHolder = self.itemDrag( file_name, self )
      dragHolder.dragCopy()

  # if freezer item is clicked read file/directory assocatied with item

  def clicked_itemSlot(self, item):
   
    # check that the item is not at the top level 
    
    if item != None and item.depth() > 0:
      top_level = item
      while top_level.parent():
        top_level = top_level.parent()

      # Rebuild the file name

      if str(top_level.text(0)).startswith(" Empty Petri"):
        file_name = str(item.text(0)) + ".empty"
      elif str(top_level.text(0)).startswith(" Full Petri"):
        file_name = str(item.text(0)) + ".full"
        file_name = os.path.join(file_name, "petri_dish")
      elif str(top_level.text(0)).startswith(" Organism"):
        file_name = str(item.text(0)) + ".organism"
      file_name = os.path.join(self.m_session_mdl.m_current_freezer, file_name)
      thawed_item = pyReadFreezer(file_name)
      self.m_session_mdl.m_session_mdtr.emit(PYSIGNAL("doDefrostDishSig"),
        (item.text(0), thawed_item,))
      self.m_session_mdl.m_session_mdtr.emit(PYSIGNAL("freezerItemDoubleClicked"),
        (file_name,))

  # if item is right clicked pull up services menu

  def right_clicked_itemSlot(self, item):

    # check that the item is not at the top level

    if item != None and item.depth() > 0:
      top_level = item
      while top_level.parent():
        top_level = top_level.parent()

      # Rebuild the file name

      if str(top_level.text(0)).startswith(" Empty Petri"):
        file_name = str(item.text(0)) + ".empty"
      elif str(top_level.text(0)).startswith(" Full Petri"):
        file_name = str(item.text(0)) + ".full"
      elif str(top_level.text(0)).startswith(" Organism"):
        file_name = str(item.text(0)) + ".organism"
      file_name = os.path.join(self.m_session_mdl.m_current_freezer, file_name)

      m_right_click_menu = pyRightClickDialogCtrl(item.text(0), file_name)
      (file_list_change, open_obj)  = m_right_click_menu.showDialog()
      if file_list_change == True:
        self.m_session_mdl.m_session_mdtr.emit(
          PYSIGNAL("doRefreshFreezerInventorySig"), ())
      if open_obj == True:
        self.clicked_itemSlot(item)

  class itemDrag(QTextDrag):
    def __init__(self, item_name, parent=None, name=None):
      QStoredDrag.__init__(self, 'item name (QString)', parent, name)
      self.setText(item_name)
      descr(item_name)

  def dragMoveEvent( self, e ):
    descr(e)

  def dragEnterEvent( self, e ):
    descr(e)
    e.acceptAction(True)
    if e.isAccepted():
      descr("isAccepted.")
    else:
      descr("not isAccepted.")

  def dragLeaveEvent( self, e ):
    descr(e)

  def dropEvent( self, e):
    descr(e)
    freezer_item_name = QString()
    if e.source() is self:
      return
    if ( QTextDrag.decode( e, freezer_item_name ) ) : #freezer_item_name is a string...the file name 
      if freezer_item_name[:9] == 'organism.':
        freezer_item_name = freezer_item_name[9:] 
        self.FreezeOrganism(freezer_item_name)
      else:
        pass 
    
  def FreezeOrganism(self, freezer_item_name):
    tmp_dict = {1:freezer_item_name}
    pop_up_organism_file_name = pyFreezeOrganismCtrl()
    file_name = pop_up_organism_file_name.showDialog(self.m_session_mdl.m_current_freezer)

    file_name_len = len(file_name.rstrip())
    if (file_name_len > 0):
      freezer_file = pyWriteToFreezer(tmp_dict, file_name)
    
    self.m_session_mdl.m_session_mdtr.emit(
      PYSIGNAL("doRefreshFreezerInventorySig"), ())
