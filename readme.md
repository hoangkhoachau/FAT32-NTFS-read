# FAT32-NTFS-read

This project is a C++ console application designed to read and interact with FAT32 and NTFS file systems.

## Features

- **Read and navigate FAT32 and NTFS file systems**: The application allows users to explore and interact with both FAT32 and NTFS file system.
- **Command-line interface**: Provides a simple command-line interface for executing various file system operations.
- **Support for basic file operations**: Including listing directory contents, reading file attributes, and viewing file contents.

## Classes

- **Utility**: Contains helper functions for string manipulation.
- **Entry**: Base class representing a file system entry.
- **Folder**: Derived from Entry, represents a directory.
- **File**: Derived from Entry, represents a file.
- **TXT**: Derived from File, represents a text file.
- **Filesystem**: Base class for handling file system operations.
- **FAT32**: Derived from Filesystem, provides FAT32-specific functionality.
- **NTFS**: Derived from Filesystem, provides NTFS-specific functionality.
- **CMD**: Manages the command-line interface for interacting with the file system.

## Getting Started

### Prerequisites

- C++ compiler (e.g., g++, clang)
- Windows or Unix-like operating system

### Building and Running

1. Clone the repository:

   ```sh
   git clone https://github.com/hoangkhoachau/FAT32-NTFS-read.git
   cd FAT32-NTFS-read
   ```

2. Compile the application:

   ```sh
   g++ -o FAT32-NTFS-read ConsoleApplication1.cpp
   ```

3. Run the application:
   ```sh
   ./FAT32-NTFS-read
   ```

### Usage

- **dir/ls**: List the contents of the current directory.
- **open [file]**: Open a file.
- **cd [directory]**: Change to a specified directory.
- **info**: Print information about the file system.
- **cls/clear**: Clear the console screen.
- **exit**: Exit the application.

## Acknowledgments

- This project was developed to facilitate learning about file system structures and operations.
- References: File system forensic analysis - Brian Carrier
