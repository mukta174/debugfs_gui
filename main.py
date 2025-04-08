# main.py - Main GUI application

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib, GObject
import json
import threading
from ext2_wrapper import Ext2Reader
import ctypes
lib = ctypes.CDLL("./libext2_reader.so")  # Ensure correct path


class DebugfsGUI:
    def __init__(self):
        # Set up the UI from the Glade file
        self.builder = Gtk.Builder()
        self.builder.add_from_file("debugfs_gui.glade")
        self.builder.connect_signals(self)
        
        # Get main window and widgets
        self.window = self.builder.get_object("main_window")
        self.filesystem_tree = self.builder.get_object("filesystem_treeview")
        self.metadata_view = self.builder.get_object("metadata_textview")
        self.superblock_view = self.builder.get_object("superblock_textview")
        self.block_visualization = self.builder.get_object("blocks_drawingarea")
        self.search_entry = self.builder.get_object("search_entry")
        
        # Set up tree view
        self.setup_filesystem_tree()
        
        # Initialize filesystem reader
        self.ext2_reader = None
        
        # Show the window
        self.window.show_all()
    
    def setup_filesystem_tree(self):
        # Set up columns for the tree view
        name_column = Gtk.TreeViewColumn("Name")
        size_column = Gtk.TreeViewColumn("Size")
        inode_column = Gtk.TreeViewColumn("Inode")
        
        name_cell = Gtk.CellRendererText()
        size_cell = Gtk.CellRendererText()
        inode_cell = Gtk.CellRendererText()
        
        name_column.pack_start(name_cell, True)
        size_column.pack_start(size_cell, True)
        inode_column.pack_start(inode_cell, True)
        
        name_column.add_attribute(name_cell, "text", 0)
        size_column.add_attribute(size_cell, "text", 1)
        inode_column.add_attribute(inode_cell, "text", 2)
        
        self.filesystem_tree.append_column(name_column)
        self.filesystem_tree.append_column(size_column)
        self.filesystem_tree.append_column(inode_column)
        
        # Create tree model
        self.filesystem_model = Gtk.TreeStore(str, str, str)  # Name, Size, Inode
        self.filesystem_tree.set_model(self.filesystem_model)
    
    def on_open_filesystem_clicked(self, button):
        dialog = Gtk.FileChooserDialog(
            title="Select Filesystem Image or Device",
            parent=self.window,
            action=Gtk.FileChooserAction.OPEN,
            buttons=(
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_OPEN, Gtk.ResponseType.OK
            )
        )
        
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            filesystem_path = dialog.get_filename()
            self.load_filesystem(filesystem_path)
            
        dialog.destroy()
    
    def load_filesystem(self, path):
        try:
            # Close previous filesystem if open
            if self.ext2_reader:
                self.ext2_reader.close()
            
            # Create new reader
            self.ext2_reader = Ext2Reader(path)
            
            # Update superblock info
            superblock_info = self.ext2_reader.get_superblock_info()
            self.update_superblock_view(superblock_info)
            
            # Load root directory
            self.load_directory(2)  # Root inode is 2 in ext2
            
        except Exception as e:
            self.show_error_dialog(f"Error opening filesystem: {str(e)}")
    
    def load_directory(self, dir_inode):
        # Clear current tree
        self.filesystem_model.clear()
        
        # Load directory entries asynchronously
        threading.Thread(target=self._load_directory_async, args=(dir_inode,)).start()
    
    def _load_directory_async(self, dir_inode):
        try:
            entries = self.ext2_reader.list_directory(dir_inode)
            
            # Update UI in main thread
            GLib.idle_add(self._update_directory_tree, entries)
        except Exception as e:
            GLib.idle_add(self.show_error_dialog, f"Error loading directory: {str(e)}")
    
    def _update_directory_tree(self, entries):
        # Add entries to tree model
        for entry in entries:
            iter = self.filesystem_model.append(None, [
                entry["name"],
                self._format_size(entry["size"]),
                str(entry["inode"])
            ])
            
            # For directories, add a dummy child for the expander to work
            if entry["filetype"] == "directory" and entry["name"] not in [".", ".."]:
                self.filesystem_model.append(iter, ["Loading...", "", ""])
        
        return False  # Remove from idle queue
    
    def _format_size(self, size_bytes):
        # Convert bytes to human-readable format
        units = ['B', 'KB', 'MB', 'GB', 'TB']
        size = float(size_bytes)
        unit_index = 0
        
        while size >= 1024 and unit_index < len(units) - 1:
            size /= 1024
            unit_index += 1
        
        return f"{size:.2f} {units[unit_index]}"
    
    def on_filesystem_tree_row_expanded(self, treeview, iter, path):
        # Get inode number of the expanded directory
        inode_str = self.filesystem_model.get_value(iter, 2)
        if not inode_str.isdigit():
            return
        
        inode = int(inode_str)
        
        # Remove dummy child
        child = self.filesystem_model.iter_children(iter)
        if child and self.filesystem_model.get_value(child, 0) == "Loading...":
            self.filesystem_model.remove(child)
        
        # Load directory contents
        try:
            entries = self.ext2_reader.list_directory(inode)
            
            for entry in entries:
                if entry["name"] in [".", ".."]:
                    continue
                    
                child_iter = self.filesystem_model.append(iter, [
                    entry["name"],
                    self._format_size(entry["size"]),
                    str(entry["inode"])
                ])
                
                # For directories, add a dummy child
                if entry["filetype"] == "directory":
                    self.filesystem_model.append(child_iter, ["Loading...", "", ""])
                    
        except Exception as e:
            self.show_error_dialog(f"Error loading directory: {str(e)}")
    
    def on_filesystem_tree_row_activated(self, treeview, path, column):
        # Get inode of selected file/directory
        iter = self.filesystem_model.get_iter(path)
        inode_str = self.filesystem_model.get_value(iter, 2)
        
        if inode_str.isdigit():
            inode = int(inode_str)
            
            # Show file metadata
            try:
                inode_info = self.ext2_reader.get_inode_info(inode)
                self.update_metadata_view(inode_info)
                
                # Show block map for visualization
                if "block_count" in inode_info and inode_info["block_count"] > 0:
                    block_map = self.ext2_reader.get_block_map(inode)
                    self.update_block_visualization(block_map)
                    
            except Exception as e:
                self.show_error_dialog(f"Error getting file information: {str(e)}")
    
    def update_superblock_view(self, superblock_info):
        # Format superblock info as text
        text_buffer = self.superblock_view.get_buffer()
        text = "Filesystem Information:\n\n"
        
        for key, value in superblock_info.items():
            text += f"{key}: {value}\n"
            
        text_buffer.set_text(text)
    
    def update_metadata_view(self, inode_info):
        # Format inode info as text
        text_buffer = self.metadata_view.get_buffer()
        text = "File Metadata:\n\n"
        
        for key, value in inode_info.items():
            text += f"{key}: {value}\n"
            
        text_buffer.set_text(text)
    
    def update_block_visualization(self, block_map):
        # Store block map for drawing
        self.block_map = block_map
        # Trigger redraw
        self.block_visualization.queue_draw()
    
    def on_blocks_drawingarea_draw(self, widget, cr):
        # Draw block visualization
        if not hasattr(self, 'block_map') or not self.block_map:
            return False
            
        width = widget.get_allocated_width()
        height = widget.get_allocated_height()
        
        # Calculate block size and layout
        blocks_count = len(self.block_map)
        max_blocks_per_row = 16
        rows = (blocks_count + max_blocks_per_row - 1) // max_blocks_per_row
        
        block_width = min(30, width / max_blocks_per_row)
        block_height = min(30, height / rows)
        
        # Draw blocks
        for i, block_num in enumerate(self.block_map):
            row = i // max_blocks_per_row
            col = i % max_blocks_per_row
            
            x = col * block_width + 5
            y = row * block_height + 5
            
            # Draw block rectangle
            cr.rectangle(x, y, block_width - 2, block_height - 2)
            
            # Color based on block number (sequential blocks have similar colors)
            hue = (block_num % 256) / 256.0
            cr.set_source_rgb(0.5 + 0.5 * hue, 0.5, 0.5 - 0.5 * hue)
            cr.fill_preserve()
            
            cr.set_source_rgb(0, 0, 0)
            cr.stroke()
            
            # Draw block number if block is big enough
            if block_width >= 20:
                cr.set_source_rgb(0, 0, 0)
                cr.select_font_face("Sans", 0, 0)
                cr.set_font_size(block_height / 3)
                cr.move_to(x + 2, y + block_height / 2)
                cr.show_text(str(block_num))
        
        return False
    
    def on_search_button_clicked(self, button):
        search_term = self.search_entry.get_text()
        if not search_term:
            return
            
        # Determine if search term is an inode number or file name
        if search_term.isdigit():
            # Search by inode
            try:
                inode = int(search_term)
                inode_info = self.ext2_reader.get_inode_info(inode)
                self.update_metadata_view(inode_info)
                
                # Show block map for visualization
                if "block_count" in inode_info and inode_info["block_count"] > 0:
                    block_map = self.ext2_reader.get_block_map(inode)
                    self.update_block_visualization(block_map)
                    
            except Exception as e:
                self.show_error_dialog(f"Error finding inode {search_term}: {str(e)}")
        else:
            # Search by file name
            threading.Thread(target=self._search_by_name_async, args=(search_term,)).start()
    
    def _search_by_name_async(self, name):
        try:
            results = self.ext2_reader.search_file(name)
            GLib.idle_add(self._show_search_results, results)
        except Exception as e:
            GLib.idle_add(self.show_error_dialog, f"Error searching for '{name}': {str(e)}")
    
    def _show_search_results(self, results):
        # Show search results in a dialog
        dialog = Gtk.Dialog(
            title="Search Results",
            parent=self.window,
            flags=0,
            buttons=(Gtk.STOCK_CLOSE, Gtk.ResponseType.CLOSE)
        )
        
        # Create a list store to hold results
        list_store = Gtk.ListStore(str, str, str)  # Path, Size, Inode
        
        # Create a tree view to display results
        tree_view = Gtk.TreeView(model=list_store)
        
        # Add columns
        for i, title in enumerate(["Path", "Size", "Inode"]):
            renderer = Gtk.CellRendererText()
            column = Gtk.TreeViewColumn(title, renderer, text=i)
            tree_view.append_column(column)
        
        # Add results to list store
        for result in results:
            list_store.append([
                result["path"],
                self._format_size(result["size"]),
                str(result["inode"])
            ])
        
        # Add tree view to dialog
        scrolled_window = Gtk.ScrolledWindow()
        scrolled_window.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        scrolled_window.add(tree_view)
        scrolled_window.set_size_request(400, 300)
        
        dialog.get_content_area().pack_start(scrolled_window, True, True, 0)
        dialog.show_all()
        
        # Handle double-click on result
        tree_view.connect("row-activated", self.on_search_result_activated, dialog)
        
        dialog.run()
        dialog.destroy()
        
        return False  # Remove from idle queue
    
    def on_search_result_activated(self, treeview, path, column, dialog):
        # Get inode of selected search result
        model = treeview.get_model()
        iter = model.get_iter(path)
        inode_str = model.get_value(iter, 2)
        
        if inode_str.isdigit():
            inode = int(inode_str)
            
            # Show file metadata
            try:
                inode_info = self.ext2_reader.get_inode_info(inode)
                self.update_metadata_view(inode_info)
                
                # Show block map for visualization
                if "block_count" in inode_info and inode_info["block_count"] > 0:
                    block_map = self.ext2_reader.get_block_map(inode)
                    self.update_block_visualization(block_map)
                    
                # Close dialog
                dialog.response(Gtk.ResponseType.CLOSE)
                    
            except Exception as e:
                self.show_error_dialog(f"Error getting file information: {str(e)}")
    
    def on_export_button_clicked(self, button):
        dialog = Gtk.FileChooserDialog(
            title="Export Metadata",
            parent=self.window,
            action=Gtk.FileChooserAction.SAVE,
            buttons=(
                Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_SAVE, Gtk.ResponseType.OK
            )
        )
        
        # Add file filters
        json_filter = Gtk.FileFilter()
        json_filter.set_name("JSON files")
        json_filter.add_pattern("*.json")
        dialog.add_filter(json_filter)
        
        txt_filter = Gtk.FileFilter()
        txt_filter.set_name("Text files")
        txt_filter.add_pattern("*.txt")
        dialog.add_filter(txt_filter)
        
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            filepath = dialog.get_filename()
            file_filter = dialog.get_filter()
            
            if file_filter == json_filter:
                self.export_metadata_json(filepath)
            else:
                self.export_metadata_text(filepath)
            
        dialog.destroy()
    
    def export_metadata_json(self, filepath):
        try:
            # Export filesystem metadata as JSON
            superblock_info = self.ext2_reader.get_superblock_info()
            
            with open(filepath, 'w') as f:
                json.dump(superblock_info, f, indent=2)
                
            self.show_info_dialog(f"Metadata exported to {filepath}")
            
        except Exception as e:
            self.show_error_dialog(f"Error exporting metadata: {str(e)}")
    
    def export_metadata_text(self, filepath):
        try:
            # Export filesystem metadata as text
            superblock_info = self.ext2_reader.get_superblock_info()
            
            with open(filepath, 'w') as f:
                f.write("Filesystem Information:\n\n")
                for key, value in superblock_info.items():
                    f.write(f"{key}: {value}\n")
                    
            self.show_info_dialog(f"Metadata exported to {filepath}")
            
        except Exception as e:
            self.show_error_dialog(f"Error exporting metadata: {str(e)}")
    
    def show_error_dialog(self, message):
        dialog = Gtk.MessageDialog(
            parent=self.window,
            flags=0,
            message_type=Gtk.MessageType.ERROR,
            buttons=Gtk.ButtonsType.OK,
            text="Error"
        )
        dialog.format_secondary_text(message)
        dialog.run()
        dialog.destroy()
    
    def show_info_dialog(self, message):
        dialog = Gtk.MessageDialog(
            parent=self.window,
            flags=0,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text="Information"
        )
        dialog.format_secondary_text(message)
        dialog.run()
        dialog.destroy()
    
    def on_main_window_destroy(self, widget):
        # Clean up resources
        if self.ext2_reader:
            self.ext2_reader.close()
            
        Gtk.main_quit()

# Main entry point
if __name__ == "__main__":
    app = DebugfsGUI()
    Gtk.main()
