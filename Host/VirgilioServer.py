import socket
import json

print("Reading conf")

with open('settings.conf') as json_file:  
    data = json.load(json_file)
    for con in data['connection']:
        host = con['host']
        port = con['port']
        
print("Server host:", host)
print("Server port:", port)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
	s.bind((host, port))
	s.listen()
	conn, addr = s.accept()
	with conn:
		print('Connect by', addr)
		while True:
			data = conn.recv(1300)
			print('Recived', data) 
