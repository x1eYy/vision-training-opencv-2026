"""Full decode, frame order/content, timestamps, CSV and tracking invariants."""
import cv2,csv,json,subprocess,os
import numpy as np
from pathlib import Path
reports={}
for name in ['task_3','task_4']:
 folder=Path('result/task3_windmill')/name
 source=cv2.VideoCapture('resources/'+name+'.mp4');fps=source.get(5);size=(int(source.get(3)),int(source.get(4)));n=int(source.get(7))
 rows=list(csv.DictReader(open(folder/'frames.csv')));assert [int(r['frame']) for r in rows]==list(range(n));assert all(abs(float(r['time'])-i/fps)<1e-5 for i,r in enumerate(rows))
 for r in rows:
  if r['status']=='detected':assert r['center_valid']=='1' and r['angle'] and int(r['selected_id'])>0
  else:assert not r['angle']
 movies={};previous_id=0
 for filename in ['recognition_overlay.mp4','binary_process.mp4']:
  path=folder/filename;cap=cv2.VideoCapture(str(path));assert abs(cap.get(5)-fps)<1e-6 and (int(cap.get(3)),int(cap.get(4)))==size
  source.set(1,0);count=0;errors=[]
  while True:
   ok,frame=cap.read()
   if not ok:break
   assert frame.shape[:2]==size[::-1]
   if filename.startswith('recognition'):
    good,original=source.read();assert good
    # Mean absolute luminance difference after removing annotation-colored pixels.
    gray=cv2.cvtColor(frame,cv2.COLOR_BGR2GRAY).astype(float);ref=cv2.cvtColor(original,cv2.COLOR_BGR2GRAY).astype(float)
    diff=np.abs(gray-ref);diff[:115]=0
    # Trim the most changed 2% (drawn outlines/labels); retains original scene content.
    cutoff=np.quantile(diff,.98);errors.append(float(diff[diff<=cutoff].mean()))
   count+=1
  cap.release();assert count==n,(filename,count,n)
  p=json.loads(subprocess.check_output([os.environ.get('FFPROBE','ffprobe'),'-v','error','-select_streams','v:0','-show_entries','frame=best_effort_timestamp_time','-of','json',str(path)]))
  pts=[float(f['best_effort_timestamp_time']) for f in p['frames']];assert len(pts)==n and all(abs(t-i/fps)<1e-5 for i,t in enumerate(pts))
  movies[filename]={'decoded_frames':count,'fps':fps,'resolution':list(size),'timestamps_in_order':True,'source_correspondence_trimmed_MAE_max':max(errors) if errors else None}
  if errors:assert max(errors)<3.0
 trackrows=list(csv.DictReader(open(folder/'tracks.csv')));byframe={}
 for r in trackrows:byframe.setdefault(int(r['frame']),[]).append(r)
 for i,r in enumerate(rows):
  observed=[t for t in byframe.get(i,[]) if t['observed']=='1'];coords=[(t['relative_x'],t['relative_y']) for t in observed];assert len(coords)==len(set(coords))
  id=int(r['selected_id'])
  if previous_id and id!=previous_id:assert r['reason']
  if id:assert any(int(t['id'])==id and t['retired']=='0' for t in byframe[i])
  previous_id=id
 reports[name]={'movies':movies,'csv_rows':len(rows),'tracking_invariants_passed':True}
 source.release()
 print(name,'verified',flush=True)
Path('result/task3_windmill/validation/video_integrity.json').write_text(json.dumps(reports,indent=2))

continuity=[]
for interval in json.load(open('config/windmill_tracking_reference.json'))['intervals']:
 name=interval['video'];rows=list(csv.DictReader(open(f'result/task3_windmill/{name}/frames.csv')))[interval['start']:interval['end']+1]
 ids={int(r['selected_id']) for r in rows};assert len(ids)==1 and 0 not in ids
 angles=[(int(r['frame']),float(r['angle'])) for r in rows if r['angle']]
 steps=[abs(np.arctan2(np.sin(b-a),np.cos(b-a)))/(j-i) for (i,a),(j,b) in zip(angles,angles[1:])]
 assert max(steps,default=0)<.2, 'Possible identity jump in annotated interval'
 continuity.append(dict(interval,selected_id=ids.pop(),id_changes=0,max_observed_angle_step_rad_per_frame=max(steps,default=0)))
Path('result/task3_windmill/validation/continuity.json').write_text(json.dumps(continuity,indent=2))
