# -*- coding: utf-8 -*-

from AvidaCore import cInitFile, cString
from Numeric import *
from pyAvidaStatsInterface import pyAvidaStatsInterface
from pyOneAna_GraphView import pyOneAna_GraphView
from pyButtonListDialog import pyButtonListDialog
from pyGraphCtrl import PrintFilter
from qt import *
from qwt import *
import os.path
import os.path

class pyOneAna_GraphCtrl(pyOneAna_GraphView):

  def __init__(self,parent = None,name = None,fl = 0):
    pyOneAna_GraphView.__init__(self,parent,name,fl)
    self.m_avida_stats_interface = pyAvidaStatsInterface()

  def construct(self, session_mdl):
    self.m_session_mdl = session_mdl
    self.m_avida = None

    self.m_graph_ctrl.construct(self.m_session_mdl)
    self.m_combo_box_1.clear()
    self.m_combo_box_2.clear()
    self.m_combo_box_1.setInsertionPolicy(QComboBox.AtBottom)
    self.m_combo_box_2.setInsertionPolicy(QComboBox.AtBottom)
    self.m_petri_dish_dir_path = ' '
    self.m_petri_dish_dir_exists_flag = False

    self.connect( self.m_session_mdl.m_session_mdtr, 
      PYSIGNAL("freezerItemDoubleClickedOnInOneAnaSig"),
      self.freezerItemDoubleClickedOn)  


    # set up the combo boxes with plot options
    for entry in self.m_avida_stats_interface.m_entries:
      self.m_combo_box_1.insertItem(entry[0])
    for entry in self.m_avida_stats_interface.m_entries:
      self.m_combo_box_2.insertItem(entry[0])

    # set up the plot line color options
    self.m_Red = ['red', Qt.red]
    self.m_Blue = ['blue', Qt.blue]
    self.m_Green = ['green', Qt.green]
    self.m_ThickBlue = ['thick', Qt.blue]
    self.m_Colors = [self.m_Red, self.m_Blue, self.m_Green, self.m_ThickBlue]
    self.m_combo_box_1_color.clear()
    self.m_combo_box_2_color.clear()
    for color in self.m_Colors:
      self.m_combo_box_1_color.insertItem(color[0])
      self.m_combo_box_2_color.insertItem(color[0])

    
    # connect combo boxes to signal
    self.connect(
      self.m_combo_box_1, SIGNAL("activated(int)"), self.modeActivatedSlot)
    self.connect(
      self.m_combo_box_2, SIGNAL("activated(int)"), self.modeActivatedSlot)
    self.connect(
      self.m_combo_box_1_color, SIGNAL("activated(int)"), self.modeActivatedSlot)
    self.connect(
      self.m_combo_box_2_color, SIGNAL("activated(int)"), self.modeActivatedSlot)
    self.connect( self.m_session_mdl.m_session_mdtr, PYSIGNAL("freezerItemDroppedInOneAnalyzeSig"),
      self.petriDropped)  
    self.m_graph_ctrl.setAxisTitle(QwtPlot.xBottom, "Time (updates)")
    self.m_graph_ctrl.setAxisAutoScale(QwtPlot.xBottom)

    # Start the left with second graph mode -- "Average Fitness"
    self.m_combo_box_1.setCurrentItem(2)

    # Start the right with zeroth mode -- "None"
    self.m_combo_box_2.setCurrentItem(0)

    # Start the left with default of red
    self.m_combo_box_1_color.setCurrentItem(0)
    self.m_combo_box_2_color.setCurrentItem(2)

    self.modeActivatedSlot() 

  def load(self, filename, colx, coly):
    
    init_file = cInitFile(cString(os.path.join(str(self.m_petri_dish_dir_path), filename)))
    init_file.Load()
    init_file.Compress()

    x_array = zeros(init_file.GetNumLines(), Float)
    y_array = zeros(init_file.GetNumLines(), Float)

    for line_id in xrange(init_file.GetNumLines()):
      line = init_file.GetLine(line_id)
      x_array[line_id] = line.GetWord(colx - 1).AsDouble()
      y_array[line_id] = line.GetWord(coly - 1).AsDouble()
    return x_array, y_array
  
  def modeActivatedSlot(self, index = None): #note: index is not used
    self.m_graph_ctrl.clear()
   
    #check to see if we have a valid directory path to analyze
    if self.m_petri_dish_dir_exists_flag == False:
      return

    if self.m_combo_box_1.currentItem() or self.m_combo_box_2.currentItem():

      if self.m_combo_box_1.currentItem():
        index_1 = self.m_combo_box_1.currentItem()

        #check to see if the file exists
        if os.path.isfile(os.path.join(str(self.m_petri_dish_dir_path), self.m_avida_stats_interface.m_entries[index_1][1])):
          pass
        else:
          print "error: there is no data file in the directory to load from"
          self.m_graph_ctrl.setTitle(self.m_avida_stats_interface.m_entries[0][0])
          self.m_graph_ctrl.setAxisTitle(QwtPlot.yLeft, self.m_avida_stats_interface.m_entries[0][0])
          self.m_graph_ctrl.replot()
          return
        self.m_graph_ctrl.setAxisTitle(QwtPlot.yLeft, self.m_avida_stats_interface.m_entries[index_1][0])
        self.m_graph_ctrl.enableYLeftAxis(True)
        self.m_graph_ctrl.setAxisAutoScale(QwtPlot.yLeft)
        print "index_1[2] is"
        print self.m_avida_stats_interface.m_entries[index_1][2]
        self.m_curve_1_arrays = self.load(
            self.m_avida_stats_interface.m_entries[index_1][1],
            1,
            self.m_avida_stats_interface.m_entries[index_1][2]
        )

        self.m_graph_ctrl.m_curve_1 = self.m_graph_ctrl.insertCurve(self.m_avida_stats_interface.m_entries[index_1][0])
        self.m_graph_ctrl.setCurveData(self.m_graph_ctrl.m_curve_1, self.m_curve_1_arrays[0], self.m_curve_1_arrays[1])
        self.m_graph_ctrl.setCurvePen(self.m_graph_ctrl.m_curve_1, QPen(self.m_Colors[self.m_combo_box_1_color.currentItem()][1]))  
        self.m_graph_ctrl.setCurveYAxis(self.m_graph_ctrl.m_curve_1, QwtPlot.yLeft)
        if not self.m_combo_box_2.currentItem():
          self.m_graph_ctrl.enableYRightAxis(False)
          self.m_graph_ctrl.setTitle(self.m_avida_stats_interface.m_entries[index_1][0])
      else:
        self.m_graph_ctrl.enableYLeftAxis(False)


      if self.m_combo_box_2.currentItem():
        index_2 = self.m_combo_box_2.currentItem()
        self.m_graph_ctrl.setAxisTitle(QwtPlot.yRight, self.m_avida_stats_interface.m_entries[index_2][0])
        self.m_graph_ctrl.enableYRightAxis(True)      
        self.m_graph_ctrl.setAxisAutoScale(QwtPlot.yRight)
        self.m_curve_2_arrays = self.load(
            self.m_avida_stats_interface.m_entries[index_2][1],
            1,
            self.m_avida_stats_interface.m_entries[index_2][2]
        )

        self.m_graph_ctrl.m_curve_2 = self.m_graph_ctrl.insertCurve(self.m_avida_stats_interface.m_entries[index_2][0])
        self.m_graph_ctrl.setCurveData(self.m_graph_ctrl.m_curve_2, self.m_curve_2_arrays[0], self.m_curve_2_arrays[1])
        if self.m_Colors[self.m_combo_box_2_color.currentItem()][0] is 'thick':
          self.m_graph_ctrl.setCurvePen(self.m_graph_ctrl.m_curve_2, QPen(self.m_Colors[self.m_combo_box_2_color.currentItem()][1],3))
        else:
          self.m_graph_ctrl.setCurvePen(self.m_graph_ctrl.m_curve_2, QPen(self.m_Colors[self.m_combo_box_2_color.currentItem()][1]))
        self.m_graph_ctrl.setCurveYAxis(self.m_graph_ctrl.m_curve_2, QwtPlot.yRight)
        if not self.m_combo_box_1.currentItem():
          self.m_graph_ctrl.setTitle(self.m_avida_stats_interface.m_entries[index_2][0])


      self.m_graph_ctrl.setAxisAutoScale(QwtPlot.xBottom)

      if self.m_combo_box_1.currentItem() and self.m_combo_box_2.currentItem():    
        self.m_graph_ctrl.setTitle(self.m_avida_stats_interface.m_entries[self.m_combo_box_1.currentItem()][0]+ ' (' + self.m_Colors[self.m_combo_box_1_color.currentItem()][0] \
        + ') and ' + self.m_avida_stats_interface.m_entries[self.m_combo_box_2.currentItem()][0]+ ' (' +  self.m_Colors[self.m_combo_box_2_color.currentItem()][0] +')')
        bounding_rect_1 = self.m_graph_ctrl.curve(self.m_graph_ctrl.m_curve_1).boundingRect()
        bounding_rect_2 = self.m_graph_ctrl.curve(self.m_graph_ctrl.m_curve_2).boundingRect()
        bounding_rect = bounding_rect_1.unite(bounding_rect_2)
      elif self.m_combo_box_1.currentItem():
        bounding_rect = self.m_graph_ctrl.curve(self.m_graph_ctrl.m_curve_1).boundingRect()
      else:
        bounding_rect = self.m_graph_ctrl.curve(self.m_graph_ctrl.m_curve_2).boundingRect()

      self.m_graph_ctrl.m_zoomer.setZoomBase(bounding_rect)
  
    else:   # goes with '   if self.m_combo_box_1.currentItem() or self.m_combo_box_2.currentItem():'
       self.m_graph_ctrl.setTitle(self.m_avida_stats_interface.m_entries[0][0])
       self.m_graph_ctrl.setAxisTitle(QwtPlot.yLeft, self.m_avida_stats_interface.m_entries[0][0])


    self.m_graph_ctrl.replot()
      
  def printGraphSlot(self):
    printer = QPrinter()
    if printer.setup():
      filter = PrintFilter()
      if (QPrinter.GrayScale == printer.colorMode()):
        filter.setOptions(QwtPlotPrintFilter.PrintAll & ~QwtPlotPrintFilter.PrintCanvasBackground)
      self.m_graph_ctrl.printPlot(printer, filter)

  def exportSlot(self):
    "Export analysis data to a file"
    dialog_caption = "Export Analysis"
    fd = QFileDialog.getSaveFileName("", "CSV (Excel compatible) (*.csv)", None,
                                     "export as", dialog_caption)
    filename = str(fd)
    if (filename[-4:].lower() != ".csv"):
      filename += ".csv"

    checks = []
    # dictionary indexed by stat name so we can lookup stats to export
    stats = {}
    stat_cnt = 0
    for stat in self.m_avida_stats_interface.m_entries:
      # Note: this relies on labeling dummy stats with None
      if stat[0] != "None":
        stats[stat[0]] = stat_cnt
        checks.append(stat[0])
      stat_cnt += 1

    dialog = pyButtonListDialog(dialog_caption, "Choose stats to export",
                                checks, True)
    # enable checkboxes
    for button in dialog.buttons:
      button.setOn(True)

    res = dialog.showDialog()
    if res == []:
      return

    data = {}
    if self.m_combo_box_1.currentItem():
      # Load stats for selected exports
      # TODO: more efficient loading
      for item in res:
        idx = stats[item]
        label1 = self.m_avida_stats_interface.m_entries[idx][0]
        data[item] = self.load(self.m_avida_stats_interface.m_entries[idx][1],
                               1,
                               self.m_avida_stats_interface.m_entries[idx][2])

      out_file = open(filename, 'w')
      out_file.write("Update,%s\n" % (",".join(res)))
      # TODO: get it working with zip
      #print zip([data[elem][1][i] for elem in res])        
      num_updates = len(data[res[0]][0])
      for i in range(num_updates):
        out_file.write("%d,%s\n"
                       % (i, ",".join([str(data[elem][1][i]) for elem in res])))
      out_file.close()

  def petriDropped(self, e): 
      # a check in pyOneAnalyzeCtrl.py makes sure this is a valid path
      self.m_petri_dish_dir_exists_flag = True
      # Try to decode to the data you understand...
      freezer_item_name = QString()
      if ( QTextDrag.decode( e, freezer_item_name ) ) :
        freezer_item_name = str(e.encodedData("text/plain"))
        self.m_petri_dish_dir_path = freezer_item_name
        self.modeActivatedSlot()
        return

      pm = QPixmap()
      if ( QImageDrag.decode( e, pm ) ) :
        print "it was a pixmap"
        return

      # QStrList strings
      #strings = QStrList()
      strings = []
      if ( QUriDrag.decode( e, strings ) ) :
        print "it was a uri"
        m = QString("Full URLs:\n")
        for u in strings:
            m = m + "   " + u + '\n'
        # QStringList files
        files = []
        if ( QUriDrag.decodeLocalFiles( e, files ) ) :
            print "it was a file"
            m += "Files:\n"
            # for (QStringList.Iterator i=files.begin() i!=files.end() ++i)
            for i in files:
                m = m + "   " + i + '\n'
        return

      decode_str = decode( e ) 
      if decode_str:
        print " in if decode_str"

  def freezerItemDoubleClickedOn(self, freezer_item_name): 
    # a check in pyOneAnalyzeCtrl.py makes sure this is a valid path
    self.m_petri_dish_dir_exists_flag = True
    self.m_petri_dish_dir_path = os.path.split(freezer_item_name)[0]
    self.modeActivatedSlot()
     

