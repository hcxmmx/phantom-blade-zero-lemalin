"""Inspect the installed Test04 sample's local joint timing; no model or package changes."""
import json,math,numpy as np
from pathlib import Path
R=Path(__file__).resolve().parents[1];O=R/'output/hair-investigation'
d=json.loads((R/'output/lively/postprocess.json').read_text())['target'];names=d['bones'];parents=d['parents'];ref=d['reference_global']
def mul(a,b):
 x,y,z,w=a;X,Y,Z,W=b
 return np.array([w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z])
def inv(a):return np.array([-a[0],-a[1],-a[2],a[3]])/np.dot(a,a)
report={'fps':30,'production_files_modified':False,'sides':{}}
for side,parity in [('L',0),('R',1)]:
 signals=[];rows=[]
 for segment in range(1,7):
  name=f'aux_{segment*2+parity:03d}';i=names.index(name);p=parents[i];r=mul(inv(ref[p][3:7]),ref[i][3:7]);angles=[]
  for f in d['frames']:
   q=mul(inv(r),mul(inv(f[p][3:7]),f[i][3:7]));q=q/np.linalg.norm(q);angles.append(2*math.degrees(math.acos(min(1,abs(q[3])))))
  gain=.22+.10*(segment-1);signal=np.array(angles)/gain;signals.append(signal);rows.append({'segment':segment,'game_bone':name,'max_local_bend_deg':max(angles),'gain':gain})
 matrix=np.corrcoef(signals);lags=[]
 for signal in signals[1:]:
  a=signals[0]-np.mean(signals[0]);b=signal-signal.mean();best=max(range(-15,16),key=lambda lag:np.dot(a[max(0,lag):len(a)+min(0,lag)],b[max(0,-lag):len(b)-max(0,lag)]));lags.append(best)
 report['sides'][side]={'segments':rows,'min_pairwise_magnitude_correlation':float(matrix.min()),'best_lag_frames_vs_first_segment':lags}
(O/'current-driver-phase.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
