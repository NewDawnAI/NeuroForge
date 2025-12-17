import os
import sqlite3
import json


def main():
    root = os.path.dirname(os.path.abspath(__file__))
    db_path = os.path.join(root, 'smoke_phase_c.sqlite')
    if not os.path.exists(db_path):
        print(f"DB not found: {db_path}. Run smoke_language_agent.py first.")
        return

    con = sqlite3.connect(db_path)
    con.row_factory = sqlite3.Row

    def count(view):
        try:
            return con.execute(f"select count(*) as c from {view}").fetchone()[0]
        except Exception as e:
            print(f"{view}: ERROR -> {e}")
            return None

    def sample(view):
        try:
            row = con.execute(f"select * from {view} order by id desc limit 1").fetchone()
            if row is None:
                return None
            return {k: row[k] for k in row.keys()}
        except Exception as e:
            print(f"{view} sample: ERROR -> {e}")
            return None

    views = ['narrative_v', 'reward_v', 'plans_v', 'language_v']
    print(f"Probing {db_path}\n")
    for v in views:
        c = count(v)
        print(f"{v}: count = {c}")
        s = sample(v)
        if s is not None:
            # Pretty print compactly
            print(f"{v} sample: {json.dumps(s, ensure_ascii=False)}")
        else:
            print(f"{v} sample: <none>")
        print('-' * 60)

    con.close()


if __name__ == '__main__':
    main()