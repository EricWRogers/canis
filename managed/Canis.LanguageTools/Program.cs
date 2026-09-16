using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Xml.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;
using Microsoft.CodeAnalysis.Rename;
var cache=new CompilationCache();
if(args.Length==1 && args[0]=="--server") {
    while(await Console.In.ReadLineAsync() is { } line) {
        try { Console.WriteLine(JsonSerializer.Serialize(new {result=await Handle(JsonNode.Parse(line)!)})); }
        catch(Exception e) { Console.WriteLine(JsonSerializer.Serialize(new {error=e.Message})); }
    }
} else {
    try { Console.Write(JsonSerializer.Serialize(await Handle(JsonNode.Parse(File.ReadAllText(args[0]))!))); }
    catch(Exception e){Console.Write(e.Message);Console.Error.WriteLine(e);Environment.ExitCode=1;}
}
async Task<object> Handle(JsonNode request) {
    string path=Path.GetFullPath(request["path"]!.GetValue<string>()),text=request["text"]!.GetValue<string>();
    int byteCursor=Math.Clamp(request["cursor"]!.GetValue<int>(),0,Encoding.UTF8.GetByteCount(text));
    int position=Encoding.UTF8.GetString(Encoding.UTF8.GetBytes(text).AsSpan(0,byteCursor)).Length;
    var compilation=cache.Update(request,path,text);
    var trees=compilation.SyntaxTrees;
    var references=compilation.References;
    var tree=trees.Single(t=>t.FilePath==path);var model=compilation.GetSemanticModel(tree);var root=tree.GetRoot();
    object Point(SyntaxTree t,int offset) {var source=t.GetText();var p=source.Lines.GetLinePosition(offset);var line=source.Lines[p.Line];return new {line=p.Line,character=Encoding.UTF8.GetByteCount(source.ToString(TextSpan.FromBounds(line.Start,offset)))};}
    object Range(SyntaxTree t,TextSpan span)=>new {start=Point(t,span.Start),end=Point(t,span.End)};
    object Location(Location l)=>new {uri=new Uri(l.SourceTree!.FilePath).AbsoluteUri,range=Range(l.SourceTree,l.SourceSpan)};
    var token=root.FindToken(Math.Clamp(position-1,0,Math.Max(0,text.Length-1)));
    ISymbol? Symbol(SemanticModel semantic,SyntaxNode? node) {
        if(node is null)return null;
        return semantic.GetSymbolInfo(node).Symbol ?? semantic.GetDeclaredSymbol(node);
    }
    var symbol=Symbol(model,token.Parent);
    object result;
    switch(request["method"]!.GetValue<string>()) {
        case "diagnostics":result=new {diagnostics=model.GetDiagnostics().Where(d=>d.Location.IsInSource && d.Location.SourceTree==tree).Select(d=>new {range=Range(tree,d.Location.SourceSpan),message=d.GetMessage(),severity=d.Severity==DiagnosticSeverity.Error?1:2}).ToArray()};break;
        case "textDocument/completion":
            var trivia=root.FindTrivia(Math.Max(0,position-1),findInsideTrivia:true);
            if(trivia.IsKind(SyntaxKind.SingleLineCommentTrivia) || trivia.IsKind(SyntaxKind.MultiLineCommentTrivia) ||
               token.Parent?.AncestorsAndSelf().Any(n=>n is LiteralExpressionSyntax or InterpolatedStringTextSyntax)==true) {
                result=new {cursor=byteCursor,items=Array.Empty<object>()};break;
            }
            int prefixStart=position;
            while(prefixStart>0 && (char.IsLetterOrDigit(text[prefixStart-1]) || text[prefixStart-1]=='_'))--prefixStart;
            var prefix=text[prefixStart..position];
            INamespaceOrTypeSymbol? container=null;
            var access=token.Parent?.AncestorsAndSelf().OfType<MemberAccessExpressionSyntax>().FirstOrDefault();
            if(access is not null)container=model.GetTypeInfo(access.Expression).Type??model.GetSymbolInfo(access.Expression).Symbol as INamespaceOrTypeSymbol;
            var symbols=model.LookupSymbols(position,container,includeReducedExtensionMethods:true);
            var candidates=symbols.Where(s=>!s.IsImplicitlyDeclared && s.CanBeReferencedByName)
                .Select(s=>new {label=s.Name,insertText=SyntaxFacts.GetKeywordKind(s.Name)!=SyntaxKind.None?"@"+s.Name:s.Name,detail=s.ToDisplayString(SymbolDisplayFormat.MinimallyQualifiedFormat)}).ToList();
            if(access is null)candidates.AddRange(new[]{"public","private","protected","internal","class","void","int","float","bool","string","return","if","else","for","foreach","while","new","null","true","false","override","static","using","var","async","await"}.Select(k=>new {label=k,insertText=k,detail="C# keyword"}));
            result=new {cursor=byteCursor,items=candidates.Where(s=>s.label.StartsWith(prefix,StringComparison.OrdinalIgnoreCase)).GroupBy(s=>s.label).Select(g=>g.First())
                .OrderByDescending(s=>s.label.StartsWith(prefix,StringComparison.Ordinal)).ThenBy(s=>s.label,StringComparer.Ordinal).Take(150).ToArray()};break;
        case "textDocument/definition":
            result=(symbol?.Locations??[]).Where(l=>l.IsInSource).Select(Location).ToArray();break;
        case "textDocument/references":
            var found=new List<object>();if(symbol is not null)foreach(var t in trees){var m=compilation.GetSemanticModel(t);foreach(var n in t.GetRoot().DescendantNodes().OfType<IdentifierNameSyntax>())if(SymbolEqualityComparer.Default.Equals(m.GetSymbolInfo(n).Symbol,symbol))found.Add(Location(n.GetLocation()));}result=found;break;
        case "textDocument/documentSymbol":
            result=root.DescendantNodes().Where(n=>n is BaseTypeDeclarationSyntax or MethodDeclarationSyntax or PropertyDeclarationSyntax or VariableDeclaratorSyntax).Select(n=>(node:n,symbol:model.GetDeclaredSymbol(n))).Where(x=>x.symbol is not null).Select(x=>new {name=x.symbol!.Name,kind=12,range=Range(tree,x.node.Span),selectionRange=Range(tree,x.node.Span)}).ToArray();break;
        case "textDocument/signatureHelp":
            var invocation=token.Parent?.AncestorsAndSelf().OfType<InvocationExpressionSyntax>().FirstOrDefault();
            var methods=invocation is null?[]:model.GetMemberGroup(invocation.Expression);
            result=new {signatures=methods.Select(s=>new {label=s.ToDisplayString(SymbolDisplayFormat.MinimallyQualifiedFormat)}).ToArray(),activeSignature=0,activeParameter=invocation?.ArgumentList.Arguments.GetSeparators().Count(s=>s.SpanStart<position)??0};break;
        case "textDocument/rename":
            var newName=request["newName"]?.GetValue<string>()??"";
            if(!SyntaxFacts.IsValidIdentifier(newName))throw new InvalidOperationException("Enter a valid C# identifier.");
            using(var workspace=new AdhocWorkspace()) {
                var projectId=ProjectId.CreateNewId();
                var solution=workspace.CurrentSolution.AddProject(projectId,"Scripts","Scripts",LanguageNames.CSharp)
                    .WithProjectCompilationOptions(projectId,new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary))
                    .WithProjectMetadataReferences(projectId,references);
                DocumentId? activeId=null;
                foreach(var source in trees) {
                    var id=DocumentId.CreateNewId(projectId);
                    solution=solution.AddDocument(id,string.IsNullOrEmpty(source.FilePath)?"Globals.cs":Path.GetFileName(source.FilePath),source.GetText(),filePath:source.FilePath);
                    if(source.FilePath==path)activeId=id;
                }
                var document=solution.GetDocument(activeId!)!;
                var renameModel=(await document.GetSemanticModelAsync())!;
                var renameRoot=(await document.GetSyntaxRootAsync())!;
                var target=Symbol(renameModel,renameRoot.FindToken(token.SpanStart).Parent);
                if(target is null || !target.Locations.Any(l=>l.IsInSource))throw new InvalidOperationException("Select a source symbol to rename.");
                var renamed=await Renamer.RenameSymbolAsync(solution,target,new SymbolRenameOptions(),newName);
                var beforeCompilation=(await solution.GetProject(projectId)!.GetCompilationAsync())!;
                var afterCompilation=(await renamed.GetProject(projectId)!.GetCompilationAsync())!;
                var existingErrors=beforeCompilation.GetDiagnostics().Where(d=>d.Severity==DiagnosticSeverity.Error).Select(d=>d.Id+":"+d.GetMessage()).ToHashSet();
                var conflicts=afterCompilation.GetDiagnostics().Where(d=>d.Severity==DiagnosticSeverity.Error && !existingErrors.Contains(d.Id+":"+d.GetMessage())).ToArray();
                if(conflicts.Length>0)throw new InvalidOperationException("Rename conflicts: "+string.Join("; ",conflicts.Select(d=>d.GetMessage())));
                var edits=new List<object>();
                foreach(var id in renamed.GetChanges(solution).GetProjectChanges().SelectMany(p=>p.GetChangedDocuments())) {
                    var old=solution.GetDocument(id)!;var updated=renamed.GetDocument(id)!;
                    if(string.IsNullOrEmpty(old.FilePath))continue;
                    edits.Add(new {path=old.FilePath,before=(await old.GetTextAsync()).ToString(),text=(await updated.GetTextAsync()).ToString()});
                }
                result=new {name=newName,changes=edits};
            }
            break;
        default:result=new {contents=symbol?.ToDisplayString()??""};break;
    }
    return result;
}

