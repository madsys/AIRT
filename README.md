	AIRT --	Advanced Incident Response Tool
	Developed and maintained by madsys, coolq


[1] What is AIRT?

[2] How can I use it?

[3] What platforms it supports now?

[4] Where can I get the lastest version?

[5] How can I report a bug?


[1] What is AIRT?
--------------------

 	AIRT(Advanced incident response tool) is a tool set for incident response which works on linux platform. 
  	AIRT is dedicated to discover the kernel backdoors on linux platform. It's useful when you want to know 
   	what evil kernel backdoor is still resident on your broken system and what the hell it is. 
  	It consists of 5 useful tools:

mod_hunter: 

	detect hidden module on the suspect system.

process_hunter: 

	detect hidden process from kernel on the suspect system.

sock_hunter: 

	detect hidden net port from kernel on the suspect system 
	(only supports IPv4 now).

modumper: 

	dumps the hidden module into file.

dismod: 

	trys to analyze the dumped module (you should use dismod.pl instead 
	of dismod)

Note: it only supports 2.6 kernel now.


[2] How can I use it?
----------------------

	Unpack the package, enter the directory and run:
	make

mod_hunter:

	if you want to check if there is hidden module in your system, just run:
	insmod mod_hunter.ko && cat /proc/showmodules
 	and then there will be something like that:

      address                name     size      core_addr     flags

  1  0xc6813400            am_evil     1536     0xc6813000       0    Warning<--
  
  2  0xc6845180         mod_hunter     5528     0xc6844000       0
  
  3  0xc6858580            pcnet32    34824     0xc6850000       0

	if u want to uncover the hidden modules which showed by mod_hunter
	(marked with Warning), you can try:
	
	echo "0xaabbccdd" > /proc/showmodules
	
	Note: 0xaabbccdd is the address of the suspect module and it must be 10 characters

process_hunter:

	if you want to check if there is hidden process in your system:
	insmod process_hunter.ko && cat /proc/showprocess

port_hunter:

	if you want to check if there is hidden net port in your system:
	insmod port_hunter.ko && cat /proc/showsocks

mod_dumper:

	if you want to dump the module:
	enter the mod_dumper directory and run:
	insmod modumper.ko mod_name=MODULE && cat /proc/get_mod
	it will dump the module automatically into dump.dat & dump.info

	if you want to analyze the dumped module:
	./dismod.pl (use the perl scritp instead of the dismod binary!)


[3] What platforms it supports now?
-----------------------------------------
	It currently supports 2.6.x kernel and Fedora Core2 and SUSE 9.2, 
	not tested on SMP yet.
	

[4] Where can I get the last version?
-----------------------------------------
  https://github.com/madsys/AIRT
	http://sourceforge.net/projects/airt-linux/


[5] How can I report a bug?
----------------------------
	We are glad to receive any comments && bugs && suggestion, plz mail to:
	madsys@gmail.com
