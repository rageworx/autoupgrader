# Auto Upgrader
(C)2013 Ragrworx freeware. , Raph.K. (rageworx@gmail.com)

## What is this project?
 This project was a stand-alone automatic package upgrade tool with FLTK in Windows (or Linux, in future) system with HTTP/1.0 GET method in socket.
 This project is currently works on Windows32 only.
 
## Used HTTP/1.0/GET
 1. It was long time ago, I have downloaded a sample source code from internet.
 2. And I don't know who was made it.
 3. **httpparser**, **httprequest**, **sock_port** was not my source code but it has been changed to work in MinGW(gcc).

## Requried prebuilt libraries
 1. socket/winsock
 2. FLTK 1.3.x (recommend to use FLTK 1.3.2 stable version)

## Server side
 1. Need a text based file for contain these informations.
 	- URL=update base URL
 	- FILES=files to be downloaded from server.
 	- KILLS=Names of be killed in process list when it start to update.
 	- EXECUTE=A executing binary name when it completed update, 
 2. Check example sources in example/client and example/server.

## Screenshot
![](https://github.com/rageworx/autoupgrader/blob/master/example/screenshot.jpg?raw=true)
