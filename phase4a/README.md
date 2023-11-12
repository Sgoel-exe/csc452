# README for Phase 4a

## Test Cases 0-2

Please note that test cases 0-2 are functioning correctly. However, due to the precision of the checks being in microseconds, the output may appear to differ slightly. This is due to the inherent variability in execution times at such a small scale, and does not indicate a problem with the implementation. 

## Test Case 7
The problem here is that we are not able to block the children in a proper way where it also works with other testcases. Ben suggested that we can use somekind of lock (Mboxes) to lock our write syscall but when we try it, the testcase 7 works partially (does not write to terminal 2 and 3) and it makes us fail test case 19,20,22. Thus we decided to leave it as it is where it works for all testcases except testcase 7.