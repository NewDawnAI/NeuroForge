import argparse
import json
import sqlite3
import os
import sys

def main():
    parser = argparse.ArgumentParser(description="Compute correlations for Phase C run.")
    parser.add_argument("--db", required=True, help="Path to SQLite DB")
    parser.add_argument("--run-id", type=int, help="Run ID to analyze (defaults to latest)")
    parser.add_argument("--out", required=True, help="Output JSON path")
    
    args = parser.parse_args()
    
    db_path = args.db
    if not os.path.exists(db_path):
        print(f"Error: Database not found at {db_path}")
        sys.exit(1)
        
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    
    # Get run_id if not provided
    run_id = args.run_id
    if run_id is None:
        cursor.execute("SELECT MAX(id) FROM runs")
        row = cursor.fetchone()
        if row and row[0]:
            run_id = row[0]
        else:
            print("Error: No runs found in database.")
            conn.close()
            sys.exit(1)
            
    print(f"Analyzing Run ID: {run_id}")
    
    # Fetch learning stats
    cursor.execute("""
        SELECT step, avg_weight_change, consolidation_rate 
        FROM learning_stats 
        WHERE run_id = ? 
        ORDER BY step ASC
    """, (run_id,))
    learning_rows = cursor.fetchall()
    
    # Fetch reward log
    cursor.execute("""
        SELECT step, reward 
        FROM reward_log 
        WHERE run_id = ? 
        ORDER BY step ASC
    """, (run_id,))
    reward_rows = cursor.fetchall()
    
    conn.close()
    
    # Process data
    # Create dicts mapping step -> value
    weight_change_map = {row['step']: row['avg_weight_change'] for row in learning_rows}
    consolidation_map = {row['step']: row['consolidation_rate'] for row in learning_rows}
    reward_map = {row['step']: row['reward'] for row in reward_rows}
    
    # Find common steps
    common_steps = sorted(list(set(weight_change_map.keys()) & set(reward_map.keys())))
    
    if not common_steps:
        print("Warning: No overlapping steps between learning stats and reward log.")
        result = {
            "run_id": run_id,
            "reward_events": len(reward_rows),
            "reward_vs_avg_weight_change_r": 0.0,
            "best_lag_consolidation": 0,
            "error": "No overlapping steps"
        }
    else:
        # Align data
        rewards = [reward_map[s] for s in common_steps]
        weight_changes = [weight_change_map[s] for s in common_steps]
        consolidations = [consolidation_map[s] for s in common_steps if s in consolidation_map]
        
        # Compute Pearson correlation manually (to avoid pandas dependency if possible, but pandas is robust)
        # Using simple manual implementation for portability
        
        def mean(data):
            return sum(data) / len(data) if data else 0.0
            
        def correlation(x, y):
            n = len(x)
            if n != len(y) or n < 2:
                return 0.0
            
            mx = mean(x)
            my = mean(y)
            
            xm = [val - mx for val in x]
            ym = [val - my for val in y]
            
            num = sum(xi * yi for xi, yi in zip(xm, ym))
            den_x = sum(xi ** 2 for xi in xm)
            den_y = sum(yi ** 2 for yi in ym)
            
            if den_x == 0 or den_y == 0:
                return 0.0
                
            return num / ((den_x * den_y) ** 0.5)
            
        r_reward_weight = correlation(rewards, weight_changes)
        
        # Lag analysis for consolidation rate vs reward
        # We want to see if reward predicts consolidation (future) or consolidation reflects past reward.
        # Guide says "best lag for consolidation_rate".
        # We will check lags from -10 to +10 steps (assuming steps are aligned).
        # Wait, steps might not be contiguous. We should use list indices if steps are regular, or shift by step count.
        # Assuming regular logging interval.
        
        best_lag = 0
        best_lag_r = -1.0
        
        # Consolidations array might be slightly different size if keys missing, but assuming same steps
        # Use common_steps intersection with consolidation_map
        common_cons_steps = sorted(list(set(common_steps) & set(consolidation_map.keys())))
        if common_cons_steps:
            cons_aligned = [consolidation_map[s] for s in common_cons_steps]
            rews_aligned = [reward_map[s] for s in common_cons_steps]
            
            for lag in range(-5, 6): # Check lag -5 to +5 indices
                # Shift rews by lag
                # if lag > 0: reward happens, then consolidation (lag steps later) -> correlation(rew[:-lag], cons[lag:])
                # We want correlation(reward[t], consolidation[t+lag])
                
                if lag == 0:
                    r = correlation(rews_aligned, cons_aligned)
                elif lag > 0:
                    # reward leads consolidation
                    if len(rews_aligned) > lag:
                        r = correlation(rews_aligned[:-lag], cons_aligned[lag:])
                    else:
                        r = 0
                else:
                    # consolidation leads reward (negative lag)
                    abs_lag = abs(lag)
                    if len(rews_aligned) > abs_lag:
                        r = correlation(rews_aligned[abs_lag:], cons_aligned[:-abs_lag])
                    else:
                        r = 0
                
                if abs(r) > abs(best_lag_r):
                    best_lag_r = r
                    best_lag = lag
        
        result = {
            "run_id": run_id,
            "reward_events": len(reward_rows),
            "reward_vs_avg_weight_change_r": r_reward_weight,
            "best_lag_consolidation": best_lag,
            "best_lag_correlation": best_lag_r
        }

    # Write output
    with open(args.out, 'w') as f:
        json.dump(result, f, indent=2)
        
    print(f"Analysis complete. Results written to {args.out}")

if __name__ == "__main__":
    main()
