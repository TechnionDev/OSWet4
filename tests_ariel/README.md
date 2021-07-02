# OS HW4 (MALLOC) Part 3 Tests
## Setup:  
Copy your malloc_3.cpp to the folder  
Copy the metadata struct to the start of test.cpp (**without changing Anything in your struct**)   
(Replace the one there now)  
run using cmake

**The tests will only work if you use a metadata list like they said in the file
(i know it's not the only way but that the way my tests work)**

## Notes
if you can help and want to add your tests create a pull request or contact me

### How To Debug:
The best way to debug that i found is to put the test outside the fork  
and then you can debug in normal methods.
just make sure you dont move more then one test outside to avoid them conflicting

## F.A.Q
  *    Why the expected in stats show x?  
    *  The expected is getting calculated from your own memory list. 
       it means you have a problem in the list or the stats func
  *    why one of the test have 2 free blocks that are not merged?  
    *  the segel dont want us to merge on split (ex: on realloc)
       https://piazza.com/class/kmeyq2ecrv940z?cid=622 
