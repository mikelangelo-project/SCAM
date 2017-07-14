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

config = ConfigParser.ConfigParser()
config.read ("scam.ini")

keepRunning = True
attackActive = False

# scam

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
QUICKHPC_PATH               = ConfigSectionMap("quickhpc")['path']
QUICKHPC_NUM_OF_ITERATION   = ConfigSectionMap("quickhpc")['num_of_iteration']
QUICKHPC_INTERVAL_DURATION  = ConfigSectionMap("quickhpc")['interval_duration']
QUICKHPC_CONF               = ConfigSectionMap("quickhpc")['conf']
QUICKHPC_CHUNK              = ((DECRYPT_DURATION + SLEEP_DURATION)) * NUM_OF_KEYS_PER_CHUNK
quickhpc_log_filename       = ConfigSectionMap("quickhpc")['log_filename']

# SCAM
SCAM_CORES                  = map(int,ConfigSectionMap("scam")['scam_cores'].split(','))
NOISIFICATION_CORES         = map(int,ConfigSectionMap("scam")['noisification_cores'].split(','))
plotEnable                  = config.getboolean("scam", "plot_enable")
monitorAnomalyThreshold     = int(ConfigSectionMap("scam")['monitor_anomaly_threshold'])
GET_THRESHOLDS              = config.getboolean("scam", "get_thresholds")
#Noisification
SANITY_TRIES                = ConfigSectionMap("noisification")['sanity_tries']
noisification_log_filename  = ConfigSectionMap("noisification")['log_filename']
NOISIFICATION_PATH          = ConfigSectionMap("noisification")['path']
Min_Rumble_Duration         = ConfigSectionMap("noisification")['min_rumble_duration']

# Socket Communication Defines
server_request              = ConfigSectionMap("comm")['server_request']
server_response             = ConfigSectionMap("comm")['server_response']
TCP_PORT                    = int(ConfigSectionMap("comm")['tcp_port'])
TCP_IP                      = ConfigSectionMap("comm")['tcp_ip']
BUFFER_SIZE                 = int(ConfigSectionMap("comm")['buffer_size'])

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

def analyzeSamples(samples,monitorChart):
    ratio = []
    for line in samples:
        [PAPI_L3_TCM,PAPI_L3_TCA] = map(int,line.split(','))
        # if PAPI_L3_TCA:
        #    ratio.append(truediv(PAPI_L3_TCM,PAPI_L3_TCA))
        # else:
        #    ratio.append(0)
        ratio.append(PAPI_L3_TCM)
    i = 0
    filterValueThresh = sorted(ratio)[len(ratio)*90/100]
    if plotEnable:
        ratioIndex = range (0, len (ratio))
        monitorChart.update (ratioIndex, ratio)
    j = 2
    while j<len(ratio)-2:
        if ratio[j] > filterValueThresh: # avereage anomalities
            ratio[j] = int(numpy.mean([ratio[j-2],ratio[j-1],ratio[j],ratio[j+1]]))
        j+=1

    j=0
    minRatio = int(numpy.max(ratio))
    while j<len(ratio)-10:
        tmpMeanRatio = int(numpy.mean(ratio[j:j+10]))
        if  tmpMeanRatio < minRatio:
            minRatio = tmpMeanRatio
        j+=10

    thresholdForAnomaly = monitorAnomalyThreshold + minRatio
    j=0
    while j < len(ratio):
        if ratio[j] > thresholdForAnomaly:
            ratio[j] = 1
        else:
            ratio[j] = 0
        j+=1
    zeroIndex = 0
    j = 0
    ratio[len(ratio)-1] = 0
    while j < len(ratio):
        if ratio[j] == 0:
            if j-zeroIndex < 10:
                ratio[zeroIndex:j] = numpy.full(j-zeroIndex,0)
            zeroIndex = j
        j += 1

    j = 0
    while j < len (ratio):
        if ratio[j] == 1:
            return True
        j += 1
    return False

    #print ratio - old method to detect

    # while True:
    #     if (i+DECRYPT_DURATION+SLEEP_DURATION > len(ratio)):
    #         return False
    #     a = numpy.mean(ratio[i:i+SLEEP_DURATION/3])
    #     b = numpy.mean(ratio[i+SLEEP_DURATION/3:i+SLEEP_DURATION/3+DECRYPT_DURATION])
    #     c = numpy.mean(ratio[i+SLEEP_DURATION/3+DECRYPT_DURATION:i+DECRYPT_DURATION+SLEEP_DURATION*2/3])
    #     if 3*a<b and 3*c<b and b>0.3:
    #         stepCount+=1
    #         i += DECRYPT_DURATION+SLEEP_DURATION+1
    #         #print ["yes", stepCount, i, a, b, c]
    #         if stepCount==5:
    #             return True
    #     else:
    #         #print ["no",stepCount, i, a, b, c]
    #         i += 5
    # return False


