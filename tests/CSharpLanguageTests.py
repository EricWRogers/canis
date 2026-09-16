import atexit,json,subprocess,tempfile,sys,time,statistics
from pathlib import Path
from xml.sax.saxutils import escape
host,core=sys.argv[1:]
worker=subprocess.Popen(['dotnet',host,'--server'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
atexit.register(worker.kill)
cold_times=[];warm_times=[]
completion_times=[]
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);foo=root/'Foo.cs';bar=root/'Bar.cs'
 bar.write_text('public class Other { public int Value; }')
 text='// snowman ☃\nusing Canis; public class Probe { public void Run(Entity entity) { entity. } }'
 foo.write_text(text)
 project=root/'Game.csproj';project.write_text('<Project><ItemGroup>'+''.join(f'<Compile Include="{escape(str(f))}" />' for f in (foo,bar))+f'<Reference Include="Canis.Core"><HintPath>{escape(core)}</HintPath></Reference></ItemGroup></Project>')
 def request(method,text,offset,overlays=None,newName='',error=None):
  path=root/'request.json';path.write_text(json.dumps(dict(method=method,path=str(foo),text=text,cursor=len(text[:offset].encode()),project=str(project),overlays=overlays or {},newName=newName)))
  start=time.perf_counter()
  proc=subprocess.run(['dotnet',host,str(path)],capture_output=True,text=True)
  cold_times.append(time.perf_counter()-start)
  start=time.perf_counter()
  worker.stdin.write(path.read_text()+'\n');worker.stdin.flush()
  response=json.loads(worker.stdout.readline())
  warm_times.append(time.perf_counter()-start)
  if method=='textDocument/completion' and len(warm_times)>1:
   completion_times.append((cold_times[-1],warm_times[-1]))
  if error:
   assert proc.returncode!=0 and error in proc.stdout,(proc.stdout,proc.stderr)
   assert error in response['error'],response
   return
  assert proc.returncode==0,proc.stderr
  expected=json.loads(proc.stdout)
  assert response.get('result')==expected,(response,expected)
  return expected
 result=request('textDocument/completion',text,text.index('entity. }')+7)
 assert any(i['label']=='Transform' for i in result['items']),result
 text='using Canis; public class Probe { void Run(Entity entity) { entity.tr } }'
 result=request('textDocument/completion',text,text.index('entity.tr')+9)
 assert result['items'][0]['label']=='Transform' and all(i['label'].lower().startswith('tr') for i in result['items']),result
 text='using Canis; public class Probe { string text = "entity.tr"; }'
 assert not request('textDocument/completion',text,text.index('entity.tr')+9)['items']
 text='using Canis; public class Probe { // entity.tr\n }'
 assert not request('textDocument/completion',text,text.index('entity.tr')+9)['items']
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
 text='// snowman ☃\npublic class Probe { public int Value; int Read() => Value; string name = "Value"; }'
 other='public class Other { public int Value; int Read(Probe p) => p.Value + Value; }'
 result=request('textDocument/rename',text,text.index('Value')+2,{str(bar):other},'Score')
 changes={c['path']:c for c in result['changes']}
 assert changes[str(foo)]['before']==text
 assert 'int Score;' in changes[str(foo)]['text'] and '=> Score;' in changes[str(foo)]['text']
 assert '"Value"' in changes[str(foo)]['text']
 assert 'p.Score + Value' in changes[str(bar)]['text']
 assert changes[str(bar)]['before']==other
 request('textDocument/rename',text,text.index('Value')+2,{str(bar):other},'Read',error='Rename conflicts')
 request('textDocument/rename',text,text.index('Value')+2,{str(bar):other},'not valid',error='valid C# identifier')
 text='public class Probe { void Run(int a, int b) { Run(1, 2); } }'
 result=request('textDocument/signatureHelp',text,text.index('1, 2')+4)
 assert result['activeParameter']==1 and result['signatures'],result
 # Refresh disk contents, project membership, and removed unsaved overlays in a warm worker.
 bar.write_text('public class Other { public int Fresh; }')
 text='public class Probe { void Run(Other other) { other.Fr } }'
 result=request('textDocument/completion',text,text.index('other.Fr')+8)
 assert any(i['label']=='Fresh' for i in result['items']),result
 extra=root/'Extra.cs';extra.write_text('public class Added {}')
 project.write_text(project.read_text().replace('</ItemGroup>',f'<Compile Include="{escape(str(extra))}" /></ItemGroup>'))
 result=request('diagnostics','public class Probe { Added field; }',0)
 assert all(d['severity']!=1 for d in result['diagnostics']),result
 project.write_text(project.read_text().replace(f'<Compile Include="{escape(str(extra))}" />',''))
 assert any(d['severity']==1 for d in request('diagnostics','public class Probe { Added field; }',0)['diagnostics'])
 worker.stdin.close();assert worker.wait(timeout=10)==0
 atexit.unregister(worker.kill)
 print(f'One-shot median: {statistics.median(cold_times)*1000:.1f} ms; warm worker median: {statistics.median(warm_times[1:])*1000:.1f} ms')
 print(f'Completion median: {statistics.median(t[0] for t in completion_times)*1000:.1f} ms one-shot; {statistics.median(t[1] for t in completion_times)*1000:.1f} ms warm worker')
 print('C# completion, navigation, diagnostics, semantic rename, signatures, Unicode and unsaved overlays passed')
