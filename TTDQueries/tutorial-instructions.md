# Time Travel Debugging and Queries

This tutorial demonstrates how to debug C++ code using a [Time Travel Debugging]
recording. This example focuses on the use of Queries to find information about
the execution of the code in question.

## Prerequisites

Note that this tutorial only applies to Windows 10 environments.
Download the following software in order to follow along:

* WindbgNext from the Windows Store
* Visual Studio 15
* Ability to run processes elevated, with admin priviledges

## Prepare program to record

This tutorial will use a pre-existing program from GitHub.

1. Clone the [Sample folder] from the Windows classic samples repository.
2. Open it in Visual Studio and build it. Alternatively just use MsBuild
directly and build the project.
3. Launch/Start the application to make sure it runs as expected.

## Recording

1. Launch Windbg Preview elevated by right clicking on its icon and selecting 'Run as Administrator'
2. On the top left, go to 'File' -> 'Start debugging' -> 'Launch executable (advanced)'
3. Copy the full path of the executable you built into the 'Executable' text block
4. Select the 'Record process with Time Travel Debugging' and fill in the 'Output directory' if
you prefer a different location from the default.

At this point in the steps you should see something like this:

![Windbg Preview Launch](images/windbg-launch.jpg)

5. Once you select 'OK', the program will be launched and recorded by TTD.

You should see a little dialog with information about the recording in progress:

![Windbg Preview Record Progress](images/windbg-record-progress.jpg)

6. <Do we need to do anything ?>
7. Close the program

Note that as soon as the program exists, Windbg Preview will load the recording
for you to replay and/or debug it.

## Running Queries

1. Make sure your debugger has the right path to the symbols (pdb files) of your program.
2. Explore the Data Model Oject that contains TTD specific queries by running
```
dx @$cursession.TTD
```
You should see something like:

![Windbg Preview TTD session](images/windbg-ttd-session.jpg)
3. 

[Time Travel Debugging]: https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/time-travel-debugging-overview
[Sample folder]: https://github.com/Microsoft/Windows-classic-samples/tree/master/Samples