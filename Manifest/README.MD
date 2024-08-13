# Extension Gallery
*To use these feature, you need to be running WinDbg 1.0.1902 or newer* 

Many people find themselves with quite a few scripts and extensions to help out with debugging specific issues. Keeping track of where they all are, what's loaded, and how to invoke them can be a bit of a tedious process. A while back we added the concept of what we call an extension gallery or repository to enable seamless loading of scripts and extensions.

There's an extension gallery included in recent versions of WinDbg and WinDbg with some of the extensions that are included with the debugger, but we're also adding one here to make it easy for people who clone this repo to load the extensions.

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
The final requirement for each component is defining what will trigger the loading of the extension. Components can be loaded by either executing a [function alias](https://learn.microsoft.com/windows-hardware/drivers/debugger/debugger-data-model-function-aliases) or retrieving the named extended by the component.
#### Function Aliases
If your script uses function aliases, you'll need to define each of them, their syntax, and their description in a `FunctionAliases` tag.

Each `FunctionAlias` in the `FunctionAliases` list has a `Name` property that is the name of the function alias and an `AliasItem` tag inside of it that contains `Syntax` and `Description` tags describing how to use the function alias.

For more information, about function aliases see [Debugger Data Model Function Aliases](https://learn.microsoft.com/windows-hardware/drivers/debugger/debugger-data-model-function-aliases)

#### Object Extensions
If your script extends the debugger data model, you'll need to define the name of the objects it extends in an `ObjectExtensionList` tag , the namespace it adds, and what property it adds.

Each `NamedExtension` in the `ObjectExtensionList` can have three properties:
* **Name** - The named model which is being extended (ex. "Debugger.Models.Module")
* **Namespace** - The acquired namespace (ex. "Debugger.Models.Module.Contents"). This is an optional property. If it is provided then "Property" property must be provided.
* **Property** - The property under the model which will be added to retrieve the acquired namespace (ex. "Contents" i.e. under "Debugger.Models.Module" will be added a property "Contents"). This is a mandatory property if the Namespace property is provided, otherwise shouldn’t be provided.

#### Views
*To use Views, you need to be running WinDbg 1.2407.24003.0 or newer* 

If your script wants to place icons on the "Extensions" tab of the WinDbg ribbon which, when invoked, will open a UI window visualizing the results of a data model query, each of those icons needs to be defined as a `View` in a `Views` tag.

Each `View` in the `Views` tag binds a coupling of an "Icon" and a "Name" provided by the script to a "Data Model Query" and a "View Type".  The `View` tag contains the following properties:

* **Query** (mandatory): The data model query which will be invoked to present the data.
* **Name** (mandatory): The name of the view.  This is the text which goes below/beside the icon on the ribbon.
* **ViewKind** (optional): The kind of view.  If present, this must be one of **Tree** or **Grid**.  If not present, the UI will select a default form to present the query in.  Note that additional view types will be forthcoming in future debugger releases.
* **Group** (optional): If present, indicates that the view is part of a group.  Being part of a group places the view icon and name under a drop-down on the ribbon.
* **IsPrimary** (optional): A boolean value which, if present, indicates that this view's icons should be used as the group's icons.  Only one view within a group may mark the **IsPrimary** attribute as true.  If no view within a group indicates that it is primary, the UI will pick one of the views arbitrarily to select group icons for the drop-down.
* **LargeIconData** (optional): A string hexdump of the 32x32 PNG data which comprises the large icon for the view.  Either this or the **LargeIcon** attribute (but not both) **MUST** be present.
* **SmallIconData** (optional): A string hexdump of the 16x16 PNG data which comprises the small icon for the view.  Either this or the **SmallIcon** attribute (but not both) **MUST** be present.
* **FilePathKind** (optional): If using **LargeIcon** or **SmallIcon** attributes, this indicates what kind of file paths those elements refer to.  As with other **FilePathKind** attributes in an extension gallery manifest, these can be **PackageRelative**, **RepositoryRelative**, or **Absolute**.
* **LargeIcon** (optional): A path to a 32x32 PNG which is the large icon for the view.  The path kind is indicated by the **FilePathKind** attribute.  Either this or the **LargeIconData** attribute (but not both) **MUST** be present.
* **SmallIcon** (optional): A path to a 16x16 PNG which is the small icon for the view.  The path kind is indicated by the **FilePathKind** attribute.  Either this or the **SmallIconData** attribute (but not both) **MUST** be present.

There is one important thing to note about the icons here.  While you **MUST** provide both a large (32x32 PNG) icon and a small (16x16 PNG) icon, that can be done in one of two ways:

* As a binary blob in the manifest itself using the **LargeIconData** and **SmallIconData** attributes
* As a file in the extension package using the **LargeIcon** and **SmallIcon** attributes

You can use the "as a file in the extension package" method **if and only if** your extension will **always** be on the local disk.  If your extension is included in a network based extension gallery, you **MUST CURRENTLY** use the binary blob method.


## Make your own gallery
You can also make your own extension gallery! You'll want to look through all three files in this folder to understand what to do.
### Manifest File - manifest.n.xml
Each gallery has a manifest file, the exact contents of a manifest is outlined above. The n in the file name can be incremented with new manifest versions and referenced from the manifest version file.

### Manifest Version File - ManifestVersion.txt
Each gallery has a manifest version file that defines the version of the gallery. For the vast majority of cases, you'll want the exact copy of what's in this repo. Incrementing the number on the first line will change which manifest file is referenced.

### Configuration File - config.xml
The configuration file is loaded into the debugger with the `.settings` command and contains some XML elements defining the gallery. Each gallery should have a GUID for a unique GUID and the location of the gallery.