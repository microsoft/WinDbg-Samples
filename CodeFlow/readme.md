# CodeFlow.js
CodeFlow is a [JavaScript](https://docs.microsoft.com/windows-hardware/drivers/debugger/javascript-debugger-scripting) debugger extension that adds the ability to trace data flow backwards through the control flow of a function.

This extension is still a work in progress.

## Usage
Run `.scriptload CodeFlow.js` to load the extension. Then you can run `dx Debugger.Utility.Code.TraceDataFlow([address])` to walk backwards through the control flow of the function to find any instruction which influenced the source operands of the traced instruction.