fuse_flags= -D_FILE_OFFSET_BITS=64 -lfuse -pthread
montaje = hola
fusesinho : fusesinho.o fusesinho.h
	gcc -g -o $@  $^ ${fuse_flags}
	@mkdir -p $(montaje)
	
fusesinho.o : fusesinho.c fusesinho.h
	gcc -g -c -o $@  $< ${fuse_flags}


mount: fusesinho
	./fusesinho -f $(montaje)
debug: fusesinho
	./fusesinho -d $(montaje)

umount:
	fusermount -u $(montaje)

clean:
	rm *.o -r $(montaje) fusesinho