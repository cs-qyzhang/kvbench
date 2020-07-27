#!/usr/bin/python3

# smtp
import email
import smtplib
from email.header import Header
from email.mime.application import MIMEApplication
from email.mime.base import MIMEBase
from email.mime.image import MIMEImage
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

from datetime import datetime
import platform
import json
import time
import os
import sys
import tarfile
import subprocess
import warnings

import kvbench_pb2 as kvbench
import numpy as np
import matplotlib.pyplot as plt
import jinja2
import requests

# ignore matplotlib warnings
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


def get_duration(num: float):
    return "{:0.2f}".format(num / 1000000.0)


def get_us(num: float):
    return "{:0.4f}".format(num)


def us_max(us1, us2):
    if float(us1) > float(us2):
        return us1
    else:
        return us2


def us_min(us1, us2):
    if float(us1) < float(us2):
        return us1
    else:
        return us2


def human_readable_min(int1: str, int2: str):
    if int(int1.replace(',', '')) < int(int2.replace(',', '')):
        return int1
    else:
        return int2


def human_readable_max(int1, int2):
    if int(int1.replace(',', '')) > int(int2.replace(',', '')):
        return int1
    else:
        return int2


# https://stackoverflow.com/a/23378780/7640227
def get_physical_cores():
    return os.popen("lscpu -p | egrep -v '^#' | sort -u -t, -k 2,4 | wc -l").read().strip()


def get_logical_cores():
    return os.popen("lscpu -p | egrep -v '^#' | wc -l").read().strip()


def get_cpu_cores():
    physical_cores = int(get_physical_cores())
    logical_cores = int(get_logical_cores())
    return '{0}x{1}'.format(int(logical_cores / physical_cores), physical_cores)


def get_cpu_model():
    raw = os.popen(
        "cat /proc/cpuinfo | grep 'model name' | uniq").read().strip()
    return raw.split(':')[1].strip()


# https://stackoverflow.com/a/20348977/7640227
def get_memory_size():
    mem_kb = int(
        os.popen("awk '/MemTotal/ {print $2}' /proc/meminfo").read().strip())
    return get_size(mem_kb * 1024)


def get_disk_size():
    return os.popen("df -Ph | grep /dev/pmem0 | awk '{print $2}'").read().strip()


# https://www.howtoforge.com/how_to_find_out_about_your_linux_distribution
def get_os_name():
    raw = os.popen("cat /etc/os-release | grep PRETTY_NAME").read().strip()
    return raw.split('=')[1][1:-1]


def draw_line_chart(x, data, labels, xlabel, ylabel, title, fig_name, y_limit=0.0):
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
    if y_limit != 0.0:
        plt.ylim((0.0, y_limit))
    fig.tight_layout()
    if settings["generatePGF"]:
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
    if settings["generatePGF"]:
        plt.savefig(fig_name + ".pgf")
    plt.savefig(fig_name + ".pdf")
    if settings["showFigure"]:
        plt.show()


# draw latency figure
def draw_latency_fig():
    # draw average latency bar chart
    bench_num = len(bench_stats)
    average_data = [[] for i in range(bench_num)]
    max_data = [[] for i in range(bench_num)]
    bench_names = []
    for i in range(bench_num):
        for j in range(1, len(bench_stats[i].stat)):
            average_data[i].append(bench_stats[i].stat[j].average_latency)
            max_data[i].append(bench_stats[i].stat[j].max_latency)
        bench_names.append(settings["bench"][i]["name"])
    labels = []
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
        labels = []
        x = [i+1 for i in range(len(bench_stats[0].stat[i].latency))]
        xlabel = "Operation"
        ylabel = "Latency (us)"
        max_latency = 0.0
        x_dim = len(bench_stats[0].stat[i].latency)
        for j in range(len(bench_stats)):
            x_dim = min(x_dim, len(bench_stats[j].stat[i].latency))
        x = [i+1 for i in range(x_dim)]
        for j in range(len(bench_stats)):
            max_latency = max(max_latency, max(bench_stats[j].stat[i].latency))
            data.append(bench_stats[j].stat[i].latency[:x_dim])
            labels.append(settings["bench"][j]["name"])
        title = settings["phase"][i - 1]["type"]
        fig_name = "latency-phase-" + str(i)
        y_limit = 0.0
        if max_latency > 100.0:
            y_limit = 100.0
        draw_line_chart(x, data, labels, xlabel, ylabel,
                        title, fig_name, y_limit)


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
    tar_file = settings["name"].replace(' ', '-') + '-' + settings["user"].replace(
        ' ', '-') + "-" + datetime.now().strftime("%Y-%-m-%-d-%H:%M:%-S") + ".tar.gz"
    with tarfile.open(tar_file, "w:gz") as tar:
        for name in ["report.tex", "latency-average.pdf", "latency-max.pdf", "throughput.pdf"]:
            tar.add(name)
        for i in range(len(settings["phase"])):
            tar.add("latency-phase-" + str(i + 1) + ".pdf")
    return tar_file


