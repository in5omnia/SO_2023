## Operating Systems Project 1

Consisted in extending TecnicoFS (Técnico File System), a simplified file system in user-mode. 
It is implemented as a library, which can be used by any client process that wishes to have a private instance of a file system in which it can maintain its data.
We had to add the functionalities:
- importing the content of a file from the native file system into the TecnicoFS
- creating and deleting both symlinks and hardlinks
- make the TecnicoFS thread-safe (enable the client program to have multiple concurrent tasks)

