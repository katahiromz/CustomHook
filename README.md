# CustomHook by katahiromz

CustomHook realizes API hook by specifying API function names.

CustomHook consists of the following components:

- Payload
- Injector
- HookMaker
- Target

## Payload

Payload is a DLL file to realize API hook.
This DLL file is injected into the target program by Injector.

## Injector

Injector can inject the payload DLL into the target program
by specifying PID (process ID).

Injector can start a target program up with injection.

## HookMaker

HookMaker is a program to rebuild a customized Payload.
Rebuilding Payload requires RosBE (ReactOS Build Environment).

## Target

Target is a sample target program. It shows its PID.
It can test MessageBoxW function.

You can check PID in Task Manager.