def quickhpc_analyzer(pipe_from,noisification_proc):
    noiseActive = False
    lastTimeAttack = 0
    attackActive = None
    lines = []
    monitorChart = None
    if plotEnable:
        monitorChart = DynamicUpdate.DynamicUpdate()
        monitorChart.on_launch(0,NUM_OF_KEYS_PER_CHUNK * (SLEEP_DURATION + DECRYPT_DURATION),-1,2)
    global keepRunning
    counter = 0
    while keepRunning:
        line = pipe_from.readline()
        if line:
            counter+=1
            lines.append(line)
            if counter == QUICKHPC_CHUNK:
                curr_time = time.time()
                counter = 0
                detect = analyzeSamples(lines,monitorChart)
                lines = []
                attackActive_r = attackActive
                if detect:
                    attackActive = True
                    lastTimeAttack = curr_time
                    if noiseActive is False:
                        print ("Turning On Noise")
                        noisification_proc.stdin.write ('5\n')
                        noiseActive = True
                else:
                    attackActive = False
                    #print "(curr_time - lastTimeAttack)=" , (curr_time - lastTimeAttack)
                    #print "noiseActive", noiseActive, "Min_Rumble_Duration", Min_Rumble_Duration
                    if (noiseActive is True) and int(curr_time - lastTimeAttack) > int(Min_Rumble_Duration):
                        print ("Turning Off Noise")
                        noisification_proc.stdin.write ('6\n')
                        noiseActive = False
                if attackActive_r != attackActive:
                    print ("Attack: " , attackActive)
                    print ("Time Since Last Attack: " , (curr_time - lastTimeAttack))



def get_stderr(p, handlingresults=True):
    # print'get_stderr()\n'
    while True:
        try:
            line = p.stderr.readline()
            if line:
                if handlingresults and (line == '0\n' or line == '1\n'):
                    if line[0] == '0':
                        # print 'line[0]=0\n'
                        return False
                    else:
                        # print 'line[0]=1\n'
                        return True
                else:
                    # print 'line[0]=?\n'
                    # print >> sys.stdout,[l,'lalala']
                    return False
            else:
                time.sleep (1)
        except IOError:
            return False


