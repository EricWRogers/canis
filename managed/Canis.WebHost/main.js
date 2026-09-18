import { dotnet } from './_framework/dotnet.js';

export async function createCanisHost(engine) {
    const runtime = await dotnet.create();
    runtime.setModuleImports('canis', {
        dispatch: request => engine.ccall('canis_web_dispatch', 'string', ['string'], [request])
    });
    const exports = await runtime.getAssemblyExports(runtime.getConfig().mainAssemblyName);
    return exports.Canis.WebHost.Program;
}