def upload_file(file_name):
    if "uploadURL" in settings and settings["uploadURL"]:
        with open(file_name, 'rb') as f:
            url = settings["uploadURL"] + "?filename=" + file_name
            print("uploading archieve file...")
            r = requests.post(url, data=f, headers={
                              'Content-type': 'application/tar+gzip'})
            if r.status_code != 200:
                print("upload failed!")
                print(r.text)
            elif r.text[:2] == '-1':
                print("upload failed! server return -1!")
            else:
                print("upload success! you can get report PDF via: " + r.text)
                return r.text
    return ''


# https://help.aliyun.com/document_detail/29453.html?spm=a2c4g.11186623.6.643.546f28c1GGZaIR
def send_mail(pdf_url):
    if not settings["mail"]["enable"]:
        return;

    # 发件人地址，通过控制台创建的发件人地址
    username = settings["mail"]["sender"]
    # 发件人密码，通过控制台创建的发件人密码
    password = settings["mail"]["password"]
    # 自定义的回复地址
    replyto = settings["mail"]["replyTo"]
    # 收件人地址或是地址列表，支持多个收件人，最多30个
    receivers = settings["mail"]["receivers"]
    rcptto = ','.join(receivers)

    subject = settings["name"] + "测试"
    nick_name = settings["mail"]["senderNickName"]
    html = '<h1>' + subject + '</h1>\n<hr>\n' + pdf_url

    # 构建alternative结构
    msg = MIMEMultipart('alternative')
    msg['Subject'] = Header(subject).encode()
    msg['From'] = '%s <%s>' % (Header(nick_name).encode(), username)
    msg['To'] = rcptto
    msg['Reply-to'] = replyto
    msg['Message-id'] = email.utils.make_msgid()
    msg['Date'] = email.utils.formatdate()
    # 构建alternative的text/plain部分
    # textplain = MIMEText('自定义TEXT纯文本部分', _subtype='plain', _charset='UTF-8')
    # msg.attach(textplain)
    # 构建alternative的text/html部分
    texthtml = MIMEText(html, _subtype='html', _charset='UTF-8')
    msg.attach(texthtml)
    # 发送邮件
    try:
        client = smtplib.SMTP()
        # python 2.7以上版本，若需要使用SSL，可以这样创建client
        #client = smtplib.SMTP_SSL()
        # SMTP普通端口为25或80
        client.connect('smtpdm.aliyun.com', 80)
        # 开启DEBUG模式
        client.set_debuglevel(0)
        client.login(username, password)
        # 发件人和认证地址必须一致
        # 备注：若想取到DATA命令返回值,可参考smtplib的sendmaili封装方法:
        #      使用SMTP.mail/SMTP.rcpt/SMTP.data方法
        # client.sendmail(username, rcptto, msg.as_string())
        # 支持多个收件人
        client.sendmail(username, receivers, msg.as_string())
        client.quit()
        print('邮件发送成功！')
    except smtplib.SMTPConnectError as e:
        print('邮件发送失败，连接失败:', e.smtp_code, e.smtp_error)
    except smtplib.SMTPAuthenticationError as e:
        print('邮件发送失败，认证错误:', e.smtp_code, e.smtp_error)
    except smtplib.SMTPSenderRefused as e:
        print('邮件发送失败，发件人被拒绝:', e.smtp_code, e.smtp_error)
    except smtplib.SMTPRecipientsRefused as e:
        print('邮件发送失败，收件人被拒绝:', e.smtp_code, e.smtp_error)
    except smtplib.SMTPDataError as e:
        print('邮件发送失败，数据接收拒绝:', e.smtp_code, e.smtp_error)
    except smtplib.SMTPException as e:
        print('邮件发送失败, ', e.message)
    except Exception as e:
        print('邮件发送异常, ', str(e))


