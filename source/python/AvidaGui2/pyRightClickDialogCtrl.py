# -*- coding: utf-8 -*-

from descr import *

from qt import *
from pyRightClickDialogView import pyRightClickDialogView
import shutil, os, os.path

# class to pop up a dialog box when the user quits
# to ask for what to freeze and to
# return the name of a file to save information to be frozen

class pyRightClickDialogCtrl (pyRightClickDialogView):
  def __init__(self, item_name, file_name):
    pyRightClickDialogView.__init__(self)
    self.file_name = str(file_name)
    self.item_name = str(item_name)
    self.NewNameLineEdit.setText(self.item_name)
    self.connect(self.ConfirmPushButton, SIGNAL("clicked()"), 
                 self.DoneDialogSlot)

    (self.file_dir, self.file_core_name)  = os.path.split(self.file_name)
    self.file_ext = self.file_core_name[self.file_core_name.rfind('.'):]
    if (self.file_ext == '.organism'):
      self.setCaption(self.file_ext.lstrip('.') + ": " + self.item_name)
      self.OpenRadioButton.setDisabled(True)
      self.OpenRadioButton.setChecked(False)
      self.SaveAsRadioButton.setChecked(True)
    else:
      self.setCaption(self.file_ext.lstrip('.') + " dish: " + self.item_name)
    self.DeleteFlag = 1
    self.RenameFlag = 2
    self.SaveAsFlag = 3
    self.OpenFlag = 4
    
  def DoneDialogSlot(self):
    if self.DeleteRadioButton.isChecked():
      self.done(self.DeleteFlag)
    elif self.RenameRadioButton.isChecked():
      self.done(self.RenameFlag)
    elif self.SaveAsRadioButton.isChecked():
      self.done(self.SaveAsFlag)
    elif self.OpenRadioButton.isChecked():
      self.done(self.OpenFlag)

  def showDialog(self):
    self.change = False
    self.open_obj = False
    self.exec_loop()
    dialog_result = self.result()
    new_name_core = str(self.NewNameLineEdit.text())
    new_name_core = new_name_core.strip()

    # if the user chose to delete file or directory

    if dialog_result == self.DeleteFlag:
      self.change = True
      if (self.file_ext == '.full'):
        tmp_file_list = os.listdir(self.file_name)
        for file in tmp_file_list:
          os.remove(os.path.join(self.file_name,file))
        os.removedirs(self.file_name)
      else:
        os.remove(self.file_name)

    # if the user chose to rename the file/directory and they gave an actual
    # check if that file already exists if it does not rename the file

    elif dialog_result == self.RenameFlag and new_name_core != "":
      new_name = os.path.join(self.file_dir, new_name_core + self.file_ext)
      if (os.path.exists(new_name) == False):
        os.rename(self.file_name, new_name)
        self.change = True

    elif dialog_result == self.SaveAsFlag:
      # file_dialog = QFileDialog (os.path.join(self.file_dir, '..'), 
      #   '.junk1234junk', self, 'Export', False)
      # file_dialog.setCaption('Export ' + self.file_ext.lstrip('.') + " " + 
      #   self.item_name + ' to:')
      # file_dialog.setMode(QFileDialog.DirectoryOnly)
      # # file_dialog.setSelection (self.file_core_name)
      # file_dialog.show()
      # file_dialog.exec_loop()
      # if file_dialog.result() == True:
      #   export_file_name = os.path.join(str(file_dialog.selectedFile()),
      #    self.file_core_name)
      #   if (self.file_ext == '.full'):
      #     shutil.copytree(self.file_name, str(export_file_name))
      #   else:
      #     shutil.copyfile(self.file_name, str(export_file_name))

      # If the directory has not been chosen self.file_dir is not
      # correct (at least on the Mac).  It is a relative path where
      # QFileDialog.getSaveFileName seems to require an absolut path

      abs_dir = os.path.abspath(self.file_dir);
      if (os.path.exists(abs_dir) == False):
        initial_dir = os.path.expanduser("~")
        if os.path.exists(os.path.join(initial_dir,"Documents")):
          initial_dir = os.path.join(initial_dir,"Documents")
        elif os.path.exists(os.path.join(initial_dir,"My Documents")):
          initial_dir = os.path.join(initial_dir,"My Documents")
      else:
        initial_dir = abs_dir
      no_ext_name, ext = os.path.splitext(self.file_core_name)
      initial_file_name = os.path.join(initial_dir,no_ext_name + ".aex")

      export_file_name = QFileDialog.getSaveFileName(
        initial_file_name,
        "Avida_ED export (*.aex);;",
        None,
        "Export Item",
        "Export dish or organism")
      export_file = open(str(export_file_name), "w")
      if (self.file_ext == '.full'):
        files_in_full = os.listdir(self.file_name)
        for indiv_file_name in files_in_full:

          # Read each file, write the name and content of the file
          # into the export file

          if (not indiv_file_name.startswith('.')):
            individual_file = open(os.path.join(self.file_name,indiv_file_name), "r")
            export_file.write("*File: " + self.file_ext + ": " + indiv_file_name + "\n")
            lines = individual_file.readlines()
            for line in lines:
              export_file.write(line)
            individual_file.close()
      else:
        individual_file = open(self.file_name, "r")
        export_file.write("*File: " + self.file_ext + "\n")
        lines = individual_file.readlines()
        for line in lines:
          export_file.write(line)
        individual_file.close()
      export_file.close()

    elif dialog_result == self.OpenFlag:
      self.open_obj = True

    return(self.change, self.open_obj)
 
