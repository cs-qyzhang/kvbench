#!/usr/bin/python3
import kvbench_pb2 as kvbench
from datetime import datetime
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
import cpuinfo
import distro
import psutil
import platform
import jinja2
import json
import os
import tarfile
import requests
import subprocess
import warnings
warnings.filterwarnings("ignore")


# https://www.thepythoncode.com/article/get-hardware-system-information-python
def get_size(bytes, suffix="B"):
    """
    Scale bytes to its proper format
    e.g:
        1253656 => '1.20MB'
        1253656678 => '1.17GB'
    """
    factor = 1024
    for unit in ["", "K", "M", "G", "T", "P"]:
        if bytes < factor:
            return f"{bytes:.2f}{unit}{suffix}"
        bytes /= factor


def human_readable(num):
    return "{:,}".format(int(num))


def get_us(num: float):
    return "{:0.4f}".format(num)


def draw_line_chart(x, data, labels, xlabel, ylabel, title, fig_name):
    fig, ax = plt.subplots()
    for i in range(len(data)):
        ax.plot(x, data[i], label=labels[i])

    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend()
    ax.set_axisbelow(True)
    ax.yaxis.grid(True, color='#EEEEEE')
    ax.xaxis.grid(False)
    fig.tight_layout()
    plt.savefig(fig_name + ".pgf")
    plt.savefig(fig_name + ".pdf")
    if settings["showFigure"]:
        plt.show()


def draw_bar_chart(data, bench_names, labels, ylabel, title, fig_name):
    bench_num = len(bench_names)
    fig, ax = plt.subplots()
    width = 0.9 / bench_num  # the width of the bars
    x = np.arange(len(labels))  # the label locations
    for i in range(bench_num):
        rect = ax.bar(x - (bench_num - 1) * width / 2.0 + i *
                      width, data[i], width, label=bench_names[i])

    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend()
    ax.set_axisbelow(True)
    ax.yaxis.grid(True, color='#EEEEEE')
    ax.xaxis.grid(False)
    fig.tight_layout()
    plt.savefig(fig_name + ".pgf")
    plt.savefig(fig_name + ".pdf")
    if settings["showFigure"]:
        plt.show()


# draw latency figure
def draw_latency_fig():
    # draw average latency bar chart
    bench_num = len(bench_stats);
    average_data = [[] for i in range(bench_num)];
    max_data = [[] for i in range(bench_num)];
    bench_names = [];
    for i in range(bench_num):
        for j in range(1, len(bench_stats[i].stat)):
            average_data[i].append(bench_stats[i].stat[j].average_latency);
            max_data[i].append(bench_stats[i].stat[j].max_latency);
        bench_names.append(settings["bench"][i]["name"]);
    labels = [];
    for phase in settings["phase"]:
        labels.append(phase["type"])
    ylabel = "Latency (us)"
    title = "Average Latency"
    fig_name = "latency-average"
    draw_bar_chart(average_data, bench_names, labels, ylabel, title, fig_name)

    title = "Maximum Latency"
    fig_name = "latency-max"
    draw_bar_chart(max_data, bench_names, labels, ylabel, title, fig_name)

    # draw phase latency line chart
    for i in range(1, len(bench_stats[0].stat)):
        data = []
        labels = [];
        x = [i+1 for i in range(len(bench_stats[0].stat[i].latency))];
        xlabel = "Operation";
        ylabel = "Latency (us)";
        for j in range(len(bench_stats)):
            data.append(bench_stats[j].stat[i].latency);
            labels.append(settings["bench"][j]["name"]);
        title = settings["phase"][i - 1]["type"];
        fig_name = "latency-phase-" + str(i);
        draw_line_chart(x, data, labels, xlabel, ylabel, title, fig_name);


def draw_throughput_fig():
    bench_num = len(bench_stats)
    data = [[] for i in range(bench_num)]
    bench_names = []
    for i in range(bench_num):
        for stat in bench_stats[i].stat:
            data[i].append(stat.throughput)
        bench_names.append(settings["bench"][i]["name"])
    labels = ["Total"]
    for phase in settings["phase"]:
        labels.append(phase["type"])
    ylabel = "Throughput (ops/s)"
    title = "Throughput"
    fig_name = "throughput"
    draw_bar_chart(data, bench_names, labels, ylabel, title, fig_name)


def archieve_file():
    tar_file = settings["name"] + '-' + settings["user"] + "-" + datetime.now().strftime("%Y-%-m-%-d-%H:%M:%-S") + ".tar.gz";
    with tarfile.open(tar_file, "w:gz") as tar:
        for name in ["report.tex", "latency-average.pdf", "latency-max.pdf", "throughput.pdf"]:
            tar.add(name);
        for i in range(len(settings["phase"])):
            tar.add("latency-phase-" + str(i + 1) + ".pdf");
    return tar_file;


def upload_file(file_name):
    if "uploadURL" in settings and settings["uploadURL"]:
        with open(file_name, 'rb') as f:
            url = settings["uploadURL"] + "?filename=" + file_name;
            print("uploading archieve file...");
            r = requests.post(url, data=f, headers={'Content-type': 'application/tar+gzip'});
            if r.status_code != 200:
                print("upload failed!");
                print(r.text);
            elif r.text[:2] == '-1':
                print("upload failed! server return -1!");
            else:
                print("upload success! you can get report PDF via: " + r.text);


# open settings
settings = {}
if not os.path.exists("kvbench.json"):
    print("kvbench.json file not exist!")
    exit(-1)
with open("kvbench.json", 'r') as f:
    settings = json.loads(f.read())

