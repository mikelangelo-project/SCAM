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
NUM_OF_KEYS_PER_CHUNK       = int(ConfigSectionMap("papitool")['num_of_keys_per_chunk'])
DECRYPT_DURATION            = int(ConfigSectionMap("papitool")['decrypt_duration'])
SLEEP_DURATION              = int(ConfigSectionMap("papitool")['sleep_duration'])
PAPITOOL_PATH               = os.path.join(os.path.dirname(os.getcwd()), "papitool/papitool")
PAPITOOL_NUM_OF_ITERATION   = ConfigSectionMap("papitool")['num_of_iteration']
PAPITOOL_INTERVAL_DURATION  = ConfigSectionMap("papitool")['interval_duration']
PAPITOOL_CONF               = os.path.join(os.path.dirname(os.getcwd()), "papitool/events.conf")
PAPITOOL_CHUNK              = ((DECRYPT_DURATION + SLEEP_DURATION)) * NUM_OF_KEYS_PER_CHUNK
MONITOR_WINDOW              = int(ConfigSectionMap("papitool")['monitor_window'])
WINDOW_AVG_THRESH           = float(ConfigSectionMap("papitool")['window_avg_thresh'])
DETECT_THRESH               = int(ConfigSectionMap("papitool")['detect_thresh'])
papitool_log_filename       = ConfigSectionMap("papitool")['log_filename']

# SCAM
plotEnable                  = config.getboolean("scam", "plot_enable")
Scam_Cores                  = ConfigSectionMap("scam")['scam_cores'].split(",")

# Noisification
noisification_log_filename  = ConfigSectionMap("noisification")['log_filename']
NOISIFICATION_PATH          = os.path.join(os.path.dirname(os.getcwd()), "scam_noisification/scam/tool")
Min_Rumble_Duration         = ConfigSectionMap("noisification")['min_rumble_duration']
Is_Complement_To	    = config.getboolean("noisification","is_complement_to")
LINES_PER_SET               = int(ConfigSectionMap("noisification")['lines_per_set'])
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

if Is_Complement_To:
    Noise_Intensity[0] = str(int(Noise_Intensity[0])+LINES_PER_SET+1)
    Noise_Intensity[1] = '0'

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

def papitool_analyzer(pipe_from):
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
    papitool_log_file = None
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

        # papitool
        target_pid = raw_input("to start monitoring, please enter target PID:")
        while not pid_exists(int(target_pid)):
            target_pid = raw_input("Wrong PID, try again, please enter target PID:")

        papitool_cmd = PAPITOOL_PATH + " -a " + target_pid + \
                       " -c " + PAPITOOL_CONF + " -i " + PAPITOOL_INTERVAL_DURATION + \
                       " -n " + PAPITOOL_NUM_OF_ITERATION

        papitool_log_file       = open(papitool_log_filename,"w")
        papitool_file_lock      = threading.Lock()
        papitool_proc           = subprocess.Popen(shlex.split(papitool_cmd),stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        papitool_analyzer_proc  = threading.Thread(target = papitool_analyzer,  args=[papitool_proc.stdout])
        papitool_stderr         = threading.Thread(target = process_logger,     args=(papitool_proc.stderr,papitool_log_file,papitool_file_lock))

        lastTimeAttack = int(time.time())
        papitool_analyzer_proc.start()
        papitool_stderr.start()

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

            papitool_proc.kill()
            if papitool_analyzer_proc is not None and papitool_analyzer_proc.isAlive():
                papitool_analyzer_proc.join()
            if papitool_stderr is not None and papitool_stderr.isAlive():
                papitool_stderr.join()

            if papitool_log_file is not None:
                papitool_log_file.close()
            if noisification_log_file is not None:
                noisification_log_file.close()
        except UnboundLocalError:
            pass

        return result

if __name__ == "__main__":
    sys.exit(main())
