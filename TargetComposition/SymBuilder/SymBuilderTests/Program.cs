using DbgX;
using DbgX.Interfaces.Services;
using DbgX.Requests;
using DbgX.Requests.Initialization;
using Microsoft.PowerShell.Commands;
using Nito.AsyncEx;

using System;
using System.IO;
using System.Text;
using System.Runtime.CompilerServices;
using System.Xml.Linq;
using System.Text.RegularExpressions;

/// <summary>
/// Represents an execution of a test suite as found in a JavaScript test script.
/// An independent instance of the engine will be spun against the selected target,
/// the script will be executed, and each method named Test_* will be executed in
/// turn against that instance of the engine.
/// 
/// It is important to note that *EACH* script spins an independent engine instance
/// against the given target.
///
/// Each test method is expected to return a boolean true value if it passes and throw
/// an exception (with diagnostic message) if it fails.  While 'false' is also considered 
/// failure, it isn't nearly as diagnosable.
/// 
/// </summary>
class TestSuiteExecution
{
    public TestSuiteExecution(string debuggerInstallPath, string scriptPath)
    {
        m_debuggerInstallPath = debuggerInstallPath;
        m_scriptPath = scriptPath;
        m_allOutput = new List<string>();
        m_lastOutput = new List<string>();
    }

    public class ExecutionResult
    {
        public int TestsRun;
        public int TestsPassed;
        public List<string> FailingTests;

        public ExecutionResult()
        {
            TestsRun = 0;
            TestsPassed = 0;
            FailingTests = new List<string>();
        }
    }

    public ExecutionResult ExecuteTests()
    {
        ExecutionResult result = new ExecutionResult();
        List<string>? loadOutput = null;

        Console.WriteLine($"Executing Test Suite {Path.GetFileName(m_scriptPath)}:");

        AsyncContext.Run(async () =>
        {
            DebugEngine engine = new DebugEngine();
            engine.DmlOutput += EngineOutput;

            int commandsProcessed = await SetupEngineForScript(engine);
            if (commandsProcessed == 0)
            {
                Console.WriteLine("    Unable to setup script: no harness commands recognized!");
                return;
            }

            // await engine.SendRequestAsync(new CreateProcessRequest("notepad.exe", "", new EngineOptions()));
            await engine.SendRequestAsync(new ExecuteRequest(".load SymbolBuilderComposition.dll"));

            //
            // It's important to note that DbgX does *NOT* have JavaScript extensibility by default.
            // We need to explicitly load it from some debugger install.
            //
            string jsProviderPath = Path.Combine(m_debuggerInstallPath, "winext\\JsProvider.dll");
            await engine.SendRequestAsync(new ExecuteRequest(".load " + jsProviderPath));

            ClearOutput();
            await engine.SendRequestAsync(new ExecuteRequest(".scriptload " + m_scriptPath));
            loadOutput = m_lastOutput;
            ClearOutput();

            await engine.SendRequestAsync(new ExecuteRequest("dx @$testInit = @$scriptContents.initializeTests()"));

            //
            // The execution of initializeTests() should return a JS array of objects:
            //
            //     { Name: <test name>, Code: <test code> }
            //
            // While the code cannot be serialized across the wire, we can go back and manually ask the data model
            // to invoke the test code and then deal with the results.
            //
            var testInfraXml = await engine.SendRequestAsync(new ModelQueryRequest("@$testInit,100000", false, DbgX.Interfaces.Enums.ModelQueryFlags.Default, recursionDepth: 2));
            XDocument modelDoc = XDocument.Parse(testInfraXml);

            //
            // rootElement: The array itself...
            //
            XElement? rootElement = modelDoc.Root?.Element("Element");
            if (rootElement != null)
            {
                //
                // Each { Name: , Code: } element...  plus some things like .length
                //
                int curTest = 0;
                foreach (var element in rootElement.Elements("Element"))
                {
                    string isIteratedElement = element.Attribute("Iterated")?.Value ?? "false";
                    if (isIteratedElement == "true")
                    {
                        foreach (var childElement in element.Elements("Element"))
                        {
                            string propName = childElement.Attribute("Name")?.Value ?? "";
                            if (propName == "Name")
                            {
                                string propValue = childElement.Attribute("DisplayValue")?.Value ?? "";
                                bool testPassed = await ExecuteTest(engine, propValue, curTest);
                                if (testPassed)
                                {
                                    result.TestsPassed++;
                                }
                                else
                                {
                                    result.FailingTests.Add(propValue);
                                }
                                result.TestsRun++;
                            }
                        }
                        curTest++;
                    }
                }
            }
        });

        if (result.TestsRun == 0)
        {
            Console.WriteLine("   WARNING: no tests were detected in execution of script test suite!");
            Console.WriteLine("   Loader Output:");
            Console.WriteLine("   ************************************************************");

            if (loadOutput != null)
            {
                foreach (string outputStr in loadOutput)
                {
                    foreach (string line in outputStr.Split('\n'))
                    {
                        Console.WriteLine("        " + line);
                    }
                }
            }
        }

        return result;
    }

    /// <summary>
    /// Performs a command within a "[Harness: (command)]" block at the start of the file
    /// </summary>
    /// <param name="cmd"></param>
    async Task<bool> PerformHarnessCommand(DebugEngine engine, string cmd)
    {
        if (cmd.StartsWith("run "))
        {
            var cmdLine = cmd.Substring(4);
            await engine.SendRequestAsync(new CreateProcessRequest(cmdLine, "", new EngineOptions()));
            return true;
        }
        return false;
    }

