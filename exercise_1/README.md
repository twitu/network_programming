# Exercise 1  
## Objective  
Using signals to synchronize execution of child processes  

## Problem statement
* integer n is taken from command line. Process i is the child of (i-1)th process such that 0<i<=n  
* First process creates an array A of 1000 integers and fills it randomly  
* Each process prints its own pid and the integer A[(itr*N) + (i%n)] where itr is the iteration number starting from 0  
* These processes print in the order of their pid  
* SIGUSR1 and SIGUSR2 are used to achieve synchronization  