sealed class CompilationCache {
    readonly MetadataReference[] platform=((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator).Select(p=>MetadataReference.CreateFromFile(p)).ToArray();
    readonly Dictionary<string,(string content,SyntaxTree tree)> sources=new();
    CSharpCompilation? compilation;
    string referenceKey="";
    public CSharpCompilation Update(JsonNode request,string path,string text) {
        var project=XDocument.Load(request["project"]!.GetValue<string>());
        var hints=project.Descendants("HintPath").Select(h=>Path.GetFullPath(h.Value)).ToArray();
        var key=string.Join("\n",hints.Select(h=>{var f=new FileInfo(h);return $"{h}:{f.Length}:{f.LastWriteTimeUtc.Ticks}";}));
        if(compilation is null || key!=referenceKey) {
            compilation=CSharpCompilation.Create("Canis.Language",references:platform.Concat(hints.Select(p=>MetadataReference.CreateFromFile(p))),options:new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
            sources.Clear();referenceKey=key;
        }
        var contents=project.Descendants("Compile").Select(c=>Path.GetFullPath(c.Attribute("Include")!.Value)).Distinct()
            .ToDictionary(p=>p,p=>p==path?text:request["overlays"]?[p]?.GetValue<string>()??File.ReadAllText(p));
        contents[path]=text;
        contents[""]="global using System; global using System.Collections.Generic; global using System.Linq; global using System.Threading; global using System.Threading.Tasks; global using System.IO;";
        foreach(var removed in sources.Keys.Except(contents.Keys).ToArray()) {
            compilation=compilation.RemoveSyntaxTrees(sources[removed].tree);sources.Remove(removed);
        }
        foreach(var (file,content) in contents) {
            if(sources.TryGetValue(file,out var old) && old.content==content)continue;
            // WithChangedText preserves reusable syntax nodes for the active buffer.
            var tree=old.tree is null?CSharpSyntaxTree.ParseText(content,path:file):old.tree.WithChangedText(SourceText.From(content));
            compilation=old.tree is null?compilation.AddSyntaxTrees(tree):compilation.ReplaceSyntaxTree(old.tree,tree);
            sources[file]=(content,tree);
        }
        return compilation;
    }
}
