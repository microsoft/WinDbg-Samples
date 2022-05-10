using DbgX;
using DbgX.Interfaces.Services;
using DbgX.Requests;
using DbgX.Requests.Initialization;
using Nito.AsyncEx;

// DbgX was designed to be used in a UI context. If you're using it outside
// of a UI, make sure to establish a SynchronizationContext.
AsyncContext.Run(async () =>
{
    DebugEngine engine = new DebugEngine();
    engine.DmlOutput += Engine_DmlOutput;
    await engine.SendRequestAsync(new CreateProcessRequest("cmd.exe", "", new EngineOptions()));
    await engine.SendRequestAsync(new ExecuteRequest(".prefer_dml 0"));
    await engine.SendRequestAsync(new ExecuteRequest("k"));
});

void Engine_DmlOutput(object? sender, OutputEventArgs e)
{
    Console.Write(e.Output);
}