
:: Bootstrapping - delete
copy /y ..\..\..\..\CoreTtd.Component\build\x64-release-lkg\bin\ttd\TTDReplay.dll TTDDownload\x64
copy /y ..\..\..\..\CoreTtd.Component\build\x64-release-lkg\bin\ttd\TTDReplayCPU.dll TTDDownload\x64
copy /y ..\..\..\..\CoreTtd.SDK\install\Apis\sdk\include\ttd\* ..\TraceInformation\packages\Microsoft.TimeTravelDebugging.Apis.0.8.127\sdk\include\TTD
copy /y ..\..\..\..\CoreTtd.SDK\install\Apis\sdk\lib\x64\* ..\TraceInformation\packages\Microsoft.TimeTravelDebugging.Apis.0.8.127\sdk\lib\x64
