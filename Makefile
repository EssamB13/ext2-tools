
CFLAGS=-std=gnu99 -Wall -g

make_all : ext2_mkdir ext2_checker ext2_cp ext2_rm ext2_restore ext2_ln readimage

ext2_mkdir :  ext2_mkdir.o ext2_utils.o
	gcc $(CFLAGS) -o ext2_mkdir $^

ext2_checker :  ext2_checker.o ext2_utils.o
	gcc $(CFLAGS) -o ext2_checker $^

ext2_cp :  ext2_cp.o ext2_utils.o
	gcc $(CFLAGS) -o ext2_cp $^

ext2_ln :  ext2_ln.o ext2_utils.o
	gcc $(CFLAGS) -o ext2_ln $^

ext2_rm :  ext2_rm.o ext2_utils.o
	gcc $(CFLAGS) -o ext2_rm $^ 

ext2_restore :  ext2_restore.o ext2_utils.o
	gcc $(CFLAGS) -o ext2_restore $^ 

readimage : readimage.o 
	gcc $(CFLAGS) -o readimage $^

%.o : %.c ext2.h ext2_mkdir.h ext2_utils.h ext2_rm.h ext2_restore.h ext2_cp.h ext2_ln.h ext2_checker.h
	gcc $(CFLAGS) -g -c $<

clean : 
	rm -f *.o ext2_mkdir *~
