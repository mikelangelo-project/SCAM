# SCAM - Side Channel Attack & Mitigation

## Overview
SCAM is a user space module that identifies cache side-channel attacks using the prime-and-probe technique and mitigates the effects of these attacks. The first function of SCAM is monitoring, which identifies attacks by analyzing the data of CPU counters collected through the PAPI library. The second function of SCAM is mitigation of an attack by adding noise to the cache in a way that makes it hard for an attacker to obtain information from priming and probing the cache.

Testing SCAM is possible with three virtual machines that can be downloaded separately. The VMs include a target, which is a TLS server based on a slightly modified GNU-TLS server, a TLS client and an attacker that obtains the server's private RSA key. 

## Installation

### SCAM
Download all files from the GIT.

Download and install PAPI 5.4.1 from http://icl.utk.edu/papi/news/news.html?id=358

Go to: ./papitool, set the path of PAPI in `Makefile` and run `make`.

Go to: ./scam_noisification and run `make`.

*If the number of cores is not a power of two:*

- Go to ./scam_noisification/src/l3.c and make sure that the `FULLMAPPING` flag is defined.

### VM Setup

Load the VMs from https://drive.google.com/drive/folders/1z5rGRblR6d2rZNVXpPket3338KneKJHv?usp=sharing

Configure the memory pages to "huge pages" and setup networking by running the following instructions:

`sudo mount -t hugetlbfs hugetlbfs /hugepages/`

`sudo chmod 777 /hugepages/`

`sudo brctl addbr br1`

`sudo ip addr add 192.168.179.1/24 broadcast 192.168.179.255 dev br1`

`sudo ip link set br1 up`

`sudo ip tuntap add dev tap1 mode tap`

`sudo ip link set tap1 up promisc on`

`sudo brctl addif br1 tap1`

`sudo dnsmasq --interface=br1 --bind-interfaces --dhcp-range=192.168.179.10,192.168.179.254 #add dhcp for all interfaces`


### VM Launch:

Launch the three virtual machines by:

**Attacker:**

`sudo kvm -m 2G -hda $PATH/VM1.qcow2 -mem-path /hugepages -boot c -smp 2  -name attacker -device e1000,netdev=tap1,mac=52:54:00:00:02:04 -netdev tap,id=tap1,script=/etc/qemu-ifup &`

Username: `attacker` & Password: `Qweasdd`.

Go to `~/FillingTheCache/` and type `make`

Attacker program - (`~/FillingTheCache/FillingTheCache`)

**Server (Target):**

`sudo kvm -m 1G -hda $PATH/VM2.qcow2 -boot c -smp 1 -name SERVER -device e1000,netdev=tap1,mac=52:54:00:00:02:05 -netdev tap,id=tap1,script=/etc/qemu-ifup &`

Username: `attacker` & Password: `1234567`

Go to `~/Desktop/Project/CseProject/GnuTLS_Server/` and type `make`

Server program-   (`~/Desktop/Project/CseProject/GnuTLS_Server/GnuTLS_Server`)

**Client:**

`sudo kvm -m 512M -hda $PATH/VM3.qcow2 -boot c -smp 1 -name CLIENT -device e1000,netdev=tap1,mac=52:54:00:00:02:03 -netdev tap,id=tap1,script=/etc/qemu-ifup &`

Username: `attacker` & Password: `1234567`

Go to `~/Desktop/Project/CseProject/GnuTLS_Client/` and type `make`

Client program-   (`~/Desktop/Project/CseProject/GnuTLS_Client/GnuTLS_Client`)

## Test:
Testing SCAM requires several steps which are described below. SCAM must be launched ahead of all the virtual machines to map the Last Level Cache by associating memory addresses in SCAM's virtual memory to cache sets. Then the target server and the client are initialized (see above) and run the handshake. In this phase SCAM learns the cache sets that the server uses. The last step is to launch the attacker. 

Perform the following steps to run the test:

Start SCAM by 'sudo python scam.py',

An xterm window will open with the scam-noisification program output which is only used for debug.

Wait until the notification 'Turn on the target, start the decryption process, and press any key...' is shown and launch the server and client VMs. Verify that TLS handshakes are executed in a loop by messages printed on the screen.

Press any key and wait for SCAM to identify all the cache sets active during a TLS handshake. The completion of this phase is indicated by the 'to start monitoring, please enter target PID:' message. Enter the Target process PID from the hypervisor point-of-view by the following steps. Run htop,  enter F5 for the tree view, locate the process tree of the VM running the TLS server by its name (in the attached VM this name is "qemu SERVER") and select the PID of the lowest sub-process of this VM's hierarchy.

Launch the attacker.

## Configuration parameters: 

'./Scam/scam.ini' configurable parameters:

scam_cores - the ID of the CPU core that SCAM will use.

plot_enable - for debug, shows the latest data on cache accesses and cache misses for the target PID.

window_avg_thresh - monitoring assumes that an attack is taking place when the ratio of cache misses to cache accesses is high for a long period of time. The parameter window_avg_thresh defines a threshold on this ratio, that takes values between 0 and 1 and is 0.8 by default. Reducing the threshold may result in more false positives (classifying legitimate traffic as attacks) and may also identify attacks that otherwise go undetected. Increasing the threshold has the opposite effect.

detect_thresh - the length of time that monitoring data must stay above window_avg_thresh to constitute an attack. The threshold is 20 milli-seconds out of a totla of 65 ms for a 4096-bit private key operation. This threshold should be modified to a fraction (no more than a third) of the time necessary to complete a private key operation by the server VM.

min_rumble_duration - minimum time of a "noisification" phase after detecting an attack.
