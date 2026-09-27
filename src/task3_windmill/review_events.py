"""Produce reproducible switch-event contact sheets, plus a machine-readable index."""
import csv,json,os
import cv2,numpy as np
out='result/task3_windmill/validation';events=[]
for name in ['task_3','task_4']:
 rows=list(csv.DictReader(open(f'result/task3_windmill/{name}/frames.csv')))
 cap=cv2.VideoCapture(f'result/task3_windmill/{name}/recognition_overlay.mp4')
 es=[int(r['frame']) for r in rows if r['reason']];panels=[]
 for event in es:
  tiles=[]
  for off in [-13,-3,0,3,12]:
   k=max(0,min(len(rows)-1,event+off));r=rows[k];cap.set(1,k);ok,im=cap.read();assert ok
   if r['rx']:
    cx,cy=float(r['rx']),float(r['ry']);x=int(np.clip(cx-320,0,im.shape[1]-640));y=int(np.clip(cy-320,0,im.shape[0]-640));im=im[y:y+640,x:x+640]
   im=cv2.resize(im,(300,300));tile=np.zeros((350,300,3),np.uint8);tile[50:]=im
   cv2.putText(tile,f"f{k} ID{r['selected_id']} {r['status']}",(4,19),0,.48,(255,255,255),1)
   cv2.putText(tile,f"event f{event} ({event/30:.3f}s)",(4,40),0,.45,(0,255,255),1);tiles.append(tile)
  panels.append(np.hstack(tiles));events.append({'video':name,'frame':event,'seconds':event/30,'id':rows[event]['selected_id'],'reason':rows[event]['reason'],'sheet':f'{name}_events_{(len(panels)-1)//3}.jpg'})
 for start in range(0,len(panels),3):cv2.imwrite(f'{out}/{name}_events_{start//3}.jpg',np.vstack(panels[start:start+3]))
 cap.release()
open(out+'/events.json','w').write(json.dumps(events,indent=2));print(json.dumps(events,indent=2))
# Full-resolution source/candidate pairs for user review of difficult transitions.
focus={'task_3':[459,461,537,539,630,632,703,704,707], 'task_4':[101,113,162,174,527,540,630,1215,1436]}
focus_index=[]
for name,indices in focus.items():
 source=cv2.VideoCapture('resources/'+name+'.mp4')
 frame_rows=list(csv.DictReader(open(f'result/task3_windmill/{name}/frames.csv')))
 candidate_rows=list(csv.DictReader(open(f'result/task3_windmill/{name}/candidates.csv')))
 for k in indices:
  source.set(1,k);ok,original=source.read();assert ok
  marked=original.copy();r=frame_rows[k]
  if r['rx']:cv2.drawMarker(marked,(round(float(r['rx'])),round(float(r['ry']))),(0,255,0),0,15,2)
  for o in candidate_rows:
   if int(o['frame'])!=k:continue
   e=((float(o['x']),float(o['y'])),(float(o['width']),float(o['height'])),float(o['angle']))
   cv2.ellipse(marked,e,(0,255,0) if o['valid']=='1' else (255,100,0),2)
   text=f"{o['index']} A={o['valid']} peaks={o['arrow_peaks']} ring={float(o['ring_contrast']):.2f}"
   cv2.putText(marked,text,(round(e[0][0])-40,round(e[0][1])-45),0,.45,(255,255,255),1)
  cv2.putText(marked,f"{name} f{k} t={k/30:.3f}s ID{r['selected_id']} {r['status']}",(25,35),0,.7,(0,255,255),2)
  raw=f'{name}_{k}_original.jpg';annotation=f'{name}_{k}_candidates.jpg';cv2.imwrite(out+'/'+raw,original);cv2.imwrite(out+'/'+annotation,marked)
  focus_index.append({'video':name,'frame':k,'seconds':k/30,'original':raw,'candidates':annotation,'status':r['status'],'selected_id':r['selected_id']})
 source.release()
open(out+'/manual_review.json','w').write(json.dumps(focus_index,indent=2))
