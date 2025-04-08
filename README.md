# Debugfs GUI: Graphical User Interface for ext2 Filesystem Inspection

A user-friendly graphical application to inspect and analyze ext2 filesystems, providing a visual alternative to the command-line `debugfs` tool.

## Features

- **Filesystem Explorer**: Navigate directories and files in a tree view
- **File Metadata Display**: View detailed inode information, permissions, timestamps, and more
- **Superblock Information Panel**: See filesystem statistics and properties
- **Block Visualization**: Graphical representation of file block allocations
- **Search Functionality**: Find files by name or inode number
- **Export Capabilities**: Save filesystem metadata as JSON or text reports
- **Error Handling**: Gracefully handles corrupt or invalid filesystems

## Prerequisites

- Linux-based operating system
- Python 3.6+
- GTK+ 3.0
- e2fsprogs development libraries

## Installation

1. Install dependencies:

```bash
# For Debian/Ubuntu
sudo apt-get install python3-gi python3-dev libgtk-3-dev libext2fs-dev

# For Fedora/RHEL
sudo dnf install python3-gobject python3-devel gtk3-devel e2fsprogs-devel
```

2. Clone the repository:

```bash
git clone https://github.com/yourusername/debugfs-gui.git
cd debugfs-gui
```

3. Build the project:

```bash
make
```

## Usage

1. Run the application:

```bash
python3 main.py
```

2. Click "Open Filesystem" to select an ext2 filesystem image or device.

3. Navigate the filesystem using the tree view on the left.

4. View file
