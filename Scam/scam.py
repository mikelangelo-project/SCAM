import sys
import subprocess
import threading
import time
import socket
import shlex
import psutil
import os
import errno
from operator import truediv
import numpy
import DynamicUpdate
import ConfigParser
import matplotlib.pyplot as plt


config = ConfigParser.ConfigParser()
config.read ("scam.ini")

keepRunning = True
attackActive = False

def ConfigSectionMap(section):
    dict1 = {}
    options = config.options(section)
    for option in options:
        try:
            dict1[option] = config.get(section, option)
            if dict1[option] == -1:
                print("skip: %s" % option)
        except:
            print("exception on %s!" % option)
            dict1[option] = None
    return dict1

# Read parameters from ini
# quickhpc
NUM_OF_KEYS_PER_CHUNK       = int(ConfigSectionMap("quickhpc")['num_of_keys_per_chunk'])
DECRYPT_DURATION            = int(ConfigSectionMap("quickhpc")['decrypt_duration'])
SLEEP_DURATION              = int(ConfigSectionMap("quickhpc")['sleep_duration'])
QUICKHPC_PATH               = os.path.join(os.path.dirname(os.getcwd()), "quickhpc/quickhpc")
QUICKHPC_NUM_OF_ITERATION   = ConfigSectionMap("quickhpc")['num_of_iteration']
QUICKHPC_INTERVAL_DURATION  = ConfigSectionMap("quickhpc")['interval_duration']
QUICKHPC_CONF               = os.path.join(os.path.dirname(os.getcwd()), "quickhpc/events.conf")
QUICKHPC_CHUNK              = ((DECRYPT_DURATION + SLEEP_DURATION)) * NUM_OF_KEYS_PER_CHUNK
MONITOR_WINDOW              = int(ConfigSectionMap("quickhpc")['monitor_window'])
WINDOW_AVG_THRESH           = float(ConfigSectionMap("quickhpc")['window_avg_thresh'])
DETECT_THRESH               = int(ConfigSectionMap("quickhpc")['detect_thresh'])

quickhpc_log_filename       = ConfigSectionMap("quickhpc")['log_filename']

# SCAM
plotEnable                  = config.getboolean("scam", "plot_enable")
Scam_Cores                  = ConfigSectionMap("scam")['scam_cores'].split(",")

# Noisification
noisification_log_filename  = ConfigSectionMap("noisification")['log_filename']
NOISIFICATION_PATH          = os.path.join(os.path.dirname(os.getcwd()), "scam_noisification/scam/tool")
Min_Rumble_Duration         = ConfigSectionMap("noisification")['min_rumble_duration']
Noise_Intensity             = ConfigSectionMap("noisification")['noise_intensity'].split(",")

# Socket Communication Defines
server_request              = ConfigSectionMap("comm")['server_request']
server_response             = ConfigSectionMap("comm")['server_response']
TCP_PORT                    = int(ConfigSectionMap("comm")['tcp_port'])
TCP_IP                      = ConfigSectionMap("comm")['tcp_ip']
BUFFER_SIZE                 = int(ConfigSectionMap("comm")['buffer_size'])

DEMO_ARRAY_LEN              = 300

noise_sock = None
target_sock = None

def pid_exists(pid):
    """Check whether pid exists in the current process table.
    UNIX only.
    """
    if pid < 0:
        return False
    if pid == 0:
        # According to "man 2 kill" PID 0 refers to every process
        # in the process group of the calling process.
        # On certain systems 0 is a valid PID but we have no way
        # to know that in a portable fashion.
        raise ValueError('invalid PID 0')
    try:
        os.kill(pid, 0)
    except OSError as err:
        if err.errno == errno.ESRCH:
            # ESRCH == No such process
            return False
        elif err.errno == errno.EPERM:
            # EPERM clearly means there's a process to deny access to
            return True
        else:
            # According to "man 2 kill" possible error values are
            # (EINVAL, EPERM, ESRCH)
            raise
    else:
        return True

def process_logger(pipe_from, pipe_dst, file_lock=None):
    global keepRunning
    while keepRunning:
        line = pipe_from.readline()
        if line:
            if file_lock is not None:
                with file_lock:
                    pipe_dst.write(line)
                    pipe_dst.flush()
            else:
                pipe_dst.write(line)
                pipe_dst.flush()
        else:
            time.sleep (1)
    pipe_dst.close()
