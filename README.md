# Description of the class
This is my second class at OMSCS. The first class is Compiler: Theory and Practice. This is another challenging class for me due to my lack of training in Computer Science. 

Through the class, I was able to learn extensively about:
-	The C programming language
-	Memory allocation in an operating system
-	Communications between processes in an OS
-	Remote procedure call

I am proud to have overcome my disadvantages, and to be able to perform well in this class. Indeed, there were many times I realized that I was able to understand the materials and the logic much better than other students in the class with a Computer Science background.


# Solution: Project design
As suggested in the project description, the webproxy and simplecached communicate through 2 channels:
## Command channel: 
### Message passing:
1st channel: for the webproxy to send: 
* The requested file name, 
* The ID of the thread making the request, and 
* The segment size

2nd channel: for the simplecached to respond with:
* The file size
* The name of the shared channel
* The amount of bytes written to the shared channel
* The type of this message is the unique ID of the requesting webproxy thread plus 10, so that only the thread can receive the message.

I decide to split into 2 channels because the structure of the message is different.
### Semaphore: 
so that the webproxy thread can signal to the simplecached that it has finished reading the data, so that simplecached can continue writing

## Data channel: 
I use POSIX shared memory for the data channel. The number of shared segments is controlled by a semaphore in the webproxy: whenever the value in the semaphore > 0, a shared segment is available and can be used by a webproxy thread. 
	
In fact, the shared segment is created and named by the simplecached. The name of the shared segment is “/segmentName” + threadID of the thread in the simplecached. Every time the simplecached handles a GetFile request, it has to create a new shared segment. 

Even though my code passes all the Bonnie tests, I feel this is a bit like cheating. It would be better to create a stack of Shared Segments so that each webproxy thread can get a Shared Segments when it is available. 

# Observations
Most of my struggles with the project come from not knowing the syntax of the different libraries, especially for shared memory. I spent quite some time researching different materials online on how to use named shared memory for POSIX and named semaphore. Simplying reading the manual page is not that helpful. I was able to learn a lot about the syntax and the usage of different C functions through stackoverflow.

