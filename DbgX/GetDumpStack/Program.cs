using DbgX;
using DbgX.Interfaces.Services;
using DbgX.Requests;
using DbgX.Requests.Initialization;
using Nito.AsyncEx;
using System.CommandLine;
using System.Xml.Linq;

/// <summary>
/// This sample program shows how to open a dump file and run a data model query. The data model queries that can be run are very similar to what can be queried
/// via the "dx" command in the debugger console. The result comes back as XML that represents the structure of the data, much like what would be displayed
/// in the watch window of WinDbg.
/// </summary>
public class Program
{
    static void Main(string[] args)
    {
        var rootCommand = new RootCommand("Reads a call stack for a crash dump file.");
        var fileOption = new Option<FileInfo>(name: "--file", description: "The crash dump file to read") { IsRequired = true };
        rootCommand.AddOption(fileOption);
        var sympathOption = new Option<string?>(name: "--sympath", description: "The symbol path to use");
        rootCommand.AddOption(sympathOption);

        rootCommand.SetHandler(async (file, path) =>
            {
                await DoReadFile(file.FullName, path);
            },
            fileOption,
            sympathOption);

        AsyncContext.Run(async () =>
        {
            await rootCommand.InvokeAsync(args);
        });
    }

    private static async Task DoReadFile(string file, string? symPath)
    {
        DebugEngine engine = new DebugEngine();
        await engine.SendRequestAsync(new OpenDumpFileRequest(file, new EngineOptions()));
        if (symPath != null)
        {
            await engine.SendRequestAsync(new SetSymbolPathRequest(symPath));
        }

        var modelXml = await engine.SendRequestAsync(new ModelQueryRequest("@$curstack.Frames", false, DbgX.Interfaces.Enums.ModelQueryFlags.Default, recursionDepth: 1));

        XDocument modelDoc = XDocument.Parse(modelXml);
        XElement? rootElement = modelDoc.Root?.Element("Element");
        if (rootElement != null)
        {
            foreach (var element in rootElement.Elements("Element"))
            {
                Console.WriteLine(element?.Attribute("DisplayValue")?.Value ?? "<unknown>");
            }
        }
    }
}