#
# def analyzeSamples(samples,monitorChart):
#     ratio = []
#     for line in samples:
#         PAPI_L3_TCM = map(int,line.split(','))[0]
#         ratio.append(PAPI_L3_TCM)
#     filterValueThresh = sorted(ratio)[len(ratio)*90/100]
#     if plotEnable:
#         ratioIndex = range (0, len (ratio))
#         monitorChart.update (ratioIndex, ratio)
#     j = 2
#     while j<len(ratio)-2:
#         if ratio[j] > filterValueThresh: # avereage anomalities
#             ratio[j] = int(numpy.mean([ratio[j-2],ratio[j-1],ratio[j],ratio[j+1]]))
#         j+=1
#
#     j=0
#     minRatio = int(numpy.max(ratio)) # init min
#     while j<len(ratio)-10:
#         tmpMeanRatio = int(numpy.mean(ratio[j:j+10]))
#         if  tmpMeanRatio < minRatio:
#             minRatio = tmpMeanRatio
#         j+=10
#
#     thresholdForAnomaly = monitorAnomalyThreshold + minRatio
#     j=0
#     while j < len(ratio):
#         if ratio[j] > thresholdForAnomaly:
#             ratio[j] = 1
#         else:
#             ratio[j] = 0
#         j+=1
#     zeroIndex = 0
#     j = 0
#     ratio[len(ratio)-1] = 0
#     while j < len(ratio):
#         if ratio[j] == 0:
#             if j-zeroIndex < 10:
#                 ratio[zeroIndex:j] = numpy.full(j-zeroIndex,0)
#             zeroIndex = j
#         j += 1
#
#     j = 0
#     while j < len (ratio):
#         if ratio[j] == 1:
#             return True
#         j += 1
#     return False


def quickhpc_analyzer(pipe_from):
    noiseActive = False
    lastTimeAttack = 0
    attackActive = None
    lines = []
    monitorChart = None
    if plotEnable:
        monitorChart = DynamicUpdate.DynamicUpdate()
        monitorChart.on_launch(0,NUM_OF_KEYS_PER_CHUNK * (SLEEP_DURATION + DECRYPT_DURATION),0,1)
    global keepRunning, noise_sock
    counter = 0
    window = [0] * MONITOR_WINDOW
    window_sum = 0
    pointer = 0
    demo_array = []
    detectCounter = 0

    while keepRunning:
        line = pipe_from.readline()
        if line:
            PAPI_L3_TCM,PAPI_L3_TCA = map (float, line.split (','))
            try:
                miss_ratio = truediv(PAPI_L3_TCM,PAPI_L3_TCA)
            except ZeroDivisionError:
                continue
            if counter <= MONITOR_WINDOW: #to collect enough data for avg
                counter += 1
            pointer = (pointer + 1) % MONITOR_WINDOW
            tmp = window[pointer]
            window[pointer] = miss_ratio
            window_sum += miss_ratio - tmp
            window_avg = window_sum / MONITOR_WINDOW

            if counter < MONITOR_WINDOW:
                continue

            if plotEnable:
                demo_array.append(window_avg)
                if len(demo_array) == DEMO_ARRAY_LEN:
                    monitorChart.update (range(len(demo_array)), demo_array)
                    demo_array = []

            curr_time = time.time ()

            if(window_avg > WINDOW_AVG_THRESH):
                detectCounter+=1
            else:
                detectCounter=0
            detect = (detectCounter>DETECT_THRESH)
            attackActive_r = attackActive
            if detect:
                attackActive = True
                lastTimeAttack = curr_time
                if noiseActive is False:
                    print ("Turning On Noise")
                    send_to_client (noise_sock, "6 {} {} {} {}".format(Noise_Intensity[0],Noise_Intensity[1],Scam_Cores[0],Scam_Cores[1]))
                    noiseActive = True
            else:
                attackActive = False
                if (noiseActive is True) and int(curr_time - lastTimeAttack) > int(Min_Rumble_Duration):
                    print "Turning Off Noise"
                    send_to_client (noise_sock, "7")
                    noiseActive = False
            if attackActive_r != attackActive:
                print "Attack: {}".format(attackActive)


