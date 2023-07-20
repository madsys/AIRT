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

	detect hidden net port from kernel on the suspect system (only supports IPv4 now).

modumper: 

	dumps the hidden module into file.

dismod: 

	trys to analyze the dumped module (you should use dismod.pl instead of dismod).

Note: it only supports 2.6 kernel now.


[2] How can I use it?
----------------------

	Unpack the package, enter the directory and run:
	make

mod_hunter:

	to check if there is hidden module in your system, just run:
	$ sudo insmod mod_hunter.ko && cat /proc/showmodules
 	and then there will be something like that:

![mod_hunter-result](https://github.com/madsys/AIRT/assets/1812459/e00d7237-ba9e-47f7-9235-86d8fdf47925)


	to uncover the hidden modules which showed by mod_hunter(marked with "Warning"), just try:
	
	$ sudo echo "0xaabbccdd" > /proc/showmodules
	
	Note: 0xaabbccdd is the address of the suspect module and it must be 10 characters

process_hunter:

	to check if there is hidden process in your system:
	$ sudo insmod process_hunter.ko && cat /proc/showprocess

port_hunter:

	to check if there is hidden net port in your system:
	$ sudo insmod port_hunter.ko && cat /proc/showsocks

mod_dumper:

	to dump the module, enter the mod_dumper directory and run:
	$ sudo insmod modumper.ko mod_name=MODULE && cat /proc/get_mod
	it will dump the module automatically into dump.dat & dump.info

	to analyze the dumped module:
	$ ./dismod.pl (use the perl scritp instead of the dismod binary!)


[3] What platforms it supports now?
-----------------------------------------
	It currently supports 2.6.x kernel and Fedora Core2 / SUSE 9.2 distro, not tested on SMP yet.
	

[4] Where can I get the last version?
-----------------------------------------
  https://github.com/madsys/AIRT
  
  http://sourceforge.net/projects/airt-linux/


[5] How can I report a bug?
----------------------------
	We are glad to receive any comments && bugs && suggestion, plz mail to:	madsys@gmail.com
