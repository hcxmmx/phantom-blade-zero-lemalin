"""Inspect skin weights in the independently exported GLB from the actual IoStore package."""
import argparse,struct,json
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('path');parser.add_argument('--before',action='store_true');args=parser.parse_args()
path=Path(args.path);blob=path.read_bytes();n,t=struct.unpack_from('<II',blob,12);doc=json.loads(blob[20:20+n]);offset=20+n
length,kind=struct.unpack_from('<II',blob,offset);data=memoryview(blob)[offset+8:offset+8+length]
def accessor(index):
 a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];formats={5121:'B',5123:'H',5125:'I',5126:'f'};fmt='<'+formats[a['componentType']]*4;size=struct.calcsize(fmt);start=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',size)
 for i in range(a['count']):yield struct.unpack_from(fmt,data,start+i*stride)
rootjoints={i for i,node in enumerate(doc['skins'][0]['joints']) if doc['nodes'][node].get('name') in {'root','LeMalin'}}
weights=[];sums=[];seen=set()
for mesh in doc['meshes']:
 for p in mesh['primitives']:
  attr=p['attributes'];key=(attr['JOINTS_0'],attr['WEIGHTS_0'])
  if key in seen:continue
  seen.add(key)
  for joints,ws in zip(accessor(key[0]),accessor(key[1])):
   weights.append(sum(w for j,w in zip(joints,ws) if j in rootjoints));sums.append(sum(ws))
if args.before:assert min(weights)>.49
else:assert max(weights)==0
assert all(abs(w-1)<.02 for w in sums)
report={'file':str(path),'vertices':len(weights),'vertices_with_root_weight':sum(w>0 for w in weights),'root_weight_range':[min(weights),max(weights)],'weight_sum_range':[min(sums),max(sums)],'expected':'before fix' if args.before else 'fixed','passed':True}
R=Path(__file__).resolve().parents[1];(R/('output/reports/cooked-weights-before.json' if args.before else 'output/reports/cooked-weights-fixed.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