def send_to_client(client_socket,msg):
    client_socket.send(msg)
    request = client_socket.recv(1024)
    print 'Received {}'.format(request)

def main():
    global huge_array, keepRunning, lastTimeAttack, noise_sock, target_sock,TCP_PORT

    p = psutil.Process ()
    p.cpu_affinity([int(Scam_Cores[1])])
    # vars
    server = None
    noisification_log_file = None
    quickhpc_log_file = None
    result = False
    try:
        # flush memory - should run with sudo
        os.system ("sh -c 'echo 3 >/proc/sys/vm/drop_caches'")

        server = socket.socket (socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt (socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        server.listen(2)  # max backlog of connections

        TCP_IP, TCP_PORT = server.getsockname()
        # Create socket
        # print 'Waiting for target, at {}:{}'.format (TCP_IP, TCP_PORT)
        # target_sock, address = server.accept ()
        # print "target - Connection address:{}".format(address)

        # open scam_noisification as a subprocess
        noisification_log_file = open(noisification_log_filename, "w")
        noisification_log_proc = subprocess.Popen(shlex.split("xterm -e tail -f " + noisification_log_filename), stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        noisification_proc = subprocess.Popen([NOISIFICATION_PATH,str(TCP_PORT)], stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        noisification_stdout = threading.Thread(target = process_logger, args=(noisification_proc.stdout, noisification_log_file))
        noisification_stdout.start ()

        print 'Listening on {}:{}'.format (TCP_IP, TCP_PORT)
        noise_sock, address = server.accept ()
        print "noisification - Connection address:{}".format(address)

        # data = target_sock.recv (BUFFER_SIZE)
        # while True:
            # if data == server_request:
            #     break
        # print "received data:{}".format(data)

        # Ranking I
        send_to_client (noise_sock, "2")
        # print "sending response to server {}".format(server_response)
        # send_to_client (target_sock, server_response)
        raw_input("Turn on the target, start the decryption process, and press any key...")
        time.sleep(2) # wait for server to start decrypt
        # Ranking II
        send_to_client (noise_sock, "3")

        # Quickhpc
        target_pid = raw_input("to start monitoring, please enter target PID:")
        while not pid_exists(int(target_pid)):
            target_pid = raw_input("Wrong PID, try again, please enter target PID:")

        quickhpc_cmd = QUICKHPC_PATH + " -a " + target_pid + \
                       " -c " + QUICKHPC_CONF + " -i " + QUICKHPC_INTERVAL_DURATION + \
                       " -n " + QUICKHPC_NUM_OF_ITERATION

        quickhpc_log_file       = open(quickhpc_log_filename,"w")
        quickhpc_file_lock      = threading.Lock()
        quickhpc_proc           = subprocess.Popen(shlex.split(quickhpc_cmd),stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        quickhpc_analyzer_proc  = threading.Thread(target = quickhpc_analyzer,  args=[quickhpc_proc.stdout])
        quickhpc_stderr         = threading.Thread(target = process_logger,     args=(quickhpc_proc.stderr,quickhpc_log_file,quickhpc_file_lock))

        lastTimeAttack = int(time.time())
        quickhpc_analyzer_proc.start()
        quickhpc_stderr.start()

        while True:
            time.sleep(0.01)

    except (KeyboardInterrupt, SystemExit):
        pass

    finally:
        try:
            if server is not None:
                server.close()
            keepRunning = False
            
            noisification_log_proc.kill ()
            noisification_proc.kill()
            if noisification_stdout.isAlive():
                noisification_stdout.join()

            quickhpc_proc.kill()
            if quickhpc_analyzer_proc is not None and quickhpc_analyzer_proc.isAlive():
                quickhpc_analyzer_proc.join()
            if quickhpc_stderr is not None and quickhpc_stderr.isAlive():
                quickhpc_stderr.join()

            if quickhpc_log_file is not None:
                quickhpc_log_file.close()
            if noisification_log_file is not None:
                noisification_log_file.close()
        except UnboundLocalError:
            pass

        return result

if __name__ == "__main__":
    sys.exit(main())
