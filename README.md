# S.C.A.M.

Side Channel Attack & Mitigation

## Installing

Download all files.

Go to: /quickhpc/papi-5.5.1/src and run `./configure` followed by `make`.

Go to: /quickhpc and run `make`.

Go to: /scam_noisification and run `make`.

*If the number of cores is not a power of two:*

- Go to /scam_noisification/src/l3.c and make sure that the `FULLMAPPING` flag is defined.

### VMs set up

In order to run the VMs run the following commands:

`sudo mount -t hugetlbfs hugetlbfs /hugepages/`

`sudo chmod 777 /hugepages/`

`sudo brctl addbr br1`

`sudo ip addr add 192.168.179.1/24 broadcast 192.168.179.255 dev br1`

`sudo ip link set br1 up`

`sudo ip tuntap add dev tap1 mode tap`

`sudo ip link set tap1 up promisc on`

`sudo brctl addif br1 tap1`

`sudo dnsmasq --interface=br1 --bind-interfaces --dhcp-range=192.168.179.10,192.168.179.254 #add dhcp for all interfaces `

`binded to the bridge`



### VMs launch comamnd:

**Attacker:**

`sudo kvm -m 2G -hda $PATH$/VM1.qcow2 -mem-path /hugepages -boot c -smp 2  -name attacker -device e1000,netdev=tap1,mac=52:54:00:00:02:04 -netdev tap,id=tap1,script=/etc/qemu-ifup &`

Username: `attacker` & Password: `Qweasdd`.

Go to `/FillingTheCache/` and type `make`

Attacker program-   (`./FillingTheCache/FillingTheCache`)

**Server (Target):**

`sudo kvm -m 1G -hda $PATH$/VM2.qcow2 -boot c -smp 1 -name SERVER -device e1000,netdev=tap1,mac=52:54:00:00:02:05 -netdev tap,id=tap1,script=/etc/qemu-ifup &`

Username: `attacker` & Password: `1234567`

Go to `/Desktop/Project/CseProject/GnuTLS_Server/` and type `make`

Server program-   (`./Desktop/Project/CseProject/GnuTLS_Server/GnuTLS_Server`)

**Client:**

`sudo kvm -m 512M -hda $PATH$/VM3.qcow2 -boot c -smp 1 -name CLIENT -device e1000,netdev=tap1,mac=52:54:00:00:02:03 -netdev tap,id=tap1,script=/etc/qemu-ifup &`

Username: `attacker` & Password: `1234567`

Go to `/Desktop/Project/CseProject/GnuTLS_Client/` and type `make`

Client program-   (`./Desktop/Project/CseProject/GnuTLS_Client/GnuTLS_Client`)

## Overall Flow:

Go to Scam and start the program by 'sudo python scam.py',

An xterm window will open with the scam-noisification program output - it's just for debug.

Now the scam-noisification program is mapping the cache and performs a baseline sample of the cache.

Wait until 'Turn on the target, start the decryption process, and press any key...' is shown, now turn on the Target (Server program, see above) and the Client program (verify that a handshake is made).

Press any key and wait for second sample of the cache.

Wait until 'to start monitoring, please enter target PID:' and enter the Target process PID from the hypervisor point-of-view (can be found with htop - press F5 for tree view and select the PID of the lowest sub-proccess of the VM hierarchy.

Now the SCAM server is running, start an attack (with the Attacker) to verify that everything is working.

## Notes: 

### '/Scam/scam.ini' configurable parameters:

scam\_cores - the cpu's core id that the program will use.

plot\_enable - for debug, shows the latest cache performance counter samples of the target PID.

window\_avg\_thresh - control the sensitivity of the monitor.

min\_rumble\_duration - minimum time of "noise the cache" activity after detecting an attack.
