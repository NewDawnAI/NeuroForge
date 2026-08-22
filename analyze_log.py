
import csv
import sys

csv_path = r'c:\Users\ashis\Desktop\NeuroForge\unified_bounded_log.csv'

with open(csv_path, 'r') as f:
    reader = csv.DictReader(f)
    print("Columns:", reader.fieldnames)
    
    count = 0
    for row in reader:
        step = int(row['step'])
        if step < 200: continue # Skip warmup
        
        obs_action = row['action']
        wm_action = row['wm_suggest_action']
        match = row['wm_action_match']
        
        if match == '0':
            if obs_action == wm_action:
                print(f"STRANGE MISMATCH at step {step}: obs={obs_action} wm={wm_action} match={match}")
            else:
                if count < 5:
                    print(f"Mismatch at step {step}: obs={obs_action} ({row['score']}) vs wm={wm_action} ({row['wm_suggest_score']})")
                    print(f"  Obs pred: dist={row['pred_dist_after']} col={row['pred_collision']}")
                    print(f"  WM pred: dist={row['wm_suggest_pred_dist']} col={row['wm_suggest_pred_collision']}")
            count += 1
            
        if count > 20: break
