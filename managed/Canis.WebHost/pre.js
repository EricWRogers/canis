Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
    addRunDependency('canis-dotnet');
    const base = new URL('managed/', document.baseURI);
    Promise.all([
        import(new URL('main.js', base).href).then(module => module.createCanisHost(Module)),
        fetch(new URL('Canis.Native.generated.cs', base)).then(response => {
            if (!response.ok) throw new Error('Missing packaged C# native bindings');
            return response.text();
        })
    ]).then(([host, bindings]) => {
        Module.canisManaged = host;
        Module.canisManagedBindings = bindings;
        removeRunDependency('canis-dotnet');
    }).catch(error => {
        console.error('C# browser startup failed:', error);
        if (Module.setStatus) Module.setStatus('C# startup failed: ' + error.message);
        // Keep main blocked: a partially initialized scene must not run without its scripts.
    });
});
