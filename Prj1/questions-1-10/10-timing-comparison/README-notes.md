Question 10 - Timing Comparison

Purpose:
Compare the running time of the three compression programs:

1. Sequential version: MyCompress
2. Forked process version: ParFork
3. Pthread version: ParThread

How to run:

1. From the Prj1 folder, run:

   make

2. Run the timing script:

   sh questions-1-10/10-timing-comparison/run_times.sh

3. Copy the real times into Prj1README.

Notes:
The same generated input file is used for all three programs so the comparison
is fair. The real time from /usr/bin/time is the easiest number to compare.
