# ext2_wrapper.py - Python bindings for ext2_reader C library

import ctypes
from ctypes import c_int, c_char_p, c_size_t, c_void_p, POINTER, Structure
import json
import os

class Ext2Context(Structure):
    _fields_ = [("fs", c_void_p), ("device_path", c_char_p)]

class Ext2Reader:
    def __init__(self, device_path):
        # Ensure the device exists
        if not os.path.exists(device_path):
            raise FileNotFoundError(f"Device or file not found: {device_path}")
            
        self.ctx = Ext2Context()
        self.device_path = device_path
        
        # Load the C library
        try:
            self._lib = ctypes.CDLL('./libext2_reader.so')
        except OSError:
            raise RuntimeError("Could not load libext2_reader.so. Make sure it's in the current directory.")
        
        # Configure function prototypes
        self._lib.ext2_init.argtypes = [POINTER(Ext2Context), c_char_p]
        self._lib.ext2_init.restype = c_int
        
        self._lib.ext2_get_superblock_info.argtypes = [POINTER(Ext2Context), c_char_p, c_size_t]
        self._lib.ext2_get_superblock_info.restype = c_int
        
        self._lib.ext2_list_directory.argtypes = [POINTER(Ext2Context), c_int, c_char_p, c_size_t]
        self._lib.ext2_list_directory.restype = c_int
        
        self._lib.ext2_get_inode_info.argtypes = [POINTER(Ext2Context), c_int, c_char_p, c_size_t]
        self._lib.ext2_get_inode_info.restype = c_int
        
        self._lib.ext2_get_block_map.argtypes = [POINTER(Ext2Context), c_int, POINTER(c_int), c_size_t]
        self._lib.ext2_get_block_map.restype = c_int
        
        self._lib.ext2_search_file.argtypes = [POINTER(Ext2Context), c_char_p, c_char_p, c_size_t]
        self._lib.ext2_search_file.restype = c_int
        
        self._lib.ext2_close.argtypes = [POINTER(Ext2Context)]
        self._lib.ext2_close.restype = None
        
        # Initialize filesystem
        ret = self._lib.ext2_init(ctypes.byref(self.ctx), device_path.encode('utf-8'))
        if ret != 0:
            raise RuntimeError(f"Failed to open ext2 filesystem: {device_path}")
    
    def get_superblock_info(self):
        """Get filesystem superblock information"""
        buffer_size = 4096
        json_buffer = ctypes.create_string_buffer(buffer_size)
        
        ret = self._lib.ext2_get_superblock_info(ctypes.byref(self.ctx), json_buffer, buffer_size)
        if ret != 0:
            raise RuntimeError("Failed to get superblock information")
        
        return json.loads(json_buffer.value.decode('utf-8'))
    
    def list_directory(self, inode_num):
        """List directory entries for the given inode"""
        buffer_size = 65536  # Large buffer for many entries
        json_buffer = ctypes.create_string_buffer(buffer_size)
        
        ret = self._lib.ext2_list_directory(ctypes.byref(self.ctx), inode_num, json_buffer, buffer_size)
        if ret != 0:
            raise RuntimeError(f"Failed to list directory contents for inode {inode_num}")
        
        return json.loads(json_buffer.value.decode('utf-8'))
    
    def get_inode_info(self, inode_num):
        """Get information about the given inode"""
        buffer_size = 8192
        json_buffer = ctypes.create_string_buffer(buffer_size)
        
        ret = self._lib.ext2_get_inode_info(ctypes.byref(self.ctx), inode_num, json_buffer, buffer_size)
        if ret != 0:
            raise RuntimeError(f"Failed to get information for inode {inode_num}")
        
        return json.loads(json_buffer.value.decode('utf-8'))
    
    def get_block_map(self, inode_num):
        """Get block map for the given inode"""
        max_blocks = 1024
        blocks_array = (c_int * max_blocks)()
        
        block_count = self._lib.ext2_get_block_map(ctypes.byref(self.ctx), inode_num, blocks_array, max_blocks)
        if block_count < 0:
            raise RuntimeError(f"Failed to get block map for inode {inode_num}")
        
        return [blocks_array[i] for i in range(block_count)]
    
    def search_file(self, name):
        """Search for files by name"""
        buffer_size = 524288  # 512KB buffer for search results
        json_buffer = ctypes.create_string_buffer(buffer_size)
        
        ret = self._lib.ext2_search_file(ctypes.byref(self.ctx), name.encode('utf-8'), 
                                       json_buffer, buffer_size)
        if ret != 0:
            raise RuntimeError(f"Failed to search for files with name '{name}'")
        
        return json.loads(json_buffer.value.decode('utf-8'))
    
    def close(self):
        """Close the filesystem and free resources"""
        self._lib.ext2_close(ctypes.byref(self.ctx))
