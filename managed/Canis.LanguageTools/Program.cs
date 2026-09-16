using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Xml.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;
try {
    var request=JsonNode.Parse(File.ReadAllText(args[0]))!;
    string path=Path.GetFullPath(request["path"]!.GetValue<string>()),text=request["text"]!.GetValue<string>();
    int byteCursor=Math.Clamp(request["cursor"]!.GetValue<int>(),0,Encoding.UTF8.GetByteCount(text));
    int position=Encoding.UTF8.GetString(Encoding.UTF8.GetBytes(text).AsSpan(0,byteCursor)).Length;
    var project=XDocument.Load(request["project"]!.GetValue<string>());
    var trees=new List<SyntaxTree>();
    foreach(var include in project.Descendants("Compile")) {
        var file=Path.GetFullPath(include.Attribute("Include")!.Value);
        var content=file==path?text:request["overlays"]?[file]?.GetValue<string>()??File.ReadAllText(file);
        trees.Add(CSharpSyntaxTree.ParseText(content,path:file));
    }
    if(!trees.Any(t=>t.FilePath==path))trees.Add(CSharpSyntaxTree.ParseText(text,path:path));
    trees.Add(CSharpSyntaxTree.ParseText("global using System; global using System.Collections.Generic; global using System.Linq; global using System.Threading; global using System.Threading.Tasks; global using System.IO;"));
    var references=((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator).Select(p=>MetadataReference.CreateFromFile(p)).ToList();
    foreach(var hint in project.Descendants("HintPath"))references.Add(MetadataReference.CreateFromFile(hint.Value));
    var compilation=CSharpCompilation.Create("Canis.Language",trees,references,new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
    var tree=trees.Single(t=>t.FilePath==path);var model=compilation.GetSemanticModel(tree);var root=tree.GetRoot();
    object Point(SyntaxTree t,int offset) {var source=t.GetText();var p=source.Lines.GetLinePosition(offset);var line=source.Lines[p.Line];return new {line=p.Line,character=Encoding.UTF8.GetByteCount(source.ToString(TextSpan.FromBounds(line.Start,offset)))};}
    object Range(SyntaxTree t,TextSpan span)=>new {start=Point(t,span.Start),end=Point(t,span.End)};
    object Location(Location l)=>new {uri=new Uri(l.SourceTree!.FilePath).AbsoluteUri,range=Range(l.SourceTree,l.SourceSpan)};
    var token=root.FindToken(Math.Clamp(position-1,0,Math.Max(0,text.Length-1)));
    var symbol=token.Parent is {} node?model.GetSymbolInfo(node).Symbol:null;
    object result;
    switch(request["method"]!.GetValue<string>()) {
        case "diagnostics":result=new {diagnostics=model.GetDiagnostics().Where(d=>d.Location.IsInSource && d.Location.SourceTree==tree).Select(d=>new {range=Range(tree,d.Location.SourceSpan),message=d.GetMessage(),severity=d.Severity==DiagnosticSeverity.Error?1:2}).ToArray()};break;
        case "textDocument/completion":
            INamespaceOrTypeSymbol? container=null;
            var access=token.Parent?.AncestorsAndSelf().OfType<MemberAccessExpressionSyntax>().FirstOrDefault();
            if(access is not null)container=model.GetTypeInfo(access.Expression).Type??model.GetSymbolInfo(access.Expression).Symbol as INamespaceOrTypeSymbol;
            var symbols=model.LookupSymbols(position,container,includeReducedExtensionMethods:true);
            result=new {items=symbols.Where(s=>!s.IsImplicitlyDeclared && s.CanBeReferencedByName).GroupBy(s=>s.Name).Select(g=>g.First()).OrderBy(s=>s.Name).Take(500).Select(s=>new {label=s.Name,insertText=s.Name,detail=s.ToDisplayString(SymbolDisplayFormat.MinimallyQualifiedFormat)}).ToArray()};break;
        case "textDocument/definition":
            result=(symbol?.Locations??[]).Where(l=>l.IsInSource).Select(Location).ToArray();break;
        case "textDocument/references":
            var found=new List<object>();if(symbol is not null)foreach(var t in trees){var m=compilation.GetSemanticModel(t);foreach(var n in t.GetRoot().DescendantNodes().OfType<IdentifierNameSyntax>())if(SymbolEqualityComparer.Default.Equals(m.GetSymbolInfo(n).Symbol,symbol))found.Add(Location(n.GetLocation()));}result=found;break;
        case "textDocument/documentSymbol":
            result=root.DescendantNodes().Where(n=>n is BaseTypeDeclarationSyntax or MethodDeclarationSyntax or PropertyDeclarationSyntax or VariableDeclaratorSyntax).Select(n=>(node:n,symbol:model.GetDeclaredSymbol(n))).Where(x=>x.symbol is not null).Select(x=>new {name=x.symbol!.Name,kind=12,range=Range(tree,x.node.Span),selectionRange=Range(tree,x.node.Span)}).ToArray();break;
        case "textDocument/signatureHelp":
            var invocation=token.Parent?.AncestorsAndSelf().OfType<InvocationExpressionSyntax>().FirstOrDefault();
            var methods=invocation is null?[]:model.GetMemberGroup(invocation.Expression);
            result=new {signatures=methods.Select(s=>new {label=s.ToDisplayString(SymbolDisplayFormat.MinimallyQualifiedFormat)}).ToArray(),activeSignature=0,activeParameter=0};break;
        default:result=new {contents=symbol?.ToDisplayString()??""};break;
    }
    Console.Write(JsonSerializer.Serialize(result));
} catch(Exception e){Console.Error.WriteLine(e);Environment.ExitCode=1;}
