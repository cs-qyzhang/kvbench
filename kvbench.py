#!/usr/bin/ipython3 --pdb
import os;
import json;

# open settings
settings = {};
if not os.path.exists("kvbench.json"):
    print("kvbench.json file not exist!");
    exit(-1);
with open("kvbench.json", 'r') as f:
    settings = json.loads(f.read());
    if settings == {}:
        print("kvbench.json file invalid!");
        exit(-1);

# create and open fifo pipe
try:
    os.mkfifo(settings["pipe"]);
except FileExistsError:
    pass;
print(settings["pipe"]);
pipe = open(settings["pipe"], 'rb');
pipe.closed();
print(settings["pipe"]);

# run bench
for bench in settings["bench"]:
    print("Start to run bench " + bench["name"]);
    if "preTask" in bench:
        res = os.system(bench["preTask"]);
    res = os.system(bench["task"]);
    if res != 0:
        print("Bench " + bench["name"] + " return " + res + "! Exit.");
        exit(-1);
    if "afterTask" in bench:
        res = os.system(bench["afterTask"]);