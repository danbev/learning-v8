import lldb
import re

def bta(debugger, command, result, dict):
  """Print stack trace with assertion scopes"""
  func_name_re = re.compile("([^(<]+)(?:\(.+\))?")
  assert_re = re.compile("^v8::internal::Per\w+AssertType::(\w+)_ASSERT, (false|true)>")
  target = debugger.GetSelectedTarget()
  process = target.GetProcess()
  thread = process.GetSelectedThread()
  frame = thread.GetSelectedFrame()
  for frame in thread:
    functionSignature = frame.GetDisplayFunctionName()
    if functionSignature is None:
      continue
    functionName = func_name_re.match(functionSignature)
    line = frame.GetLineEntry().GetLine()
    sourceFile = frame.GetLineEntry().GetFileSpec().GetFilename()
    if line:
      sourceFile = sourceFile + ":" + str(line)

    if sourceFile is None:
      sourceFile = ""
    print("[%-2s] %-60s %-40s" % (frame.GetFrameID(), 
                                  functionName.group(1),
                                  sourceFile))
    match = assert_re.match(str(functionSignature))
    if match:
      if match.group(3) == "false":
        prefix = "Disallow"
        color = "\033[91m"
      else:
        prefix = "Allow"
        color = "\033[92m"
      print("%s -> %s %s (%s)\033[0m" % (color, prefix, match.group(2), match.group(1)))
    


def __lldb_init_module (debugger, dict):
  debugger.HandleCommand('command script add -f lldb_commands.bta bta')
