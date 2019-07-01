import socket
import json
import sys
from time import gmtime, strftime

# Reading configuration file -----------
print("Reading conf")
with open('settings.conf') as json_file:  
    data = json.load(json_file)
    for con in data['connection']:
        host = con['host']
        port = con['port']
        
print("Server host:", host)
print("Server port:", port)

time = strftime("%Y%m%d_%H%M%S", gmtime());

name = "data/" + sys.argv[1] + ".csv"
print(sys.argv[1])
print(name)

f = open(name, "a")
f.write("write;time_start;elapsed_time;mode;flags;path;size_start;size_gain;thread;process;write_size;entropy;exe_path;name;ground_truth\n");
f.close()

# Add record to db
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
	s.bind((host, port))
	s.listen()
	conn, addr = s.accept()
	with conn:
		print('Connect by', addr)
		while True:
			data = conn.recv(3000)
			data = str(data.decode('utf-8'))
			print(data)
			f = open(name, "a")
			print(f.write(data))
			f.close()