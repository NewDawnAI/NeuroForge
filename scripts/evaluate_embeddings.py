import csv
import math
import os
from typing import List

def read_vectors(path: str) -> List[List[float]]:
    vs = []
    with open(path, 'r', encoding='utf-8') as f:
        r = csv.reader(f)
        next(r, None)
        for row in r:
            vs.append([float(x) for x in row])
    return vs

def dot(a: List[float], b: List[float]) -> float:
    return sum(x*y for x, y in zip(a, b))

def recall_at_k(students: List[List[float]], teachers: List[List[float]], k: int) -> float:
    hits = 0
    for i, s in enumerate(students):
        sims = [(dot(s, t), j) for j, t in enumerate(teachers)]
        sims.sort(key=lambda x: x[0], reverse=True)
        topk = [j for _, j in sims[:k]]
        if i < len(teachers) and i in topk:
            hits += 1
    return hits / max(1, len(students))

def main():
    out_dir = os.environ.get('DA_OUT', 'Artifacts/CSV/run_latest')
    # Placeholder: students.csv and teachers.csv expected; each row is embedding
    students_path = os.path.join(out_dir, 'students.csv')
    teachers_path = os.path.join(out_dir, 'teachers.csv')
    if not (os.path.exists(students_path) and os.path.exists(teachers_path)):
        print('Missing students/teachers embeddings CSV')
        return
    students = read_vectors(students_path)
    teachers = read_vectors(teachers_path)
    r1 = recall_at_k(students, teachers, 1)
    r5 = recall_at_k(students, teachers, 5)
    print(f'recall@1={r1:.3f} recall@5={r5:.3f}')

if __name__ == '__main__':
    main()

