build:
	rm -rf client.bin || echo -n ''
	rm -rf server.bin || echo -n ''
	rm -rf server_db.bin || echo -n ''
	./make.sh sources/client.c
	./make.sh sources/server.c
	./make.sh sources/server_db.c

run_server:
	./sources/server.bin

run_server_db:
	./sources/server_db.bin

run_client:
	./sources/client.bin 127.0.0.1