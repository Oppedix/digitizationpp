"""Plot one ROOT TWIN record. Requires PyROOT; never used by digitization."""
import argparse
import ROOT

parser=argparse.ArgumentParser()
parser.add_argument('file')
parser.add_argument('--entry',type=int,default=0)
parser.add_argument('--output',default='twin-event.png')
args=parser.parse_args()
ROOT.gROOT.SetBatch(True)
source=ROOT.TFile.Open(args.file)
if not source or source.IsZombie(): raise SystemExit('Cannot open ROOT file')
tree=source.Get('event_info')
if not tree or args.entry<0 or args.entry>=tree.GetEntries(): raise SystemExit('Entry is unavailable')
tree.GetEntry(args.entry)
if not hasattr(tree,'ml_schema_version') or tree.ml_schema_version!=1: raise SystemExit('Unsupported TWIN schema')
print('parent:',tree.ml_parent_id,'valid:',bool(tree.ml_valid),'reason:',tree.ml_reject_reason)
canvas=ROOT.TCanvas('twin','TWIN truth (audit only) and observations',1500,2400)
canvas.Divide(3,6)
keep=[]
canvas.cd(1)
hits=ROOT.TPolyMarker3D(len(tree.ml_hit_x_mm))
for i,(x,y,z) in enumerate(zip(tree.ml_hit_x_mm,tree.ml_hit_y_mm,tree.ml_hit_z_mm)): hits.SetPoint(i,x,y,z)
hits.SetMarkerStyle(20);hits.Draw();keep.append(hits)
canvas.cd(2)
w,h=int(tree.obs_camera_width),int(tree.obs_camera_height)
image=ROOT.TH2D('camera','Observed camera counts;pixel x;pixel y',w,-.5,w-.5,h,-.5,h-.5)
if len(tree.obs_camera_counts)!=w*h: raise SystemExit('Invalid camera shape')
for y in range(h):
    for x in range(w): image.SetBinContent(x+1,y+1,tree.obs_camera_counts[y*w+x])
image.Draw('COLZ');keep.append(image)
settings=source.Get('twin_configuration')
if not settings: raise SystemExit('Missing detector metadata')
for i in range(8):
    for digitizer,pad in [('fast',3+2*i),('slow',4+2*i)]:
        canvas.cd(pad)
        values=getattr(tree,f'wav_{digitizer}_pmt{i+1}')
        frequency=float(settings.GetValue(f'{digitizer}_freq').GetString().Data())
        graph=ROOT.TGraph(len(values))
        for n,value in enumerate(values): graph.SetPoint(n,n/frequency*1e9,value)
        graph.SetTitle(f'{digitizer} PMT {i+1};trigger-relative time [ns];voltage [V]')
        graph.Draw('AL');keep.append(graph)
canvas.SaveAs(args.output)
