7-Zip 9.10 Sources
------------------

7-Zip is a file archiver for Windows. 

7-Zip Copyright (C) 1999-2009 Igor Pavlov.


License Info
------------

7-Zip is free software distributed under the GNU LGPL 
(except for unRar code).
read License.txt for more infomation about license.

Notes about unRAR license:

Please check main restriction from unRar license:

   2. The unRAR sources may be used in any software to handle RAR
      archives without limitations free of charge, but cannot be used
      to re-create the RAR compression algorithm, which is proprietary.
      Distribution of modified unRAR sources in separate form or as a
      part of other software is permitted, provided that it is clearly
      stated in the documentation and source comments that the code may
      not be used to develop a RAR (WinRAR) compatible archiver.

In brief it means:
1) You can compile and use compiled files under GNU LGPL rules, since 
   unRAR license almost has no restrictions for compiled files.
   You can link these compiled files to LGPL programs.
2) You can fix bugs in source code and use compiled fixed version.
3) You can not use unRAR sources to re-create the RAR compression algorithm.


LZMA SDK
--------

Also this package contains files from LZMA SDK
you can download LZMA SDK from this page:
http://www.7-zip.org/sdk.html
read about addtional licenses for LZMA SDK in file
DOC/lzma.txt


How to compile
--------------
To compile sources you need Visual C++ 6.0.
For compiling some files you also need 
new Platform SDK from Microsoft' Site:
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/psdk-full.htm
or
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/XPSP2FULLInstall.htm
or
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/

If you use MSVC6, specify SDK directories at top of directories lists:
Tools / Options / Directories
  - Include files
  - Library files


To compile 7-Zip for AMD64 and IA64 you need:
  Windows Server 2003 SP1 Platform SDK from microsoft.com

Also you need Microsoft Macro Assembler:
  - ml.exe for x86 
  - ml64.exe for AMD64
You can use ml.exe from Windows SDK for Windows Vista or some other version.


Compiling under Unix/Linux
--------------------------
Check this site for Posix/Linux version:
http://sourceforge.net/projects/p7zip/


Notes:
------
7-Zip consists of COM modules (DLL files).
But 7-Zip doesn't use standard COM interfaces for creating objects.
Look at
7zip\UI\Client7z folder for example of using DLL files of 7-Zip. 
Some DLL files can use other DLL files from 7-Zip.
If you don't like it, you must use standalone version of DLL.
To compile standalone version of DLL you must include all used parts
to project and define some defs. 
For example, 7zip\Bundles\Format7z is a standalone version  of 7z.dll 
that works with 7z format. So you can use such DLL in your project 
without additional DLL files.


Description of 7-Zip sources package
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

DOC                Documentation
---
  7zFormat.txt   - 7z format description
  copying.txt    - GNU LGPL license
  unRarLicense.txt - License for unRAR part of source code
  history.txt    - Sources history
  Methods.txt    - Compression method IDs
  readme.txt     - Readme file
  lzma.txt       - LZMA SDK description
  7zip.nsi       - installer script for NSIS


C   - Source code in C
CPP - Source code in CPP

Common            Common modules
Windows           Win32 wrappers

7zip
-------
  Common          Common modules for 7-zip

  Archive         7-Zip Archive Format Plugins 
  --------
    Common
    7z
    Arj
    BZip2
    Cab
    Cpio
    GZip
    Rar
    Rpm            
    Split
    Tar
    Zip

  Bundle          Modules that are bundles of other modules
  ------
    Alone         7za.exe: Standalone version of 7z
    Alone7z       7zr.exe: Standalone version of 7z that supports only 7z/LZMA/BCJ/BCJ2
    SFXCon        7zCon.sfx: Console 7z SFX module
    SFXWin        7z.sfx: Windows 7z SFX module
    SFXSetup      7zS.sfx: Windows 7z SFX module for Installers
    Format7z            7za.dll:  .7z support
    Format7zExtract     7zxa.dll: .7z support, extracting only
    Format7zR           7zr.dll:  .7z support, LZMA/BCJ* only
    Format7zExtractR    7zxr.dll: .7z support, LZMA/BCJ* only, extracting only
    Format7zF           7z.dll:   all formats

  UI
  --
    Agent         Intermediary modules for FAR plugin and Explorer plugin
    Console       7z.exe Console version
    Explorer      Explorer plugin
    Resource      Resources
    Far           FAR plugin  
    Client7z      Test application for 7za.dll 

  Compress
  --------
    BZip2        BZip2 compressor
    Branch       Branch converter
    ByteSwap     Byte Swap converter
    Copy         Copy coder
    Deflate       
    Implode
    Arj
    LZMA
    PPMd          Dmitry Shkarin's PPMdH with small changes.
    LZ            Lempel - Ziv

  Crypto          Crypto modules
  ------
    7zAES         Cipher for 7z
    AES           AES Cipher
    Rar20         Cipher for Rar 2.0
    RarAES        Cipher for Rar 3.0
    Zip           Cipher for Zip

  FileManager       File Manager


---
Igor Pavlov
http://www.7-zip.org
