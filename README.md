This is a collection of extensions and sample scripts for extending WinDbg. We'll be adding more samples and extensions over time.

# Getting Started
To load JavaScript extensions:
1. Download the script file locally.
2. Ensure you have a recent version of WinDbg - [WinDbg Preview](http://aka.ms/WinDbgPreview) from the Microsoft Store will always be up to date. Otherwise you can use one of the other methods listed [here](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/) to install it.
3. Start your debugging session.
4. The JavaScript extension (JSProvider) should load automatically. You can validate it's loaded by running the `.scriptproviders` command and checking if JavaScript is on the list.
    * If JavaScript isn't on the list, run `.load jsprovider`
5. Run `.scriptload <path to script>` or `.scriptrun <path to script>`. The README for each script has more detailed usage information.

We have more information on our JavaScript support at https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/javascript-debugger-scripting. If you want to learn more about a script you can open in it WinDbg Preview by hitting the "Scripting" ribbon and clicking "Open Script...". It has intellisense support for JavaScript and NatVis and you can load scripts by hitting "Exceute" in the scripting ribbon.

# Contribute
All the samples and extensions we are publishing are open to contributions of fixes and improvements. At this point we aren't open to accepting new scripts and extensions from the community, but you can make your own repo and share them that way.
