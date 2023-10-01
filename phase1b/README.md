## README for Phase 1b

### Testcases 14, 15, 16
In testcases 14-16, there are race conditions which contradicts the one in testcase 13. Here, the zapped process (i.e `XXp3`) has priority 3 as well as `testcase_main`. According to testcase 13, zapped processes should be given more precedence when there is a race between two processes with the same priorities. But in testcases 14-16, `testcase_main` is given more precedence. Our scheduler will always give more precedence to a zapped process over a non-zapped process in the same priority (Russ 1).  Hence, testcases 14-16 will fail.

### Tesscase 17, 27

In testcase 17 and 27, we are following a strict FIFO ordering in each priority level. Hence, the testcase will fail as the order of execution will always take us to the first process that was created in that priority level. Hence, the `testcase_main` will always end up executing first until it is blocked on a `join()`

### Testcase 18

Again, since we are following a strict FIFO order, the order in which all the processes are being terminated is the same as the order in which they were created. However, the testcase expects the processes to be terminated in the reverse order of creation. Hence, the testcase will fail. 

Work cited:
1. Instead,it sets a flag which tells theprocess that it should quit() as soon as is practical.
    Pg number 3

