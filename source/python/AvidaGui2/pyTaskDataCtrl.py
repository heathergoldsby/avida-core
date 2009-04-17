from qt import QListView, QListViewItem, QFont, qApp, QColorGroup, Qt, PYSIGNAL

class ColoredListViewItem(QListViewItem):
    "QListViewItem with colored text"
    def __init__(self, parent, label1, label2, label3):
        QListViewItem.__init__(self, parent, label1, label2, label3)
        self.color = Qt.black
    def __init__(self, parent, label1, label2, label3, color):
        QListViewItem.__init__(self, parent, label1, label2, label3)
        self.color = color
    def setColor(self, color):
        self.color = color
    def paintCell(self, painter, cg, column, width, align):
        painter.save()
        grp = QColorGroup(cg)
        grp.setColor(QColorGroup.Text, self.color)
        QListViewItem.paintCell(self, painter, grp, column, width, align)
        painter.restore()
    def dim(self, flag):
        "Dim the text if true, brighten if false"
        if flag:
            self.setColor(Qt.gray)
        else:
            self.setColor(Qt.black)

class pyTaskDataCtrl(QListView):
    "Control to list tasks accomplished by an organism"
    def __init__(self, parent):
        QListView.__init__(self, parent)

        self.uncompleted_tasks = []

        font = QFont(qApp.font())
        font.setPointSize(9)
        self.setFont(font)

        self.addColumn("")
        self.addColumn("Task")
        self.addColumn("Count")
        self.add_tasks()
        self.show()

    def add_tasks(self):
        self.list_items = []
        # TODO: set from TaskLib
        self.list_items.append(ColoredListViewItem(self, "0", "not", "0",
						   Qt.gray))
        self.list_items.append(ColoredListViewItem(self, "1", "nand", "0",
						   Qt.gray))
        self.list_items.append(ColoredListViewItem(self, "2", "and", "0",
						   Qt.gray))
        self.list_items.append(ColoredListViewItem(self, "3", "orn", "0",
						   Qt.gray))
        self.list_items.append(ColoredListViewItem(self, "4", "or", "0",
						   Qt.gray))
        self.list_items.append(ColoredListViewItem(self, "5", "andn", "0",
						   Qt.gray))
        self.list_items.append(ColoredListViewItem(self, "6", "nor", "0",
						   Qt.gray))
        self.list_items.append(ColoredListViewItem(self, "7", "xor", "0",
						   Qt.gray))
        self.list_items.append(ColoredListViewItem(self, "8", "equ", "0",
						   Qt.gray))
        self.uncompleted_tasks = [0, 1, 2, 3, 4, 5, 6, 7, 8]

    def setReadFn(self, sender, read_fn):
        "Register to receive frame shown signals"
        self.connect(sender, PYSIGNAL("propagated-FrameShownSig"),
                     self.frameShownSlot)

    def frameShownSlot(self, frames, frame_no):
        "Called when a frame is shown"
        if frames is not None and frame_no < frames.m_gestation_time:
            for task in self.uncompleted_tasks:
		count = frames.m_tasks_info[frame_no][task]
		self.list_items[task].setText(2, str(count))
                if count > 0:
                    self.list_items[task].dim(False)
                else:
                    self.list_items[task].dim(True)
                self.list_items[task].repaint()

    def reset():
        "Reset the uncompleted tasks"
        # TODO: also use cTaskLib
        self.uncompleted_tasks = [0, 1, 2, 3, 4, 5, 6, 7, 8]
        for task in self.uncompleted_tasks:
            self.list_items[task].dim(True)
        