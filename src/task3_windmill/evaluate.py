"""Evaluate frozen, visually reviewed reference ellipses. Run from repository root."""
import csv,json,hashlib
from collections import defaultdict
import cv2,numpy as np
ref_path='config/windmill_reference.json'
refs=json.load(open(ref_path))['frames'];pred=defaultdict(list)
for r in csv.DictReader(open('result/task3_windmill/validation/predictions.csv')):pred[(r['video'],int(r['frame']))].append(r)
def outline(e):
 x,y,w,h,a=e;t=np.linspace(0,2*np.pi,360,endpoint=False);u=w/2*np.cos(t);v=h/2*np.sin(t);a=np.deg2rad(a)
 return np.column_stack((x+u*np.cos(a)-v*np.sin(a),y+u*np.sin(a)+v*np.cos(a)))
reports={};failures=[]
for name in ['task_3','task_4']:
 tp=fp=fn=0;centers=[];contours=[];rerrors=[]
 for r in refs:
  if r['video']!=name or r['ignore']:continue
  rows=pred[(name,r['frame'])];ps=[[float(z[k]) for k in ['x','y','width','height','angle']] for z in rows if z['valid']=='1'];gs=r['targets'];used=set()
  if r['center'] and rows and rows[0]['rx']:rerrors.append(float(np.linalg.norm(np.array(r['center'])-[float(rows[0]['rx']),float(rows[0]['ry'])])))
  for g in gs:
   rad=(g[2]+g[3])/4;dist=[np.linalg.norm(np.array(p[:2])-g[:2]) if i not in used else 1e9 for i,p in enumerate(ps)]
   j=int(np.argmin(dist)) if dist else -1
   if j<0 or dist[j]>rad*.5:fn+=1;failures.append([name,r['frame'],'miss',g[:2]]);continue
   used.add(j);tp+=1;ce=dist[j]/rad;centers.append(ce);a,b=outline(g),outline(ps[j]);dd=np.linalg.norm(a[:,None]-b[None,:],axis=2);err=float((dd.min(0).mean()+dd.min(1).mean())/2/rad);contours.append(err)
   if ce>.1 or err>.1:failures.append([name,r['frame'],'localization',round(ce,4),round(err,4)])
  fp+=len(ps)-len(used)
  if len(ps)>len(used):failures.append([name,r['frame'],'false_positive',[p[:2] for i,p in enumerate(ps) if i not in used]])
 reports[name]={'tp':tp,'fp':fp,'fn':fn,'precision':tp/max(1,tp+fp),'recall':tp/max(1,tp+fn),'center_error_radius_mean':float(np.mean(centers)),'center_error_radius_max':float(max(centers,default=0)),'outline_error_radius_mean':float(np.mean(contours)),'outline_error_radius_max':float(max(contours,default=0)),'R_center_error_pixels_max':max(rerrors,default=0)}
result={'reference_sha256':hashlib.sha256(open(ref_path,'rb').read()).hexdigest(),'annotation_limit':'Visually reviewed algorithm-assisted annotations; not independent human ground truth. Used for development, not a held-out test.','metrics':reports,'failures':failures}
open('result/task3_windmill/validation/metrics.json','w').write(json.dumps(result,indent=2));print(json.dumps(result,indent=2))

assert all(m["precision"]>=.95 and m["recall"]>=.95 and m["center_error_radius_max"]<=.10 and m["outline_error_radius_max"]<=.10 for m in reports.values()), "Reference acceptance gate failed; inspect metrics.json"
