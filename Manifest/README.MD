# Extension Gallery
Many people find themselves with quite a few scripts and extensions to help out with debugging specific issues. Keeping track of where they all are, what's loaded, and how to invoke them can be a bit of a tedious process. A while back we added the concept of what we call an extension gallery or repository to enable seamless loading of scripts and extensions.

There's an extension gallery included in recent versions of WinDbg and WinDbg Preview with some of the extensions that are included with the debugger, but we're also adding one here to make it easy for people who clone this repo to load the extensions.

*The process here seems a bit more complicated than it could be and there are some properties and fields that are required but only have a single option. These are for future use as we grow what can be contained in the gallery.*

## How to use
Extension galleries are enabled through the `.settings` command, and we've provided a configuration file you can load to set it up. Once you've cloned this repo locally, you can follow these steps to setup the gallery.
1. Find the `LocalCacheRootFolder` element in *your repo path*\Manifest\config.xml
2. Update the value to "*your repo path*\Manifest\"
3. In a debugger session run `.settings load *your repo path*\Manifest\config.xml`. The debugger should indicate the config file has been loaded successfully.
4. Run `.settings save`. This will save your settings to persist between sessions.
5. Restart your debugger session.
6. Run `dx @$curprocess.Modules.First().Contents`. You should see the extended module information, such as Header and Directories.
7. To see that the ImageInfo script was loaded dynamically, run .scriptlist. You should see ImageInfo.js loaded after the associated objects were referenced with the dx command.

Because the manifest information is saved as part of the debuggers settings,  when running a command, calling a function alias, or accessing a namespace that is defined in that manifest, the debugger will load the extension and run it.

## Add your own extension
To add an extension to a gallery, you just need to modify the manifest XML. You can use the existing extensions as a reference.
### Add an ExtensionPackage element
`ExtensionPackages` consist of some metadata elements and a list of components:
* **Name** - The name of the extension. This should be unique for each manifest file.
* **Version** - The version number of the extension. If multiple galleries have the same extension, that higher version will be used. Modify this if you're forking an extension that we include in the debugger package and want to use your version.
* **Description** - Description of what the extension is used for.
* **Components** - A collection of components that make the extension.

### Components
The `Components` tag contains a list of `ScriptComponent` tags that define each script in the extension. Each `ScriptComponent` has four required properties:
* **Name** - Name of the script
* **Type** - Type of extension. For now "Engine" is the only value.
* **File** - Path to the file relative to the manifest file location.
* **FilePathKind** - Where the path is relative to. For this case it'll always be "RepositoryRelative". You may also use "Absolute" in which case you'd change File to be an absolute path.
### Load triggers
The final requirement for each component is defining what will trigger the loading of the extension. Components can be loaded by either executing a [function alias](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-data-model-function-aliases) or retrieving the named extended by the component.
#### Function Aliases
If your script uses function aliases, you'll need to define each of them, their syntax, and their description in a `FunctionAliases` tag.

Each `FunctionAlias` in the `FunctionAliases` list has a `Name` property that is the name of the function alias and an `AliasItem` tag inside of it that contains `Syntax` and `Description` tags describing how to use the function alias.

For more information, about function aliases see [Debugger Data Model Function Aliases](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-data-model-function-aliases)

#### Object Extensions
If your script extends the debugger data model, you'll need to define the name of the objects it extends in an `ObjectExtensionList` tag , the namespace it adds, and what property it adds.

Each `NamedExtension` in the `ObjectExtensionList` can have three properties:
* **Name** - The named model which is being extended (ex. "Debugger.Models.Module")
* **Namespace** - The acquired namespace (ex. "Debugger.Models.Module.Contents"). This is an optional property. If it is provided then "Property" property must be provided.
* **Property** - The property under the model which will be added to retrieve the acquired namespace (ex. "Contents" i.e. under "Debugger.Models.Module" will be added a property "Contents"). This is a mandatory property if the Namespace property is provided, otherwise shouldn’t be provided.

## Make your own gallery
You can also make your own extension gallery! You'll want to look through all three files in this folder to understand what to do.
### Manifest File - manifest.n.xml
Each gallery has a manifest file, the exact contents of a manifest is outlined above. The n in the file name can be incremented with new manifest versions and referenced from the manifest version file.

### Manifest Version File - ManifestVersion.txt
Each gallery has a manifest version file that defines the version of the gallery. For the vast majority of cases, you'll want the exact copy of what's in this repo. Incrementing the number on the first line will change which manifest file is referenced.

### Configuration File - config.xml
The configuration file is loaded into the debugger with the `.settings` command and contains some XML elements defining the gallery. Each gallery should have a GUID for a unique GUID and the location of the gallery.