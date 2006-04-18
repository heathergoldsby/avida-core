# -*- coding: utf-8 -*-

from qt import *
from pyFreezeDialogView import pyFreezeDialogView
import shutil, os, os.path

# class to pop up a dialog box to ask for what to freeze and to
# return the name of a file to save information to be frozen

class pyFreezeDialogCtrl (pyFreezeDialogView):
  def __init__(self):
    pyFreezeDialogView.__init__(self)
    
  def showDialog(self, freezer_dir = None, freeze_empty_only_flag = False):
    found_valid_name = False
    dialog_result = 1
    if freeze_empty_only_flag == True:
      self.FullRadioButton.setEnabled(False)
    else:
      self.FullRadioButton.setEnabled(True)
    while (found_valid_name == False and dialog_result > 0):
      self.exec_loop()
      dialog_result = self.result()
      tmp_name = str(self.FileNameLineEdit.text())
      if dialog_result == 0:
        return ''
      elif (tmp_name == ''):
        found_valid_name = False
        self.MainMessageTextLabel.setText("Enter a Non-Blank Name of Item to Freeze")
      else:
        if self.EmptyRadioButton.isChecked():
          if (tmp_name.endswith(".empty") == False):
            tmp_name = tmp_name + ".empty"
          tmp_name = os.path.join(freezer_dir, tmp_name)
          if os.path.exists(tmp_name):
            found_valid_name = False
            self.MainMessageTextLabel.setText("Petri Dish Exists, Please Enter a Different Name")
          else:
            found_valid_name = True
            return tmp_name
        else:
          if (tmp_name.endswith(".full") == False):
            tmp_name = tmp_name + ".full"
          tmp_name = os.path.join(freezer_dir, tmp_name)
          if os.path.exists(tmp_name):
            found_valid_name = False
            self.MainMessageTextLabel.setText("Petri Dish Exists, Please Enter a Different Name")
          else:
            found_valid_name = True
            return tmp_name
      
  def isEmpty(self):
    if self.EmptyRadioButton.isChecked():
      return True
    else:
      return False