    /// <summary>
    /// Opens the script and does some very limited parsing for "setup" commands to run the test suite contained
    /// in the script.  These must come in the form of comment lines immediately after any optional "use strict"
    /// at the top of the file.
    /// 
    /// Instructions to the harness are of the form "// [Harness: <command>]".
    /// 
    /// Supported commands are:
    /// 
    ///     run <command-line>  : Starts the process as given by <command-line> and waits for it to start
    ///     
    /// </summary>
    async Task<int> SetupEngineForScript(DebugEngine engine)
    {
        Regex harnessRe = new Regex(@"^//\s*\[Harness:\s*([^\\]+)\]");

        int commandsProcessed = 0;
        using (StreamReader reader = File.OpenText(m_scriptPath))
        {
            while(reader.Peek() >= 0)
            {
                string? line = reader.ReadLine();
                if (line == null) { break; }

                if (line.StartsWith("//"))
                {
                    Match match = harnessRe.Match(line);
                    if (match.Success && await PerformHarnessCommand(engine, match.Groups[1].Value))
                    {
                            commandsProcessed++;
                    }

                    continue;
                }
                else if (String.IsNullOrWhiteSpace(line) || line.StartsWith("\"use strict\""))
                {
                    //
                    // Just skip over any blank lines or "use strict" directives at the top of the .js file.
                    //
                    continue;
                }
                else
                {
                    //
                    // Anything else and we are done.
                    //
                    break;
                }
            }
        }
        return commandsProcessed;
    }

    /// <summary>
    /// Executes a single test case on the given engine by its index into the initializeTests() returned array from JavaScript.
    /// </summary>
    /// <param name="engine">The engine on which to execute the test</param>
    /// <param name="testName">The name of the test for display and diagnostic purposes</param>
    /// <param name="testId">The zero based index of the test into the initializeTests() returned array</param>
    /// <returns>An indication of whether the test passed or failed</returns>
    async Task<bool> ExecuteTest(DebugEngine engine, string testName, int testId)
    {
        string? errorMsg = null;
        bool pass = false;

        Console.Write($"    Executing Test {testName}: ");
        var execResult = await engine.SendRequestAsync(new ModelQueryRequest("@$testInit[" + testId.ToString() + "].Code()", false, DbgX.Interfaces.Enums.ModelQueryFlags.Default, recursionDepth : 1));
        XDocument modelDoc = XDocument.Parse(execResult);
        XElement? rootElement = modelDoc.Root?.Element("Element");
        if (rootElement != null)
        {
            string isError = rootElement.Attribute("IsError")?.Value ?? "false";
            if (isError == "true")
            {
                XAttribute? displayValAttr = rootElement.Attribute("DisplayValue");
                if (displayValAttr != null)
                {
                    errorMsg = displayValAttr.Value;
                }
            }
            else
            {
                string? valueType = rootElement.Attribute("ValueType")?.Value;
                if (valueType != null && valueType == "11")
                {
                    pass = (rootElement.Attribute("DisplayValue")?.Value ?? "false") == "true";
                }
            }
        }

        if (!pass)
        {
            Console.WriteLine("FAILED ({0})", errorMsg ?? "unknown error");
        }
        else
        {
            Console.WriteLine("PASSED");
        }

        return pass;
    }
    void ClearOutput()
    {
        m_lastOutput = new List<string>();
    }
    void EngineOutput(object? sender, OutputEventArgs e)
    {
        m_allOutput.Add(e.Output);
        m_lastOutput.Add(e.Output);
    }

    string m_debuggerInstallPath;
    string m_scriptPath;
    List<string> m_allOutput;
    List<string> m_lastOutput;
}

public class Program
{
    static void Main(string[] args)
    {
        if (args.Length == 0)
        {
            Console.WriteLine("Usage: SymbolBuilderTests <debugger install path> [<script path>]");
            return;
        }

        string debuggerInstallPath = args[0];

        string scriptPath;
        if (args.Length > 1)
        {
            scriptPath = args[1];
        }
        else
        {
            scriptPath = Directory.GetCurrentDirectory();
        }

        //
        // Once we have a directory containing a set of test scripts against which
        // to validate behaviors of the symbol builder, spin up an instance of the engine
        // against some target *FOR EACH* script in the directory.
        //
        // Top level methods within the script that are named Test_* will be executed
        // as test cases 
        //
        TestSuiteExecution.ExecutionResult overallResults = new TestSuiteExecution.ExecutionResult();

        var scripts = Directory.EnumerateFiles(scriptPath, "*.js");
        if (scripts != null)
        {
            foreach (var script in scripts)
            {
                var fileName = Path.GetFileName(script);
                TestSuiteExecution suiteExec = new TestSuiteExecution(debuggerInstallPath, script);
                TestSuiteExecution.ExecutionResult result = suiteExec.ExecuteTests();

                overallResults.TestsPassed += result.TestsPassed;
                overallResults.TestsRun += result.TestsRun;
                overallResults.FailingTests.AddRange(result.FailingTests.Select(name => fileName + ": " + name));
            }
        }

        Console.WriteLine("\n************************************************************");
        Console.WriteLine($"{overallResults.TestsRun} tests were run, PASS = {overallResults.TestsPassed}, FAIL = {overallResults.TestsRun - overallResults.TestsPassed}");
        if (overallResults.TestsPassed != overallResults.TestsRun)
        {
            Console.WriteLine("\nFAILING TESTS:");
            Console.WriteLine("************************************************************");
            foreach (string failureName in overallResults.FailingTests)
            {
                Console.WriteLine($"    {failureName}\n");
            }
        }
    }
}