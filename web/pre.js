// Persistent storage for the web build: /persist (the project and the kits)
// lives in the browser's IndexedDB. It is loaded before main() runs, then
// written back every few seconds and when the page is hidden.
Module.preRun = Module.preRun || [];
Module.preRun.push(function() {
  // True while a sync is running: the first one loads the files.
  var syncing = true;
  FS.mkdir("/persist");
  FS.mount(IDBFS, {}, "/persist");
  addRunDependency("persist");
  FS.syncfs(true, function(error) {
    if (error) {
      console.warn("Drumf(re)ake: cannot load the saved files", error);
    }
    syncing = false;
    removeRunDependency("persist");
  });

  var flush = function() {
    if (syncing) {
      return;
    }
    syncing = true;
    FS.syncfs(false, function(error) {
      syncing = false;
      if (error) {
        console.warn("Drumf(re)ake: cannot store the saved files", error);
      }
    });
  };
  setInterval(flush, 3000);
  document.addEventListener("visibilitychange", function() {
    if (document.visibilityState === "hidden") {
      flush();
    }
  });
});
