#!/usr/bin/env python
import sys

from PyQt4.QtCore import *
from PyQt4.QtGui import *


class Window(QMainWindow):
    def __init__(self):
        super(Window, self).__init__()
        self.createMenu(self.menuBar(), "File", "New", "Open", "Quit")
        self.createMenu(self.menuBar(), "Help", "About")

        centralWidget = QWidget()
        self.setCentralWidget(centralWidget)

        layout = QVBoxLayout(centralWidget)
        layout.addWidget(self.createEditWidget())
        layout.addWidget(self.createEditWidget())
        layout.addWidget(self.createEditWidget())

    def createEditWidget(self):
        widget = QWidget()
        menuBar = QMenuBar(widget)
        self.createMenu(menuBar, "Edit", "Cut", "Copy", "Paste")
        edit = QTextEdit()
        layout = QVBoxLayout(widget)
        layout.addWidget(menuBar)
        layout.addWidget(edit)
        return widget

    def createMenu(self, menuBar, name, *items):
        menu = menuBar.addMenu(name)
        for item in items:
            menu.addAction(item)

def main():
    app = QApplication(sys.argv)
    window = Window()

    window.show()
    app.exec_()
    return 0

if __name__ == "__main__":
    sys.exit(main())
# vi: ts=4 sw=4 et
