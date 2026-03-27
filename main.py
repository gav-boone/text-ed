import sys
import tkinter as tk
from tkinter.filedialog import asksaveasfilename
from tkinter import ttk
import utils

FONTS = [
    "Courier",
    "Helvetica",
    "Arial",
    "Times New Roman",
]

root = tk.Tk(className="text-ed")

root.title("text-ed")
root.resizable(True, True)
root.minsize(300, 300)

text = tk.Text(root)

button = tk.Button(root, text="Save", command=saveas)

font_selector = ttk.Combobox(root, text="Font", values=FONTS, state="readonly")
font_selector.bind(
    "<<ComboboxSelected>>", lambda x: utils.set_font(font_selector.get())
)

button.grid()
font_selector.grid()
text.grid()

root.mainloop()
