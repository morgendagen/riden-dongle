This directory contains a mock server for the most complicated page: the control page.

It holds:

* the images (that are to be transformed to base64 strings via a tool like https://www.base64-image.de/)
* a basic web server that mocks the PSU, to allow testing without flashing
* a script to generate the appropriate sections in "http_static.h"
