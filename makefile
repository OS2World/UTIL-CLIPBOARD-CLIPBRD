all :         clipbrd.exe pmclip.exe

clipbrd.exe : clipbrd.obj clipbrd.def
	gcc -Zomf -o $@ -s -T 0x10000 $< clipbrd.def

clipbrd.obj : clipbrd.c common.h
	gcc -c -Wall $< -Zomf -o $@ -DNDEBUG

pmclip.exe :  pmclip.obj pmclip.def
	gcc -Zomf -o $@ -s -T 0x10000 $< pmclip.def

pmclip.obj :  pmclip.c common.h
	gcc -c -Wall $< -Zomf -o $@ -DNDEBUG -O2
