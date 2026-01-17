PROGS = raspberry-unsplit

all:
	gcc -o ${PROGS} ${PROGS}.c
	sudo mv ${PROGS} /usr/local/bin/${PROGS}

clean:
	-rm -f ${PROGS} ${TEMPFILES}
