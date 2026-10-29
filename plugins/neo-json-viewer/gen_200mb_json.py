import json, os
path = r"C:\Sources\NeoJsonViewer\test_200mb.json"
target = 200 * 1024 * 1024
payload = "x" * 900
count = 0
with open(path, "w", encoding="utf-8") as f:
    f.write("[")
    first = True
    while f.tell() < target - 2:
        obj = {"id": count, "name": f"item_{count}", "payload": payload, "flag": (count % 2 == 0)}
        s = json.dumps(obj, ensure_ascii=False, separators=(",", ":"))
        if not first:
            f.write(",")
        f.write(s)
        first = False
        count += 1
    f.write("]")
size = os.path.getsize(path)
print(path)
print(size)
print(count)