def main():
    global keepRunning, lastTimeAttack
    print ('SCAM')
    print ("Proccess ID: ", psutil.Process().pid)
    # vars
    conn = None
    s = None
    result = False
    try:
        # flush memory
        os.system ("sh -c 'echo 3 >/proc/sys/vm/drop_caches'")

        # pin scam to its cores
        noisification_psutil = psutil.Process()
        #noisification_psutil.cpu_affinity(SCAM_CORES)
        
        # open scam_noisification as a subprocess
        noisification_log_file = open(noisification_log_filename, "w")
        noisification_log_proc = subprocess.Popen(shlex.split("xterm -e tail -f " + noisification_log_filename), stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        noisification_proc = subprocess.Popen([NOISIFICATION_PATH], stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        noisification_stdout = threading.Thread(target = process_logger, args=(noisification_proc.stdout, noisification_log_file))
        noisification_stdout.start()

        time.sleep(1)
		
        # Sanity check - must for getting thresholds
        tries = 0
        if GET_THRESHOLDS:
            while not result and tries < SANITY_TRIES:
                noisification_proc.stdin.write('1\n')
                result = get_stderr(noisification_proc)
                if not result:
                    tries += 1
            if not result:
                return result

        # Mapping the cache
        noisification_proc.stdin.write('2\n')
        result = get_stderr(noisification_proc)
        if not result:
            return result

        # Create socket
        print ("Waiting for server at " , TCP_IP , ":" ,TCP_PORT)
        s = socket.socket (socket.AF_INET, socket.SOCK_STREAM)
        s.bind((TCP_IP, TCP_PORT))
        s.listen(1)
        conn, addr = s.accept()
        
        print ('Connection address:', addr)
        data = conn.recv (BUFFER_SIZE)
        while True:
            if data == server_request:
                break
        print ("received data:", data)

        # Ranking I
        noisification_proc.stdin.write('3\n')
        result = get_stderr(noisification_proc)
        if not result:
            return result
        conn.send(server_response)

        time.sleep(2) # wait for server to start decrypt

        # Ranking II
        noisification_proc.stdin.write ('4\n')
        result = get_stderr(noisification_proc)
        if not result:
            return result

        # noisification done with the hard part, pin noisification to a specific core
        noisification_psutil = psutil.Process (noisification_proc.pid)
        #noisification_psutil.cpu_affinity(NOISIFICATION_CORES)

        # Quickhpc
        target_pid = raw_input("to start monitoring, please enter target PID:")
        while not pid_exists(int(target_pid)):
            target_pid = raw_input("Wrong PID, try again, please enter target PID:")

        quickhpc_cmd = QUICKHPC_PATH + " -a " + target_pid + \
                       " -c " + QUICKHPC_CONF + " -i " + QUICKHPC_INTERVAL_DURATION + \
                       " -n " + QUICKHPC_NUM_OF_ITERATION

        quickhpc_log_file = open(quickhpc_log_filename,"w")
        #quickhpc_log_proc = subprocess.Popen(shlex.split("xterm -e tail -f " + quickhpc_log_filename),stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        quickhpc_file_lock = threading.Lock()
        quickhpc_proc = subprocess.Popen(shlex.split(quickhpc_cmd),stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        #quickhpc_stdout     = threading.Thread(target = process_logger,     args=(quickhpc_proc.stdout,quickhpc_log_file,quickhpc_file_lock))
        quickhpc_analyzer_proc   = threading.Thread(target = quickhpc_analyzer,  args=[quickhpc_proc.stdout,noisification_proc])
        quickhpc_stderr     = threading.Thread(target = process_logger,     args=(quickhpc_proc.stderr,quickhpc_log_file,quickhpc_file_lock))

        #quickhpc_stdout.start()
        lastTimeAttack = int(time.time())
        quickhpc_analyzer_proc.start()
        quickhpc_stderr.start()

        while True:
            time.sleep(0.01)

    except (KeyboardInterrupt, SystemExit):
        pass

    finally:
        try:
            if s is not None:
                s.close()
            if conn is not None:
                conn.close ()
            keepRunning = False
            
            noisification_log_proc.kill ()
            noisification_proc.kill()
            if noisification_stdout.isAlive():
                noisification_stdout.join()

            #quickhpc_log_proc.kill()
            quickhpc_proc.kill()
            if quickhpc_analyzer_proc is not None and quickhpc_analyzer_proc.isAlive():
                quickhpc_analyzer_proc.join()
            if quickhpc_stderr is not None and quickhpc_stderr.isAlive():
                quickhpc_stderr.join()
        except UnboundLocalError:
            pass

        return result

if __name__ == "__main__":
    sys.exit(main())