# get environment
environ = {}
environ["machine_type"] = platform.node()
environ["os_type"] = distro.name() + " " + distro.version()
cpu_info = cpuinfo.get_cpu_info()
environ["cpu_model"] = ", ".join(
    [cpu_info["brand_raw"], cpu_info["arch"], str(cpu_info["count"]) + " Core"])
mem = psutil.virtual_memory()
environ["ram_size"] = get_size(mem.total)
environ["kernel"] = platform.release()

bench_stats = []
tex_stats = []

# test whether task is a kvbench program
for bench in settings["bench"]:
    test_cmd = bench["task"] + " are-you-kvbench";
    result = subprocess.check_output(test_cmd, shell=True);
    if result != b'YES!\n':
        print("ERROR! task " + bench["task"] + " is not a kvbench program!!! exit.");
        exit(-1);

first = True;
# run bench
for bench in settings["bench"]:
    if (not first):
        print("\n");
    first = False;
    print("Start to run bench " + bench["name"])
    if "preTask" in bench:
        res = os.system(bench["preTask"])

    task_arg = "";
    for phase in settings["phase"]:
        task_arg += " " + phase["type"] + " " + str(phase["size"]);
    task_arg += " -thread " + str(settings["testThread"]);
    task = bench["task"] + task_arg;
    print("Run task: " + task);
    res = os.system(task)

    if res != 0:
        print("Bench " + bench["name"] + " return " + str(res) + "! Exit.")
        exit(-1)
    if "afterTask" in bench:
        res = os.system(bench["afterTask"])

    if not os.path.exists(settings["protoData"]):
        print(settings["protoData"] + " file not exist!")
        exit(-1)

    stats = kvbench.Stats()
    with open(settings["protoData"], 'rb') as f:
        stats.ParseFromString(f.read())
    bench_stats.append(stats)

    tex_stat = {}
    tex_stat["phases"] = []
    tex_stat["db"] = bench["name"]
    for i in range(len(stats.stat)):
        stat = stats.stat[i]
        phase = {}
        if i == 0:
            phase["name"] = "Total"
        else:
            phase["name"] = settings["phase"][i - 1]["type"]
        phase["throughput"] = human_readable(stat.throughput)
        phase["latency"] = get_us(stat.average_latency)
        phase["max_latency"] = get_us(stat.max_latency)
        tex_stat["phases"].append(phase)
    tex_stats.append(tex_stat)

draw_throughput_fig();
draw_latency_fig();

def us_max(us1, us2):
    if float(us1) > float(us2):
        return us1;
    else:
        return us2;

def us_min(us1, us2):
    if float(us1) < float(us2):
        return us1;
    else:
        return us2;

def human_readable_min(int1:str, int2:str):
    if int(int1.replace(',','')) < int(int2.replace(',','')):
        return int1;
    else:
        return int2;

def human_readable_max(int1, int2):
    if int(int1.replace(',','')) > int(int2.replace(',','')):
        return int1;
    else:
        return int2;

phase_max_min = []
for i in range(len(tex_stats[0]["phases"])):
    max_latency = tex_stats[0]["phases"][i]["latency"]
    min_latency = tex_stats[0]["phases"][i]["latency"]
    max_throughput = tex_stats[0]["phases"][i]["throughput"]
    min_throughput = tex_stats[0]["phases"][i]["throughput"]
    max_max_latency = tex_stats[0]["phases"][i]["max_latency"]
    min_max_latency = tex_stats[0]["phases"][i]["max_latency"]
    for j in range(len(tex_stats)):
        max_latency = us_max(max_latency, tex_stats[j]["phases"][i]["latency"])
        min_latency = us_min(min_latency, tex_stats[j]["phases"][i]["latency"])
        max_max_latency = us_max(max_max_latency, tex_stats[j]["phases"][i]["max_latency"])
        min_max_latency = us_min(min_max_latency, tex_stats[j]["phases"][i]["max_latency"])
        max_throughput = human_readable_max(
            max_throughput, tex_stats[j]["phases"][i]["throughput"])
        min_throughput = human_readable_min(
            min_throughput, tex_stats[j]["phases"][i]["throughput"])
    phase = {}
    phase["max_latency"] = max_latency
    phase["min_latency"] = min_latency
    phase["max_max_latency"] = max_max_latency
    phase["min_max_latency"] = min_max_latency
    phase["max_throughput"] = max_throughput
    phase["min_throughput"] = min_throughput
    phase_max_min.append(phase)

latex_jinja_env = jinja2.Environment(
    block_start_string='\BLOCK{',
    block_end_string='}',
    variable_start_string='\VAR{',
    variable_end_string='}',
    comment_start_string='\#{',
    comment_end_string='}',
    line_statement_prefix='%-',
    line_comment_prefix='%#',
    trim_blocks=True,
    lstrip_blocks=True,
    autoescape=False,
    loader=jinja2.FileSystemLoader(os.path.abspath('.'))
)

template = latex_jinja_env.get_template(settings["texTemplate"])

render_dict = {}
render_dict["now"] = datetime.now().strftime("%Y 年 %-m 月 %-d 日 %H:%M")
render_dict["user"] = settings["user"]
render_dict["title"] = settings["name"]
render_dict["environ"] = environ
render_dict["stats"] = tex_stats
render_dict["phase_max_min"] = phase_max_min
render_dict["benchs"] = settings["bench"];
render_dict["phases"] = settings["phase"];
for phase in render_dict["phases"]:
    phase["size"] = human_readable(phase["size"]);

document = template.render(**render_dict)

with open("report.tex", 'w') as output:
    output.write(document)

if settings["compileTeX"]:
    print("compiling pdf...");
    os.system("latexmk -silent " + "report.tex");
    print("pdf compiled");

arch_file = archieve_file();
print("achieve file: " + arch_file);
upload_file(arch_file);