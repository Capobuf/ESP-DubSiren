"""Tk viewport for panels larger than the available window or DPI setting."""
import tkinter as tk
from tkinter import ttk


class ScrollablePanel(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.canvas = tk.Canvas(self, highlightthickness=0, width=700, height=500)
        vertical = ttk.Scrollbar(self, orient="vertical", command=self.canvas.yview)
        horizontal = ttk.Scrollbar(self, orient="horizontal", command=self.canvas.xview)
        self.canvas.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        vertical.grid(row=0, column=1, sticky="ns")
        horizontal.grid(row=1, column=0, sticky="ew")
        self.rowconfigure(0, weight=1)
        self.columnconfigure(0, weight=1)
        self.content = ttk.Frame(self.canvas, padding=6)
        self.window = self.canvas.create_window(0, 0, window=self.content, anchor="nw")
        self.content.bind("<Configure>", self._resize)
        self.canvas.bind("<Configure>", self._resize)
        parent.winfo_toplevel().bind("<FocusIn>", self._reveal_focus, add="+")

    def _resize(self, _event=None):
        width = max(self.canvas.winfo_width(), self.content.winfo_reqwidth())
        self.canvas.itemconfigure(self.window, width=width)
        self.canvas.configure(scrollregion=self.canvas.bbox("all"))

    def _reveal_focus(self, event):
        widget = event.widget
        if not str(widget).startswith(str(self.content) + "."):
            return
        self.update_idletasks()
        for axis in ("x", "y"):
            position = getattr(widget, f"winfo_root{axis}")() - getattr(self.content, f"winfo_root{axis}")()
            size_name = "width" if axis == "x" else "height"
            size = getattr(widget, f"winfo_{size_name}")()
            viewport = getattr(self.canvas, f"winfo_{size_name}")()
            total = getattr(self.content, f"winfo_{size_name}")()
            start = getattr(self.canvas, f"canvas{axis}")(0)
            if position < start or position + size > start + viewport:
                getattr(self.canvas, f"{axis}view_moveto")(max(0, position - 8) / max(1, total))
