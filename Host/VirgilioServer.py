import socket
import json
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
name = "data/caronte_" + time + ".csv"

f = open(name, "a")
f.write("id;time_start;elapsed_time;kernel;operation;path;size_start;size_gain;thread;process;write_size;entropy;exec_path;exec;gt\n");
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