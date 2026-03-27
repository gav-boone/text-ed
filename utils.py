import sys
import tkinter as tk
from tkinter.filedialog import asksaveasfilename
from tkinter import ttk

def set_font(text: tk.Text, fontname: str) -> None:
    text.config(font=(fontname, 12, ""))


def saveas() -> None:
    global text
    t = text.get("1.0", "end-1c")
    savelocation = asksaveasfilename()
    file = open(savelocation, "wt")
    file.write(t)
    file.close()