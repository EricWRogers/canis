import json,subprocess,tempfile,sys
from pathlib import Path
from xml.sax.saxutils import escape
host,core=sys.argv[1:]
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);foo=root/'Foo.cs';bar=root/'Bar.cs'
 bar.write_text('public class Other { public int Value; }')
 text='// snowman ☃\nusing Canis; public class Probe { public void Run(Entity entity) { entity. } }'
 foo.write_text(text)
 project=root/'Game.csproj';project.write_text('<Project><ItemGroup>'+''.join(f'<Compile Include="{escape(str(f))}" />' for f in (foo,bar))+f'<Reference Include="Canis.Core"><HintPath>{escape(core)}</HintPath></Reference></ItemGroup></Project>')
 def request(method,text,offset,overlays=None):
  path=root/'request.json';path.write_text(json.dumps(dict(method=method,path=str(foo),text=text,cursor=len(text[:offset].encode()),project=str(project),overlays=overlays or {})))
  proc=subprocess.run(['dotnet',host,str(path)],capture_output=True,text=True);assert proc.returncode==0,proc.stderr
  return json.loads(proc.stdout)
 result=request('textDocument/completion',text,text.index('entity. }')+7)
 assert any(i['label']=='Transform' for i in result['items']),result
 text='using Canis; public class Probe { Other field = new Other(); }'
 result=request('textDocument/definition',text,text.index('Other')+3)
 assert result and result[0]['uri']==bar.as_uri(),result
 result=request('diagnostics','public class Probe { unknown field; }',0)
 assert result['diagnostics'],result
 text='public class Probe { public int Value; }'
 result=request('textDocument/documentSymbol',text,0)
 assert any(i['name']=='Value' for i in result),result
 # An unsaved second file participates in the semantic model.
 text='public class Probe { Other field; }'
 result=request('diagnostics',text,0,{str(bar):'public class RenamedOther {}'})
 assert any('Other' in d['message'] for d in result['diagnostics']),result
 print('C# Roslyn completion, navigation, diagnostics, Unicode and unsaved overlays passed')
