# A simple HTTP server

## Usage
A makefile is provided which compiles the server using Clang with flags `-Wall -Werror -std=gnu99 -D_GNU_SOURCE -pthread -g`. To run the server use the executable `server`, which can be run as `./server <port>`

## Server Features
* Can handle GET and HEAD requests
* Can serve files and directories from the system
* Can serve text and image files
* A null URI displays an index.html file if one is present or defaults to a directory view otherwise
* Supports byte ranges

## Acknowledgements
The source code is based heavily on example code provided by [Ian Batten](https://www.batten.eu.org/~igb/), lecturer of the Networks course at the University of Birmingham.
