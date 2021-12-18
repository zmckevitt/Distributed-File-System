# Distributed File System - CSCI 5273:

Built a distributed file system using C sockets. This file system consists of one client and 4 servers. The user is able to put files onto the server, retrieve said files, create subfolders, and list all files in a given directory. This file system relies on the hashing scheme in the PA4 writeup. This file system also supports multiple connections, user validation, and encrypted files (both in transit and stored on the server). Both the client and server have configuration files that include server locations and user credentials.

NOTE: this distributed file system makes use of temporary folders. On netsys, these folders cannot be removed, even with use of the ```rm -rf``` command. Although this does not affect functionality in any way, it does print an error message on the screen when a user tries to ```get``` a file.

The client directory comes with 5 test files: foo1, foo2, foo3, foo11, and testfile.txt

## Instructions

### Building and Running the program

To build the program, type ```make``` in the ```pa4/``` directory. This will create a ```dfc``` executable in the ```client/``` directory and a ```dfs``` executable in the ```server/``` directory. The makefile also supports ```make clean``` which will clear all files on the server side as well as all gotten files on the client side.

To run the client, run the command ```./dfc <config file>``` where ```dfc.conf``` is the default configuration file for the client.

To run the server, run the command ```./dfs /DFSX <port>``` where ```/DFSX``` specifies server X's directory. Note: the server is expecting a leading '/' in the directory name.

Both the client and server will continue until an interrupt is raised.

### Commands

- GET
   
    To run the GET command, type ```get <filename> <subfolder/>``` where the subfolder parameter is optional. All files gotten via GET will be stored in the ```get_files/``` directory. This includes all files gotten by all users. If a subfolder is specified, GET will fetch from the corresponding subdirectory on the server side.

- PUT
   
    To run the PUT command, type ```put <filename> <subfolder/>``` where the subfolder parameter is optional. This will search for a corresponding file in the ```client/``` directory. If a subfolder is specified, the file will be placed under the corresponding subdirectory on the server if it happens to exist.

- LIST
   
    To run the LIST command ```list <subfolder/>``` where the subfolder parameter is optional. This will display a list of all FILES in the specified directory, but NOT any subdirectories OR files within subdirectories. If no subfolder is specified, list will return all files in the user's default directory on the server, but not files within the subfolders or the subfolders themselves.

- MKDIR
   
    To run the MKDIR command, type ```mkdir subfolder/```. This will create a subfolder under your user directory on the server.

## Config Files

Config files MUST match format. In ```dfc.conf```, be sure to write usernames as ```Username: <username>``` and passwords as ```Password: <password>``` each on their own line. In ```dfs.conf```, put a single username and password separated by a space on each line: ```<username> <password>```.

## Encryption

Packets of data are encrypted and decrypted client side with simple XOR encryption. Files are stored in their encrrypted form on the server side.

## Traffic optimization

Traffic optimization was achieved by keeping a boolean ```pages[]``` array to track which pages are local. If a page is already in the local directory, the client will not send a request for it. This ensures that only 4 pieces of the file are ever transmitted during a GET.

## Future work? Security concerns?

In the future, I believe this work can be expanded upon by improving on the current encryption scheme. Instead of XORing data with a single char, I think that keys should be able to be generated at runtime and should be as long as the max buffer size.
