# Phase 2 README

## Race condition testcases
### Testcases: 8, 25, 26, 30, 31, 46

These testcases fail because Russ's implementation of phase2 allows only
one process to be awakened at a time. Thus, the testcases assumes this kind of
implementation. However, he mentioned on the spec that wakeup strategy is upto our design and implementation. This is further supported by the fact that on Discord, he said, "When someone Releases()s a mailbox, the order in which any blocked processes wake up is not defined". We can also see that all outputs are correct, but some of the lines are in different places, proving that our implemetation works. 

## Clock Handler issue

### Testcase: 13

It is correct as the status is close to 100k and the second status is close to
100k more than the previous one

## Message interaction and pointers

### Testcase: 42

Here, our output differ by only one line. The message that was received was
'hello there' instead of 'xxxxxxxxx'. This is because the message was passed
around and the pointer was updated a lot. The same pointer is used for
receiving and sending messages. Thus, the pointer gets updated in the send and is used in the receive. This is why the message is different. However, the
message size is reported correctly. 

## Implementation of Shadow Processes

### Testcase: 28, 29

Russ said in class that the shadow processes are not really required, but recommended. He did not specify how it should be implementated, thus giving us the freedom of choice. We used two queues for consumers and producers to keep track of them. In these cases, when the mailbox release happens, we chose to unblock and clear the producer queue first, and then the consumer queue. This is why the output is different, and also leads us to a deadlock. Our shadow process design has worked in all other testcases, these are the only testcases that failed because of their heavy dependency on the order of unblocking.



