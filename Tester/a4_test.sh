#!/bin/bash







if [ $1 = "mkdir" ]; then

	rm -f emptydisk.img
	cp images/emptydisk.img .

	../ext2_mkdir emptydisk.img DIRECTORY

	#./readimage emptydisk.img > mkdir_output.txt

	chmod 777 mkdir_output.txt
	chmod 777 ext2_dump

	./ext2_dump emptydisk.img > mkdir_output.txt



	diff -q case4-mkdir.txt mkdir_output.txt

	../ext2_checker emptydisk.img

fi


if [ $1 = "rm" ]; then

	rm -f twolevel.img
	cp images/twolevel.img .

	../ext2_rm twolevel.img /afile/

	#./readimage emptydisk.img > mkdir_output.txt

	chmod 777 mkdir_output.txt
	chmod 777 ext2_dump

	./ext2_dump twolevel.img > mkdir_output.txt

	diff -q case2-rm-afile.txt mkdir_output.txt

	../ext2_checker twolevel.img

fi



if [ $1 = "restore" ]; then

	rm -f twolevel.img
	cp images/twolevel.img .

	../ext2_rm twolevel.img /afile/

	#./readimage emptydisk.img > mkdir_output.txt

	chmod 777 mkdir_output.txt
	chmod 777 ext2_dump

	./ext2_dump twolevel.img > mkdir_output.txt

	diff -q case2-rm-afile.txt mkdir_output.txt

	../ext2_checker twolevel.img

fi


if [ $1 = "ln" ]; then

	rm -f twolevel.img
	cp images/twolevel.img .

	../ext2_ln twolevel.img /afile/ lnfile

	#./readimage emptydisk.img > mkdir_output.txt

	chmod 777 mkdir_output.txt
	chmod 777 ext2_dump

	./ext2_dump twolevel.img > mkdir_output.txt

	diff -q case3-ln.txt mkdir_output.txt

	../ext2_checker twolevel.img


fi

if [ $1 = "lns" ]; then

	rm -f twolevel.img
	cp images/twolevel.img .

	../ext2_ln -s twolevel.img /afile/ lnfile

	#./readimage emptydisk.img > mkdir_output.txt

	chmod 777 mkdir_output.txt
	chmod 777 ext2_dump

	./ext2_dump twolevel.img > mkdir_output.txt

	diff -q case3-ln.txt mkdir_output.txt

	../ext2_checker twolevel.img


fi

if [ $1 = "check" ]; then

	rm -f twolevel.img
	cp images/twolevel-corrupt.img .

	../ext2_checker twolevel-corrupt.img

	#./readimage emptydisk.img > mkdir_output.txt

	chmod 777 mkdir_output.txt
	chmod 777 ext2_dump

	./ext2_dump twolevel-corrupt.img > mkdir_output.txt

	#diff -q case3-ln.txt mkdir_output.txt

fi

if [ $1 = "cp" ]; then

        rm -f emptydisk.img
        cp images/emptydisk.img .

        ../ext2_cp emptydisk.img FILE_ONEBLK.txt /FILE_ONEBLK.txt

        #./readimage emptydisk.img > mkdir_output.txt

        chmod 777 mkdir_output.txt
        chmod 777 ext2_dump

        ./ext2_dump emptydisk.img > mkdir_output.txt

        diff -q case1-cp.txt mkdir_output.txt

        ../ext2_checker emptydisk.img

fi
