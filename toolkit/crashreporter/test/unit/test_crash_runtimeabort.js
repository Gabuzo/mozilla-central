function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crash_purevirtual.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // Try crashing with a runtime abort
  do_crash(function() {
             crashType = Components.interfaces.nsITestCrasher.CRASH_RUNTIMEABORT;
             crashReporter.annotateCrashReport("TestKey", "TestValue");
           },
           function(mdump, extra) {
             do_check_eq(extra.TestKey, "TestValue");
           },
          // process will exit with a zero exit status
          true);
}
