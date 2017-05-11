CC=mpicc

othellox: othellox.c
	$(CC) -o $@ $<
