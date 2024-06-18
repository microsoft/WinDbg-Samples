# ImageInfo.js

ImageInfo.js is a [JavaScript](https://learn.microsoft.com/windows-hardware/drivers/debugger/javascript-debugger-scripting) debugger extension which parses PE images mapped into the address space of the target and presents the resources within those images on the debugger's module object.

## Usage
Run `.scriptload ImageInfo.js` to load the script. Any module accessed via `dx`, for example `dx @$curprocess.Modules[0]` will have the information below added to it.

## Extension 
This extension extends the debugger's concept of a module to include:
* <b>Headers</b> - A list of available headers from the PE image for the module.
    * <b>ImageHeader</b> - The DOS header for the module -- an _IMAGE_DOS_HEADER.
    * <b>FileHeader</b> - The file header for the module -- an [_IMAGE_FILE_HEADER](https://learn.microsoft.com/windows/win32/api/winnt/ns-winnt-image_file_header).
    * <b>OptionalHeader</b> - The optional header for the module -- an [_IMAGE_OPTIONAL_HEADER(64)](https://learn.microsoft.com/windows/win32/api/winnt/ns-winnt-image_optional_header64).
* <b>Directories</b> - A list of several available directories from the PE image for the module.
    * <b>ExportDirectory</b>
    * <b>ImportDirectory</b>
    * <b>ResourceDirectory</b>
    * <b>DebugDirectories</b> 
* <b>Resources</b> - The resources contained within the module exposed as a tree.
* <b>Imports</b> - A collection of all imports from the given module.  This is structured as a collection of import modules each of which has a collection of import functions.
* <b>DelayImports</b> - A collection of all delayed imports from the given module. This is structured as a collection of import modules each of which has a collection of delay import functions.
* <b>Exports</b> - A collection of all exports from the given module.
* <b>DebugInfo</b> - Information about how to match or look up symbols for the given module obtained from the debug directories.
* <b>Version</b> - A parsed form of the version resources within the given module exposed as a tree similar to the added Resources property.
