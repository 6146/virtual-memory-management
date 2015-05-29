p3: vmm Command
vmm: vmm.c vmm.h
	gcc vmm.c -o vmm
Command: Command.c
	gcc Command.c -o Command
clean:
	rm vmm Command *~

