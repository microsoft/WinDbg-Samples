# Time Travel Debugging and Queries

Traditional debugging uses a combination of breakpoints and stepping through 1000s
lines of code to find the problem. This traditional approach is both complex and
time consuming. In this folder you can find easy to follow tutorials on how to
use [Time Travel Debugging] with queries and the debugging data model to find
problems faster and easier.

## Prerequisites

In order to use Time Travel Debugging you must:

* Have a Windows 10 environment
* Download WindbgNext from the Windows Store
* Ability to run processes elevated, aka. with admin privileges

## Time Travel Debugging

[Time Travel Debugging] (TTD), is a tool that allows you to record the execution of
your process, then replay it later both forwards and backwards. TTD can help you
debug issues easier by letting you "rewind" your debugger session, instead of having
to reproduce the issue many times as you debug and try to find the bug.

### Potential issues when recording

As you start to use TTD, you may encounter some issues depending on what the settings
of your environment are or depending on what process you are trying to record.
Check out [TTD - Troubleshoot] for typical issues and how to troubleshoot them.

## Using Queries

Once you have recorded a program's execution, you can use LINQ queries in Windbg
in order to investigate what happened during execution.
Using queries enables powerful debugging experiences by searching the entire code
execution instead of stepping through individual lines of code. You can use a LINQ
query to **search** a TTD trace file by **function calls, time positions, memory addresses**, etc.
Check out [TTD - Queries] to learn more.

### Debugging using a query is great because you can

* Search for problems and control results using familiar LINQ syntax
* Easily visualize patterns in grid view
* Narrow your search faster than using conditional breakpoints

## Video references

* [Defrag Tools - Time Travel Debugging - Introduction](https://channel9.msdn.com/Shows/Defrag-Tools/Defrag-Tools-185-Time-Travel-Debugging-Introduction)
* [Defrag Tools - Time Travel Debugging - Advanced](https://channel9.msdn.com/Shows/Defrag-Tools/Defrag-Tools-186-Time-Travel-Debugging-Advanced)
* [CppCon 2017 - Root Causing Bugs in Commercial Scale Software](https://www.youtube.com/watch?v=l1YJTg_A914)

[Time Travel Debugging]: https://docs.microsoft.com/windows-hardware/drivers/debugger/time-travel-debugging-overview
[TTD - Troubleshoot]: https://docs.microsoft.com/windows-hardware/drivers/debugger/time-travel-debugging-troubleshooting
[TTD - Queries]: https://blogs.msdn.microsoft.com/windbg/2018/02/01/time-travel-debugging-queries/
