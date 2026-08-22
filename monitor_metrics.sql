-- Monitor Phase C allowed metrics (should see new rows)
SELECT 
    run_id, 
    step, 
    assemblies, 
    bindings, 
    avg_coherence, 
    growth_velocity 
FROM phasec_stats 
ORDER BY id DESC 
LIMIT 5;

-- Monitor Forbidden Metrics (should return no rows or unchanged count)
SELECT 'preference_memory' as table_name, COUNT(*) as row_count FROM preference_memory
UNION ALL
SELECT 'autonomy_credit_log', COUNT(*) FROM autonomy_credit_log
UNION ALL
SELECT 'goal_nodes', COUNT(*) FROM goal_nodes
UNION ALL
SELECT 'autonomy_modulation_log', COUNT(*) FROM autonomy_modulation_log;
