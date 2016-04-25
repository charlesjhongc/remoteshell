## Remoteshell
simple remote login shell environment

## Example
```
csh> telnet myserver.nctu.edu.tw 7000 # the server port number
**************************************************************
** Welcome to the information server, myserver.nctu.edu.tw. **
**************************************************************
** You are in the directory, /home/studentA/ras/.
** This directory will be under "/", in this system.
** This directory includes the following executable programs.
**
**	bin/
**	test.html	(test file)
**
** The directory bin/ includes:
**	cat
**	ls
**	removetag		(Remove HTML tags.)
**	removetag0		(Remove HTML tags with error message.)
**	number  		(Add a number in each line.)
**
** In addition, the following two commands are supported by ras.
**	setenv
**	printenv
**
% printenv PATH                       # Initial PATH is bin/ and ./
PATH=bin:.
% setenv PATH bin                     # Set to bin/ only
% printenv PATH
PATH=bin
```
