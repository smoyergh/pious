PIOUS
=====

PIOUS (Parallel Input/OUtput System) is a parallel file system I
developed long ago for the PVM (Parallel Virtual Machine) cluster
computing environment.  PVM was widely used for (what we now
call) Big Data problems before the days of MPI and Hadoop.

The PIOUS architecture is very similar to that of the Hadoop file
system (HDFS).  Applications link with the PIOUS library and access
files declustered across a set of PIOUS data servers (PDS).  The PIOUS
service coordinator (PSC) supports coordination and metadata operations
but is not on the data path.

While the PIOUS architecture is similar to HDFS the feature set is
of course different.  PIOUS focuses on multi-dimensional files and
light-weight concurrency control to support parallel file access.

PIOUS is also written in C versus Java.

So why am I putting this code out on GitHub when PVM is no longer
widely used?  Good question.

Mainly because PIOUS has minimal dependence on PVM and can be trivally
ported to other environments.  All PVM dependencies are isolated
to a single file: src/pdce/pdce.c (and related .h files in that directory).

So if someone is looking for HDFS-like functionality but in C rather than
Java, it will be a lot easier to port PIOUS and use it as a starting point
versus starting from scratch.

I confirmed that both PIOUS and PVM still build, run, and pass tests on
recent Linux systems.
