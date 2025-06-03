all: uds_server uds_client

uds_server: uds_server.cpp
	g++ -o uds_server uds_server.cpp

uds_client: uds_client.cpp
	g++ -o uds_client uds_client.cpp

clean:
	rm -f uds_server uds_client