# open settings
settings = {}
json_file = "kvbench.json"
if len(sys.argv) >= 2:
    json_file = sys.argv[1]
if not os.path.exists(json_file):
    print(json_file + " file not exist!")
    exit(-1)
with open(json_file, 'r') as f:
    settings = json.loads(f.read())

# get environment
environ = {}
environ["machine_type"] = platform.node()
environ["os_name"] = get_os_name()
environ["cpu_model"] = ", ".join([get_cpu_model(), get_cpu_cores() + " Cores"])
environ["ram_size"] = get_memory_size()
environ["kernel"] = platform.release()
environ["disk_model"] = get_disk_size()  # TODO

bench_stats = []
tex_stats = []

# test whether task is a kvbench program
for bench in settings["bench"]:
    test_cmd = bench["task"] + " are-you-kvbench"
    result = subprocess.check_output(test_cmd, shell=True)
    if result != b'YES!\n':
        print("ERROR! task " + bench["task"] +
              " is not a kvbench program!!! exit.")
        exit(-1)

first = True
# run bench
for bench in settings["bench"]:
    try_times = 5
    if "tryTimes" in settings:
        try_times = max(5, settings["tryTimes"])
    for tries in range(try_times):
        if (not first):
            print("\n")
        print("Start to run bench " + bench["name"])

        nr_thread = settings["threadNumber"]
        if "threadNumber" in bench:
            nr_thread = bench["threadNumber"]
        bench["threadNumber"] = nr_thread   # used in latex template

        # run pre task
        if "preTask" in bench:
            res = os.system(bench["preTask"])

        # run task
        task_arg = ""
        for phase in settings["phase"]:
            task_arg += " " + phase["type"] + " " + str(phase["size"])
        task_arg += " -thread " + str(nr_thread)
        task = bench["task"] + task_arg
        print("Run task: " + task)
        res = os.system(task)

        # run after task
        if "afterTask" in bench:
            res = os.system(bench["afterTask"])

        # if task failed, try another time or exit
        if res != 0:
            if tries == try_times - 1:
                print("Bench " + bench["name"] +
                      " return " + str(res) + "!")
                print("Failed for " + str(try_times) + " times! Exit.");
                exit(-1)
            else:
                print("Bench " + bench["name"] +
                      " return " + str(res) + "! Retry.")
                time.sleep(5.0)
                continue

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
            phase["duration"] = get_duration(stat.duration)
            phase["latency"] = get_us(stat.average_latency)
            phase["max_latency"] = get_us(stat.max_latency)
            tex_stat["phases"].append(phase)
        tex_stats.append(tex_stat)
        first = False
        break

draw_throughput_fig()
draw_latency_fig()

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
        max_max_latency = us_max(
            max_max_latency, tex_stats[j]["phases"][i]["max_latency"])
        min_max_latency = us_min(
            min_max_latency, tex_stats[j]["phases"][i]["max_latency"])
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
render_dict["benchs"] = settings["bench"]
render_dict["phases"] = settings["phase"]
for phase in render_dict["phases"]:
    phase["size"] = human_readable(phase["size"])

document = template.render(**render_dict)

with open("report.tex", 'w') as output:
    output.write(document)

if settings["compileTeX"]:
    print("compiling pdf...")
    os.system("latexmk -silent " + "report.tex")
    print("pdf compiled")

arch_file = archieve_file()
print("achieve file: " + arch_file)
pdf_url = upload_file(arch_file)
if pdf_url:
    send_mail(pdf_url)
