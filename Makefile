chinnu: chinnu.l chinnu.y chinnu.h
	bison -d chinnu.y
	flex -o chinnu.lex.c chinnu.l
	cc -o $@ chinnu.tab.c chinnu.lex.c chinnu.c

clean:
	rm chinnu chinnu.tab.c chinnu.tab.h chinnu.lex.c
