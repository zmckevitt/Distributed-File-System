all: server/df_server.c client/df_client.c
	gcc -pthread -o server/dfs server/df_server.c -lssl -lcrypto -lm
	gcc -pthread -o client/dfc client/df_client.c client/linkedlist.c -lssl -lcrypto -lm

.PHONY: all clean

clean:
	rm -f client/*.o
	rm -f server/*.o
	rm client/dfc
	rm server/dfs

	rm -r server/DFS1
	rm -r server/DFS2
	rm -r server/DFS3
	rm -r server/DFS4

	rm client/get_files/*