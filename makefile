vmm:
	gcc vmm.c -o vmm
	gcc Command.c -o Command
clean:
	rm vmm Command *~

