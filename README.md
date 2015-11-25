## EECS 343 Course Projects

### General Guides
1. Please use git clone https://github.com/hercule24/EECS-343.git to clone this repository, then you can begin to write your part of code, BEFORE PUSH, please do a **"git pull --rebase"** to make sure you are pulling the latest change by others without merging, this will keep the commits history linear, then do a **"git push"** to push your code.
2. **Please conform to the styles in the skeleton**, such as indentation, as this will make others life easier.
3. **Please write reasonable, self-explanatory commit messages**.

### Project 1
Project 1 has included a sample code as TA has shown in discussion class. So do read the sample code as to understand the basics.

### Project 2
This project deals with pointers a lot. If you declare a pointer for later use, **please make it equal to NULL**, like the following.
```
int *p = NULL;
```
Before you dereference the pointer when you traverse the linked list or do some other things, **make sure it doesn't point to NULL**, like the following:
```
while (p != NULL) {
  Node *next = p->next;
}
```
or:
```
if (p != NULL) {
  Node *next = p->next;
}
```
After you use the pointer, if the pointer will be used in the future, **please make it also equal to NULL**. Because in this way, you can later check if this pointer is NULL or not before dereference it.
```
p = NULL;
```
All the above efforts are to prevent the notorious segmentation fault, I hope we won't spend to much time debugging seg fault.

### Project 4
There is also a good article about the Ext2 on http://wiki.osdev.org/Ext2.
