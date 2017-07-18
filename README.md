#SCAM


#Setupt SCAM configurations:

open Scam/scam.ini and change the following


#In the [quickhpc] scope:

change path: /home/amitkap/Desktop/Files_to_git/quickhpc/quickhpc to the path of quickhpc in your pc change 

conf: /home/amitkap/Desktop/Files_to_git/quickhpc/events.conf to the path of events.conf in your pc 

both files are in quickhpc folder


#In the [noisification] scope:

change path: /home/amitkap/Desktop/Files_to_git/scam_noisification/src/scam_noisification to the path of scam_noisification in your pc


#In quickhpc/papi-5.5.1 folder

Build PAPI (`./configure; make`)


#Run SCAM from terminal using: sudo python scam.py


#Run ServerLoader.py in the server side: python ServerLoader.py <scam ip> <scam port>

Enter IP address of scam 

Enter port (the port from [comm] scop in the scam.ini file) 
