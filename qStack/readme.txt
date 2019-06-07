Dependencies:
Boost library
pthreads

To build:
$make

For help running:
$./a.out --help

ToDo:
array of active leaf nodes (optimization)
check for marked pointer before add/remove
elimination? Why not? it should increase performance

can pred[0] be marked but pred[1] not be? (I think not, should let us just check pred[0] in remove())

Double check nodes are being initialized with correct vaues (nullptr in _pred)
Update psuedocode in the paper

MAYBE:
threads don't cas the divider, they just ignore it and go back to it if their actualy CAS fails, appending their op


CRAZY IDEA:
Have operations inserted into a heap by their keys. Have threads FAA a number that corresponds to a set of elements in the heap which they must perform. Elements are likely to be in the same "divider"

