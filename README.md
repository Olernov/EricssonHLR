Project is compiled under Microsoft Visual Studio 2013 Community Edition.

EricssonHLR requires Microsoft Redistributable Package for VS2012 (install both 32-bit and 64-bit).

Build configurations description.

Debug: for debugging.

Release: builds EricssonHLR.exe file for running tests. Deploy it to machine having network access to HLR and run: 
	d:\IRBiS\Temp\HLR>EricssonHLR.exe "host=172.27.32.68; username={UUU}; password={PPP}; port=5000; logpath=Logs; num_threads=8" > test_log.txt.

Several threads (num_threads) would be started running commands for MSISDN 79047126560. Commands set and remove CLIR flag.
This activity emulates Irbis CQM activity.
If test scenario needs to be changed you can modify TestCommandSender function at EricssonHLR.cpp.

DLL release: release version for plugging to DMS.